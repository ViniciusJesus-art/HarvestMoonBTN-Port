/* The villagers: their schedules and walking routes, their daily affection, greeting the
 * player, and drawing them. */

#include <sys/types.h>
#include <abs.h>
#include <memory.h>
#include <libgte.h>
#include "types.h"
#include "anim.h"
#include "clock.h"
#include "cue.h"
#include "doors.h"
#include "estate.h"
#include "event_commands.h"
#include "event_flags.h"
#include "event_rules.h"
#include "farm_data.h"
#include "farm_load.h"
#include "farm_probe.h"
#include "farm_state.h"
#include "items.h"
#include "main.h"
#include "menu_window.h"
#include "player.h"
#include "scene.h"
#include "screen_fade.h"
#include "villager.h"

#define VILLAGER_TALK_COOLDOWN 20
#define VILLAGER_DELAY_LIMIT 30
/* How long a greeted villager stands talking, in ticks. */
#define VILLAGER_TALK_TIME 150
/* Where the route grid starts in the loaded section buffer. */
#define ROUTE_GRID_OFFSET 0x1000
#define ANGLE_HALF_TURN 0x800
/* A villager is greeted within this squared distance of the player, and within this
 * many angle units of the way the player faces. */
#define GREET_RANGE_SQ 16000000
#define GREET_ANGLE 600
/* The friendship a new villager record starts with. A bachelorette starts at 0. */
#define VILLAGER_START_FRIENDSHIP 50
/* The affection a bachelorette gains from a family update, or loses on a roll. */
#define VILLAGER_AFFECTION_STEP 200
/* The affection at which a bachelorette raises her event flag. */
#define VILLAGER_AFFECTION_FLAG_LEVEL 1500
/* The shipment menu texts: nothing shipped, a small shipment, and a shipment worth
 * SHIPMENT_LARGE_VALUE or more. */
#define SHIPMENT_MENU_NONE 0x72B
#define SHIPMENT_MENU_SMALL 0x72C
#define SHIPMENT_MENU_LARGE 0x72D
#define SHIPMENT_LARGE_VALUE 10000

static void villager_clear_record(FarmVillager *villager);
static void villager_claim_slot(s32 id);
static void villager_set_destination(FarmVillager *villager);
static void villager_aim_cell(FarmVillager *villager);
static void villager_update_frames(FarmVillager *villager, s32 frames);
static s32 villager_tick_wait(FarmVillager *villager);
static void villager_set_action(FarmVillager *villager, enum VillagerAction action, s32 animation, s32 lockCount);
static void villager_draw_if_here(FarmVillager *villager);
static s32 villager_try_greet(FarmVillager *villager);
static void villager_draw(FarmVillager *villager);
static FarmRouteRow *villager_route_grid(FarmVillager *unused);
static s32 villager_route_step_move(FarmVillager *villager, s32 command, s32 resume);
static void villager_head_for_cell(FarmVillager *villager, FarmCell *cell);
static s32 villager_event_cell_start(FarmVillager *villager, FarmCell *cell);
static void villager_shipment_cell(FarmVillager *villager, FarmCell *cell);
static s16 villager_route_mode;
static s32 villager_greet_dist_sq;
static s32 villager_greet_min_dist;
static s32 schedule_row;
static s32 schedule_entry;

s32 one_minute = 30;
s32 villager_speed_outdoors = 768;
s32 villager_speed_indoors = 256;
u32 position_correction = 64;
u32 villager_parity = 0;

/* Reset the villagers of the current zone and resume their walks: each one that is on
 * the map takes its place in the zone and resumes from the time since its last schedule
 * step, or stops when that step is not yet due. Runs on a zone change and at noon and
 * 18 h. */
void farm_item_history_resume(void)
{
    FarmVillager *item = game_state.villagers;
    s32 i;
    s16 zone;
    s16 now;
    s32 remaining;

    game_state.villagers[CHARACTER_UNNAMED_27].place.value = PLACE_13;
    game_state.villagers[CHARACTER_UNNAMED_27].place.location = PLACE_28;
    villager_route_mode = 0;
    for (i = 0; i < CHARACTER_COUNT; i++, item++)
    {
        item->wait = 0;
        item->position.vx = 0;
        item->position.vy = 0;
        item->position.vz = 0;
        item->flags &= ~(VILLAGER_FLAG_EVENT_PENDING | VILLAGER_FLAG_EVENT_STARTED | VILLAGER_FLAG_BIT6 | VILLAGER_FLAG_ON_MAP | VILLAGER_FLAG_BIT14);
        item->placeFlags &= ~(VILLAGER_PLACE_EVENT_CELL | VILLAGER_PLACE_KEPT_PLACE);
        zone = farm_area.mode.data->head.place.location;
        villager_parity = 0;
        if (zone != PLACE_EVENT_CELL && item->place.location == zone && villager_load_request(item->id, &item->sprite80) != 0)
        {
            if (item->id == ITEM_CLIPPERS)
            {
                item->place.value = PLACE_EVENT_CELL;
                item->place.location = PLACE_EVENT_CELL;
            }
            else if (villager_route_step_move(item, villager_route_mode, 0) != 0)
            {
                item->flags |= VILLAGER_FLAG_ON_MAP;
                villager_aim_cell(item);
                villager_set_destination(item);
            }
            else
            {
                item->flags |= VILLAGER_FLAG_ON_MAP;
                now = game_state.progress.clock.minuteOfDay;
                if (now < CLOCK_DAY_START_MINUTE)
                {
                    remaining = 0;
                }
                else
                {
                    remaining = now - (item->schedule[item->step].time + item->delay);
                }
                if (remaining >= 0)
                {
                    villager_update_frames(item, one_minute * remaining / 2);
                }
                else
                {
                    item->flags &= ~VILLAGER_FLAG_ON_MAP;
                }
                villager_set_destination(item);
            }
        }
    }
    villager_route_mode = 1;
}

void villager_reset_all(void)
{
    s32 i;
    for (i = 0; i < CHARACTER_COUNT; i++)
    {
        villager_claim_slot(i);
    }
    farm_item_rows_init();
}

void villager_clear_delays(void)
{
    FarmVillager *p = game_state.villagers;
    u32 i = 0;
    do
    {
        p->delay = 0;
        i++;
        p++;
    } while (i < CHARACTER_COUNT);
}

void villager_clear_all(void)
{
    FarmVillager *p = game_state.villagers;
    s32 i = 36;
    do
    {
        villager_clear_record(p);
        cue_reset(&p->cue);
        i--;
        p++;
    } while (i >= 0);
    farm_item_rows_init();
}

static void villager_clear_record(FarmVillager *villager)
{
    memset(villager, 0, sizeof(FarmVillager));
    villager->id = -1;
}

void villager_clear_flag_bit4_all(void)
{
    FarmVillager *p = game_state.villagers;
    s32 i = 36;
    do
    {
        p->flags &= ~VILLAGER_FLAG_BIT4;
        p++;
    } while (--i >= 0);
}

/* Give villager `id` the first free record. A bachelorette starts at 0 affection and
 * friendship; anyone else starts at VILLAGER_START_FRIENDSHIP friendship. */
static void villager_claim_slot(s32 id)
{
    FarmVillager *item;
    s32 slot;
    s32 result;

    item = game_state.villagers;
    slot = 0;
    do
    {
        if (item->id == -1)
        {
            villager_clear_record(item);
            item->id = id;
            item->flags |= VILLAGER_FLAG_JUST_RESET;
            cue_reset(&item->cue);
            result = villager_is_bachelorette(slot);
            if (result == 1)
            {
                item->affection = 0;
                item->friendship = 0;
            }
            else
            {
                item->affection = 0;
                item->friendship = VILLAGER_START_FRIENDSHIP;
            }
            return;
        }
        slot++;
        item++;
    } while (slot < 37);
}

/* Update villager affection and friendship. A villager flagged by an event today
 * (VILLAGER_FLAG_BIT10) gains: +200 affection for a bachelorette, else +1 friendship.
 * The others, when not talked to today and not flagged, drop on a 1-in-10 roll, except
 * Kai, Kano, Gourmet, Louis, Greg and Won, who never drop. Affection is clamped to
 * 0..65535, friendship to 0..FRIENDSHIP_MAX. The wife's talked-today flag follows her
 * record. Karen, Ann, Elli, Mary and Popuri raise their event flag once their affection
 * passes VILLAGER_AFFECTION_FLAG_LEVEL. Clears the talked-today and flagged bits. */
void family_flags_update(void)
{
    u32 i;
    s32 is_bachelorette;
    s32 affection;
    s32 level;
    FarmVillager *item;

    item = game_state.villagers;
    for (i = 0; i < CHARACTER_COUNT; i++, item++)
    {
        is_bachelorette = villager_is_bachelorette(i);
        affection = item->affection;
        level = item->friendship;
        if (item->flags & VILLAGER_FLAG_BIT10)
        {
            if (is_bachelorette == 1)
            {
                affection += VILLAGER_AFFECTION_STEP;
            }
            else
            {
                level++;
            }
        }
        switch (i)
        {
        case CHARACTER_KAI:
        case CHARACTER_KANO:
        case CHARACTER_GOURMET:
        case CHARACTER_LOUIS:
        case CHARACTER_GREG:
        case CHARACTER_WON:
            break;
        default:
            if (!(item->flags & (VILLAGER_FLAG_TALKED_TODAY | VILLAGER_FLAG_BIT10)) && rand_range(10) == 0)
            {
                if (is_bachelorette == 1)
                {
                    affection -= VILLAGER_AFFECTION_STEP;
                }
                else
                {
                    level--;
                }
            }
            break;
        }
        if (affection > 65535)
        {
            affection = 65535;
        }
        if (affection < 0)
        {
            affection = 0;
        }
        if (level > FRIENDSHIP_MAX)
        {
            level = FRIENDSHIP_MAX;
        }
        if (level < 0)
        {
            level = 0;
        }
        item->affection = affection;
        item->friendship = level;
        if (i == game_state.family.wifeVillager)
        {
            if (item->flags & VILLAGER_FLAG_TALKED_TODAY)
            {
                event_flag_set(EVENT_FLAG_WIFE_TALKED_TODAY, 1);
            }
            else
            {
                event_flag_set(EVENT_FLAG_WIFE_TALKED_TODAY, 0);
            }
        }
        item->flags &= ~(VILLAGER_FLAG_TALKED_TODAY | VILLAGER_FLAG_BIT10);
    }
    if (game_state.villagers[CHARACTER_KAREN].affection > VILLAGER_AFFECTION_FLAG_LEVEL)
    {
        event_flag_set(EVENT_FLAG_KAREN_AFFECTION_1500, 1);
    }
    if (game_state.villagers[CHARACTER_ANN].affection > VILLAGER_AFFECTION_FLAG_LEVEL)
    {
        event_flag_set(EVENT_FLAG_ANN_AFFECTION_1500, 1);
    }
    if (game_state.villagers[CHARACTER_ELLI].affection > VILLAGER_AFFECTION_FLAG_LEVEL)
    {
        event_flag_set(EVENT_FLAG_ELLI_AFFECTION_1500, 1);
    }
    if (game_state.villagers[CHARACTER_MARY].affection > VILLAGER_AFFECTION_FLAG_LEVEL)
    {
        event_flag_set(EVENT_FLAG_MARY_AFFECTION_1500, 1);
    }
    if (game_state.villagers[CHARACTER_POPURI].affection > VILLAGER_AFFECTION_FLAG_LEVEL)
    {
        event_flag_set(EVENT_FLAG_POPURI_AFFECTION_1500, 1);
    }
}

/* Start the villager's animation for the cell it is at: walking on (ROUTE_CELL_WAIT_WALK
 * and ROUTE_CELL_STEP), or standing (ROUTE_CELL_STAND, its speed cleared). */
static void villager_cell_anim(FarmVillager *villager, FarmCell *cell)
{
    s32 mode;

    mode = cell->kind;
    switch (mode)
    {
    case ROUTE_CELL_WAIT_WALK:
    case ROUTE_CELL_STEP:
        if (villager->wait != 0)
        {
            villager_set_action(villager, VILLAGER_ACTION_WALKING, 0, -2);
            return;
        }
        villager_set_action(villager, VILLAGER_ACTION_WALKING, villager->facing + 1, -2);
        return;
    case ROUTE_CELL_STAND:
        villager->speed.vx = 0;
        villager->speed.vy = 0;
        villager->speed.vz = 0;
        villager_set_action(villager, VILLAGER_ACTION_STANDING, 0, -2);
    }
}

/* Put the villager at the cell's position, in whole units and in 16.16 fixed point. */
static inline void move_to_cell(FarmVillager *villager, FarmCell *cell)
{
    villager->position.vx = cell->x;
    villager->position.vy = cell->y;
    villager->position.vz = cell->z;
    villager->fixedPosition.vx = cell->x << 16;
    villager->fixedPosition.vy = cell->y << 16;
    villager->fixedPosition.vz = cell->z << 16;
}

/* The villager reached `cell`: stop it there and start the next cell of its route
 * row, or stand when the row ends. */
static void villager_reached_cell(FarmVillager *villager, FarmRouteRow *grid, FarmCell *cell)
{
    s16 col;

    villager->position.vx = cell->x;
    villager->position.vz = cell->z;
    col = ++villager->column;
    villager->speed.vx = 0;
    villager->speed.vy = 0;
    villager->speed.vz = 0;
    if (col == ROUTE_ROW_CELLS)
    {
        villager->column = ROUTE_ROW_CELLS - 1;
        villager->action = VILLAGER_ACTION_NONE;
        villager->facing = cell->pose;
        villager_set_action(villager, VILLAGER_ACTION_STANDING, 0, -2);
        return;
    }
    cell = &grid[villager->row][col];
    if (villager_route_mode == 0)
    {
        if (cell->kind == ROUTE_CELL_EVENT || cell->kind == ROUTE_CELL_SHIPMENT || cell->kind == ROUTE_CELL_EVENT_2 || cell->kind == ROUTE_CELL_EVENT_FLAGGED)
        {
            villager->column++;
            cell = &grid[villager->row][villager->column];
        }
    }
    switch (cell->kind)
    {
    case ROUTE_CELL_JUMP:
        villager->column = cell->id;
        cell = &grid[villager->row][villager->column];
        villager->flags &= ~VILLAGER_FLAG_BIT14;
        if (cell->kind != ROUTE_CELL_ENTER_FLAGGED)
        {
            if (cell->kind != ROUTE_CELL_ENTER)
            {
                break;
            }
        }
        else
        {
            villager->flags |= VILLAGER_FLAG_BIT14;
        }
        move_to_cell(villager, cell);
        villager->column++;
        cell = &grid[villager->row][villager->column];
        if (villager_route_mode != 0)
        {
            villager->wait = cell->id;
        }
        break;
    case ROUTE_CELL_STEP:
        villager->column++;
        cell = &grid[villager->row][villager->column];
        move_to_cell(villager, cell);
        break;
    case ROUTE_CELL_EVENT_FLAGGED:
        villager->placeFlags |= VILLAGER_PLACE_EVENT_CELL;
        /* fall through */
    case ROUTE_CELL_EVENT:
    case ROUTE_CELL_EVENT_2:
        if ((villager->flags & VILLAGER_FLAG_IN_EVENT) == 0)
        {
            villager_event_cell_start(villager, cell);
            return;
        }
        villager->column++;
        cell = &grid[villager->row][villager->column];
        move_to_cell(villager, cell);
        if (villager_route_mode != 0)
        {
            villager->wait = cell->id;
        }
        break;
    case ROUTE_CELL_SHIPMENT:
        villager_shipment_cell(villager, cell);
        return;
    case ROUTE_CELL_SCHEDULE:
        villager->step++;
        ((void (*)())villager_set_destination)(villager);
        villager->place.value = villager->schedule[villager->step].place.value;
        villager->place.location = villager->schedule[villager->step].place.location;
        villager->schedule[villager->step].time = game_state.progress.clock.minuteOfDay;
        if (villager->place.location != farm_area.mode.data->head.place.location)
        {
            villager->placeFlags &= ~VILLAGER_PLACE_KEPT_PLACE;
            villager->flags &= ~VILLAGER_FLAG_ON_MAP;
            villager->schedule[villager->step].time -= villager->delay;
            if (place_entries[villager->place.location] != 0)
            {
                villager->delay = 0;
            }
            return;
        }
        villager->schedule[villager->step].time = game_state.progress.clock.minuteOfDay;
        villager_route_step_move(villager, villager_route_mode, 0);
        if (villager_route_mode != 0)
        {
            villager->wait = cell->id;
        }
        if (villager->wait != 0)
        {
            villager_set_action(villager, VILLAGER_ACTION_WALKING, villager->facing, -2);
        }
        break;
    case ROUTE_CELL_WAIT_WALK:
    case ROUTE_CELL_WAIT_WALK_2:
        if (villager_route_mode != 0)
        {
            villager->wait = cell->id;
        }
        break;
    case ROUTE_CELL_STAND:
    case ROUTE_CELL_STAND_2:
        villager->facing = cell[-1].pose;
        villager_set_action(villager, VILLAGER_ACTION_STANDING, 0, -2);
        return;
    }
    if (villager->wait == 0)
    {
        villager_head_for_cell(villager, cell);
        villager_aim_cell(villager);
        villager_set_action(villager, VILLAGER_ACTION_WALKING, villager->facing + 1, -2);
    }
    villager_cell_anim(villager, cell);
}

/* Advance a villager by its speed, then fire the route cell it stands on. Event
 * cells fire only while their event is running; other cells fire once the villager
 * is near the cell's centre. */
static void villager_cell_trigger(FarmVillager *villager)
{
    FarmRouteRow *grid;
    FarmCell *cell;
    s32 limit;

    grid = villager_route_grid(villager);
    villager->fixedPosition.vx += villager->speed.vx;
    villager->fixedPosition.vz += villager->speed.vz;
    cell = &grid[villager->row][villager->column];
    villager->position.vx = villager->fixedPosition.vx / 65536;
    villager->position.vz = villager->fixedPosition.vz / 65536;
    if ((villager->flags & VILLAGER_FLAG_ON_MAP) && !(villager_parity & 1))
    {
        farm_settle_on_ground(&villager->position, 0xC000);
    }
    switch (cell->kind)
    {
    case ROUTE_CELL_EVENT:
    case ROUTE_CELL_EVENT_2:
    case ROUTE_CELL_EVENT_FLAGGED:
        if ((villager->flags & VILLAGER_FLAG_EVENT_STARTED) && villager->action == VILLAGER_ACTION_EVENT && door_entry_for_event(cell->id) == 3)
        {
            villager_reached_cell(villager, grid, cell);
            villager->talkCooldown = VILLAGER_TALK_COOLDOWN;
            villager->flags &= ~(VILLAGER_FLAG_EVENT_PENDING | VILLAGER_FLAG_EVENT_STARTED);
        }
        return;
    }
    limit = 64;
    if (place_entries[villager->place.location] == 0x1000)
    {
        limit = 16;
    }
    if (villager->flags & VILLAGER_FLAG_BIT14)
    {
        limit = 16;
    }
    if (abs(cell->x - villager->position.vx) < limit && abs(cell->z - villager->position.vz) < limit)
    {
        villager_reached_cell(villager, grid, cell);
    }
}

/* The index of request `value` in an id pack, or -1. The pack opens with its count,
 * then one request id per animation. */
static s32 anim_pack_find_id(s16 value, s32 *data)
{
    u32 count = data[0];
    s32 i = 0;
    data++;
    while (i < count)
    {
        if (value == data[i])
            return i;
        i++;
    }
    return -1;
}

/* Look request `id` up in the primary resource table, falling back to the
 * alternate table with a row chosen from the farm calendar, and load the entry
 * it names into `dst`. Returns 0 when neither table has the request. */
s32 villager_load_request(s32 id, AnimState *dst)
{
    s32 row;
    s32 entry;
    s32 first;
    s32 day;
    s32 *resource;
    s32 *fallback;

    resource = farm_area.mode.resourceBuffer;
    fallback = (s32 *)farm_area.slotAnims;
    row = farm_area.mode.data->head.resourceId;
    entry = anim_pack_find_id(id, resource);
    first = entry;
    if (entry == -1)
    {
        entry = anim_pack_find_id(id, fallback);
        if (entry != first)
        {
            row = game_state.progress.clock.elapsed.hours / 6;
            if (row >= 3)
            {
                row = 2;
            }
            resource = fallback;
            day = game_state.progress.clock.date.weekday;
            row += day * 3 - (day / 7) * 21;
            row += 14;
        }
        else
        {
            return 0;
        }
    }
    if (row == -1)
    {
        while (1)
        {
        }
    }
    schedule_row = row;
    schedule_entry = entry + 1;
    if (row == 0x3E6)
    {
        row = game_state.family.wifeIndex * 3 + 35;
        row += game_state.family.childLevel;
    }
    /* The row table points into the resource table. */
    anim_bind_file(dst, (AnimFile *)((u8 *)resource + ANIM_PACK_START(farm_pack_tocs[row], entry)));
    return 1;
}

/* Take the place and destination of the villager's current schedule step. A new
 * destination clears VILLAGER_FLAG_BIT4. */
static void villager_set_destination(FarmVillager *villager)
{
    u16 derived;
    villager->place = villager->schedule[villager->step].place;
    derived = villager->schedule[villager->step].route >> 8;
    if (villager->destination != derived)
        villager->flags &= ~VILLAGER_FLAG_BIT4;
    villager->prevDestination = villager->destination;
    villager->destination = derived;
}

/* Move the villager on to its next schedule step once the clock reaches the step's
 * time (plus its delay). Its place follows the step, and villager_route_step_move
 * then walks it to the step's route row. */
static void villager_schedule_advance(FarmVillager *villager)
{
    s16 old_location;
    s32 threshold;
    s32 total;
    s32 moved;
    s32 eligible;
    u16 flags;

    if ((game_state.globalFlags & GLOBAL_FLAG_MINUTE_PASSED) == 0)
    {
        return;
    }
    if (villager->step == VILLAGER_SCHEDULE_STEPS - 1)
    {
        return;
    }

    total = villager->schedule[villager->step + 1].time + villager->delay;
    threshold = game_state.progress.clock.minuteOfDay;
    eligible = 0;
    if (total < CLOCK_DAY_START_MINUTE)
    {
        threshold -= CLOCK_DAY_MINUTES;
    }
    if (!(threshold < total))
    {
        eligible = 1;
    }
    if (eligible == 0)
    {
        return;
    }

    old_location = villager->place.location;
    moved = 0;
    if (villager->flags & VILLAGER_FLAG_ON_MAP)
    {
        villager->placeFlags |= VILLAGER_PLACE_KEPT_PLACE;
    }
    else
    {
        villager->placeFlags &= ~VILLAGER_PLACE_KEPT_PLACE;
    }
    villager->step++;
    ((void (*)())villager_set_destination)(villager);
    if (old_location != villager->place.location)
    {
        moved = 1;
    }

    if (scene_place_mismatch(&villager->place.value) != 0)
    {
        return;
    }
    if ((villager->flags & VILLAGER_FLAG_ON_MAP) == 0)
    {
        if (villager_load_request(villager->id, &villager->sprite80) == 0)
        {
            return;
        }
        villager->wait = 0;
    }

    flags = villager->flags;
    villager->placeFlags &= ~VILLAGER_PLACE_EVENT_CELL;
    if ((flags & VILLAGER_FLAG_IN_EVENT) == 0 && moved != 0)
    {
        villager->flags = flags | VILLAGER_FLAG_ON_MAP;
    }
    villager->talkCooldown = VILLAGER_TALK_COOLDOWN;
    villager_route_step_move(villager, villager_route_mode, 0);
}

/* Aim the villager at the cell its column is on and start its animation. */
static void villager_aim_cell(FarmVillager *villager)
{
    FarmCell *cell;

    cell = &villager_route_grid(villager)[villager->row][villager->column];
    villager_head_for_cell(villager, cell);
    villager_cell_anim(villager, cell);
}

/* Advance a villager by `frames` frames: its talk cooldown, its delay while it talks or
 * has VILLAGER_FLAG_BIT2 set, then per frame its cue, its lock countdown and its route
 * wait. */
static void villager_update_frames(FarmVillager *villager, s32 frames)
{
    VECTOR coords;
    void *out_x;
    s32 out_y;
    s32 check;
    u16 flags;
    s32 i;
    s16 timer;

    if (villager->talkCooldown != 0)
    {
        villager->talkCooldown--;
    }
    flags = villager->flags;
    if (flags & VILLAGER_FLAG_ON_MAP)
    {
        check = 1;
        if (flags & VILLAGER_FLAG_IN_EVENT)
        {
            coords.vx = villager->ePosition.vx;
            coords.vy = villager->ePosition.vy;
            coords.vz = villager->ePosition.vz;
        }
        else
        {
            coords.vx = villager->position.vx;
            coords.vy = villager->position.vy;
            coords.vz = villager->position.vz;
            if (flags & VILLAGER_FLAG_EVENT_PENDING)
            {
                check = 0;
            }
            if (villager->delay >= VILLAGER_DELAY_LIMIT)
            {
                check = 0;
            }
            if (villager->placeFlags & VILLAGER_PLACE_EVENT_CELL)
            {
                check = 0;
            }
        }
        if (villager->flags & VILLAGER_FLAG_EVENT_PENDING)
        {
            check = 0;
        }
        if (game_state.player.mode == PLAYER_MODE_POSE_4)
        {
            check = 0;
        }
        if (check != 0 && farm_callback_capped(&coords, villager->place, 1000, 1000, &out_x, &out_y, 0, 1) != 0 && villager->delay < VILLAGER_DELAY_LIMIT)
        {
            villager->flags |= VILLAGER_FLAG_BIT2;
        }
    }
    if ((villager->action == VILLAGER_ACTION_TALKING || (villager->flags & VILLAGER_FLAG_BIT2)) && (game_state.globalFlags & GLOBAL_FLAG_MINUTE_PASSED))
    {
        villager->delay++;
        if (villager->delay >= VILLAGER_DELAY_LIMIT)
        {
            villager->flags &= ~VILLAGER_FLAG_BIT2;
            villager_tick_wait(villager);
        }
    }
    for (i = 0; i < frames; i++)
    {
        cue_countdown(&villager->cue);
        timer = villager->lockCount;
        if (timer != -2)
        {
            if (timer != -3)
            {
                if (timer != -2)
                {
                    villager->lockCount--;
                }
            }
            if (villager->lockCount == -1)
            {
                villager_aim_cell(villager);
            }
        }
        else
        {
            villager_tick_wait(villager);
            if ((villager->flags & VILLAGER_FLAG_ON_MAP) == 0)
            {
                return;
            }
        }
        if (villager_route_mode == 0)
        {
            villager_parity++;
        }
    }
}

/* Per-frame update of one villager. Clears VILLAGER_FLAG_JUST_RESET. Its schedule
 * advances outside events; nothing else moves while paused, during an event or fade,
 * or off the map. */
static void villager_update(FarmVillager *villager)
{
    if (villager->flags & VILLAGER_FLAG_JUST_RESET)
    {
        villager->flags &= ~VILLAGER_FLAG_JUST_RESET;
    }
    if (game_state.globalFlags & GLOBAL_FLAG_PAUSED)
    {
        return;
    }
    if (event_flag_get(EVENT_FLAG_EVENT_ACTIVE) == 0)
    {
        villager_schedule_advance(villager);
    }
    if (fade_is_settled() == 0)
    {
        return;
    }
    if ((villager->flags & VILLAGER_FLAG_ON_MAP) == 0)
    {
        return;
    }
    if ((villager->flags & VILLAGER_FLAG_BIT5) != 0)
    {
        return;
    }
    if (event_flag_get(EVENT_FLAG_EVENT_ACTIVE) != 0)
    {
        return;
    }
    villager_update_frames(villager, 1);
}

/* Count down a walking or event villager's wait; when it runs out the route cell the
 * villager is on fires. */
static s32 villager_tick_wait(FarmVillager *villager)
{
    s16 timer;
    u16 timer_value;
    s32 state;

    state = villager->action;
    switch (state)
    {
    case VILLAGER_ACTION_WALKING:
    case VILLAGER_ACTION_EVENT:
    case 6:
        if ((villager->flags & VILLAGER_FLAG_BIT2) == 0)
        {
            timer = villager->wait;
            timer_value = villager->wait;
            if (timer == 0)
            {
                villager_cell_trigger(villager);
            }
            else
            {
                villager->wait = timer_value - 1;
                if (((timer_value - 1) << 16) == 0)
                {
                    villager_aim_cell(villager);
                }
            }
        }
        break;
    }
    return 1;
}

/* Update every villager, and draw those on the current map. */
void player_records_tick(void)
{
    FarmVillager *villager;
    s32 remaining;
    u16 flags;

    villager = game_state.villagers;
    villager_parity += 1;
    if (game_state.debugFlags & DEBUG_FLAG_PEOPLE_PRO)
    {
        return;
    }
    remaining = 36;
    do
    {
        villager_update(villager);
        flags = villager->flags;
        if ((flags & VILLAGER_FLAG_IN_EVENT) == 0)
        {
            if (flags & VILLAGER_FLAG_ON_MAP)
            {
                villager_draw_if_here(villager);
            }
        }
        remaining -= 1;
        villager += 1;
    } while (remaining >= 0);
}

/* The angle units that each facing (FACING_*) points to. */
static s32 facing_angles[4] = {
    0,
    1023,
    2048,
    -1023,
};

/* Try to greet every villager near the player (within GREET_RANGE_SQ) and in front of
 * them (within GREET_ANGLE units of the way the player faces). Each one rolls in
 * villager_try_greet. */
s32 player_start_nearby_actions(FarmPlayer *player)
{
    FarmVillager *item;
    s32 i;
    u16 flags;
    s32 dx;
    s32 dz;
    s32 angle;
    s32 facing;

    item = game_state.villagers;
    villager_greet_min_dist = 0x7FFFFFFF;
    for (i = 0; i < 35; i++, item++)
    {
        flags = item->flags;
        if ((flags & VILLAGER_FLAG_ON_MAP) && player->place.location == item->place.location && item->talkCooldown == 0 && item->action == VILLAGER_ACTION_WALKING && !(item->placeFlags & VILLAGER_PLACE_EVENT_CELL) && !(flags & (VILLAGER_FLAG_EVENT_PENDING | VILLAGER_FLAG_EVENT_STARTED | VILLAGER_FLAG_IN_EVENT)) && item->delay < VILLAGER_DELAY_LIMIT)
        {
            dx = player->position.vx - item->position.vx + 1;
            dz = player->position.vz - item->position.vz + 1;
            villager_greet_dist_sq = dx * dx + dz * dz;
            if (villager_greet_dist_sq < GREET_RANGE_SQ)
            {
                angle = ratan2(dx * 10, dz * 10);
                facing = facing_angles[player->facing38];
                if (angle - GREET_ANGLE < facing && facing < angle + GREET_ANGLE)
                {
                    villager_try_greet(item);
                }
            }
        }
    }
    return 0;
}

/* Switch a villager to `action` and play `animation`, with its lock countdown. The
 * spouse, once a child is born (EVENT_FLAG_HAS_CHILD) and while childLevel is 0, plays
 * animation + 5 for animations below 5 outside 5-11 and 14-15 h. From 16 h on she plays
 * animation 10, and sets EVENT_FLAG_SPOUSE_AT_HOME when the house is level 2. */
static void villager_set_action(FarmVillager *villager, enum VillagerAction action, s32 animation, s32 lockCount)
{
    FarmState *farm;
    FarmState *clock;
    FarmState *state;
    s32 dark;
    s32 early;
    AnimState *sprite = &villager->sprite80;
    u16 prev;
    u16 hour;

    prev = villager->action;
    villager->action = action;
    villager->lockCount = lockCount;
    villager->flags |= VILLAGER_FLAG_BIT7;
    villager->prevAction = prev;
    anim_select_range(sprite, animation);
    farm = &game_state;
    if (farm->family.wifeVillager == villager->id)
    {
        if (event_flag_get(EVENT_FLAG_HAS_CHILD) == 1 && farm->family.childLevel == 0)
        {
            hour = farm->progress.clock.elapsed.hours;
            dark = !(hour >= 5 && hour < 12);
            if (hour >= 14 && hour < 16)
            {
                dark = 0;
            }
            if (dark && animation < 5)
            {
                anim_select_range(sprite, animation + 5);
            }
        }
        clock = &game_state;
        if (clock->progress.clock.elapsed.hours >= 16)
        {
            if (clock->progress.estate.houseLevel == 2)
            {
                event_flag_set(EVENT_FLAG_SPOUSE_AT_HOME, 1);
            }
            anim_select_range(&villager->sprite80, 10);
            return;
        }
    }
    if (villager->id == CHARACTER_CHILD)
    {
        if (event_flag_get(EVENT_FLAG_HAS_CHILD) == 1)
        {
            state = &game_state;
            if (state->family.childLevel == 0)
            {
                villager->facing = FACING_DOWN;
                if (rand_range(2) != 0)
                {
                    anim_select_range(&villager->sprite80, 2);
                }
                else
                {
                    anim_select_range(&villager->sprite80, 4);
                }
                clock = &game_state;
                early = clock->progress.clock.elapsed.hours < 14;
            }
            else
            {
                anim_select_range(&villager->sprite80, villager->facing + 10);
                early = state->progress.clock.elapsed.hours < 12;
            }
            if (!early)
            {
                anim_select_range(&villager->sprite80, 5);
                villager->facing = FACING_DOWN;
            }
        }
    }
    if (villager->id == CHARACTER_GOTZ)
    {
        if (farm_area.mode.data->head.place.location == PLACE_FARM && villager->row == 4)
        {
            anim_select_range(&villager->sprite80, villager->facing + 5);
        }
    }
    if (anim_frame_jumps(&villager->sprite80) != 0)
    {
        villager->sprite80.frame = villager->facing;
    }
    if (villager->id == CHARACTER_ELLEN)
    {
        anim_select_range(&villager->sprite80, 1);
    }
}

/* Whether `villager` is one of the five bachelorettes: Karen, Ann, Elli, Mary and Popuri. */
s32 villager_is_bachelorette(CharacterId villager)
{
    s32 result;

    result = 0;
    switch (villager)
    {
    case CHARACTER_KAREN:
    case CHARACTER_ANN:
    case CHARACTER_ELLI:
    case CHARACTER_MARY:
    case CHARACTER_POPURI:
        result = 1;
    }
    return result;
}

static void villager_draw_if_here(FarmVillager *villager)
{
    if (villager->place.location == farm_area.mode.place.location && !(game_state.globalFlags & GLOBAL_FLAG_TRANSITION))
    {
        villager_draw(villager);
    }
}

/* Roll whether a villager greets the player. It must be on the map, not talking or in an
 * event, and under VILLAGER_DELAY_LIMIT. The chance in percent is friendship - delay + 20.
 * A greeting makes it talk for VILLAGER_TALK_TIME, shows the talk cue and turns it to face
 * the player. */
static s32 villager_try_greet(FarmVillager *villager)
{
    s32 chance;
    u16 flags;
    s16 age;
    if (villager->action == VILLAGER_ACTION_TALKING)
        return 0;
    flags = villager->flags;
    if (flags & VILLAGER_FLAG_EVENT_PENDING)
        return 0;
    if (flags & VILLAGER_FLAG_IN_EVENT)
        return 0;
    if (!(flags & VILLAGER_FLAG_ON_MAP))
        return 0;
    age = villager->delay;
    if (age >= VILLAGER_DELAY_LIMIT)
        return 0;
    chance = villager->friendship - age + 20;
    if (rand_range(101) >= chance)
        return 0;
    villager_set_action(villager, VILLAGER_ACTION_TALKING, 0, VILLAGER_TALK_TIME);
    cue_set(&villager->cue, CUE_ICON_TALK, VILLAGER_TALK_TIME);
    villager->facing = (game_state.player.facing38 + 2) & 3;
    villager->sprite80.frame = villager->facing;
    return 1;
}

/* Draw the villager at its screen position with its talk cue, cursor and sprite, and
 * advance its animation unless the game is paused. The spouse, when at home, draws
 * 5 ordering slots back. */
static void villager_draw(FarmVillager *villager)
{
    SVECTOR input;
    FarmScreenPoint result;
    u_long *ot_slot;

    input.vx = villager->position.vx + 150;
    input.vy = villager->position.vy - 100;
    input.vz = villager->position.vz - 150;
    if (estate_point_project(&input, &result) != 0)
    {
        ot_slot = &current_frame->ot[result.depth >> 2];
        if (villager->id == game_state.family.wifeVillager)
        {
            if (event_flag_get(EVENT_FLAG_SPOUSE_AT_HOME) != 0)
            {
                ot_slot -= 5;
            }
        }
        cue_draw_frame(&villager->cue, ot_slot, result.x, result.y - 40);
        sprite_draw_traced(ot_slot, current_frame->primCursor.ft4, &villager->sprite80, villager->sprite80.animation, villager->sprite80.frame,
                           result.x, result.y, villager->sprite80.scale, 0, 64);
        farm_draw_cursor(ot_slot, result.x, result.y, 0);
    }
    if ((game_state.globalFlags & GLOBAL_FLAG_PAUSED) == 0)
    {
        anim_tick(&villager->sprite80);
    }
}

/* The route grid, from the loaded data. */
static FarmRouteRow *villager_route_grid(FarmVillager *unused)
{
    return (FarmRouteRow *)(farm_area.sectionBuffer + ROUTE_GRID_OFFSET);
}

/* Move a villager onto the grid row of its current route step and run the cells
 * of that row from its column. Returns 1 when it is already there or its cell is an
 * entry-flagged one, else 0. */
static s32 villager_route_step_move(FarmVillager *villager, s32 command, s32 resume)
{
    FarmRouteRow *grid;
    FarmCell *cell;
    s32 placed;
    u16 flags;
    u16 kind;
    u8 row;

    grid = villager_route_grid(villager);
    if (!resume && villager->step != 0 &&
        villager->schedule[villager->step].place.location == villager->schedule[villager->step - 1].place.location &&
        (u8)villager->schedule[villager->step].route == villager->row && (villager->flags & VILLAGER_FLAG_ON_MAP))
    {
        return 1;
    }
    row = villager->schedule[villager->step].route;
    cell = grid[row];
    villager->row = row;
    villager->facing = cell->pose;
    placed = 0;
    if (cell->kind != ROUTE_CELL_KEPT_PLACE || !(villager->placeFlags & VILLAGER_PLACE_KEPT_PLACE))
    {
        placed = 1;
    }
    if (placed)
    {
        villager->position.vx = cell->x;
        villager->position.vy = cell->y;
        villager->position.vz = cell->z;
        villager->fixedPosition.vx = cell->x << 16;
        villager->fixedPosition.vy = cell->y << 16;
        villager->fixedPosition.vz = cell->z << 16;
        if (!resume)
        {
            villager->column = 1;
        }
    }
    if (cell->kind == ROUTE_CELL_KEPT_PLACE)
    {
        villager->column = 0;
        villager_cell_trigger(villager);
    }
    villager_head_for_cell(villager, &cell[villager->column]);
    villager_set_action(villager, VILLAGER_ACTION_STANDING, 0, -2);
    flags = villager->flags & ~VILLAGER_FLAG_BIT14;
    villager->flags = flags;
    kind = cell->kind;
    if (kind == ROUTE_CELL_ENTER_FLAGGED)
    {
        villager->flags = flags | VILLAGER_FLAG_BIT14;
        villager_set_action(villager, VILLAGER_ACTION_WALKING, villager->facing + 1, -2);
        if (villager_route_mode != 0)
        {
            villager->wait = cell->id;
        }
        return 1;
    }
    if (kind == ROUTE_CELL_KEPT_PLACE)
    {
        villager_set_action(villager, VILLAGER_ACTION_WALKING, villager->facing + 1, -2);
        return 0;
    }
    cell++;
    if (cell->kind == ROUTE_CELL_EVENT || cell->kind == ROUTE_CELL_EVENT_2 || cell->kind == ROUTE_CELL_EVENT_FLAGGED)
    {
        if (cell->kind == ROUTE_CELL_EVENT_FLAGGED)
        {
            villager->placeFlags |= VILLAGER_PLACE_EVENT_CELL;
        }
        villager->facing = cell->pose;
        if (command && !(villager->flags & VILLAGER_FLAG_IN_EVENT))
        {
            villager_event_cell_start(villager, cell);
            return 0;
        }
        cell++;
        villager->column++;
        villager_head_for_cell(villager, cell);
    }
    if (cell->kind == ROUTE_CELL_SHIPMENT)
    {
        villager_shipment_cell(villager, cell);
        cell++;
        villager->column++;
        villager_head_for_cell(villager, cell);
    }
    if (cell->kind == ROUTE_CELL_WAIT_WALK)
    {
        villager_set_action(villager, VILLAGER_ACTION_WALKING, villager->facing + 1, -2);
    }
    return 0;
}

/* Head the villager for `cell`: face the cell's pose (the previous cell's
 * once it is past the first column) and set its speed towards it; a cell
 * that starts an event, or one it stands on, stops it. */
static void villager_head_for_cell(FarmVillager *villager, FarmCell *cell)
{
    s32 diff_x;
    s32 diff_z;
    s32 angle;
    s32 speed;
    u16 type;

    type = cell->kind;
    if (type == ROUTE_CELL_EVENT || type == ROUTE_CELL_SHIPMENT || type == ROUTE_CELL_EVENT_2 || type == ROUTE_CELL_EVENT_FLAGGED)
    {
        villager->speed.vx = 0;
        villager->speed.vy = 0;
        villager->speed.vz = 0;
        return;
    }
    if (villager->column == 0)
    {
        villager->facing = cell->pose;
    }
    else
    {
        villager->facing = cell[-1].pose;
    }
    /* Offset by 1, so a villager standing exactly on the cell reads as (1, 1). */
    diff_x = (villager->position.vx - cell->x) * 1000 + 1;
    diff_z = (villager->position.vz - cell->z) * 1000 + 1;
    if (diff_x == 1 && diff_z == 1)
    {
        villager->speed.vx = 0;
        villager->speed.vy = 0;
        villager->speed.vz = 0;
        return;
    }
    angle = ratan2(diff_x, diff_z) + ANGLE_HALF_TURN;
    speed = villager_speed_outdoors;
    if (farm_area.mode.data->head.flags & AREA_FLAG_BIT12)
    {
        speed = villager_speed_indoors;
    }
    villager->speed.vx = speed * rsin(angle);
    villager->speed.vz = speed * rcos(angle);
}

/* In PLACE_13, every villager loses 1 friendship and 1 affection, not below 0, and the
 * player's counts70[0] counts up to 15. */
void villager_affection_decay(void)
{
    s32 remaining;
    s32 timer;
    FarmVillager *record;
    s16 current;
    u16 amount;

    if (farm_area.mode.data->head.place.value == PLACE_13)
    {
        current = game_state.player.counts70[0];
        amount = game_state.player.counts70[0];
        if (current < 15)
        {
            game_state.player.counts70[0] = amount + 1;
        }
        record = game_state.villagers;
        remaining = 36;
        do
        {
            record->friendship--;
            if (record->friendship < 0)
            {
                record->friendship = 0;
            }
            timer = record->affection - 1;
            if (timer < 0)
            {
                record->affection = 0;
            }
            else
            {
                record->affection = timer;
            }
            remaining--;
            record++;
        } while (remaining >= 0);
    }
}

/* Outdoor areas with a place entry: on the farm (PLACE_FARM) at 18 h the framebuffer
 * header is reloaded once. Other such areas clear every villager's delay at 12 h and
 * 18 h and, the first time at each, load resource 0xE and resume the villagers' walks. */
void farm_time_event_tick(void)
{
    FarmZoneHeader *head;
    s16 location;
    s16 lookup;
    head = &farm_area.mode.data->head;
    if (!(head->flags & AREA_FLAG_BIT12))
    {
        location = head->place.location;
        lookup = place_entries[location];
        if (lookup != 0x1000)
        {
            if (location == PLACE_FARM)
            {
                if (game_state.progress.clock.minuteOfDay == CLOCK_EVENING_MINUTE)
                {
                    if (event_flag_get(EVENT_FLAG_EVENING_18H) == 0)
                    {
                        farm_load_framebuffer_header();
                        event_flag_set(EVENT_FLAG_EVENING_18H, 1);
                    }
                }
            }
            else
            {
                if (game_state.progress.clock.minuteOfDay == CLOCK_NOON_MINUTE)
                {
                    villager_clear_delays();
                    if (event_flag_get(EVENT_FLAG_NOON_12H) == 0)
                    {
                        if (!(game_state.statusFlags & STATUS_FLAG_EVENT_HOLDS_RESOURCE_E))
                        {
                            farm_load_resource_id(0xE, 1);
                            farm_item_history_resume();
                        }
                        event_flag_set(EVENT_FLAG_NOON_12H, 1);
                    }
                }
                if (game_state.progress.clock.minuteOfDay == CLOCK_EVENING_MINUTE)
                {
                    villager_clear_delays();
                    if (event_flag_get(EVENT_FLAG_EVENING_18H) == 0)
                    {
                        if (!(game_state.statusFlags & STATUS_FLAG_EVENT_HOLDS_RESOURCE_E))
                        {
                            farm_load_resource_id(0xE, 1);
                            farm_item_history_resume();
                        }
                        event_flag_set(EVENT_FLAG_EVENING_18H, 1);
                    }
                }
            }
        }
    }
}

/* The villager reached an event cell: start the cell's event (once) with the
 * villager facing its way. */
static s32 villager_event_cell_start(FarmVillager *villager, FarmCell *cell)
{
    u16 flags;

    flags = villager->flags;
    if (!(flags & VILLAGER_FLAG_ON_MAP))
    {
        return 1;
    }
    if (flags & VILLAGER_FLAG_EVENT_STARTED)
    {
        return 0;
    }
    event_pending_pair_lookup(cell->id, villager->facing, 0, 1);
    villager->flags = villager->flags | (VILLAGER_FLAG_EVENT_PENDING | VILLAGER_FLAG_EVENT_STARTED);
    villager_set_action(villager, VILLAGER_ACTION_EVENT, villager->facing + 1, -2);
    return 1;
}

/* The villager reached a shipment cell: with cell id 0, take today's
 * shipment and, if the player is in its area, announce how it went. */
static void villager_shipment_cell(FarmVillager *villager, FarmCell *cell)
{
    s16 id;
    s32 pending;

    id = cell->id;
    if (id == 0)
    {
        if (event_flag_get(EVENT_FLAG_SHIPMENT_SETTLED) != 1)
        {
            pending = game_state.progress.estate.shipment.shippedValue;
            estate_shipment_settle();
            if (game_state.player.place.location == villager->place.location)
            {
                if (pending == 0)
                {
                    menu_request_open(1, SHIPMENT_MENU_NONE);
                }
                else if (pending < SHIPMENT_LARGE_VALUE)
                {
                    menu_request_open(1, SHIPMENT_MENU_SMALL);
                }
                else
                {
                    menu_request_open(1, SHIPMENT_MENU_LARGE);
                }
            }
        }
    }
}

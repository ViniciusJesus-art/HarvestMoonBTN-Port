/* The farm's animals, pets, spawned objects and walkers. */

#include <abs.h>
#include <sys/types.h>
#include <libgte.h>
#include <memory.h>
#include "types.h"
#include "actor.h"
#include "anim.h"
#include "animal_actions.h"
#include "animals.h"
#include "cd_audio.h"
#include "chicken.h"
#include "clock.h"
#include "cue.h"
#include "dog.h"
#include "doors.h"
#include "estate.h"
#include "event_commands.h"
#include "event_rules.h"
#include "farm.h"
#include "farm_main.h"
#include "farm_probe.h"
#include "farm_state.h"
#include "game_types.h"
#include "harvest_sprites.h"
#include "horse.h"
#include "items.h"
#include "livestock.h"
#include "main.h"
#include "menu_window.h"
#include "model.h"
#include "picture_cells.h"
#include "player.h"
#include "pond.h"
#include "scene.h"
#include "screen_fade.h"
#include "text.h"

typedef struct ChickenSpawnSource {
    u32 vx;
    u32 vy;
    u32 vz;
    u32 unusedC;
} ChickenSpawnSource;

typedef struct FarmActionSlot {
    s16 kind;
    u8 pad02[0x26 - 0x2];
    s16 taskNo;
    s16 taskId;
    s16 taskTime;
    s16 interruptTime;
    u8 pad2E[0x34 - 0x2E];
    s32 flags;
    u8 pad38[0x48 - 0x38];
    VECTOR payload48;
    u8 pad58[0xA8 - 0x58];
    ItemCue cue;
} FarmActionSlot;

/* One entry of the cue table farm_mode_cues. farm_cue_matches_mode scans for `id` and
 * answers whether the current mode bit (farm_season_mode_bit) is in `mask`. `x`, `y` and
 * `z` are where the object is created; `unused14` is always 0. */
typedef struct FarmModeCue {
    s16 id;
    s16 kind;
    u16 mask;
    u16 pad06;
    s32 x;
    s32 y;
    s32 z;
    s32 unused14;
} FarmModeCue;

/* One row of the spawn table farm_spawn_table: create object `kind` when the farm
 * mode equals `mode`, at (x, y, z). */
typedef struct FarmSpawn {
    s16 kind;
    s16 mode;
    s16 needsMatch;
    s32 x;
    s32 y;
    s32 z;
    u8 pad14[0x18 - 0x14];
} FarmSpawn;

typedef struct {
    s32 vx;
    s32 vy;
    s32 vz;
    s32 pad;
} FarmSpot;

static s32 farm_cue_matches_mode(s32 id);
static void farm_spawn_mode_object(void);
static void farm_spawn_table_objects(void);
static void farm_roll_stray_dogs(void);
static void farm_draw_unless_hidden(u32 flags, u_long *ot, u32 x, u32 y);
static u16 farm_walker_pick_heading(FarmActor *walker, s32 blocked, s32 flags);
static u16 farm_walker_pick_turn(u16 heading, u16 turn, u16 bias);
static u16 farm_heading_classify(FarmActor *state);
static void farm_update_zone_flag(FarmActor *object);
static void farm_objects_update(void);
static void farm_chickens_extra_update(void);
static void farm_actions_tick(void);
static void farm_cue_runners_step(void);
static s32 farm_check_movement(FarmActor *object, VECTOR pos, s16 mode, s32 state);
static s32 farm_timer_step(FarmActor *obj, s32 speed);
static void farm_mode_timer_tick(FarmActor *object, u16 *timer, u16 mode);
static void farm_mode_check_poll(void);
static void farm_mode_check(s32 mode);
static s32 farm_kind_draw_offset(s32 arg);
static void farm_walker_try_exits(FarmActor *walker);
static void farm_action_set_kind_flag(FarmAction *action);
static s32 farm_walker_cell_free(VECTOR *arg);
static s32 farm_season_mode_bit(void);
static void farm_actor_sprite_refresh(FarmActor *object);
static void farm_actor_pair_at_38(FarmAction *obj);

static SVECTOR farm_draw_point;
static FarmScreenXY farm_draw_screen_xy;
static FarmScreenXY farm_request_screen_xy;
static SVECTOR farm_request_draw_point;
static u32 farm_walker_phase;
/* Countdowns for the livestock, dog and horse checks. */
static u16 farm_mode_check_timers[3];
/* Cleared at the end of each day. */
static u16 farm_mode_cue_latch;
/* The running mode check, or -1 when none is running. */
static s16 farm_mode_check_handle;
static s32 farm_walker_not_stray;
static u16 farm_message_days[4];

/* Creates the farm action for an item kind and starts it. Returns the action, or 0 when the
 * kind has none. */
void *farm_action_create(s32 kind, s32 slot)
{
    void *action = 0;

    switch (kind) {
    case ITEM_CALF ... ITEM_COW:
        action = livestock_spawn(kind, slot, 1);
        break;
    case ITEM_VISITING_COW:
        action = farm_action_claim(ITEM_COW);
        break;
    case ITEM_LAMB ... ITEM_SHEEP:
        action = livestock_spawn_b(kind, slot, 1);
        break;
    case ITEM_VISITING_SHEEP:
        action = farm_action_claim(ITEM_SHEEP);
        break;
    case ITEM_CHICKEN ... ITEM_CHICK:
        action = chicken_spawn(kind, slot, 1);
        break;
    case ITEM_CAT:
    case ITEM_WHITE_RABBIT ... ITEM_SNAKE:
    case ITEM_BUTTERFLY ... ITEM_GRASSHOPPER:
    case ITEM_153 ... ITEM_157:
    case ITEM_159:
    case ITEM_STRAY_DOG:
    case ITEM_17A ... ITEM_17B:
        action = farm_action_claim(kind);
        break;
    case ITEM_ADULT_DOG ... ITEM_YOUNG_DOG:
        action = dog_start(kind, 1);
        break;
    case ITEM_PONY ... ITEM_HORSE:
        action = horse_spawn(kind);
        break;
    case ITEM_LARGE_FISH ... ITEM_SMALL_FISH:
        pond_add_fish(kind);
        break;
    }
    if (action == 0) {
        return 0;
    }
    farm_action_set_kind_flag(action);
    return action;
}

/* Per-frame farm animal update, skipped under DEBUG_FLAG_NO_ANIMALS. From 18:10 to 22:59 on the
 * farm a stray dog may be created (ESTATE_FLAG_STRAY_DOGS, no dog walk), and the pending CD
 * track plays once the CD and the menu are idle. */
void farm_animals_tick(void) {
    s16 mode;

    if ((game_state.debugFlags & DEBUG_FLAG_NO_ANIMALS) == 0) {
        farm_actions_tick();
        farm_chickens_extra_update();
        farm_objects_update();
        farm_cue_runners_step();
        farm_mode_check_poll();
        if (farm_area.mode.place.location == PLACE_FARM) {
            mode = game_state.progress.clock.time.hm.hour;
            if (mode >= 18) {
                if (mode < 23 && (game_state.progress.estate.estateFlags & (ESTATE_FLAG_STRAY_DOGS | ESTATE_FLAG_DOG_WALK)) == ESTATE_FLAG_STRAY_DOGS) {
                    if (mode == 18) {
                        if (game_state.progress.clock.time.hm.minute >= 10) {
                            farm_action_create(ITEM_STRAY_DOG, -1);
                        }
                    } else {
                        farm_action_create(ITEM_STRAY_DOG, -1);
                    }
                }
            }
        }
        if (game_state.animals.pendingTrack != 0) {
            if (cd_audio_get_state() == CD_AUDIO_IDLE && game_state.system.menuState == MENU_STATE_IDLE) {
                cd_audio_play_track(game_state.animals.pendingTrack);
                game_state.animals.pendingTrack = 0;
            }
        }
    }
}

/* Reset the farm's record tables for a new game: clear the chicken, livestock, pond, action,
 * extra chicken and horse records, put the young dog on the farm, then reset the mode-check
 * timers, the mode slots, the spawn gate and the pending track. */
void farm_reset_tables(void)
{
    FarmLivestock *item;
    s32 i;
    u32 j;

    memset(game_state.animals.chickens, 0, sizeof(game_state.animals.chickens));
    for (i = 0; i < MAX_CHICKENS; i++) {
        game_state.animals.chickens[i].actor.kind = ITEM_NOTHING;
        game_state.animals.chickens[i].actor.name[0] = 0xFFFF;
    }
    memset(game_state.animals.livestock, 0, sizeof(game_state.animals.livestock));
    for (i = 0; i < MAX_LIVESTOCK; i++) {
        game_state.animals.livestock[i].actor.kind = ITEM_NOTHING;
        game_state.animals.livestock[i].actor.name[0] = 0xFFFF;
    }
    for (i = 0; i < MAX_LIVESTOCK; i++) {
        game_state.animals.livestock[i].actor.kind = ITEM_NOTHING;
        game_state.animals.livestock[i].actor.name[0] = 0xFFFF;
    }
    memset(&game_state.animals.pond, 0, sizeof(game_state.animals.pond));
    for (i = 0; i < 99; i++) {
        game_state.animals.pond.fish[i].item = ITEM_NOTHING;
    }
    memset(game_state.animals.actions, 0, sizeof(game_state.animals.actions));
    for (i = 0; i < 6; i++) {
        game_state.animals.actions[i].actor.kind = ITEM_NOTHING;
        game_state.animals.actions[i].actor.name[0] = 0xFFFF;
    }
    for (i = 0; i < 3; i++) {
        game_state.animals.extraChickens[i].actor.kind = ITEM_NOTHING;
        game_state.animals.extraChickens[i].actor.name[0] = 0xFFFF;
    }
    game_state.animals.dog.actor.kind = ITEM_NOTHING;
    game_state.animals.dog.actor.flags = 0;
    item = farm_action_create(ITEM_YOUNG_DOG, 0);
    item->actor.position.vx = -1800;
    item->actor.position.vy = 0;
    item->actor.position.vz = 3100;
    item->actor.place.value = 0;
    item->actor.place.location = PLACE_FARM;
    memset(&game_state.animals.horse, 0, sizeof(game_state.animals.horse));
    game_state.animals.horse.actor.kind = ITEM_NOTHING;
    game_state.animals.horse.actor.name[0] = 0xFFFF;
    farm_spawn_chicken_sources();
    for (j = 0; j < 3; j++) {
        farm_mode_check_timers[j] = 300;
    }
    farm_mode_check_handle = -1;
    for (i = 0; i < 4; i++) {
        game_state.animals.slots[i].value = SLOT_NONE;
        game_state.animals.slots[i].animal = SLOT_NONE;
    }
    pond_clear();
    game_state.animals.spawnGate = 0;
    game_state.animals.pendingTrack = 0;
    game_state.statusFlags &= ~STATUS_FLAG_SKIP_ANIMAL_UPDATE;
}

/* Ticks the chickens' weather timers, the livestock action timers (both passes) and the training
 * boost of each action. Does nothing while STATUS_FLAG_SKIP_ANIMAL_UPDATE is set. */
void farm_cleanup_records(void) {
    FarmChicken *record;
    s32 i;

    if (game_state.statusFlags & STATUS_FLAG_SKIP_ANIMAL_UPDATE) {
        return;
    }
    for (i = 0; i < MAX_CHICKENS; i++) {
        record = &game_state.animals.chickens[i];
        if (record->actor.kind != ITEM_NOTHING) {
            chicken_weather_tick(record);
        }
    }
    for (i = 0; i < MAX_LIVESTOCK; i++) {
        if (*(u16 *)&game_state.animals.livestock[i] != 0) {
            livestock_action_timer(&game_state.animals.livestock[i]);
        }
    }
    for (i = 0; i < MAX_LIVESTOCK; i++) {
        if (*(u16 *)&game_state.animals.livestock[i] != 0) {
            livestock_action_timer_b(&game_state.animals.livestock[i]);
        }
    }
    for (i = 0; i < 6; i++) {
        farm_event_training_boost(&game_state.animals.actions[i]);
    }
}

/* End-of-day pass over the farm: the daily ticks of the chickens, livestock, dog, horse and pond
 * (skipped under STATUS_FLAG_SKIP_ANIMAL_UPDATE), the age count of the animals aged today, then
 * the day's visitor and stray-dog flags are cleared, the stray dogs are rolled, and the
 * animals and chicken sources are placed again. */
void farm_end_of_day(void)
{
    FarmState *farm = &game_state;
    FarmChicken *records;
    FarmState *clock;
    FarmState *state;
    FarmPet *slot;
    s32 days;
    s32 count;
    u32 flags;
    s32 i;

    farm->progress.estate.estateFlags &= ~ESTATE_FLAG_ANIMAL_AGED;
    if (!(farm->statusFlags & STATUS_FLAG_SKIP_ANIMAL_UPDATE)) {
        records = farm->animals.chickens;
        for (i = 0; i < MAX_CHICKENS; i++) {
            if (records[i].actor.kind != ITEM_NOTHING || (records[i].actor.flags & ACTOR_FLAG_CLAIMED)) {
                chicken_daily_tick(&records[i]);
            }
        }
        for (i = 0; i < MAX_LIVESTOCK; i++) {
            if (*(u16 *)&game_state.animals.livestock[i] != 0) {
                livestock_overnight_update(&game_state.animals.livestock[i]);
            }
        }
        for (i = 0; i < MAX_LIVESTOCK; i++) {
            if (*(u16 *)&game_state.animals.livestock[i] != 0) {
                livestock_overnight_update_alt(&game_state.animals.livestock[i]);
            }
        }
        slot = &game_state.animals.dog;
        if (slot->actor.kind != ITEM_NOTHING) {
            dog_tick(slot);
        }
        slot++;
        if (slot->actor.kind != ITEM_NOTHING) {
            horse_update_bond(slot);
        }
        pond_daily_update();
    }
    clock = &game_state;
    if (clock->progress.estate.estateFlags & ESTATE_FLAG_ANIMAL_AGED) {
        count = (u16)clock->player.counts70[2] + 1;
        clock->player.counts70[2] = count;
        if ((s16)count >= 16) {
            days = 15;
        } else {
            days = count;
        }
        clock->player.counts70[2] = days;
    } else {
        clock->player.counts70[2] = 0;
    }
    state = &game_state;
    flags = state->progress.estate.estateFlags;
    flags &= ~ESTATE_FLAG_VISITORS;
    flags &= ~ESTATE_FLAG_STRAY_DOGS;
    flags &= ~ESTATE_FLAG_DOG_WALK;
    state->progress.estate.estateFlags = flags;
    farm_mode_cue_latch = 0;
    state->statusFlags &= ~STATUS_FLAG_SKIP_ANIMAL_UPDATE;
    farm_roll_stray_dogs();
    farm_items_apply_kind(1);
    farm_items_apply_kind(0);
    farm_put_back_blocked_animals();
    if (state->world.weatherTomorrow == 0) {
        farm_spawn_chicken_sources();
    } else {
        for (i = 0; i < 3; i++) {
            state->animals.extraChickens[i].actor.kind = ITEM_NOTHING;
            state->animals.extraChickens[i].actor.name[0] = 0xFFFF;
        }
    }
}

/* Start an action slot. Returns 1 when its kind is not a field object; 2 while its 0x100000 flag
 * waits for room (the flag is cleared and the dog set idle once there is room); otherwise resets
 * the task state and returns 0. */
s32 farm_action_begin(FarmActionSlot *self) {
    s32 flags;

    if (item_is_field_object(self->kind) == 0) {
        return 1;
    }
    flags = self->flags;
    if (flags & 0x100000) {
        if (event_cmd_has_room(1, 0, 0, 0) == 1) {
            dog_set_phase(&game_state.animals.dog, DOG_PHASE_IDLE_ACTION);
            self->flags = self->flags & 0xFFEFFFFF;
        }
        return 2;
    }
    self->flags = (flags | 0x20000) & ~0x2000;
    self->taskNo = 0;
    self->taskId = 0;
    self->taskTime = 0;
    self->interruptTime = 0;
    farm_vector_adjust(4, &self->payload48, 0);
    cue_reset(&self->cue);
    return 0;
}

static s32 farm_actor_place_at(FarmActor *dest, LimitPair *place, VECTOR *position) {
    dest->flags &= ~ACTOR_FLAG_HELD;
    dest->place = *place;
    dest->position.vx = position->vx;
    dest->position.vy = position->vy;
    dest->position.vz = position->vz;
    dest->position.pad = position->pad;
    farm_settle_on_ground(&dest->position, 0);
    return 1;
}

/* Hand `position`/`block` to farm_actor_place_at for the held object of kind `kind`: a chicken,
 * a livestock or the dog, or else an action slot of that kind, which is then cleared when the
 * current mode no longer wants it. */
s32 farm_place_held(s16 kind, void *position, void *block) {
    FarmChicken *item;
    FarmLivestock *tool;
    FarmAction *actor;
    s32 i;
    s32 slot;

    if ((u32)(kind - ITEM_CHICKEN) < 2) {
        item = game_state.animals.chickens;
        for (i = 0; i < MAX_CHICKENS; i++, item++) {
            if (item->actor.flags & ACTOR_FLAG_HELD) {
                return farm_actor_place_at(&item->actor, position, block);
            }
        }
        return 1;
    }
    if ((u32)(kind - ITEM_YOUNG_COW) < 2) {
        tool = game_state.animals.livestock;
        for (i = 0; i < MAX_LIVESTOCK; i++, tool++) {
            if (tool->actor.kind == kind && (tool->actor.flags & ACTOR_FLAG_HELD)) {
                return farm_actor_place_at(&tool->actor, position, block);
            }
        }
        return 1;
    }
    if ((u32)(kind - ITEM_ADULT_DOG) < 2) {
        FarmPet *dog = &game_state.animals.dog;

        if (!(dog->actor.flags & ACTOR_FLAG_HELD)) {
            return 1;
        }
        dog_set_phase(dog, DOG_PHASE_PLACED);
        return farm_actor_place_at(&dog->actor, position, block);
    }
    actor = game_state.animals.actions;
    for (slot = 0; slot < 6; slot++, actor++) {
        if (actor->actor.kind == kind && (actor->actor.flags & ACTOR_FLAG_HELD)) {
            actor->actor.sprite.source = 0;
            switch (actor->actor.kind) {
            case ITEM_CRICKET:
            case ITEM_LADY_BUG:
            case ITEM_GRASSHOPPER:
                farm_actor_set_task(&actor->actor, FARM_TASK_PLACED_CRICKET);
                actor->actor.facing = game_state.player.facing38;
                break;
            case ITEM_BEETLE:
            case ITEM_STAG_BEETLE:
                farm_actor_set_task(&actor->actor, FARM_TASK_PLACED_BEETLE);
                actor->actor.facing = game_state.player.facing38;
                break;
            case ITEM_CICADA:
            case ITEM_DRAGONFLY:
                farm_actor_set_task(&actor->actor, FARM_TASK_PLACED_CICADA);
                actor->actor.facing = game_state.player.facing38;
                break;
            }
            farm_actor_place_at(&actor->actor, position, block);
            if (farm_cue_matches_mode(actor->actor.kind) == 0) {
                farm_action_clear(actor);
            }
        }
    }
    return 1;
}

static u16 farm_turn_weights[4][4][9][4] = {
    {
        {
            {80, 10, 0, 10},
            {0, 10, 0, 90},
            {50, 0, 0, 50},
            {50, 50, 0, 0},
            {0, 90, 0, 10},
            {0, 50, 0, 50},
            {0, 0, 0, 100},
            {100, 0, 0, 0},
            {0, 100, 0, 0},
        },
        {
            {80, 0, 0, 20},
            {20, 0, 0, 80},
            {50, 0, 0, 50},
            {80, 0, 0, 20},
            {50, 0, 0, 50},
            {10, 0, 0, 90},
            {0, 0, 0, 100},
            {100, 0, 0, 0},
            {90, 0, 0, 10},
        },
        {
            {80, 20, 0, 0},
            {50, 50, 0, 0},
            {80, 20, 0, 0},
            {50, 50, 0, 0},
            {20, 80, 0, 0},
            {10, 90, 0, 0},
            {90, 10, 0, 0},
            {100, 0, 0, 0},
            {0, 100, 0, 0},
        },
        {
            {0, 50, 0, 50},
            {0, 20, 0, 80},
            {0, 0, 0, 100},
            {0, 100, 0, 0},
            {0, 80, 0, 20},
            {0, 50, 0, 50},
            {0, 0, 0, 100},
            {0, 50, 0, 50},
            {0, 100, 0, 0},
        },
    },
    {
        {
            {10, 80, 10, 0},
            {10, 0, 90, 0},
            {90, 0, 10, 0},
            {50, 50, 0, 0},
            {0, 50, 50, 0},
            {0, 0, 100, 0},
            {50, 0, 50, 0},
            {100, 0, 0, 0},
            {0, 100, 0, 0},
        },
        {
            {20, 80, 0, 0},
            {50, 50, 0, 0},
            {80, 20, 0, 0},
            {50, 50, 0, 0},
            {20, 80, 0, 0},
            {10, 90, 0, 0},
            {90, 10, 0, 0},
            {100, 0, 0, 0},
            {0, 100, 0, 0},
        },
        {
            {0, 80, 20, 0},
            {0, 20, 80, 0},
            {0, 50, 50, 0},
            {0, 80, 20, 0},
            {0, 50, 50, 0},
            {0, 0, 100, 0},
            {0, 10, 90, 0},
            {0, 90, 10, 0},
            {0, 100, 0, 0},
        },
        {
            {50, 0, 50, 0},
            {20, 0, 80, 0},
            {80, 0, 20, 0},
            {100, 0, 0, 0},
            {0, 0, 100, 0},
            {0, 0, 100, 0},
            {50, 0, 50, 0},
            {100, 0, 0, 0},
            {50, 0, 50, 0},
        },
    },
    {
        {
            {0, 10, 80, 10},
            {0, 0, 50, 50},
            {0, 10, 0, 90},
            {0, 90, 0, 10},
            {0, 50, 50, 0},
            {0, 0, 100, 0},
            {0, 0, 0, 100},
            {0, 50, 0, 50},
            {0, 100, 0, 0},
        },
        {
            {0, 20, 80, 0},
            {0, 20, 80, 0},
            {0, 50, 50, 0},
            {0, 80, 20, 0},
            {0, 50, 50, 0},
            {0, 0, 100, 0},
            {0, 10, 90, 0},
            {0, 90, 10, 0},
            {0, 100, 0, 0},
        },
        {
            {0, 0, 80, 20},
            {0, 0, 50, 50},
            {0, 0, 20, 80},
            {0, 0, 50, 50},
            {0, 0, 80, 20},
            {0, 0, 100, 0},
            {0, 0, 0, 100},
            {0, 0, 10, 90},
            {0, 0, 90, 10},
        },
        {
            {0, 50, 0, 20},
            {0, 0, 0, 50},
            {0, 20, 0, 80},
            {0, 80, 0, 50},
            {0, 100, 0, 20},
            {0, 50, 0, 0},
            {0, 0, 0, 100},
            {0, 50, 0, 90},
            {0, 100, 0, 10},
        },
    },
    {
        {
            {10, 0, 10, 80},
            {0, 0, 50, 50},
            {50, 0, 0, 50},
            {90, 0, 10, 0},
            {10, 0, 90, 0},
            {0, 0, 100, 0},
            {0, 0, 0, 100},
            {100, 0, 0, 0},
            {50, 0, 50, 0},
        },
        {
            {0, 0, 20, 80},
            {0, 0, 50, 50},
            {0, 0, 20, 80},
            {0, 0, 50, 50},
            {0, 0, 80, 20},
            {0, 0, 100, 0},
            {0, 0, 0, 100},
            {0, 0, 10, 90},
            {0, 0, 90, 10},
        },
        {
            {20, 0, 0, 80},
            {20, 0, 0, 80},
            {50, 0, 0, 50},
            {80, 0, 0, 20},
            {50, 0, 0, 50},
            {10, 0, 0, 90},
            {0, 0, 0, 100},
            {100, 0, 0, 0},
            {90, 0, 0, 10},
        },
        {
            {50, 0, 50, 0},
            {0, 0, 100, 0},
            {100, 0, 0, 0},
            {80, 0, 20, 0},
            {20, 0, 80, 0},
            {0, 0, 100, 0},
            {50, 0, 50, 0},
            {100, 0, 0, 0},
            {50, 0, 50, 0},
        },
    },
};

/* Distance on both axes under which an approaching object stops homing (farm_heading_classify);
 * farm_set_proximity sets it. */
static s16 farm_proximity_threshold = 600;

static FarmModeCue farm_mode_cues[17] = {
    {0x010c, 0x34, 0x07, 0, -12000, 0, 7000, 0},    {0x010b, 0x34, 0x08, 0, -12000, 0, 7000, 0},
    {0x010d, 0x0c, 0x07, 0, -7000, -1892, 7061, 0}, {0x010e, 0x0d, 0x0c, 0, -13000, -1751, 6000, 0},
    {0x010f, 0x0c, 0x08, 0, -7000, -1892, 9000, 0}, {0x0110, 0x0d, 0x01, 0, -5500, -2036, 7250, 0},
    {0x011a, 0x0e, 0x01, 0, -7200, -280, 6000, 0},  {0x0119, 0x0e, 0x01, 0, -8000, -280, 5500, 0},
    {0x0114, 0x0e, 0x02, 0, -8000, -280, 5500, 0},  {0x0115, 0x0d, 0x02, 0, -3100, -2050, 6300, 0},
    {0x0116, 0x0e, 0x02, 0, -7200, -280, 5500, 0},  {0x0118, 0x0e, 0x04, 0, -10000, -280, 6300, 0},
    {0x0117, 0x0e, 0x04, 0, -10850, -280, 5400, 0}, {0x0163, 0x08, 0x07, 0, 2000, 0, -3000, 0},
    {0x0163, 0x08, 0x07, 0, -200, 0, -200, 0},      {0x0166, 0x08, 0x07, 0, 200, 0, -2600, 0},
    {0x0166, 0x08, 0x07, 0, 2000, 0, -1000, 0},
};

/* Does the cue for `id` apply to the current mode? */
static s32 farm_cue_matches_mode(s32 id) {
    FarmModeCue *cue;
    s16 mode;
    s32 i;

    mode = farm_season_mode_bit();
    cue = farm_mode_cues;
    i = 0;
    do {
        if (cue->id != id) {
            i++;
            cue++;
        } else {
            return (mode & cue->mask) != 0;
        }
    } while (i < 17);
    return 0;
}

/* Clear ACTOR_FLAG_HELD on `obj` unless the player still holds an item of its kind (held[1]). */
void farm_actor_release_held(FarmActor *obj) {
    s32 flags = obj->flags;
    if ((flags & ACTOR_FLAG_HELD) && (game_state.player.held[1].id != obj->kind)) {
        obj->flags = flags & ~ACTOR_FLAG_HELD;
    }
}

void farm_mode_check_start(s16 handle) {
    farm_mode_check_handle = handle;
}

/* Put the horse at `position` on the place `cell` (location, then value) and reset its cue. */
void farm_horse_place(FarmPet *horse, u16 *cell, VECTOR *position) {
    VECTOR *pos = &game_state.animals.horse.actor.position;

    pos->vx = position->vx;
    pos->vy = position->vy;
    pos->vz = position->vz;
    ACTOR_OF_POSITION(pos)->place.location = cell[1];
    ACTOR_OF_POSITION(pos)->place.value = cell[0];
    cue_reset(&horse->actor.cue);
}

/* Clear the action slots that are not flagged 0x20000 (every slot when `force` is set), except a
 * slot whose kind has a bit of 0x15B while the dog walk is on. */
void farm_actions_clear(s32 force) {
    FarmAction *entry;
    u32 status_mask;
    FarmState *farm;
    s32 remaining;
    u32 global_mask;

    entry = game_state.animals.actions;
    status_mask = 0x20000;
    farm = &game_state;
    remaining = 5;
    global_mask = ESTATE_FLAG_DOG_WALK;
    do {
        if ((entry->actor.flags & status_mask) == 0 || force != 0) {
            if ((entry->actor.kind & 0x15B) == 0 || (farm->progress.estate.estateFlags & global_mask) == 0) {
                farm_action_clear(entry);
            }
        }
        remaining--;
        entry++;
    } while (remaining >= 0);
}

/* Three positions for the daily visitors. */
static VECTOR farm_visitor_positions[3] = {{-4000, 0, 4500}, {-3600, 0, 4200}, {-4000, 0, 3900}};

/* Daily spawn of the farm's wild visitors. Nothing else spawns in bad weather. Cows and sheep
 * always come; the other cues of farm_mode_cues come one day in ten and then latch for the day.
 * On the farm, from PLACE_2 between 6 and 8 h, three ITEM_VISITORs come one day in ten outside
 * winter. From 18 to 22 h a stray dog comes if ESTATE_FLAG_STRAY_DOGS is set and no dog walk is on. */
void farm_spawn_daily_visitors(void)
{
    FarmModeCue *cue;
    FarmAction *object;
    s32 i;
    s16 mode;

    farm_spawn_mode_object();
    farm_spawn_table_objects();
    if (game_state.world.weather != 0) {
        return;
    }
    mode = farm_season_mode_bit();
    cue = farm_mode_cues;
    for (i = 0; i < 17; i++, cue++) {
        if (((s16)farm_mode_cue_latch >> i) & 1) {
            continue;
        }
        if (farm_area.mode.place.location != cue->kind) {
            continue;
        }
        if (!(mode & cue->mask)) {
            continue;
        }
        if (cue->id != ITEM_VISITING_COW && cue->id != ITEM_VISITING_SHEEP && rand_range(10) != 0) {
            continue;
        }
        object = farm_action_create(cue->id, -1);
        if (object == 0) {
            continue;
        }
        object->actor.position.vx = cue->x;
        object->actor.position.vy = cue->y;
        object->actor.position.vz = cue->z;
        if (rand_range(2) != 0) {
            object->actor.facing = 0;
        } else {
            object->actor.facing = 3;
        }
        if (cue->id != ITEM_VISITING_COW && cue->id != ITEM_VISITING_SHEEP) {
            farm_mode_cue_latch |= 1 << i;
        }
    }
    if (farm_area.mode.place.location == PLACE_FARM && farm_area.mode.previous->place.location == PLACE_2) {
        if (game_state.progress.clock.time.hm.hour >= 6)
        if (game_state.progress.clock.time.hm.hour < 9)
        if (rand_range(10) == 0 && game_state.progress.clock.date.season != SEASON_WINTER && !(farm_mode_cue_latch >> 15)) {
            for (i = 0; i < 3; i++) {
                object = farm_action_create(ITEM_VISITOR, -1);
                if (object != 0) {
                    object->actor.position.vx = farm_visitor_positions[i].vx;
                    object->actor.position.vy = farm_visitor_positions[i].vy;
                    object->actor.position.vz = farm_visitor_positions[i].vz;
                    object->actor.facing = i;
                }
                game_state.progress.estate.estateFlags |= ESTATE_FLAG_VISITORS;
            }
        }
    }
    if (farm_area.mode.place.location == PLACE_FARM && farm_area.mode.previous->place.location != PLACE_C) {
        if (game_state.progress.clock.time.hm.hour >= 18) {
            if (game_state.progress.clock.time.hm.hour < 23 && (game_state.progress.estate.estateFlags & (ESTATE_FLAG_STRAY_DOGS | ESTATE_FLAG_DOG_WALK)) == ESTATE_FLAG_STRAY_DOGS) {
                farm_action_create(ITEM_STRAY_DOG, -1);
            }
        }
    }
    if (0) {
        text_draw_number(0, 0, 0, 0, 0);
    }
}

/* Cat spawn positions: [0] for PLACE_1F, [1] for PLACE_1B. */
static VECTOR farm_mode_spawn_positions[2] = {
    -800, 0, -400, 0, -700, -800, 2500, 0};

/* In PLACE_1B and PLACE_1F, spawn a cat (ITEM_CAT) at that place's position, facing 0 or 3. */
static void farm_spawn_mode_object(void)
{
    FarmLoadArea *area;
    FarmActor *object;
    VECTOR pos;

    area = &farm_area;
    switch (area->mode.place.location) {
    case PLACE_1F:
        pos = farm_mode_spawn_positions[0];
        break;
    case PLACE_1B:
        pos = farm_mode_spawn_positions[1];
        break;
    case PLACE_1C:
    case PLACE_1E:
    default:
        return;
    }
    object = farm_action_create(ITEM_CAT, -1);
    if (object == 0) {
        return;
    }
    object->position.vx = pos.vx;
    object->position.vy = pos.vy;
    object->position.vz = pos.vz;
    if (rand_range(2) != 0) {
        object->facing = 0;
    } else {
        object->facing = 3;
    }
}

static FarmSpawn farm_spawn_table[6] = {
    {ITEM_17B, 9, 0, 300, 0, -400},
    {ITEM_17A, 9, 0, -300, 0, -1400},
    {ITEM_17A, 9, 1, -300, 0, -1200},
    {ITEM_17A, 24, 1, -800, 0, 800},
    {ITEM_17A, 25, 1, -300, 0, 900},
    {ITEM_17A, 53, 1, -1100, 0, 0},
};

/* Walk the six-row spawn table and create each object whose mode equals the current place (kind
 * ITEM_17A only while the spawn gate is set and, for rows that ask for it, equal to the current
 * place), placing it and randomly setting its facing to 0 or 2. */
static void farm_spawn_table_objects(void)
{
    FarmSpawn *spawn;
    FarmActor *object;
    s32 i;
    FarmState *state;
    FarmModeGlobal *farm;

    spawn = farm_spawn_table;
    farm = &farm_area.mode;
    state = &game_state;
    for (i = 5; i >= 0; i--, spawn++) {
        if (farm->place.location != spawn->mode)
            continue;
        if (spawn->kind == ITEM_17A) {
            if (state->animals.spawnGate == 0)
                continue;
            if (spawn->needsMatch != 0 && state->animals.spawnGate != farm->place.location)
                continue;
        }
        object = farm_action_create(spawn->kind, -1);
        if (object == 0)
            continue;
        object->position.vx = spawn->x;
        object->position.vy = spawn->y;
        object->position.vz = spawn->z;
        if (rand_range(2) != 0)
            object->facing = 0;
        else
            object->facing = 2;
    }
}

/* Create one farm object at `pos` on a 1 in 50 roll, outside bad weather and winter: ITEM_156 in
 * mode 1, else ITEM_157. Mode 1 faces the player; the others face 0 or 3 at random. */
void farm_create_object(VECTOR pos, s32 mode) {
    FarmState *farm;
    FarmActor *object;
    s32 object_id;
    s32 value;
    farm = &game_state;
    if (farm->world.weather != 0) return;
    if (rand_range(50) != 0) return;
    if (farm->progress.clock.date.season == SEASON_WINTER) return;
    if (mode == 1)
        object_id = ITEM_156;
    else
        object_id = ITEM_157;
    object = farm_action_create(object_id, -1);
    if (object == 0) return;
    object->position.vx = pos.vx;
    object->position.vy = pos.vy;
    object->position.vz = pos.vz;
    if (mode == 1) {
        object->facing = farm->player.facing38;
        return;
    }
    if (rand_range(2) == 0)
        value = 3;
    else
        value = 0;
    object->facing = value;
}

/* Roll a stray dog walk for the day (one day in ten): needs a free action slot in pool 0 or 1,
 * no bad weather and no winter. Sets ESTATE_FLAG_STRAY_DOGS. */
static void farm_roll_stray_dogs(void) {
    s32 v;
    v = farm_action_pick_slot(0);
    if (v < 0) {
        v = farm_action_pick_slot(1);
        if (v < 0) return;
    }
    if (game_state.world.weather != 0) return;
    if (game_state.progress.clock.date.season == SEASON_WINTER) return;
    if (rand_range(10) != 0) return;
    game_state.progress.estate.estateFlags |= ESTATE_FLAG_STRAY_DOGS;
}

/* Is a dead farm item of a kind `kind` selects? `kind` bit 1 matches items that died of age, bit 2
 * the others. `mode` picks the tables: bit 4 the chickens, and bits 1 and 2 the livestock (bit 2
 * for records with ACTOR_FLAG_BIT16, bit 1 for the rest). */
s32 farm_kind_selected(s32 mode, u32 kind)
{
    s32 farm_kind_matches_flags(FarmActor *object, u32 kind)
    {
        u32 flags;
        u32 diedOfAge;

        flags = object->flags;
        diedOfAge = flags & ACTOR_FLAG_DIED_OF_AGE;
        if (flags & ACTOR_FLAG_DIED) {
            if ((kind & 1) && diedOfAge == 1) {
                return 1;
            }
            if ((kind & 2) && diedOfAge == 0) {
                return 1;
            }
        }
        return 0;
    }

    s32 i;

    if (mode & 3) {
        for (i = 0; i < MAX_LIVESTOCK; i++) {
            if ((game_state.animals.livestock[i].actor.flags & ACTOR_FLAG_BIT16) ? (mode & 2) : (mode & 1)) {
                if (farm_kind_matches_flags(&game_state.animals.livestock[i].actor, kind)) {
                    return 1;
                }
            }
        }
    }
    if (mode & 4) {
        for (i = 0; i < MAX_CHICKENS; i++) {
            if (farm_kind_matches_flags(&game_state.animals.chickens[i].actor, kind)) {
                return 1;
            }
        }
    }
    return 0;
}

static inline void count_discarded(void) {
    game_state.player.counts70[3]++;
    if (game_state.player.counts70[3] >= 16) {
        game_state.player.counts70[3] = 15;
    }
    game_state.progress.estate.discardCount++;
    game_state.progress.estate.discardCount = game_state.progress.estate.discardCount < 100 ? game_state.progress.estate.discardCount : 99;
}

/* Clears every item record flagged ACTOR_FLAG_DIED, counting each one as discarded; a held dead
 * chicken is dropped first. */
void farm_clear_flagged_items(void) {
    s32 i;
    FarmLivestock *record;
    FarmChicken *item;

    record = game_state.animals.livestock;
    for (i = 0; i < MAX_LIVESTOCK; i++, record++) {
        if (record->actor.flags & ACTOR_FLAG_DIED) {
            livestock_clear_record(record);
            count_discarded();
        }
    }
    item = game_state.animals.chickens;
    for (i = 0; i < MAX_CHICKENS; i++, item++) {
        if (item->actor.flags & ACTOR_FLAG_DIED) {
            if (item->actor.flags & ACTOR_FLAG_HELD) {
                game_state.player.stateFlags &= ~(PLAYER_FLAG_SHOWING_ITEM | PLAYER_FLAG_CARRY_ITEM);
                player_reset_pose();
                player_drop_held_item(&game_state.player);
            }
            chicken_clear(item);
            count_discarded();
        }
    }
}

static ChickenSpawnSource chicken_spawn_sources[3] = {
    {-2700, 0, 2500, 0},
    {-2000, 0, 1400, 0},
    {-2500, 0, 250, 0},
};

/* Spawn the three daily chickens at chicken_spawn_sources, on place 0x30, facing 0, 1 and 2. */
void farm_spawn_chicken_sources(void) {
    s32 count;
    s32 place;
    FarmChicken *result;

    count = 0;
    place = 0x30;
    do {
        result = chicken_spawn(ITEM_CHICKEN, -1, 0);
        if (result != 0) {
            result->actor.place.location = place;
            result->actor.facing = count;
            result->actor.position.vx = chicken_spawn_sources[count].vx;
            result->actor.position.vy = chicken_spawn_sources[count].vy;
            result->actor.position.vz = chicken_spawn_sources[count].vz;
        }
        count = count + 1;
    } while (count < 3);
}

/* Draw an animal: project its position, draw its cue and sprite (and its cursor on the ground
 * when it is jumping), then animate its sprite unless the farm is paused. Nothing is drawn
 * during a transition. */
void farm_draw_actor(FarmActor *object) {
    FarmScreenPoint result;
    u_long *primitive;

    if ((game_state.globalFlags & GLOBAL_FLAG_TRANSITION) == 0) {
        farm_draw_point.vx = object->position.vx + 150;
        farm_draw_point.vy = object->position.vy - 100;
        farm_draw_point.vz = object->position.vz - 150;
        if (estate_point_project(&farm_draw_point, &result) != 0) {
            primitive = &current_frame->ot[result.depth >> 2];
            cue_draw_frame(&object->cue, primitive, result.x,
                          result.y + farm_kind_draw_offset((s16)object->kind));
            sprite_draw_traced(primitive, current_frame->primCursor.ft4, &object->sprite, object->sprite.animation,
                          object->sprite.frame, result.x, result.y, object->sprite.scale, 0, 0x40);
            if ((object->flags & ACTOR_FLAG_JUMPING) != 0) {
                farm_draw_point.vx = object->position.vx + 150;
                farm_draw_point.vy = -100;
                farm_draw_point.vz = object->position.vz - 150;
                primitive =
                    &current_frame->ot[gte_project_vertex(&farm_draw_screen_xy, &farm_draw_point) >> 2];
            } else {
                farm_draw_screen_xy.x = *&result.x;
                farm_draw_screen_xy.y = *&result.y;
            }
            farm_draw_unless_hidden(object->flags, primitive, farm_draw_screen_xy.x, farm_draw_screen_xy.y);
        }
        if ((game_state.globalFlags & GLOBAL_FLAG_PAUSED) == 0) {
            anim_tick(&object->sprite);
        }
    }
}

/* Draw a helper: its cue, the sprite of the item it carries (when it has one) and its own sprite,
 * then animate it unless the farm is paused. Nothing is drawn during a transition. */
void farm_draw_object(FarmHelper *helper) {
    SVECTOR input;
    FarmScreenPoint result;
    u_long *primitive;

    if ((game_state.globalFlags & GLOBAL_FLAG_TRANSITION) != 0) {
        return;
    }

    input.vx = helper->actor.position.vx + 150;
    input.vy = helper->actor.position.vy - 100;
    input.vz = helper->actor.position.vz - 150;
    if (estate_point_project(&input, &result) != 0) {
        primitive = &current_frame->ot[result.depth >> 2];
        cue_draw_frame(&helper->actor.cue, primitive, result.x, result.y - 20);
        if (helper->item != ITEM_NOTHING) {
            anim_draw_sprt_cursor(primitive, &helper->itemSprite, 0, 0, result.x, result.y - 16);
        }
        sprite_draw_traced(primitive, current_frame->primCursor.ft4, &helper->actor.sprite, helper->actor.sprite.animation,
                      helper->actor.sprite.frame, result.x, result.y, helper->actor.sprite.scale, 0, 0x40);
        farm_draw_unless_hidden(helper->actor.flags, primitive, *&result.x, *&result.y);
    }
    if ((game_state.globalFlags & GLOBAL_FLAG_PAUSED) == 0) {
        anim_tick(&helper->actor.sprite);
    }
}

/* Draw a farm request: its sprite is queued for the text layer, then its cue and cursor are drawn
 * (the cursor on the ground when it is jumping), and it animates unless the farm is paused. */
void farm_request_project(FarmActor *request, VramPlace *places, s32 base)
{
    FarmScreenPoint result;
    u_long *object;

    if ((game_state.globalFlags & GLOBAL_FLAG_TRANSITION) == 0) {
        farm_request_draw_point.vx = request->position.vx + 150;
        farm_request_draw_point.vy = request->position.vy - 100;
        farm_request_draw_point.vz = request->position.vz - 150;
        if (estate_point_project(&farm_request_draw_point, &result) != 0) {
            object = &current_frame->ot[result.depth >> 2];
            text_queue_sprite(object, &request->sprite, (s16)result.x, (s16)result.y, 0x1000, 0, 0,
                          frame_ot_base + base, places);
            cue_draw_frame(&request->cue, object, result.x,
                          result.y + farm_kind_draw_offset((s16)request->kind));
            if ((request->flags & 0x01000000) != 0) {
                farm_request_draw_point.vx = request->position.vx + 150;
                farm_request_draw_point.vy = -100;
                farm_request_draw_point.vz = request->position.vz - 150;
                object = &current_frame->ot[gte_project_vertex(&farm_request_screen_xy, &farm_request_draw_point) >> 2];
            } else {
                farm_request_screen_xy.x = result.x;
                farm_request_screen_xy.y = result.y;
            }
            farm_draw_unless_hidden(request->flags, object, farm_request_screen_xy.x, farm_request_screen_xy.y);
        }
        if ((game_state.globalFlags & GLOBAL_FLAG_PAUSED) == 0) {
            anim_tick(&request->sprite);
        }
    }
}

static void farm_draw_unless_hidden(u32 flags, u_long *ot, u32 x, u32 y) {
    if (!(flags & 0x1000)) {
        farm_draw_cursor(ot, x, y, flags >> 31);
    }
}

/* Add `b` into `a`, and zero `b` too unless `clear` is nonzero. */
void farm_vec3_accumulate(VECTOR *a, VECTOR *b, s32 clear) {
    a->vx += b->vx;
    a->vy += b->vy;
    a->vz += b->vz;
    if (!clear) {
        b->vx = 0;
        b->vy = 0;
        b->vz = 0;
    }
}

/* Set `result` to a step of `amount` in direction `mode`: 0 is -Z, 1 is -X, 2 is +Z, 3 is +X,
 * 4 (and any other value) gives zero; -1 is -Y and -2 is +Y. */
void farm_vector_adjust(s32 mode, VECTOR *result, s32 amount) {
    s32 index;

    mode = (mode << 16) + 0x20000;
    index = mode >> 16;
    result->vx = 0;
    result->vy = 0;
    result->vz = 0;
    switch (index) {
    case 2:
        result->vz = -amount;
        return;
    case 3:
        result->vx = -amount;
        return;
    case 4:
        result->vz = amount;
        return;
    case 5:
        result->vx = amount;
        return;
    case 6:
        result->vx = 0;
        result->vy = 0;
        result->vz = 0;
        return;
    case 1:
        result->vy = -amount;
        return;
    case 0:
        result->vy = amount;
        return;
    }
}

/* Start the praise cue (37 to 45) for an animal with affection 10 or more. */
void farm_cue_praise(FarmActor *p) {
    if (p->affection >= 10) {
        cue_set(&p->cue, 37, 45);
    }
}

/* Advance a cue table by one frame. The step moves on when `timer` reaches the entry's frames
 * (on the current map only with the sprite flags 0x300 set). Returns 1 when the next pose is
 * CUE_POSE_END. */
s32 farm_cue_step_run(FarmActor *self, struct FarmCueStep *table, s16 *step, s16 offset, s16 *timer) {
    s16 pose;
    s32 finished;
    FarmCueStep *entry;
    s32 next_timer;
    s16 frames;
    FarmLoadArea *area;
    area = &farm_area;
    entry = table + *step + offset;
    pose = entry->pose;
    self->action = pose;
    next_timer = (u16)*timer + 1;
    *timer = next_timer;
    finished = 0;
    if (self->place.location == area->mode.place.location) {
        if ((self->sprite.flags & 0x300) != 0 && (s16)next_timer >= entry->frames) {
            (*step)++;
            *timer = 0;
            pose = entry[1].pose;
            finished = pose == CUE_POSE_END;
        }
    } else {
        frames = entry->frames;
        if ((s16)next_timer >= frames) {
            (*step)++;
            *timer = 0;
            pose = entry[1].pose;
            if (pose == CUE_POSE_END) finished = 1;
        }
    }
    if (!finished) self->action = pose;
    return finished;
}

/* Aim the actor at `v` and set it walking to the player (ACTOR_FLAG_APPROACHING_PLAYER), clearing
 * ACTOR_FLAG_BIT14. */
void farm_set_destination(FarmActor *actor, VECTOR v) {
    actor->destination.vx = v.vx;
    actor->destination.vy = v.vy;
    actor->destination.vz = v.vz;
    actor->flags = (actor->flags | ACTOR_FLAG_APPROACHING_PLAYER) & ~ACTOR_FLAG_BIT14;
}

/* farm_move_table_lookup's result: how the mover's position compares with its destination on
 * X and on Z (LT: position below, GT: above, EQ: level). */
enum FarmMoveClass {
    FARM_MOVE_SAME = 0,
    FARM_MOVE_X_LT_Z_LT = 1,
    FARM_MOVE_X_LT_Z_GT = 2,
    FARM_MOVE_X_GT_Z_GT = 3,
    FARM_MOVE_X_GT_Z_LT = 4,
    FARM_MOVE_X_EQ_Z_LT = 5,
    FARM_MOVE_X_LT_Z_EQ = 6,
    FARM_MOVE_X_EQ_Z_GT = 7,
    FARM_MOVE_X_GT_Z_EQ = 8,
};

/* 3x3 result table, indexed by the two comparisons. */
static u16 farm_move_compare_table[10] = {
    FARM_MOVE_SAME, FARM_MOVE_X_EQ_Z_LT, FARM_MOVE_X_EQ_Z_GT,
    FARM_MOVE_X_LT_Z_EQ, FARM_MOVE_X_LT_Z_LT, FARM_MOVE_X_LT_Z_GT,
    FARM_MOVE_X_GT_Z_EQ, FARM_MOVE_X_GT_Z_LT, FARM_MOVE_X_GT_Z_GT, FARM_MOVE_SAME,
};

/* Compare both coordinate pairs three ways and look the pair up in the table. */
static u16 farm_move_table_lookup(FarmActor *state)
{
    s32 farm_move_compare(s32 a, s32 b)
    {
        if (a == b) {
            return 0;
        }
        if (a < b) {
            return 1;
        }
        return 2;
    }

    s32 first;
    s32 second;

    first = farm_move_compare(state->position.vx, state->destination.vx);
    second = farm_move_compare(state->position.vz, state->destination.vz);
    return farm_move_compare_table[first * 3 + second];
}

/* Set the mover's facing from its heading class: which of the two candidate
 * facings depends on whether the X or the Z distance to the target dominates. */
void farm_mover_set_facing(FarmActor *mover) {
    s32 x_major;

    x_major = abs(mover->position.vx - mover->destination.vx) > abs(mover->position.vy - mover->destination.vy);
    switch (farm_move_table_lookup(mover)) {
    case FARM_MOVE_X_LT_Z_LT:
    case FARM_MOVE_X_EQ_Z_LT:
        mover->facing = x_major ? 3 : 2;
        break;
    case FARM_MOVE_X_LT_Z_GT:
    case FARM_MOVE_X_LT_Z_EQ:
        mover->facing = x_major ? 3 : 0;
        break;
    case FARM_MOVE_X_GT_Z_GT:
    case FARM_MOVE_X_EQ_Z_GT:
        mover->facing = x_major ? 1 : 0;
        break;
    case FARM_MOVE_X_GT_Z_LT:
    case FARM_MOVE_X_GT_Z_EQ:
        mover->facing = x_major ? 1 : 2;
        break;
    }
}

void farm_place_spot(void *spot, s16 index, s32 mode) {
    if (mode == 1) {
        place_row_spot(spot, index);
    } else {
        place_grid_spot(spot, index);
    }
}

/* Place every live animal on its spot: the livestock on PLACE_5 in mode 1, else the chickens on
 * PLACE_4 (grid spots). Then the two animals the mode slots name go to the row spots, tagged
 * ROW_SPOT_MODE_SLOT_0 and _1. */
void farm_items_apply_kind(s32 mode) {
    s32 i;

    if (mode == 1) {
        FarmLivestock *records = game_state.animals.livestock;

        for (i = 0; i < MAX_LIVESTOCK; i++) {
            if (records[i].actor.kind != ITEM_NOTHING && records[i].actor.place.location == PLACE_5) {
                farm_place_spot(&records[i], i, 1);
            }
        }
    } else {
        FarmChicken *records = game_state.animals.chickens;

        for (i = 0; i < MAX_CHICKENS; i++) {
            if (records[i].actor.kind != ITEM_NOTHING && records[i].actor.place.location == PLACE_4) {
                farm_place_spot(&records[i], i, 0);
            }
        }
    }
    for (i = 0; i < 2; i++) {
        if (game_state.animals.slots[i].value != SLOT_NONE) {
            farm_place_spot(&game_state.animals.livestock[game_state.animals.slots[i].animal], i == 0 ? ROW_SPOT_MODE_SLOT_0 : ROW_SPOT_MODE_SLOT_1, 1);
        }
    }
}

/* Find a free (-1) slot in FarmState's mode table for `mode`: slots 0-1 for
 * mode 1, slots 2-3 otherwise. The second slot of each pair is only offered
 * while its gate (barnLevel / coopLevel) is open, as farm_mode_available checks. */
FarmModeSlot *farm_mode_slot_find(s32 mode) {
    FarmModeSlot *found;
    s32 i;

    found = 0;
    if (mode == 1) {
        for (i = 0; i < 2; i++) {
            if (i == 1 && game_state.progress.estate.barnLevel == 0) break;
            if (game_state.animals.slots[i].value == SLOT_NONE) {
                found = &game_state.animals.slots[i];
                break;
            }
        }
    } else {
        for (i = 2; i < 4; i++) {
            if (i == 3 && game_state.progress.estate.coopLevel == 0) break;
            if (game_state.animals.slots[i].value == SLOT_NONE) {
                found = &game_state.animals.slots[i];
                break;
            }
        }
    }
    return found;
}

/* Place an item at (x, y, z). */
static inline void set_position(FarmLivestock *item, s32 x, s32 y, s32 z) {
    item->actor.position.vx = x;
    item->actor.position.vy = y;
    item->actor.position.vz = z;
}

/* Respawns the mode slots' animals. Mode 1: the livestock in slots 0 and 1 get their placement
 * again while they still wait to be named. Otherwise the chicks in slots 2 and 3 are rebuilt as
 * ITEM_CHICK at the coop spots. */
void farm_mode_slot_respawn(s32 mode) {
    FarmLivestock *item;
    s32 slot;
    s32 index;
    s32 target;
    u32 flags;

    if (mode == 1) {
        slot = 0;
        do {
            index = game_state.animals.slots[slot].value;
            target = game_state.animals.slots[slot].animal;
            flags = game_state.animals.livestock[target].actor.flags;
            if (flags & ACTOR_FLAG_NAMING_PENDING) {
                if (flags & ACTOR_FLAG_BIT16) {
                    livestock_copy_placement_b(target, index);
                } else {
                    livestock_copy_placement(target, index);
                }
            }
            slot++;
            if (slot >= 2) {
                break;
            }
        } while (slot != 1 || game_state.progress.estate.barnLevel != 0);
    } else {
        slot = 2;
        do {
            index = game_state.animals.slots[slot].value;
            if (game_state.animals.chickens[index].actor.kind == ITEM_CHICK) {
                chicken_clear(&game_state.animals.chickens[index]);
                item = farm_action_create(ITEM_CHICK, index);
                if (game_state.animals.slots[slot].animal == 2) {
                    item->actor.flags |= ACTOR_FLAG_BIT30;
                }
                item->actor.facing = 3;
                set_position(item, -1600, -200, (slot == 2) ? -1100 : 1300);
            }
            slot++;
            if (slot >= 4) {
                break;
            }
        } while (slot != 3 || game_state.progress.estate.coopLevel != 0);
    }
}

/* Looks up the mode slot at index `mode`. Returns 2 when the gate is shut (barnLevel for mode 1,
 * coopLevel for mode 3), else 1 when the slot holds an item and 0 when it is free. */
s32 farm_mode_available(s32 mode) {
    if (mode == 1 && game_state.progress.estate.barnLevel == 0) return 2;
    if (mode == 3 && game_state.progress.estate.coopLevel == 0) return 2;
    return game_state.animals.slots[mode].value != SLOT_NONE;
}

/* First free livestock record (ITEM_NOTHING and not ACTOR_FLAG_CLAIMED) among the first 10, or
 * 20 once the barn is built (barnLevel); NULL when all are taken. */
FarmLivestock *livestock_find_free(void) {
    s32 slot;
    s32 limit;
    FarmLivestock *item;

    item = 0;
    limit = 10;
    if (game_state.progress.estate.barnLevel != 0) {
        limit = 20;
    }
    slot = 0;
    while (slot < limit) {
        item = &game_state.animals.livestock[slot];
        if ((item->actor.kind == ITEM_NOTHING) && ((item->actor.flags & ACTOR_FLAG_CLAIMED) == 0))
            break;
        slot++;
    }
    if (slot == limit) {
        return 0;
    }

  return item;
}

/* First free chicken record (ITEM_NOTHING and not ACTOR_FLAG_CLAIMED) among the first 5, or 10
 * when coopLevel is set; NULL when all are taken. */
FarmChicken *farm_chicken_find_free(void) {
    s32 slot;
    s32 limit;
    FarmChicken *item;

    item = 0;
    limit = 5;
    if (game_state.progress.estate.coopLevel != 0) {
        limit = 10;
    }
    slot = 0;
    while (slot < limit) {
        item = &game_state.animals.chickens[slot];
        if (item->actor.kind == ITEM_NOTHING && (item->actor.flags & ACTOR_FLAG_CLAIMED) == 0) break;
        slot++;
    }
    if (slot == limit) {
        return 0;
    }
    return item;
}

s32 livestock_index(FarmLivestock *animal) {
    s32 i = 0;
    FarmLivestock *p = game_state.animals.livestock;
    while (i < MAX_LIVESTOCK) {
        if (animal == p) return i;
        i++;
        p++;
    }
    return -1;
}

void farm_mode_slot_set(FarmModeSlot *slot, s16 value, s16 animal) {
    slot->value = value;
    slot->animal = animal;
}

void animal_slot_clear(s16 *pair) {
    pair[0] = SLOT_NONE;
    pair[1] = SLOT_NONE;
}

/* Set the farm message request and choose its message based on the current state. */
void farm_message_request(FarmLivestock *state) {
    s32 message;
    u16 mode;
    u16 sickDays;
    s16 age;
    text_set_source(0, state->actor.name);
    *text_format_number(LIVESTOCK_GESTATION_DAYS - state->pregnancy, farm_message_days, 2) = 0xFFFF;
    text_set_source(1, farm_message_days);
    sickDays = state->daysSick;
    message = LIVESTOCK_MSG_SICK_LONG;
    if (sickDays < 4U) {
        message = LIVESTOCK_MSG_SICK_RECENT;
        if (sickDays == 0) {
            mode = state->pregnancy;
            if (mode == 1) {
                message = LIVESTOCK_MSG_PREGNANT_START;
            } else if (mode != 1 && mode < LIVESTOCK_GESTATION_DAYS) {
                message = LIVESTOCK_MSG_PREGNANT;
                if (mode == 0) {
                    age = state->actor.affection;
                    message = LIVESTOCK_MSG_HEALTHY;
                    if (age >= LIVESTOCK_FOND_AFFECTION) {
                        message = LIVESTOCK_MSG_FOND;
                        if (age >= LIVESTOCK_DEVOTED_AFFECTION) message = LIVESTOCK_MSG_DEVOTED;
                    }
                }
            } else {
                message = LIVESTOCK_MSG_PREGNANT_OVERDUE;
            }
        }
    }
    menu_request_open(0, message);
}

/* Mark feed bin `slot` as filled: the barn's when `sel` is 1, else the coop's. */
void farm_feed_bin_set(s32 slot, s32 sel) {
    if (sel == 1) {
        game_state.progress.estate.barnFeedBins[slot] = 1;
    } else {
        game_state.progress.estate.coopFeedBins[slot] = 1;
    }
}

/* Is feed bin `index` of the barn (`barn` 1) or of the coop full? */
s32 farm_feed_bin_full(s32 index, s32 barn) {
    if (barn == 1) {
        if (game_state.progress.estate.barnFeedBins[index] != 0) return 1;
    } else {
        if (game_state.progress.estate.coopFeedBins[index] != 0) return 1;
    }
    return 0;
}

/* Claim mode slot `mode + 2` for a newly taken item: marks the first free item taken and
 * records its index and `animal` in the slot. Returns FARM_CLAIM_OK on success, else
 * FARM_CLAIM_SLOT_TAKEN or FARM_CLAIM_NO_ITEM. */
s32 farm_mode_slot_claim(s32 mode, s32 animal)
{
    s32 slot;
    FarmChicken *item;
    s32 index;
    FarmState *state;

    slot = mode + 2;
    if (farm_mode_available(slot) != 0) {
        return FARM_CLAIM_SLOT_TAKEN;
    }
    item = farm_chicken_find_free();
    if (item != 0) {
        item->actor.flags |= ACTOR_FLAG_CLAIMED;
        index = chicken_index(item);
        state = &game_state;
        state->animals.slots[slot].value = index;
        state->animals.slots[slot].animal = animal;
        return FARM_CLAIM_OK;
    }
    return FARM_CLAIM_NO_ITEM;
}

#define BLOCKED_MASK(a, mask) (farm_callback_capped(&(a)->actor.position, (a)->actor.place, 0, 0, 0, 0, 0, mask) != 0)

/* Puts back every animal that stands somewhere blocked, and brings the waiting ones out when
 * the greenhouse is not up yet. */
void farm_put_back_blocked_animals(void) {
    s32 mask = OCCUPANT_CHOICE | OCCUPANT_TILE;
    FarmPet *pet;
    FarmChicken *record;
    s32 i;
    FarmState *farm;

    pet = &game_state.animals.dog;
    if (pet->actor.place.location == PLACE_GREENHOUSE && game_state.progress.estate.greenhouseLevel == 0) {
        pet->actor.position.vx = -1800;
        pet->actor.position.vy = 0;
        pet->actor.position.vz = 3100;
        pet->actor.place.location = PLACE_FARM;
    } else if ((pet->actor.place.location == PLACE_FARM || pet->actor.place.location == PLACE_GREENHOUSE) && BLOCKED_MASK(pet, mask)) {
        if (pet->actor.place.location == PLACE_FARM) {
            pet->actor.position.vx = -1800;
            pet->actor.position.vy = 0;
            pet->actor.position.vz = 3100;
        } else {
            pet->actor.position.vx = -1800;
            pet->actor.position.vy = 0;
            pet->actor.position.vz = 3100;
            pet->actor.place.location = PLACE_FARM;
        }
    }
    pet = &game_state.animals.horse;
    if (pet->actor.place.location == PLACE_FARM && BLOCKED_MASK(pet, mask)) {
        pet->actor.position.vx = 5000;
        pet->actor.position.vy = 0;
        pet->actor.position.vz = 2700;
    }
    for (i = 0; i < MAX_LIVESTOCK; i++) {
        pet = (FarmPet *)&game_state.animals.livestock[i];
        if (pet->actor.place.location == PLACE_FARM && BLOCKED_MASK(pet, mask)) {
            pet->actor.position.vx = i * 400 - 1000;
            pet->actor.position.vy = 0;
            pet->actor.position.vz = 2200;
        }
    }
    for (i = 0; i < MAX_CHICKENS; i++) {
        record = &game_state.animals.chickens[i];
        if (record->actor.place.location == PLACE_GREENHOUSE) {
            farm = &game_state;
            if (farm->progress.estate.greenhouseLevel == 0) {
                record->actor.position.vx = -5000;
                record->actor.position.vy = 0;
                record->actor.position.vz = i * 400 - 7600;
                record->actor.place.location = PLACE_FARM;
                continue;
            }
        }
        if (record->actor.place.location == PLACE_FARM || record->actor.place.location == PLACE_GREENHOUSE) {
            if (farm_callback_capped(&record->actor.position, record->actor.place, 0, 0, 0, 0, 0, mask) != 0) {
                if (record->actor.place.location == PLACE_FARM) {
                    record->actor.position.vx = -5000;
                    record->actor.position.vy = 0;
                    record->actor.position.vz = i * 400 - 7600;
                } else {
                    record->actor.position.vx = -5000;
                    record->actor.position.vy = 0;
                    record->actor.position.vz = i * 400 - 7600;
                    record->actor.place.location = PLACE_FARM;
                }
            }
        }
    }
}

/* Queue CD track `value`; farm_animals_tick plays it once the CD and the menu are idle. */
void farm_request_track(u16 value) {
    game_state.animals.pendingTrack = value;
}

static s32 farm_walker_timer = 0;

/* Moves a walker one frame at `speed`: it now and then looks at the cells ahead, left and
 * right, picks a new heading, and then steps along its heading unless the cell ahead is
 * blocked. */
void farm_walker_move(FarmActor *walker, s32 speed, s32 flags)
{
    s32 limit;
    s32 blocked;
    s32 stuck;
    s16 heading;
    s32 side;
    u16 width;
    u16 depth;
    VECTOR box;
    void *out_a;
    s32 out_b;

    if (vsync_elapsed(150, &farm_walker_timer, 1) == 1) {
        farm_walker_phase = farm_walker_phase != 1;
    }
    limit = ((farm_walker_phase == 1) ? 1080 : 720) / speed;
    farm_update_zone_flag(walker);
    if (walker->flags & ACTOR_FLAG_BIT21) {
        flags &= ~(OCCUPANT_CHOICE | OCCUPANT_TILE);
    }
    if (walker->waitTimer != 0) {
        walker->waitTimer--;
        if (walker->waitTimer < 0) {
            walker->waitTimer = 0;
        }
        flags &= ~OCCUPANT_PLAYER;
    }
    if (walker->kind == ITEM_STRAY_DOG) {
        farm_walker_not_stray = 0;
    } else {
        farm_walker_not_stray = 1;
    }
    if (walker->place.location != farm_area.mode.place.location && (walker->flags & ACTOR_FLAG_BIT21)) {
        farm_vector_adjust(4, &walker->speed, 0);
        return;
    }
    if (walker->flags & ACTOR_FLAG_ADULT_SPRITE) {
        flags &= ~OCCUPANT_TILE;
    }
    width = game_state.itemInfo[walker->kind].halfWidth;
    depth = game_state.itemInfo[walker->kind].halfDepth;
    if (++walker->place.value >= limit && !(walker->flags & ACTOR_FLAG_JUMPING)) {
        blocked = 0;
        walker->place.value = 0;

        farm_vector_adjust((s16)walker->facing, &box, speed);
        box.vx += walker->position.vx;
        box.vz += walker->position.vz;
        if (walker->flags & ACTOR_FLAG_BIT21) {
            blocked = farm_walker_cell_free(&box) == 0;
        }
        if ((walker->flags & ACTOR_FLAG_ADULT_SPRITE) && farm_check_movement(walker, box, walker->facing, 0) != 1) {
            blocked |= 1;
        }
        if (farm_callback_capped(&box, walker->place, width, depth, &out_a, &out_b, walker, flags) == 0) {
            blocked |= 1;
        }

        heading = walker->facing;
        heading--;
        side = (heading < 0) ? 3 : heading;
        farm_vector_adjust(side, &box, 400);
        box.vx += walker->position.vx;
        box.vz += walker->position.vz;
        if ((walker->flags & ACTOR_FLAG_BIT21) && farm_walker_cell_free(&box) == 0) {
            blocked |= 4;
        }
        if ((walker->flags & ACTOR_FLAG_ADULT_SPRITE) && farm_check_movement(walker, box, side, 0) != 1) {
            blocked |= 4;
        }
        if (farm_callback_capped(&box, walker->place, width, depth, &out_a, &out_b, walker, flags) == 0) {
            blocked |= 4;
        }

        heading = walker->facing;
        heading++;
        side = (heading < 4) ? heading : 0;
        farm_vector_adjust(side, &box, 400);
        box.vx += walker->position.vx;
        box.vz += walker->position.vz;
        if ((walker->flags & ACTOR_FLAG_BIT21) && farm_walker_cell_free(&box) == 0) {
            blocked |= 2;
        }
        if ((walker->flags & ACTOR_FLAG_ADULT_SPRITE) && farm_check_movement(walker, box, side, 0) != 1) {
            blocked |= 2;
        }
        if (farm_callback_capped(&box, walker->place, width, depth, &out_a, &out_b, walker, flags) == 0) {
            blocked |= 2;
        }

        heading = farm_walker_pick_heading(walker, blocked, flags);
        farm_vector_adjust(heading, &walker->target, 400);
        if (heading != 4) {
            walker->facing = heading;
        }
        walker->target.vx += walker->position.vx;
        walker->target.vz += walker->position.vz;
        walker->flags |= ACTOR_FLAG_BIT14;
        farm_vector_adjust(4, &walker->speed, 0);
    }
    if ((walker->flags & ACTOR_FLAG_APPROACHING_PLAYER) && farm_heading_classify(walker) == 0) {
        farm_vector_adjust(4, &walker->speed, 0);
        farm_mover_set_facing(walker);
        return;
    }
    farm_vector_adjust((s16)walker->facing, &walker->speed, speed);
    box.vx = walker->position.vx + walker->speed.vx;
    box.vz = walker->position.vz + walker->speed.vz;
    box.vy = walker->position.vy;
    stuck = 0;
    if (!(walker->flags & ACTOR_FLAG_JUMPING)) {
        if (walker->flags & ACTOR_FLAG_BIT21) {
            stuck = farm_walker_cell_free(&box) != 0;
        }
        if (farm_callback_capped(&box, walker->place, width, depth, &out_a, &out_b, walker, flags) != 0) {
            stuck = 1;
        }
        if ((walker->flags & ACTOR_FLAG_ADULT_SPRITE) && farm_check_movement(walker, box, walker->facing, 1) == 1) {
            stuck = 1;
        }
        if (stuck == 1) {
            farm_vector_adjust(4, &walker->speed, 0);
            walker->place.value += 2;
        }
        walker->position.vy = box.vy;
        farm_walker_try_exits(walker);
    } else {
        farm_timer_step(walker, speed);
    }
    farm_proximity_threshold = 600;
}

void farm_set_proximity(s16 threshold) {
    farm_proximity_threshold = threshold;
}

/* Picks the walker's next heading from the blocked mask: 1 is ahead, 2 the side at facing + 1,
 * 4 the side at facing - 1. Ahead only keeps the facing; one side turns to the other; both sides
 * or all three take a weighted turn; none reverses, unless the cell behind is blocked, when it
 * stops (returns 4). */
static u16 farm_walker_pick_heading(FarmActor *walker, s32 blocked, s32 flags)
{
    u16 command;
    u16 result;
    u16 bias;
    s16 heading;
    s16 back;
    u16 width;
    u16 depth;
    VECTOR box;
    void *out_a;
    s32 out_b;
    s32 hit;

    command = blocked;
    result = 4;
    bias = 0;
    if (walker->flags & ACTOR_FLAG_APPROACHING_PLAYER) {
        bias = farm_heading_classify(walker);
    }
    heading = walker->facing;
    if (command == 7) {
        result = farm_walker_pick_turn(heading, 0, bias);
    } else if (command == 5) {
        result = farm_walker_pick_turn(heading, 1, bias);
    } else if (command == 3) {
        result = farm_walker_pick_turn(heading, 2, bias);
    } else if (command == 5) {
        result = farm_walker_pick_turn(heading, 1, bias);
    } else if (command == 1) {
        result = walker->facing;
    } else if (command == 6) {
        result = farm_walker_pick_turn(heading, 3, bias);
    } else if (command == 4) {
        heading--;
        result = (heading < 0) ? 3 : heading;
    } else if (command == 2) {
        heading++;
        result = (heading < 4) ? heading : 0;
    } else {
        back = (heading < 2) ? heading + 2 : heading - 2;
        farm_vector_adjust(back, &box, 400);
        box.vx += walker->position.vx;
        box.vz += walker->position.vz;
        width = game_state.itemInfo[walker->kind].halfWidth;
        depth = game_state.itemInfo[walker->kind].halfDepth;
        if (walker->flags & ACTOR_FLAG_ADULT_SPRITE) {
            flags &= ~OCCUPANT_TILE;
        }
        if (walker->flags & ACTOR_FLAG_BIT21) {
            if (farm_walker_cell_free(&box) == 0) {
                hit = farm_callback_capped(&box, walker->place, width, depth, &out_a, &out_b, walker, flags);
                result = back;
                if (hit == 0) {
                    return result;
                }
            }
            result = 4;
        } else {
            hit = farm_callback_capped(&box, walker->place, width, depth, &out_a, &out_b, walker, flags);
            result = back;
            if (hit != 0) {
                result = 4;
            }
            if ((walker->flags & ACTOR_FLAG_ADULT_SPRITE)
                && farm_check_movement(walker, box, back, 0) != 1) {
                result = back;
            }
        }
    }
    return result;
}

/* Roll 0..100 and pick the first of the four outcomes whose running weight
 * reaches it; 4 when none does. */
static u16 farm_walker_pick_turn(u16 heading, u16 turn, u16 bias)
{
    s32 roll;
    s32 total;
    s32 i;
    u16 result;
    s32 weight;

    roll = rand_range(101);
    result = 4;
    i = 0;
    total = 0;
    do {
        weight = farm_turn_weights[heading][turn][bias][i];
        if (weight != 0) {
            total += weight;
            if (total >= roll) {
                result = i;
                break;
            }
        }
        i++;
    } while (i < 4);
    return result;
}

/* Heading for each pair of comparisons from farm_heading_classify, indexed first * 3 + second. */
static u16 farm_heading_table[10] = {
    0x0000, 0x0005, 0x0007, 0x0006, 0x0001, 0x0002, 0x0008, 0x0004, 0x0003, 0x0000,
};

/* Classify the object's heading from its two coordinate pairs; when it is close to its target on
 * both axes, clear ACTOR_FLAG_APPROACHING_PLAYER (0x2000) and report 0 instead. */
static u16 farm_heading_classify(FarmActor *state)
{
    s32 farm_heading_compare(s32 a, s32 b)
    {
        if (a == b) {
            return 0;
        }
        if (a < b) {
            return 1;
        }
        return 2;
    }

    s32 x1;
    s32 y1;
    s32 x2;
    s32 y2;
    s32 dx;
    s32 dy;
    s32 first;
    s32 second;
    u16 result;

    x1 = state->position.vx;
    y1 = state->destination.vx;
    x2 = state->position.vz;
    y2 = state->destination.vz;
    dx = abs(y1 - x1);
    dy = abs(y2 - x2);
    first = farm_heading_compare(x1, y1);
    second = 0;
    if (dy * 3 >= dx) {
        second = farm_heading_compare(x2, y2);
    }
    if (dx < farm_proximity_threshold && dy < farm_proximity_threshold && (dx < 50 || dy < 50)) {
        result = 0;
        state->flags &= ~ACTOR_FLAG_APPROACHING_PLAYER;
    } else {
        result = farm_heading_table[first * 3 + second];
    }
    return result;
}

/* Set ACTOR_FLAG_BIT21 while farm_callback_capped reports a hit for choice occupants at the
 * object's position, else clear it. */
static void farm_update_zone_flag(FarmActor *object) {
    VECTOR input;
    void *output1;
    s32 output2;
    s32 result;

    input.vx = object->position.vx;
    input.vy = object->position.vy;
    input.vz = object->position.vz;
    result = farm_callback_capped(&input, object->place, 0, 0, &output1, &output2, object, OCCUPANT_CHOICE);
    if (result != 0) {
        object->flags = object->flags | ACTOR_FLAG_BIT21;
    } else {
        object->flags = object->flags & ~ACTOR_FLAG_BIT21;
    }
}

/* Per-frame update of the livestock and chicken records. Each one ticks its timers and cue
 * unless the farm is paused, then refreshes its sprite on the current map, or has its cue reset
 * when it is elsewhere. */
static void farm_objects_update(void) {
    FarmLivestock *item;
    FarmChicken *record;
    s32 i;
    u32 mask;

    for (i = 0; i < MAX_LIVESTOCK; i++) {
        item = &game_state.animals.livestock[i];
        if (item->actor.kind == ITEM_NOTHING) {
            continue;
        }
        if (item->actor.flags & ACTOR_FLAG_IN_EVENT) {
            continue;
        }
        if (item->actor.flags & ACTOR_FLAG_BIT16) {
            if (!(game_state.globalFlags & GLOBAL_FLAG_PAUSED)) {
                if (!(game_state.globalFlags & GLOBAL_FLAG_CLOCK_TICKED)) {
                    if (farm_area.mode.place.location == item->actor.place.location) {
                        livestock_dispatch_tick_b(item);
                        livestock_pair_at_38_b(item);
                    }
                } else {
                    livestock_dispatch_tick_b(item);
                    livestock_pair_at_38_b(item);
                }
                cue_countdown(&item->actor.cue);
            }
            if (farm_area.mode.place.location != item->actor.place.location) {
                cue_reset(&item->actor.cue);
            } else {
                livestock_sprite_refresh_b(&item->actor);
            }
        } else {
            if (!(game_state.globalFlags & GLOBAL_FLAG_PAUSED)) {
                if (!(game_state.globalFlags & GLOBAL_FLAG_CLOCK_TICKED)) {
                    if (farm_area.mode.place.location == item->actor.place.location) {
                        livestock_dispatch_tick(item);
                        livestock_pair_at_38(item);
                    }
                } else {
                    livestock_dispatch_tick(item);
                    livestock_pair_at_38(item);
                }
                cue_countdown(&item->actor.cue);
            }
            if (item->actor.flags & ACTOR_FLAG_HELD) {
                continue;
            }
            if (farm_area.mode.place.location != item->actor.place.location) {
                cue_reset(&item->actor.cue);
            } else {
                livestock_sprite_refresh(&item->actor);
            }
        }
    }
    for (i = 0; i < MAX_CHICKENS; i++) {
        record = &game_state.animals.chickens[i];
        if (record->actor.kind == ITEM_NOTHING) {
            continue;
        }
        chicken_timer_tick(record);
        mask = 0x20000;
        if (farm_area.mode.place.location != record->actor.place.location) {
            cue_reset(&record->actor.cue);
            continue;
        }
        if (record->actor.flags & mask) {
            continue;
        }
        if (!(game_state.globalFlags & GLOBAL_FLAG_PAUSED)) {
            chicken_pair_at_38(record);
        }
        if (record->actor.flags & ACTOR_FLAG_CLAIMED) {
            continue;
        }
        livestock_frame_refresh(&record->actor);
        cue_countdown(&record->actor.cue);
    }
}

/* Update the three extra chickens (FarmState.animals.extraChickens). */
static void farm_chickens_extra_update(void) {
    FarmChicken *object;
    s32 i;

    for (i = 0; i < 3; i++) {
        object = &game_state.animals.extraChickens[i];
        if (object->actor.kind != ITEM_NOTHING) {
            chicken_timer_tick(object);
            if (farm_area.mode.place.location != object->actor.place.location) {
                cue_reset(&object->actor.cue);
            } else {
                if ((game_state.globalFlags & GLOBAL_FLAG_PAUSED) == 0) {
                    chicken_pair_at_38(object);
                }
                livestock_frame_refresh(&object->actor);
                cue_countdown(&object->actor.cue);
            }
        }
    }
}

/* Update each action slot that is not held: its timers and cue unless paused, then its sprite on
 * the current map (its cue is reset when it is elsewhere). */
static void farm_actions_tick(void) {
    FarmAction *action;
    s32 i;
    for (i = 0; i < 6; i++) {
        action = &game_state.animals.actions[i];
        if (action->actor.kind != ITEM_NOTHING && (action->actor.flags & ACTOR_FLAG_HELD) == 0) {
            farm_action_update(action);
            if ((game_state.globalFlags & GLOBAL_FLAG_PAUSED) == 0) {
                farm_actor_pair_at_38(action);
                cue_countdown(&action->actor.cue);
            }
            if (farm_area.mode.place.location != action->actor.place.location) {
                cue_reset(&action->actor.cue);
            } else {
                farm_actor_sprite_refresh(&action->actor);
            }
        }
    }
}

/* GLOBAL_FLAG_PAUSED: the farm is paused (an event holds every timer). */
static inline s32 farm_paused(void) {
    return game_state.globalFlags & GLOBAL_FLAG_PAUSED;
}

/* Steps the two cue runners, the dog and the horse. A runner that is not on the current map has
 * its cue reset, and a held one is left alone. The others tick their mode timer and pair step
 * (not while paused; the horse not while the player rides it), then refresh their sprite and
 * count down their cue. */
static void farm_cue_runners_step(void) {
    FarmPet *runner;
    s32 i;

    for (i = 0; i < 2; i++) {
        if (i == 0) {
            runner = &game_state.animals.dog;
            if (runner->actor.kind == ITEM_NOTHING) {
                continue;
            }
            if ((runner->actor.flags & ACTOR_FLAG_IN_EVENT) &&
                clock_date_index(game_state.progress.clock.date.season, game_state.progress.clock.date.day) == 8) {
                continue;
            }
            dog_update_phase(runner);
        } else {
            runner = &game_state.animals.horse;
            if (runner->actor.kind == ITEM_NOTHING) {
                continue;
            }
            if (runner->actor.flags & ACTOR_FLAG_IN_EVENT) {
                continue;
            }
            horse_dispatch_tick(&runner->actor);
        }
        if (farm_area.mode.place.location != runner->actor.place.location) {
            cue_reset(&runner->actor.cue);
            continue;
        }
        if (runner->actor.flags & ACTOR_FLAG_HELD) {
            continue;
        }
        if (i == 0) {
            if (!farm_paused()) {
                farm_mode_timer_tick(&runner->actor, &farm_mode_check_timers[1], 0x11);
                cue_pair_at_38(runner);
            }
            pet_refresh_sprite(runner);
            cue_countdown(&runner->actor.cue);
        } else {
            if (game_state.player.stateFlags & PLAYER_FLAG_RIDING) {
                continue;
            }
            if (!farm_paused()) {
                farm_mode_timer_tick(&runner->actor, &farm_mode_check_timers[2], 0x11);
                horse_pair_at_38(runner);
            }
            horse_sprite_refresh(&runner->actor);
            cue_countdown(&runner->actor.cue);
        }
    }
}

/* Probe `pos` for the object's place with farm_probe_tile. When the probe returns 2 and `state` is
 * 1, the object starts jumping. Returns the probe result. */
static s32 farm_check_movement(FarmActor *object, VECTOR pos, s16 mode, s32 state) {
    s32 result;

    result = farm_probe_tile(pos, object->place, mode, 0);
    if (result == 2 && state == 1) {
        object->flags |= ACTOR_FLAG_JUMPING;
    }
    return result;
}

/* Move a jumping object's height (position.vy): it rises above -200, then falls to the ground at
 * 0 and loses its jump flags. Returns 0 on the landing frame, else 1. */
static s32 farm_timer_step(FarmActor *obj, s32 speed) {
    s32 arrived = 1;
    s32 steps;
    s32 step;
    s32 flags;
    s32 new_flags;
    s32 clear_hi;
    s32 clear_active;

    steps = 400 / speed / 2;
    if (400 % speed) steps += 2;
    step = 200 / steps;
    flags = obj->flags;
    if (!(flags & ACTOR_FLAG_BIT22)) {
        if (obj->position.vy < -200) {
            obj->speed.vy = step;
            new_flags = flags | ACTOR_FLAG_BIT22;
            obj->flags = new_flags;
        } else {
            obj->speed.vy = -step;
        }
    } else {
        obj->speed.vy = step;
        if (obj->position.vy + step >= 0) {
            clear_hi = ~0x01000000;
            clear_active = ~0x400000;
            arrived = 0;
            new_flags = flags & clear_hi & clear_active;
            obj->speed.vy = 0;
            obj->position.vy = 0;
            obj->flags = new_flags;
        }
    }
    return arrived;
}

/* While `*timer` is under 300 the timer counts up when the object's taskNo is `mode`, and the
 * object walks to the player while it is approaching. Once the timer reaches 300 it stays there. */
static void farm_mode_timer_tick(FarmActor *object, u16 *timer, u16 mode) {
    s32 value;

    if (*timer < 300) {
        if (object->flags & ACTOR_FLAG_APPROACHING_PLAYER) {
            FarmState *farm = &game_state;
            farm_set_destination(object, farm->player.position);
        }
        value = object->taskNo;
        if (value == mode) {
            value = *timer + 1;
            *timer = value;
        }
    } else {
        value = 300;
        *timer = value;
    }
}

/* Runs the pending farm mode check once the player can act. */
static void farm_mode_check_poll(void) {
    if (farm_mode_check_handle != -1) {
        if (player_can_act() != 0) {
            farm_mode_check(farm_mode_check_handle);
            farm_mode_check_handle = -1;
        }
    }
}

/* The checks farm_mode_check runs, in order. */
enum FarmModeCheck {
    FARM_MODE_CHECK_LIVESTOCK = 0,
    FARM_MODE_CHECK_DOG = 1,
    FARM_MODE_CHECK_HORSE = 2,
};

/* Run the farm-mode check for `mode`: the livestock check walks the 20 item records on
 * the current map, the dog and horse checks test one fixed object each. A check whose action
 * reports done zeroes its countdown in farm_mode_check_timers. */
static void farm_mode_check(s32 mode)
{
    FarmLivestock *item;
    s32 i;

    switch (mode) {
    case FARM_MODE_CHECK_LIVESTOCK:
        if (farm_area.mode.place.location == game_state.animals.dog.actor.place.location) {
            dog_set_phase(&game_state.animals.dog, DOG_PHASE_SEEK_ITEM);
        }
        for (i = 0; i < MAX_LIVESTOCK; i++) {
            item = &game_state.animals.livestock[i];
            if (item->actor.kind == ITEM_NOTHING) {
                continue;
            }
            if (item->actor.flags & ACTOR_FLAG_BIT16) {
                if (farm_area.mode.place.location == item->actor.place.location) {
                    if (livestock_action_dispatch(item, 0xB) == 0) {
                        farm_mode_check_timers[0] = 0;
                    }
                }
            } else if (farm_area.mode.place.location == item->actor.place.location) {
                if (livestock_action_dispatch_b(item, 0xB) == 0) {
                    farm_mode_check_timers[0] = 0;
                }
            }
        }
        break;
    case FARM_MODE_CHECK_DOG:
        if (farm_area.mode.place.location == game_state.animals.dog.actor.place.location) {
            if (dog_set_phase(&game_state.animals.dog, DOG_PHASE_TRY_ACTION) == 0) {
                farm_mode_check_timers[1] = 0;
            }
        }
        break;
    case FARM_MODE_CHECK_HORSE:
        if (farm_area.mode.place.location == game_state.animals.horse.actor.place.location) {
            if (horse_set_state(&game_state.animals.horse.actor, 0x13) == 0) {
                farm_mode_check_timers[2] = 0;
            }
        }
        break;
    }
}

/* Screen y offset for a kind's sprite (-10 to -40), from the kind's table; 0 for kinds not listed. */
static s32 farm_kind_draw_offset(s32 arg) {
    int index = ((arg << 16) - 0xA10000) >> 16;
    switch (index) {
    case 0:
    case 1:
    case 2:
    case 101:
    case 102:
    case 103:
    case 104:
    case 106:
    case 107:
    case 108:
    case 109:
    case 110:
    case 111:
    case 113:
    case 114:
    case 115:
    case 116:
    case 117:
    case 118:
    case 119:
    case 120:
    case 121:
    case 178:
    case 179:
    case 180:
    case 181:
    case 182:
    case 184:
    case 186:
    case 191:
    case 195:
        return -20;
    case 192:
    case 196:
        return -30;
    case 189:
    case 190:
    case 193:
        return -40;
    case 105:
        return -10;
    }
    return 0;
}

/* Origins of the six row spots, by row; rows 4 and 5 are the two ROW_SPOT_MODE_SLOT spots. */
static FarmSpot farm_row_spots[6] = {
    {1980, 0, -1268, 0},
    {1250, 0, -1268, 0},
    {-1250, 0, -1268, 0},
    {-1980, 0, -1268, 0},
    {2400, 0, -2450, 0},
    {-2050, 0, -2450, 0},
};

/* Place an animal on the spot for `slot`: slots 0.. are rows of five, 600 apart in z, and the
 * two ROW_SPOT_MODE_SLOT values the two extra spots (rows 4 and 5). Odd rows face 1, even rows
 * face 3; the object's area is 5. */
void place_row_spot(FarmActor *object, s16 slot) {
    s16 row;
    s16 col;

    if (slot == ROW_SPOT_MODE_SLOT_0) {
        row = 4;
        col = 0;
    } else if (slot == ROW_SPOT_MODE_SLOT_1) {
        row = 5;
        col = 0;
    } else {
        row = slot / 5;
        col = slot % 5;
    }
    object->position.vx = farm_row_spots[row].vx;
    object->position.vy = farm_row_spots[row].vy;
    object->position.vz = farm_row_spots[row].vz + col * 600;
    if (row & 1) {
        object->facing = 1;
    } else {
        object->facing = 3;
    }
    object->place.location = 5;
}

static FarmSpot farm_grid_spots[2] = {{-760, 0, -600, 0}, {-750, 0, 1600, 0}};

/* Place an object on grid spot `slot`: rows of five spots 400 apart in x, starting at
 * the row's position in the table. It faces a random direction, and its area is 4. */
void place_grid_spot(FarmActor *object, s16 slot) {
    s16 row;
    s16 col;

    row = slot / 5;
    col = slot % 5;
    object->position.vx = farm_grid_spots[row].vx + col * 400;
    object->position.vy = farm_grid_spots[row].vy;
    object->position.vz = farm_grid_spots[row].vz;
    object->facing = rand_range(4);
    object->place.location = 4;
}

/* Tries the walker's four exits (dir 0 to 3, each with its flag bit) in order. On the farm the
 * door is checked first: a walker there turns to face it and goes through only when
 * door_entry_for_event says so (== 3), then waits 90 frames. Elsewhere it goes through at once. */
static void farm_walker_try_exits(FarmActor *walker)
{
    s32 dir;
    u32 mask;
    s32 door;
    u32 row;
    s32 side;
    u16 cue;
    s32 target[3];
    u16 target_area;

    for (dir = 0; dir < 4; dir++) {
        door = 1;
        switch (dir) {
        case 0:
            mask = 0x4000000;
            row = 0;
            side = 2;
            cue = 2;
            break;
        case 1:
            mask = 0x8000000;
            row = 1;
            door = 3;
            side = 2;
            cue = 2;
            break;
        case 2:
            if (game_state.progress.estate.greenhouseLevel == 0) {
                continue;
            }
            mask = 0x10000000;
            row = 2;
            door = 4;
            side = 1;
            cue = 1;
            break;
        case 3:
            mask = 0x20000000;
            row = 3;
            door = 2;
            side = 1;
            cue = 1;
            break;
        default:
            continue;
        }
        if ((walker->flags & mask) == 0) {
            continue;
        }
        if (farm_zone_row_lookup(walker->position, walker->place.location, target, &target_area, row) == 0) {
            continue;
        }
        if (farm_area.mode.place.location == PLACE_FARM) {
            event_pending_pair_lookup(door, side, 0, 1);
            walker->interruptTime = 0;
            if (walker->place.location == PLACE_FARM) {
                walker->facing = cue;
            } else {
                farm_vector_adjust(4, &walker->speed, 0);
            }
            if (door_entry_for_event(door) == 3) {
                walker->position.vx = target[0];
                walker->position.vz = target[2];
                walker->place.location = target_area;
                farm_vector_adjust(4, &walker->speed, 0);
                walker->waitTimer = 90;
                walker->flags &= ~ACTOR_FLAG_APPROACHING_PLAYER;
            }
        } else {
            walker->position.vx = target[0];
            walker->position.vz = target[2];
            walker->place.location = target_area;
            farm_vector_adjust(4, &walker->speed, 0);
            walker->flags &= ~ACTOR_FLAG_APPROACHING_PLAYER;
        }
        return;
    }
}

/* Set the flags of a freshly created action by kind: ACTOR_FLAG_BIT31 for the cows (ITEM_COW,
 * ITEM_VISITING_COW); ACTOR_FLAG_BIT12 for the insects, ITEM_BUTTERFLY_2, ITEM_VISITOR and
 * ITEM_153, ITEM_155 to ITEM_157. Other kinds get none. */
static void farm_action_set_kind_flag(FarmAction *action)
{
    u32 *flags = &action->actor.flags;

    switch (action->actor.kind) {
    case ITEM_LARGE_FISH ... ITEM_SMALL_FISH:
    case ITEM_ADULT_DOG ... ITEM_SNAKE:
    case ITEM_BUTTERFLY:
    case ITEM_159:
    case ITEM_STRAY_DOG:
    case ITEM_PONY ... ITEM_YOUNG_COW:
    case ITEM_LAMB ... ITEM_SHEEP:
        break;
    case ITEM_COW:
    case ITEM_VISITING_COW:
        *flags |= ACTOR_FLAG_BIT31;
        break;
    case ITEM_BUTTERFLY_2: *flags |= ACTOR_FLAG_BIT12; break;
    case ITEM_BEETLE: *flags |= ACTOR_FLAG_BIT12; break;
    case ITEM_STAG_BEETLE: *flags |= ACTOR_FLAG_BIT12; break;
    case ITEM_CICADA: *flags |= ACTOR_FLAG_BIT12; break;
    case ITEM_DRAGONFLY: *flags |= ACTOR_FLAG_BIT12; break;
    case ITEM_CRICKET: *flags |= ACTOR_FLAG_BIT12; break;
    case ITEM_LADY_BUG: *flags |= ACTOR_FLAG_BIT12; break;
    case ITEM_GRASSHOPPER: *flags |= ACTOR_FLAG_BIT12; break;
    case ITEM_153: *flags |= ACTOR_FLAG_BIT12; break;
    case ITEM_VISITOR: *flags |= ACTOR_FLAG_BIT12; break;
    case ITEM_155: *flags |= ACTOR_FLAG_BIT12; break;
    case ITEM_156: *flags |= ACTOR_FLAG_BIT12; break;
    case ITEM_157: *flags |= ACTOR_FLAG_BIT12; break;
    }
}

/* Whether a walker can stand on the box at `arg`: a walker that is not a stray dog accepts a cell
 * inside the farm zone, and the rest settle the box on the ground (flags 0xC000). */
static s32 farm_walker_cell_free(VECTOR *arg) {
    if (farm_walker_not_stray == 1) {
        if (farm_zone_find(farm_area.mode.data, arg, 0) != 0) return 1;
    }
    return farm_settle_on_ground(arg, 0xC000) != 0;
}

/* The mode bit of today's season: spring 1, summer 2, fall 4, winter 8; unknown values count as
 * spring. */
static s32 farm_season_mode_bit(void) {
    s16 value;
    value = game_state.progress.clock.date.season;
    switch (value) {
    case SEASON_SPRING:
    default:
        return 1;
    case SEASON_SUMMER:
        return 2;
    case SEASON_FALL:
        return 4;
    case SEASON_WINTER:
        return 8;
    }
}

/* Set the actor's sprite animation and frame from its action and facing, then draw it. A resting
 * actor also gets sprite flag 0x100. */
static void farm_actor_sprite_refresh(FarmActor *object) {
    u16 first;
    u16 second;

    farm_table_lookup_pair(object->action, object->facing, &first, &second);
    if (first != object->sprite.animation) {
        anim_select_range(&object->sprite, first);
    }
    if ((second != 0) && (second != object->sprite.frame)) {
        anim_set_frame(&object->sprite, second);
    }
    anim_set_tick_flag(0);
    farm_draw_actor(object);
    if (object->action == ACTOR_ACTION_REST) {
        object->sprite.flags = object->sprite.flags | 0x100;
    }
}

static void farm_actor_pair_at_38(FarmAction *obj) {
    farm_pair_at_38(&obj->actor);
}

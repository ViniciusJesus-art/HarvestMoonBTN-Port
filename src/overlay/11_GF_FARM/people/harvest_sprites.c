/* Harvest sprites: the farm helpers' daily work, their scripts and their talk, and what each harvest is worth. */
#include <sys/types.h>
#include <memory.h>
#include <libgte.h>
#include "types.h"
#include "actor.h"
#include "anim.h"
#include "animals.h"
#include "clock.h"
#include "cue.h"
#include "estate.h"
#include "event_commands.h"
#include "event_flags.h"
#include "farm.h"
#include "farm_data.h"
#include "farm_main.h"
#include "farm_probe.h"
#include "farm_state.h"
#include "field.h"
#include "greenhouse.h"
#include "harvest_sprites.h"
#include "item_storage.h"
#include "items.h"
#include "livestock.h"
#include "menu_window.h"
#include "picture_cells.h"
#include "player.h"
#include "scene.h"
#include "screen_fade.h"
#include "sound_request.h"
#include "text.h"
#include "text_bank.h"
#include "villager.h"
#include "weather.h"

typedef struct FarmPlacement {
    s32 x;
    s32 y;
    s32 z;
    s32 key;
} FarmPlacement;

/* A farm slot's row holds five steps, each HARVEST_SLOT_STEP units along z. */
#define HARVEST_SLOT_STEP 600

/* A helper's work spots in one row are this far apart along x. */
#define HARVEST_SPOT_STEP 400

/* The item art lists of the barn and coop (PLACE_4 and PLACE_5), and of the greenhouse. */
#define HARVEST_ART_LIST_BARN_COOP 7
#define HARVEST_ART_LIST_GREENHOUSE 4

/* How close a helper must be to its walk target before it stops waiting. */
#define HARVEST_NEAR_PROXIMITY 230
#define HARVEST_FAR_PROXIMITY 600

/* The four walk-state flags a helper toggles while it walks to its work. */
#define HARVEST_WALK_FLAGS (ACTOR_FLAG_BIT26 | ACTOR_FLAG_BIT27 | ACTOR_FLAG_BIT28 | ACTOR_FLAG_BIT29)

/* A helper's answer to an order: one line per helper, from this id. */
#define HELPER_ANSWER_LINE 0x26C

/* How far in front of a helper the tile it works on lies. */
#define HARVEST_FRONT_DISTANCE 100

/* The work amount of a helper with no job, and the skill points per moveSpeeds entry. */
#define HARVEST_IDLE_AMOUNT 0x18
#define HARVEST_AMOUNT_STEP 52

/* From the 28th of winter on. */
#define HARVEST_IS_WINTER_DAY() (game_state.progress.clock.date.season == SEASON_WINTER && game_state.progress.clock.date.day >= 28)

/* The barn feed bins of the animals in mode slots 0 and 1. The animal pick hands these
 * two bins out as ids of their own. */
#define BARN_SLOT_BIN_0 0x14
#define BARN_SLOT_BIN_1 0x15

typedef struct {
    u16 x;
    u16 y;
} HarvestFrameEntry;

typedef struct {
    HarvestFrameEntry entries[4];
} HarvestFrameRow;

typedef struct {
    VECTOR position;
    s16 column;
    s16 row;
} FarmWorldCell;

typedef s32 (*CellFromWorldHarvest)(VECTOR *, s16 *, s16 *);

/* Test that one field tile qualifies for the search. */
typedef s32 (*FarmCheck)(FarmFieldTile *);

static void harvest_helper_refresh(FarmHelper *helper);
static void harvest_helper_pair_at_38(FarmHelper *obj);
static s32 harvest_helper_try_work(FarmHelper *obj);
static s32 harvest_sprite_tick(FarmHelper *object);
static s32 harvest_helper_timer_tick(FarmHelper *helper);
static void harvest_helper_event_step(FarmHelper *helper);
static s32 harvest_helper_init(FarmHelper *helper);
static s32 harvest_helper_sprite_source(FarmHelper *helper);
static s32 harvest_action_entry(s32 row, s32 entry, u16 *out_x, u16 *out_y);
static s32 harvest_helper_seasonal_task(FarmHelper *helper);
static s32 harvest_helper_work(FarmHelper *helper);
static s32 harvest_helper_claim_slots(FarmHelper *helper, s32 mode_arg);
static s32 harvest_helper_process_results(FarmHelper *helper, s16 value);
static s32 harvest_helper_place_item(FarmHelper *helper, s16 mode);
static s32 harvest_helper_talk(FarmHelper *helper);
static s32 harvest_helper_lower_affection(FarmHelper *obj);
static s32 harvest_helper_greet(FarmHelper *helper);
static s32 harvest_helper_react_item(FarmHelper *helper);
static s32 harvest_helper_set_work_state(FarmHelper *helper);
static s32 harvest_helper_script_tick(FarmHelper *helper);
static s32 harvest_obj_move_input_tick(FarmHelper *helper);
static s32 harvest_obj_move_action_tick(FarmHelper *helper);
static s32 harvest_obj_carry_tick(FarmHelper *object);
static s32 harvest_obj_place_tick(FarmHelper *helper);
static s32 harvest_obj_walk_tick(FarmHelper *helper);
static s32 harvest_obj_input_step(FarmHelper *helper, s32 mode);
static s32 harvest_obj_action_step(FarmHelper *helper, s32 mode);
static s32 harvest_obj_fetch_tick(FarmHelper *helper, s32 mode);
static s32 harvest_obj_handover_tick(FarmHelper *helper, s32 mode);
static s32 harvest_obj_claim_tick(FarmHelper *helper, s32 mode);
static s32 harvest_helper_release_item(FarmHelper *helper);
static s32 harvest_placement_lookup(FarmHelper *helper, s16 kind, VECTOR *output, s16 key);
static s32 harvest_obj_resolve_result(FarmHelper *helper);
static void harvest_obj_resolve_place(FarmActor *actor, s32 mode);
static u32 harvest_actor_wait_action(FarmActor *p, s32 raw_x, s32 raw_y);
static s32 harvest_helper_item_wait(FarmHelper *helper, s16 item);
static s32 harvest_obj_wait_walk(FarmHelper *helper, s32 proximity);
static s32 harvest_obj_walk_to_tile(FarmHelper *helper, s32 mode, s32 direction);
static s32 harvest_tile_match_category(FarmFieldTile *tile);
static s32 harvest_tile_is_dry_soil(FarmFieldTile *tile);
static s32 harvest_tile_is_plant(FarmFieldTile *tile);
static s32 harvest_tile_usable(FarmFieldTile *tile);
static s32 harvest_pick_animal(s16 mode, u16 flags);
static void harvest_slot_position(VECTOR *out, s16 value);
static void harvest_helper_spot_position(VECTOR *out, s16 in);
static void harvest_helper_clear_item(FarmHelper *helper);
static void harvest_helper_place_at_spot(FarmHelper *helper);
static void harvest_result_values(FarmHelper *helper, s32 *out1, s32 *out2);
static s32 harvest_result_amount(FarmHelper *helper);
static void harvest_item_sprite_load(FarmHelper *helper);
static s32 harvest_item_category_0(s16 item);
static s32 harvest_item_category_1(s16 event);
static s32 harvest_item_category_2(s16 event);
static s32 harvest_item_category_3(s16 item_id);
static s32 harvest_item_category(s16 item);
static s32 harvest_item_display_category(s16 item);

static HarvestFrameRow harvest_frame_rows[16] = {
    {{{0, 0}, {0, 1}, {0, 2}, {0, 3}}},
    {{{1, 0}, {2, 0}, {3, 0}, {4, 0}}},
    {{{5, 0}, {6, 0}, {7, 0}, {8, 0}}},
    {{{9, 0}, {10, 0}, {11, 0}, {12, 0}}},
    {{{13, 0}, {14, 0}, {15, 0}, {16, 0}}},
    {{{17, 0}, {18, 0}, {19, 0}, {20, 0}}},
    {{{21, 0}, {22, 0}, {23, 0}, {24, 0}}},
    {{{25, 0}, {26, 0}, {27, 0}, {28, 0}}},
    {{{29, 0}, {30, 0}, {31, 0}, {32, 0}}},
    {{{33, 0}, {34, 0}, {35, 0}, {36, 0}}},
    {{{37, 0}, {38, 0}, {39, 0}, {40, 0}}},
    {{{41, 0}, {42, 0}, {43, 0}, {44, 0}}},
    {{{45, 0}, {46, 0}, {47, 0}, {48, 0}}},
    {{{49, 0}, {49, 1}, {49, 2}, {49, 3}}},
    {{{50, 0}, {51, 0}, {52, 0}, {53, 0}}},
    {{{54, 0}, {54, 1}, {54, 2}, {54, 3}}},
};

static s16 harvest_helper_offsets[8] = {
    0x0000, 0x0003, 0x0001, 0x0006, 0x0002, 0x0004, 0x0005, 0x0000};

static FarmInitPair harvest_init_pairs[7] = {{2, 13}, {3, 21}, {1, 15}, {2, 9}, {0, 14}, {0, 3}, {0, 25}};

/* Offsets of each helper's name in the text bank. */
static s32 harvest_helper_name_offsets[7] = {offsetof(TextNames, harvest_sprites.chef), offsetof(TextNames, harvest_sprites.nappy), offsetof(TextNames, harvest_sprites.timid), offsetof(TextNames, harvest_sprites.hoggy), offsetof(TextNames, harvest_sprites.staid), offsetof(TextNames, harvest_sprites.bold), offsetof(TextNames, harvest_sprites.aqua)};

/* Clear and set up the seven helpers, then the animal move speeds. */
void harvest_helpers_init(void) {
    FarmHelper *records = game_state.helpers;
    FarmHelper *record;
    TextBankStarts *offsets;
    s32 i;

    memset(records, 0, 7 * sizeof(FarmHelper));
    record = records;
    for (i = 0, offsets = &text_bank_starts; i < HELPER_COUNT; i++, record++) {
        harvest_helper_init(record);
        memcpy(record->actor.name, TEXT_AT_OFFSET(offsets->names + harvest_helper_name_offsets[i]), sizeof(record->actor.name));
        record->number = i;
        harvest_helper_place_at_spot(record);
        record->pairC4 = harvest_init_pairs[i];
    }
    for (i = 0; i < 5; i++) {
        game_state.animals.moveSpeeds[i] = i * 6 + 12;
    }
}

void harvest_helpers_update(void) {
    FarmHelper *p;
    s32 i;

    if (game_state.debugFlags & DEBUG_FLAG_NO_HARVEST_SPRITES) {
        return;
    }
    p = game_state.helpers;
    for (i = 0; i < HELPER_COUNT; i++, p++) {
        if (p->flags & HELPER_BIT5) {
            continue;
        }
        if (farm_area.mode.place.location != PLACE_28) {
            p->idleCount = 1;
        }
        harvest_helper_event_step(p);
        if (!(game_state.globalFlags & GLOBAL_FLAG_PAUSED)) {
            if (!(game_state.globalFlags & GLOBAL_FLAG_CLOCK_TICKED)) {
                if (farm_area.mode.place.location != p->actor.place.location) {
                    continue;
                }
                harvest_helper_timer_tick(p);
                harvest_helper_pair_at_38(p);
            } else {
                harvest_helper_timer_tick(p);
                harvest_helper_pair_at_38(p);
            }
        }
        if (farm_area.mode.place.location == p->actor.place.location) {
            if (!(game_state.globalFlags & GLOBAL_FLAG_PAUSED)) {
                cue_countdown(&p->actor.cue);
            }
            harvest_helper_sprite_source(p);
            harvest_helper_refresh(p);
        }
    }
}

void harvest_helpers_work_all(void) {
    FarmHelper *p;
    s32 i;
    p = game_state.helpers;
    i = 6;
    do {
        if (!(p->flags & HELPER_BIT5)) {
            harvest_helper_try_work(p);
        }
        i--;
        p++;
    } while (i >= 0);
}

void harvest_helpers_tick_all(void) {
    FarmHelper *p = game_state.helpers;
    s32 i = 6;
    do {
        if (!(p->flags & HELPER_BIT5)) {
            harvest_sprite_tick(p);
        }
        i--;
        p++;
    } while (i >= 0);
}

/* Refresh a helper's displayed pose and carried item. */
static void harvest_helper_refresh(FarmHelper *helper) {
    u16 animation;
    u16 frame;

    harvest_action_entry(helper->actor.action, helper->actor.facing, &animation, &frame);
    if (animation != helper->actor.sprite.animation) anim_select_range(&helper->actor.sprite, animation);
    if (helper->actor.action == ACTOR_ACTION_REST || helper->actor.action == ACTOR_ACTION_CARRY_REST) {
        if (frame != helper->actor.sprite.frame) anim_set_frame(&helper->actor.sprite, frame);
    }
    harvest_item_sprite_load(helper);
    farm_draw_object(helper);
}

static void harvest_helper_pair_at_38(FarmHelper *obj) {
    farm_pair_at_38(&obj->actor);
}

/* Run a helper's work step, unless the weather is a storm, no work slot is pending,
 * it is outside work hours, or it is already walking home. */
static s32 harvest_helper_try_work(FarmHelper *obj) {
    u16 weather;
    s32 status;
    weather = game_state.world.weather;
    if ((u32)(weather - WEATHER_STORM) < 2U) {
        return 0;
    }
    status = clock_is_pending();
    if (status != 1) {
        return 0;
    }
    if ((u32)((u16)game_state.progress.clock.time.hm.hour - HELPER_WORK_START_HOUR) >= HELPER_WORK_HOURS) {
        return 0;
    }
    if ((obj->flags & HELPER_BIT6) != 0) {
        return 0;
    }
    harvest_helper_work(obj);
    return 0;
}

/* One day's tick of a helper. Talking to it today raises its affection; any job
 * lowers it by 2 and raises that job's skill, each capped at 255. While jobDays
 * runs it works from the fixed spot. When the days run out the jobs are cleared
 * and the helper goes back to action 0. */
static s32 harvest_sprite_tick(FarmHelper *helper)
{
    if (helper->actor.flags & ACTOR_FLAG_TALKED_TODAY) {
        helper->actor.affection++;
    }
    if (helper->flags & HELPER_JOBS) {
        helper->actor.affection -= 2;
    }
    if (helper->flags & HELPER_HARVEST) {
        helper->harvestSkill++;
        helper->harvestSkill = helper->harvestSkill > HELPER_SKILL_MAX ? HELPER_SKILL_MAX : helper->harvestSkill;
    }
    if (helper->flags & HELPER_WATER) {
        helper->waterSkill++;
        helper->waterSkill = helper->waterSkill > HELPER_SKILL_MAX ? HELPER_SKILL_MAX : helper->waterSkill;
    }
    if (helper->flags & HELPER_ANIMALS) {
        helper->animalSkill++;
        helper->animalSkill = helper->animalSkill > HELPER_SKILL_MAX ? HELPER_SKILL_MAX : helper->animalSkill;
    }
    helper->actor.affection = helper->actor.affection < 0 ? 0 : helper->actor.affection;
    helper->actor.affection = helper->actor.affection > HELPER_AFFECTION_MAX ? HELPER_AFFECTION_MAX : helper->actor.affection;
    harvest_helper_clear_item(helper);
    if (helper->jobDays != 0 && --helper->jobDays != 0) {
        harvest_helper_set_action(helper, HELPER_HANDLER_PLACE);
        helper->actor.facing = FACING_DOWN;
        helper->actor.position.vx = HELPER_SPOT_X;
        helper->actor.position.vy = 0;
        helper->actor.place.location = PLACE_FARM;
        helper->actor.position.vz = HELPER_SPOT_Z_BASE - helper->number * HELPER_SPOT_Z_STEP;
    } else {
        helper->flags &= ~HELPER_JOBS;
        harvest_helper_set_action(helper, 0);
        helper->savedTaskNo = 0;
        harvest_helper_place_at_spot(helper);
    }
    helper->actor.interruptTime = rand_range(HELPER_TASK_INTERVAL);
    helper->flags &= ~(HELPER_BIT3 | HELPER_MARKED | HELPER_BIT6 | HELPER_BIT8 | HELPER_BIT9);
    helper->actor.flags &= ~ACTOR_FLAG_TALKED_TODAY;
    helper->actor.flags &= ~ACTOR_FLAG_APPROACHING_PLAYER;
    return 0;
}

s32 (*const harvest_helper_start_handlers[14])() = {
    0, 0, 0, 0, 0, harvest_helper_lower_affection, harvest_helper_talk, 0, 0, 0, 0, 0, 0, harvest_helper_greet,
};

s32 (*const harvest_helper_tick_handlers[14])() = {
    harvest_helper_idle_cue, harvest_helper_cue_1, harvest_helper_cue_2, harvest_helper_cue_3, harvest_helper_cue_4,
    harvest_helper_scold_cue, harvest_helper_script_tick, harvest_helper_set_work_state, harvest_obj_move_input_tick, harvest_obj_move_action_tick,
    harvest_obj_carry_tick, harvest_obj_place_tick, harvest_obj_walk_tick, 0,
};

/* Run a helper's current action. When its interrupt timer is due, apply a seasonal task
 * and reset the timer. Outside work hours a helper that holds nothing walks home.
 * Then count the timer up. */
static s32 harvest_helper_timer_tick(FarmHelper *helper) {
    s32 (*const *actions)();
    s32 (*action)();
    u16 action_id;
    u16 clock;

    action_id = helper->actor.taskNo;
    actions = harvest_helper_tick_handlers;
    action = actions[action_id];
    if (action != 0) {
        action();
    }
    if (!(helper->actor.flags & ACTOR_FLAG_CUE_RUNNING) && helper->actor.interruptTime >= HELPER_TASK_INTERVAL) {
        harvest_helper_seasonal_task(helper);
        helper->actor.interruptTime = 0;
    }
    if (helper->actor.place.location != PLACE_28 && (u32)((u16)game_state.progress.clock.time.hm.hour - HELPER_WORK_START_HOUR) >= HELPER_WORK_HOURS && helper->actor.taskNo != HELPER_HANDLER_WALK &&
        helper->item == ITEM_NOTHING) {
        helper->savedTaskNo = 0;
        helper->flags |= HELPER_BIT6;
        helper->actor.flags &= ~ACTOR_FLAG_CUE_RUNNING;
        harvest_helper_set_action(helper, HELPER_HANDLER_WALK);
    }
    clock = helper->actor.interruptTime;
    clock++;
    helper->actor.interruptTime = clock;
    return 0;
}

/* Start a helper on an action. The action's start handler runs first; a result of -1
 * (the helper is already on it with its cue running) keeps the current action. */
s32 harvest_helper_set_action(FarmHelper *helper, s32 action) {
    s32 result;

    result = 0;
    if (harvest_helper_start_handlers[action] != 0) {
        if (helper->actor.taskNo == action && (helper->actor.flags & ACTOR_FLAG_CUE_RUNNING)) {
            result = -1;
        } else {
            result = harvest_helper_start_handlers[action](helper);
        }
    }
    if (harvest_helper_tick_handlers[action] != 0 && result != -1) {
        helper->actor.taskNo = action;
        helper->actor.taskId = 0;
        helper->actor.taskTime = 0;
        helper->actor.interruptTime = 0;
        helper->flags &= ~HELPER_BIT11;
    }
    return result;
}

/* Once the text window has closed and no event is running, put a helper back at its spot. */
static void harvest_helper_event_step(FarmHelper *helper) {
    if (helper->flags & HELPER_BIT11) {
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            helper->flags &= ~HELPER_BIT11;
            event_item_can_take(game_state.player.held[1].id, 1, 0, 1);
            if (event_flag_get(EVENT_FLAG_REQUEST_BUSY) == 0) {
                if (helper->actor.place.location == PLACE_28 && helper->actor.taskNo != HELPER_HANDLER_SCRIPT) {
                    harvest_helper_place_at_spot(helper);
                }
            }
        }
    } else if (helper->actor.place.location == PLACE_28) {
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            if (event_flag_get(EVENT_FLAG_REQUEST_BUSY) == 0 && helper->actor.taskNo != HELPER_HANDLER_SCRIPT) {
                harvest_helper_place_at_spot(helper);
            }
        }
    }
}

/* Clear a helper record and set its starting place, affection and timer. */
static s32 harvest_helper_init(FarmHelper *helper) {
    u16 unset = 0xFFFF;

    memset(helper, 0, sizeof(FarmHelper));
    helper->actor.date = game_state.progress.clock.date;
    helper->actor.place.location = PLACE_FARM;
    helper->actor.name[0] = unset;
    helper->targetAnimal = unset;
    helper->savedTaskNo = 0;
    helper->actor.sprite.source = 0;
    helper->actor.affection = HELPER_START_AFFECTION;
    helper->actor.flags |= 0x3C000000;
    cue_reset(&helper->actor.cue);
    helper->actor.interruptTime = rand_range(HELPER_TASK_INTERVAL);
    return 0;
}

/* Choose the helper's sprite for the current area: the villager sprite in PLACE_28,
 * the farm sprite bank in the farm and the other farm places, else keep the current
 * one. When it changes and the helper is in the current area, attach it and restart
 * its frame. */
static s32 harvest_helper_sprite_source(FarmHelper *helper)
{
    void *source;
    s32 resource = 10;
    s32 offset = ANIM_PACK_START(farm_slot_tocs[0], resource - 1);

    switch (farm_area.mode.place.location) {
    case PLACE_28:
        source = game_state.villagers[CHARACTER_UNNAMED_27].sprite80.source;
        break;
    case PLACE_FARM:
    case PLACE_3:
    case PLACE_4:
    case PLACE_5:
    case PLACE_GREENHOUSE:
        source = farm_area.slotAnims + offset;
        break;
    default:
        source = helper->actor.sprite.source;
        break;
    }
    if (source != helper->actor.sprite.source) {
        if (farm_area.mode.place.location == helper->actor.place.location) {
            anim_bind_file(&helper->actor.sprite, source);
            anim_set_offsets(&helper->actor.sprite, 0, 0, 0, harvest_helper_offsets[helper->number]);
        }
    }
    helper->actor.kind = ITEM_HARVEST_SPRITE;
    return 0;
}

static s32 harvest_action_entry(s32 row, s32 entry, u16 *out_x, u16 *out_y) {
    u16 i0 = row;
    u16 i1 = entry;
    *out_x = harvest_frame_rows[i0].entries[i1].x;
    if (out_y != 0) *out_y = harvest_frame_rows[i0].entries[i1].y;
    return 0;
}

static s16 harvest_seasonal_results[4] = {0x0007, 0x0003, 0x0002, 0x0004};

/* Pick a random action for a helper outside PLACE_28 and start it, or return to its
 * saved task. */
static s32 harvest_helper_seasonal_task(FarmHelper *helper) {
    s16 result;
    s32 random_value;
    s16 *table;

    result = 0;
    random_value = rand_range(4);
    if (helper->actor.place.location != PLACE_28) {
        if (helper->actor.taskNo == 0xF) {
            result = 1;
        } else {
            table = harvest_seasonal_results;
            result = table[random_value];
        }
    }
    if (helper->savedTaskNo != 0 && helper->actor.taskNo != 0xF) {
        helper->actor.taskNo = helper->savedTaskNo;
        helper->actor.taskId = helper->savedTaskId;
        helper->actor.taskTime = helper->savedTaskTime;
    } else if (result != helper->actor.taskNo) {
        harvest_helper_set_action(helper, result);
    }
    return 0;
}

/* One work step of a helper, chosen by its job: harvest ships the items it takes
 * from the field, water works a random number of tiles, animals hands in the item
 * it carries and runs the placement chain. Stops early when no tile is left. */
static s32 harvest_helper_work(FarmHelper *helper) {
    s16 column;
    s16 row;
    s16 item;
    s32 count;
    s32 i;
    FarmFieldTile *tile;

    if (helper->flags & HELPER_HARVEST) {
        count = helper->harvestSkill / 50 + rand_range(3);
        for (i = 0; i < count; i++) {
            if (helper->item != ITEM_NOTHING) {
                item = helper->item;
                harvest_helper_clear_item(helper);
            } else {
                if (harvest_find_free_tile(&column, &row, 1, 1, 1) != 0) {
                    if (harvest_find_free_tile(&column, &row, 0, 1, 1) != 0) {
                        return 0;
                    }
                    tile = game_state.slots7x7[0].rows + row * 7 + column;
                } else {
                    tile = game_state.fieldTiles[0].rows + row * 45 + column;
                }
                item = field_tile_item_stage(tile);
            }
            estate_shipment_add((u16)item);
        }
    } else if (helper->flags & HELPER_WATER) {
        count = helper->waterSkill / 10 + rand_range(5);
        i = 0;
        goto next;
    work_plot:
        field_gate_open(1, column, row, 0);
        goto counted;
    work_field:
        field_gate_open(0, column, row, 0);
    counted:
        i++;
    next:
        if (i >= count) {
            return 0;
        }
        if (harvest_find_free_tile(&column, &row, 1, 0, 1) == 0) {
            goto work_field;
        }
        if (harvest_find_free_tile(&column, &row, 0, 0, 1) == 0) {
            goto work_plot;
        }
        return 0;
    } else if (helper->flags & HELPER_ANIMALS) {
        switch (helper->item) {
        case ITEM_FODDER:
            estate_fodder_add(0, 0);
            break;
        case ITEM_CHICKEN_FEED:
            estate_chicken_feed_add(0, 0);
            break;
        case ITEM_NOTHING:
            break;
        default:
            estate_shipment_add(helper->item);
            break;
        }
        harvest_helper_clear_item(helper);
        if (harvest_helper_claim_slots(helper, 5) == 0) {
            return 0;
        }
        if (harvest_helper_process_results(helper, 5) == 0 && harvest_helper_place_item(helper, 5) == 0) {
            return 0;
        }
        if (harvest_helper_claim_slots(helper, 4) == 0) {
            return 0;
        }
        if (harvest_helper_place_item(helper, 4) == 0) {
            return 0;
        }
        if (harvest_helper_process_results(helper, 1) != 0) {
            return 0;
        }
        if (harvest_helper_place_item(helper, 1) == 0) {
            return 0;
        }
    }
    return 0;
}

/* Fill empty feed bins: each pick hands one chicken feed to a coop chicken (mode 4) or
 * fodder to a barn animal (mode 5). Tries up to a random count set by animalSkill.
 * Returns 1 when no animal or no feed is left. */
static s32 harvest_helper_claim_slots(FarmHelper *helper, s32 mode_arg) {
    s32 result;
    s32 count;
    s32 limit;
    s32 slot;
    s16 mode;

    mode = mode_arg;
    result = 0;
    count = 0;
    limit = ((helper->animalSkill / 50) & 0xFFFF) + rand_range(3);
    do {
        if (count >= limit) {
            break;
        }
        slot = (s16)harvest_pick_animal(mode, 1);
        if (slot == -1) {
            result = 1;
            break;
        }
        if (mode == 4) {
            if (estate_chicken_feed_take(1) != 0) {
                farm_feed_bin_set(slot, 0);
            } else {
                result = 1;
            }
        } else if (mode == 5) {
            if (estate_fodder_take(1) == 0) {
                result = 1;
            } else {
                farm_feed_bin_set(slot, 1);
            }
        }
        count++;
    } while (1);
    return result;
}

/* Send up to a random count (set by animalSkill) of idle animals in `mode` to livestock
 * action 6. Returns 1 when none is left. */
static s32 harvest_helper_process_results(FarmHelper *helper, s16 mode)
{
    s32 result;
    s32 slot;
    s32 limit;
    s32 status;
    limit = helper->animalSkill / 100 + rand_range(3);
    result = 0;
    for (slot = 0; slot < limit; slot++) {
        status = (s16)harvest_pick_animal(mode, 2);
        if (status == -1) {
            result = 1;
            break;
        }
        if ((game_state.animals.livestock[status].actor.flags & ACTOR_FLAG_BIT16) == 0) {
            livestock_action_dispatch(&game_state.animals.livestock[status], 6);
        } else {
            livestock_action_dispatch_b(&game_state.animals.livestock[status], 6);
        }
    }
    return result;
}

/* Place the helper's item: try the animal slots first, then up to `limit` records,
 * each handing the helper the id it found. A placed item is shipped. Returns 1
 * when a try failed. */
static s32 harvest_helper_place_item(FarmHelper *helper, s16 mode)
{
    FarmStoredItem found;
    FarmLivestock *items;
    s32 limit;
    s32 slot;
    s32 failed;
    s32 i;

    limit = helper->animalSkill / 100 + rand_range(3);
    failed = 0;
    items = game_state.animals.livestock;
    goto try_pool4;
place:
    items[slot].helperNumber = helper->number;
    if ((items[slot].actor.flags & ACTOR_FLAG_BIT16) == 0) {
        livestock_action_dispatch(&items[slot], 7);
    } else {
        livestock_action_dispatch_b(&items[slot], 8);
    }
    if (helper->item == ITEM_NOTHING) {
        failed = 1;
    }
    goto pool4_done;
try_pool4:
    if (limit > 0) {
        slot = (s16)harvest_pick_animal(mode, 4);
        if (slot != -1) {
            goto place;
        }
        failed = 1;
    }
pool4_done:
    if (failed) {
        failed = 0;
        for (i = 0; i < limit; i++) {
            slot = (s16)harvest_pick_animal(mode, 8);
            if (slot != -1) {
                if (estate_field_pickup(&game_state.runs.records[16][(s16)slot], &found) == 1) {
                    helper->item = found.id;
                } else {
                    failed = 1;
                    break;
                }
            } else {
                failed = 1;
                break;
            }
        }
    }
    if (helper->item != ITEM_NOTHING) {
        estate_shipment_add(helper->item);
        harvest_helper_clear_item(helper);
    }
    return failed;
}

/* The direction opposite `facing` (four directions). */
static inline u16 facing_away(u16 facing) {
    if (facing < 2) {
        return facing + 2;
    }
    return facing - 2;
}

static s16 harvest_helper_messages[8] = {
    0x0147, 0x015C, 0x0160, 0x0150, 0x014C, 0x0154, 0x0158, 0x0000};

/* Talk to a farm helper: face the player and, unless the held item gets a reply, show
 * the message for its current state. Each talk toggles HELPER_BIT3. */
static s32 harvest_helper_talk(FarmHelper *helper) {
    s32 choose;
    s32 idle;
    s32 result;
    s32 group;
    s32 message;
    u16 facing;
    u16 flags;

    choose = 1;
    idle = 0;
    helper->actor.flags |= ACTOR_FLAG_TALKED_TODAY;
    result = -1;
    facing = game_state.player.facing38;
    group = 0;
    message = 0;
    helper->actor.facing = facing_away(facing);
    if (game_state.player.held[HELD_ITEM].id != ITEM_NOTHING && harvest_helper_react_item(helper) == 0) {
        game_state.player.stateFlags &= ~PLAYER_FLAG_SCENE_CHANGE;
        return -1;
    }
    if (helper->actor.place.location == PLACE_28) {
        if (helper->jobDays >= 2) {
            choose = 0;
            group = 2;
            message = harvest_helper_messages[helper->number] + 1;
        } else if (helper->flags & HELPER_BIT3) {
            if (helper->actor.affection >= 50) {
                s16 column;
                s16 row;

                if (harvest_find_free_tile(&column, &row, 1, 1, 1) == 0
                    || harvest_find_free_tile(&column, &row, 1, 0, 0) == 0
                    || harvest_find_free_tile(&column, &row, 0, 1, 1) == 0
                    || harvest_find_free_tile(&column, &row, 0, 0, 0) == 0
                    || harvest_pick_animal(1, 0xF) != -1
                    || harvest_pick_animal(4, 0xF) != -1
                    || harvest_pick_animal(5, 0xF) != -1) {
                    idle = 1;
                }
            }
            if (idle == 1) {
                if (HARVEST_IS_WINTER_DAY()) {
                    if (++helper->idleCount < 6) {
                        idle = 0;
                    }
                } else if (game_state.progress.clock.date.season == SEASON_SPRING) {
                    if (++helper->idleCount < 6) {
                        idle = 0;
                    }
                }
                if (idle == 1) {
                    choose = 0;
                    result = 0;
                    group = 2;
                    message = harvest_helper_messages[helper->number];
                }
            }
        }
    }
    if (choose == 1) {
        harvest_result_values(helper, &group, &message);
    }
    menu_request_open(group, message);
    flags = helper->flags;
    if (flags & HELPER_BIT3) {
        if (helper->idleCount < 4) {
            helper->flags = flags & ~HELPER_BIT3;
        }
    } else {
        helper->flags = flags | HELPER_BIT3;
        if (helper->idleCount != 0) {
            helper->idleCount++;
        }
    }
    if (result == -1) {
        game_state.player.stateFlags &= ~PLAYER_FLAG_SCENE_CHANGE;
    }
    return result;
}

/* Lower the helper's affection by 3 and play its upset cue. Returns -1 in PLACE_28. */
static s32 harvest_helper_lower_affection(FarmHelper *obj) {
    obj->actor.affection -= 3;
    obj->actor.flags &= ~ACTOR_FLAG_CUE_RUNNING;
    obj->actor.affection = obj->actor.affection < 0 ? 0 : obj->actor.affection;
    obj->actor.affection = obj->actor.affection > HELPER_AFFECTION_MAX ? HELPER_AFFECTION_MAX : obj->actor.affection;
    cue_set(&obj->actor.cue, 0x24, 0x1E);
    if (obj->actor.place.location == PLACE_28) return -1;
    return 0;
}

static s16 harvest_helper_greet_menus[8] = {0x0577, 0x068F, 0x06A7, 0x0647,
                               0x062F, 0x065F, 0x0677, 0x0000};

/* Greet the player: face him and open the helper's greeting menu. */
static s32 harvest_helper_greet(FarmHelper *helper) {
    s32 result;
    s16 message;
    u16 facing;

    helper->actor.flags |= ACTOR_FLAG_TALKED_TODAY;
    result = -1;
    facing = game_state.player.facing38;
    helper->actor.facing = facing_away(facing);
    message = harvest_helper_greet_menus[helper->number];
    menu_request_open(0, message);
    if (result == -1) {
        game_state.player.stateFlags &= ~PLAYER_FLAG_SCENE_CHANGE;
    }
    return result;
}

static u16 harvest_helper_cue_ids[8] = {
    0x0572, 0x068A, 0x06A2, 0x0642, 0x062A, 0x065A, 0x0672, 0x0000};

static s16 harvest_helper_special_cue_ids[8] = {
    0x06E5, 0x06EA, 0x06EB, 0x06E7, 0x06E6, 0x06E8, 0x06E9, 0x0000};

static s16 harvest_helper_mood_deltas[2][5] = {
    {9, 3, 1, -3, -9}, {3, 1, 0, -3, -9},
};

/* React to the item the player holds. The item's villager reaction moves the mood by the
 * table amount, five times over on the helper's special day, and picks a line; a few
 * item ids override the line. Helper 0 gets its one-time reward line once her affection
 * is over 100 and the estate's house level is set. Clamps the mood and opens the line
 * (flagging the helper when the reaction counts) and returns 0, or returns -1 when there
 * is no line. */
static s32 harvest_helper_react_item(FarmHelper *helper) {
    s16 cue;
    s32 sets_flags;
    s32 rewarded;
    s32 factor;
    s32 marked;
    s16 item;

    cue = 0;
    sets_flags = 0;
    rewarded = 0;
    factor = 1;
    marked = 0;
    if (helper->flags & HELPER_MARKED) {
        marked = 1;
    }
    item = game_state.player.held[HELD_ITEM].id;
    if (helper->pairC4.kind == game_state.progress.clock.date.season && helper->pairC4.count == game_state.progress.clock.date.day) {
        factor = 5;
    }
    if (item == ITEM_GIFT) {
        item = game_state.player.held[HELD_ITEM].modifier;
        helper->actor.affection += 2;
    }
    switch (item_villager_reaction(item, CHARACTER_UNNAMED_27)) {
    case ITEM_REACTION_BEST:
        cue = harvest_helper_cue_ids[helper->number];
        helper->actor.affection += harvest_helper_mood_deltas[marked][0] * factor;
        if (factor != 1) {
            cue = harvest_helper_special_cue_ids[helper->number];
        }
        sets_flags = 1;
        rewarded = 1;
        break;
    case ITEM_REACTION_GOOD:
        cue = harvest_helper_cue_ids[helper->number] + 1;
        helper->actor.affection += harvest_helper_mood_deltas[marked][1] * factor;
        if (factor != 1) {
            cue = harvest_helper_special_cue_ids[helper->number];
        }
        sets_flags = 1;
        rewarded = 1;
        break;
    case ITEM_REACTION_OK:
        cue = harvest_helper_cue_ids[helper->number] + 2;
        helper->actor.affection += harvest_helper_mood_deltas[marked][2] * factor;
        if (factor != 1) {
            cue = harvest_helper_special_cue_ids[helper->number];
        }
        sets_flags = 1;
        rewarded = 1;
        break;
    case ITEM_REACTION_BAD:
        sets_flags = 1;
        cue = harvest_helper_cue_ids[helper->number] + 3;
        helper->actor.affection += harvest_helper_mood_deltas[marked][3] * factor;
        break;
    case ITEM_REACTION_WORST:
        sets_flags = 1;
        cue = harvest_helper_cue_ids[helper->number] + 4;
        helper->actor.affection += harvest_helper_mood_deltas[marked][4] * factor;
        break;
    case ITEM_REACTION_QUIET:
        cue = harvest_helper_cue_ids[helper->number] + 7;
        break;
    case ITEM_REACTION_UPSET:
        cue = harvest_helper_cue_ids[helper->number] + 8;
        break;
    }
    if (item >= ITEM_BRACELET) {
        if (item >= ITEM_BANDAGE) {
            if (item == ITEM_PERFUME) {
                sets_flags = 0;
                cue = harvest_helper_cue_ids[helper->number] + 6;
            }
        } else {
            sets_flags = 1;
            cue = harvest_helper_cue_ids[helper->number] + 9;
        }
    }
    if (rewarded == 1 && helper->number == 0 && cue != 0 && !(helper->flags & HELPER_REWARDED)
        && game_state.progress.estate.houseLevel != 0 && helper->actor.affection > 100 && item != ITEM_SWEET_POTATO_DISH) {
        cue = harvest_helper_cue_ids[helper->number] + 10;
        helper->flags |= HELPER_REWARDED;
    }
    helper->actor.affection = helper->actor.affection > HELPER_AFFECTION_MAX ? HELPER_AFFECTION_MAX : helper->actor.affection;
    helper->actor.affection = helper->actor.affection < 0 ? 0 : helper->actor.affection;
    if (cue != 0) {
        menu_request_open(0, cue);
        if (sets_flags == 1) {
            helper->flags |= HELPER_BIT11 | HELPER_MARKED;
        }
        return 0;
    }
    return -1;
}

FARM_CUE_RUNNER(harvest_helper_idle_cue, {0, 30}, {-1, 0})

FARM_CUE_RUNNER(harvest_helper_cue_1, {13, 30}, {14, 15}, {-1, 0})

FARM_CUE_RUNNER_NO_TIMER(harvest_helper_cue_2, {2, 15}, {13, 30}, {-1, 0})

FARM_CUE_RUNNER_NO_TIMER(harvest_helper_cue_3, {3, 15}, {-1, 0})

FARM_CUE_RUNNER_NO_TIMER(harvest_helper_cue_4, {12, 15}, {-1, 0})

/* Hold the helper for its work state: walk it at the amount from harvest_result_amount,
 * pick the rest or carry pose by whether it holds an item, and clear the cue flag. */
static s32 harvest_helper_set_work_state(FarmHelper *helper) {
    s32 result;
    s32 moveAction;
    s32 restAction;

    result = harvest_result_amount(helper);
    farm_walker_move(&helper->actor, result, OCCUPANT_CHOICE | OCCUPANT_TILE | OCCUPANT_TILE_BIT6);
    restAction = ACTOR_ACTION_CARRY_REST;
    if (helper->item == ITEM_NOTHING) {
        restAction = ACTOR_ACTION_REST;
        moveAction = ACTOR_ACTION_MOVING;
    } else {
        moveAction = ACTOR_ACTION_CARRY_MOVING;
    }
    if (helper->actor.speed.vx == 0 && helper->actor.speed.vy == 0 && helper->actor.speed.vz == 0) {
        helper->actor.action = restAction;
    } else {
        helper->actor.action = moveAction;
    }
    helper->actor.flags &= ~ACTOR_FLAG_CUE_RUNNING;
    return 1;
}

static s16 harvest_helper_lines[8] = {
    0x0147, 0x015C, 0x0160, 0x0150, 0x014C, 0x0154, 0x0158, 0x0000};

/* The steps of a helper's script. */
enum HelperScript {
    HELPER_SCRIPT_ORDER = 0,
    HELPER_SCRIPT_RELEASE = 1,
    HELPER_SCRIPT_ASK = 2,
    HELPER_SCRIPT_ANSWER = 3,
    HELPER_SCRIPT_MESSAGE = 4,
    HELPER_SCRIPT_FAREWELL = 5,
    HELPER_SCRIPT_DONE = -1,
};

/* The orders the player can give a helper, in menu order. */
enum HelperOrder {
    HELPER_ORDER_NONE = 0,
    HELPER_ORDER_HARVEST = 1,
    HELPER_ORDER_WATER = 2,
    HELPER_ORDER_ANIMALS = 3,
    HELPER_ORDER_CANCEL = 4,
};

/* The answers to the day count question, in menu order. */
enum HelperAnswer {
    HELPER_ANSWER_NONE = 0,
    HELPER_ANSWER_TWO_DAYS = 1,
    HELPER_ANSWER_FOUR_DAYS = 2,
    HELPER_ANSWER_EIGHT_DAYS = 3,
    HELPER_ANSWER_DECLINE = 4,
};

/* One tick of a helper's script. Every tick holds the player and turns the
 * helper to face him. The order step takes the player's choice of work, the answer
 * step takes the number of days and sets jobDays, the ask and farewell steps show
 * the helper's line and the message step a fixed message. Release and done end the
 * script and return 1; every other tick returns 0. */
static s32 harvest_helper_script_tick(FarmHelper *helper) {
    s32 result;
    s16 column;
    s16 row;
    s32 first;
    s32 second;
    u16 facing;
    s16 message;

    result = 0;
    game_state.player.scriptRunning = 1;
    game_state.player.scriptHeld = 0;
    game_state.player.scriptPressed = 0;
    game_state.player.stateFlags |= (PLAYER_FLAG_SCENE_CHANGE | PLAYER_FLAG_SCRIPTED);
    facing = game_state.player.facing38;
    helper->actor.facing = facing_away(facing);
    switch (helper->actor.taskId) {
    case HELPER_SCRIPT_ORDER:
        helper->actor.flags |= ACTOR_FLAG_CUE_RUNNING;
        switch (menu_choice()) {
        case HELPER_ORDER_NONE:
            break;
        case HELPER_ORDER_HARVEST:
            helper->actor.taskId = HELPER_SCRIPT_ASK;
            if (harvest_find_free_tile(&column, &row, 1, 1, 1) != 0
                && harvest_find_free_tile(&column, &row, 0, 1, 1) != 0) {
                helper->actor.taskId = HELPER_SCRIPT_MESSAGE;
            }
            helper->flags |= HELPER_BIT6 | HELPER_HARVEST;
            break;
        case HELPER_ORDER_WATER:
            helper->actor.taskId = HELPER_SCRIPT_ASK;
            if (harvest_find_free_tile(&column, &row, 1, 0, 0) != 0
                && harvest_find_free_tile(&column, &row, 0, 0, 0) != 0) {
                helper->actor.taskId = HELPER_SCRIPT_MESSAGE;
            }
            helper->flags |= HELPER_BIT6 | HELPER_WATER;
            break;
        case HELPER_ORDER_ANIMALS:
            helper->actor.taskId = HELPER_SCRIPT_ASK;
            first = harvest_pick_animal(1, 0xF);
            if (first == -1) {
                second = harvest_pick_animal(4, 0xF);
                if (second == first && harvest_pick_animal(5, 0xF) == second) {
                    helper->actor.taskId = HELPER_SCRIPT_MESSAGE;
                }
            }
            helper->flags |= HELPER_BIT6 | HELPER_ANIMALS;
            break;
        case HELPER_ORDER_CANCEL:
            helper->actor.taskId = HELPER_SCRIPT_FAREWELL;
            break;
        }
        break;
    case HELPER_SCRIPT_ASK:
        message = harvest_helper_lines[helper->number];
        menu_request_open(2, (helper->number != 0) ? message + 2 : message + 3);
        helper->actor.taskId = HELPER_SCRIPT_ANSWER;
        break;
    case HELPER_SCRIPT_ANSWER:
        switch (menu_choice()) {
        case HELPER_ANSWER_NONE:
            break;
        case HELPER_ANSWER_TWO_DAYS:
            helper->jobDays = 2;
            helper->actor.taskId = HELPER_SCRIPT_DONE;
            menu_request_open(2, helper->number + HELPER_ANSWER_LINE);
            break;
        case HELPER_ANSWER_FOUR_DAYS:
            helper->jobDays = 4;
            helper->actor.taskId = HELPER_SCRIPT_DONE;
            menu_request_open(2, helper->number + HELPER_ANSWER_LINE);
            break;
        case HELPER_ANSWER_EIGHT_DAYS:
            helper->jobDays = 8;
            helper->actor.taskId = HELPER_SCRIPT_DONE;
            menu_request_open(2, helper->number + HELPER_ANSWER_LINE);
            break;
        case HELPER_ANSWER_DECLINE:
            helper->actor.taskId = HELPER_SCRIPT_FAREWELL;
            break;
        }
        break;
    case HELPER_SCRIPT_MESSAGE:
        menu_request_open(2, 0x149);
        helper->actor.taskId = HELPER_SCRIPT_RELEASE;
        break;
    case HELPER_SCRIPT_FAREWELL:
        message = harvest_helper_lines[helper->number];
        menu_request_open(2, (helper->number != 0) ? message + 3 : message + 4);
        helper->actor.taskId = HELPER_SCRIPT_RELEASE;
        break;
    case HELPER_SCRIPT_RELEASE:
        helper->actor.taskId = HELPER_SCRIPT_DONE;
        helper->flags &= ~(HELPER_BIT6 | HELPER_JOBS);
        /* fall through */
    case HELPER_SCRIPT_DONE:
        game_state.player.scriptRunning = 0;
        game_state.player.stateFlags &= ~PLAYER_FLAG_SCENE_CHANGE;
        game_state.player.stateFlags &= ~PLAYER_FLAG_SCRIPTED;
        helper->actor.interruptTime = HELPER_TASK_INTERVAL;
        helper->actor.flags &= ~ACTOR_FLAG_CUE_RUNNING;
        result = 1;
        break;
    }
    return result;
}

FARM_CUE_RUNNER(harvest_helper_scold_cue, {11, 30}, {-1, 0})

/* One tick of a helper's input move script. With no target (task -1) it waits for
 * the walk. On the farm it finds a free tile and walks there; in the greenhouse it
 * runs the input steps. While it runs, its task is saved to be restored later. When
 * it stops it plays action 7 and returns 1. */
static s32 harvest_obj_move_input_tick(FarmHelper *helper) {
    s32 running;
    s32 result;
    s16 column;
    s16 row;
    VECTOR pos;

    running = 1;
    helper->actor.flags |= ACTOR_FLAG_CUE_RUNNING;
    if (helper->actor.taskId == -1) {
        harvest_obj_wait_walk(helper, HARVEST_NEAR_PROXIMITY);
        if (helper->actor.place.location == PLACE_GREENHOUSE) {
            helper->actor.taskId = 0;
        }
    } else if (helper->actor.place.location == PLACE_FARM) {
        helper->actor.flags &= ~HARVEST_WALK_FLAGS;
        if (harvest_obj_input_step(helper, 1) != 0) {
            if (harvest_find_free_tile(&column, &row, 0, 1, 1) != 0) {
                running = 0;
            } else {
                harvest_placement_lookup(helper, 3, &pos, helper->actor.place.location);
                farm_set_destination(&helper->actor, pos);
                helper->actor.taskId = -1;
                helper->actor.flags |= ACTOR_FLAG_BIT28;
            }
        }
    } else if (helper->actor.place.location == PLACE_GREENHOUSE) {
        helper->actor.flags &= ~HARVEST_WALK_FLAGS;
        if (harvest_obj_input_step(helper, 0) != 0) {
            running = 0;
        }
    } else {
        running = 0;
    }
    if (running == 1) {
        result = 0;
        helper->savedTaskNo = helper->actor.taskNo;
        helper->savedTaskId = helper->actor.taskId;
        helper->savedTaskTime = helper->actor.taskTime;
    } else {
        result = 1;
        helper->savedTaskNo = 0;
        helper->actor.flags &= ~ACTOR_FLAG_CUE_RUNNING;
        helper->flags |= HELPER_BIT6;
        harvest_helper_set_action(helper, HELPER_HANDLER_SET_WORK);
        helper->actor.flags |= HARVEST_WALK_FLAGS;
    }
    return result;
}

/* One tick of a helper's action move script. With no target (task -1) it waits for
 * the walk. On the farm it finds a free tile and walks there; in the greenhouse it
 * runs the action steps. While it runs, its task is saved to be restored later.
 * When it stops it plays action 7 and returns 1. */
static s32 harvest_obj_move_action_tick(FarmHelper *helper) {
    s32 running;
    s32 result;
    s16 column;
    s16 row;
    VECTOR pos;

    running = 1;
    helper->actor.flags |= ACTOR_FLAG_CUE_RUNNING;
    if (helper->actor.taskId == -1) {
        harvest_obj_wait_walk(helper, HARVEST_NEAR_PROXIMITY);
        if (helper->actor.place.location == PLACE_GREENHOUSE) {
            helper->actor.taskId = 0;
        }
    } else if (helper->actor.place.location == PLACE_FARM) {
        helper->actor.flags &= ~HARVEST_WALK_FLAGS;
        if (harvest_obj_action_step(helper, 1) != 0) {
            if (harvest_find_free_tile(&column, &row, 0, 0, 1) != 0) {
                running = 0;
            } else {
                harvest_placement_lookup(helper, 3, &pos, helper->actor.place.location);
                farm_set_destination(&helper->actor, pos);
                helper->actor.taskId = -1;
                helper->actor.flags |= ACTOR_FLAG_BIT28;
            }
        }
    } else if (helper->actor.place.location == PLACE_GREENHOUSE) {
        helper->actor.flags &= ~HARVEST_WALK_FLAGS;
        if (harvest_obj_action_step(helper, 0) != 0) {
            running = 0;
        }
    } else {
        running = 0;
    }
    if (running == 1) {
        result = 0;
        helper->savedTaskNo = helper->actor.taskNo;
        helper->savedTaskId = helper->actor.taskId;
        helper->savedTaskTime = helper->actor.taskTime;
    } else {
        result = 1;
        helper->savedTaskNo = 0;
        helper->actor.flags &= ~ACTOR_FLAG_CUE_RUNNING;
        helper->flags |= HELPER_BIT6;
        harvest_helper_set_action(helper, HELPER_HANDLER_SET_WORK);
        helper->actor.flags |= HARVEST_WALK_FLAGS;
    }
    return result;
}

/* One tick of a helper's carry script. Tasks -1 to -4 wait for the walk to their
 * target. Otherwise the place decides: on the farm it picks an animal to carry feed
 * to, in PLACE_5 it hands over and fetches, in PLACE_4 it fetches and claims. While
 * it runs, its task is saved to be restored later. When it stops it plays action 7
 * and returns 1. */
static s32 harvest_obj_carry_tick(FarmHelper *object) {
    FarmHelper *self;
    s32 running;
    s32 result;
    VECTOR pos;

    void harvest_helper_arrive_or_wait(FarmHelper *o, s16 kind, u32 mask) {
        if (o->actor.place.location == kind) {
            o->targetAnimal = SLOT_NONE;
            o->actor.taskId = 0;
            o->actor.flags &= ~mask;
            o->flags |= HELPER_BIT9;
        } else {
            o->actor.flags |= mask;
            harvest_obj_wait_walk(object, HARVEST_NEAR_PROXIMITY);
        }
    }

    self = object;
    running = 1;
    self->actor.flags |= ACTOR_FLAG_CUE_RUNNING;
    if (self->actor.taskId == -1) {
        harvest_helper_arrive_or_wait(self, 5, ACTOR_FLAG_BIT26);
    } else if (self->actor.taskId == -2) {
        harvest_helper_arrive_or_wait(self, 4, ACTOR_FLAG_BIT29);
    } else if (self->actor.taskId == -3) {
        harvest_helper_arrive_or_wait(self, 1, ACTOR_FLAG_BIT26);
    } else if (self->actor.taskId == -4) {
        harvest_helper_arrive_or_wait(self, 1, ACTOR_FLAG_BIT29);
    } else if (self->actor.place.location == PLACE_FARM) {
        if (!(self->flags & HELPER_BIT8)) {
            if (harvest_pick_animal(5, 1) != -1 && game_state.progress.estate.fodder != 0) {
                self->actor.taskId = -1;
                harvest_placement_lookup(object, 5, &pos, self->actor.place.location);
                farm_set_destination(&self->actor, pos);
            } else if (harvest_pick_animal(5, 6) != -1) {
                self->actor.taskId = -1;
                harvest_placement_lookup(object, 5, &pos, self->actor.place.location);
                farm_set_destination(&self->actor, pos);
            } else if (harvest_pick_animal(4, 1) != -1 && game_state.progress.estate.chickenFeed != 0) {
                self->actor.taskId = -2;
                harvest_placement_lookup(object, 4, &pos, self->actor.place.location);
                farm_set_destination(&self->actor, pos);
            } else if (harvest_pick_animal(4, 8) != -1) {
                self->actor.taskId = -2;
                harvest_placement_lookup(object, 4, &pos, self->actor.place.location);
                farm_set_destination(&self->actor, pos);
            } else {
                object->flags |= HELPER_BIT8;
            }
            object->targetAnimal = SLOT_NONE;
            object->flags |= HELPER_BIT9;
        } else {
            self->actor.flags &= ~HARVEST_WALK_FLAGS;
            if (self->flags & HELPER_BIT9) {
                if (harvest_obj_handover_tick(self, 0) != 0) {
                    object->targetAnimal = SLOT_NONE;
                    object->flags &= ~HELPER_BIT9;
                }
            } else if (harvest_obj_claim_tick(self, 0) != 0) {
                running = 0;
            }
        }
    } else if (self->actor.place.location == PLACE_5) {
        self->actor.flags &= ~HARVEST_WALK_FLAGS;
        if (self->flags & HELPER_BIT9) {
            if (harvest_obj_fetch_tick(self, 1) != 0) {
                object->targetAnimal = SLOT_NONE;
                object->flags &= ~HELPER_BIT9;
            }
        } else if (harvest_obj_handover_tick(self, 1) != 0) {
            if (harvest_pick_animal(4, 9) != -1) {
                self->actor.taskId = -3;
                harvest_placement_lookup(object, 5, &pos, self->actor.place.location);
                farm_set_destination(&self->actor, pos);
            } else if (harvest_pick_animal(1, 0xA) != -1) {
                self->actor.taskId = -3;
                object->flags |= HELPER_BIT8;
                harvest_placement_lookup(object, 5, &pos, self->actor.place.location);
                farm_set_destination(&self->actor, pos);
            } else {
                running = 0;
            }
        }
    } else if (self->actor.place.location == PLACE_4) {
        self->actor.flags &= ~HARVEST_WALK_FLAGS;
        if (self->flags & HELPER_BIT9) {
            if (harvest_obj_fetch_tick(self, 0) != 0) {
                object->targetAnimal = SLOT_NONE;
                object->flags &= ~HELPER_BIT9;
            }
        } else if (harvest_obj_claim_tick(self, 0) != 0) {
            if (harvest_pick_animal(1, 0xA) != -1) {
                self->actor.taskId = -4;
                object->flags |= HELPER_BIT8;
                harvest_placement_lookup(object, 4, &pos, self->actor.place.location);
                farm_set_destination(&self->actor, pos);
            } else {
                running = 0;
            }
        }
    } else {
        running = 0;
    }

    if (running == 1) {
        result = 0;
        object->savedTaskNo = self->actor.taskNo;
        object->savedTaskId = self->actor.taskId;
        object->savedTaskTime = self->actor.taskTime;
    } else {
        result = 1;
        object->savedTaskNo = 0;
        self->actor.flags &= ~ACTOR_FLAG_CUE_RUNNING;
        object->flags |= HELPER_BIT6;
        harvest_helper_set_action(object, HELPER_HANDLER_SET_WORK);
        self->actor.flags |= HARVEST_WALK_FLAGS;
        object->flags &= ~(HELPER_BIT8 | HELPER_BIT9);
    }
    return result;
}

/* The steps of the place script. */
enum HelperPlace {
    HELPER_PLACE_ENTER = 0,
    HELPER_PLACE_WAIT_WALK = 1,
    HELPER_PLACE_SETTLE = 2,
};

/* The steps of the walk script. */
enum HelperWalk {
    HELPER_WALK_START = 0,
    HELPER_WALK_WAIT = 1,
    HELPER_WALK_TO_SPOT = 2,
};

/* The steps of the input script. */
enum HelperInput {
    HELPER_INPUT_SEEK = 0,
    HELPER_INPUT_WAIT_WALK = 1,
    HELPER_INPUT_RESOLVE = 2,
    HELPER_INPUT_ITEM_WAIT = 3,
    HELPER_INPUT_TO_PLACE = 4,
    HELPER_INPUT_WAIT_PLACE = 5,
    HELPER_INPUT_RELEASE = 6,
};

/* The steps of the action script. */
enum HelperAction {
    HELPER_ACTION_SEEK = 0,
    HELPER_ACTION_WAIT_WALK = 1,
    HELPER_ACTION_PERFORM = 2,
};

/* The steps of the feed script. */
enum HelperFetch {
    HELPER_FETCH_SEEK = 0,
    HELPER_FETCH_WAIT_SEEK = 1,
    HELPER_FETCH_TAKE = 2,
    HELPER_FETCH_CARRY = 3,
    HELPER_FETCH_WAIT_ANIMAL = 4,
    HELPER_FETCH_FEED = 5,
    HELPER_FETCH_STOCK_BACK = 6,
    HELPER_FETCH_GO_BACK = 7,
    HELPER_FETCH_WAIT_BACK = 8,
};

/* The steps of the hand-over script. */
enum HelperHandover {
    HELPER_HANDOVER_SEEK = 0,
    HELPER_HANDOVER_WAIT_SEEK = 1,
    HELPER_HANDOVER_TAKE = 2,
    HELPER_HANDOVER_GIVE = 3,
    HELPER_HANDOVER_WAIT_GIVE = 4,
    HELPER_HANDOVER_WAIT_BACK = 5,
    HELPER_HANDOVER_RELEASE = 6,
    HELPER_HANDOVER_HOLD = 7,
};

/* The steps of the claim script. */
enum HelperClaim {
    HELPER_CLAIM_SEEK = 0,
    HELPER_CLAIM_WAIT_SEEK = 1,
    HELPER_CLAIM_PICK = 2,
    HELPER_CLAIM_HOLD = 3,
    HELPER_CLAIM_TO_PLACE = 4,
    HELPER_CLAIM_WAIT_PLACE = 5,
    HELPER_CLAIM_RELEASE = 6,
};

/* Put a helper on the farm and start the action its job picks. It walks in, waits
 * for the walk to finish, then settles. Returns 1 once it has settled. */
static s32 harvest_obj_place_tick(FarmHelper *helper) {
    s32 result;
    VECTOR pos;

    result = 0;
    switch (helper->actor.taskId) {
    case HELPER_PLACE_ENTER:
        helper->actor.taskId = HELPER_PLACE_WAIT_WALK;
        helper->actor.facing = FACING_DOWN;
        pos.vx = HELPER_SPOT_X;
        pos.vy = 0;
        pos.vz = 2400;
        helper->actor.flags |= ACTOR_FLAG_CUE_RUNNING;
        farm_set_destination(&helper->actor, pos);
        helper->actor.position.vx = HELPER_SPOT_X;
        helper->actor.position.vy = 0;
        helper->actor.place.location = PLACE_FARM;
        helper->actor.position.vz = HELPER_SPOT_Z_BASE - helper->number * HELPER_SPOT_Z_STEP;
        break;
    case HELPER_PLACE_WAIT_WALK:
        if (harvest_obj_wait_walk(helper, HARVEST_NEAR_PROXIMITY) == 0) {
            break;
        }
        helper->actor.taskId = HELPER_PLACE_SETTLE;
    case HELPER_PLACE_SETTLE:
        helper->actor.flags &= ~ACTOR_FLAG_CUE_RUNNING;
        if (helper->flags & HELPER_HARVEST) {
            harvest_helper_set_work_state(helper);
            harvest_helper_set_action(helper, HELPER_HANDLER_HARVEST);
        } else if (helper->flags & HELPER_WATER) {
            harvest_helper_set_work_state(helper);
            harvest_helper_set_action(helper, HELPER_HANDLER_WATER);
        } else if (helper->flags & HELPER_ANIMALS) {
            helper->actor.facing = FACING_RIGHT;
            harvest_helper_set_work_state(helper);
            harvest_helper_set_action(helper, HELPER_HANDLER_ANIMALS);
        } else {
            harvest_helper_set_work_state(helper);
            harvest_helper_set_action(helper, HELPER_HANDLER_SET_WORK);
        }
        result = 1;
        break;
    }
    return result;
}

/* One tick of a helper's walk script: wait for the walk, then walk to a waiting
 * spot on the farm, or to the place for its kind in PLACE_4, PLACE_5 and the
 * greenhouse. Returns 1 once it has arrived. */
static s32 harvest_obj_walk_tick(FarmHelper *helper) {
    s32 done;
    VECTOR pos;

    done = 0;
    helper->actor.flags |= (ACTOR_FLAG_CUE_RUNNING | HARVEST_WALK_FLAGS);
    if (helper->actor.taskId == -1) {
        harvest_obj_wait_walk(helper, HARVEST_NEAR_PROXIMITY);
        if (helper->actor.place.location == PLACE_FARM) {
            helper->actor.taskId = HELPER_WALK_START;
        }
    } else if (helper->actor.place.location == PLACE_FARM) {
        switch (helper->actor.taskId) {
        case HELPER_WALK_START:
            helper->actor.flags |= ACTOR_FLAG_CUE_RUNNING;
            helper->actor.taskId = HELPER_WALK_WAIT;
            pos.vx = HELPER_SPOT_X;
            pos.vy = 0;
            pos.vz = 4000;
            farm_set_destination(&helper->actor, pos);
            break;
        case HELPER_WALK_WAIT:
            if (harvest_obj_wait_walk(helper, HARVEST_FAR_PROXIMITY) == 0) {
                break;
            }
            helper->actor.taskId = HELPER_WALK_TO_SPOT;
            /* fall through */
        case HELPER_WALK_TO_SPOT:
            helper->actor.action = ACTOR_ACTION_MOVING;
            helper->actor.facing = FACING_UP;
            farm_vector_adjust(2, &helper->actor.speed, 24);
            if (helper->actor.position.vz >= 6701) {
                harvest_helper_place_at_spot(helper);
                helper->actor.flags &= ~ACTOR_FLAG_CUE_RUNNING;
                harvest_helper_set_action(helper, 0);
                done = 1;
            }
            break;
        }
    } else if (helper->actor.place.location == PLACE_4) {
        harvest_placement_lookup(helper, 4, &pos, helper->actor.place.location);
        farm_set_destination(&helper->actor, pos);
        helper->actor.taskId = -1;
    } else if (helper->actor.place.location == PLACE_5) {
        harvest_placement_lookup(helper, 5, &pos, helper->actor.place.location);
        farm_set_destination(&helper->actor, pos);
        helper->actor.taskId = -1;
    } else if (helper->actor.place.location == PLACE_GREENHOUSE) {
        harvest_placement_lookup(helper, 3, &pos, helper->actor.place.location);
        farm_set_destination(&helper->actor, pos);
        helper->actor.taskId = -1;
    }
    return done;
}

/* Advance a helper through its input state machine: seek a free tile, wait for the walk,
 * take the item the tile gives, carry it to the place for its kind and release it. */
static s32 harvest_obj_input_step(FarmHelper *helper, s32 mode) {
    s16 state;
    s32 status;
    VECTOR result;

    state = helper->actor.taskId;
    switch (state) {
    case HELPER_INPUT_SEEK:
        if (harvest_obj_walk_to_tile(helper, mode, 1) != 0) {
            helper->actor.taskId = HELPER_INPUT_SEEK;
            return 1;
        }
        helper->actor.taskId = HELPER_INPUT_WAIT_WALK;
        return 0;
    case HELPER_INPUT_WAIT_WALK:
        if (harvest_obj_wait_walk(helper, HARVEST_NEAR_PROXIMITY) != 0) helper->actor.taskId = HELPER_INPUT_RESOLVE;
        return 0;
    case HELPER_INPUT_RESOLVE:
        status = harvest_obj_resolve_result(helper);
        helper->item = status;
        if (helper->item == ITEM_NOTHING)
            helper->actor.taskId = HELPER_INPUT_SEEK;
        else
            helper->actor.taskId = HELPER_INPUT_ITEM_WAIT;
        return 0;
    case HELPER_INPUT_ITEM_WAIT:
        status = harvest_helper_item_wait(helper, helper->item);
        if (status != 0) helper->actor.taskId = HELPER_INPUT_TO_PLACE;
        return 0;
    case HELPER_INPUT_TO_PLACE:
        status = harvest_placement_lookup(helper, 2, &result, helper->actor.place.location);
        if (status == 1) {
            farm_set_destination(&helper->actor, result);
        }
        helper->actor.taskId = HELPER_INPUT_WAIT_PLACE;
        return 0;
    case HELPER_INPUT_WAIT_PLACE:
        if (harvest_obj_wait_walk(helper, HARVEST_FAR_PROXIMITY) != 0) helper->actor.taskId = HELPER_INPUT_RELEASE;
        return 0;
    case HELPER_INPUT_RELEASE:
        if (harvest_helper_release_item(helper) != 0) {
            helper->actor.taskId = HELPER_INPUT_SEEK;
        }
        return 0;
    default:
        return 0;
    }
}

/* Walk a helper to a free tile, wait for it there, then work the tile in front of it
 * (action 8) and open its gate. */
static s32 harvest_obj_action_step(FarmHelper *helper, s32 mode) {
    switch (helper->actor.taskId) {
    case HELPER_ACTION_SEEK:
        if (harvest_obj_walk_to_tile(helper, mode, 0) != 0) {
            helper->actor.taskId = HELPER_ACTION_SEEK;
            return 1;
        }
        helper->actor.taskId = HELPER_ACTION_WAIT_WALK;
        break;
    case HELPER_ACTION_WAIT_WALK:
        if (harvest_obj_wait_walk(helper, HARVEST_NEAR_PROXIMITY) != 0) {
            helper->actor.taskId = HELPER_ACTION_PERFORM;
        }
        break;
    case HELPER_ACTION_PERFORM:
        if (harvest_actor_wait_action(&helper->actor, 8, 0) != 0) {
            harvest_obj_resolve_place(&helper->actor, mode);
            helper->actor.taskId = HELPER_ACTION_SEEK;
        }
        break;
    default:
        return 0;
    }
    return 0;
}

/* One tick of the fetch script: walk to an animal whose feed bin is empty, take the
 * feed from the estate (fodder for mode 1, chicken feed otherwise), carry it to an
 * animal and walk back. Returns 1 when the script ends, 0 while it runs. */
static s32 harvest_obj_fetch_tick(FarmHelper *helper, s32 mode)
{
    VECTOR pos;
    s32 slot;
    s32 placed;

    switch (helper->actor.taskId) {
    case HELPER_FETCH_SEEK:
        if (harvest_pick_animal(helper->actor.place.location, 1) == SLOT_NONE) {
            helper->actor.taskId = HELPER_FETCH_SEEK;
            return 1;
        }
        harvest_placement_lookup(helper, mode == 0, &pos, helper->actor.place.location);
        farm_set_destination(&helper->actor, pos);
        helper->actor.taskId = HELPER_FETCH_WAIT_SEEK;
        helper->flags |= HELPER_BIT9;
        break;
    case HELPER_FETCH_WAIT_SEEK:
        if (harvest_obj_wait_walk(helper, HARVEST_NEAR_PROXIMITY) != 0) {
            helper->actor.taskId = HELPER_FETCH_TAKE;
        }
        break;
    case HELPER_FETCH_TAKE:
        if (mode == 1) {
            if (estate_fodder_take(1) == 0) {
                helper->actor.taskId = HELPER_FETCH_SEEK;
                return 1;
            }
            helper->item = ITEM_FODDER;
        } else {
            if (estate_chicken_feed_take(1) == 0) {
                helper->actor.taskId = HELPER_FETCH_SEEK;
                return 1;
            }
            helper->item = ITEM_CHICKEN_FEED_06B;
        }
        helper->actor.taskId = HELPER_FETCH_CARRY;
        /* fall through */
    case HELPER_FETCH_CARRY:
        if (harvest_helper_item_wait(helper, helper->item) == 0) {
            return 0;
        }
        slot = harvest_pick_animal(helper->actor.place.location, 1);
        if (slot == SLOT_NONE) {
            helper->actor.taskId = HELPER_FETCH_STOCK_BACK;
            break;
        }
        helper->targetAnimal = slot;
        if (mode == 1) {
            harvest_slot_position(&pos, slot);
        } else {
            harvest_helper_spot_position(&pos, slot);
        }
        farm_set_destination(&helper->actor, pos);
        helper->actor.taskId = HELPER_FETCH_WAIT_ANIMAL;
        break;
    case HELPER_FETCH_WAIT_ANIMAL:
        if (harvest_obj_wait_walk(helper, HARVEST_NEAR_PROXIMITY) != 0) {
            helper->actor.taskId = HELPER_FETCH_FEED;
        }
        break;
    case HELPER_FETCH_FEED:
        placed = farm_feed_bin_full(helper->targetAnimal, mode);
        if (placed == 1) {
            slot = harvest_pick_animal(helper->actor.place.location, 1);
            if (slot == SLOT_NONE) {
                helper->actor.taskId = HELPER_FETCH_GO_BACK;
                break;
            }
            helper->targetAnimal = slot;
            if (mode == placed) {
                harvest_slot_position(&pos, slot);
            } else {
                harvest_helper_spot_position(&pos, slot);
            }
            farm_set_destination(&helper->actor, pos);
            helper->actor.taskId = HELPER_FETCH_WAIT_ANIMAL;
        } else {
            farm_feed_bin_set(helper->targetAnimal, mode);
            harvest_helper_clear_item(helper);
            helper->actor.taskId = HELPER_FETCH_SEEK;
        }
        break;
    case HELPER_FETCH_GO_BACK:
        if (mode == 1) {
            harvest_placement_lookup(helper, 0, &pos, helper->actor.place.location);
        } else {
            harvest_placement_lookup(helper, 1, &pos, helper->actor.place.location);
        }
        farm_set_destination(&helper->actor, pos);
        helper->actor.taskId = HELPER_FETCH_WAIT_BACK;
        /* fall through */
    case HELPER_FETCH_WAIT_BACK:
        if (harvest_obj_wait_walk(helper, HARVEST_NEAR_PROXIMITY) == 0) {
            return 0;
        }
        helper->actor.taskId = HELPER_FETCH_STOCK_BACK;
        break;
    case HELPER_FETCH_STOCK_BACK:
        if (mode == 1) {
            estate_fodder_add(0, 1);
        } else {
            estate_chicken_feed_add(0, 1);
        }
        harvest_helper_clear_item(helper);
        helper->actor.taskId = HELPER_FETCH_SEEK;
        return 1;
    }
    return 0;
}

/* One tick of the hand-over script: find a livestock slot, walk to it, take the
 * item, hand it over, then fetch the object's own item and walk back. Returns 1
 * when no slot is free, 0 otherwise. */
static s32 harvest_obj_handover_tick(FarmHelper *helper, s32 mode) {
    VECTOR pos;
    FarmLivestock *item;
    FarmLivestock *target;
    s32 slot = SLOT_NONE;

    if (helper->targetAnimal != slot) {
        target = &game_state.animals.livestock[helper->targetAnimal];
        item = target;
    } else {
        target = 0;
        item = 0;
    }
    switch (helper->actor.taskId) {
    case HELPER_HANDOVER_SEEK:
        slot = harvest_pick_animal(helper->actor.place.location, 6);
        if (slot == SLOT_NONE) {
            helper->actor.taskId = HELPER_HANDOVER_SEEK;
            helper->targetAnimal = slot;
            return 1;
        }
        helper->targetAnimal = slot;
        farm_set_destination(&helper->actor, game_state.animals.livestock[(s16)slot].actor.position);
        helper->actor.taskId = HELPER_HANDOVER_WAIT_SEEK;
        break;
    case HELPER_HANDOVER_WAIT_SEEK:
        if (harvest_obj_wait_walk(helper, HARVEST_NEAR_PROXIMITY) != 0) {
            helper->actor.taskId = HELPER_HANDOVER_TAKE;
        } else if (item->actor.place.location == helper->actor.place.location) {
            farm_set_destination(&helper->actor, item->actor.position);
            break;
        } else {
            helper->actor.taskId = HELPER_HANDOVER_HOLD;
            break;
        }
        /* fall through */
    case HELPER_HANDOVER_TAKE:
        if (harvest_actor_wait_action(&helper->actor, 9, 0x3C) != 0) {
            if ((item->actor.flags & ACTOR_FLAG_BIT16) == 0) {
                livestock_action_dispatch(target, 6);
            } else {
                livestock_action_dispatch_b(target, 6);
            }
            farm_set_destination(&helper->actor, game_state.animals.livestock[helper->targetAnimal].actor.position);
            helper->actor.taskId = HELPER_HANDOVER_WAIT_GIVE;
        }
        break;
    case HELPER_HANDOVER_WAIT_GIVE:
        if (harvest_obj_wait_walk(helper, HARVEST_NEAR_PROXIMITY) != 0) {
            helper->actor.taskId = HELPER_HANDOVER_GIVE;
        } else if (item->actor.place.location == helper->actor.place.location) {
            farm_set_destination(&helper->actor, item->actor.position);
            break;
        } else {
            helper->actor.taskId = HELPER_HANDOVER_HOLD;
            break;
        }
        /* fall through */
    case HELPER_HANDOVER_GIVE:
        if (harvest_actor_wait_action(&helper->actor, 10, 0x3C) == 0) {
            return 0;
        }
        target->helperNumber = helper->number;
        if ((item->actor.flags & ACTOR_FLAG_BIT16) == 0) {
            livestock_action_dispatch(target, 7);
        } else {
            livestock_action_dispatch_b(target, 8);
        }
        if (helper->item == ITEM_NOTHING) {
            helper->actor.taskId = HELPER_HANDOVER_SEEK;
            break;
        }
        helper->actor.taskId = HELPER_HANDOVER_HOLD;
        /* fall through */
    case HELPER_HANDOVER_HOLD:
        if (harvest_helper_item_wait(helper, helper->item) != 0) {
            harvest_placement_lookup(helper, 2, &pos, helper->actor.place.location);
            farm_set_destination(&helper->actor, pos);
            helper->actor.taskId = HELPER_HANDOVER_WAIT_BACK;
        }
        break;
    case HELPER_HANDOVER_WAIT_BACK:
        if (harvest_obj_wait_walk(helper, HARVEST_FAR_PROXIMITY) != 0) {
            helper->actor.taskId = HELPER_HANDOVER_RELEASE;
        }
        break;
    case HELPER_HANDOVER_RELEASE:
        if (harvest_helper_release_item(helper) == 0) {
            return 0;
        }
        helper->actor.taskId = HELPER_HANDOVER_SEEK;
        break;
    }
    return 0;
}

/* Advance a helper through its claim-a-record state machine: find a record, wait,
 * take it, act on it, move, wait and release it. Returns 1 when no record was
 * found. */
static s32 harvest_obj_claim_tick(FarmHelper *helper, s32 mode) {
    VECTOR target;
    FarmStoredItem found;
    FarmStoredItem taken;
    s32 slot;
    s32 status;

    switch (helper->actor.taskId) {
    case HELPER_CLAIM_SEEK:
        slot = (s16)harvest_pick_animal(helper->actor.place.location, 8);
        helper->targetAnimal = slot;
        if (slot == SLOT_NONE) {
            helper->actor.taskId = HELPER_CLAIM_SEEK;
            return 1;
        }
        status = estate_result_select(ITEM_GOOD_EGG, &target, &found.id, slot);
        if (status != 1) {
            return 0;
        }
        if (found.id != helper->actor.place.location) {
            return 0;
        }
        farm_set_destination(&helper->actor, target);
        helper->actor.taskId = status;
        break;
    case HELPER_CLAIM_WAIT_SEEK:
        if (harvest_obj_wait_walk(helper, HARVEST_NEAR_PROXIMITY) != 0) {
            helper->actor.taskId = HELPER_CLAIM_PICK;
            break;
        }
        if (game_state.runs.records[16][helper->targetAnimal].recordFlags & ESTATE_RECORD_ENABLED) {
            return 0;
        }
        helper->actor.taskId = HELPER_CLAIM_SEEK;
        return 0;
    case HELPER_CLAIM_PICK:
        if (estate_field_pickup(&game_state.runs.records[16][helper->targetAnimal], &taken) != 1) {
            helper->actor.taskId = HELPER_CLAIM_SEEK;
            break;
        }
        helper->actor.taskId = HELPER_CLAIM_HOLD;
        helper->item = taken.id;
        break;
    case HELPER_CLAIM_HOLD:
        if (harvest_helper_item_wait(helper, helper->item) != 0) {
            helper->actor.taskId = HELPER_CLAIM_TO_PLACE;
        }
        break;
    case HELPER_CLAIM_TO_PLACE:
        if (harvest_placement_lookup(helper, 2, &target, helper->actor.place.location) == 1) {
            farm_set_destination(&helper->actor, target);
        }
        helper->actor.taskId = HELPER_CLAIM_WAIT_PLACE;
        break;
    case HELPER_CLAIM_WAIT_PLACE:
        if (harvest_obj_wait_walk(helper, HARVEST_FAR_PROXIMITY) != 0) {
            helper->actor.taskId = HELPER_CLAIM_RELEASE;
        }
        break;
    case HELPER_CLAIM_RELEASE:
        if (harvest_helper_release_item(helper) == 0) {
            return 0;
        }
        helper->actor.taskId = HELPER_CLAIM_SEEK;
        break;
    }
    return 0;
}

/* Ship the helper's item, sound the shipment and set the shipped flag when this area is
 * the helper's, then empty its hands. */
static s32 harvest_helper_release_item(FarmHelper *helper) {
    FarmLoadArea *g;
    estate_shipment_add(helper->item);
    g = &farm_area;
    if (g->mode.place.location == helper->actor.place.location) {
        sound_play(0x13);
        event_flag_set(EVENT_FLAG_ITEM_SHIPPED, 1);
    }
    helper->item = ITEM_NOTHING;
    helper->itemSprite.source = 0;
    return 1;
}

static FarmPlacement harvest_barn_placements[2] = {
    {1700, 0, 2350, PLACE_5}, {-1500, 0, 2350, PLACE_5},
};

static FarmPlacement harvest_coop_placements[2] = {
    {-1450, 0, -2200, PLACE_4}, {-1450, 0, 300, PLACE_4},
};

static FarmPlacement harvest_placements_kind2[3] = {
    {1600, 0, -2150, PLACE_4}, {-5000, 0, 1450, PLACE_FARM}, {-200, 0, -2500, PLACE_5},
};

static FarmPlacement harvest_placements_kind3[2] = {
    {-6800, 0, 1100, PLACE_FARM}, {2700, 0, 0, PLACE_GREENHOUSE},
};

static FarmPlacement harvest_placements_kind4[2] = {
    {-6000, 0, -6900, PLACE_FARM}, {2200, 0, -1200, PLACE_4},
};

static FarmPlacement harvest_placements_kind5[2] = {
    {1100, 0, 3300, PLACE_FARM}, {1500, 0, -3100, PLACE_5},
};

/* Look up the placement for `key` in the kind's candidate list and copy its position to
 * `output`. The barn and coop lists depend on the estate's barn and coop levels. */
static s32 harvest_placement_lookup(FarmHelper *helper, s16 kind, VECTOR *output, s16 key) {
    FarmPlacement *records;
    s32 count;
    s32 i;

    harvest_barn_placements[0].z = 2350;
    harvest_placements_kind2[2].x = -200;
    harvest_placements_kind2[2].z = -2500;
    count = 0;
    records = 0;
    switch (kind) {
    case 0:
        count = 1;
        records = harvest_barn_placements;
        if (game_state.progress.estate.barnLevel != 0) {
            if (helper->actor.position.vx < 0) {
                records++;
            }
        } else {
            records->z -= 150;
        }
        break;
    case 1:
        count = 1;
        records = harvest_coop_placements;
        if (game_state.progress.estate.coopLevel != 0 && helper->actor.position.vz > 0) {
            records++;
        }
        break;
    case 2:
        count = 3;
        if (game_state.progress.estate.coopLevel == 0) {
            harvest_placements_kind2[2].x = 550;
            harvest_placements_kind2[2].z = -2400;
        }
        records = harvest_placements_kind2;
        break;
    case 3:
        count = 2;
        records = harvest_placements_kind3;
        break;
    case 4:
        count = 2;
        records = harvest_placements_kind4;
        break;
    case 5:
        count = 2;
        records = harvest_placements_kind5;
        break;
    }
    for (i = 0; i < count; i++) {
        if (key == records->key) {
            output->vx = records->x;
            output->vy = records->y;
            output->vz = records->z;
            return 1;
        }
        records++;
    }
    return 0;
}

/* Return the growth stage of the field tile in front of a helper, or 0 when there is none. */
static s32 harvest_obj_resolve_result(FarmHelper *helper) {
    VECTOR values;
    void *tile;
    s32 occupants;
    s32 result;
    farm_vector_adjust(helper->actor.facing, &values, HARVEST_FRONT_DISTANCE);
    result = 0;
    helper->itemSprite.source = 0;
    occupants = 0;
    values.vx += helper->actor.position.vx;
    values.vz += helper->actor.position.vz;
    if (farm_callback_count(&values, helper->actor.place, 0, 0, &tile, &occupants, 0, OCCUPANT_CATTLE | OCCUPANT_SHEEP | OCCUPANT_TILE | OCCUPANT_ESTATE_RECORD) == 0)
        occupants = 0;
    if (occupants & OCCUPANT_TILE) result = field_tile_item_stage(tile);
    return result;
}

/* Turn the spot in front of a helper into a field or greenhouse cell and open its gate. */
static void harvest_obj_resolve_place(FarmActor *actor, s32 mode) {
    FarmWorldCell values;
    s32 result;
    s32 selectedMode;
    CellFromWorldHarvest callback;

    if (mode == 1) {
        selectedMode = 0;
        callback = field_world_to_cell;
    } else {
        selectedMode = 1;
        callback = greenhouse_world_to_cell;
    }
    farm_vector_adjust(actor->facing, &values.position, HARVEST_FRONT_DISTANCE);
    values.position.vx += actor->position.vx;
    values.position.vz += actor->position.vz;
    result = callback(&values.position, &values.column, &values.row);
    if (result == 0) {
        field_gate_open(selectedMode, values.column, values.row, 0);
    }
}

/* Hold the actor on action `x` until its task time reaches `y`, then return 1 once it has
 * left this area or its sprite flags 0x300 are set. A new action restarts the count. */
static u32 harvest_actor_wait_action(FarmActor *p, s32 raw_x, s32 raw_y) {
    s16 x = raw_x;
    s16 y = raw_y;

    if (p->action == x) {
        if (p->taskTime >= y) {
            if (p->place.location != farm_area.mode.place.location || (p->sprite.flags & 0x300)) return 1;
        } else {
            p->taskTime = p->taskTime + 1;
        }
    } else {
        p->action = x;
        p->taskTime = 0;
    }
    return 0;
}

/* Wait on action 5 until its time is up. The item is not checked. */
static s32 harvest_helper_item_wait(FarmHelper *helper, s16 item) {
    return harvest_actor_wait_action(&helper->actor, 5, 0) > 0;
}

/* Set the walk proximity and run the helper's work state. Returns 1 once its walk has
 * ended (the approach flag is clear), after zeroing its speed. */
static s32 harvest_obj_wait_walk(FarmHelper *helper, s32 proximity) {
    u32 flags = helper->actor.flags;
    farm_set_proximity(proximity);
    harvest_helper_set_work_state(helper);
    if (flags & ACTOR_FLAG_CUE_RUNNING) {
        helper->actor.flags |= ACTOR_FLAG_CUE_RUNNING;
    }
    if (!(helper->actor.flags & ACTOR_FLAG_APPROACHING_PLAYER)) {
        farm_vector_adjust(4, &helper->actor.speed, 0);
        return 1;
    }
    return 0;
}

/* Walk a helper to a free field or greenhouse tile found in the given direction. */
static s32 harvest_obj_walk_to_tile(FarmHelper *helper, s32 mode, s32 direction) {
    VECTOR result;
    s16 column;
    s16 row;
    s32 greenhouse;
    s32 status;
    greenhouse = mode != 1;
    status = harvest_find_free_tile(&column, &row, mode, direction, 1);
    if (status != 0) {
        return 1;
    }
    field_result_coords(column, row, &result, greenhouse);
    farm_set_destination(&helper->actor, result);
    return 0;
}

/* Scan the field tiles (or the 7x7 greenhouse, when it is built) from a random start for
 * the first one the chosen test accepts, and report its column and row. Returns 0 on a
 * hit, 1 if no tile qualified. */
s32 harvest_find_free_tile(s16 *column_out, s16 *row_out, s32 field, s32 direction, s32 alternate) {
    FarmCheck check;
    s32 start;
    s32 index;
    s32 slot_count;
    s32 columns;
    FarmFieldTile *slots;

    if (field != 0) {
        slots = game_state.fieldTiles[0].rows;
        slot_count = 1125;
        columns = 45;
    } else {
        if (game_state.progress.estate.greenhouseLevel == 0) {
            return 1;
        }
        slots = game_state.slots7x7[0].rows;
        slot_count = 49;
        columns = 7;
    }

    if (direction != 0) {
        check = harvest_tile_match_category;
    } else if (alternate != 0) {
        check = harvest_tile_is_dry_soil;
    } else {
        check = harvest_tile_is_plant;
    }

    index = rand_range(slot_count);
    start = index;
    do {
        if (check(&slots[index]) != 0) {
            *column_out = index % columns;
            *row_out = index / columns;
            return 0;
        }
        index++;
        if (index >= slot_count) {
            index = 0;
        }
    } while (index != start);
    return 1;
}

/* Check whether a usable tile holds a crop at its grown stage. Seed items and
 * the magic red plants (ITEM_BLUE_MAGIC_RED_PLANT, ITEM_RED_MAGIC_RED_PLANT) never match. */
static s32 harvest_tile_match_category(FarmFieldTile *tile) {
    if (harvest_tile_usable(tile) == 0) return 0;
    switch (tile->value) {
    case ITEM_MOONDROP_SEEDS ... ITEM_ORANGE_CUP_SEEDS:
    case ITEM_BLUE_MAGIC_RED_PLANT ... ITEM_RED_MAGIC_RED_PLANT:
        break;
    default:
        if (tile->kind == game_state.cropTable[game_state.itemInfo[tile->value].crop].grownStage) return 1;
        break;
    }
    return 0;
}

/* Check whether a usable tile is tilled or sown soil. */
static s32 harvest_tile_is_dry_soil(FarmFieldTile *tile) {
    s32 result;
    s32 flag;
    if (harvest_tile_usable(tile)) {
        u16 ground = tile->ground;
        flag = 0;
        if (ground == ITEM_TILLED_SOIL)
            flag = 1;
        else if (ground == ITEM_SOWN_SOIL)
            flag = 1;
        result = flag;
    } else
        result = 0;
    return result;
}

/* Check whether a usable tile's ground is any of the four soil kinds, watered or not. */
static s32 harvest_tile_is_plant(FarmFieldTile *tile) {
    s32 result;
    if (harvest_tile_usable(tile) != 0) {
        result = 0;
        if (tile->ground == ITEM_TILLED_SOIL)
            result = 1;
        else if (tile->ground == ITEM_SOWN_SOIL)
            result = 1;
        else if (tile->ground == ITEM_WATERED_SOIL)
            result = 1;
        else if (tile->ground == ITEM_WATERED_SOWN_SOIL)
            result = 1;
        return result;
    }
    return 0;
}

/* A tile is usable when it holds a seed, other than item 0x36. */
static s32 harvest_tile_usable(FarmFieldTile *tile) {
    s32 result;
    if (item_is_seed(tile->value) != 0) {
        result = (tile->value != 0x36);
    } else {
        result = 0;
    }
    return result;
}

/* Pick at random one animal in area `mode` that the request `flags` allows. Mode 5 and
 * requests with flag 2 or 4 pick barn livestock; mode 4 and the rest pick coop chickens.
 * Flag 8 accepts an index whose egg the estate reports in area `mode`; flag 1 one with an
 * empty feed bin (barn in mode 5, coop in mode 4); flag 2 one that is not busy; flag 4 a
 * cow or sheep that is ready: seven days after shearing for a sheep, otherwise one fed
 * yesterday and not cued today. With flags exactly 1 the two barn slot animals hand out
 * BARN_SLOT_BIN_0 and BARN_SLOT_BIN_1 instead of their index. An animal a helper is
 * already heading for is skipped. Returns the chosen index, or -1 when none qualifies. */
static s32 harvest_pick_animal(s16 mode, u16 flags) {
    u8 candidates[MAX_LIVESTOCK];
    VECTOR position;
    s16 area;
    s32 count;
    s32 chickens;
    s32 limit;
    s32 i;
    s32 j;
    s32 eligible;
    FarmLivestock *animal = game_state.animals.livestock;

    memset(candidates, 0xFF, sizeof(candidates));
    count = 0;
    if (mode == 5) {
        chickens = 0;
        limit = MAX_LIVESTOCK;
    } else if (mode == 4) {
        chickens = 1;
        limit = MAX_CHICKENS;
    } else if (flags & 6) {
        chickens = 0;
        limit = MAX_LIVESTOCK;
    } else {
        chickens = 1;
        limit = MAX_CHICKENS;
    }
    for (i = 0; i < limit; i++) {
        eligible = 0;
        if ((flags & 0x8) && i < MAX_CHICKENS && estate_result_select(ITEM_NORMAL_EGG, &position, &area, i) == 1) {
            eligible = area == mode;
        }
        if (!chickens) {
            animal = &game_state.animals.livestock[i];

            if (animal->actor.place.location == mode && animal->actor.kind != 0
                && !(animal->actor.flags & (ACTOR_FLAG_IN_EVENT | ACTOR_FLAG_HELD))) {
                if ((flags & 1) && mode == 5 && game_state.progress.estate.barnFeedBins[i] == 0) {
                    eligible = 1;
                }
                if ((flags & 2) && !(animal->actor.flags & ACTOR_FLAG_BIT2)) {
                    eligible = 1;
                }
                if ((flags & 4) && (animal->actor.kind == ITEM_COW || animal->actor.kind == ITEM_SHEEP)) {
                    if (animal->actor.flags & ACTOR_FLAG_BIT16) {
                        if (animal->daysSinceShearing >= 7) {
                            eligible = 1;
                        }
                    } else if (!(animal->actor.flags & ACTOR_FLAG_COW_CUE_TODAY)) {
                        if (animal->actor.flags & ACTOR_FLAG_FED_YESTERDAY) {
                            eligible = 1;
                        }
                    }
                }
            }
        } else {
            animal = (FarmLivestock *)&game_state.animals.chickens[i];

            if (animal->actor.place.location == mode && animal->actor.kind != 0
                && animal->actor.kind != ITEM_CHICK && (flags & 1) && mode == 4
                && game_state.progress.estate.coopFeedBins[i] == 0) {
                eligible = 1;
            }
        }
        for (j = 0; j < HELPER_COUNT; j++) {
            if (game_state.helpers[j].actor.taskNo == HELPER_HANDLER_ANIMALS && game_state.helpers[j].actor.place.location == mode
                && game_state.helpers[j].targetAnimal == i
                && !(((flags & 1) + ((game_state.helpers[j].flags >> 9) & 1)) & 1)) {
                eligible = 0;
            }
        }
        if (eligible) {
            if (flags == 1) {
                FarmState *farm = &game_state;

                if (farm->animals.slots[0].animal == i) {
                    if (farm->progress.estate.barnFeedBins[BARN_SLOT_BIN_0] != 0) {
                        continue;
                    }
                    candidates[count++] = BARN_SLOT_BIN_0;
                } else if (farm->animals.slots[1].animal == i) {
                    if (farm->progress.estate.barnFeedBins[BARN_SLOT_BIN_1] != 0) {
                        continue;
                    }
                    candidates[count++] = BARN_SLOT_BIN_1;
                } else {
                    candidates[count++] = i;
                }
            } else {
                candidates[count++] = i;
            }
        }
    }
    if (count == 0) {
        return -1;
    }
    return candidates[rand_range((u16)count)];
}

static VECTOR harvest_slot_rows[6] = {
    {2080, 0, -1268, 0},
    {1160, 0, -1268, 0},
    {-1100, 0, -1268, 0},
    {-2080, 0, -1268, 0},
    {2500, 0, -2450, 0},
    {-2150, 0, -2450, 0},
};

/* Look up the position of slot `value`: the row is value / 5 and the step value % 5
 * pushes it HARVEST_SLOT_STEP units along z. The two barn feed bins, BARN_SLOT_BIN_0 and
 * BARN_SLOT_BIN_1, sit at the last two rows, unshifted. */
static void harvest_slot_position(VECTOR *out, s16 value) {
    s16 row;
    s16 step;

    if (value == BARN_SLOT_BIN_0) {
        row = 4;
        step = 0;
    } else if (value == BARN_SLOT_BIN_1) {
        row = 5;
        step = 0;
    } else {
        row = value / 5;
        step = value % 5;
    }
    out->vx = harvest_slot_rows[row].vx;
    out->vy = harvest_slot_rows[row].vy;
    out->vz = harvest_slot_rows[row].vz + step * HARVEST_SLOT_STEP;
}

/* The base spot of each helper row. */
static VECTOR harvest_spot_bases[2] = {{-760, 0, -410, 0}, {-750, 0, 1990, 0}};

/* The work spot `in`: five spots to a row, HARVEST_SPOT_STEP apart along x, from the row's base. */
static void harvest_helper_spot_position(VECTOR *out, s16 in) {
    s32 q = in / 5;
    s32 r = in % 5;
    out->vx = harvest_spot_bases[q].vx + r * HARVEST_SPOT_STEP;
    out->vy = harvest_spot_bases[q].vy;
    out->vz = harvest_spot_bases[q].vz;
}

static void harvest_helper_clear_item(FarmHelper *helper) {
    helper->item = ITEM_NOTHING;
    helper->itemSprite.source = 0;
}

static HarvestSpot harvest_helper_spots[7] = {
    {-937, 0, -64, 1}, {-404, 0, -802, 3}, {1061, 0, -260, 1}, {-240, 0, -146, 0},
    {598, 0, -1127, 2}, {487, 0, 519, 0}, {-128, 0, 642, 2},
};

/* Put the helper at its spot in PLACE_28. */
static void harvest_helper_place_at_spot(FarmHelper *helper) {
    helper->actor.position.vx = harvest_helper_spots[helper->number].x;
    helper->actor.position.vy = harvest_helper_spots[helper->number].y;
    helper->actor.position.vz = harvest_helper_spots[helper->number].z;
    helper->actor.facing = harvest_helper_spots[helper->number].facing;
    helper->actor.place.location = PLACE_28;
}

/* An item art pack and the function that maps an item to its category in that pack. */
typedef struct CategoryMap {
    s32 list;
    s32 (*category)(s16 item);
} CategoryMap;

typedef struct CategoryMaps {
    CategoryMap map[4];
} CategoryMaps;

static s16 harvest_row_bonuses[7] = {
    0x0564, 0x067C, 0x0694, 0x0634, 0x061C, 0x064C, 0x0664};

/* Work out a helper's harvest values. out1 is always 0. out2 is a base amount chosen
 * by the helper's place and work, plus its row bonus, and one more when
 * HELPER_BIT3 is set. */
static void harvest_result_values(FarmHelper *helper, s32 *out1, s32 *out2) {
    s16 level;
    u16 flags;

    level = 0;
    if (helper->actor.place.location == PLACE_28) {
        if (HARVEST_IS_WINTER_DAY()) {
            *out1 = 0;
            *out2 = 2;
        } else if (game_state.progress.clock.date.season == SEASON_SPRING) {
            *out1 = 0;
            *out2 = 2;
        } else {
            *out1 = 0;
            *out2 = 0;
        }
    } else {
        flags = helper->flags;
        if (flags & HELPER_BIT6) {
            switch (helper->actor.taskNo) {
            case HELPER_HANDLER_CUE_2:
                *out1 = 0;
                *out2 = 10;
                helper->actor.interruptTime = HELPER_TASK_INTERVAL;
                helper->actor.action = ACTOR_ACTION_REST;
                helper->actor.flags &= ~ACTOR_FLAG_CUE_RUNNING;
                break;
            case HELPER_HANDLER_CUE_3:
                *out1 = 0;
                *out2 = 12;
                break;
            case HELPER_HANDLER_CUE_4:
            case HELPER_HANDLER_SET_WORK:
                *out1 = 0;
                *out2 = 8;
                break;
            default:
                *out1 = 0;
                *out2 = 8;
                break;
            }
        } else {
            if (flags & HELPER_HARVEST) {
                level = helper->harvestSkill;
            } else if (flags & HELPER_WATER) {
                level = helper->waterSkill;
            } else if (flags & HELPER_ANIMALS) {
                level = helper->animalSkill;
            }
            if (level < 200) {
                *out1 = 0;
                *out2 = 4;
            } else {
                *out1 = 0;
                *out2 = 6;
            }
        }
    }
    *out2 += harvest_row_bonuses[helper->number];
    if (helper->flags & HELPER_BIT3) {
        *out2 += 1;
    }
}

/* Return the work amount for the helper's current job: its animal table value,
 * doubled. 24 for any other task. */
static s32 harvest_result_amount(FarmHelper *helper) {
    s32 amount;

    switch (helper->actor.taskNo) {
    case HELPER_HANDLER_HARVEST:
        amount = helper->harvestSkill;
        break;
    case HELPER_HANDLER_WATER:
        amount = helper->waterSkill;
        break;
    case HELPER_HANDLER_ANIMALS:
        amount = helper->animalSkill;
        break;
    default:
        return HARVEST_IDLE_AMOUNT;
    }
    return game_state.animals.moveSpeeds[amount / HARVEST_AMOUNT_STEP] * 2;
}

/* Load the sprite for the helper's item once, when the helper is in the current area.
 * The item is mapped to a category for the area, then that category's entry in its
 * art list is bound. An item with no category is cleared. */
static void harvest_item_sprite_load(FarmHelper *helper) {
    /* Each entry is an art list and the mapper that picks a category in it. */
    CategoryMaps maps = {{
        {0, harvest_item_category_0}, {1, harvest_item_category_1}, {2, harvest_item_category_2}, {3, harvest_item_category_3},
    }};
    FarmLoadArea *far;
    s32 list;
    s32 category;

    if (helper->item == ITEM_NOTHING) {
        helper->itemSprite.source = 0;
        return;
    }
    if (helper->itemSprite.source != 0) {
        return;
    }
    far = &farm_area;
    if (helper->actor.place.location != far->mode.place.location) {
        return;
    }
    switch (helper->actor.place.location) {
    case PLACE_FARM:
        list = maps.map[game_state.progress.clock.date.season].list;
        category = maps.map[game_state.progress.clock.date.season].category(helper->item);
        break;
    case PLACE_4:
    case PLACE_5:
        category = harvest_item_display_category(helper->item);
        list = HARVEST_ART_LIST_BARN_COOP;
        break;
    case PLACE_GREENHOUSE:
        category = harvest_item_category(helper->item);
        list = HARVEST_ART_LIST_GREENHOUSE;
        break;
    default:
        category = -1;
        list = 0;
        break;
    }
    if (category == -1) {
        helper->item = ITEM_NOTHING;
        helper->itemSprite.source = 0;
        return;
    }
    anim_bind_file(&helper->itemSprite,
                   (AnimFile *)&farm_area.itemArt[ANIM_PACK_START(farm_item_tocs[list], category)]);
}

/* Categories of an item in the farm art list of each season: mapper N serves list N, which is
 * season N (SEASON_SPRING to SEASON_WINTER). -1 means no category. */
static s32 harvest_item_category_0(s16 item) {
    switch (item) {
    case ITEM_TURNIP:
        return 7;
    case ITEM_POTATO:
        return 8;
    case ITEM_CUCUMBER:
        return 9;
    case ITEM_STRAWBERRY:
        return 10;
    case ITEM_CABBAGE:
        return 11;
    case ITEM_MOON_DROP_FLOWER:
        return 12;
    case ITEM_TOY_FLOWER:
        return 13;
    case ITEM_GOLDEN_EGG:
        return 23;
    case ITEM_MILK_S:
        return 24;
    case ITEM_MILK_M:
        return 25;
    case ITEM_MILK_L:
        return 26;
    case ITEM_MILK_G:
        return 27;
    case ITEM_WOOL_S:
        return 28;
    case ITEM_WOOL_M:
        return 29;
    case ITEM_WOOL_L:
        return 30;
    case ITEM_WOOL_G:
        return 31;
    case ITEM_NORMAL_EGG:
    case ITEM_GOOD_EGG:
    case ITEM_EXCELLENT_EGG:
        return 22;
    default:
        return -1;
    }
}

static s32 harvest_item_category_1(s16 event) {
    switch (event) {
    case ITEM_TOMATO:
        return 12;
    case ITEM_CORN:
        return 13;
    case ITEM_ONION:
        return 14;
    case ITEM_PUMPKIN:
        return 15;
    case ITEM_PINEAPPLE:
        return 16;
    case ITEM_PINK_CAT_FLOWER:
        return 17;
    case ITEM_ORANGE_CUP_FRUIT:
        return 18;
    case ITEM_GOLDEN_EGG:
        return 28;
    case ITEM_MILK_S:
        return 29;
    case ITEM_MILK_M:
        return 30;
    case ITEM_MILK_L:
        return 31;
    case ITEM_MILK_G:
        return 32;
    case ITEM_WOOL_S:
        return 33;
    case ITEM_WOOL_M:
        return 34;
    case ITEM_WOOL_L:
        return 35;
    case ITEM_WOOL_G:
        return 36;
    case ITEM_NORMAL_EGG:
    case ITEM_GOOD_EGG:
    case ITEM_EXCELLENT_EGG:
        return 27;
    default:
        return -1;
    }
}

static s32 harvest_item_category_2(s16 event) {
    switch (event) {
    case ITEM_EGGPLANT:
        return 12;
    case ITEM_CARROT:
        return 13;
    case ITEM_SWEET_POTATO:
        return 14;
    case ITEM_SPINACH:
        return 15;
    case ITEM_GREEN_PEPPER:
        return 16;
    case ITEM_BLUE_MAGIC_RED_FLOWER:
        return 17;
    case ITEM_RED_MAGIC_RED_FLOWER:
        return 25;
    case ITEM_GOLDEN_EGG:
        return 28;
    case ITEM_MILK_S:
        return 29;
    case ITEM_MILK_M:
        return 30;
    case ITEM_MILK_L:
        return 31;
    case ITEM_MILK_G:
        return 32;
    case ITEM_WOOL_S:
        return 33;
    case ITEM_WOOL_M:
        return 34;
    case ITEM_WOOL_L:
        return 35;
    case ITEM_WOOL_G:
        return 36;
    case ITEM_NORMAL_EGG:
    case ITEM_GOOD_EGG:
    case ITEM_EXCELLENT_EGG:
        return 27;
    default:
        return -1;
    }
}

static s32 harvest_item_category_3(s16 item_id) {
    switch (item_id) {
    case ITEM_NORMAL_EGG ... ITEM_EXCELLENT_EGG:
        return 12;
    case ITEM_GOLDEN_EGG:
        return 13;
    case ITEM_MILK_S:
        return 14;
    case ITEM_MILK_M:
        return 15;
    case ITEM_MILK_L:
        return 16;
    case ITEM_MILK_G:
        return 17;
    case ITEM_WOOL_S:
        return 18;
    case ITEM_WOOL_M:
        return 19;
    case ITEM_WOOL_L:
        return 20;
    case ITEM_WOOL_G:
        return 21;
    default:
        return -1;
    }
}

/* Category of an item in the greenhouse art list (list 4). */
static s32 harvest_item_category(s16 item) {
    switch (item) {
    case ITEM_TURNIP:
        return 4;
    case ITEM_POTATO:
        return 5;
    case ITEM_CUCUMBER:
        return 6;
    case ITEM_STRAWBERRY:
        return 7;
    case ITEM_CABBAGE:
        return 8;
    case ITEM_TOMATO:
        return 9;
    case ITEM_CORN:
        return 10;
    case ITEM_ONION:
        return 11;
    case ITEM_PUMPKIN:
        return 12;
    case ITEM_PINEAPPLE:
        return 13;
    case ITEM_EGGPLANT:
        return 14;
    case ITEM_CARROT:
        return 15;
    case ITEM_SWEET_POTATO:
        return 16;
    case ITEM_SPINACH:
        return 17;
    case ITEM_GREEN_PEPPER:
        return 18;
    case ITEM_MOON_DROP_FLOWER:
        return 20;
    case ITEM_PINK_CAT_FLOWER:
        return 21;
    case ITEM_BLUE_MAGIC_RED_FLOWER:
        return 22;
    case ITEM_RED_MAGIC_RED_FLOWER:
        return 27;
    case ITEM_TOY_FLOWER:
        return 23;
    case ITEM_ORANGE_CUP_FRUIT:
        return 24;
    case ITEM_NORMAL_EGG:
    case ITEM_GOOD_EGG:
    case ITEM_EXCELLENT_EGG:
        return 29;
    case ITEM_GOLDEN_EGG:
        return 30;
    }
    return -1;
}

/* Map an item id onto its category in the barn and coop art list, or -1 when it has none. */
static s32 harvest_item_display_category(s16 item) {
    switch (item) {
    case ITEM_FODDER:
        return 0;
    case ITEM_CHICKEN_FEED_06B:
        return 1;
    case ITEM_NORMAL_EGG:
    case ITEM_GOOD_EGG:
    case ITEM_EXCELLENT_EGG:
        return 5;
    case ITEM_MILK_S:
        return 8;
    case ITEM_MILK_M:
        return 9;
    case ITEM_MILK_L:
        return 0xA;
    case ITEM_MILK_G:
        return 0xB;
    case ITEM_WOOL_S:
        return 0xC;
    case ITEM_WOOL_M:
        return 0xD;
    case ITEM_WOOL_L:
        return 0xE;
    case ITEM_WOOL_G:
        return 0xF;
    case ITEM_GOLDEN_EGG:
        return 6;
    }
    return -1;
}

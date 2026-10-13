/* Farm start-up and re-entry, the zone entry table, and the event availability checks. */

#include <memory.h>
#include "types.h"
#include "actor.h"
#include "afile.h"
#include "animals.h"
#include "clock.h"
#include "clock_events.h"
#include "cue_history.h"
#include "debug_stubs.h"
#include "dog.h"
#include "estate.h"
#include "event_commands.h"
#include "event_flags.h"
#include "event_request.h"
#include "event_script.h"
#include "family.h"
#include "farm_init.h"
#include "farm_load.h"
#include "farm_main.h"
#include "farm_probe.h"
#include "farm_state.h"
#include "farm_tables.h"
#include "farm_vars.h"
#include "field.h"
#include "field_item_requests.h"
#include "ground.h"
#include "harvest_sprites.h"
#include "house.h"
#include "items.h"
#include "main.h"
#include "menu_window.h"
#include "menus.h"
#include "messages.h"
#include "milestones.h"
#include "mine.h"
#include "model.h"
#include "options_menu.h"
#include "photo_album.h"
#include "picture_cells.h"
#include "player.h"
#include "scene.h"
#include "scene_view.h"
#include "screen_fade.h"
#include "shop_restock.h"
#include "sound.h"
#include "sound_request.h"
#include "text.h"
#include "text_bank.h"
#include "villager.h"
#include "weather.h"

static void farm_shop_rows_reset(void);
static void farm_estate_reset(void);
static void farm_bring_up_requests_hook(void);
static void farm_bring_up_begin_hook(void);
static void farm_bring_up_weather_hook(void);
static void farm_zone_register(FarmZone *zone);

#define FARM_ZONE_ENTRY_COUNT 10
#define DEMO_FODDER_STOCK 50
#define DEMO_CHICKEN_FEED_STOCK 50

FarmZoneEntry farm_zone_entries[FARM_ZONE_ENTRY_COUNT];

/* Bring the farm up: reset and load every subsystem in order. In the attract-mode demo
 * (GLOBAL_FLAG_DEMO_START) it also restarts the demo clock and stocks the farm with the
 * demo's starting animals. */
void farm_bring_up(void)
{
    debug_print_buf();
    if (game_state.globalFlags & GLOBAL_FLAG_DEMO_START) {
        game_state.demo->frame = 0;
        rand_seed = 1;
    }
    farm_bring_up_begin_hook();
    farm_estate_reset();
    picture_grid_clear();
    scene_item_queue_reset();
    clock_reset();
    player_new_day_reset();
    villager_clear_all();
    villager_reset_all();
    weather_fx_init();
    drift_reset();
    scene_exit_clear();
    scene_clamp_bounds_default();
    slots_clear();
    scene_ground_tolerance_set();
    greenhouse_clear_cells();
    farm_clear_loaded_slot();
    farm_load_hook();
    farm_load_slot_six();
    player_init_toys(&game_state.player);
    farm_load_season_art(game_state.progress.clock.date.season);
    scene_entries_clear();
    scene_morphs_clear();
    farm_reset_hook();
    estate_result_window_clear(game_state.progress.clock.date.season, 0);
    farm_reset_tables();
    thrown_items_reset();
    message_state_reset();
    farm_reset_hook();
    ending_track_reset();
    harvest_helpers_init();
    menu_reset();
    milestone_reset_timers();
    estate_queues_clear();
    item_requests_clear_all();
    farm_bring_up_requests_hook();
    options_pick_reset();
    farm_shop_rows_reset();
    family_reset();
    farm_load_fixed_block();
    cue_history_reset(STAGING_BUFFER);
    farm_area.mode.modeScene = -1;
    game_state.pendingMode = -1;
    mine_clear_grid();
    farm_clear_resource_buffer();
    shop_restock();
    weather_step_today();
    weather_roll_tomorrow();
    farm_bring_up_weather_hook();
    if (game_state.globalFlags & GLOBAL_FLAG_DEMO_START) {
        farm_action_create(ITEM_COW, -1);
        farm_action_create(ITEM_CALF, -1);
        farm_action_create(ITEM_YOUNG_COW, -1);
        farm_action_create(ITEM_COW, -1);
        farm_action_create(ITEM_LAMB, -1);
        farm_action_create(ITEM_SHEEP, -1);
        farm_action_create(ITEM_CHICK, -1);
        farm_action_create(ITEM_CHICKEN, -1);
        farm_action_create(ITEM_CHICKEN, -1);
        farm_action_create(ITEM_CHICK, -1);
        farm_action_create(ITEM_CHICKEN, -1);
        game_state.progress.estate.fodder = DEMO_FODDER_STOCK;
        game_state.progress.estate.chickenFeed = DEMO_CHICKEN_FEED_STOCK;
    }
}

/* Reset the six race bet rows of each of the three modes. */
static void farm_shop_rows_reset(void) {
    u32 mode;
    u32 row;

    for (mode = 0; mode < 3; mode++) {
        for (row = 0; row < 6; row++) {
            game_state.raceBets.rows[mode][row].runner = -1;
            game_state.raceBets.rows[mode][row].training = 0;
            game_state.raceBets.rows[mode][row].affection = 0;
            game_state.raceBets.rows[mode][row].chance = 0;
            game_state.raceBets.rows[mode][row].rivalChance = 0;
            game_state.raceBets.rows[mode][row].odds = 0;
            game_state.raceBets.rows[mode][row].tickets = 0;
        }
    }
}

/* Reset the building levels, the stock and the mode, and clear the five rival wedding slots. */
static void farm_estate_reset(void) {
    u32 slot;
    game_state.world.options.statusLatch = 1;
    game_state.progress.estate.houseLevel = 0;
    game_state.progress.estate.barnLevel = 0;
    game_state.progress.estate.coopLevel = 0;
    if (game_state.globalFlags & GLOBAL_FLAG_DEMO_START) {
        game_state.progress.estate.houseLevel = 0;
        game_state.progress.estate.barnLevel = 0;
        game_state.progress.estate.coopLevel = 0;
    }
    game_state.progress.estate.greenhouseLevel = 0;
    game_state.progress.estate.lumber = 0;
    game_state.progress.estate.fodder = 0;
    game_state.progress.estate.chickenFeed = 0;
    farm_area.mode.currentMode = -1;
    game_state.pendingMode = -1;
    for (slot = 0; slot < 5; slot++) {
        game_state.world.rivalWeddings[slot].date.year = -1;
        game_state.world.rivalWeddings[slot].groom = -1;
        game_state.world.rivalWeddings[slot].bride = -1;
    }
}

static void farm_bring_up_requests_hook(void) {
}

/* Reset the mode state, then set the STATUS_FLAG_FROM_MG flag of the minigame the player
 * came from (MG1 to MG5), with a new drift hour. */
void farm_apply_mode_flags(void) {
    FarmState *farm;
    s32 result;
    farm = &game_state;
    farm_area.mode.enteredArea = 0xFFFF;
    farm_area.mode.absentArea = 0xFFFF;
    farm->globalFlags &= ~GLOBAL_FLAG_TRANSITION;
    farm_load_hook();
    weather_fx_init();
    drift_reset();
    weather_fx_set_mode(farm->world.weather);
    thrown_items_reset();
    message_state_reset();
    farm_reset_hook();
    farm->progress.entryStep = 1;
    farm_startup_phase = FARM_STARTUP_LAYOUT;
    farm_area.mode.view = FARM_VIEW_NORMAL;
    farm_area.mode.script.head.resourceId = -1;
    farm_area.mode.script.head.resourceSet = -1;
    farm_area.mode.script.head.place.value = -1;
    farm_area.mode.modeScene = -1;
    farm_area.mode.exit = 0;
    farm_area.mode.currentMode = -1;
    farm->pendingMode = -1;
    sound_request(SOUND_REQUEST_SEQ_1, 0);
    farm->statusFlags &= ~STATUS_FLAG_FROM_MG_ANY;
    result = main_get_previous_scene();
    if (result == SCENE_MG1) {
        farm->statusFlags |= STATUS_FLAG_FROM_MG1;
        drift_select_hour();
    }
    result = main_get_previous_scene();
    if (result == SCENE_MG2) {
        farm->statusFlags |= STATUS_FLAG_FROM_MG2;
        drift_select_hour();
    }
    result = main_get_previous_scene();
    if (result == SCENE_MG3) {
        farm->statusFlags |= STATUS_FLAG_FROM_MG3;
        drift_select_hour();
    }
    result = main_get_previous_scene();
    if (result == SCENE_MG4) {
        farm->statusFlags |= STATUS_FLAG_FROM_MG4;
        drift_select_hour();
    }
    result = main_get_previous_scene();
    if (result == SCENE_MG5) {
        farm->statusFlags |= STATUS_FLAG_FROM_MG5;
        drift_select_hour();
    }
}

void farm_clear_resource_buffer(void) {
    s32 *p = farm_area.mode.resourceBuffer;
    u32 i = 0;
    do {
        *p = 0;
        i++;
        p++;
    } while (i < 1664);
}

static void farm_bring_up_begin_hook(void) {
}

static void farm_bring_up_weather_hook(void) {
}

/* Farm overlay start-up: reset the farm state and initialise every subsystem. */
void farm_startup(void)
{
    s32 base;

    farm_area.mode.enteredArea = 0xFFFF;
    farm_area.mode.absentArea = 0xFFFF;
    farm_area.mode.script.head.resourceId = -1;
    farm_area.mode.script.head.resourceSet = -1;
    farm_area.mode.script.head.place.value = -1;
    farm_area.mode.previous = &farm_area.mode.previousHead;
    farm_area.mode.modeScene = -1;
    farm_area.mode.exit = 0;
    farm_area.mode.currentMode = -1;
    farm_area.mode.data = &farm_area.mode.script;
    game_state.globalFlags &= ~GLOBAL_FLAG_TRANSITION;
    base = game_state.progress.estate.houseLevel * 4 + HOUSE_SCENE_BASE;
    farm_area.mode.area = game_state.progress.clock.date.season + base;
    memset(&farm_area.mode.previousHead, 0, sizeof(farm_area.mode.previousHead));
    game_state.player.scriptSteps = 0;
    farm_tables_register(1);
    farm_load_hook();
    picture_grid_clear();
    weather_fx_init();
    drift_reset();
    weather_fx_set_mode(game_state.world.weather);
    farm_load_player_sprite();
    farm_load_resource(0);
    player_load_item_pictures();
    farm_load_slot_six();
    player_init_toys(&game_state.player);
    farm_load_season_art(game_state.progress.clock.date.season);
    scene_entries_clear();
    scene_morphs_clear();
    farm_reset_hook();
    estate_result_window_clear(game_state.progress.clock.date.season, 1);
    clock_reset_queue();
    thrown_items_reset();
    message_state_reset();
    farm_reset_hook();
    ending_track_reset();
    menu_reset();
    milestone_refresh();
    item_requests_clear_all();
    options_pick_reset();
    farm_spawn_chicken_sources();
    clock_refresh_dated_event_flag();
    dog_sprite_load(&game_state.animals.dog);
    farm_load_fixed_block();
    cue_history_reset(STAGING_BUFFER);
    game_state.progress.entryStep = 1;
    farm_startup_phase = FARM_STARTUP_LAYOUT;
    farm_area.mode.view = FARM_VIEW_NORMAL;
    game_state.pendingMode = -1;
    farm_area.mode.eventMode = FARM_MODE_SLEEP;
    game_state.statusFlags |= STATUS_FLAG_STARTUP_PENDING;
    if (game_state.progress.clock.time.hm.hour >= 7) {
        event_flag_set(EVENT_FLAG_MORNING_MESSAGE, 1);
    }
    options_apply_settings();
}

/* Farm re-entry: a lighter version of the start-up. It resets the mode state, reloads the
 * scene lists and waits for the sound fader. On PLACE_2 it runs the place's event; unless
 * the player has collapsed or a request is busy, it gives a random sandwich.
 * GLOBAL_FLAG_TRANSITION is cleared on entry and on exit. */
void farm_reenter(void) {
    game_state.globalFlags &= ~GLOBAL_FLAG_TRANSITION;
    weather_fx_init();
    drift_reset();
    weather_fx_set_mode(game_state.world.weather);
    drift_select_hour();
    thrown_items_reset();
    game_state.progress.entryStep = 2;
    farm_area.mode.view = FARM_VIEW_NORMAL;
    model_bind_tmd(&farm_model, farm_area.sectionBuffer + SECTION_STRINGS_OFFSET);
    scene_clamp_bounds_set(farm_area.mode.script.clampMin, farm_area.mode.script.clampMax);
    ground_enable_objects(&farm_model);
    slots_clear();
    scene_morphs_clear();
    scene_entries_clear();
    scene_populate_objects();
    scene_mode_queue_run(&farm_area.mode.script);
    farm_zones_register_script(&farm_area.mode.script);
    farm_mode_change();
    while (sound_fader_ready() == 0) {
        sound_load_tick();
    }
    sound_update();
    farm_set_message_timing();
    if (farm_area.mode.place.location == PLACE_2) {
        event_trigger_mode_set(8);
        event_triggers_run();
        event_request_step();
        event_request_step();
    }
    if (!(game_state.statusFlags & STATUS_FLAG_COLLAPSED) && event_flag_get(EVENT_FLAG_REQUEST_BUSY) == 0) {
        player_receive_item(&game_state.player, ITEM_TRIANGLE_SANDWICH + rand_range(4), RECEIVE_ITEM_HOLD);
        if (game_state.progress.clock.time.hm.hour >= 7) {
            event_flag_set(EVENT_FLAG_MORNING_MESSAGE, 1);
        }
    }
    sound_fade_in(10);
    fade_start_in(10, 0, 2);
    game_state.globalFlags &= ~GLOBAL_FLAG_TRANSITION;
}

void farm_enter_startup(void) {
    farm_startup();
}

#define PLOT_COUNT (game_state.progress.estate.coopLevel == 0 ? 5 : 10)
#define PREVIOUS_PLACE (farm_area.mode.previous->place.location)
/* The game years a courtship ceiling is checked against (the year counter starts at 0). */
enum CourtshipYear {
    COURTSHIP_FIRST_YEAR = 0,
    COURTSHIP_SECOND_YEAR = 1,
};

/* The affection at which each heart starts. */
#define AFFECTION_HEART_2 20000
#define AFFECTION_HEART_3 30000
#define AFFECTION_HEART_4 40000
#define AFFECTION_HEART_5 50000
#define AFFECTION_HEART_6 60000
#define AFFECTION_HEART_6U 60000U

/* Mary's daytime window, 10:00 to 17:59, less the break from 13:00 to 15:59. */
#define MARY_DAYTIME_START 10
#define MARY_DAYTIME_SPAN 8U
#define MARY_AWAY_START 13
#define MARY_AWAY_SPAN 3U

/* field_find_free_tile flags (field.c): 2 lets the ground match anything, 4 the kind, 8 keeps
 * counting after a match. 14 counts by value alone, 10 by value and kind. */
#define FIELD_FIND_VALUE_ONLY 14
#define FIELD_FIND_VALUE_AND_KIND 10

#define YEAR_CYCLE_LENGTH 9
#define POND_FISH_GOAL 50
#define PREGNANCY_FIRST_WEEK_FIRST 1
#define PREGNANCY_FIRST_WEEK_LAST 7
#define PREGNANCY_TERM_DAYS 60
#define WIFE_IGNORED_DAYS 30
#define CHILD_GROWTH_DAYS 10
#define HOUSE_TOP_LEVEL 2
#define DAY_COUNT2_END 89
#define HELPER_AFFECTION_GOAL 80
#define CONTEST_AFFECTION_GOAL 50
#define GOLD_GOAL_100 100
#define GOLD_GOAL_500 500
#define GOLD_GOAL_5000 5000
#define GOLD_GOAL_30000 30000
/* farm_kind_selected arguments: mode 7 is livestock (either kind) and chickens; kind 3 is
 * any death (died of age or young). */
#define FARM_SELECT_LIVESTOCK_AND_CHICKENS 7
#define FARM_SELECT_ANY_DIED 3

#define COURTSHIP_GATE(girl)                                            \
    {                                                                   \
        FarmState *partner = &game_state;                               \
        if (partner->player.wedding.bride != -1 && partner->player.wedding.bride != (girl)) {     \
            return 1;                                                   \
        }                                                               \
    }

/* While nobody is engaged, or `girl` is the one, return whether her
 * affection is still below this year's ceiling (`first` in the first year,
 * AFFECTION_HEART_4 in the second, `later` after that); otherwise return 1. */
#define COURTABLE(girl, first, later)                                   \
    COURTSHIP_GATE(girl)                                            \
    {                                                                   \
        FarmState *farm = &game_state;                                  \
        u16 points = farm->villagers[girl].affection;                   \
        u32 limit;                                                      \
        switch (farm->progress.clock.date.year) {                                        \
        case COURTSHIP_FIRST_YEAR:                                      \
            limit = (first);                                            \
            break;                                                      \
        case COURTSHIP_SECOND_YEAR:                                     \
            limit = AFFECTION_HEART_4;                                  \
            break;                                                      \
        default:                                                        \
            limit = (later);                                            \
            break;                                                      \
        }                                                               \
        return points < limit;                                          \
    }

/* COURTABLE with one ceiling for every year. */
#define COURTABLE_UNDER(girl, limit)                                    \
    COURTSHIP_GATE(girl)                                            \
    {                                                                   \
        FarmState *farm = &game_state;                                  \
        u32 ceiling = (limit);                                          \
                                                                        \
        return farm->villagers[girl].affection < ceiling;               \
    }

/* Like COURTABLE_UNDER, but without the local ceiling. */
#define COURTABLE_BELOW(girl, limit)                                    \
    COURTSHIP_GATE(girl)                                            \
    {                                                                   \
        FarmState *farm = &game_state;                                  \
        return farm->villagers[girl].affection < (limit);               \
    }

/* 1 when the actor has grown up (ACTOR_FLAG_ADULT), else 0. */
#define ACTOR_ADULT_BIT(actor) ((((actor).flags >> 8) & 0xFF) >> 7)

/* The five crop item ids checked by farm_event_available. */
static u16 farm_init_crop_ids[5] = {ITEM_MOONDROP_SEEDS, ITEM_PINK_CAT_SEEDS, ITEM_BLUE_MAGIC_RED_PLANT, ITEM_RED_MAGIC_RED_PLANT, ITEM_TOY_FLOWER_SEEDS};

/* Return 1 when the event condition `id` holds now, else 0. */
s32 farm_event_available(s16 id) {

    switch (id) {
    case EVENT_COND_NO_HORSE_ON_MAP_2A_30:
        if (game_state.animals.horse.actor.kind != 0) {
            return 0;
        }
        if (PREVIOUS_PLACE == PLACE_30 || PREVIOUS_PLACE == PLACE_2A) {
            return 1;
        }
        return 0;
    case EVENT_COND_HORSE_ADULT:
    case EVENT_COND_HORSE_ADULT_2:
        return ACTOR_ADULT_BIT(game_state.animals.horse.actor);
    case EVENT_COND_CAN_TAKE_BANDAGE:
        return event_item_can_take(ITEM_BANDAGE, 1, 1, 0);
    case EVENT_COND_PLOT_FREE: {
        s32 count;
        s32 i;

        count = 0;
        for (i = 0; i < 10; i++) {
            if (game_state.animals.chickens[i].actor.kind != 0 || (game_state.animals.chickens[i].actor.flags & ACTOR_FLAG_CONTEST_ENTRANT)) {
                count++;
            }
        }
        if (count < PLOT_COUNT) {
            return 1;
        }
        return 0;
    }
    case EVENT_COND_POPURI_CHICK_NAMED: {
        s32 i;

        for (i = 0; i < 10; i++) {
            if ((game_state.animals.chickens[i].actor.flags & ACTOR_FLAG_BIT30) && game_state.animals.chickens[i].actor.kind == ITEM_CHICK) {
                break;
            }
        }
        if (i == 10) {
            return 0;
        }
        if (game_state.progress.clock.date.day == game_state.animals.chickens[i].actor.date.day) {
            return 0;
        }
        return text_match_either(game_state.animals.chickens[i].actor.name,
                                 LOCALIZED_NAME(text_bank.names.chicken_names.popuri),
                                 LOCALIZED_NAME(text_bank.names.chicken_names.popuri_2)) != 0;
    }
    case EVENT_COND_POPURI_CHICK_NOT_NAMED: {
        s32 i;

        for (i = 0; i < 10; i++) {
            if ((game_state.animals.chickens[i].actor.flags & ACTOR_FLAG_BIT30) && game_state.animals.chickens[i].actor.kind == ITEM_CHICK) {
                break;
            }
        }
        if (i == 10) {
            return 0;
        }
        if (game_state.progress.clock.date.day == game_state.animals.chickens[i].actor.date.day) {
            return 0;
        }
        return text_match_either(game_state.animals.chickens[i].actor.name,
                                 LOCALIZED_NAME(text_bank.names.chicken_names.popuri),
                                 LOCALIZED_NAME(text_bank.names.chicken_names.popuri_2)) == 0;
    }
    case EVENT_COND_MOONDROP_SEEDS_PLANTABLE: {
        FarmFieldTile cell;
        s16 remainder0, quotient0;

        cell.value = ITEM_MOONDROP_SEEDS;
        cell.kind = crop_grown_stage(ITEM_MOONDROP_SEEDS);
        if (field_find_free_tile(&remainder0, &quotient0, 1, &cell, 2) != 0) {
            return 1;
        }
        return field_find_free_tile(&remainder0, &quotient0, 0, &cell, 2) != 0;
    }
    case EVENT_COND_CAN_TAKE_WOODCUTTER_BOOK:
        return event_item_can_take(ITEM_WOODCUTTER_BOOK, 1, 1, 0);
    case EVENT_COND_INVITATION_DUE: {
        s32 result;

        result = event_item_can_take(ITEM_INVITATION, 1, 1, 0);
        if (result == 1 && game_state.days.values[DAY_VALUE_INVITATION_YEAR] == game_state.progress.clock.date.year + 1 &&
            event_cmd_is_birthday(7, 0, -1, -1) == result) {
            return 1;
        }
        return 0;
    }
    case EVENT_COND_POPURI_LOVE_BELOW_1:
        COURTABLE(CHARACTER_POPURI, AFFECTION_HEART_2, AFFECTION_HEART_5)
    case EVENT_COND_POPURI_LOVE_BELOW_2:
        COURTABLE(CHARACTER_POPURI, AFFECTION_HEART_2, AFFECTION_HEART_5)
    case EVENT_COND_POPURI_LOVE_BELOW_3:
        COURTABLE(CHARACTER_POPURI, AFFECTION_HEART_2, AFFECTION_HEART_5)
    case EVENT_COND_POPURI_LOVE_UNDER_50000:
        COURTABLE_UNDER(CHARACTER_POPURI, AFFECTION_HEART_5)
    case EVENT_COND_ANN_LOVE_BELOW_1: {
        FarmState *farm;

        if (event_flag_get(EVENT_FLAG_192) == 0 && game_state.player.place.location != game_state.villagers[CHARACTER_CLIFF].place.location) {
            return 0;
        }
        farm = &game_state;
        if (farm->progress.clock.date.year == 0) {
            if (farm->progress.clock.date.season == SEASON_FALL) {
                if ((u16)farm->progress.clock.date.weekday - 14 < 5U) {
                    return 0;
                }
            }
        }
        COURTABLE(CHARACTER_ANN, AFFECTION_HEART_3, AFFECTION_HEART_6)
    }
    case EVENT_COND_ANN_LOVE_BELOW_2:
        if (event_flag_get(EVENT_FLAG_E6) == 0) {
            return 0;
        }
        COURTABLE(CHARACTER_ANN, AFFECTION_HEART_3, AFFECTION_HEART_6)
    case EVENT_COND_ANN_LOVE_BELOW_3:
        COURTABLE(CHARACTER_ANN, AFFECTION_HEART_3, AFFECTION_HEART_6)
    case EVENT_COND_ANN_LOVE_UNDER_60000:
        COURTABLE_UNDER(CHARACTER_ANN, AFFECTION_HEART_6)
    case EVENT_COND_FLAG_E6_CLEAR:
        return event_flag_get(EVENT_FLAG_E6) != 1;
    case EVENT_COND_KAREN_LOVE_BELOW_1:
        COURTABLE(CHARACTER_KAREN, AFFECTION_HEART_2, AFFECTION_HEART_6)
    case EVENT_COND_KAREN_LOVE_BELOW_2:
        COURTABLE(CHARACTER_KAREN, AFFECTION_HEART_2, AFFECTION_HEART_6)
    case EVENT_COND_KAREN_LOVE_WEDDING_GATE:
        {
            FarmState *partner = &game_state;
            if (partner->player.wedding.bride != -1 && partner->player.wedding.bride != CHARACTER_KAREN) {
                return 1;
            }
        }
        {
            FarmState *farm = &game_state;
            u16 points = farm->villagers[CHARACTER_KAREN].affection;

            if (farm->progress.clock.date.year == 1) {
                if (points < AFFECTION_HEART_4) {
                    return 1;
                }
            } else if (points < AFFECTION_HEART_6 && (farm->villagers[CHARACTER_POPURI].flags & VILLAGER_FLAG_WEDDING_ROLE)) {
                return 1;
            }
            return 0;
        }
    case EVENT_COND_POPURI_MARRIED_OR_KAREN_LOVE:
        if (event_flag_get(EVENT_FLAG_MARRIED_POPURI) == 1) {
            return 1;
        }
        if (event_flag_get(EVENT_FLAG_POPURI_KAI_MARRIED) == 0) {
            return 0;
        }
        COURTABLE_BELOW(CHARACTER_KAREN, AFFECTION_HEART_6U)
    case EVENT_COND_KAREN_OFFER_UNDER_60000:
        if (wedding_offer_state(0) != WEDDING_OFFER_YES_VARIANT) {
            return 0;
        }
        COURTABLE_UNDER(CHARACTER_KAREN, AFFECTION_HEART_6)
    case EVENT_COND_ELLI_LOVE_BELOW_1:
        COURTABLE(CHARACTER_ELLI, AFFECTION_HEART_2, AFFECTION_HEART_6)
    case EVENT_COND_ELLI_LOVE_BELOW_2:
        COURTABLE(CHARACTER_ELLI, AFFECTION_HEART_2, AFFECTION_HEART_6)
    case EVENT_COND_MARY_LOVE_BELOW_1:
        COURTABLE(CHARACTER_MARY, AFFECTION_HEART_2, AFFECTION_HEART_6)
    case EVENT_COND_MARY_LOVE_BELOW_2_DAYTIME:
        if ((u16)game_state.progress.clock.time.hm.hour - MARY_DAYTIME_START >= MARY_DAYTIME_SPAN) {
            return 0;
        }
        if ((u16)game_state.progress.clock.time.hm.hour - MARY_AWAY_START < MARY_AWAY_SPAN) {
            return 0;
        }
        COURTABLE(CHARACTER_MARY, AFFECTION_HEART_2, AFFECTION_HEART_6)
    case EVENT_COND_MARY_LOVE_BELOW_3:
        COURTABLE(CHARACTER_MARY, AFFECTION_HEART_2, AFFECTION_HEART_6)
    case EVENT_COND_MARY_LOVE_UNDER_60000:
        COURTABLE_UNDER(CHARACTER_MARY, AFFECTION_HEART_6)
    case EVENT_COND_FIELD_TILES_200: {
        FarmFieldTile cell;
        s16 remainder1, quotient1;
        s32 count;

        if (game_state.animals.horse.actor.kind != 0) {
            return 0;
        }
        cell.value = FIELD_TILE_GROWN;
        cell.kind = 0;
        count = field_find_free_tile(&remainder1, &quotient1, 1, &cell, FIELD_FIND_VALUE_ONLY);
        count += field_find_free_tile(&remainder1, &quotient1, 0, &cell, FIELD_FIND_VALUE_ONLY);
        count -= field_find_free_tile(&remainder1, &quotient1, 1, &cell, FIELD_FIND_VALUE_AND_KIND);
        count -= field_find_free_tile(&remainder1, &quotient1, 0, &cell, FIELD_FIND_VALUE_AND_KIND);
        return count >= 200;
    }
    case EVENT_COND_STORM_TODAY:
        return event_flag_get(EVENT_FLAG_STORM_TODAY);
    case EVENT_COND_CORN_OR_DOG_ADULT: {
        FarmFieldTile cell2;
        s16 remainder2, quotient2;

        if (event_item_can_take(ITEM_CORN, 1, 1, 0) == 1) {
            return 1;
        }
        cell2.value = ITEM_CORN_SEEDS;
        cell2.kind = crop_grown_stage(ITEM_CORN_SEEDS);
        if (field_find_free_tile(&remainder2, &quotient2, 1, &cell2, 2) != 0) {
            return 1;
        }
        if (field_find_free_tile(&remainder2, &quotient2, 0, &cell2, 2) != 0) {
            return 1;
        }
        return ACTOR_ADULT_BIT(game_state.animals.dog.actor);
    }
    case EVENT_COND_HELPERS_AT_28_AFFECTION_80: {
        s32 i;

        for (i = 0; i < HELPER_COUNT; i++) {
            if (game_state.helpers[i].actor.place.location != PLACE_28 || (game_state.helpers[i].flags & HELPER_BIT5)) {
                return 0;
            }
        }
        for (i = 0; i < HELPER_COUNT; i++) {
            if (game_state.helpers[i].actor.affection >= HELPER_AFFECTION_GOAL) {
                return 1;
            }
        }
        return 0;
    }
    case EVENT_COND_NOT_ANN_MARRIED:
        if (game_state.player.wedding.bride == CHARACTER_ANN && event_flag_get(EVENT_FLAG_SINGLE) == 0) {
            return 0;
        }
        return 1;
    case EVENT_COND_FLOWER_TILES_90: {
        FarmFieldTile cell2;
        s16 remainder3, quotient3;
        s32 i;
        s16 total;

        total = 0;
        for (i = 0; i < 5; i++) {
            cell2.value = farm_init_crop_ids[i];
            cell2.kind = crop_grown_stage(farm_init_crop_ids[i]);
            total += field_find_free_tile(&remainder3, &quotient3, 1, &cell2, 10);
            total += field_find_free_tile(&remainder3, &quotient3, 0, &cell2, 10);
        }
        return total >= 90;
    }
    case EVENT_COND_CAN_TAKE_APPLE_PIE:
        return event_item_can_take(ITEM_APPLE_PIE, 1, 0, 0);
    case EVENT_COND_FLAG_E8_SET:
        return event_flag_get(EVENT_FLAG_E8) == 1;
    case EVENT_COND_FREE_CHICKEN_PLOTS_5: {
        s32 count;
        s32 i;

        count = 0;
        for (i = 0; i < PLOT_COUNT; i++) {
            if (game_state.animals.chickens[i].actor.kind == 0 && !(game_state.animals.chickens[i].actor.flags & ACTOR_FLAG_CLAIMED)) {
                count++;
            }
        }
        return count >= 5;
    }
    case EVENT_COND_CONTEST_ENTRANTS_AFFECTION_50: {
        s32 i;

        for (i = 0; i < PLOT_COUNT; i++) {
            if (game_state.animals.chickens[i].actor.kind != 0 && (game_state.animals.chickens[i].actor.flags & ACTOR_FLAG_CONTEST_ENTRANT) &&
                game_state.animals.chickens[i].actor.affection < CONTEST_AFFECTION_GOAL) {
                return 0;
            }
        }
        return 1;
    }
    case EVENT_COND_NOT_MAP_24_25:
        return PREVIOUS_PLACE != 0x24 && PREVIOUS_PLACE != 0x25;
    case EVENT_COND_CHICKENS_3: {
        s32 count;
        s32 i;

        count = 0;
        for (i = 0; i < PLOT_COUNT; i++) {
            if (game_state.animals.chickens[i].actor.kind == ITEM_CHICKEN) {
                count++;
            }
        }
        return count >= 3;
    }
    case EVENT_COND_EGGS_3: {
        s16 total;

        if (game_state.days.count0[1] == 0) {
            return 0;
        }
        total = event_items_take(ITEM_NORMAL_EGG, 1, 1, 0) + event_items_take(ITEM_GOOD_EGG, 1, 1, 0);
        total += event_items_take(ITEM_EXCELLENT_EGG, 1, 1, 0);
        total += event_items_take(ITEM_GOLDEN_EGG, 1, 1, 0);
        total += event_items_take(ITEM_POPURIS_EGG, 1, 1, 0);
        return total >= 3;
    }
    case EVENT_COND_POPURI_OFFER_YES:
        if (game_state.player.wedding.bride != CHARACTER_POPURI) {
            return 0;
        }
        return wedding_offer_state(-1) == WEDDING_OFFER_YES;
    case EVENT_COND_ANN_OFFER_YES:
        if (game_state.player.wedding.bride != CHARACTER_ANN) {
            return 0;
        }
        return wedding_offer_state(-1) == WEDDING_OFFER_YES;
    case EVENT_COND_KAREN_OFFER_YES:
        if (game_state.player.wedding.bride != CHARACTER_KAREN) {
            return 0;
        }
        return wedding_offer_state(-1) == WEDDING_OFFER_YES;
    case EVENT_COND_ELLI_OFFER_YES:
        if (game_state.player.wedding.bride != CHARACTER_ELLI) {
            return 0;
        }
        return wedding_offer_state(-1) == WEDDING_OFFER_YES;
    case EVENT_COND_MARY_OFFER_YES:
        if (game_state.player.wedding.bride != CHARACTER_MARY) {
            return 0;
        }
        return wedding_offer_state(-1) == WEDDING_OFFER_YES;
    case EVENT_COND_DAYS_COUNT2_NOT_0X59:
        return game_state.days.count0[0x2] != DAY_COUNT2_END;
    case EVENT_COND_DAYS_COUNT0X30_SET:
        return game_state.days.count0[0x30] != 0;
    case EVENT_COND_ANIMAL_DIED:
        return farm_kind_selected(FARM_SELECT_LIVESTOCK_AND_CHICKENS, FARM_SELECT_ANY_DIED) == 1;
    case EVENT_COND_FISH_50:
        return game_state.animals.fishCount >= POND_FISH_GOAL;
    case EVENT_COND_CATCHES_ON_MAP_C_22:
        if ((game_state.progress.estate.estateFlags & ESTATE_FLAG_CATCHES) != ESTATE_FLAG_CATCHES) {
            return 0;
        }
        if (PREVIOUS_PLACE == PLACE_C || PREVIOUS_PLACE == PLACE_22) {
            return 1;
        }
        return 0;
    case EVENT_COND_KAREN_NOT_PREGNANT_OR_PENDING:
        if (game_state.player.wedding.bride == CHARACTER_KAREN) {
            if (event_flag_get(EVENT_FLAG_PREGNANT) == 1) {
                return 0;
            }
            if (event_flag_get(EVENT_FLAG_WIFE_REQUEST_PENDING) == 1) {
                return 0;
            }
        }
        return 1;
    case EVENT_COND_NOT_WIFE_REQUEST_PENDING:
        return event_flag_get(EVENT_FLAG_WIFE_REQUEST_PENDING) != 1;
    case EVENT_COND_FAMILY_SELECTION_TODAY:
        if (event_flag_get(EVENT_FLAG_WIFE_REQUEST_PENDING) == 1) {
            return 0;
        }
        if (game_state.family.selection.season == game_state.progress.clock.date.season &&
            game_state.family.selection.day == game_state.progress.clock.date.day) {
            return 1;
        }
        return 0;
    case EVENT_COND_BRIDE_BIRTHDAY_TODAY:
        if (event_flag_get(EVENT_FLAG_WIFE_REQUEST_PENDING) == 1) {
            return 0;
        }
        if (game_state.villagers[game_state.player.wedding.bride].birthday.season == game_state.progress.clock.date.season &&
            game_state.villagers[game_state.player.wedding.bride].birthday.day == game_state.progress.clock.date.day) {
            return 1;
        }
        return 0;
    case EVENT_COND_CHILD_BIRTHDAY_TODAY:
        if (event_flag_get(EVENT_FLAG_WIFE_REQUEST_PENDING) == 1) {
            return 0;
        }
        if (game_state.villagers[CHARACTER_CHILD].birthday.season == game_state.progress.clock.date.season &&
            game_state.villagers[CHARACTER_CHILD].birthday.day == game_state.progress.clock.date.day) {
            return 1;
        }
        return 0;
    case EVENT_COND_PLAYER_BIRTHDAY_TODAY:
        if (event_flag_get(EVENT_FLAG_WIFE_REQUEST_PENDING) == 1) {
            return 0;
        }
        if (game_state.player.birthday.season == game_state.progress.clock.date.season && game_state.player.birthday.day == game_state.progress.clock.date.day) {
            return 1;
        }
        return 0;
    case EVENT_COND_PREGNANT_1_TO_7_DAYS:
        return game_state.family.pregnancyDays >= PREGNANCY_FIRST_WEEK_FIRST && game_state.family.pregnancyDays <= PREGNANCY_FIRST_WEEK_LAST;
    case EVENT_COND_PREGNANT_60_NOT_BIRTHDAY:
        if (game_state.family.pregnancyDays < PREGNANCY_TERM_DAYS) {
            return 0;
        }
        if (game_state.player.birthday.season == game_state.progress.clock.date.season && game_state.player.birthday.day == game_state.progress.clock.date.day) {
            return 0;
        }
        if (game_state.villagers[game_state.player.wedding.bride].birthday.season == game_state.progress.clock.date.season &&
            game_state.villagers[game_state.player.wedding.bride].birthday.day == game_state.progress.clock.date.day) {
            return 0;
        }
        return 1;
    case EVENT_COND_WIFE_REQUESTS_1_2:
        return game_state.family.wifeRequests >= 1 && game_state.family.wifeRequests <= 2;
    case EVENT_COND_WIFE_REQUESTS_3_UP:
        return game_state.family.wifeRequests >= 3;
    case EVENT_COND_WIFE_IGNORED_30_DAYS:
        if (game_state.family.wifeIgnoredDays >= WIFE_IGNORED_DAYS && game_state.family.pregnancyDays == 0 && game_state.family.childGrowthDays == 0 &&
            game_state.family.wifeRequests == 0) {
            return 1;
        }
        return 0;
    case EVENT_COND_CHILD_GROWTH_10_DAYS:
        return game_state.family.childGrowthDays >= CHILD_GROWTH_DAYS;
    case EVENT_COND_SPAWN_GATE_COUNT0X17:
        if (game_state.animals.spawnGate != 0 && game_state.animals.spawnGate != PLACE_9) {
            return 0;
        }
        return game_state.days.count0[0x17] != 0;
    case EVENT_COND_FLOWER_SEEDS_PLANTABLE: {
        FarmFieldTile cell3;
        s16 remainder4, quotient4;

        cell3.value = ITEM_MOONDROP_SEEDS;
        cell3.kind = crop_grown_stage(ITEM_MOONDROP_SEEDS);
        if (field_find_free_tile(&remainder4, &quotient4, 1, &cell3, 2) != 0) {
            return 1;
        }
        cell3.value = ITEM_PINK_CAT_SEEDS;
        cell3.kind = crop_grown_stage(ITEM_PINK_CAT_SEEDS);
        if (field_find_free_tile(&remainder4, &quotient4, 1, &cell3, 2) != 0) {
            return 1;
        }
        cell3.value = ITEM_BLUE_MAGIC_RED_PLANT;
        cell3.kind = crop_grown_stage(ITEM_BLUE_MAGIC_RED_PLANT);
        if (field_find_free_tile(&remainder4, &quotient4, 1, &cell3, 2) != 0) {
            return 1;
        }
        cell3.value = ITEM_RED_MAGIC_RED_PLANT;
        cell3.kind = crop_grown_stage(ITEM_RED_MAGIC_RED_PLANT);
        if (field_find_free_tile(&remainder4, &quotient4, 1, &cell3, 2) != 0) {
            return 1;
        }
        cell3.value = ITEM_TOY_FLOWER_SEEDS;
        cell3.kind = crop_grown_stage(ITEM_TOY_FLOWER_SEEDS);
        return field_find_free_tile(&remainder4, &quotient4, 1, &cell3, 2) != 0;
    }
    case EVENT_COND_GOLD_500:
        return game_state.player.gold >= GOLD_GOAL_500;
    case EVENT_COND_GOLD_30000_AFFECTION_50000: {
        s32 i;

        if (game_state.progress.estate.estateFlags & ESTATE_FLAG_BIT21) {
            return 0;
        }
        if (game_state.player.gold < GOLD_GOAL_30000) {
            return 0;
        }
        for (i = 0; i < CHARACTER_COUNT; i++) {
            if (game_state.villagers[i].affection >= AFFECTION_HEART_5) {
                return 1;
            }
        }
        return 0;
    }
    case EVENT_COND_ADULT_DOG_GOLD_100:
        if (game_state.player.gold >= GOLD_GOAL_100 && !(game_state.progress.estate.estateFlags & ESTATE_FLAG_TRIGGER_ARMED) && game_state.animals.dog.actor.kind == ITEM_ADULT_DOG) {
            return 1;
        }
        return 0;
    case EVENT_COND_HOUSE_LEVEL_2_GOLD_5000:
        if (game_state.progress.estate.houseLevel != HOUSE_TOP_LEVEL) {
            return 0;
        }
        if (game_state.player.gold < GOLD_GOAL_5000) {
            return 0;
        }
        return game_state.days.values[0x2] < 4;
    case EVENT_COND_YEAR_CYCLE_ITEMS_3: {
        s32 flag;
        s16 total;

        flag = 0;
        total = 0;
        switch (game_state.progress.clock.date.year % YEAR_CYCLE_LENGTH) {
        case 0:
            flag = EVENT_FLAG_PLAYER_MARRIED;
            break;
        case 1:
            flag = EVENT_FLAG_OVERNIGHT;
            break;
        case 2:
            flag = EVENT_FLAG_MINE_RETURN_37;
            break;
        case 3:
            flag = EVENT_FLAG_WIFE_REQUEST_PENDING;
            break;
        case 4:
            total = event_items_take(ITEM_NORMAL_EGG, 1, 1, 0);
            total += event_items_take(ITEM_GOOD_EGG, 1, 1, 0);
            total += event_items_take(ITEM_EXCELLENT_EGG, 1, 1, 0);
            total += event_items_take(ITEM_GOLDEN_EGG, 1, 1, 0);
            total += event_items_take(ITEM_POPURIS_EGG, 1, 1, 0);
            break;
        case 5:
            total = event_items_take(ITEM_MILK_S, 1, 1, 0);
            total += event_items_take(ITEM_MILK_M, 1, 1, 0);
            total += event_items_take(ITEM_MILK_L, 1, 1, 0);
            total += event_items_take(ITEM_MILK_G, 1, 1, 0);
            break;
        case 6:
            flag = 0xA2;
            break;
        case 7:
            flag = EVENT_FLAG_MINE_NEXT_LAYER;
            break;
        default:
            flag = EVENT_FLAG_PLAYER_BIRTHDAY;
            break;
        }
        if (total >= 3) {
            return 1;
        }
        if (flag != 0 && event_items_take(flag, 1, 1, 0) >= 3) {
            return 1;
        }
        return 0;
    }
    case EVENT_COND_UTENSIL_DUE_KNIFE:
        return event_flag_get(EVENT_FLAG_UTENSIL_DUE_KNIFE) == 1;
    case EVENT_COND_UTENSIL_DUE_FRYING_PAN:
        return event_flag_get(EVENT_FLAG_UTENSIL_DUE_FRYING_PAN) == 1;
    case EVENT_COND_UTENSIL_DUE_POT:
        return event_flag_get(EVENT_FLAG_UTENSIL_DUE_POT) == 1;
    case EVENT_COND_UTENSIL_DUE_MIXER:
        return event_flag_get(EVENT_FLAG_UTENSIL_DUE_MIXER) == 1;
    case EVENT_COND_UTENSIL_DUE_WHISK:
        return event_flag_get(EVENT_FLAG_UTENSIL_DUE_WHISK) == 1;
    case EVENT_COND_UTENSIL_DUE_ROLLING_PIN:
        return event_flag_get(EVENT_FLAG_UTENSIL_DUE_ROLLING_PIN) == 1;
    case EVENT_COND_UTENSIL_DUE_OVEN:
        return event_flag_get(EVENT_FLAG_UTENSIL_DUE_OVEN) == 1;
    case EVENT_COND_UTENSIL_DUE_SEASONING_SET:
        return event_flag_get(EVENT_FLAG_UTENSIL_DUE_SEASONING_SET) == 1;
    case EVENT_COND_OFFER_9_DUE:
        return event_flag_get(EVENT_FLAG_OFFER_9_DUE) == 1;
    case EVENT_COND_DOG_ADULT:
    case EVENT_COND_DOG_ADULT_2:
        return ACTOR_ADULT_BIT(game_state.animals.dog.actor);
    case EVENT_COND_NEWBORN_READY_FOR_NAMING:
        if (event_cmd_dispatch(0xD, 1, 0, 0) == 1) {
            return 1;
        }
        if (event_cmd_dispatch(0xD, 1, 1, 0) == 1) {
            return 1;
        }
        if (event_cmd_dispatch(0xD, 1, 2, 0) == 1) {
            return 1;
        }
        return event_cmd_dispatch(0xD, 1, 3, 0) == 1;
    case EVENT_COND_POPURI_KAI_WEDDING_TODAY:
        return event_flag_get(EVENT_FLAG_POPURI_KAI_WEDDING_TODAY) == 1;
    case EVENT_COND_POPURI_KAI_WEDDING_TODAY_2:
        return event_flag_get(EVENT_FLAG_POPURI_KAI_WEDDING_TODAY) == 1;
    case EVENT_COND_ANN_CLIFF_WEDDING_TODAY:
        return event_flag_get(EVENT_FLAG_ANN_CLIFF_WEDDING_TODAY) == 1;
    case EVENT_COND_KAREN_RICK_WEDDING_TODAY:
        return event_flag_get(EVENT_FLAG_KAREN_RICK_WEDDING_TODAY) == 1;
    case EVENT_COND_ELLI_DOCTOR_WEDDING_TODAY:
        return event_flag_get(EVENT_FLAG_ELLI_DOCTOR_WEDDING_TODAY) == 1;
    case EVENT_COND_MARY_GRAY_WEDDING_TODAY:
        return event_flag_get(EVENT_FLAG_MARY_GRAY_WEDDING_TODAY) == 1;
    case EVENT_COND_NOT_BUILD_IN_PROGRESS:
        if (game_state.progress.estate.houseLevel != 0) {
            return 0;
        }
        return event_flag_get(EVENT_FLAG_BUILD_IN_PROGRESS) != 1;
    case EVENT_COND_FLAG_15F_OR_163_OR_164:
        if (event_flag_get(EVENT_FLAG_15F) == 0) {
            return 1;
        }
        if (event_flag_get(EVENT_FLAG_163) == 1) {
            return 1;
        }
        return event_flag_get(EVENT_FLAG_164) == 1;
    case EVENT_COND_NO_WEDDING_TODAY:
        if (event_flag_get(EVENT_FLAG_KAREN_RICK_WEDDING_TODAY) == 1) {
            return 0;
        }
        if (event_flag_get(EVENT_FLAG_ANN_CLIFF_WEDDING_TODAY) == 1) {
            return 0;
        }
        if (event_flag_get(EVENT_FLAG_ELLI_DOCTOR_WEDDING_TODAY) == 1) {
            return 0;
        }
        if (event_flag_get(EVENT_FLAG_MARY_GRAY_WEDDING_TODAY) == 1) {
            return 0;
        }
        return event_flag_get(EVENT_FLAG_POPURI_KAI_WEDDING_TODAY) != 1;
    case EVENT_COND_ANN_OFFER_UNDER_60000:
        if (wedding_offer_state(1) != WEDDING_OFFER_YES_VARIANT) {
            return 0;
        }
        COURTABLE_UNDER(CHARACTER_ANN, AFFECTION_HEART_6)
    case EVENT_COND_OFFER_4_YES:
        return wedding_offer_state(4) == WEDDING_OFFER_YES_VARIANT;
    case EVENT_COND_MARY_OFFER_UNDER_60000:
        if (wedding_offer_state(3) != WEDDING_OFFER_YES_VARIANT) {
            return 0;
        }
        COURTABLE_UNDER(CHARACTER_MARY, AFFECTION_HEART_6)
    case EVENT_COND_OFFER_0_YES:
        return wedding_offer_state(0) == WEDDING_OFFER_YES_VARIANT;
    case EVENT_COND_ELLI_OFFER_UNDER_60000:
        if (wedding_offer_state(2) != WEDDING_OFFER_YES_VARIANT) {
            return 0;
        }
        /* fall through */
    case EVENT_COND_ELLI_LOVE_UNDER_60000:
        COURTABLE_UNDER(CHARACTER_ELLI, AFFECTION_HEART_6)
    case EVENT_COND_HORSE_OWNED:
        return game_state.animals.horse.actor.kind == ITEM_HORSE;
    case EVENT_COND_PLACE_SEASON_DAY:
        switch (farm_area.mode.data->head.place.location) {
        case PLACE_2A:
            if (game_state.progress.clock.date.season == SEASON_SPRING &&
                (game_state.progress.clock.date.day == 7 || game_state.progress.clock.date.day == 17 || game_state.progress.clock.date.day == 21)) {
                return 1;
            }
            if (game_state.progress.clock.date.season == SEASON_SUMMER && (game_state.progress.clock.date.day == 6 || game_state.progress.clock.date.day == 11)) {
                return 1;
            }
            if (game_state.progress.clock.date.season == SEASON_FALL && game_state.progress.clock.date.day == 8) {
                return 1;
            }
            if (game_state.progress.clock.date.season == SEASON_WINTER && game_state.progress.clock.date.day == 9) {
                return 1;
            }
            break;
        case PLACE_2C:
            if (game_state.progress.clock.date.season == SEASON_SUMMER && game_state.progress.clock.date.day == 0) {
                return 1;
            }
            break;
        case PLACE_8:
            if (game_state.progress.clock.date.season == SEASON_SUMMER && game_state.progress.clock.date.day == 19) {
                return 1;
            }
            if (game_state.progress.clock.date.season == SEASON_FALL && game_state.progress.clock.date.day == 20) {
                return 1;
            }
            return 0;
        }
        return 0;
    case EVENT_COND_WIFE_REQUEST_DAY:
        if (clock_date_index(game_state.progress.clock.date.season, game_state.progress.clock.date.day) == 0) {
            return 0;
        }
        if (game_state.family.wifeRequests == 0) {
            return 0;
        }
        if (game_state.progress.clock.date.season != SEASON_SPRING || game_state.progress.clock.date.day != 7) {
            return 1;
        }
        return 0;
    case EVENT_COND_GOLD_500_ROD_NOT_TAKEABLE:
        if (game_state.player.gold >= GOLD_GOAL_500 && event_flag_get(EVENT_FLAG_E7) == 1 && event_item_can_take(ITEM_FISHING_ROD, 0, 2, 0) == 0) {
            return 1;
        }
        return 0;
    case EVENT_COND_TALK_VILLAGER_MARRIED: {
        u16 flag;

        switch (game_state.player.talkVillager) {
        case CHARACTER_KAREN:
            flag = EVENT_FLAG_KAREN_RICK_MARRIED;
            break;
        case CHARACTER_ANN:
            flag = EVENT_FLAG_ANN_CLIFF_MARRIED;
            break;
        case CHARACTER_ELLI:
            flag = EVENT_FLAG_ELLI_DOCTOR_MARRIED;
            break;
        case CHARACTER_MARY:
            flag = EVENT_FLAG_MARY_GRAY_MARRIED;
            break;
        case CHARACTER_POPURI:
            flag = EVENT_FLAG_POPURI_KAI_MARRIED;
            break;
        default:
            return 0;
        }
        return event_flag_get(flag);
    }
    case EVENT_COND_NOT_SINGLE:
        return event_flag_get(EVENT_FLAG_SINGLE) != 1;
    case EVENT_COND_ALWAYS_38:
    case EVENT_COND_ALWAYS_43:
    case EVENT_COND_ALWAYS_44:
    case EVENT_COND_ALWAYS_45:
    case EVENT_COND_ALWAYS_46:
    case EVENT_COND_ALWAYS_77:
    case EVENT_COND_ALWAYS_93:
    case EVENT_COND_ALWAYS_94:
    case EVENT_COND_ALWAYS_96:
        return 1;
    }
    return 0;
}

/* Clear one zone entry and set its facing to 4. */
static void farm_zone_entry_clear(FarmZoneEntry *entry) {
    entry->turnX = 0;
    entry->turnY = 0;
    entry->turnZ = 0;
    entry->swingSpeed = 0;
    entry->holdTimer = 0;
    entry->facing = 4;
    entry->id = FARM_ZONE_FREE;
    entry->zone = 0;
}

/* Clear every zone entry and the farm pairs. */
void farm_zones_reset(void) {
    u32 i;
    FarmZoneEntry *p = farm_zone_entries;
    for (i = 0; i < FARM_ZONE_ENTRY_COUNT; i++) {
        farm_zone_entry_clear(p);
        p++;
    }
    farm_pairs_reset();
}

/* Put the zone in the first free entry, closed. */
static void farm_zone_register(FarmZone *zone) {
    FarmZoneEntry *p = farm_zone_entries;
    u32 i = 0;
    do {
        if (p->id == FARM_ZONE_FREE) {
            farm_zone_entry_clear(p);
            p->id = FARM_ZONE_CLOSED;
            p->zone = zone;
            p->facing = zone->facing;
            return;
        }
        i++;
        p++;
    } while (i < FARM_ZONE_ENTRY_COUNT);
}

/* Register each of the script's zones that has an event. */
void farm_zones_register_script(FarmModeScript *data) {
    u32 i = 0;
    do {
        if (data->zones[i].event != ZONE_EVENT_NONE) farm_zone_register(&data->zones[i]);
        i++;
    } while (i < FARM_ZONE_COUNT);
}

/* Trigger the zone entry for `event`. A zone with an event goes back to closed; any other
 * closed, opening or closing zone starts to open (a closed one plays sound 0x16 first),
 * and a held-open zone extends its hold by 5. Returns 1 when an entry matched. */
s32 farm_zone_trigger(s16 event, u16 facing) {
    FarmZoneEntry *entry;
    u32 count;
    s32 id;

    entry = farm_zone_entries;
    for (count = 0; count < FARM_ZONE_ENTRY_COUNT; count++, entry++) {
        if (entry->zone->event != event) {
            continue;
        }
        id = entry->id;
        switch (id) {
        case FARM_ZONE_CLOSED:
            if (!(entry->zone->flags & ZONE_FLAG_HAS_EVENT)) sound_play(0x16);
            /* fall through */
        case FARM_ZONE_OPENING:
        case FARM_ZONE_CLOSING:
            if (entry->zone->flags & ZONE_FLAG_HAS_EVENT) {
                entry->id = FARM_ZONE_CLOSED;
                return 1;
            }
            entry->id = FARM_ZONE_OPENING;
            entry->facing = facing;
            return 1;
        case FARM_ZONE_HELD_OPEN:
            entry->holdTimer += 5;
            return 1;
        }
    }
    return 0;
}

/* The clock's event callbacks, in the order of clock_callbacks: the ledger roll, the
 * end-of-day sequence, the season and hour refreshes and the objects shown on a date. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include <memory.h>
#include "types.h"
#include "afile.h"
#include "animals.h"
#include "clock.h"
#include "clock_events.h"
#include "cue_history.h"
#include "estate.h"
#include "event_commands.h"
#include "event_flags.h"
#include "event_request.h"
#include "event_script.h"
#include "family.h"
#include "farm_init.h"
#include "farm_load.h"
#include "farm_main.h"
#include "farm_state.h"
#include "field.h"
#include "harvest_sprites.h"
#include "items.h"
#include "main.h"
#include "milestones.h"
#include "player.h"
#include "scene.h"
#include "scene_view.h"
#include "screen_fade.h"
#include "sound.h"
#include "sound_request.h"
#include "villager.h"
#include "weather.h"
/* The fade in and out of the day change, in frames. */
#define FADE_FRAMES 45
/* The sound fade in at the start of the morning, in frames. */
#define SOUND_FADE_FRAMES 10
/* The hour from which the morning message is shown. */
#define MORNING_HOUR 7
/* The hours of the noon and evening villager refresh. */
#define NOON_HOUR 12
#define EVENING_HOUR 18

s32 clock_callback_nothing(void)
{
    return 1;
}

/* Roll the ledger: the past seasons move one row down and the current season starts
 * at zero. */
s32 estate_ledger_roll(void)
{
    s32 i;
    for (i = LEDGER_COUNT - 2; i >= 0; i--)
    {
        game_state.progress.estate.ledger[i + 1] = game_state.progress.estate.ledger[i];
    }
    game_state.progress.estate.ledger[0].earnings = 0;
    game_state.progress.estate.ledger[0].expenses = 0;
    return 1;
}

/* The end-of-day sequence, one step per call: wait for the screen to fade, grow the
 * farm, change the season's scene, run the overnight updates, then hand control back.
 * Returns 1 once finished. */
s32 scene_overnight_step(void)
{
    s32 done;
    s32 previous;
    s32 offset;
    FarmLoadArea *far;
    FarmState *state;

    event_flag_set(EVENT_FLAG_EVENT_ACTIVE, 1);
    game_state.globalFlags |= GLOBAL_FLAG_PAUSED;
    input_pad1->pressed = 0;
    input_pad1->held = 0;
    input_pad1->triggered = 0;
    done = 0;
    switch (game_state.system.overnightStep)
    {
    case OVERNIGHT_FADE_OUT:
    {
        FarmState *farm = &game_state;
        if (farm->statusFlags & STATUS_FLAG_COLLAPSED)
        {
            /* Collapsed: skip the settle step, to OVERNIGHT_GROW_FARM. */
            farm->system.overnightStep += 2;
            break;
        }
        if (event_flag_get(EVENT_FLAG_OVERNIGHT) == 0)
        {
            if (fade_is_settled() != 1)
            {
                break;
            }
            if (fade_get_mode() != 2)
            {
                fade_start_out(FADE_FRAMES, 0, 2);
            }
        }
        farm->system.overnightStep++;
        break;
    }
    case OVERNIGHT_FADE_SETTLED:
        if (fade_is_settled() != 1)
        {
            break;
        }
        game_state.system.overnightStep++;
        game_state.globalFlags |= GLOBAL_FLAG_TRANSITION;
        break;
    case OVERNIGHT_GROW_FARM:
    {
        FarmState *farm = &game_state;
        event_flag_set(EVENT_FLAG_STORM_TODAY, farm->world.weather == WEATHER_STORM);
        field_daily_tick();
        farm_end_of_day();
        weather_step_today();
        weather_roll_tomorrow();
        farm->system.overnightStep++;
        break;
    }
    case OVERNIGHT_HOLD:
        game_state.system.overnightStep++;
        break;
    case OVERNIGHT_CHANGE_SCENE:
        if (fade_is_settled() != 1)
        {
            break;
        }
        player_morning_reset();
        farm_zones_reset();
        milestone_tick();
        far = &farm_area;
        state = &game_state;
        far->mode.absentArea = -1;
        previous = far->mode.area;
        offset = state->progress.estate.houseLevel * 4 + 0x50;
        far->mode.area = state->progress.clock.date.season + offset;
        scene_change_area(far->mode.area, 0x8000, previous == far->mode.area);
        family_flags_update();
        family_daily_update();
        farm_load_item_art(far->mode.data->head.resourceSet);
        farm_load_resource_id(far->mode.data->head.resourceId, 0);
        farm_load_resource(0);
        slots_clear();
        scene_morphs_clear();
        scene_entries_clear();
        scene_populate_objects();
        farm_load_item_art(far->mode.script.head.resourceSet);
        scene_mode_queue_run(&farm_area.mode.script);
        far->mode.place = far->mode.script.head.place;
        far->mode.data = &farm_area.mode.script;
        farm_zones_register_script(&farm_area.mode.script);
        house_door_position(&state->player.position);
        thrown_items_reset();
        state->progress.estate.estateFlags = (state->progress.estate.estateFlags | ESTATE_FLAG_CHICKEN_NOTICE) &
                                             ~(ESTATE_FLAG_VISITORS | ESTATE_FLAG_BIT16 | ESTATE_FLAG_BIT17 | ESTATE_FLAG_NOON_CHIME | ESTATE_FLAG_EVENING_CHIME);
        player_latch_place();
        rival_weddings_hold_due();
        farm_load_fixed_block();
        cue_history_reset(STAGING_BUFFER);
        farm_item_history_resume();
        villager_clear_delays();
        weather_fx_init();
        drift_reset();
        weather_fx_set_mode(state->world.weather);
        far->mode.eventMode = FARM_MODE_IDLE;
        state->system.overnightStep++;
        if (event_flag_get(EVENT_FLAG_48) != 0)
        {
            break;
        }
        farm_mode_change();
        while (sound_fader_ready() == 0)
        {
            sound_load_tick();
        }
        sound_update();
        farm_set_message_timing();
        break;
    case OVERNIGHT_UPDATES:
        estate_fatigue_tick();
        scene_message_timers_tick();
        harvest_helpers_tick_all();
        estate_record_tables_refresh();
        estate_feed_bins_clear();
        estate_queues_daily();
        clock_refresh_dated_event_flag();
        estate_basket_check();
        game_state.progress.estate.estateFlags &= ~ESTATE_FLAG_TAKE_ONCE;
        switch (game_state.world.weather)
        {
        case WEATHER_RAIN:
            field_reset_cells();
            field_table_refresh(0);
            break;
        case WEATHER_SNOW:
            field_table_refresh(1);
            break;
        case WEATHER_STORM:
        case WEATHER_BLIZZARD:
            field_storm_damage();
            greenhouse_reset_level();
            break;
        }
        game_state.system.overnightStep++;
        if (clock_is_pending() == 0)
        {
            clock_set_end_flag(0);
            farm_overnight_finish();
        }
        else
        {
            clock_set_end_flag(1);
        }
        break;
    case OVERNIGHT_FINISH:
        event_flag_set(EVENT_FLAG_EVENT_ACTIVE, 0);
        game_state.globalFlags &= ~GLOBAL_FLAG_PAUSED;
        done = 1;
        break;
    }
    return done;
}

/* Reload the current season's art and its estate selection, then clear the estate request. */
s32 estate_season_refresh(void)
{
    s16 season = game_state.progress.clock.date.season;
    farm_load_season_art(season);
    season = game_state.progress.clock.date.season;
    estate_selection_refresh(season);
    estate_request_clear();
    return 1;
}

/* The hourly work: clean up the records, run the harvest helpers and the player's status,
 * and clear the villager delays at noon and in the evening. */
int farm_hourly_refresh(void)
{
    s16 hour;
    FarmState *state;

    farm_cleanup_records();
    harvest_helpers_work_all();
    player_hourly_status_tick();
    state = &game_state;
    hour = state->progress.clock.time.hm.hour;
    if (hour == NOON_HOUR)
    {
        villager_clear_delays();
        hour = state->progress.clock.time.hm.hour;
    }
    if (hour == EVENING_HOUR)
        villager_clear_delays();
    return 1;
}

/* Drain the event request queue: step any busy request, queue the request 0x12D, run the
 * triggers, then step until the queue is free. */
void event_request_drain(void)
{
    FarmState *farm;
    s32 expected;

    if (event_flag_get(EVENT_FLAG_REQUEST_BUSY) == 1)
    {
        event_request_start();
        if (game_state.requests.state == FARM_REQUEST_FINISH)
        {
            event_request_step();
        }
    }
    event_request_force(0x12D, 0);
    event_triggers_run();
    event_request_step();
    if (event_flag_get(EVENT_FLAG_REQUEST_BUSY) == 1)
    {
        farm = &game_state;
        expected = 1;
        while (1)
        {
            if (farm->requests.state == expected)
            {
                event_request_step();
                break;
            }
            event_request_step();
            if (event_flag_get(EVENT_FLAG_REQUEST_BUSY) == 0)
            {
                break;
            }
        }
    }
}

/* A rest request collapses the player into sleep: the minute is recorded, the collapsed
 * status set, and the clock runs on to the morning. */
void farm_rest_request(void)
{
    if (event_flag_get(EVENT_FLAG_REST_REQUESTED) != 0)
    {
        FarmState *fs = &game_state;
        u32 flags = fs->statusFlags;
        u16 minuteOfDay = fs->progress.clock.minuteOfDay;
        fs->player.sleptAt = minuteOfDay;
        fs->statusFlags = flags | STATUS_FLAG_COLLAPSED;
        clock_sleep_until_5am(360);
        event_flag_set(EVENT_FLAG_REST_REQUESTED, 0);
    }
}

/* Finish the night. EVENT_FLAG_48 goes to the save screen and EVENT_FLAG_9D to the
 * staff roll. Otherwise the zone-2 request runs, the fade comes back in, and the morning
 * gift is given; GLOBAL_FLAG_TRANSITION is cleared last. */
void farm_overnight_finish(void)
{
    drift_select_hour();
    if (event_flag_get(EVENT_FLAG_48) == 1)
    {
        event_flag_set(EVENT_FLAG_48, 0);
        event_flag_set(EVENT_FLAG_OVERNIGHT, 0);
        main_request_scene(SCENE_SAVE);
        return;
    }
    if (event_flag_get(EVENT_FLAG_9D) == 1)
    {
        event_flag_set(EVENT_FLAG_OVERNIGHT, 0);
        event_flag_set(EVENT_FLAG_9D, 0);
        main_request_scene(SCENE_STAFF_ROLL);
        return;
    }
    if (event_flag_get(EVENT_FLAG_48) == 1)
    {
        return;
    }
    event_flag_set(EVENT_FLAG_OVERNIGHT, 0);
    if (farm_area.mode.place.location == PLACE_2)
    {
        event_trigger_mode_set(8);
        event_triggers_run();
        event_request_step();
        if (event_flag_get(EVENT_FLAG_REQUEST_BUSY) == 1)
        {
            while (1)
            {
                if (game_state.requests.state == FARM_REQUEST_RUN)
                {
                    event_request_step();
                    break;
                }
                event_request_step();
                if (event_flag_get(EVENT_FLAG_REQUEST_BUSY) == 0)
                {
                    break;
                }
            }
        }
    }
    if ((farm_area.mode.eventMode - FARM_MODE_TARGET_LOAD) >= 2)
    {
        fade_start_in(FADE_FRAMES, 0, 2);
        sound_fade_in(SOUND_FADE_FRAMES);
    }
    if (!(game_state.statusFlags & STATUS_FLAG_COLLAPSED) && event_flag_get(EVENT_FLAG_REQUEST_BUSY) == 0)
    {
        /* The morning's gift: one of the four sandwiches and buns. */
        player_receive_item(&game_state.player, rand_range(4) + ITEM_TRIANGLE_SANDWICH, RECEIVE_ITEM_HOLD);
        if (game_state.progress.clock.time.hm.hour >= MORNING_HOUR)
        {
            event_flag_set(EVENT_FLAG_MORNING_MESSAGE, 1);
        }
    }
    game_state.globalFlags &= ~GLOBAL_FLAG_TRANSITION;
}

s32 clock_show_object_4(void) {
    event_cmd_object_show(4, 0, 0, 0);
    return 1;
}

s32 clock_show_object_1(void) {
    event_cmd_object_show(1, 0, 0, 0);
    return 1;
}

s32 clock_show_object_2(void) {
    event_cmd_object_show(2, 0, 0, 0);
    return 1;
}

s32 clock_show_object_0(void) {
    event_cmd_object_show(0, 0, 0, 0);
    return 1;
}

/* Set status flag 1 while the current date is a dated event day. */
s32 clock_refresh_dated_event_flag(void) {
    if (clock_date_index(game_state.progress.clock.date.season, game_state.progress.clock.date.day)) {
        game_state.statusFlags |= 1;
    } else {
        game_state.statusFlags &= ~1;
    }
    return 1;
}

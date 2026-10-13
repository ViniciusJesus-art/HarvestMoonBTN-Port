/* Farm overlay: entry and start-up, the event step and the per-frame update. */

#include <memory.h>
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "types.h"
#include "animals.h"
#include "clock.h"
#include "clock_events.h"
#include "cpu_asm.h"
#include "estate.h"
#include "event_commands.h"
#include "event_flags.h"
#include "event_request.h"
#include "farm_data_size.h"
#include "farm_frame.h"
#include "farm_init.h"
#include "farm_load.h"
#include "farm_main.h"
#include "farm_state.h"
#include "farm_vars.h"
#include "field.h"
#include "field_item_requests.h"
#include "fishing.h"
#include "frame_buffers.h"
#include "greenhouse.h"
#include "items.h"
#include "loading_screen.h"
#include "main.h"
#include "messages.h"
#include "milestones.h"
#include "mine.h"
#include "model.h"
#include "player.h"
#include "scene.h"
#include "scene_view.h"
#include "screen_fade.h"
#include "sound.h"
#include "sound_request.h"
#include "villager.h"
#include "weather.h"

static void farm_event_step(void);
static void farm_advance_step(void);
static void farm_startup_step(void);
static s32 farm_track_id(void);
static void farm_frame_update(void);
static void farm_enter_none(void);

/* Entry-step handlers, indexed by progress.entryStep. */
static void (*farm_enter_handlers[8])(void) = {
    farm_advance_step, farm_startup_step, farm_mode_tick, farm_enter_none, farm_apply_mode_flags,
    farm_startup, farm_reenter, farm_enter_startup,
};

/* Run the handler the overlay was entered for. */
void farm_enter_dispatch(void) {
    farm_enter_handlers[game_state.progress.entryStep]();
}

/* Run farm_event_step on the scratchpad stack. */
void farm_run_on_scratchpad(void) {
    SCRATCHPAD_STACK_ENTER();
    farm_event_step();
    SCRATCHPAD_STACK_LEAVE();
}

u32 section_load_pending = 0;

/* Step the farm event state machine (farm_area.mode.eventMode). */
static void farm_event_step(void) {
    FarmZone *event;
    s32 id;

    switch (farm_area.mode.eventMode) {
    case FARM_MODE_DEMO_END:
        farm_area.mode.eventMode = FARM_MODE_TO_TITLE;
        fade_start_out(10, 0, 2);
        sound_fade_out(10);
        break;
    case FARM_MODE_TO_TITLE:
        if (fade_is_settled() == 1) {
            main_request_scene(SCENE_TITLE);
        }
        break;
    case FARM_MODE_IDLE:
        section_load_pending = 0;
        break;
    case FARM_MODE_ZONE_WALK:
        if ((game_state.player.stateFlags & PLAYER_FLAG_AUTO_WALK) == 0) {
            farm_area.mode.eventMode = FARM_MODE_ZONE_WALK_WAIT;
            if (current_zone->id != ZONE_ID_NO_RESET) {
                player_walk_start(20);
            } else {
                player_walk_start(27);
            }
        } else if (game_state.player.stateFlags & PLAYER_FLAG_SCRIPTED) {
            farm_area.mode.eventMode = FARM_MODE_EXIT_LOAD;
            game_state.player.stateFlags |= PLAYER_FLAG_SCENE_CHANGE;
            game_state.globalFlags |= GLOBAL_FLAG_PAUSED;
        }
        break;
    case FARM_MODE_ZONE_WALK_WAIT:
        if (game_state.player.stateFlags & PLAYER_FLAG_AUTO_WALK) {
            break;
        }
        if (current_zone == 0) {
            farm_area.mode.eventMode = FARM_MODE_IDLE;
            break;
        }
        if (current_zone->id == ZONE_ID_NO_RESET) {
            farm_area.mode.eventMode = FARM_MODE_IDLE;
            if (current_zone->event == 0x2E) {
                game_state.player.latches |= PLAYER_LATCH_JUMP_PENDING;
            }
            if (current_zone->event == 0x20 && current_zone->facing == FACING_UP) {
                game_state.player.latches |= PLAYER_LATCH_DOOR_ENTRY;
            }
        } else {
            farm_area.mode.eventMode = FARM_MODE_EXIT_LOAD;
            game_state.player.stateFlags |= PLAYER_FLAG_SCENE_CHANGE;
            game_state.globalFlags |= GLOBAL_FLAG_PAUSED;
        }
        break;
    case FARM_MODE_EXIT_LOAD:
        player_reset_pose();
        fade_start_blend(20);
        estate_trigger_fire();
        zone_event_pending = 0;
        if (current_zone->event != ZONE_EVENT_NONE && (current_zone->flags & ZONE_FLAG_HAS_EVENT) == 0) {
            zone_event_pending = 1;
        }
        event = current_zone;
        id = event->id;
        farm_area.mode.exit = event;
        farm_area.mode.eventMode = FARM_MODE_EXIT_ARRIVE;
        game_state.player.stateFlags &= ~PLAYER_FLAG_SCENE_CHANGE;
        farm_area.mode.transitionTicks = id;
        farm_section_id = scene_resolve_selection(id);
        farm_section_load_async(farm_section_id, 1);
        section_load_pending = 1;
        if (game_state.requests.state == FARM_REQUEST_FINISH) {
            event_request_step();
        }
        break;
    case FARM_MODE_TARGET_LOAD:
        player_reset_pose();
        farm_area.mode.exit = 0;
        farm_area.mode.eventMode = FARM_MODE_TARGET_ARRIVE;
        game_state.globalFlags |= GLOBAL_FLAG_PAUSED;
        farm_section_id = scene_resolve_selection(farm_area.mode.transitionTicks);
        farm_section_load_async(farm_section_id, 1);
        section_load_pending = 1;
        break;
    case FARM_MODE_ASLEEP:
    case FARM_MODE_OVERNIGHT_ASLEEP:
        if (fade_is_settled() == 1) {
            farm_area.mode.eventMode = FARM_MODE_IDLE;
        }
        break;
    case FARM_MODE_EXIT_ARRIVE:
    case FARM_MODE_TARGET_ARRIVE:
        game_state.globalFlags |= GLOBAL_FLAG_TRANSITION;
        if (section_load_pending != 0 && farm_section_load_async(farm_section_id, 0) == 0) {
            break;
        }
    case FARM_MODE_CYCLE_AREA:
    case FARM_MODE_SAVE_SCREEN:
    case FARM_MODE_LOAD_SCREEN:
    case FARM_MODE_PDA:
    case FARM_MODE_LIVESTOCK_TRADE:
    case FARM_MODE_MINIGAME_1:
    case FARM_MODE_MINIGAME_2:
    case FARM_MODE_MINIGAME_3:
    case FARM_MODE_MINIGAME_4:
    case FARM_MODE_MINIGAME_5:
    case FARM_MODE_STAFF_ROLL:
        game_state.globalFlags = (game_state.globalFlags | GLOBAL_FLAG_TRANSITION) & ~GLOBAL_FLAG_MENU_OPEN;
    case FARM_MODE_SLEEP:
    case FARM_MODE_OVERNIGHT:
        if (fade_is_settled() != 1) {
            break;
        }
        if (game_state.requests.state == FARM_REQUEST_FINISH) {
            event_request_step();
        }
        main_set_put_env(1);
        main_set_clear_screen(1);
        game_state.globalFlags &= ~GLOBAL_FLAG_TRANSITION;
        switch (farm_area.mode.eventMode) {
        case FARM_MODE_STAFF_ROLL:
            main_request_scene(SCENE_STAFF_ROLL);
            break;
        case FARM_MODE_SAVE_SCREEN:
            main_request_scene(SCENE_SAVE);
            break;
        case FARM_MODE_LOAD_SCREEN:
            main_request_scene(SCENE_LOAD);
            break;
        case FARM_MODE_PDA:
            main_request_scene(SCENE_PDA);
            break;
        case FARM_MODE_LIVESTOCK_TRADE:
            main_request_scene(SCENE_LIVESTOCK_TRADE);
            break;
        case FARM_MODE_MINIGAME_1:
            main_request_scene(SCENE_MG1);
            sound_seq_stop_first(0);
            break;
        case FARM_MODE_MINIGAME_2:
            main_request_scene(SCENE_MG2);
            break;
        case FARM_MODE_MINIGAME_3:
            main_request_scene(SCENE_MG3);
            break;
        case FARM_MODE_MINIGAME_4:
            main_request_scene(SCENE_MG4);
            break;
        case FARM_MODE_MINIGAME_5:
            main_request_scene(SCENE_MG5);
            break;
        case FARM_MODE_SLEEP:
        case FARM_MODE_OVERNIGHT:
            game_state.player.stateFlags |= PLAYER_FLAG_ASLEEP;
            farm_unpause();
            if (farm_area.mode.eventMode == FARM_MODE_OVERNIGHT) {
                farm_area.mode.eventMode = FARM_MODE_OVERNIGHT_ASLEEP;
                event_flag_set(EVENT_FLAG_9D, 1);
                event_flag_set(EVENT_FLAG_OVERNIGHT, 1);
            } else {
                farm_area.mode.eventMode = FARM_MODE_ASLEEP;
            }
            player_go_to_sleep();
            break;
        case FARM_MODE_EXIT_ARRIVE:
            if (zone_event_pending != 0) {
                sound_play(0x4C);
            }
        case FARM_MODE_TARGET_ARRIVE:
            farm_startup_phase = FARM_STARTUP_WAIT;
            game_state.player.stateFlags &= ~(PLAYER_FLAG_NO_INPUT | PLAYER_FLAG_SCENE_CHANGE);
            game_state.progress.entryStep--;
            farm_area.mode.area = farm_area.mode.transitionTicks;
            break;
        case FARM_MODE_CYCLE_AREA:
            farm_startup_phase = FARM_STARTUP_WAIT;
            game_state.progress.entryStep--;
            farm_area.mode.area++;
            if (farm_area.mode.area == 0x100) {
                farm_area.mode.area = 0;
            }
            if (event_flag_get(EVENT_FLAG_SWIND_MODEL_CHANGED) == 1) {
                farm_area.mode.area = farm_swind_size;
                event_flag_set(EVENT_FLAG_SWIND_MODEL_CHANGED, 0);
            }
            game_state.progress.clock.date.season = farm_area.mode.area - farm_area.mode.area / 4 * 4;
            estate_season_refresh();
            farm_area.mode.exit = 0;
            break;
        }
        break;
    case FARM_MODE_OVERVIEW:
        if (fade_is_settled() == 1) {
            farm_area.mode.view = FARM_VIEW_OVERVIEW;
            farm_area.mode.eventMode = FARM_MODE_IDLE;
        }
        break;
    case FARM_MODE_MG_DEBUG_SCREEN:
        if (fade_is_settled() == 1) {
            farm_area.mode.eventMode = FARM_MODE_IDLE;
            main_request_scene(SCENE_MG_DEBUG);
        }
        break;
    }
}

/* Leave the entry step, and start the start-up at its first phase. */
static void farm_advance_step(void) {
    farm_startup_phase = FARM_STARTUP_BRING_UP;
    farm_area.mode.view = FARM_VIEW_NORMAL;
    game_state.progress.entryStep++;
}

/* Keep stepping the event requests while one is busy; stop one step after
 * requests.state reads 1. */
static inline void drain_requests(void)
{
    if (event_flag_get(EVENT_FLAG_REQUEST_BUSY) == 1) {
        do {
            if (game_state.requests.state == FARM_REQUEST_RUN) {
                event_request_step();
                break;
            }
            event_request_step();
        } while (event_flag_get(EVENT_FLAG_REQUEST_BUSY) != 0);
    }
}

u32 farm_music_enabled = 0;

/* Farm overlay start-up, one step per call: farm_startup_phase is the step. */
static void farm_startup_step(void)
{
    switch (farm_startup_phase) {
    case FARM_STARTUP_BRING_UP:
        farm_bring_up();
    case FARM_STARTUP_PAUSE:
        game_state.globalFlags |= GLOBAL_FLAG_PAUSED;
        game_state.debugFlags |= DEBUG_FLAG_SCR_LIMIT;
        farm_startup_pause_hook();
        event_flag_set(EVENT_FLAG_EVENT_ACTIVE, 1);
        farm_startup_phase++;
        break;
    case FARM_STARTUP_PLAYER_SPRITE:
        farm_load_player_sprite();
        farm_startup_phase++;
        break;
    case FARM_STARTUP_SET_AREA:
        if (game_state.globalFlags & GLOBAL_FLAG_NEW_GAME) {
            farm_area.mode.area = 0;
        } else {
            farm_area.mode.area = game_state.progress.clock.date.season + AREA_HOUSE0_SPRING + game_state.progress.estate.houseLevel * 4;
        }
    case FARM_STARTUP_LAYOUT:
        if (sound_fader_ready() != 0) {
            farm_startup_phase++;
            frame_buffers_layout();
            camera_init_projection();
        }
        break;
    case FARM_STARTUP_FRAME:
    case FARM_STARTUP_FRAME_2:
        farm_frame_update();
        farm_startup_phase++;
        break;
    case FARM_STARTUP_MODE_CHANGE:
    case FARM_STARTUP_MODE_CHANGE_2:
        farm_music_enabled = 0;
        farm_mode_change();
        farm_startup_phase++;
        break;
    case FARM_STARTUP_FADER_WAIT:
    case FARM_STARTUP_FADER_WAIT_2:
        if (sound_fader_ready() == 0) {
            break;
        }
    case FARM_STARTUP_WAIT:
    case FARM_STARTUP_WAIT_2:
        farm_startup_phase++;
        break;
    case FARM_STARTUP_FINISH:
    case FARM_STARTUP_FINISH_2:
        loading_screen_stop();
        if (farm_music_enabled != 0 && (game_state.statusFlags & STATUS_FLAG_MUSIC_HOLD) == 0) {
            sound_update();
            sound_fade_in(10);
        }
        game_state.statusFlags &= ~STATUS_FLAG_MUSIC_HOLD;
        if (farm_area.mode.eventMode != FARM_MODE_TARGET_ARRIVE) {
            fade_start_in(10, 0, 2);
        }
        game_state.progress.entryStep++;
        if (farm_area.mode.eventMode == FARM_MODE_SLEEP) {
            house_door_position(&game_state.player.position);
            farm_area.mode.scroll.centre.vx = game_state.player.position.vx;
            farm_area.mode.scroll.centre.vy = game_state.player.position.vy;
            farm_area.mode.scroll.centre.vz = game_state.player.position.vz;
        }
        if ((game_state.globalFlags & GLOBAL_FLAG_NEW_GAME) == 0) {
            if (event_flag_get(EVENT_FLAG_REQUEST_BUSY) == 0) {
                event_flag_set(EVENT_FLAG_EVENT_ACTIVE, 0);
            }
        } else {
            event_flag_set(EVENT_FLAG_EVENT_ACTIVE, 1);
        }
        player_latch_place();
        farm_set_message_timing();
        farm_actions_clear(0);
        farm_spawn_daily_visitors();
        villager_clear_flag_bit4_all();
        estate_seasonal_refresh();
        item_requests_clear_all();
        if (game_state.statusFlags & STATUS_FLAG_STARTUP_PENDING) {
            game_state.statusFlags &= ~STATUS_FLAG_STARTUP_PENDING;
            if (farm_area.mode.place.location == PLACE_2) {
                event_trigger_mode_set(8);
                event_triggers_run();
                event_request_step();
                drain_requests();
            }
            if ((game_state.statusFlags & STATUS_FLAG_COLLAPSED) == 0 && event_flag_get(EVENT_FLAG_REQUEST_BUSY) == 0) {
                player_receive_item(&game_state.player, ITEM_TRIANGLE_SANDWICH + rand_range(4), RECEIVE_ITEM_HOLD);
            }
        } else {
            if (game_state.globalFlags & GLOBAL_FLAG_NEW_GAME) {
                /* the new game's intro */
                event_request_force(0x74, 1);
                game_state.globalFlags &= ~GLOBAL_FLAG_NEW_GAME;
            } else {
                event_trigger_mode_set(2);
            }
            event_triggers_run();
            event_request_step();
            drain_requests();
        }
        if (farm_area.mode.eventMode != FARM_MODE_TARGET_LOAD) {
            farm_area.mode.eventMode = FARM_MODE_IDLE;
            game_state.globalFlags &= ~GLOBAL_FLAG_PAUSED;
        }
        break;
    }
}

/* When the farm's track changes, fade out the playing song, request the new track and
 * clear the minigame-return flags. Music stays off while the music hold is set. */
void farm_mode_change(void) {
    s32 mode;
    s32 status;
    s32 value;

    mode = farm_track_id();
    if ((game_state.statusFlags & STATUS_FLAG_MUSIC_HOLD) == 0) {
        if (farm_area.mode.currentMode != mode) {
            sound_fade_out(20);
            while (sound_fader_mode() != SOUND_FADER_IDLE)
                sound_load_tick();
            sound_seq_stop_first(0);
            sound_update();
            sound_release_from(2, 0);
            value = farm_area.mode.data->head.sceneId;
            farm_area.mode.modeScene = value;
            sound_request((s16)mode, 0);
            farm_area.mode.currentMode = mode;
            if ((game_state.statusFlags & STATUS_FLAG_FROM_MG_ANY) != 0)
                farm_music_enabled = 0;
            else
                farm_music_enabled = 1;
            status = game_state.statusFlags;
            status &= ~STATUS_FLAG_FROM_MG1;
            status &= ~STATUS_FLAG_FROM_MG5;
            status &= ~STATUS_FLAG_FROM_MG2;
            status &= ~STATUS_FLAG_FROM_MG3;
            status &= ~STATUS_FLAG_FROM_MG4;
            game_state.statusFlags = status;
        }
    }
}

/* The fade-in timings the farm's music starts with. */
enum MusicFade {
    MUSIC_FADE_NONE = 0,
    MUSIC_FADE_SHORT = 80,
    MUSIC_FADE_FULL = 100,
};

/* Pick the timing of the farm's music fade-in, then start the song with it. Without music
 * enabled, only the volume of a song already playing is set. The song before is stopped
 * first. */
void farm_set_message_timing(void) {
    s32 mode;
    s16 timing;
    FarmZoneHeader *object;

    if (farm_music_enabled != 0) {
        sound_seq_stop_first(0);
    }
    timing = MUSIC_FADE_FULL;
    mode = clock_time_period(game_state.progress.clock.time.hm.hour);
    object = &farm_area.mode.data->head;
    if ((object->flags & AREA_FLAG_BIT12) != 0) {
        timing = MUSIC_FADE_SHORT;
    }
    if (object->place.value == PLACE_MINE) {
        timing = MUSIC_FADE_SHORT;
    }
    if (object->place.value == PLACE_MINE_43) {
        timing = MUSIC_FADE_SHORT;
    }
    if (mode == CLOCK_PERIOD_NIGHT) {
        if (object->place.value != PLACE_B && object->place.value != PLACE_33) {
            timing = timing / 2;
        }
        if (game_state.world.weather == WEATHER_RAIN &&
            (farm_area.mode.data->head.flags & AREA_FLAG_BIT12) == 0) {
            timing = MUSIC_FADE_FULL;
        }
        if (farm_area.mode.data->head.place.location == PLACE_F) {
            timing = MUSIC_FADE_NONE;
        }
        if (game_state.progress.clock.date.season == SEASON_WINTER) {
            timing = MUSIC_FADE_NONE;
        }
    }
    if (farm_area.mode.data->head.place.location == PLACE_28) {
        timing = MUSIC_FADE_FULL;
    }
    if (farm_music_enabled != 0) {
        sound_seq_start_reverb(0, 0, farm_area.mode.currentMode, timing);
    } else {
        sound_seq_volume_if_idle(0, 0, 0, timing, 10);
    }
}

/* The farm's songs, as the song ids sound_request takes. A zone whose scene id is
 * 0x1F to 0x24 plays the song with that same number. */
enum FarmTrack {
    FARM_TRACK_AFTER_MINIGAME_1_2 = 0xF,    /* back from the horse race or the cockfight */
    FARM_TRACK_AFTER_MINIGAME_3_4_5 = 0xC,  /* back from the tomato, the dog or the swimming race */
    FARM_TRACK_RAIN = 0x16,
    FARM_TRACK_STORM = 0x17,                /* a storm or a blizzard */
    FARM_TRACK_DAY = 0x1B,
    FARM_TRACK_NIGHT = 0x1C,
    FARM_TRACK_NEW_GAME = 0x1F,             /* the farm of a new game, until the intro is over */
};

/* Pick the track for the current farm state: event flags first, then the zone's
 * kind, the weather and the time of day. */
static s32 farm_track_id(void) {
    u32 flags;
    s16 kind;
    u16 mode;
    u16 id;

    flags = game_state.statusFlags;
    if (flags & STATUS_FLAG_FROM_MG1) {
        return FARM_TRACK_AFTER_MINIGAME_1_2;
    }
    if (flags & STATUS_FLAG_FROM_MG5) {
        return FARM_TRACK_AFTER_MINIGAME_3_4_5;
    }
    if (flags & STATUS_FLAG_FROM_MG2) {
        return FARM_TRACK_AFTER_MINIGAME_1_2;
    }
    if (flags & STATUS_FLAG_FROM_MG3) {
        return FARM_TRACK_AFTER_MINIGAME_3_4_5;
    }
    if (flags & STATUS_FLAG_FROM_MG4) {
        return FARM_TRACK_AFTER_MINIGAME_3_4_5;
    }
    if (game_state.globalFlags & GLOBAL_FLAG_NEW_GAME) {
        return FARM_TRACK_NEW_GAME;
    }
    kind = farm_area.mode.data->head.sceneId;
    if (kind == 0x1D) {
        return 0x1D;
    }
    if (kind == 0xE) {
        return 0xE;
    }
    mode = game_state.world.weather;
    if (mode == WEATHER_STORM || mode == WEATHER_BLIZZARD) {
        return FARM_TRACK_STORM;
    }
    if ((s16)mode == WEATHER_RAIN) {
        return FARM_TRACK_RAIN;
    }
    if ((s16)mode == WEATHER_SNOW) {
        return game_state.progress.clock.date.season + 1;
    }
    if (kind == 0x18) {
        if (clock_time_period(game_state.progress.clock.time.hm.hour) == CLOCK_PERIOD_NIGHT) {
            return 0x18;
        }
        return 0x19;
    }
    if (clock_time_period(game_state.progress.clock.time.hm.hour) == CLOCK_PERIOD_NIGHT) {
        return FARM_TRACK_NIGHT;
    }
    if (farm_area.mode.data->head.sceneId == SCENE_NONE && game_state.world.weather == WEATHER_SUNNY) {
        return game_state.progress.clock.date.season + 1;
    }
    id = farm_area.mode.data->head.sceneId;
    if (id >= 0x1F && id <= 0x24) {
        return (s16)id;
    }
    return FARM_TRACK_DAY;
}

/* Per-frame farm update: run the farm subsystems in order, the zone probe
 * sweeps included, then clear the per-frame state. */
static void farm_frame_update(void) {
    scene_set_projection();
    scene_set_lights();
    farm_scroll_reset();
    slots_clear();
    farm_zones_reset();
    scroll_clear_centre();
    if (farm_startup_phase == FARM_STARTUP_WAIT_2) {
        scene_enter(0, 0);
    } else {
        scene_enter(1, section_load_pending);
    }
    player_stop_motion();
    scene_morphs_clear();
    scene_entries_clear();
    scene_populate_objects();
    message_state_reset();
    if (farm_area.mode.place.location == PLACE_FARM) {
        SCRATCHPAD_STACK_ENTER();
        field_probe_sweep(0);
        picture_cells_tick();
        SCRATCHPAD_STACK_LEAVE();
    }
    if (farm_area.mode.place.location == PLACE_GREENHOUSE) {
        SCRATCHPAD_STACK_ENTER();
        greenhouse_probe_sweep(0);
        greenhouse_update_cells();
        SCRATCHPAD_STACK_LEAVE();
    }
    if ((farm_area.mode.place.value == PLACE_MINE || farm_area.mode.place.value == PLACE_MINE_43) &&
        farm_area.mode.place.location != PLACE_42) {
        mine_probe_grid(0);
    }
    if (farm_area.mode.place.location == PLACE_C || farm_area.mode.place.location == PLACE_D) {
        mine_clear_grid();
    }
    event_flag_set(EVENT_FLAG_45, 0);
    milestone_refresh_all();
    weather_frame_update_hook();
    game_state.player.currentAction = 0;
    section_load_pending = 0;
    player_init_toys(&game_state.player);
    player_reset_trail(&game_state.player);
    event_flag_set(EVENT_FLAG_SMALL_FISH_PLACED, 0);
    fishing_streak = 0;
    game_state.player.latches &= ~PLAYER_LATCH_MARKER;
}

/* The entry step that does nothing. */
static void farm_enter_none(void) {
}

/* Clear the pause and menu flags, and latch the player's unpause. */
void farm_unpause(void) {
    game_state.globalFlags &= ~(GLOBAL_FLAG_PAUSED | GLOBAL_FLAG_MENU_OPEN);
    game_state.player.latches |= PLAYER_LATCH_UNPAUSE;
}

/* The farm's frame: the per-frame mode switch and the subsystems it runs, in order, and
 * the one-shot status latch. */

#include <sys/types.h>
#include <abs.h>
#include <memory.h>
#include <libgte.h>
#include "types.h"
#include "animals.h"
#include "cd_audio.h"
#include "clock.h"
#include "clock_events.h"
#include "cpu_asm.h"
#include "doors.h"
#include "estate.h"
#include "event_commands.h"
#include "event_flags.h"
#include "event_request.h"
#include "event_script.h"
#include "farm_frame.h"
#include "farm_main.h"
#include "farm_probe.h"
#include "farm_state.h"
#include "field_item_requests.h"
#include "fishing.h"
#include "harvest_sprites.h"
#include "menu_window.h"
#include "menus.h"
#include "messages.h"
#include "mine.h"
#include "model.h"
#include "photo_album.h"
#include "player.h"
#include "scene.h"
#include "screen_fade.h"
#include "shop_dialogues.h"
#include "status_pages.h"
#include "villager.h"
#include "weather.h"
#include "scene_view.h"

static s32 zone_action_result;
static u32 player_unused_80128B64;

/* The per-frame mode switch: the normal view runs the farm's subsystems and applies
 * the pending mode, the overview view runs its camera and draws, the idle views do nothing. */
void farm_mode_tick(void) {
    s32 mode;
    u32 eventMode;
    u32 flags;

    if (game_state.globalFlags & (GLOBAL_FLAG_DEMO_START | GLOBAL_FLAG_INPUT_REPLAY)) {
        rand_seed = 1;
    }
    mode = farm_area.mode.view;
    switch (mode) {
    case FARM_VIEW_NORMAL:
        farm_one_shot_latch_set();
        eventMode = farm_area.mode.eventMode;
        if (eventMode != FARM_MODE_EXIT_ARRIVE && eventMode != FARM_MODE_TARGET_LOAD && eventMode != FARM_MODE_TARGET_ARRIVE) {
            farm_run_frame_subsystems();
        }
        farm_run_on_scratchpad();
        farm_apply_pending_mode();
        cd_audio_fade_tick();
        break;
    case FARM_VIEW_IDLE_1:
    case FARM_VIEW_IDLE_2:
        break;
    case FARM_VIEW_OVERVIEW:
        farm_overview_camera();
        scroll_camera_rotate();
        scroll_camera_apply();
        model_load_lighting();
        if (!(game_state.globalFlags & GLOBAL_FLAG_TRANSITION)) {
            scene_spin_entries();
            scroll_camera_draw();
        }
        break;
    }
    flags = game_state.globalFlags;
    if (flags & GLOBAL_FLAG_TRANSITION_END) {
        game_state.globalFlags = flags & ~(GLOBAL_FLAG_TRANSITION | GLOBAL_FLAG_TRANSITION_END);
    }
    if (game_state.globalFlags & GLOBAL_FLAG_TRANSITION_BEGIN) {
        flags = game_state.globalFlags;
        game_state.globalFlags = (flags & ~GLOBAL_FLAG_TRANSITION_BEGIN) | GLOBAL_FLAG_TRANSITION;
    }
}

/* Run one frame of the farm's subsystems, in order. */
void farm_run_frame_subsystems(void)
{
    zone_action_result = 2;
    clock_frame_update();
    clock_tick_minute();
    farm_time_event_tick();

    SCRATCHPAD_STACK_ENTER();
    farm_callback_reset();
    weather_fx_frame();
    item_storage_reset();
    mine_update_transition();
    scroll_camera_rotate();
    player_frame_update();
    message_machine_step();
    ending_track_tick();
    if (farm_area.mode.eventMode == FARM_MODE_IDLE)
    {
        zone_action_result = player_zone_action(&game_state.player);
    }
    SCRATCHPAD_STACK_LEAVE();

    SCRATCHPAD_STACK_ENTER();
    if (zone_action_result == 2)
    {
        player_update_position();
    }
    else
    {
        game_state.player.velocity[0] = 0;
        game_state.player.velocity[2] = 0;
        farm_area.mode.scroll.dx = 0;
        farm_area.mode.scroll.dy = 0;
    }
    slots_step();
    door_entries_update();
    weather_scroll_nudge(1);
    farm_overview_drift();
    SCRATCHPAD_STACK_LEAVE();

    SCRATCHPAD_STACK_ENTER();
    scroll_camera_apply();
    model_load_lighting();
    SCRATCHPAD_STACK_LEAVE();

    if (!(game_state.globalFlags & GLOBAL_FLAG_TRANSITION))
    {
        scene_spin_entries();
        scroll_camera_draw();
        SCRATCHPAD_STACK_ENTER();
        player_draw(&game_state.player.position, 1);
        thrown_items_update();
        scene_daily_announce();
        SCRATCHPAD_STACK_LEAVE();
    }

    SCRATCHPAD_STACK_ENTER();
    event_triggers_run();
    event_request_step();
    player_records_tick();
    harvest_helpers_update();
    farm_animals_tick();
    shop_phase_run();
    estate_items_refresh();
    item_requests_tick_all();
    estate_pending_op_tick();
    menu_run_state();
    farm_rest_request();
    SCRATCHPAD_STACK_LEAVE();
}

/* Decide the one-shot status latch from the story flags, or else from the saved option.
 * Nothing happens while the menu is open. */
void farm_one_shot_latch_set(void)
{
    if (clock_is_pending() == 1)
    {
        status_latch_reset();
        return;
    }
    if (game_state.globalFlags & GLOBAL_FLAG_MENU_OPEN)
    {
        return;
    }
    if (event_flag_get(EVENT_FLAG_REQUEST_BUSY) == 1)
    {
        if (((farm_area.mode.data->head.flags & AREA_FLAG_BIT12) != 0) && (event_flag_get(EVENT_FLAG_B0) == 1))
        {
            status_latch_reset();
            return;
        }
        if ((farm_area.mode.data->head.flags & AREA_FLAG_BIT12) == 0)
        {
            if (event_flag_get(EVENT_FLAG_CUE_LATCH_RESET) == 1)
            {
                status_latch_reset();
                return;
            }
            if (event_flag_get(EVENT_FLAG_CUE_LATCH_SET) == 1)
            {
                status_latch_set();
                return;
            }
        }
    }
    if (game_state.world.options.statusLatch != 0)
    {
        status_latch_set();
    }
    else
    {
        status_latch_reset();
    }
}

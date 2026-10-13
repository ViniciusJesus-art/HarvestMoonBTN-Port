/* Entry point, the frame loop, and the frame records: the ordering tables and
 * primitive buffers that each frame is drawn into. */

#include <memory.h>
#include <sys/types.h>
#include <libetc.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include <libcd.h>
#include <libapi.h>
#include <libmcrd.h>
#include <libmcx.h>
#include "types.h"
#include "afile.h"
#include "camera.h"
#include "debug.h"
#include "farm_data_size.h"
#include "farm_main.h"
#include "farm_state.h"
#include "farm_tables.h"
#include "frame_buffers.h"
#include "game_types.h"
#include "loading_screen.h"
#include "main.h"
#include "mcard.h"
#include "mg1.h"
#include "mg2.h"
#include "mg3.h"
#include "mg4.h"
#include "mg5.h"
#include "overlay_regions.h"
#include "pad.h"
#include "pad_vibration.h"
#include "pdasamp.h"
#include "saveload.h"
#include "screen_fade.h"
#include "sound.h"
#include "sound_request.h"
#include "staff.h"
#include "swind.h"
#include "system.h"
#include "text.h"
#include "title.h"

/* Read one overlay from A_FILE into its region. */
#define LOAD_OVERLAY(member, region) \
    afile_read_range((member), (member) + 1, overlay_region_table[region], overlay_region_table[region])

/* Offsets within the region at region_base where each frame's pieces start: the
 * tag arrays of its three ordering tables (64, 16 and 2 entries), the main OT,
 * the primitive buffer and the work area. */
#define WORK_REGION ((u8 *)0x801BE1B0)
#define REGION_TAGS1 0x200
#define REGION_TAGS2 0x280
#define REGION_WORK 0x33090
#define REGION_BYTES 0x3EE50
/* The active ordering-table block starts here; each frame index moves it by 0x3000. */
#define REGION_FRAME_OT_BASE 0x38E50

static void main_run_frame(void);
static void main_draw_ots(void);
static void main_set_debug_mode(s32 scene);
static void main_set_speed_mode(unsigned char mode);
static void main_frame_begin(void);
static void main_frame_reset(int index);
static void main_init_frame_records(void);
static void main_init_frame_records_wide(void);
static void main_copy_prim_buffer(s32 enabled, s32 frame_count);

s32 prim_copy_frame_count;
void (*scene_frame_handler)(void);
void (*vsync_pending_callback)(void);
s32 vsync_before_draw;
s32 vsync_after_draw;
s32 main_previous_scene;
s32 prim_copy_pending;
s32 main_scene;
/* The main loop runs while this is 1; main_request_scene clears it to end the loop. */
s32 main_loop_running;
s32 frame_index;
InputRecord *input_pad1;
InputRecord *input_pad2;
u8 *region_base;
u_long *active_ot;
u8 *frame_ot_base_init;
u8 *frame_ot_base;
FrameState *current_frame;

/* Two ordering tables of two entries each (the active one is active_ot), then the
 * two frame records and the controller input records. */
static u_long frame_ots[4];
FrameState frame_records[2];
VabStream vab_stream_copy;
InputRecord input_records[2];

static const Func frame_reset_funcs[] = {
    main_init_frame_records,
    main_init_frame_records_wide,
};

/* Run the pending VSync callback, poll the A_FILE reads and draw the loading screen. */
static void vsync_frame_callback(void) {
    if (vsync_pending_callback != 0) {
        vsync_pending_callback();
        vsync_pending_callback = 0;
    }
    afile_poll_range();
    loading_screen_draw();
}

u8 main_speed_mode = MAIN_SPEED_FAST_2;
s32 main_put_env_enabled = 1;
s32 main_clear_screen_enabled = 1;
s32 main_unused_8005E280 = 0;
s32 main_frame_started = 1;
s32 scene_overlay_loaded = 0;

/* The entry point. It brings up the libraries, VRAM, both frame buffers and the
 * GTE, installs the VSync handler, then runs the main loop. Each pass loads the
 * overlay for the requested scene (main_scene), installs its per-frame handler
 * (scene_frame_handler), and runs one frame (main_run_frame). */
int main(void) {
    u_long *env;
    FrameState *fb;

    input_pad1 = input_records;
    input_pad2 = &input_records[1];
    vsync_pending_callback = 0;
    ResetCallback();
    CdInit();
    SetMem(2);
    VSync(3);
    VSync(3);
    VSync(3);
    VSync(3);
    VSync(3);
    VSync(3);
    VSync(3);
    VSync(3);
    VSync(3);
    VSync(3);
    screen_clear_rect(0, 0, 640, 480, 0, 0, 0);
    while (DrawSync(1) != 0) {
    }
    sound_fader_init();
    sound_init();
    SetVideoMode(0);
    ResetGraph(0);
    main_scene = SCENE_TITLE;
    afile_init();

    fb = frame_records;
    SetDefDrawEnv(&fb[0].drawEnv, 0, 0, 320, 240);
    SetDefDrawEnv(&fb[1].drawEnv, 0, 240, 320, 240);
    SetDefDispEnv(&fb[0].dispEnv, 0, 240, 320, 240);
    SetDefDispEnv(&fb[1].dispEnv, 0, 0, 320, 240);
    fb[0].drawEnv.ofs[0] = 0;
    fb[0].drawEnv.ofs[1] = 0;
    fb[1].drawEnv.ofs[0] = 0;
    fb[1].drawEnv.ofs[1] = 240;
    fb[1].dispEnv.screen.x = 0;
    fb[0].dispEnv.screen.x = 0;
    fb[1].dispEnv.screen.y = 0;
    fb[0].dispEnv.screen.y = 0;
    SetGraphDebug(0);
    VSync(3);
    VSync(3);
    InitGeom();
    SetGeomOffset(0, 0);
    SetGeomScreen(1000);
    GsInitGraph(320, 240, 0, 0, 0);
    GsDefDispBuff(0, 0, 0, 0);
    GsSetOffset(0, 0);
    GsSetDrawBuffOffset();
    GsInit3D();
    MemCardInit(0);
    MemCardStart();
    McxStartCom();
    pad_init();
    pad_reset_records();
    screen_clear_rect(0, 0, 1023, 511, 0xFF, 0xFF, 0xFF);
    VSyncCallback(vsync_frame_callback);
    current_frame = &fb[1];
    frame_index = current_frame != frame_records;
    env = frame_ots;
    if (active_ot == env) {
        env += 2;
    }
    active_ot = env;
    main_frame_reset(0);
    main_frame_begin();
    camera_init();
    frame_buffers_layout();
    farm_state_init();
    sound_init_stereo_xa();
    sound_update();
    text_set_palette(3);
    text_set_speed(3);
    prim_copy_pending = 0;
    while(1) {
        if (prim_copy_pending == 1) {
            main_copy_prim_buffer(1, 0);
        }
        main_frame_reset(0);
        if (main_clear_screen_enabled == 1) {
            screen_clear_rect(0, 0, 320, 480, 0, 0, 0);
        }
        pad_set_repeat(PAD_REPEAT_DELAY, PAD_REPEAT_RATE);
        pad_set_repeat2(PAD_REPEAT_DELAY, PAD_REPEAT_RATE);
        main_frame_started = 1;
        switch (main_scene) {
        case SCENE_DEBUG:
            main_set_debug_mode(1);
            LOAD_OVERLAY(AFILE_DEBUG, OVERLAY_LOW);
            scene_frame_handler = debug_tick;
            scene_overlay_loaded = 0;
            break;
        case SCENE_PDA_SAMPLE:
            main_set_debug_mode(0);
            LOAD_OVERLAY(AFILE_PDA_SAMPLE, OVERLAY_LOW);
            scene_overlay_loaded = 0;
            scene_frame_handler = (void (*)(void))pda_sample_run;
            break;
        case SCENE_FARM:
            if (scene_overlay_loaded == 0) {
                loading_screen_init();
                main_set_debug_mode(0);
                LOAD_OVERLAY(AFILE_FARM, OVERLAY_LOW);
                scene_overlay_loaded = 1;
                farm_entry_args_clear();
            }
            scene_frame_handler = farm_enter_dispatch;
            break;
        case SCENE_MG1:
            main_frame_reset(1);
            main_set_debug_mode(0);
            LOAD_OVERLAY(AFILE_MG1, OVERLAY_LOW);
            scene_frame_handler = mg1_run_state;
            scene_overlay_loaded = 0;
            break;
        case SCENE_MG2:
            main_set_debug_mode(0);
            LOAD_OVERLAY(AFILE_MG2, OVERLAY_LOW);
            scene_frame_handler = mg2_phase_run;
            scene_overlay_loaded = 0;
            break;
        case SCENE_MG3:
            main_set_debug_mode(0);
            LOAD_OVERLAY(AFILE_MG3, OVERLAY_LOW);
            scene_frame_handler = mg3_step_dispatch;
            scene_overlay_loaded = 0;
            break;
        case SCENE_FARM_MINIGAME_EXIT:
            if (scene_overlay_loaded == 0) {
                main_set_debug_mode(0);
                LOAD_OVERLAY(AFILE_FARM, OVERLAY_LOW);
                scene_overlay_loaded = 1;
            }
            main_set_put_env(1);
            game_state.progress.entryStep = 4;
            scene_frame_handler = farm_enter_dispatch;
            break;
        case SCENE_SAVE:
            main_frame_reset(1);
            main_copy_prim_buffer(0, 1);
            main_set_debug_mode(0);
            LOAD_OVERLAY(AFILE_SAVELOAD, OVERLAY_HIGH);
            game_state.progress.entrySubStep = 0;
            scene_frame_handler = save_screen_tick;
            break;
        case SCENE_LOAD:
            main_frame_reset(1);
            main_copy_prim_buffer(0, 1);
            main_set_debug_mode(0);
            LOAD_OVERLAY(AFILE_SAVELOAD, OVERLAY_HIGH);
            game_state.progress.entrySubStep = 0;
            scene_frame_handler = load_screen_tick;
            break;
        case SCENE_LIVESTOCK_TRADE:
            main_frame_reset(1);
            main_copy_prim_buffer(0, 1);
            main_set_debug_mode(0);
            LOAD_OVERLAY(AFILE_SAVELOAD, OVERLAY_HIGH);
            game_state.progress.entrySubStep = 0;
            scene_frame_handler = save_transfer_screen_tick;
            break;
        case SCENE_MG4:
            main_frame_reset(1);
            main_set_debug_mode(0);
            LOAD_OVERLAY(AFILE_MG4, OVERLAY_LOW);
            scene_frame_handler = mg4_run_step;
            scene_overlay_loaded = 0;
            break;
        case SCENE_MG5:
            main_set_debug_mode(0);
            LOAD_OVERLAY(AFILE_MG5, OVERLAY_LOW);
            scene_frame_handler = mg5_step_run;
            scene_overlay_loaded = 0;
            break;
        case SCENE_TITLE:
            main_set_debug_mode(0);
            LOAD_OVERLAY(AFILE_TITLE, OVERLAY_LOW);
            farm_entry_args_clear();
            scene_frame_handler = title_step_dispatch;
            scene_overlay_loaded = 0;
            break;
        case SCENE_FARM_LOADED:
            if (scene_overlay_loaded == 0) {
                loading_screen_init();
                main_set_debug_mode(0);
                LOAD_OVERLAY(AFILE_FARM, OVERLAY_LOW);
                scene_overlay_loaded = 1;
            }
            game_state.progress.entryStep = 5;
            scene_frame_handler = farm_enter_dispatch;
            break;
        case SCENE_FARM_SAVED:
            if (scene_overlay_loaded == 0) {
                main_set_debug_mode(0);
                LOAD_OVERLAY(AFILE_FARM, OVERLAY_HIGH);
                scene_overlay_loaded = 1;
            }
            game_state.progress.entryStep = 6;
            scene_frame_handler = farm_enter_dispatch;
            break;
        case SCENE_STAFF_ROLL:
            main_set_debug_mode(0);
            LOAD_OVERLAY(AFILE_STAFF, OVERLAY_LOW);
            farm_entry_args_clear();
            scene_overlay_loaded = 0;
            scene_frame_handler = staff_tick;
            break;
        case SCENE_MG_DEBUG:
            if (scene_overlay_loaded == 0) {
                LOAD_OVERLAY(AFILE_FARM, OVERLAY_LOW);
                scene_overlay_loaded = 1;
                loading_screen_stop();
            }
            main_frame_reset(1);
            main_copy_prim_buffer(0, 0);
            LOAD_OVERLAY(AFILE_SWIND, OVERLAY_HIGH);
            scene_frame_handler = swind_window_tick;
            break;
        case SCENE_MEMCARD:
            main_set_debug_mode(0);
            LOAD_OVERLAY(AFILE_MCARD, OVERLAY_LOW);
            farm_entry_args_clear();
            scene_frame_handler = mcard_screen_tick;
            scene_overlay_loaded = 0;
            break;
        case SCENE_FARM_MENU_RETURN:
            if (scene_overlay_loaded == 0) {
                main_set_debug_mode(0);
                LOAD_OVERLAY(AFILE_FARM, OVERLAY_LOW);
                scene_overlay_loaded = 1;
                game_state.progress.entryStep = 4;
            } else {
                sound_fade_in(10);
                fade_start_in(10, 0, 2);
                farm_area.mode.eventMode = FARM_MODE_IDLE;
            }
            scene_frame_handler = farm_enter_dispatch;
            break;
        case SCENE_FARM_STAFF_EXIT:
            main_set_debug_mode(0);
            LOAD_OVERLAY(AFILE_FARM, OVERLAY_LOW);
            scene_overlay_loaded = 1;
            farm_entry_args_clear();
            game_state.progress.entryStep = 7;
            scene_frame_handler = farm_enter_dispatch;
            break;
        }
        main_run_frame();
    }
}

/* One pass of the frame loop. While the loading screen is up only the scene handler runs. Otherwise
 * the other frame record becomes current, the scene draws into it, and its ordering tables are
 * drawn at the next VSync (main_draw_ots). */
static void main_run_frame(void) {
    FrameState *fb;
    u_long *env;

    vsync_after_draw = 0;
    vsync_before_draw = 0;
    main_loop_running = 1;
    main_set_speed_mode(MAIN_SPEED_FAST_2);
    while (DrawSync(1) != 0) {
    }
    if (main_loop_running != 1) {
        return;
    }
    do {
        rand_range(65535);
        pad_vibration_tick();
        pad_poll_ports();
        sound_load_tick();
        if (loading_screen_is_active() == 1) {
            scene_frame_handler();
            DrawSync(0);
            VSync(2);
        } else {
            display_on();
            fb = frame_records;
            if (current_frame == frame_records) {
                fb = &frame_records[1];
            }
            env = frame_ots;
            current_frame = fb;
            frame_index = (fb != frame_records);
            if (active_ot == frame_ots) {
                env = frame_ots + 2;
            }
            active_ot = env;
            main_frame_begin();
            screen_fade_frame();
            screen_fade_step();
            scene_frame_handler();
            game_state.frameCount += 1;
            vsync_before_draw = VSync(1);
            DrawSync(0);
            vsync_after_draw = VSync(1);
            vsync_pending_callback = main_draw_ots;
            VSync(2);
        }
    } while (main_loop_running == 1);
}

s32 main_put_env_enabled;

/* Put the display and draw envs of the current frame, then draw its ordering tables in turn. If
 * main_put_env_enabled is 0, only the active table is drawn. */
static void main_draw_ots(void) {
    if (main_put_env_enabled == 1) {
        PutDispEnv(&current_frame->dispEnv);
        PutDrawEnv(&current_frame->drawEnv);
    } else {
        DrawOTag(active_ot + 1);
        return;
    }
    if (current_frame->otTags[3] != 0) {
        DrawOTag(current_frame->otTags[3] + current_frame->otTagCounts[3] - 1);
    }
    if (current_frame->otTags[2] != 0) {
        DrawOTag(current_frame->otTags[2] + current_frame->otTagCounts[2] - 1);
    }
    DrawOTag(&current_frame->ot[FRAME_OT_DEPTH - 1]);
    if (current_frame->otTags[1] != 0) {
        DrawOTag(current_frame->otTags[1] + current_frame->otTagCounts[1] - 1);
    }
    if (current_frame->otTags[0] != 0) {
        DrawOTag(current_frame->otTags[0] + current_frame->otTagCounts[0] - 1);
    }
    DrawOTag(active_ot + 1);
}

/* Does nothing: the scene argument is ignored. */
static void main_set_debug_mode(s32 scene) {
}

/* Make `scene` the next scene and end the main loop. */
void main_request_scene(s32 scene) {
    s32 previous = main_scene;
    main_loop_running = 0;
    main_scene = scene;
    main_previous_scene = previous;
}

s32 main_get_previous_scene(void) {
    return main_previous_scene;
}

static s32 main_get_scene(void) {
    return main_scene;
}

static void main_set_unused_flag(s32 value) {
    main_unused_8005E280 = value;
}

static void main_set_speed_mode(unsigned char mode) {
    main_speed_mode = mode;
}

unsigned char main_get_speed_mode(void) {
    return main_speed_mode;
}

void main_set_put_env(s32 value) {
    main_put_env_enabled = value;
}

void main_set_clear_screen(s32 value) {
    main_clear_screen_enabled = value;
}

/* Allocate `size` bytes of packets from the GS work area. */
void *gs_work_alloc(s32 size) {
    u8 *limit = current_frame->workBase + current_frame->workSize;
    u8 *packet = GsGetWorkBase();
    if (limit < packet + size) {
        do {
        } while (host_poll() != 0);
        SystemError(0x50, 0);
    }
    GsSetWorkBase(packet + size);
    return packet;
}

/* Hand the work area back up to `end`, the first packet not used. */
void gs_work_release(void *end) {
    if (current_frame->workBase + current_frame->workSize < (u8 *)end) {
        do {
        } while (host_poll() != 0);
    }
    GsSetWorkBase(end);
}

static s32 main_region_gap_const(void) {
    s32 region = (s32)region_base;
    s32 base = (s32)overlay_region_table[OVERLAY_HIGH];
    s32 offset = 0xFFF7D000;
    s32 end = (s32)farm_area.loadWindow;

    region -= base;
    region += offset;
    return region - (end - base);
}

/* Free bytes between farm_area.loadWindow and region_base, less farm_section_size and a fixed
 * 0x49000 bytes. */
s32 main_region_free_bytes(void) {
    u8 *region;
    u8 *table_high;
    s32 section_size;
    u8 *window_end;
    s32 offset;
    s32 total;

    region = region_base;
    table_high = overlay_region_table[OVERLAY_HIGH];
    section_size = farm_section_size;
    total = 0xFFFB7000;
    offset = region - table_high;
    total += offset;
    window_end = farm_area.loadWindow;
    offset = total - section_size;
    return offset - (window_end - table_high);
}

/* The start of the free region: main_region_free_bytes below region_base. */
u8 *main_region_free_start(void) {
    return region_base - main_region_free_bytes();
}

s32 main_get_region_base(void) {
    return (s32)region_base;
}

/* Start a frame: point the GS work area at its work base, select its ordering-table block (0x3000
 * bytes per frame index), clear the tables and rewind the primitive cursor. */
static void main_frame_begin(void) {
    s32 ot_base;

    GsSetWorkBase(current_frame->workBase);
    ot_base = ((frame_index * 3) << 12) + REGION_FRAME_OT_BASE;
    frame_ot_base = region_base + ot_base;
    ClearOTagR(active_ot, 2);

    if (current_frame->otTags[0] != 0) {
        ClearOTagR(current_frame->otTags[0], current_frame->otTagCounts[0]);
    }
    if (current_frame->otTags[1] != 0) {
        ClearOTagR(current_frame->otTags[1], current_frame->otTagCounts[1]);
    }
    if (current_frame->otTags[2] != 0) {
        ClearOTagR(current_frame->otTags[2], current_frame->otTagCounts[2]);
    }
    if (current_frame->otTags[3] != 0) {
        ClearOTagR(current_frame->otTags[3], current_frame->otTagCounts[3]);
    }
    ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);

    current_frame->primCursor.bytes = current_frame->primStart;
}

void main_frame_clear(void) {
    if (current_frame->otTags[0] != 0) {
        ClearOTagR(current_frame->otTags[0], current_frame->otTagCounts[0]);
    }
    if (current_frame->otTags[1] != 0) {
        ClearOTagR(current_frame->otTags[1], current_frame->otTagCounts[1]);
    }
    if (current_frame->otTags[2] != 0) {
        ClearOTagR(current_frame->otTags[2], current_frame->otTagCounts[2]);
    }
    if (current_frame->otTags[3] != 0) {
        ClearOTagR(current_frame->otTags[3], current_frame->otTagCounts[3]);
    }
    ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
    current_frame->primCursor.bytes = current_frame->primStart;
}

/* Reset the frame records for `index` (frame_reset_funcs) and start the frame. */
static void main_frame_reset(int index) {
    while (DrawSync(1) != 0) {
    }
    SetGeomOffset(0, 0);
    SetGeomScreen(1000);
    GsSetOffset(0, 0);
    GsSetDrawBuffOffset();
    frame_reset_funcs[index]();
    main_frame_begin();
    main_frame_begin();
}

/* Clear the work region and lay both frame records out over it: three tag arrays of 64, 16 and 2
 * entries with a GsOT header each (the fourth OT is unused), the 0x800-entry main OT, the DRAWENV
 * clear flags and black background, a work area and the primitive buffer. Then lay out the frame
 * buffers. */
static void main_init_frame_records(void) {
    s32 i;
    u8 *base;

    base = WORK_REGION;
    region_base = base;
    memset(base, 0, REGION_BYTES);
    frame_ot_base_init = frame_ot_base = (u8 *)0x801F7000;
    for (i = 0; i < 2; i++) {
        frame_records[i].otTags[0] = (u_long *)(base + i * 0x100);
        frame_records[i].otTagCounts[0] = 64;
        frame_records[i].otLengths[0] = 6;
        frame_records[i].otTags[1] = (u_long *)(base + REGION_TAGS1 + i * 0x40);
        frame_records[i].otTagCounts[1] = 16;
        frame_records[i].otLengths[1] = 4;
        frame_records[i].otTags[2] = (u_long *)(base + REGION_TAGS2 + i * 8);
        frame_records[i].otTagCounts[2] = 2;
        frame_records[i].otLengths[2] = 1;
        frame_records[i].otTags[3] = 0;
        frame_records[i].otTagCounts[3] = 0;
        frame_records[i].otLengths[3] = 0;
        frame_records[i].otDepth = FRAME_OT_DEPTH;
        frame_records[i].otDepthBits = 11;
        frame_records[i].ot = (u_long *)(base + REGION_OT + i * 0x2000);
        frame_records[i].drawEnv.dtd = 1;
        frame_records[i].drawEnv.dfe = 1;
        frame_records[i].drawEnv.isbg = 1;
        frame_records[i].drawEnv.r0 = 0;
        frame_records[i].drawEnv.g0 = 0;
        frame_records[i].drawEnv.b0 = 0;
        frame_records[i].ots[0].length = 6;
        frame_records[i].ots[0].point = 0;
        frame_records[i].ots[0].offset = 0;
        frame_records[i].ots[0].org = (GsOT_TAG *)frame_records[i].otTags[0];
        frame_records[i].ots[0].tag = (GsOT_TAG *)&frame_records[i].otTags[0][64 - 1];
        frame_records[i].ots[1].length = 4;
        frame_records[i].ots[1].point = 0;
        frame_records[i].ots[1].offset = 0;
        frame_records[i].ots[1].org = (GsOT_TAG *)frame_records[i].otTags[1];
        frame_records[i].ots[1].tag = (GsOT_TAG *)&frame_records[i].otTags[1][16 - 1];
        frame_records[i].ots[2].length = 1;
        frame_records[i].ots[2].point = 0;
        frame_records[i].ots[2].offset = 0;
        frame_records[i].ots[2].org = (GsOT_TAG *)frame_records[i].otTags[2];
        frame_records[i].ots[2].tag = (GsOT_TAG *)&frame_records[i].otTags[2][2 - 1];
        frame_records[i].ots[3].length = 0;
        frame_records[i].ots[3].point = 0;
        frame_records[i].ots[3].offset = 0;
        frame_records[i].ots[3].org = 0;
        frame_records[i].ots[3].tag = 0;
        frame_records[i].workSize = 0x2EE0;
        frame_records[i].workBase = base + REGION_WORK + i * 0x2EE0;
        frame_records[i].primBuffer = base + REGION_PRIM + i * FRAME_PRIM_SIZE;
    }
    frame_buffers_layout();
}

/* Like main_init_frame_records, with a larger work area (0x5EDC bytes) per frame. Then both records'
 * main OTs and primitive buffers are carved again from region_base: each OT is cleared, each
 * primitive buffer is zeroed and its cursor rewound. */
static void main_init_frame_records_wide(void) {
    s32 i;
    s32 k;
    s32 j;
    FrameState *fb;
    u8 *region;
    u8 *base;

    base = WORK_REGION;
    region_base = base;
    memset(base, 0, REGION_BYTES);
    frame_ot_base_init = frame_ot_base = (u8 *)0x801FCFF8;
    for (i = 0; i < 2; i++) {
        frame_records[i].otTags[0] = (u_long *)(base + i * 0x100);
        frame_records[i].otTagCounts[0] = 64;
        frame_records[i].otLengths[0] = 6;
        frame_records[i].otTags[1] = (u_long *)(base + REGION_TAGS1 + i * 0x40);
        frame_records[i].otTagCounts[1] = 16;
        frame_records[i].otLengths[1] = 4;
        frame_records[i].otTags[2] = (u_long *)(base + REGION_TAGS2 + i * 8);
        frame_records[i].otTagCounts[2] = 2;
        frame_records[i].otLengths[2] = 1;
        frame_records[i].otTags[3] = 0;
        frame_records[i].otTagCounts[3] = 0;
        frame_records[i].otLengths[3] = 0;
        frame_records[i].otDepth = FRAME_OT_DEPTH;
        frame_records[i].otDepthBits = 11;
        frame_records[i].ot = (u_long *)(base + REGION_OT + i * 0x2000);
        frame_records[i].drawEnv.dtd = 1;
        frame_records[i].drawEnv.dfe = 1;
        frame_records[i].drawEnv.isbg = 1;
        frame_records[i].drawEnv.r0 = 0;
        frame_records[i].drawEnv.g0 = 0;
        frame_records[i].drawEnv.b0 = 0;
        frame_records[i].ots[0].length = 6;
        frame_records[i].ots[0].point = 0;
        frame_records[i].ots[0].offset = 0;
        frame_records[i].ots[0].org = (GsOT_TAG *)frame_records[i].otTags[0];
        frame_records[i].ots[0].tag = (GsOT_TAG *)&frame_records[i].otTags[0][64 - 1];
        frame_records[i].ots[1].length = 4;
        frame_records[i].ots[1].point = 0;
        frame_records[i].ots[1].offset = 0;
        frame_records[i].ots[1].org = (GsOT_TAG *)frame_records[i].otTags[1];
        frame_records[i].ots[1].tag = (GsOT_TAG *)&frame_records[i].otTags[1][16 - 1];
        frame_records[i].ots[2].length = 1;
        frame_records[i].ots[2].point = 0;
        frame_records[i].ots[2].offset = 0;
        frame_records[i].ots[2].org = (GsOT_TAG *)frame_records[i].otTags[2];
        frame_records[i].ots[2].tag = (GsOT_TAG *)&frame_records[i].otTags[2][2 - 1];
        frame_records[i].ots[3].length = 0;
        frame_records[i].ots[3].point = 0;
        frame_records[i].ots[3].offset = 0;
        frame_records[i].ots[3].org = 0;
        frame_records[i].ots[3].tag = 0;
        frame_records[i].workSize = 0x5EDC;
        frame_records[i].workBase = base + REGION_WORK + i * 0x5EDC;
        frame_records[i].primBuffer = base + REGION_PRIM + i * FRAME_PRIM_SIZE;
    }
    region = region_base;
    for (k = 0; k < 2; k++) {
        fb = &frame_records[k];
        fb->otDepth = FRAME_OT_DEPTH;
        fb->otDepthBits = 11;
        fb->ot = (u_long *)&region[k * 0x2000 + REGION_OT];
        fb->primSize = FRAME_PRIM_SIZE;
        fb->primStart = &region[k * FRAME_PRIM_SIZE + REGION_PRIM];
        ClearOTagR(fb->ot, FRAME_OT_DEPTH);
        for (j = 0; j < FRAME_PRIM_SIZE; j++) {
            fb->primStart[j] = 0;
        }
        fb->primCursor.bytes = fb->primStart;
    }
}

/* Save the high overlay region into a frame's primitive buffer (enabled == 0), or restore it from
 * there. The size is 0x10000 bytes of record 1 when frame_count is 0, else the current frame's
 * primSize * 2 bytes of record 0. */
static void main_copy_prim_buffer(s32 enabled, s32 frame_count) {
    s32 frame;
    s32 size;
    if (enabled == 0) {
        if (frame_count == 0) {
            frame = 1;
            size = 0x10000;
        } else {
            frame = 0;
            size = current_frame->primSize << 1;
        }
        memcpy(frame_records[frame].primBuffer,
                      overlay_region_table[OVERLAY_HIGH], size);
        prim_copy_pending = 1;
        prim_copy_frame_count = frame_count;
        return;
    }
    if (prim_copy_frame_count == 0) {
        frame = 1;
        size = 0x10000;
    } else {
        frame = 0;
        size = current_frame->primSize << 1;
    }
    memcpy(overlay_region_table[OVERLAY_HIGH],
                  frame_records[frame].primBuffer, size);
    prim_copy_pending = 0;
}

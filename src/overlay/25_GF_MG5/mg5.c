/* Minigame 5 (overlay 25): the swimming race. The swimmers race the lanes and the
 * player steers one of them. */

#include <sys/types.h>
#include <memory.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "afile.h"
#include "anim.h"
#include "farm_state.h"
#include "game_types.h"
#include "ground.h"
#include "main.h"
#include "math_util.h"
#include "menu_window.h"
#include "mg5.h"
#include "model.h"
#include "pad.h"
#include "scene.h"
#include "screen_fade.h"
#include "sound.h"
#include "sound_request.h"
#include "text.h"
#include "texture_pack.h"
#include "ui_icons.h"
#include "villager.h"

/* The minigame's music, the size of its stage image, and the frames of its fades. */
#define MINIGAME_SEQUENCE 18
#define STAGE_DATA_SIZE 0x13DC
#define MINIGAME_FADE_FRAMES 10
/* Where the stage's sections start in the farm data file's section table. */
#define MG5_STAGE_SECTION 228
#define MG5_INTRO_FRAMES 60
#define MG5_DISPATCH_RACE 2
#define MG5_SLIDE_STEP 18
#define MG5_BANDS_PER_RACER 5
#define MG5_ROLL_MAX 128
#define MG5_HEAD_DEPTH_BIAS 0x400
/* Menu text ids (menu_request_open): the start steps' prompts, and the intro's. */
#define MG5_MENU_PROMPT 0x31
#define MG5_MENU_SCROLL_IN 0x32
#define MG5_MENU_SCROLL_SETTLE 0x33
#define MG5_MENU_START 0x34
#define MG5_MENU_INTRO 0x35

/* Each start table holds two sets of six (or two) rows: the first for the start of the
 * race, the second for the race itself. */
typedef struct { SVECTOR first[6]; SVECTOR second[6]; } Mg5StartRows;

typedef struct { SVECTOR first[2]; SVECTOR second[2]; } Mg5ExtraStarts;

static void mg5_load_step(void);
static void mg5_step_intro_wait(void);
static void mg5_frame_update(void);
static void mg5_step_fade_out(void);
static void mg5_leave_minigame(void);
static void mg5_state_init(void);
static void mg5_state_reset(void);
static void mg5_racers_init_race(void);
static void mg5_racers_init_start(void);
static void mg5_step_dispatch(void);
static void mg5_intro_timer(void);
static void mg5_race_frame(void);
static void mg5_player_input(void);
static void mg5_rivals_tick(void);
static void mg5_scroll_follow(void);
static void mg5_racers_tick(void);
static void mg5_racer_tick(Mg5Racer *racer);
static void mg5_check_finish(void);
static void mg5_paused_frame(void);
static void mg5_draw_scene(void);
static void mg5_draw_racers(void);
static void mg5_racer_roll(Mg5Racer *target);
static void mg5_racer_roll_band(Mg5Racer *racer);
static s32 mg5_racer_is_active(Mg5Racer *racer);
static s32 mg5_racer_odds_roll(Mg5Racer *racer, s16 band);
static void mg5_racer_step(Mg5Racer *mover);
static s32 mg5_position_step_value(s32 position);
static s16 mg5_position_bonus(s32 position);
static void mg5_racer_set_status(Mg5Racer *racer, s16 status, s16 timer);
static void mg5_racer_draw_anim(Mg5Racer *racer);
static void mg5_extra_tick(Mg5Racer *extra);
static void mg5_draw_rank(void);
static void mg5_draw_marker(void);
static void mg5_draw_fixed_anims(void);
static void mg5_anims_enable(void);
static void mg5_anims_disable(void);
static void mg5_draw_backdrop(void);
static void mg5_anim_draw_at(AnimState *object, SVECTOR *position);
static void mg5_sprite_draw_at(AnimState *sprite, SVECTOR *vec);
static void mg5_step_init_racers(void);
static void mg5_step_menu_prompt(void);
static void mg5_step_scroll_in(void);
static void mg5_step_scroll_settle(void);
static void mg5_step_start_menu(void);
static void mg5_step_start_race(void);
static void mg5_scroll_init(void);
static void mg5_load_sprites(void);
static void mg5_load_script(void);
static void mg5_script_blit_frames(FarmModeScript *script);
static void mg5_set_bg_color(s16 r, s16 g, s16 b);
static void mg5_bg_fade_step(void);
static void mg5_scroll_camera_rotate(void);
static void mg5_scroll_camera_apply(void);
static void mg5_scroll_camera_draw(void);
static void mg5_stage_data_load(void);
static void mg5_staging_load(void);
static void mg5_audio_start(void);
static void mg5_slots_clear(void);
static void mg5_slots_step(void);
static void mg5_slot_image_step(FarmImageSlot *slot);
static void mg5_slot_rect_step(FarmImageSlot *rect);
static void mg5_slot_add(FarmBlitFrame *frame);
static void mg5_villager_friendship_add(CharacterId villager);
static void mg5_villager_affection_add(CharacterId villager);

#include "overlay/common/farm_section_table.h"
/* Two text-code strings for mg5_player_input, one after the other. */
static u16 mg5_text_codes[82] = {
    0x0009, 0x0034, 0x0032, 0x0033, 0x0FFD, 0x0020, 0x0FFD, 0x002B, 0x0028, 0x0033, 0x0033, 0x002B,
    0x0024, 0x0FFD, 0x002C, 0x002E, 0x0031, 0x0024, 0x0063, 0x0FFD, 0x0FFD, 0x0002, 0x002E, 0x002C,
    0x0024, 0x0FFD, 0x002E, 0x002D, 0x0063, 0xFFFE, 0x0018, 0x002E, 0x0034, 0x0FFD, 0x0022, 0x0020,
    0x002D, 0x0FFD, 0x0023, 0x002E, 0x0FFD, 0x0028, 0x0033, 0x0063, 0xFFFF,
    0x0016, 0x0027, 0x0020, 0x0033, 0x0FFD, 0x0027, 0x0020, 0x002F, 0x002F, 0x0024, 0x002D, 0x0024,
    0x0023, 0x0062, 0xFFFE, 0x0004, 0x0035, 0x0024, 0x0031, 0x0038, 0x0021, 0x002E, 0x0023, 0x0038,
    0x0FFD, 0x0028, 0x0032, 0x0FFD, 0x0036, 0x0020, 0x0028, 0x0033, 0x0028, 0x002D, 0x0026, 0x006C,
    0xFFFF,
};

/* The step handlers mg5_step_run dispatches through. */
static Func mg5_step_handlers[5] = {
    mg5_load_step, mg5_step_intro_wait, mg5_frame_update,
    mg5_step_fade_out, mg5_leave_minigame,
};

/* Handlers for the race states, indexed by Mg5State.dispatch. */
static void (*mg5_dispatch_handlers[3])(void) = {mg5_step_dispatch, mg5_intro_timer, mg5_race_frame};
static Func mg5_start_steps[6] = {
    mg5_step_init_racers, mg5_step_menu_prompt, mg5_step_scroll_in,
    mg5_step_scroll_settle, mg5_step_start_menu, mg5_step_start_race,
};

/* The racers' start positions, two sets of six: mg5_racers_init_start seeds the racers
 * from the first, mg5_racers_init_race from the second. mg5_scroll_init aims the scroll at
 * the first one's x and z. */
static Mg5StartRows mg5_start_rows = {
    {{-2300, 0, 164, 0}, {-2380, 0, 1107, 0}, {-2127, 0, -580, 0}, {-2830, 0, -615, 0}, {-1694, 0, 990, 0}, {-2296, 0, 1964, 0}},
    {{-320, 0, 164, 0}, {-320, 0, 644, 0}, {-320, 0, -316, 0}, {-320, 0, -796, 0}, {-320, 0, -1276, 0}, {-320, 0, 1124, 0}},
};
/* The two extra actors' start positions, the same two sets. */
static Mg5ExtraStarts mg5_extra_starts = {
    {{-2750, 0, 164, 0}, {4916, -64, 180, 0}},
    {{-852, 0, 410, 0}, {4916, -64, 180, 0}},
};
/* The per-stride position bonus, by band (mg5_position_bonus). */
static s16 mg5_bonus_40 = -2;
static s16 mg5_bonus_30 = -2;
static s16 mg5_bonus_20 = -2;
static s16 mg5_bonus_10 = -2;
/* The lane step by band (mg5_position_step_value). */
static s16 mg5_lane_pos_a = 160;
static s16 mg5_lane_pos_b = 130;
static s16 mg5_lane_pos_c = 100;
static s16 mg5_lane_pos_d = 70;
/* Odds thresholds for mg5_racer_odds_roll: five per racer (0 is the player's row). */
static s16 mg5_odds_thresholds[] = {
    0, 80, 100, 110, 10, 40, 20, 5,
    5, 120, 20, 60, 110, 30, 5, 0,
};
/* Odds per band, runner and slot, for mg5_racer_roll_band. */
static s16 mg5_band_values[4][3][4] = {
    {{80, 20, 0, 0}, {10, 90, 0, 0}, {50, 50, 0, 0}},
    {{60, 30, 10, 0}, {10, 20, 70, 0}, {25, 70, 5, 0}},
    {{40, 40, 15, 5}, {5, 10, 20, 65}, {20, 65, 10, 5}},
    {{30, 40, 20, 10}, {90, 5, 5, 0}, {25, 60, 10, 5}},
};
/* The start steps of the race (Mg5State.step), in the order mg5_step_dispatch runs them. */
enum Mg5StartStep {
    MG5_START_INIT_RACERS = 0,
    MG5_START_MENU_PROMPT = 1,
    MG5_START_SCROLL_IN = 2,
    MG5_START_SCROLL_SETTLE = 3,
    MG5_START_MENU = 4,
    MG5_START_RACE = 5,
    MG5_START_WAIT_FADE = 6,
};

/* The steps of mg5_load_step, in order; MG5_LOAD_DONE leaves the load. */
enum Mg5LoadStep {
    MG5_LOAD_STAGE_DATA = 0,
    MG5_LOAD_STAGING = 1,
    MG5_LOAD_SPRITES = 2,
    MG5_LOAD_SCRIPT = 3,
    MG5_LOAD_AUDIO = 4,
    MG5_LOAD_FADER_WAIT = 5,
    MG5_LOAD_DONE = 6,
};

/* mg5_request_flag: the screen's drop. The screen rises (RAISE), then falls
 * back with growing speed (DROP) until it lands and goes back to REST. */
enum Mg5ScreenState {
    MG5_SCREEN_REST = 0,
    MG5_SCREEN_RAISE = 1,
    MG5_SCREEN_DROP = 2,
};

/* mg5_phase_a drives mg5_anims[4]: wait until phase b has counted to 30, then
 * run the animation to its end (DONE). */
enum Mg5PhaseA {
    MG5_PHASE_A_WAIT = 0,
    MG5_PHASE_A_RUN = 1,
    MG5_PHASE_A_DONE = 0xFF,
};

/* mg5_phase_b drives mg5_anims[5]: wait until racer 0 reaches the mark, then
 * count up (any value from COUNT on) while the animation runs, to DONE. */
enum Mg5PhaseB {
    MG5_PHASE_B_WAIT = 0,
    MG5_PHASE_B_COUNT = 1,
    MG5_PHASE_B_DONE = 0xFF,
};

/* A racer's status (Mg5Racer.status). The racers and the extras share it: the
 * racers run GRID..FINISHED, the extras EXTRA_* (9..12). */
enum Mg5RacerStatus {
    MG5_RACER_GRID = 0,
    MG5_RACER_READY = 1,
    MG5_RACER_COAST = 2,
    MG5_RACER_STUMBLE = 3,
    MG5_RACER_STRIDE = 4,
    MG5_RACER_DASH = 5,
    MG5_RACER_DRIFT = 6,
    MG5_RACER_ROLL = 7,
    MG5_RACER_FINISHED = 8,
    MG5_EXTRA_CHEER = 9,
    MG5_EXTRA_IDLE = 10,
    MG5_EXTRA_IDLE_2 = 11,
    MG5_EXTRA_CELEBRATE = 12,
};

static u8 mg5_step = MG5_LOAD_STAGE_DATA;
static u8 mg5_request_flag = MG5_SCREEN_REST;
/* Phase bytes: mg5_phase_a drives mg5_anims[4], mg5_phase_b drives mg5_anims[5]. */
static u8 mg5_phase_a = MG5_PHASE_A_WAIT;
static u8 mg5_phase_b = MG5_PHASE_B_WAIT;

static u8 mg5_landing_anim_on = 0;

/* Positions of the fixed animations, and the screen position they are drawn from. */
static SVECTOR mg5_anim2_pos = {2919, 560, 3859, 0};
static SVECTOR mg5_anim3_pos = {4621, 560, 3289, 0};
static SVECTOR mg5_anim4_pos = {1874, 560, 3289, 0};
static SVECTOR mg5_anim5_pos = {2366, 560, 3289, 0};
static SVECTOR mg5_screen_pos = {0};
static s16 mg5_lerp_reverse = 0;
static s16 mg5_lerp_t = 0;

/* How fast the fixed animations fall. */
static s16 mg5_fall_step = 60;
static FarmScreenXY mg5_project_xy = {0};
static FarmScreenXY mg5_project_xy2 = {0};

/* Where each animation of the sprite sheet starts: rows 0..2, the racers' frames 3..8, the
 * extras' 9..10, and the scene's six animations 11..16. */
static u32 mg5_sprite_offsets[17] = {
    0x00000000, 0x00000054, 0x000000CC, 0x00000300, 0x000006C0, 0x00000A34,
    0x00000DA8, 0x000011E0, 0x0000148C, 0x00001874, 0x00001914, 0x000001D8,
    0x0000026C, 0x00001A18, 0x00001A6C, 0x00001AC0, 0x00001BA4,
};

/* The background colour's two ends, interpolated by mg5_bg_fade_step. */
static u16 mg5_lerp_from[6] = {0x0000, 0x0000, 0x0063, 0x0000, 0x007F, 0x0000};
static s16 mg5_lerp_to[6] = {0x0000, 0x0000, 0x00A5, 0x0000, 0x00D5, 0x0000};

static SVECTOR mg5_converted_vec;
static SVECTOR mg5_project_vec;
static u8 mg5_stage_data[STAGE_DATA_SIZE];
static u8 mg5_sprite_sheet[0x1C88];
static Mg5State mg5_state;
/* Six animation slots: the fixed scene animations and the ones the racers use. */
static AnimState mg5_anims[6];
static Model mg5_model;

/* Run the current step with the text palette set to 3. */
void mg5_step_run(void) {
    s32 saved;

    saved = text_get_palette();
    text_set_palette(3);
    mg5_step_handlers[game_state.progress.entrySubStep]();
    text_set_palette(saved);
    if (game_state.globalFlags & GLOBAL_FLAG_PAUSED) {
        game_state.globalFlags &= ~GLOBAL_FLAG_PAUSED;
    }
}

static void mg5_load_step(void) {
    switch (mg5_step) {
    case MG5_LOAD_STAGE_DATA:
        mg5_stage_data_load();
        mg5_step++;
        break;
    case MG5_LOAD_STAGING:
        mg5_staging_load();
        mg5_step++;
        break;
    case MG5_LOAD_SPRITES:
        mg5_load_sprites();
        mg5_step++;
        break;
    case MG5_LOAD_SCRIPT:
        mg5_load_script();
        mg5_step++;
        break;
    case MG5_LOAD_AUDIO:
        mg5_audio_start();
        mg5_step++;
        break;
    case MG5_LOAD_FADER_WAIT:
        if (sound_fader_ready() != 0) {
            mg5_step = MG5_LOAD_DONE;
            sound_update();
            sound_seq_start_reverb(0, 0, MINIGAME_SEQUENCE, 127);
        }
        break;
    }
    if (mg5_step == MG5_LOAD_DONE) {
        mg5_slots_clear();
        farm_area.mode.place = farm_area.mode.script.head.place;
        mg5_script_blit_frames(&farm_area.mode.script);
        mg5_state_init();
        mg5_scroll_init();
        menu_reset();
        fade_start_in(MINIGAME_FADE_FRAMES, 0, 2);
        sound_fade_in(MINIGAME_FADE_FRAMES);
        game_state.progress.entrySubStep++;
        mg5_step = MG5_LOAD_STAGE_DATA;
    }
}

/* Wait for the fade-in and the sound fader to settle, then move the game on. */
static void mg5_step_intro_wait(void) {
    s32 fadeStage;

    mg5_scroll_camera_rotate();
    mg5_scroll_camera_apply();
    model_load_lighting();
    mg5_scroll_camera_draw();
    fadeStage = fade_is_settled() == 1;
    if (sound_fader_mode() == SOUND_FADER_IDLE) {
        fadeStage = (fadeStage + 1) & 0xFF;
    }
    mg5_racers_init_start();
    mg5_draw_scene();
    if (fadeStage == 2) {
        game_state.progress.entrySubStep++;
    }
    mg5_set_bg_color(0, 0xA5, 0xD5);
}
/* One frame of the race scene: the menu, the camera, the image slots, then the state's dispatch. */
static void mg5_frame_update(void) {
    menu_run_state();
    mg5_scroll_camera_rotate();
    mg5_scroll_camera_apply();
    model_load_lighting();
    mg5_scroll_camera_draw();
    mg5_slots_step();
    mg5_dispatch_handlers[mg5_state.dispatch]();
    mg5_bg_fade_step();
}
/* Hold the scene for the fade-out, then start the sound fade-out and move the game on. */
static void mg5_step_fade_out(void) {
    mg5_draw_scene();
    fade_start_blend(MINIGAME_FADE_FRAMES);
    sound_fade_out(MINIGAME_FADE_FRAMES);
    game_state.progress.entrySubStep++;
}

/* Once the fade has settled, release the minigame's voices and sounds and request the farm
 * scene back. */
static void mg5_leave_minigame(void) {
    s32 mode;

    mg5_draw_scene();
    mode = fade_is_settled() == 1;
    if (sound_fader_mode() == SOUND_FADER_IDLE) {
        mode = (mode + 1) & 0xFF;
    }
    if (mode == 2) {
        if (mg5_state.voice != SOUND_VOICE_NONE) {
            sound_release_voice(mg5_state.voice);
        }
        sound_seq_stop_first(0);
        sound_release_from(2, 0);
        sound_release_from(1, 0);
        game_state.progress.entrySubStep = 0xFF;
        main_request_scene(SCENE_FARM_MINIGAME_EXIT);
        game_state.globalFlags |= GLOBAL_FLAG_MINIGAME_EXIT;
        mg5_set_bg_color(0, 0, 0);
    }
}
/* Set the race's tuning values. */
static void mg5_state_init(void) {
    mg5_state.dispatch = 0;
    mg5_state.step = 0;
    mg5_state_reset();
    mg5_state.strideFrames = 5;
    mg5_state.dashStep = 2;
    mg5_state.rollScale = 5;
    mg5_state.coastStep = 1;
    mg5_state.idleTimeout = 3;
    mg5_state.trackWidth = 4000;
    mg5_state.driftPercent = 130;
    mg5_state.racerStepPercent = 70;
    mg5_state.steadyRivalStep = 320;
    mg5_state.decayRivalStep = 685;
    mg5_state.decayPercent = 10;
    mg5_state.idleLimit = 200;
}
/* Reset the race's flags, voices and timers, place the marker, and set the racers up for the race. */
static void mg5_state_reset(void) {
    Mg5State *p = &mg5_state;
    SVECTOR *q = &p->marker;
    p->voice = SOUND_VOICE_NONE;
    p->marker.vx = 4896;
    p->paused = 0;
    p->resultLatched = 0;
    q->vy = -10;
    q->vz = 164;
    p->idleFrames = 0;
    p->engineVoice = SOUND_VOICE_NONE;
    p->sceneTimer = 0;
    p->raceTime = 0;
    mg5_racers_init_race();
}

/* Set up the racers and extras for the race itself, from the second set of start rows. */
static void mg5_racers_init_race(void) {
    s32 i;

    i = 0;
    do {
        mg5_state.racers[i].id = i;
        mg5_state.racers[i].status = MG5_RACER_READY;
        mg5_state.racers[i].timer = 0;
        mg5_state.racers[i].delay = 0;
        mg5_state.racers[i].positionStep = 0;
        mg5_state.racers[i].position = 0x32000;
        mg5_state.racers[i].start = mg5_start_rows.second[i];
        i++;
    } while (i < 6);
    i = 0;
    do {
        mg5_state.extras[i].id = i;
        mg5_state.extras[i].start = mg5_extra_starts.second[i];
        i++;
    } while (i < 2);
    mg5_state.extras[0].status = MG5_EXTRA_IDLE;
    mg5_state.extras[1].status = MG5_EXTRA_IDLE_2;
}

/* Set up the racers and extras for the start of the race. */
static void mg5_racers_init_start(void) {
    s32 i;

    i = 0;
    do {
        mg5_state.racers[i].id = i;
        mg5_state.racers[i].status = MG5_RACER_GRID;
        mg5_state.racers[i].timer = 0;
        mg5_state.racers[i].position = 0x32000;
        mg5_state.racers[i].start = mg5_start_rows.first[i];
        i++;
    } while (i < 6);
    i = 0;
    do {
        mg5_state.extras[i].id = i;
        mg5_state.extras[i].start = mg5_extra_starts.first[i];
        i++;
    } while (i < 2);
    mg5_state.extras[0].status = MG5_EXTRA_IDLE;
    mg5_state.extras[1].status = MG5_EXTRA_IDLE_2;
}

/* Runs the current start step, or waits for the fade on the last one. The race time counts up
 * one a frame and wraps after 9999. */
static void mg5_step_dispatch(void) {
    s16 dispatch;
    s32 result;
    s32 quotient;
    s32 amount;

    mg5_draw_scene();
    dispatch = mg5_state.step;
    if (dispatch == MG5_START_WAIT_FADE) {
        result = fade_is_settled();
        if (result == 1) {
            mg5_state.step = MG5_START_INIT_RACERS;
            mg5_state.dispatch = result;
        }
    } else {
        mg5_start_steps[dispatch]();
        quotient = (mg5_state.raceTime + 1) / 10000;
        amount = quotient * 10000 - 1;
        mg5_state.raceTime = mg5_state.raceTime - amount;
    }
}

/* The intro: on the first frame it opens the intro message and makes the first extra cheer. After
 * MG5_INTRO_FRAMES it starts the race: resets the race, plays the start sound, moves the camera to
 * the player's start and raises the screen. */
static void mg5_intro_timer(void) {
    u16 startX;
    u16 startZ;
    mg5_draw_scene();
    if (mg5_state.sceneTimer == 0) {
        menu_begin_close();
        text_set_input(0);
        menu_set_sound_enabled(0);
        menu_request_open(4, MG5_MENU_INTRO);
        mg5_racer_set_status(&mg5_state.extras[0], MG5_EXTRA_CHEER, 59);
    }
    mg5_state.sceneTimer++;
    if (mg5_state.sceneTimer >= MG5_INTRO_FRAMES) {
        mg5_state_reset();
        mg5_state.voice = sound_play(0x38);
        menu_begin_close();
        mg5_state.dispatch = MG5_DISPATCH_RACE;
        startX = mg5_state.racers[0].start.vx;
        startZ = mg5_state.racers[0].start.vz;
        mg5_state.sceneTimer = 0;
        mg5_state.raceTime = 0;
        farm_area.mode.scroll.centre.vy = 0;
        farm_area.mode.scroll.centre.vx = startX;
        farm_area.mode.scroll.centre.vz = startZ;
        mg5_racer_set_status(&mg5_state.extras[0], MG5_EXTRA_IDLE, 0);
        mg5_request_flag = MG5_SCREEN_RAISE;
        mg5_screen_pos.vx = 2138;
        mg5_screen_pos.vy = 0;
        mg5_screen_pos.vz = -778;
    }
}

/* One race frame: the pause, then the player, the racers, the scene and the finish check. The
 * race time counts up here too. */
static void mg5_race_frame(void) {
    s32 toggled;
    s32 current;
    s32 quotient;
    s32 amount;

    if (input_pad1->pressed & PAD_START) {
        toggled = mg5_state.paused != 1;
        mg5_state.paused = toggled;
        if (toggled == 1) {
            mg5_anims_enable();
            mg5_state.voice = sound_play(0x36);
        } else {
            mg5_anims_disable();
        }
    }
    if (mg5_state.paused == 1) {
        mg5_paused_frame();
        return;
    }
    mg5_player_input();
    mg5_scroll_follow();
    mg5_rivals_tick();
    mg5_racers_tick();
    mg5_draw_scene();
    mg5_check_finish();
    current = mg5_state.raceTime;
    quotient = (current + 1) / 10000;
    amount = quotient * 10000 - 1;
    mg5_state.raceTime = mg5_state.raceTime - amount;
}

/* The player's pad: triangle dashes, cross strides (or rolls, far back). With no button held a
 * stride or dash turns into a coast once its timer runs out. A pending scene timer shows the hint
 * (1) or the result text (-1) until a button is pressed. */
static void mg5_player_input(void) {
    Mg5Racer *player;
    InputRecord *pad;
    s32 result;
    s16 pending;

    player = &mg5_state.racers[0];
    if (player->status != MG5_RACER_FINISHED) {
        result = mg5_racer_is_active(player);
        if (result == 1) {
            pad = input_pad1;
            if (pad->held & PAD_TRIANGLE) {
                mg5_state.idleFrames = 0;
                mg5_racer_set_status(player, MG5_RACER_DASH, 1);
            } else if (pad->pressed & PAD_CROSS) {
                mg5_state.idleFrames = 0;
                if (player->position <= 40959) {
                    mg5_racer_set_status(player, MG5_RACER_ROLL, 60);
                    mg5_request_flag = result;
                    mg5_screen_pos.vy = 0;
                    mg5_screen_pos.vx = player->start.vx - 800;
                    mg5_screen_pos.vz = player->start.vz;
                } else {
                    mg5_racer_set_status(player, MG5_RACER_STRIDE, mg5_state.strideFrames);
                    mg5_state.racers[0].positionStep = mg5_position_step_value(mg5_state.racers[0].position);
                }
            } else {
                mg5_state.idleFrames++;
                if ((u16)(player->status - 4) < 2) {
                    if (player->timer == 0) {
                        mg5_racer_set_status(player, MG5_RACER_COAST, 1);
                    }
                }
            }
        }
        pending = mg5_state.sceneTimer;
        if (pending != 0) {
            if (input_pad1->pressed & (PAD_TRIANGLE | PAD_CROSS)) {
                mg5_state.sceneTimer = 0;
            } else if (pending == 1) {
                text_draw_in_frame(&current_frame->otTags[0][3], &current_frame->otTags[0][2], mg5_text_codes);
            } else if (pending == (-1)) {
                text_draw_in_frame(&current_frame->otTags[0][3], &current_frame->otTags[0][2], &mg5_text_codes[45]);
            }
        }
    }
}

/* Each rival's next move. Rivals 4 and 5 stride every frame. The others pick from the odds of
 * their position: a stride, or a dash from the band roll. From whole position 10 up the rear band
 * can also stumble or coast; below that a rival rolls. */
static void mg5_rivals_tick(void) {
    Mg5Racer *racer;
    s16 roll;
    s32 i;

    if (mg5_state.racers[4].status != MG5_RACER_FINISHED) {
        mg5_racer_set_status(&mg5_state.racers[4], MG5_RACER_STRIDE, 1);
    }
    if (mg5_state.racers[5].status != MG5_RACER_FINISHED) {
        mg5_racer_set_status(&mg5_state.racers[5], MG5_RACER_STRIDE, 1);
    }
    for (i = 1; i < 4; i++) {
        racer = &mg5_state.racers[i];
        if (racer->status == MG5_RACER_FINISHED) {
            continue;
        }
        if (racer->timer != 0) {
            if ((u16)(racer->status - 4) < 2 || racer->status == MG5_RACER_ROLL || racer->status == MG5_RACER_DRIFT) {
                continue;
            }
        }
        if (racer->status == MG5_RACER_STUMBLE) {
            continue;
        }
        if (racer->position > 0x27FFF) {
            if (mg5_racer_odds_roll(racer, 0) == 1) {
                mg5_racer_set_status(racer, MG5_RACER_STRIDE, mg5_state.strideFrames);
            } else {
                mg5_racer_roll_band(racer);
            }
        } else if (racer->position > 0x1DFFF) {
            if (mg5_racer_odds_roll(racer, 1) == 1) {
                mg5_racer_set_status(racer, MG5_RACER_STRIDE, mg5_state.strideFrames);
            } else {
                mg5_racer_roll_band(racer);
            }
        } else if (racer->position > 0x13FFF) {
            if (mg5_racer_odds_roll(racer, 2) == 1) {
                mg5_racer_set_status(racer, MG5_RACER_STRIDE, mg5_state.strideFrames);
            } else {
                mg5_racer_roll_band(racer);
            }
        } else if (racer->position > 40959) {
            if (mg5_racer_odds_roll(racer, 3) == 1) {
                mg5_racer_set_status(racer, MG5_RACER_STRIDE, mg5_state.strideFrames);
            } else if (mg5_racer_odds_roll(racer, 4) == 1) {
                mg5_racer_roll_band(racer);
            } else {
                roll = rand_range((u16)(120 / mg5_state.rollScale)) + 1;
                if (roll >= mg5_state.idleLimit * 30 / (mg5_state.rollScale * 100)) {
                    mg5_racer_set_status(racer, MG5_RACER_STUMBLE, 60);
                    racer->delay = (roll - 2) * 30 / mg5_state.strideFrames;
                } else {
                    mg5_racer_set_status(racer, MG5_RACER_COAST, roll * mg5_state.strideFrames);
                }
            }
        } else {
            mg5_racer_set_status(racer, MG5_RACER_ROLL, 60);
        }
    }
}

/* Ease the camera centre toward the player's start x (nine tenths of the gap a frame), up to
 * x 3300, and follow its z. */
static void mg5_scroll_follow(void) {
    FarmLoadArea *area;
    Mg5State *state;
    s16 cur;
    s16 limit;
    u16 ucur;
    area = &farm_area;
    cur = area->mode.scroll.centre.vx;
    state = &mg5_state;
    limit = state->racers[0].start.vx;
    ucur = area->mode.scroll.centre.vx;
    if (cur < limit) area->mode.scroll.centre.vx = ucur + (((limit - cur) * 9) / 10);
    if (area->mode.scroll.centre.vx >= 3301) area->mode.scroll.centre.vx = 3300;
    area->mode.scroll.centre.vy = 0;
    area->mode.scroll.centre.vz = state->racers[0].start.vz;
}
/* Count down each racer's timer and tick the racer. */
static void mg5_racers_tick(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
        if (mg5_state.racers[i].timer > 0) {
            mg5_state.racers[i].timer--;
            if (mg5_state.racers[i].timer < 0) mg5_state.racers[i].timer = 0;
        }
        mg5_racer_tick(&mg5_state.racers[i]);
    }
}

/* Move one racer by its status and keep its position on the track. The player also drives the
 * engine sound's voice and volume and the coast-to-drift change. */
static void mg5_racer_tick(Mg5Racer *racer) {
    switch (racer->status) {
    case MG5_RACER_ROLL:
        if (racer->id != MG5_RACER_PLAYER && racer->timer <= 0) {
            mg5_racer_roll(racer);
        }
        break;
    case MG5_RACER_DASH:
        racer->position += (mg5_state.dashStep << 12) / mg5_state.strideFrames;
        break;
    case MG5_RACER_COAST:
        racer->position -= (mg5_state.coastStep << 12) / mg5_state.strideFrames;
        break;
    case MG5_RACER_DRIFT:
        racer->position += mg5_state.dashStep * (mg5_state.driftPercent << 12) / (mg5_state.strideFrames * 100);
        break;
    case MG5_RACER_STRIDE:
        mg5_racer_step(racer);
        break;
    case MG5_RACER_STUMBLE:
        racer->position -= (mg5_state.coastStep << 12) / mg5_state.strideFrames;
        if (racer->timer <= 0) {
            mg5_racer_set_status(racer, MG5_RACER_DRIFT, racer->delay);
            racer->delay = 0;
        }
        break;
    }
    if (racer->position > 0x31FFF) {
        racer->position = 0x32000;
    }
    if (racer->position < 0) {
        racer->position = 0;
    }
    if (racer->id != MG5_RACER_PLAYER) {
        return;
    }
    switch (racer->status) {
    case MG5_RACER_COAST:
        if (mg5_state.idleFrames >= mg5_state.idleLimit * 30 / 100) {
            mg5_racer_set_status(racer, MG5_RACER_DRIFT, racer->timer);
        } else if (racer->position <= 40959) {
            mg5_racer_set_status(racer, MG5_RACER_ROLL, 60);
        }
        /* fallthrough */
    case MG5_RACER_STRIDE:
    case MG5_RACER_DASH:
        if (mg5_state.engineVoice == SOUND_VOICE_NONE) {
            mg5_state.engineVoice = sound_play(0x37);
        }
        break;
    case MG5_RACER_DRIFT:
        if (mg5_state.idleFrames >= mg5_state.idleTimeout * 30) {
            mg5_state.sceneTimer = 1;
        }
        /* fallthrough */
    case MG5_RACER_ROLL:
        if (mg5_state.engineVoice != SOUND_VOICE_NONE) {
            sound_release_voice(mg5_state.engineVoice);
            mg5_state.engineVoice = SOUND_VOICE_NONE;
        }
        if (racer->timer == 0) {
            mg5_racer_set_status(racer, MG5_RACER_DRIFT, 0);
        }
        break;
    case MG5_RACER_FINISHED:
        if (mg5_state.engineVoice != SOUND_VOICE_NONE) {
            sound_release_voice(mg5_state.engineVoice);
            mg5_state.engineVoice = SOUND_VOICE_NONE;
        }
        break;
    }
    if (racer->status == MG5_RACER_DASH) {
        if (mg5_state.engineVoice != SOUND_VOICE_NONE) {
            sound_voice_set_volume(mg5_state.engineVoice, 70, 70);
        }
    } else {
        if (mg5_state.engineVoice != SOUND_VOICE_NONE) {
            sound_voice_set_volume(mg5_state.engineVoice, 127, 127);
        }
    }
    if (mg5_state.resultLatched == 1 && mg5_state.racers[0].start.vx == mg5_start_rows.second[0].vx) {
        mg5_state.sceneTimer = -1;
    }
}

/* Mark the racers that have crossed the finish line. The first to finish latches the result and
 * plays the finish sound; a player's win raises friendship with twelve villagers and affection
 * with five. Once all six have finished the game moves on. */
static void mg5_check_finish(void) {
    s32 active;
    s32 slot;

    active = 1;
    slot = 0;
    do {
        if (mg5_state.racers[slot].status != MG5_RACER_FINISHED) {
            if (mg5_state.racers[slot].start.vx >= mg5_state.trackWidth - 320) {
                mg5_racer_set_status(&mg5_state.racers[slot], MG5_RACER_FINISHED, 30);
                if (mg5_state.resultLatched == 0) {
                    mg5_state.resultLatched = 1;
                    mg5_state.voice = sound_play(0x39);
                    mg5_racer_set_status(&mg5_state.extras[1], MG5_EXTRA_CELEBRATE, 30);
                    if (slot == 0) {
                        game_state.minigames.mg5PlayerWon = 1;
                        game_state.minigames.mg5Slot = 0;
                        mg5_villager_friendship_add(CHARACTER_JEFF);
                        mg5_villager_friendship_add(CHARACTER_SASHA);
                        mg5_villager_friendship_add(CHARACTER_SAIBARA);
                        mg5_villager_friendship_add(CHARACTER_GRAY);
                        mg5_villager_friendship_add(CHARACTER_DOCTOR);
                        mg5_villager_friendship_add(CHARACTER_CLIFF);
                        mg5_villager_friendship_add(CHARACTER_MAY);
                        mg5_villager_friendship_add(CHARACTER_MAYOR_THOMAS);
                        mg5_villager_friendship_add(CHARACTER_RICK);
                        mg5_villager_friendship_add(CHARACTER_KAI);
                        mg5_villager_friendship_add(CHARACTER_HARRIS);
                        mg5_villager_friendship_add(CHARACTER_STU);
                        mg5_villager_affection_add(CHARACTER_KAREN);
                        mg5_villager_affection_add(CHARACTER_ANN);
                        mg5_villager_affection_add(CHARACTER_ELLI);
                        mg5_villager_affection_add(CHARACTER_MARY);
                        mg5_villager_affection_add(CHARACTER_POPURI);
                    } else {
                        game_state.minigames.mg5PlayerWon = 0;
                        game_state.minigames.mg5Slot = slot;
                    }
                }
            } else {
                active = 0;
            }
        }
        slot++;
    } while (slot < 6);
    if (active == 1) {
        game_state.progress.entrySubStep++;
    }
}

/* While paused, draw the backdrop and the scene again. */
static void mg5_paused_frame(void) {
    mg5_draw_backdrop();
    mg5_draw_scene();
}

static void mg5_draw_scene(void) {
    mg5_draw_racers();
    mg5_draw_rank();
    mg5_draw_marker();
    mg5_draw_fixed_anims();
}

static void mg5_draw_racers(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
        mg5_racer_draw_anim(&mg5_state.racers[i]);
    }
    for (i = 0; i < 2; i++) {
        mg5_extra_tick(&mg5_state.extras[i]);
    }
}

/* A rolling racer's roll: with the band-4 odds it stumbles (a long roll) or coasts for a random
 * number of strides; otherwise it takes the band roll. */
static void mg5_racer_roll(Mg5Racer *target) {
    s16 frame;
    s16 scaled;
    s32 threshold;
    s32 quotient;
    if (mg5_racer_odds_roll(target, 4) == 1) {
        quotient = 120 / mg5_state.rollScale;
        frame = rand_range(quotient & 0xFFFF) + 1;
        threshold = (mg5_state.idleLimit * 30) / (mg5_state.rollScale * 100);
        if (frame >= threshold) {
            mg5_racer_set_status(target, MG5_RACER_STUMBLE, 60);
            target->delay = ((frame - 2) * 30) / mg5_state.strideFrames;
        } else {
            scaled = frame * mg5_state.strideFrames;
            mg5_racer_set_status(target, MG5_RACER_COAST, scaled);
        }
    } else {
        mg5_racer_roll_band(target);
    }
}

/* The position picks an odds band (the further along, the lower the band; past 0x27FFF the
 * racer has finished). A percentile roll picks the first of the band's four odds whose
 * running total beats it. The racer then dashes for a timer: 50 past the finish band, else
 * 49 - 10 * slot (+1 for slot 0), less the position in whole units, scaled by rollScale /
 * dashStep. */
static void mg5_racer_roll_band(Mg5Racer *racer) {
    s16 roll;
    s16 total;
    s16 band;
    s16 value;
    s32 i;

    band = total = i = 0;
    roll = rand_range(100);
    if (racer->position > 0x27FFF) {
        band = -1;
    } else if (racer->position > 0x1DFFF) {
        band = 0;
    } else if (racer->position > 0x13FFF) {
        band = 1;
    } else if (racer->position > 40959) {
        band = 3;
    }
    for (i = 0; i < 4; i++) {
        total += mg5_band_values[band][racer->id - 1][i];
        if (total > roll) {
            break;
        }
    }
    if (band == -1) {
        value = 50;
    } else {
        value = 49 - i * 10;
        value += (i == 0) ? 1 : 0;
    }
    mg5_racer_set_status(racer, MG5_RACER_DASH, ((value - racer->position / 0x1000) * mg5_state.rollScale / mg5_state.dashStep));
}

/* 1 unless the racer is rolling or striding, which the player's input waits on. */
static s32 mg5_racer_is_active(Mg5Racer *racer) {
    s16 value = racer->status;
    if (value != MG5_RACER_ROLL && value != MG5_RACER_STRIDE) return 1;
    return 0;
}
/* Roll 0..127; 1 when it reaches the racer's threshold for this band. */
static s32 mg5_racer_odds_roll(Mg5Racer *racer, s16 band) {
    s32 result;
    result = rand_range(MG5_ROLL_MAX);
    return result >= mg5_odds_thresholds[(racer->id - 1) * MG5_BANDS_PER_RACER + band];
}

/* Move a striding racer one stride along the track. Rivals 4 and 5 use their own steps; racers 0
 * to 3 gain the position bonus. A racer with no timer left coasts. */
static void mg5_racer_step(Mg5Racer *mover) {
    s16 step;
    s16 laps;
    s32 i;
    s32 total;

    step = mg5_position_step_value(mover->position) / mg5_state.strideFrames;
    if (mover->id == MG5_RACER_RIVAL_5) {
        laps = mg5_state.raceTime / 30;
        step = mg5_state.decayRivalStep / 30;
        for (i = 0; i < laps; i++) {
            step = step * (100 - mg5_state.decayPercent) / 100;
        }
        if (step <= 0) {
            step = 1;
        }
        mover->timer = 1;
    } else if (mover->id == MG5_RACER_RIVAL_4) {
        step = mg5_state.steadyRivalStep / 30;
        mover->timer = 1;
    } else if (mover->id != MG5_RACER_PLAYER) {
        step = step * mg5_state.racerStepPercent / 100;
    }
    total = step + (u16)mover->start.vx;
    mover->start.vx = total;
    if (mover->timer <= 0) {
        mg5_racer_set_status(mover, MG5_RACER_COAST, 0);
    }
    if ((u16)(mover->id - 4) >= 2) {
        mover->position += (mg5_position_bonus(mover->position) << 12) / mg5_state.strideFrames;
    }
}

/* The lane step for a 20.12 fixed-point position, by its whole part: 40 and up, 30 and up, 20 and
 * up, or below. */
static s32 mg5_position_step_value(s32 position) {
    s32 value;
    if (position < 0) position += 0xFFF;
    value = position << 4;
    position = value >> 16;
    if (position >= 40) {
        position = mg5_lane_pos_a;
    } else if (position >= 30) {
        position = mg5_lane_pos_b;
    } else if (position >= 20) {
        position = mg5_lane_pos_c;
    } else {
        position = mg5_lane_pos_d;
    }
    return position;
}
/* The per-stride position bonus, by the whole part of a 20.12 fixed-point position: 40 and up,
 * 30, 20, 10, or none. */
static s16 mg5_position_bonus(s32 position) {
    s32 value;
    if (position < 0) position += 0xFFF;
    value = position << 4;
    position = value >> 16;
    if (position >= 40) return mg5_bonus_40;
    if (position >= 30) return mg5_bonus_30;
    if (position >= 20) return mg5_bonus_20;
    if (position >= 10) return mg5_bonus_10;
    return 0;
}
/* Set a racer's status; the timer restarts only when the status changes. */
static void mg5_racer_set_status(Mg5Racer *racer, s16 status, s16 timer) {
    if (racer->status != status) racer->timer = timer;
    racer->status = status;
}

/* Pick the record's two animation banks from its status and draw them. */
static void mg5_racer_draw_anim(Mg5Racer *racer) {
    s32 body;
    s32 head;

    head = -1;
    body = -1;
    switch (racer->status) {
    case MG5_RACER_GRID:
        switch (racer->id) {
        case MG5_RACER_PLAYER:
            body = 4;
            break;
        case MG5_RACER_RIVAL_1:
        case MG5_RACER_RIVAL_2:
            body = 5;
            break;
        case MG5_RACER_RIVAL_3:
        case MG5_RACER_RIVAL_4:
        case MG5_RACER_RIVAL_5:
            body = 7;
            break;
        }
        break;
    case MG5_RACER_READY:
    case MG5_RACER_DRIFT:
        body = 10;
        head = 11;
        break;
    case MG5_RACER_COAST:
    case MG5_RACER_STUMBLE:
    case MG5_RACER_STRIDE:
        body = 0;
        head = 1;
        break;
    case MG5_RACER_DASH:
        body = 2;
        head = 3;
        break;
    case MG5_RACER_ROLL:
        body = 6;
        if (racer->id != MG5_RACER_PLAYER) {
            body = 0;
            head = 1;
        } else {
            head = 12;
        }
        break;
    case MG5_RACER_FINISHED:
        body = 8;
        if (racer->timer != 0) {
            body = 10;
            head = 11;
        } else {
            head = 9;
        }
        break;
    }
    if (racer->body.animation != body) anim_select_range(&racer->body, body & 0xFF);
    mg5_anim_draw_at(&racer->body, &racer->start);
    if (head != -1) {
        if (racer->head.animation != head) anim_select_range(&racer->head, head & 0xFF);
        mg5_sprite_draw_at(&racer->head, &racer->start);
    }
}

/* Pick an extra's animation from its status (cheer, idle, celebrate) and draw it. The celebration
 * counts its timer down, then the extra goes back to idle. */
static void mg5_extra_tick(Mg5Racer *extra) {
    s16 type;
    s32 value;
    s32 count;
    s32 new_type;
    void *base;
    type = extra->status;
    value = -1;
    switch (type) {
    case MG5_EXTRA_CHEER:
        value = 1;
        break;
    case MG5_EXTRA_IDLE:
        value = 0;
        break;
    case MG5_EXTRA_IDLE_2:
        value = 0;
        break;
    case MG5_EXTRA_CELEBRATE:
        count = (u16)extra->timer - 1;
        extra->timer = count;
        value = 1;
        new_type = MG5_EXTRA_IDLE_2;
        if ((count << 16) <= 0) extra->status = new_type;
        break;
    }
    base = &extra->body;
    if (extra->body.animation != value) anim_select_range(base, value & 0xFF);
    mg5_anim_draw_at(base, &extra->start);
}
/* Draw the rank sprite over the player. Its frame is the position band (0 past 40, 4 below 10);
 * it is hidden on the grid and once the player has finished. */
static void mg5_draw_rank(void) {
    s32 mode;
    s16 state_type;
    SVECTOR position;

    if (0x28000 < mg5_state.racers[0].position) {
        mode = 0;
    } else if (0x1E000 < mg5_state.racers[0].position) {
        mode = 1;
    } else if (0x14000 < mg5_state.racers[0].position) {
        mode = 2;
    } else if (40960 < mg5_state.racers[0].position) {
        mode = 3;
    } else {
        mode = 4;
    }
    anim_select_range(&mg5_state.rowSprites[3], mode);
    position.vx = mg5_state.racers[0].start.vx - 100;
    position.vy = mg5_state.racers[0].start.vy - 220;
    position.vz = mg5_state.racers[0].start.vz + 220;
    state_type = mg5_state.racers[0].status;
    if (state_type != 8 && state_type != 0) {
        mg5_anim_draw_at(&mg5_state.rowSprites[3], &position);
    }
}
/* Draw the two marker sprites at the marker's place. */
static void mg5_draw_marker(void) {
    anim_select_range(&mg5_state.rowSprites[1], 0);
    anim_select_range(&mg5_state.rowSprites[2], 1);
    mg5_anim_draw_at(&mg5_state.rowSprites[1], &mg5_state.marker);
    mg5_sprite_draw_at(&mg5_state.rowSprites[2], &mg5_state.marker);
}

/* Draw the scene's fixed animations and step their three phase machines. */
static void mg5_draw_fixed_anims(void) {
    SVECTOR pos;

    anim_select_range(&mg5_anims[2], 0);
    mg5_anim_draw_at(&mg5_anims[2], &mg5_anim2_pos);
    anim_select_range(&mg5_anims[3], 0);
    mg5_anim_draw_at(&mg5_anims[3], &mg5_anim3_pos);

    switch (mg5_phase_a) {
    case MG5_PHASE_A_WAIT:
        anim_select_range(&mg5_anims[4], 0);
        mg5_anim_draw_at(&mg5_anims[4], &mg5_anim4_pos);
        if (mg5_phase_b >= 30) {
            mg5_phase_a = MG5_PHASE_A_RUN;
        }
        break;
    case MG5_PHASE_A_RUN:
        if (mg5_anims[4].animation != 1) {
            anim_select_range(&mg5_anims[4], 1);
        }
        mg5_anim_draw_at(&mg5_anims[4], &mg5_anim4_pos);
        mg5_anim4_pos.vx += MG5_SLIDE_STEP;
        if (mg5_anim4_pos.vx >= 3514) {
            mg5_phase_a = MG5_PHASE_A_DONE;
        }
        break;
    case MG5_PHASE_A_DONE:
        anim_select_range(&mg5_anims[4], 0);
        mg5_anim_draw_at(&mg5_anims[4], &mg5_anim4_pos);
        break;
    }

    switch (mg5_phase_b) {
    case MG5_PHASE_B_WAIT:
        anim_select_range(&mg5_anims[5], 0);
        mg5_anim_draw_at(&mg5_anims[5], &mg5_anim5_pos);
        if (mg5_state.racers[0].start.vx >= 1680) {
            mg5_phase_b = MG5_PHASE_B_COUNT;
        }
        break;
    case MG5_PHASE_B_DONE:
        anim_select_range(&mg5_anims[5], 0);
        mg5_anim_draw_at(&mg5_anims[5], &mg5_anim5_pos);
        break;
    default:
        mg5_phase_b++;
        if (mg5_anims[5].animation != 1) {
            anim_select_range(&mg5_anims[5], 1);
        }
        mg5_anim_draw_at(&mg5_anims[5], &mg5_anim5_pos);
        mg5_anim5_pos.vx += MG5_SLIDE_STEP;
        if (mg5_anim5_pos.vx >= 4006) {
            mg5_phase_b = MG5_PHASE_B_DONE;
        }
        break;
    }

    switch (mg5_request_flag) {
    case MG5_SCREEN_REST:
        mg5_fall_step = 60;
        break;
    case MG5_SCREEN_RAISE:
        anim_select_range(&mg5_anims[0], 0);
        mg5_anim_draw_at(&mg5_anims[0], &mg5_screen_pos);
        mg5_screen_pos.vy -= mg5_fall_step;
        mg5_fall_step -= mg5_fall_step / 3;
        if (mg5_fall_step < 6) {
            mg5_request_flag = MG5_SCREEN_DROP;
        }
        break;
    case MG5_SCREEN_DROP:
        anim_select_range(&mg5_anims[0], 1);
        mg5_anim_draw_at(&mg5_anims[0], &mg5_screen_pos);
        mg5_screen_pos.vy += mg5_fall_step;
        mg5_fall_step += mg5_fall_step * 6 / 5;
        if (mg5_screen_pos.vy >= 0) {
            mg5_request_flag = MG5_SCREEN_REST;
            mg5_landing_anim_on = 1;
            sound_play(0x4D);
            anim_select_range(&mg5_state.stageSprite, 0x17);
        }
        break;
    }

    if (mg5_landing_anim_on) {
        pos = mg5_screen_pos;
        pos.vx += 150;
        pos.vy += 100;
        pos.vz += 150;
        mg5_anim_draw_at(&mg5_state.stageSprite, &mg5_screen_pos);
        if (mg5_state.stageSprite.flags & ANIM_FLAG_ENDED) {
            mg5_landing_anim_on = 0;
        }
    }
}

/* Sets the pause bit on the sprites, so their animations stop. */
static void mg5_anims_enable(void) {
    s32 i;

    bits16_set(&mg5_state.rowSprites[0].status, ANIM_STATUS_PAUSED);
    bits16_set(&mg5_state.rowSprites[3].status, ANIM_STATUS_PAUSED);
    bits16_set(&mg5_state.rowSprites[1].status, ANIM_STATUS_PAUSED);
    bits16_set(&mg5_state.rowSprites[2].status, ANIM_STATUS_PAUSED);
    for (i = 0; i < 6; i++) {
        bits16_set(&mg5_state.racers[i].body.status, ANIM_STATUS_PAUSED);
        bits16_set(&mg5_state.racers[i].head.status, ANIM_STATUS_PAUSED);
    }
    for (i = 0; i < 2; i++) {
        bits16_set(&mg5_state.extras[i].body.status, ANIM_STATUS_PAUSED);
        bits16_set(&mg5_state.extras[i].head.status, ANIM_STATUS_PAUSED);
    }
    bits16_set(&mg5_anims[0].status, ANIM_STATUS_PAUSED);
    bits16_set(&mg5_anims[1].status, ANIM_STATUS_PAUSED);
    bits16_set(&mg5_anims[2].status, ANIM_STATUS_PAUSED);
    bits16_set(&mg5_anims[3].status, ANIM_STATUS_PAUSED);
    bits16_set(&mg5_anims[4].status, ANIM_STATUS_PAUSED);
    bits16_set(&mg5_anims[5].status, ANIM_STATUS_PAUSED);
}

/* Clears the pause bit on the sprites, so their animations run again. */
static void mg5_anims_disable(void) {
    s32 i;

    bits16_clear(&mg5_state.rowSprites[0].status, ANIM_STATUS_PAUSED);
    bits16_clear(&mg5_state.rowSprites[3].status, ANIM_STATUS_PAUSED);
    bits16_clear(&mg5_state.rowSprites[1].status, ANIM_STATUS_PAUSED);
    bits16_clear(&mg5_state.rowSprites[2].status, ANIM_STATUS_PAUSED);
    for (i = 0; i < 6; i++) {
        bits16_clear(&mg5_state.racers[i].body.status, ANIM_STATUS_PAUSED);
        bits16_clear(&mg5_state.racers[i].head.status, ANIM_STATUS_PAUSED);
    }
    for (i = 0; i < 2; i++) {
        bits16_clear(&mg5_state.extras[i].body.status, ANIM_STATUS_PAUSED);
        bits16_clear(&mg5_state.extras[i].head.status, ANIM_STATUS_PAUSED);
    }
    bits16_clear(&mg5_anims[0].status, ANIM_STATUS_PAUSED);
    bits16_clear(&mg5_anims[1].status, ANIM_STATUS_PAUSED);
    bits16_clear(&mg5_anims[2].status, ANIM_STATUS_PAUSED);
    bits16_clear(&mg5_anims[3].status, ANIM_STATUS_PAUSED);
    bits16_clear(&mg5_anims[4].status, ANIM_STATUS_PAUSED);
    bits16_clear(&mg5_anims[5].status, ANIM_STATUS_PAUSED);
}

/* Draw the full-screen backdrop sprite, behind the paused scene. */
static void mg5_draw_backdrop(void) {
    u32 x;
    u32 y;
    u_long *q;
    s32 sx;
    s32 sy;
    anim_select_range(&mg5_state.rowSprites[0], 0);
    x = HWD0;
    q = current_frame->otTags[0];
    y = VWD0;
    sx = (s32)((x + (x >> 31)) << 15) >> 16;
    sy = (s32)((y + (y >> 31)) << 15) >> 16;
    anim_draw_sprt_cursor(&q[1], &mg5_state.rowSprites[0], mg5_state.rowSprites[0].animation,
                  mg5_state.rowSprites[0].frame, sx, sy);
}

/* Projects the object's position and draws it. Before the race the racers' bodies get the
 * marker icon; the extras get it throughout, and the fixed animations always do. */
static void mg5_anim_draw_at(AnimState *object, SVECTOR *position) {
    u_long *drawing_area;
    SVECTOR *converted;

    converted = &mg5_converted_vec;
    converted->vx = position->vx - 150;
    converted->vy = position->vy - 100;
    converted->vz = position->vz - 150;
    converted->pad = position->pad;
    drawing_area = &current_frame->ot[gte_project_vertex(&mg5_project_xy, converted) >> 2];
    anim_draw_ft4_advance(drawing_area, object, mg5_project_xy.x, mg5_project_xy.y);

    if (mg5_state.dispatch == 0) {
        /* Racer and extra bodies. */
        if (object == &mg5_state.racers[0].body || object == &mg5_state.racers[1].body ||
            object == &mg5_state.racers[2].body || object == &mg5_state.racers[3].body ||
            object == &mg5_state.racers[4].body || object == &mg5_state.racers[5].body ||
            object == &mg5_state.extras[0].body || object == &mg5_state.extras[1].body) {
            ui_draw_icon_18_sub(drawing_area, mg5_project_xy.x, mg5_project_xy.y, 0);
        }
    } else if (object == &mg5_state.extras[0].body || object == &mg5_state.extras[1].body) {
        /* After the start, only the extras. */
        ui_draw_icon_18_sub(drawing_area, mg5_project_xy.x, mg5_project_xy.y, 0);
    }

    if (object == &mg5_anims[0] || object == &mg5_anims[1] ||
        object == &mg5_anims[2] || object == &mg5_anims[3] ||
        object == &mg5_anims[4] || object == &mg5_anims[5]) {
        ui_draw_icon_18_sub(drawing_area, mg5_project_xy.x, mg5_project_xy.y, 0);
    }
}

static void mg5_sprite_draw_at(AnimState *sprite, SVECTOR *vec) {
    s32 depth;
    mg5_project_vec.vx = vec->vx;
    mg5_project_vec.vy = vec->vy;
    mg5_project_vec.vz = vec->vz;
    mg5_project_vec.pad = vec->pad;
    depth = gte_project_vertex(&mg5_project_xy2, &mg5_project_vec);
    depth += MG5_HEAD_DEPTH_BIAS;
    anim_draw_ft4_advance(&current_frame->ot[depth >> 2], sprite, mg5_project_xy2.x,
                  mg5_project_xy2.y);
}

/* Seed the racers on the grid, then move to the menu prompt. */
static void mg5_step_init_racers(void) {
    mg5_racers_init_start();
    mg5_state.step = MG5_START_MENU_PROMPT;
}
/* Show the prompt, and move on once its window has closed. */
static void mg5_step_menu_prompt(void) {
    s32 value;
    menu_request_open(4, MG5_MENU_PROMPT);
    value = menu_get_phase();
    if (value == TEXT_WINDOW_CLOSED) {
        mg5_state.step = value;
    }
}
/* Pan the camera along the track (33 a frame) to its end. There, show the message and, once it
 * is closed, raise the screen and move on. */
static void mg5_step_scroll_in(void) {
    if (farm_area.mode.scroll.centre.vx >= mg5_state.trackWidth - 320) {
        farm_area.mode.scroll.centre.vx = mg5_state.trackWidth - 320;
        menu_request_open(4, MG5_MENU_SCROLL_IN);
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            mg5_state.step = MG5_START_SCROLL_SETTLE;
            mg5_request_flag = MG5_SCREEN_RAISE;
            mg5_screen_pos.vx = 4300;
            mg5_screen_pos.vy = 0;
            mg5_screen_pos.vz = 1800;
        }
    } else {
        farm_area.mode.scroll.centre.vx = farm_area.mode.scroll.centre.vx + 33;
    }
}
/* Pan the camera back to the player's start (33 a frame). Once it is there, show the message: on
 * choice 1 go on to the start menu, otherwise go back to the grid. */
static void mg5_step_scroll_settle(void) {
    FarmLoadArea *far;
    Mg5State *s;
    s16 far_signed;
    s16 state_signed;
    u16 far_unsigned;
    u16 state_unsigned;
    far = &farm_area;
    far_signed = far->mode.scroll.centre.vx;
    s = &mg5_state;
    state_signed = s->racers[0].start.vx;
    far_unsigned = far->mode.scroll.centre.vx;
    state_unsigned = s->racers[0].start.vx;
    if (state_signed >= far_signed) {
        far->mode.scroll.centre.vx = state_unsigned;
        menu_request_open(4, MG5_MENU_SCROLL_SETTLE);
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            if (menu_choice() == 1)
                s->step = MG5_START_MENU;
            else
                s->step = MG5_START_INIT_RACERS;
        }
    } else {
        far->mode.scroll.centre.vx = far_unsigned - 33;
    }
}
/* Show the start menu; once it is closed, fade out and move on to the race. */
static void mg5_step_start_menu(void) {
    menu_request_open(4, MG5_MENU_START);
    if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
        fade_start_out(MINIGAME_FADE_FRAMES, 0, 2);
        mg5_state.step = MG5_START_RACE;
    }
}
/* Once the fade has settled, set up the race and fade back in. */
static void mg5_step_start_race(void) {
    if (fade_is_settled() == 1) {
        mg5_racers_init_race();
        fade_start_in(MINIGAME_FADE_FRAMES, 0, 2);
        farm_area.mode.scroll.centre.vx = -320;
        mg5_state.step = MG5_START_WAIT_FADE;
    }
}
/* Set up the stage camera and aim the scroll at the first start row. */
static void mg5_scroll_init(void) {
    farm_area.mode.scroll.z = -9000;
    farm_area.mode.scroll.x = -4500;
    farm_area.mode.scroll.unused8 = 0;
    farm_area.mode.scroll.yaw = 0x200;
    farm_area.mode.scroll.unused10 = 0;
    farm_area.mode.scroll.centre.vy = 0;
    farm_area.mode.scroll.centre.vx = mg5_start_rows.first[0].vx;
    farm_area.mode.scroll.centre.vz = mg5_start_rows.first[0].vz;
}
/* Turn the stage camera's offset by its yaw and store the eye's position in the scroll record. */
static void mg5_scroll_camera_rotate(void) {
    CameraWork work;

    work.rotation = GsIDMATRIX;
    work.translation = GsIDMATRIX;
    PushMatrix();
    work.angle.vx = 0;
    work.angle.vz = 0;
    work.angle.vy = farm_area.mode.scroll.yaw;
    RotMatrix(&work.angle, &work.rotation);
    SetRotMatrix(&work.rotation);
    SetTransMatrix(&work.translation);
    work.angle.vx = 0;
    work.angle.vy = 0;
    work.angle.vz = farm_area.mode.scroll.z;
    RotTrans(&work.angle, &work.position, &work.flag);
    PopMatrix();
    farm_area.mode.scroll.a18 = work.position.vx;
    farm_area.mode.scroll.b1A = farm_area.mode.scroll.x + farm_area.mode.scroll.centre.vy;
    farm_area.mode.scroll.c1C = work.position.vz;
}

/* Aim the game camera from the stage camera's eye (offset from its centre) at the centre. */
static void mg5_scroll_camera_apply(void) {
    camera_set_eye_roll(farm_area.mode.scroll.a18 + farm_area.mode.scroll.centre.vx, farm_area.mode.scroll.b1A,
                  farm_area.mode.scroll.c1C + farm_area.mode.scroll.centre.vz, 0);
    camera_set_target(farm_area.mode.scroll.centre.vx, farm_area.mode.scroll.centre.vy,
                  farm_area.mode.scroll.centre.vz);
}

/* Draw the stage's model as seen from the stage camera's eye toward its centre. */
static void mg5_scroll_camera_draw(void) {
    VECTOR eye;
    VECTOR target;

    eye.vx = farm_area.mode.scroll.a18 + farm_area.mode.scroll.centre.vx;
    eye.vy = farm_area.mode.scroll.b1A;
    eye.vz = farm_area.mode.scroll.c1C + farm_area.mode.scroll.centre.vz;
    target.vx = farm_area.mode.scroll.centre.vx;
    target.vy = farm_area.mode.scroll.centre.vy;
    target.vz = farm_area.mode.scroll.centre.vz;
    model_draw_visible(&mg5_model, &eye, &target);
}

/* Read the minigame's stage image (A_FILE member 0) and its texture pack, copy the image
 * into our own buffer and into the farm area's staging copy, and bind it as the stage
 * sprite: the first step of the load. */
static void mg5_stage_data_load(void) {
    u8 *ready;

    DrawSync(0);
    ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
    afile_read_bytes(0, 0x25000, 0x5800, STAGING_BUFFER, 1, 1);
    do {
        ready = afile_read_bytes(0, 0x25000, 0x5800, STAGING_BUFFER, 1, 0);
    } while (ready == 0);
    texture_pack_load(STAGING_BUFFER + STAGE_DATA_SIZE);
    memcpy(mg5_stage_data, STAGING_BUFFER, STAGE_DATA_SIZE);
    memcpy(farm_area.stageData, STAGING_BUFFER, STAGE_DATA_SIZE);
    anim_bind_file(&mg5_state.stageSprite, (AnimFile *)mg5_stage_data);
    anim_select_range(&mg5_state.stageSprite, 0x17);
}

/* Read the minigame's texture pack into the staging buffer and unpack it: the second step of the
 * load. */
static void mg5_staging_load(void) {
    DrawSync(0);
    ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
    afile_read_bytes(0, 0, 0x2000, STAGING_BUFFER, 1, 1);
    do {
    } while (afile_read_bytes(0, 0, 0x2000, STAGING_BUFFER, 1, 0) == 0);
    texture_pack_load(STAGING_BUFFER);
}

/* Load the minigame's sprite sheet and bind every animation to its frames. */
static void mg5_load_sprites(void) {
    u8 *ready;
    AnimFile *frames;
    s32 i;

    DrawSync(0);
    ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
    afile_read_bytes(0x1A, 0, 0x124D0, STAGING_BUFFER, 1, 1);
    do {
        ready = afile_read_bytes(0x1A, 0, 0x124D0, STAGING_BUFFER, 1, 0);
    } while (ready == 0);
    texture_pack_load(&region_base[0x5F18]);
    memcpy(mg5_sprite_sheet, STAGING_BUFFER, sizeof(mg5_sprite_sheet));
    anim_bind_file(&mg5_state.rowSprites[0], (AnimFile *)(mg5_sprite_sheet + mg5_sprite_offsets[0]));
    anim_bind_file(&mg5_state.rowSprites[1], (AnimFile *)(mg5_sprite_sheet + mg5_sprite_offsets[1]));
    anim_bind_file(&mg5_state.rowSprites[2], (AnimFile *)(mg5_sprite_sheet + mg5_sprite_offsets[1]));
    anim_bind_file(&mg5_state.rowSprites[3], (AnimFile *)(mg5_sprite_sheet + mg5_sprite_offsets[2]));
    for (i = 0; i < 6; i++) {
        frames = (AnimFile *)(mg5_sprite_sheet + mg5_sprite_offsets[3 + i]);
        anim_bind_file(&mg5_state.racers[i].body, frames);
        anim_bind_file(&mg5_state.racers[i].head, frames);
    }
    for (i = 0; i < 2; i++) {
        frames = (AnimFile *)(mg5_sprite_sheet + mg5_sprite_offsets[9 + i]);
        anim_bind_file(&mg5_state.extras[i].body, frames);
        anim_bind_file(&mg5_state.extras[i].head, frames);
    }
    for (i = 0; (u32)i < 6; i++) {
        frames = (AnimFile *)(mg5_sprite_sheet + mg5_sprite_offsets[11 + i]);
        anim_bind_file(&mg5_anims[i], frames);
    }
}

/* Load the minigame's section data and model, and install its animation script. */
static void mg5_load_script(void) {
    u32 *sections = &section_table[MG5_STAGE_SECTION];

    afile_read_sectors(0, sections[0], sections[1] - sections[0], 0, farm_area.sectionBuffer);
    afile_read_sectors(0, sections[1], sections[2] - sections[1], 0, region_base + 0x4290);
    texture_pack_load(region_base + 0x4290);
    *(FarmModeScriptImage *)&farm_area.mode.script = *(FarmModeScriptImage *)farm_area.sectionBuffer;
    model_bind_tmd(&mg5_model, farm_area.sectionBuffer + 0x2000);
    farm_area.mode.area = 0x39;
    farm_area.mode.data = &farm_area.mode.script;
    ground_enable_objects(&mg5_model);
    ground_set_tolerance(200);
}

/* Stop the two sound banks and start the minigame's music. */
static void mg5_audio_start(void) {
    sound_release_from(2, 0);
    sound_release_from(1, 0);
    sound_request(999, 0);
    sound_request(MINIGAME_SEQUENCE, 1);
}

/* Empty the eight image slots of the mode script and mark each free (id 0xFFFF). */
static void mg5_slots_clear(void) {
    FarmLoadArea *fs;
    FarmImageSlot *p;
    u32 i;
    u16 value;
    memset(&farm_area.mode.slots, 0, sizeof(farm_area.mode.slots));
    i = 0;
    value = 0xFFFF;
    fs = &farm_area;
    p = fs->mode.slots.entries;
    do {
        p->id = value;
        i++;
        p++;
    } while (i < 8);
}

/* Add the script's frames that are in use to the image slots. */
static void mg5_script_blit_frames(FarmModeScript *script) {
    u32 i = 0;
    u16 sentinel = 0xFFFF;
    FarmBlitFrame *cur = script->frames;
    FarmBlitFrame *callp = cur;
    do {
        if (cur->id != sentinel) mg5_slot_add(callp);
        callp++;
        i++;
        cur++;
    } while (i < 8U);
}
/* Step every used slot unless the game is paused: an image animation (bit 15 of the id)
 * or a rectangle one. */
static void mg5_slots_step(void) {
    FarmImageSlot *entry = farm_area.mode.slots.entries;
    s32 i;

    for (i = 0; i < 8; i++, entry++) {
        if (entry->id == 0xFFFF) continue;
        if (game_state.globalFlags & GLOBAL_FLAG_PAUSED) continue;
        if (entry->id & 0x8000) {
            mg5_slot_image_step(entry);
        } else {
            mg5_slot_rect_step(entry);
        }
    }
}

/* Copy the slot's current frame into VRAM, and advance to the next frame once
 * it has been shown for its duration. */
static void mg5_slot_image_step(FarmImageSlot *slot) {
    RECT rect;
    FarmBlitFrame *frame;

    rect.x = slot->image.x;
    rect.y = slot->image.y;
    rect.w = slot->image.w;
    rect.h = slot->image.h;
    MoveImage(&rect, slot->x, slot->y);
    if (farm_area.mode.place.location == 0x2C && (slot->id & 1)) {
        MoveImage(&rect, slot->x + 8, slot->y);
        MoveImage(&rect, slot->x + 16, slot->y);
    }
    slot->timer++;
    if (slot->timer >= slot->duration) {
        frame = &farm_area.mode.data->frames[slot->next];
        slot->duration = frame->duration;
        slot->next = frame->next;
        slot->x = frame->x;
        slot->y = frame->y;
        slot->image.x = frame->image.x;
        slot->image.y = frame->image.y;
        slot->image.w = frame->image.w;
        slot->image.h = frame->image.h;
        slot->timer = 0;
    }
}

/* Advance a rectangle animation (the slot's image scrolls vertically within itself): move
 * its image in two slices, and step the scroll every `next` frames, wrapping at the image's
 * height. */
static void mg5_slot_rect_step(FarmImageSlot *rect) {
    RECT image;
    u16 height;
    u16 count;
    u16 next;
    image.x = rect->image.x;
    image.y = rect->image.y;
    image.w = rect->image.w;
    image.h = rect->image.h - rect->timer;
    MoveImage(&image, rect->x, rect->y + rect->timer);
    height = rect->timer;
    if (height != 0) {
        image.h = height;
        image.x = rect->image.x;
        image.y = rect->image.y + rect->image.h - height;
        image.w = rect->image.w;
        MoveImage(&image, rect->x, rect->y);
    }
    count = rect->count;
    if (count == rect->next) {
        next = rect->timer + 2;
        rect->timer = next;
        rect->count = 0;
        if (!(rect->timer < rect->image.h)) {
            rect->timer = 0;
        }
    } else {
        rect->count = count + 1;
    }
}

/* Find the slot already playing this frame's animation, or fill the first free one with it. */
static void mg5_slot_add(FarmBlitFrame *frame) {
    FarmImageSlot *slot;
    s32 index;

    slot = farm_area.mode.slots.entries;
    index = 0;
    do {
        if (frame->id == slot->id) {
            return;
        }
        index++;
        slot++;
    } while (index < 8);

    slot = farm_area.mode.slots.entries;
    index = 0;
    do {
        if (slot->id == 0xFFFF) {
            memset(slot, 0, sizeof(FarmImageSlot));
            slot->id = frame->id;
            slot->duration = frame->duration;
            slot->next = frame->next;
            slot->x = frame->x;
            slot->y = frame->y;
            slot->image.x = frame->image.x;
            slot->image.y = frame->image.y;
            slot->image.w = frame->image.w;
            slot->image.h = frame->image.h;
            return;
        }
        index++;
        slot++;
    } while (index < 8);
}

/* Raise a villager's friendship by 20, up to FRIENDSHIP_MAX. The race win gives it to the
 * villagers who watched. */
static void mg5_villager_friendship_add(CharacterId villager) {
    if (game_state.villagers[villager].friendship < 235) {
        game_state.villagers[villager].friendship += 20;
    } else {
        game_state.villagers[villager].friendship = FRIENDSHIP_MAX;
    }
}

/* Raise a villager's affection by 500, capped at 65535. */
static void mg5_villager_affection_add(CharacterId villager) {
    if (game_state.villagers[villager].affection <= 65034) {
        game_state.villagers[villager].affection += 500;
    } else {
        game_state.villagers[villager].affection = 65535;
    }
}

/* Set the background colour in both frame records' draw environments. */
static void mg5_set_bg_color(s16 r, s16 g, s16 b) {
    FrameState *p = frame_records;
    FrameState *q = p + 1;
    p->drawEnv.r0 = r;
    p->drawEnv.g0 = g;
    p->drawEnv.b0 = b;
    q->drawEnv.r0 = r;
    q->drawEnv.g0 = g;
    q->drawEnv.b0 = b;
    p->drawEnv.isbg = 1;
    p[1].drawEnv.isbg = 1;
}

/* Set the background colour part way between the two ends (mg5_lerp_from and mg5_lerp_to) and
 * step that point back and forth. */
static void mg5_bg_fade_step(void) {
    s32 values[3];
    s32 position;

    math_lerp_vec3(mg5_lerp_t, 511, (s32 *)mg5_lerp_from, (s32 *)mg5_lerp_to, values);
    mg5_set_bg_color(values[0], values[1], values[2]);
    if (mg5_lerp_reverse == 0) {
        position = (u16)mg5_lerp_t + 1;
        mg5_lerp_t = position;
        if ((s16)position >= 512) {
            mg5_lerp_reverse = 1;
            mg5_lerp_t = 511;
        }
    } else {
        position = (u16)mg5_lerp_t - 1;
        mg5_lerp_t = position;
        if ((s16)position < 0) {
            mg5_lerp_reverse = 0;
            mg5_lerp_t = 0;
        }
    }
}

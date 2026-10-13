/* Minigame 3: the six-record game where shots are launched at
 * targets. Each record has its own state, sprite and target group. */

#include <sys/types.h>
#include <memory.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include <libspu.h>
#include "types.h"
#include "afile.h"
#include "anim.h"
#include "farm_state.h"
#include "game_types.h"
#include "ground.h"
#include "main.h"
#include "math_util.h"
#include "menu_window.h"
#include "mg3.h"
#include "model.h"
#include "pad.h"
#include "pad_vibration.h"
#include "scene.h"
#include "screen_fade.h"
#include "sound.h"
#include "sound_request.h"
#include "text.h"
#include "texture_pack.h"
#include "ui_icons.h"
#include "villager.h"

/* How many of a group's three records are in play. */
enum Mg3GroupCount {
    MG3_GROUP_ALL_IN_PLAY = 3,
    MG3_GROUP_TWO_IN_PLAY = 2,
};

/* The sounds the minigame plays: the pause, the start and end cue, a hit, a launch and a message's voice. */
enum Mg3Sound {
    MG3_SOUND_PAUSE = 0x36,
    MG3_SOUND_ROUND_CUE = 0x39,     /* starts the play and ends the round */
    MG3_SOUND_HIT = 0x3F,
    MG3_SOUND_LAUNCH = 0x40,
    MG3_SOUND_MESSAGE = 0x45,       /* a message's voice clip; the text waits for it to end */
};

/* The minigame's music, the size of its stage image, and the frames of its fades. */
#define MINIGAME_SEQUENCE 18
#define STAGE_DATA_SIZE 0x13DC
#define MINIGAME_FADE_FRAMES 10
/* Where the stage's sections start in the farm data file's section table. */
#define MG3_STAGE_SECTION 900
/* Frames per second of the round clock, and the frames a message or a caption stays up. */
#define MG3_FPS 30
/* Menu text ids, by the step that opens them. */
#define MG3_MENU_REPLAY_PROMPT 0x29
#define MG3_MENU_CONFIRM 0x18
#define MG3_MENU_ROUND_INTRO 0x19
#define MG3_MENU_START 0x1A
#define MG3_MENU_RESULT 0x1D
#define MG3_MENU_RESULT_NEXT 0x26
#define MG3_MENU_REPLAY 0x28
#define MG3_MESSAGE_FRAMES 150
#define MG3_CAPTION_FRAMES 90
/* The frames a hit record shows its hit before it goes DONE. */
#define MG3_HIT_FRAMES 30
/* What a round gives a watching villager: affection for the bachelorettes, friendship for the others. */
#define MG3_AFFECTION_GAIN 500
#define MG3_FRIENDSHIP_GAIN 20
/* The launch height of a shot, the mark's height over an idle record, and the marker's height above the record's start. */
#define MG3_SHOT_HEIGHT 350
#define MG3_MARK_HEIGHT 350
#define MG3_MARKER_HEIGHT 500
/* Sprites that a pause toggles (sprites 0 to 15). */
#define MG3_SPRITE_COUNT 16

static void mg3_load_step(void);
static void mg3_step_intro_wait(void);
static void mg3_round_dispatch(void);
static void mg3_step_fade_out(void);
static void mg3_leave_minigame(void);
static void mg3_state_reset(void);
static void mg3_round_reset(void);
static void mg3_records_setup(void);
static void mg3_state_begin(void);
static void mg3_state_confirm(void);
static void mg3_state_intro_menu(void);
static void mg3_intro_first_message(void);
static void mg3_intro_second_message(void);
static void mg3_state_start_message(void);
static void mg3_round_begin_play(void);
static void mg3_play_frame(void);
static void mg3_play_finish_step(void);
static void mg3_round_close_out(void);
static void mg3_state_result_message(void);
static void mg3_state_next_course_message(void);
static void mg3_state_message_fade_out(void);
static void mg3_state_fade_in_menu(void);
static void mg3_state_replay_choice(void);
static void mg3_state_replay_menu(void);
static void mg3_state_next_round(void);
static void mg3_award_villagers(void);
static void mg3_pick_next_course(void);
static void mg3_set_group_state(s16 index, s16 value);
static s32 mg3_winning_group(void);
static s16 mg3_count_active_targets(void);
static void mg3_record_set_ready(Mg3Record *p);
static void mg3_check_target_hits(void);
static void mg3_pick_record_lanes(void);
static void mg3_update_record_states(void);
static void mg3_launch_shots(void);
static void mg3_update_targets(void);
static void mg3_draw_all(void);
static void mg3_draw_pause(void);
static void mg3_draw_records(void);
static void mg3_draw_targets(void);
static void mg3_draw_selection_marker(void);
static s32 *mg3_find_target_near(s16 *point, s16 limit, s16 group);
static s16 mg3_count_group_records(s16 group);
static s32 mg3_pick_active_record(s32 group);
static s32 mg3_pick_idle_record(s16 group);
static void mg3_sprites_pause(void);
static void mg3_sprites_unpause(void);
static void mg3_sprites_set_blocks(s32 pause);
static void mg3_villager_add_affection(CharacterId villager);
static s32 mg3_villager_score(CharacterId villagerId);
static s16 mg3_record_speed(CharacterId arg);
static AnimState *mg3_sprite_for_category(s16 category);
static void mg3_record_update_sprite(Mg3Record *self);
static void mg3_target_update_sprite(Mg3Target *target);
static void mg3_draw_marker(s32 markerIndex);
static void mg3_draw_frame(void);
static void mg3_draw_sprite_at(AnimState *object, s16 *coordinates);
static void mg3_load_payload(void);
static void mg3_scroll_camera_reset(void);
static void mg3_scroll_camera_rotate(void);
static void mg3_scroll_camera_apply(void);
static void mg3_scroll_camera_draw(void);
static void mg3_stage_data_load(void);
static void mg3_staging_load(void);
static void mg3_stage_geometry_load(void);
static void mg3_audio_start(void);

#include "overlay/common/farm_section_table.h"
/* The play captions: text a on the player's first input, text b when the player's record hits. */
static u16 mg3_caption_text_a[48] = {
    0x0007, 0x0024, 0x0038, 0x0063, 0x0FFD, 0x0018, 0x002E, 0x0034, 0x0055, 0x0024, 0x0FFD, 0x002E,
    0x0034, 0x0033, 0x0FFD, 0x0020, 0x002B, 0x0031, 0x0024, 0x0020, 0x0023, 0x0038, 0x006C, 0xFFFE,
    0x0018, 0x002E, 0x0034, 0x0FFD, 0x0022, 0x0020, 0x002D, 0x0050, 0x0FFD, 0x0033, 0x0027, 0x0031,
    0x002E, 0x0036, 0x0FFD, 0x0020, 0x002D, 0x0038, 0x002C, 0x002E, 0x0031, 0x0024, 0x006C, 0xFFFF,
};
static u16 mg3_caption_text_b[64] = {
    0x0000, 0x0027, 0x0027, 0x0027, 0x008C, 0x0FFD, 0x0038, 0x002E, 0x0034, 0x0055, 0x0024,
    0x0FFD, 0x002E, 0x0034, 0x0033, 0x0FFD, 0x002E, 0x0025, 0x0FFD, 0x0033, 0x0027, 0x0024,
    0x0FFD, 0x0026, 0x0020, 0x002C, 0x0024, 0x006C, 0xFFFE, 0x0018, 0x002E, 0x0034, 0x0FFD,
    0x0022, 0x0020, 0x002D, 0x0050, 0x0FFD, 0x0033, 0x0027, 0x0031, 0x002E, 0x0036, 0x0FFD,
    0x0020, 0x002D, 0x0038, 0x002C, 0x002E, 0x0031, 0x0024, 0x008C, 0xFFFE, 0x0038, 0x002E,
    0x0034, 0x0FFD, 0x002A, 0x002D, 0x002E, 0x0036, 0x006C, 0xFFFF, 0x0000,
};

/* The round states, in the order of mg3_state_handlers. */
enum Mg3RoundState {
    MG3_STATE_BEGIN = 0,
    MG3_STATE_CONFIRM = 1,
    MG3_STATE_INTRO_MENU = 2,
    MG3_STATE_INTRO_FIRST = 3,
    MG3_STATE_INTRO_SECOND = 4,
    MG3_STATE_START_MESSAGE = 5,
    MG3_STATE_BEGIN_PLAY = 6,
    MG3_STATE_PLAY = 7,
    MG3_STATE_PLAY_FINISH = 8,
    MG3_STATE_CLOSE_OUT = 9,
    MG3_STATE_RESULT_MESSAGE = 10,
    MG3_STATE_NEXT_COURSE_MESSAGE = 11,
    MG3_STATE_MESSAGE_FADE_OUT = 12,
    MG3_STATE_FADE_IN_MENU = 13,
    MG3_STATE_REPLAY_CHOICE = 14,
    MG3_STATE_REPLAY_MENU = 15,
    MG3_STATE_NEXT_ROUND = 16,
    MG3_STATE_AWARD = 17,
    MG3_STATE_PICK_NEXT_COURSE = 18,
};

/* The game's top-level steps, and the round's steps. */
static Func mg3_step_handlers[5] = {
    mg3_load_step, mg3_step_intro_wait, mg3_round_dispatch, mg3_step_fade_out, mg3_leave_minigame,
};
static Func mg3_state_handlers[19] = {
    mg3_state_begin, mg3_state_confirm, mg3_state_intro_menu, mg3_intro_first_message,
    mg3_intro_second_message, mg3_state_start_message, mg3_round_begin_play, mg3_play_frame,
    mg3_play_finish_step, mg3_round_close_out, mg3_state_result_message, mg3_state_next_course_message,
    mg3_state_message_fade_out, mg3_state_fade_in_menu, mg3_state_replay_choice, mg3_state_replay_menu,
    mg3_state_next_round, mg3_award_villagers, mg3_pick_next_course,
};
/* The loading steps, in order. */
enum Mg3LoadStep {
    MG3_LOAD_STAGE_DATA = 0,
    MG3_LOAD_STAGING = 1,
    MG3_LOAD_PAYLOAD = 2,
    MG3_LOAD_GEOMETRY = 3,
    MG3_LOAD_AUDIO = 4,
    MG3_LOAD_FADER_WAIT = 5,
    MG3_LOAD_DONE = 6,
};
static u32 mg3_load_index = MG3_LOAD_STAGE_DATA;
/* The villagers of records 1 and 2 for each team (slot 0 is unused). */
static s16 mg3_record_villagers[4][3] = {
    {0, CHARACTER_DOUG, CHARACTER_ANN}, {0, CHARACTER_ELLI, CHARACTER_STU},
    {0, CHARACTER_RICK, CHARACTER_POPURI}, {0, CHARACTER_DUKE, CHARACTER_MANNA},
};
/* The villagers of records 3 to 5 for each team and round. */
static s16 mg3_record_villagers_round[4][2][3] = {
    {{CHARACTER_POPURI, CHARACTER_KAI, CHARACTER_RICK}, {CHARACTER_STU, CHARACTER_ELLI, CHARACTER_MAY}},
    {{CHARACTER_POPURI, CHARACTER_KAI, CHARACTER_RICK}, {CHARACTER_ANN, CHARACTER_GRAY, CHARACTER_DOUG}},
    {{CHARACTER_STU, CHARACTER_ELLI, CHARACTER_MAY}, {CHARACTER_DOUG, CHARACTER_GRAY, CHARACTER_ANN}},
    {{CHARACTER_STU, CHARACTER_ELLI, CHARACTER_MAY}, {CHARACTER_ANN, CHARACTER_GRAY, CHARACTER_DOUG}},
};
/* The course index roundParams[1] picks, per team and round. */
static s16 mg3_round_params[4][2] = {
    {2, 1}, {2, 0}, {1, 0}, {1, 0},
};
/* The courses the next round is picked from. Record speed is mid for the villagers of these
 * four courses, low for MAY and STU, and base for the rest. */
static u16 mg3_course_ids[4] = {CHARACTER_ANN, CHARACTER_ELLI, CHARACTER_POPURI, CHARACTER_MANNA};
static s16 mg3_speed_base = 480;
static s16 mg3_speed_mid = 420;
static s16 mg3_speed_low = 360;
/* The target animation for each record's aim: the column is the aim's lane (aim % 3). */
static u16 mg3_aim_table[6][3] = {
    {7, 8, 2}, {8, 7, 8}, {5, 8, 7}, {6, 8, 4}, {8, 7, 8}, {3, 8, 7},
};
/* A point in the play field. */
typedef struct Mg3Point {
    u16 x;
    u16 z;
} Mg3Point;

/* The six records' starting points. */
static Mg3Point mg3_points[6] = {
    {-750, -750}, {0, -1500}, {750, -873}, {-750, 750}, {0, 1500}, {750, 750},
};
/* The SPU voice for each sound slot, as a key-status mask; the last slot has none. */
static u32 mg3_voice_masks[25] = {
    SPU_00CH, SPU_01CH, SPU_02CH, SPU_03CH, SPU_04CH, SPU_05CH,
    SPU_06CH, SPU_07CH, SPU_08CH, SPU_09CH, SPU_10CH, SPU_11CH,
    SPU_12CH, SPU_13CH, SPU_14CH, SPU_15CH, SPU_16CH, SPU_17CH,
    SPU_18CH, SPU_19CH, SPU_20CH, SPU_21CH, SPU_22CH, SPU_23CH,
    0,
};
/* The round-intro menu ids of each team (roundParams[0]), the second intro message of each course
 * (roundParams[1]) and the result message of each group's win (roundParams[group]). */
static s16 mg3_menu_ids_a[4] = {0x21, 0x1F, 0x1E, 0x20};
static s16 mg3_menu_ids_b[4] = {0x21, 0x1F, 0x1E, 0x20};
static s16 mg3_menu_ids_c[4] = {0x25, 0x23, 0x22, 0x24};
/* Offset of each piece of the stage payload in mg3_payload. */
static u32 mg3_sprite_offsets[15] = {
    0x0000, 0x0318, 0x036C, 0x0550, 0x080C, 0x0AD8, 0x0D94, 0x1050,
    0x130C, 0x169C, 0x1958, 0x1C14, 0x1DC0, 0x1FE4, 0x2190,
};

static SVECTOR mg3_offset_vec;
static FarmScreenXY mg3_offset_xy;
static u8 mg3_stage_data[STAGE_DATA_SIZE];
/* The part of the staged payload that is kept. */
#define MG3_PAYLOAD_SIZE 0x233C
static u8 mg3_payload[MG3_PAYLOAD_SIZE];
static Mg3State mg3_state;
/* The stage model. */
static Model mg3_model;

/* Run the current entry step with the minigame's text palette. */
void mg3_step_dispatch(void) {
    s32 palette = text_get_palette();
    text_set_palette(3);
    mg3_step_handlers[game_state.progress.entrySubStep]();
    text_set_palette(palette);
}

/* Minigame loading steps: stage data, staging, payload, geometry and audio, then a wait
 * for the fader. When it is ready the state is reset and the game's first step starts. */
static void mg3_load_step(void) {
    switch (mg3_load_index) {
    case MG3_LOAD_STAGE_DATA:
        mg3_stage_data_load();
        mg3_load_index++;
        break;
    case MG3_LOAD_STAGING:
        mg3_staging_load();
        mg3_load_index++;
        break;
    case MG3_LOAD_PAYLOAD:
        mg3_load_payload();
        mg3_load_index++;
        break;
    case MG3_LOAD_GEOMETRY:
        mg3_stage_geometry_load();
        mg3_load_index++;
        break;
    case MG3_LOAD_AUDIO:
        mg3_audio_start();
        mg3_load_index++;
        break;
    case MG3_LOAD_FADER_WAIT:
        if (sound_fader_ready() != 0) {
            mg3_load_index = MG3_LOAD_DONE;
            sound_update();
            sound_seq_start_reverb(0, 0, MINIGAME_SEQUENCE, 127);
        }
        break;
    }
    if (mg3_load_index == MG3_LOAD_DONE) {
        mg3_state_reset();
        mg3_scroll_camera_reset();
        menu_reset();
        fade_start_in(MINIGAME_FADE_FRAMES, 0, 2);
        sound_fade_in(MINIGAME_FADE_FRAMES);
        game_state.progress.entrySubStep++;
        mg3_load_index = MG3_LOAD_STAGE_DATA;
    }
}

/* Draw the stage until the screen fade and the sound fader have both settled, then move on. */
static void mg3_step_intro_wait(void) {
    s32 fade;
    mg3_scroll_camera_rotate();
    mg3_scroll_camera_apply();
    model_load_lighting();
    mg3_scroll_camera_draw();
    mg3_round_reset();
    mg3_set_group_state(0, 0);
    mg3_set_group_state(1, 0);
    mg3_draw_all();
    fade = fade_is_settled() == 1;
    if (sound_fader_mode() == SOUND_FADER_IDLE) {
        fade = (fade + 1) & 0xFF;
    }
    if (fade == 2) {
        game_state.progress.entrySubStep = game_state.progress.entrySubStep + 1;
    }
}

/* Run the menu and the current round state, with the stage drawn behind them. */
static void mg3_round_dispatch(void) {
    menu_run_state();
    mg3_scroll_camera_rotate();
    mg3_scroll_camera_apply();
    model_load_lighting();
    mg3_scroll_camera_draw();
    mg3_state_handlers[mg3_state.state]();
}

/* Start the stage's fade out and the sound fade out, then move on. */
static void mg3_step_fade_out(void) {
    mg3_scroll_camera_rotate();
    mg3_scroll_camera_apply();
    model_load_lighting();
    mg3_scroll_camera_draw();
    mg3_draw_all();
    fade_start_blend(MINIGAME_FADE_FRAMES);
    sound_fade_out(MINIGAME_FADE_FRAMES);
    game_state.progress.entrySubStep++;
}

/* Wait for the fades to settle, then release the minigame's voices and sound banks and leave for the farm. */
static void mg3_leave_minigame(void) {
    s32 fade;

    mg3_scroll_camera_rotate();
    mg3_scroll_camera_apply();
    model_load_lighting();
    mg3_scroll_camera_draw();
    mg3_draw_all();
    fade = fade_is_settled() == 1;
    if (sound_fader_mode() == SOUND_FADER_IDLE) {
        fade = (fade + 1) & 0xFF;
    }
    if (fade == 2) {
        if (mg3_state.effectVoice != SOUND_VOICE_NONE) sound_release_voice(mg3_state.effectVoice);
        if (mg3_state.messageVoice != SOUND_VOICE_NONE) sound_release_voice(mg3_state.messageVoice);
        sound_seq_stop_first(0);
        sound_release_from(2, 0);
        sound_release_from(1, 0);
        game_state.progress.entrySubStep = 0xFF;
        main_request_scene(SCENE_FARM_MINIGAME_EXIT);
        game_state.globalFlags |= GLOBAL_FLAG_MINIGAME_EXIT;
    }
}

/* Reset the round state and the tuning values to their defaults. */
static void mg3_state_reset(void) {
    mg3_state.state = MG3_STATE_BEGIN;
    mg3_state.roundIndex = 0;
    mg3_state.captionText = 0;
    mg3_round_reset();
    mg3_state.roundParams[3] = 0;
    mg3_state.roundParams[4] = 1;
    mg3_state.roundParams[5] = 2;
    mg3_state.roundSeconds = 30;
    mg3_state.laneInterval = 5;
    mg3_state.stateInterval = 1;
    mg3_state.arcScale = 7;
    mg3_state.unusedA = 50;
    mg3_state.unusedB = 10;
    mg3_state.unusedC = 40;
    mg3_state.rivalRange = 300;
    mg3_state.hitRange = 170;
    mg3_state.unusedD = 270;
    mg3_state.unusedE = 255;
    mg3_state.quietRivalEngageRoll = 5;
    mg3_state.quietRivalReadyRoll = 128;
    mg3_state.quietFreeReleaseRoll = 15;
    mg3_state.quietRivalReleaseRoll = 5;
    mg3_state.busyFreeReadyRoll = 115;
    mg3_state.busyFreeReleaseRoll = 30;
    mg3_state.busyRivalEngageRoll = 5;
    mg3_state.busyRivalIdleRoll = 110;
    mg3_state.busyRivalReleaseRoll = 5;
    mg3_state.quietFreeReadyRoll = 100;
    mg3_state.idleTargetChance = 10;
    mg3_state.playerAimChance = 70;
    mg3_state.playerAimScoreDrop = 40;
    mg3_state.idleAimChance = 10;
    mg3_state.fullPlayerAimChance = 60;
    mg3_state.fullPlayerAimScoreDrop = 40;
}

/* Start a round's clock and records: clear the pause, the voices and the sub step, then set up the records. */
static void mg3_round_reset(void) {
    mg3_state.paused = 0;
    mg3_state.effectVoice = SOUND_VOICE_NONE;
    mg3_state.messageVoice = SOUND_VOICE_NONE;
    mg3_state.subStep = 0;
    mg3_state.roundTimer = mg3_state.roundSeconds * MG3_FPS;
    mg3_records_setup();
}

/* Pick the records' villagers from the team and the round, then place the six records at their
 * start points with their score, speed and sprite. */
static void mg3_records_setup(void) {
    s32 i;
    s32 j;
    s16 round;
    s16 x;
    s16 *pos;

    x = game_state.minigames.mg3Team;
    if (x == CHARACTER_ANN) {
        i = 0;
    } else if (x == CHARACTER_ELLI) {
        i = 1;
    } else if (x == CHARACTER_POPURI) {
        i = 2;
    } else {
        i = 3;
    }
    mg3_state.roundParams[0] = i;
    round = mg3_state.roundIndex;
    mg3_state.roundParams[1] = mg3_round_params[i][round];
    mg3_state.records[0].villager = 0;
    mg3_state.records[1].villager = mg3_record_villagers[i][1];
    mg3_state.records[2].villager = mg3_record_villagers[i][2];
    mg3_state.records[3].villager = mg3_record_villagers_round[i][round][0];
    mg3_state.records[4].villager = mg3_record_villagers_round[i][round][1];
    mg3_state.records[5].villager = mg3_record_villagers_round[i][round][2];

    for (i = 0; i < 6; i++) {
        mg3_state.records[i].index = i;
        mg3_state.records[i].inPlay = 1;
        mg3_state.records[i].state = 0;
        pos = mg3_state.records[i].pos;
        pos[1] = 0;
        pos[0] = mg3_points[i].x;
        pos[2] = mg3_points[i].z;
        mg3_state.records[i].score = mg3_villager_score(mg3_state.records[i].villager);
        mg3_state.records[i].speed = mg3_record_speed(mg3_state.records[i].villager);
        mg3_state.records[i].ready = 0;
        /* Aim starts on the middle lane of the cursor's 3..5 range. */
        mg3_state.records[i].aim = 4;
        mg3_state.records[i].hitTimer = 0;
        mg3_state.records[i].sprite = mg3_sprite_for_category(mg3_state.records[i].villager);
        for (j = 0; j < 3; j++) {
            mg3_state.records[i].targets[j].active = 0;
            mg3_state.records[i].targets[j].anim = 0;
            mg3_state.records[i].targets[j].timer = 0;
            mg3_state.records[i].targets[j].start = 0;
            mg3_state.records[i].targets[j].step[0] = 0;
            mg3_state.records[i].targets[j].step[1] = 0;
            mg3_state.records[i].targets[j].step[2] = 0;
            mg3_state.records[i].targets[j].pos[0] = mg3_state.records[i].pos[0];
            mg3_state.records[i].targets[j].pos[1] = mg3_state.records[i].pos[1];
            mg3_state.records[i].targets[j].pos[2] = mg3_state.records[i].pos[2];
        }
    }
}

/* Round state BEGIN: after the first round, ask the replay prompt and go on by its answer. */
static void mg3_state_begin(void) {
    s32 fade;
    s32 phase;
    mg3_round_reset();
    mg3_set_group_state(0, 0);
    mg3_set_group_state(1, 0);
    mg3_draw_all();
    fade = fade_is_settled();
    if (fade == 1) {
        if (mg3_state.roundIndex != 0) {
            menu_request_open(4, MG3_MENU_REPLAY_PROMPT);
            phase = menu_get_phase();
            if (phase == TEXT_WINDOW_CLOSED) {
                if (menu_choice() == fade)
                    mg3_state.state = phase;
                else
                    mg3_state.state = fade;
            }
        } else {
            mg3_state.state = fade;
        }
    }
}

/* Round state CONFIRM: ask the confirm prompt. Choice 1 goes on to the intro; any other asks again. */
static void mg3_state_confirm(void) {
    s32 phase;
    mg3_draw_all();
    menu_request_open(4, MG3_MENU_CONFIRM);
    phase = menu_get_phase();
    if (phase == TEXT_WINDOW_CLOSED) {
        if (menu_choice() == 1)
            mg3_state.state = phase;
        else
            mg3_state.state = MG3_STATE_CONFIRM;
    }
}

/* Round state INTRO_MENU: show the round's intro menu, then close it and start the messages. */
static void mg3_state_intro_menu(void) {
    mg3_round_reset();
    mg3_draw_all();
    menu_request_open(4, MG3_MENU_ROUND_INTRO);
    if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
        menu_begin_close();
        mg3_state.subStep = 0;
        mg3_state.state = MG3_STATE_INTRO_FIRST;
    }
}

/* The two round-intro messages, sequenced by subStep. -1 waits for TEXT_WINDOW_CLOSED and
 * moves on; 2 and up count frames until the voice ends or MG3_MESSAGE_FRAMES pass; 1 waits
 * for TEXT_WINDOW_END_WAIT and plays the message voice; 0 opens the message's menu. */
static void mg3_intro_first_message(void) {
    mg3_draw_all();
    if (mg3_state.subStep == -1 && menu_get_phase() == TEXT_WINDOW_CLOSED) {
        menu_begin_close();
        mg3_state.subStep = 0;
        mg3_state.state = MG3_STATE_INTRO_SECOND;
        return;
    }
    if (mg3_state.subStep >= 2) {
        mg3_state.subStep++;
        if (SpuGetKeyStatus(mg3_voice_masks[mg3_state.messageVoice]) == 0 || mg3_state.subStep >= MG3_MESSAGE_FRAMES) {
            mg3_set_group_state(0, 0);
            text_set_input(1);
            menu_set_sound_enabled(1);
            mg3_state.subStep = -1;
            return;
        }
    }
    if (mg3_state.subStep == 1 && menu_get_phase() == TEXT_WINDOW_END_WAIT) {
        mg3_state.messageVoice = sound_play(MG3_SOUND_MESSAGE);
        mg3_set_group_state(0, 11);
        mg3_state.subStep = 2;
    }
    if (mg3_state.subStep == 0) {
        menu_begin_close();
        text_set_input(0);
        menu_set_sound_enabled(0);
        menu_request_open(4, mg3_menu_ids_a[mg3_state.roundParams[0]]);
        mg3_state.subStep = 1;
    }
}

/* The second round-intro message, on group 1; the same steps as the first. */
static void mg3_intro_second_message(void) {
    mg3_draw_all();
    if (mg3_state.subStep == -1 && menu_get_phase() == TEXT_WINDOW_CLOSED) {
        menu_begin_close();
        mg3_state.subStep = 0;
        mg3_state.state = MG3_STATE_START_MESSAGE;
        return;
    }
    if (mg3_state.subStep >= 2) {
        mg3_state.subStep++;
        if (SpuGetKeyStatus(mg3_voice_masks[mg3_state.messageVoice]) == 0 || mg3_state.subStep >= MG3_MESSAGE_FRAMES) {
            mg3_set_group_state(1, 0);
            text_set_input(1);
            menu_set_sound_enabled(1);
            mg3_state.subStep = -1;
            return;
        }
    }
    if (mg3_state.subStep == 1 && menu_get_phase() == TEXT_WINDOW_END_WAIT) {
        mg3_state.messageVoice = sound_play(MG3_SOUND_MESSAGE);
        mg3_set_group_state(1, 0xB);
        mg3_state.subStep = 2;
    }
    if (mg3_state.subStep == 0) {
        menu_begin_close();
        text_set_input(0);
        menu_set_sound_enabled(0);
        menu_request_open(4, mg3_menu_ids_b[mg3_state.roundParams[1]]);
        mg3_state.subStep = 1;
    }
}

/* Round state START_MESSAGE: open the start message and move on to BEGIN_PLAY. */
static void mg3_state_start_message(void) {
    mg3_round_reset();
    mg3_draw_all();
    menu_begin_close();
    text_set_input(0);
    menu_set_sound_enabled(0);
    menu_request_open(4, MG3_MENU_START);
    mg3_state.subStep = 0;
    mg3_state.state = MG3_STATE_BEGIN_PLAY;
}

/* Round state BEGIN_PLAY: set the records idle, then after the start message (about 60 frames)
 * play the cue, start the round's clock and begin the play. */
static void mg3_round_begin_play(void) {
    s32 remaining;
    s32 counter;
    s32 resetValue;

    resetValue = MG3_RECORD_IDLE;
    remaining = 5;
    do {
        mg3_state.records[remaining].state = resetValue;
        remaining -= 1;
    } while (remaining >= 0);
    mg3_draw_all();
    if (menu_get_phase() == TEXT_WINDOW_END_WAIT) {
        counter = (u16)mg3_state.subStep + 1;
        mg3_state.subStep = counter;
        if ((s16)counter >= 61) {
            menu_begin_close();
            mg3_state.effectVoice = sound_play(MG3_SOUND_ROUND_CUE);
            mg3_state.subStep = 0;
            mg3_state.state = MG3_STATE_PLAY;
            mg3_state.roundTimer = mg3_state.roundSeconds * MG3_FPS;
        }
    }
}

/* Round state PLAY: toggle pause on Start; while unpaused, move the player's cursor (3..5) and act
 * on Triangle and Cross, fire the periodic spawns, age records, run the caption timer, and end the
 * round on time-out or a cleared group. */
static void mg3_play_frame(void) {
    s32 i;

    if (input_pad1->pressed & PAD_START) {
        mg3_state.paused = (mg3_state.paused != 1);
        if (mg3_state.paused == 1) {
            mg3_sprites_pause();
            mg3_state.effectVoice = sound_play(MG3_SOUND_PAUSE);
        } else {
            mg3_sprites_unpause();
        }
    }
    if (mg3_state.paused == 1) {
        mg3_draw_all();
        mg3_draw_pause();
        return;
    }
    if (mg3_state.records[0].inPlay == 1) {
        switch (input_pad1->pressed) {
        case PAD_LEFT:
            mg3_state.records[0].aim--;
            break;
        case PAD_RIGHT:
            mg3_state.records[0].aim++;
            break;
        case PAD_TRIANGLE:
            if (mg3_state.records[0].sprite->flags & ANIM_FLAG_ENDED) {
                mg3_state.records[0].state = MG3_RECORD_ENGAGED;
            }
            break;
        case PAD_CROSS:
            if (mg3_state.records[0].sprite->flags & ANIM_FLAG_ENDED) {
                if (mg3_state.records[0].state != MG3_RECORD_IDLE) {
                    mg3_state.records[0].state = MG3_RECORD_IDLE;
                } else {
                    mg3_record_set_ready(&mg3_state.records[0]);
                }
            }
            break;
        }
        if (mg3_state.records[0].aim < 3) {
            mg3_state.records[0].aim = 5;
        }
        if (mg3_state.records[0].aim >= 6) {
            mg3_state.records[0].aim = 3;
        }
    } else if (mg3_state.subStep == 0 && (input_pad1->pressed & (PAD_TRIANGLE | PAD_CROSS | PAD_RIGHT | PAD_LEFT))) {
        mg3_state.effectVoice = sound_play(2);
        mg3_state.captionText = mg3_caption_text_a;
        mg3_state.subStep = 1;
    }
    if (mg3_state.roundTimer == mg3_state.roundTimer / mg3_state.laneInterval * mg3_state.laneInterval) {
        mg3_pick_record_lanes();
    }
    if (mg3_state.roundTimer == mg3_state.roundTimer / mg3_state.stateInterval * mg3_state.stateInterval) {
        mg3_update_record_states();
    }
    for (i = 0; i < 6; i++) {
        if (mg3_state.records[i].inPlay == 1 && mg3_state.records[i].state == MG3_RECORD_READY
            && (mg3_state.records[i].sprite->flags & ANIM_FLAG_ENDED)) {
            mg3_state.records[i].state = MG3_RECORD_IDLE;
        }
    }
    if (mg3_state.roundTimer == mg3_state.roundTimer / mg3_state.roundParams[4] * mg3_state.roundParams[4]) {
        mg3_launch_shots();
    }
    mg3_update_targets();
    mg3_check_target_hits();
    for (i = 0; i < 6; i++) {
        if (mg3_state.records[i].state == MG3_RECORD_HIT) {
            mg3_state.records[i].hitTimer++;
            if (mg3_state.records[i].hitTimer >= MG3_HIT_FRAMES) {
                mg3_state.records[i].hitTimer = 0;
                mg3_state.records[i].state = MG3_RECORD_DONE;
            }
        }
    }
    mg3_draw_all();
    if (mg3_state.subStep != 0) {
        if (mg3_state.captionText != 0) {
            text_draw_in_frame(&current_frame->otTags[0][3], &current_frame->otTags[0][2], mg3_state.captionText);
        }
        mg3_state.subStep++;
        if (mg3_state.subStep >= MG3_CAPTION_FRAMES) {
            mg3_state.subStep = 0;
        }
    }
    mg3_state.roundTimer--;
    if (mg3_state.roundTimer <= 0 || mg3_count_group_records(0) == 0 || mg3_count_group_records(1) == 0) {
        mg3_state.messageVoice = sound_play(MG3_SOUND_ROUND_CUE);
        menu_begin_close();
        mg3_state.state = MG3_STATE_PLAY_FINISH;
        mg3_state.subStep = 0;
    }
}

/* Round state PLAY_FINISH: the round is over. Age the HIT records into DONE, open the result
 * message once the round's voice ends or after MG3_MESSAGE_FRAMES, and move to CLOSE_OUT once
 * no target is active and the message window has reached its end wait. */
static void mg3_play_finish_step(void) {
    s32 i;

    mg3_update_targets();
    mg3_check_target_hits();
    for (i = 0; i < 6; i++) {
        if (mg3_state.records[i].state == MG3_RECORD_HIT) {
            mg3_state.records[i].hitTimer++;
            if (mg3_state.records[i].hitTimer >= MG3_HIT_FRAMES) {
                mg3_state.records[i].hitTimer = 0;
                mg3_state.records[i].state = MG3_RECORD_DONE;
            }
        }
    }
    mg3_draw_all();
    mg3_state.subStep++;
    if (SpuGetKeyStatus(mg3_voice_masks[mg3_state.messageVoice]) == 0 || mg3_state.subStep >= MG3_MESSAGE_FRAMES) {
        text_set_input(0);
        menu_set_sound_enabled(0);
        menu_request_open(4, MG3_MENU_RESULT);
    }
    if (mg3_count_active_targets() == 0 && menu_get_phase() == TEXT_WINDOW_END_WAIT) {
        menu_begin_close();
        mg3_state.subStep = 0;
        mg3_state.state = MG3_STATE_CLOSE_OUT;
    }
}

/* Close out the round: a tie (winning group -1) opens the tie message, otherwise the winning
 * group's message. Then park the state machine on the matching step. */
static void mg3_round_close_out(void) {
    s32 result;
    s32 state;

    mg3_draw_all();
    result = mg3_winning_group();
    if (result == -1) {
        menu_request_open(4, MG3_MENU_RESULT_NEXT);
        state = MG3_STATE_MESSAGE_FADE_OUT;
    } else {
        state = MG3_STATE_NEXT_COURSE_MESSAGE;
        if (result == 0) {
            state = MG3_STATE_RESULT_MESSAGE;
        }
        text_set_input(0);
        menu_set_sound_enabled(0);
        menu_request_open(4, mg3_menu_ids_c[mg3_state.roundParams[result]]);
    }
    mg3_state.subStep = 0;
    mg3_state.state = state;
}

/* Round state RESULT_MESSAGE: the group 0 win message. After round 0 it fades out for the next
 * round; after round 1 it goes on to the award. */
static void mg3_state_result_message(void) {
    mg3_draw_all();
    if (mg3_state.subStep == -1 && menu_get_phase() == TEXT_WINDOW_CLOSED) {
        menu_begin_close();
        mg3_state.subStep = 0;
        if (mg3_state.roundIndex == 0) {
            fade_start_out(MINIGAME_FADE_FRAMES, 0, 2);
            mg3_state.state = MG3_STATE_NEXT_ROUND;
        } else {
            mg3_state.state = MG3_STATE_AWARD;
        }
        return;
    }
    if (mg3_state.subStep > 0) {
        mg3_state.subStep++;
        if (SpuGetKeyStatus(mg3_voice_masks[mg3_state.messageVoice]) == 0 || mg3_state.subStep >= MG3_MESSAGE_FRAMES) {
            mg3_set_group_state(0, 0);
            text_set_input(1);
            menu_set_sound_enabled(1);
            mg3_state.subStep = -1;
        }
    }
    if (mg3_state.subStep == 0 && menu_get_phase() == TEXT_WINDOW_END_WAIT) {
        mg3_state.messageVoice = sound_play(MG3_SOUND_MESSAGE);
        mg3_set_group_state(0, 11);
        mg3_state.subStep = 1;
    }
}

/* Round state NEXT_COURSE_MESSAGE: the group 1 win message, then the next course is picked. */
static void mg3_state_next_course_message(void) {
    mg3_draw_all();
    if (mg3_state.subStep == -1 && menu_get_phase() == TEXT_WINDOW_CLOSED) {
        menu_begin_close();
        mg3_state.subStep = 0;
        mg3_state.state = MG3_STATE_PICK_NEXT_COURSE;
        return;
    }
    if (mg3_state.subStep > 0) {
        mg3_state.subStep++;
        if (SpuGetKeyStatus(mg3_voice_masks[mg3_state.messageVoice]) == 0 || mg3_state.subStep >= MG3_MESSAGE_FRAMES) {
            mg3_set_group_state(1, 0);
            text_set_input(1);
            menu_set_sound_enabled(1);
            mg3_state.subStep = -1;
        }
    }
    if (mg3_state.subStep == 0 && menu_get_phase() == TEXT_WINDOW_END_WAIT) {
        mg3_state.messageVoice = sound_play(MG3_SOUND_MESSAGE);
        mg3_set_group_state(1, 11);
        mg3_state.subStep = 1;
    }
}

/* Round state MESSAGE_FADE_OUT: after the tie message, play sound 0x44 and fade out once it ends. */
static void mg3_state_message_fade_out(void) {
    mg3_draw_all();
    if (mg3_state.subStep > 0) {
        mg3_state.subStep++;
        if (SpuGetKeyStatus(mg3_voice_masks[mg3_state.messageVoice]) == 0 || mg3_state.subStep >= MG3_MESSAGE_FRAMES) {
            menu_begin_close();
            mg3_state.subStep = 0;
            fade_start_out(MINIGAME_FADE_FRAMES, 0, 2);
            mg3_state.state = MG3_STATE_FADE_IN_MENU;
            return;
        }
    }
    if (mg3_state.subStep == 0) {
        if (menu_get_phase() == TEXT_WINDOW_END_WAIT) {
            mg3_state.messageVoice = sound_play(0x44);
            mg3_state.subStep = 1;
        }
    }
}

/* Round state FADE_IN_MENU: when the fade out settles, fade back in and open the replay prompt. */
static void mg3_state_fade_in_menu(void) {
    mg3_draw_all();
    if (fade_is_settled() == 1) {
        mg3_round_reset();
        fade_start_in(MINIGAME_FADE_FRAMES, 0, 2);
        menu_request_open(4, MG3_MENU_REPLAY);
        mg3_state.state = MG3_STATE_REPLAY_CHOICE;
    }
}

/* Round state REPLAY_CHOICE: choice 1 starts the round again at the start message; any other
 * choice opens the confirm (REPLAY_MENU). */
static void mg3_state_replay_choice(void) {
    if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
        if (menu_choice() == 1) {
            mg3_state.state = MG3_STATE_START_MESSAGE;
        } else {
            mg3_state.state = MG3_STATE_REPLAY_MENU;
        }
    }
    mg3_draw_all();
}

/* Round state REPLAY_MENU: ask the confirm prompt; choice 1 starts the round again. */
static void mg3_state_replay_menu(void) {
    menu_request_open(4, MG3_MENU_CONFIRM);
    if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
        if (menu_choice() == 1) {
            mg3_state.state = MG3_STATE_START_MESSAGE;
        }
    }
    mg3_draw_all();
}

/* Round state NEXT_ROUND: when the fade out settles, fade in and start the next round at BEGIN. */
static void mg3_state_next_round(void) {
    mg3_draw_all();
    if (fade_is_settled() == 1) {
        fade_start_in(MINIGAME_FADE_FRAMES, 0, 2);
        mg3_state.state = MG3_STATE_BEGIN;
        mg3_state.roundIndex++;
    }
}

/* Round state AWARD: the villagers of records 1 and 2 get their affection or friendship, the
 * minigame is marked done with its course, and the entry step moves on. */
static void mg3_award_villagers(void) {
    mg3_draw_all();
    mg3_villager_add_affection(mg3_state.records[1].villager);
    mg3_villager_add_affection(mg3_state.records[2].villager);
    {
        u16 course = game_state.minigames.mg3Team;
        s32 counter = game_state.progress.entrySubStep;
        game_state.minigames.mg3Done = 1;
        game_state.minigames.courseId = course;
        game_state.progress.entrySubStep = counter + 1;
    }
}

/* Round state PICK_NEXT_COURSE: in round 0 pick a random team other than the player's; after that
 * the course is round 1's. Then clear mg3Done and move the entry step on. */
static void mg3_pick_next_course(void) {
    mg3_draw_all();
    if (mg3_state.roundIndex == 0) {
        s16 pick = rand_range(4);
        if (mg3_state.roundParams[0] == pick) {
            pick++;
            pick %= 4;
        }
        game_state.minigames.courseId = mg3_course_ids[pick];
    } else {
        game_state.minigames.courseId = mg3_course_ids[mg3_state.roundParams[1]];
    }
    game_state.minigames.mg3Done = 0;
    game_state.progress.entrySubStep++;
}

/* Set the state of the three records in group `index` (0: records 0 to 2, 1: records 3 to 5). */
static void mg3_set_group_state(s16 index, s16 value) {
    s32 x = index * 3;
    s32 y = x + 3;
    while (x < y) {
        mg3_state.records[x].state = value;
        x++;
    }
}

/* The group with more records in play wins: 0 or 1, or -1 on a tie. */
static s32 mg3_winning_group(void) {
    s32 a = mg3_count_group_records(0);
    s32 b = mg3_count_group_records(1);
    if (b < a) {
        return 0;
    }
    if (a < b) {
        return 1;
    }
    return -1;
}

/* Count the active targets across all six records. */
static s16 mg3_count_active_targets(void) {
    s16 found;
    s32 row;
    s32 i;

    found = 0;
    for (row = 0; row < 6; row++) {
        for (i = 0; i < mg3_state.roundParams[5]; i++) {
            if (mg3_state.records[row].targets[i].active == 1) {
                found++;
            }
        }
    }
    return found;
}

/* Arm the record when its aim is on the lanes it fires at: 3 to 5 for group 0, 0 to 2 for group 1. */
static void mg3_record_set_ready(Mg3Record *p) {
    if (p->index < 3) {
        p->ready = ((u16)(p->aim - 3) < 3);
    } else {
        p->ready = (u16)p->aim < 3;
    }
}

/* A record in play hits a target within hitRange of it, unless it is ENGAGED: the target is cleared
 * and the record goes HIT. A hit by the player's record also shows caption b and vibrates the pad.
 * Record 0 is skipped while roundParams[3] is set. */
static void mg3_check_target_hits(void) {
    s32 i;
    s32 active;
    s32 *target;

    for (i = 0; i < 6; i++) {
        if (mg3_state.roundParams[3] != 0 && i == 0) {
            i = 1;
        }
        active = mg3_state.records[i].inPlay;
        if (active == 1) {
            target = mg3_find_target_near(mg3_state.records[i].pos, mg3_state.hitRange, i < 3);
            if (target != 0 && mg3_state.records[i].state != MG3_RECORD_ENGAGED) {
                *target = 0;
                mg3_state.records[i].inPlay = 0;
                mg3_state.records[i].state = MG3_RECORD_HIT;
                mg3_state.records[i].hitTimer = 0;
                mg3_state.effectVoice = sound_play(MG3_SOUND_HIT);
                if (i == 0) {
                    pad_vibration_start(0, 1);
                    mg3_state.captionText = mg3_caption_text_b;
                    mg3_state.subStep = active;
                }
            }
        }
    }
}

/* Pick the aim of records 1 to 5: records 1 and 2 aim at group 1's records, records 3 to 5 at
 * group 0's, by how many records of the target group are in play. */
static void mg3_pick_record_lanes(void) {
    s32 i;

    for (i = 1; i < 3; i++) {
        switch (mg3_count_group_records(1)) {
        case MG3_GROUP_ALL_IN_PLAY:
            mg3_state.records[i].aim = rand_range(3) + 3;
            break;
        case MG3_GROUP_TWO_IN_PLAY:
            if (rand_range(100) < mg3_state.idleTargetChance) {
                mg3_state.records[i].aim = mg3_pick_idle_record(1);
            } else {
                mg3_state.records[i].aim = mg3_pick_active_record(1);
            }
            break;
        default:
            mg3_state.records[i].aim = mg3_pick_active_record(1);
            break;
        }
    }
    for (i = 3; i < 6; i++) {
        switch (mg3_count_group_records(0)) {
        case MG3_GROUP_ALL_IN_PLAY:
            if (rand_range(100) < mg3_state.fullPlayerAimChance - mg3_state.fullPlayerAimScoreDrop * mg3_state.records[i].score / 255) {
                mg3_state.records[i].aim = 0;
            } else {
                mg3_state.records[i].aim = mg3_pick_active_record(2);
            }
            break;
        case MG3_GROUP_TWO_IN_PLAY:
            if (mg3_state.records[0].inPlay == 1) {
                if (rand_range(100) < mg3_state.playerAimChance - mg3_state.playerAimScoreDrop * mg3_state.records[i].score / 255) {
                    mg3_state.records[i].aim = 0;
                } else if (rand_range(100) < mg3_state.idleAimChance) {
                    mg3_state.records[i].aim = mg3_pick_idle_record(0);
                } else {
                    mg3_state.records[i].aim = mg3_pick_active_record(2);
                }
            } else {
                mg3_state.records[i].aim = mg3_pick_active_record(0);
            }
            break;
        default:
            mg3_state.records[i].aim = mg3_pick_active_record(0);
            break;
        }
    }
}

/* Pick the next state of every record but the first: records 0-2 form group 0,
 * 3-5 group 1; the rival group is the other one. */
static void mg3_update_record_states(void) {
    s32 i;
    s32 rival_group;
    s32 own_group;

    for (i = 1; i < 6; i++) {
        rival_group = i < 3;
        own_group = rival_group ^ 1;
        if (mg3_state.records[i].inPlay != 1) {
            continue;
        }
        if (mg3_count_group_records(own_group) >= 2) {
            if (mg3_find_target_near(mg3_state.records[i].pos, mg3_state.rivalRange, rival_group) != 0) {
                if (mg3_state.records[i].state == MG3_RECORD_ENGAGED) {
                    if (rand_range(128) <= mg3_state.busyRivalReleaseRoll) {
                        mg3_state.records[i].state = MG3_RECORD_IDLE;
                    }
                } else if (rand_range(128) > mg3_state.busyRivalEngageRoll) {
                    mg3_state.records[i].state = MG3_RECORD_ENGAGED;
                } else if (rand_range(128) > mg3_state.busyRivalIdleRoll) {
                    mg3_state.records[i].state = MG3_RECORD_IDLE;
                }
            } else if (mg3_state.records[i].state == MG3_RECORD_ENGAGED) {
                if (rand_range(128) > mg3_state.busyFreeReleaseRoll) {
                    mg3_state.records[i].state = MG3_RECORD_IDLE;
                }
            } else if (rand_range(128) > mg3_state.busyFreeReadyRoll) {
                mg3_record_set_ready(&mg3_state.records[i]);
            }
        } else if (mg3_state.records[i].state == MG3_RECORD_ENGAGED) {
            if (mg3_find_target_near(mg3_state.records[i].pos, mg3_state.rivalRange, rival_group) != 0) {
                if (rand_range(128) <= mg3_state.quietRivalReleaseRoll) {
                    mg3_state.records[i].state = MG3_RECORD_IDLE;
                }
            } else if (rand_range(128) > mg3_state.quietFreeReleaseRoll) {
                mg3_state.records[i].state = MG3_RECORD_IDLE;
            }
        } else if (mg3_find_target_near(mg3_state.records[i].pos, mg3_state.rivalRange, rival_group) != 0) {
            if (rand_range(128) > mg3_state.quietRivalEngageRoll) {
                mg3_state.records[i].state = MG3_RECORD_ENGAGED;
            } else if (rand_range(128) <= mg3_state.quietRivalReadyRoll) {
                mg3_record_set_ready(&mg3_state.records[i]);
            }
        } else if (rand_range(128) <= mg3_state.quietFreeReadyRoll) {
            mg3_record_set_ready(&mg3_state.records[i]);
        }
    }
}

static inline void set_vector(s16 *v, s16 x, s16 y, s16 z) {
    v[0] = x;
    v[1] = y;
    v[2] = z;
}

/* Start a shot at the record's position, MG3_SHOT_HEIGHT above the ground. */
static inline void set_launch_position(s16 *pos, s32 record) {
    pos[0] = mg3_state.records[record].pos[0];
    pos[1] = -MG3_SHOT_HEIGHT;
    pos[2] = mg3_state.records[record].pos[2];
}

/* Launch a shot from each record that is armed and idle, whose sprite has finished and whose aim is
 * on its lanes, into a free target slot. */
static void mg3_launch_shots(void) {
    s32 i;
    s32 j;
    s16 dx;
    s16 dz;
    s16 speed;
    s16 steps;
    s16 value;

    for (i = 0; i < 6; i++) {
        if (mg3_state.records[i].inPlay != 1) {
            mg3_state.records[i].ready = 0;
            continue;
        }
        if (mg3_state.records[i].ready != 1) {
            continue;
        }
        if (mg3_state.records[i].state != MG3_RECORD_IDLE) {
            mg3_state.records[i].state = MG3_RECORD_IDLE;
            mg3_state.records[i].ready = 0;
            continue;
        }
        if (!(mg3_state.records[i].sprite->flags & ANIM_FLAG_ENDED)) {
            mg3_state.records[i].ready = 0;
            continue;
        }
        if (i < 3) {
            if (mg3_state.records[i].aim < 3) {
                mg3_state.records[i].ready = 0;
                continue;
            }
        } else {
            if (mg3_state.records[i].aim >= 3) {
                mg3_state.records[i].ready = 0;
                continue;
            }
        }
        for (j = 0; j < mg3_state.roundParams[5]; j++) {
            if (mg3_state.records[i].targets[j].active == 0) {
                break;
            }
        }
        if (j == mg3_state.roundParams[5]) {
            continue;
        }
        mg3_state.records[i].state = MG3_RECORD_READY;
        mg3_state.records[i].ready = 0;
        dx = mg3_state.records[mg3_state.records[i].aim].pos[0] - mg3_state.records[i].pos[0];
        dz = mg3_state.records[mg3_state.records[i].aim].pos[2] - mg3_state.records[i].pos[2];
        speed = mg3_state.records[i].speed * 400 / 3000;
        steps = SquareRoot0(dx * dx + dz * dz);
        value = mg3_state.arcScale * 400 / speed;
        steps = steps / speed;
        dx = dx / steps;
        dz = dz / steps;
        mg3_state.records[i].targets[j].active = 1;
        set_launch_position(mg3_state.records[i].targets[j].pos, i);
        mg3_state.records[i].targets[j].anim =
            mg3_aim_table[i][mg3_state.records[i].aim - mg3_state.records[i].aim / 3 * 3];
        mg3_state.records[i].targets[j].duration = steps;
        mg3_state.records[i].targets[j].timer = value;
        mg3_state.records[i].targets[j].start = value;
        set_vector(mg3_state.records[i].targets[j].step, dx, 0, dz);
        mg3_state.effectVoice = sound_play(MG3_SOUND_LAUNCH);
    }
}

static inline void clear_vector(s16 *v) {
    v[0] = 0;
    v[1] = 0;
    v[2] = 0;
}

/* Move every active target along its arc and retire it once it has landed long enough. */
static void mg3_update_targets(void) {
    s32 row;
    s32 col;
    s32 angle;
    s16 timer;
    s16 start;
    s16 duration;

    for (row = 0; row < 6; row++) {
        for (col = 0; col < mg3_state.roundParams[5]; col++) {
            if (mg3_state.records[row].targets[col].active != 0) {
                timer = --mg3_state.records[row].targets[col].timer;
                mg3_state.records[row].targets[col].pos[0] += mg3_state.records[row].targets[col].step[0];
                mg3_state.records[row].targets[col].pos[1] += mg3_state.records[row].targets[col].step[1];
                mg3_state.records[row].targets[col].pos[2] += mg3_state.records[row].targets[col].step[2];
                duration = mg3_state.records[row].targets[col].duration;
                start = mg3_state.records[row].targets[col].start;
                if (duration != 0) {
                    angle = ((start - timer) << 12) / (duration * 2);
                    if (angle < 0x800) {
                        mg3_state.records[row].targets[col].step[1] = -(ccos(angle) * 24 / 4096);
                    } else {
                        mg3_state.records[row].targets[col].step[1] += 12;
                    }
                    if (mg3_state.records[row].targets[col].pos[1] >= 0) {
                        mg3_state.records[row].targets[col].pos[1] = 0;
                        clear_vector(mg3_state.records[row].targets[col].step);
                        mg3_state.records[row].targets[col].anim = 1;
                    }
                    if (mg3_state.records[row].targets[col].timer < -59) {
                        mg3_state.records[row].targets[col].active = 0;
                    }
                }
            }
        }
    }
}

/* Draw the records, the targets and the selection marker. */
static void mg3_draw_all(void) {
    mg3_draw_records();
    mg3_draw_targets();
    mg3_draw_selection_marker();
}

/* Draw the pause frame. */
static void mg3_draw_pause(void) {
    mg3_draw_frame();
}

/* Update the six records' sprites and draw the mark over each idle record whose animation has ended. */
static void mg3_draw_records(void) {
    s32 i;
    s16 result[3];

    for (i = 0; i < 6; i++) {
        mg3_record_update_sprite(&mg3_state.records[i]);
        if (mg3_state.records[i].state == MG3_RECORD_IDLE && (mg3_state.records[i].sprite->flags & ANIM_FLAG_ENDED) != 0) {
            result[0] = mg3_state.records[i].pos[0];
            result[1] = mg3_state.records[i].pos[1] - MG3_MARK_HEIGHT;
            result[2] = mg3_state.records[i].pos[2];
            anim_select_range(&mg3_state.sprites[2], 0);
            mg3_draw_sprite_at(&mg3_state.sprites[2], result);
        }
    }
}

/* Let every active target run its own update. */
static void mg3_draw_targets(void) {
    s32 row;
    s32 col;

    for (row = 0; row < 6; row++) {
        for (col = 0; col < 3; col++) {
            if (mg3_state.records[row].targets[col].active != 0) {
                mg3_target_update_sprite(&mg3_state.records[row].targets[col]);
            }
        }
    }
}

/* Draw the marker at the point of record 0's aim. */
static void mg3_draw_selection_marker(void) {
    mg3_draw_marker(mg3_state.records[0].aim);
}

/* The first active target of `group`'s three records within `limit` of `point`. */
static s32 *mg3_find_target_near(s16 *point, s16 limit, s16 group) {
    s32 row;
    s32 col;
    s16 dx;
    s16 dz;

    for (row = group * 3; row < group * 3 + 3; row++) {
        for (col = 0; col < mg3_state.roundParams[5]; col++) {
            if (mg3_state.records[row].targets[col].active == 1) {
                dx = mg3_state.records[row].targets[col].pos[0] - point[0];
                dz = mg3_state.records[row].targets[col].pos[2] - point[2];
                if ((s16)SquareRoot0(dx * dx + dz * dz) <= limit) {
                    return &mg3_state.records[row].targets[col].active;
                }
            }
        }
    }
    return 0;
}

/* How many of the group's three records are in play. */
static s16 mg3_count_group_records(s16 group) {
    s32 start;
    s32 i;
    s16 found;

    start = group * 3;
    found = 0;
    for (i = start; i < start + 3; i++) {
        if (mg3_state.records[i].inPlay == 1) {
            found++;
        }
    }
    return found;
}

/* A random record in play of `group`: 0 or 1, or 2 for group 0 without the player's record.
 * -1 when none is in play. */
static s32 mg3_pick_active_record(s32 group) {
    s32 i;
    s32 start;
    s16 count;
    s16 values[4];

    group = (s16)group;
    start = (group % 2) * 3;
    count = 0;
    for (i = start; i < start + 3; i++) {
        values[count] = 0;
        if (group == 2 && i == 0) {
            continue;
        }
        if (mg3_state.records[i].inPlay == 1) {
            values[count] = i;
            count++;
        }
    }
    if (count == 0) {
        return -1;
    }
    return values[rand_range((u16)count)];
}

/* A random record of `group` that is out of play, or -1 when all three are in play. */
static s32 mg3_pick_idle_record(s16 group) {
    s32 i;
    s32 start;
    s16 count;
    s16 values[4];

    start = group * 3;
    count = 0;
    for (i = start; i < start + 3; i++) {
        values[count] = 0;
        if (mg3_state.records[i].inPlay == 0) {
            values[count] = i;
            count++;
        }
    }
    if (count == 0) {
        return -1;
    }
    return values[rand_range((u16)count)];
}

static void mg3_sprites_pause(void) {
    mg3_sprites_set_blocks(1);
}

static void mg3_sprites_unpause(void) {
    mg3_sprites_set_blocks(0);
}

/* Pause or resume every sprite (sprites 0 to 15) and every target's sprite, by setting or clearing
 * ANIM_STATUS_PAUSED. */
static void mg3_sprites_set_blocks(s32 pause) {
    Bits16Fn setup;
    Mg3Record *row;
    s32 i;
    s32 slot;

    if (pause) {
        setup = bits16_set;
    } else {
        setup = bits16_clear;
    }
    setup(&mg3_state.sprites[0].status, ANIM_STATUS_PAUSED);
    setup(&mg3_state.sprites[1].status, ANIM_STATUS_PAUSED);
    setup(&mg3_state.sprites[2].status, ANIM_STATUS_PAUSED);
    for (i = 0; i < MG3_SPRITE_COUNT - 3; i++) {
        setup(&mg3_state.sprites[3 + i].status, ANIM_STATUS_PAUSED);
    }
    for (i = 0; i < 6; i++) {
        row = &mg3_state.records[i];
        for (slot = 0; slot < 3; slot++) {
            setup(&row->targets[slot].sprite.status, ANIM_STATUS_PAUSED);
        }
    }
}

/* Reward a villager who watched the round: friendship +20 (a sum of 235 or more is set to the
 * maximum), or affection +500 (capped at 65535; the cap applies once the sum passes 65034). */
static void mg3_villager_add_affection(CharacterId villager) {
    s32 index;
    s32 value;
    s32 result;

    index = villager;
    result = 7;
    if (index == 0) {
        return;
    }
    if (index == CHARACTER_ANN || index == CHARACTER_ELLI || index == CHARACTER_POPURI) {
        value = game_state.villagers[index].affection + MG3_AFFECTION_GAIN;
        result = value > 65034;
        if (result != 0) {
            value = 65535;
        }
        game_state.villagers[index].affection = value;
    } else {
        value = game_state.villagers[index].friendship + MG3_FRIENDSHIP_GAIN;
        result = value < 235;
        if (result == 0) {
            value = FRIENDSHIP_MAX;
        }
        game_state.villagers[index].friendship = value;
    }
}

/* A record's score: the square root of an ANN, ELLI or POPURI's affection, else the friendship. */
static s32 mg3_villager_score(CharacterId villagerId) {
    CharacterId villager;

    villager = villagerId;
    if (villager == CHARACTER_ANN || villager == CHARACTER_ELLI || villager == CHARACTER_POPURI) {
        return (s16)SquareRoot0(game_state.villagers[villager].affection);
    }
    return game_state.villagers[villager].friendship;
}

/* A record's speed: low for MAY and STU, mid for the four course villagers, base for the rest. */
static s16 mg3_record_speed(CharacterId arg) {
    s16 x = arg;
    if (x == CHARACTER_MAY || x == CHARACTER_STU) return mg3_speed_low;
    if (x == CHARACTER_ANN || x == CHARACTER_ELLI || x == CHARACTER_POPURI || x == CHARACTER_MANNA) return mg3_speed_mid;
    return mg3_speed_base;
}

/* The sprite that stands for a record's villager; the villagers without one share sprites[3]. */
static AnimState *mg3_sprite_for_category(s16 category) {
    if (category == CHARACTER_GRAY) return &mg3_state.sprites[15];
    if (category == CHARACTER_DOUG) return &mg3_state.sprites[6];
    if (category == CHARACTER_ANN) return &mg3_state.sprites[5];
    if (category == CHARACTER_ELLI) return &mg3_state.sprites[10];
    if (category == CHARACTER_CLIFF) return &mg3_state.sprites[14];
    if (category == CHARACTER_MAY) return &mg3_state.sprites[13];
    if (category == CHARACTER_RICK) return &mg3_state.sprites[4];
    if (category == CHARACTER_POPURI) return &mg3_state.sprites[11];
    if (category == CHARACTER_DUKE) return &mg3_state.sprites[7];
    if (category == CHARACTER_MANNA) return &mg3_state.sprites[8];
    if (category == CHARACTER_KAI) return &mg3_state.sprites[12];
    if (category == CHARACTER_STU) return &mg3_state.sprites[9];
    return &mg3_state.sprites[3];
}

/* Set the record's animation from its state (group 0's records use the animation after it; DONE flips
 * that and ENGAGED drops it), then draw it at the record's position. */
static void mg3_record_update_sprite(Mg3Record *self) {
    s32 offset = self->index < 3;
    s16 state = self->state;
    if (state == MG3_RECORD_DONE) offset = offset == 0;
    if (state == MG3_RECORD_ENGAGED) offset = 0;
    if ((state + offset) != self->sprite->animation) {
        anim_select_range(self->sprite, (self->state + offset) & 0xFF);
    }
    mg3_draw_sprite_at(self->sprite, self->pos);
}

/* Select the target's animation if it has changed, then draw it at its position. */
static void mg3_target_update_sprite(Mg3Target *target) {
    if (target->anim != target->sprite.animation) {
        anim_select_range(&target->sprite, target->anim);
    }
    mg3_draw_sprite_at(&target->sprite, target->pos);
}

/* Draw the indexed marker: start its animation if it is not already running,
 * then submit it at the table's coordinates, 500 units above the floor. */
static void mg3_draw_marker(s32 markerIndex) {
    s16 index;
    s16 coordinates[3];

    index = markerIndex;
    if (mg3_state.sprites[0].animation != 0xE) {
        anim_select_range(&mg3_state.sprites[0], 0xE);
    }
    coordinates[0] = mg3_points[index].x;
    coordinates[1] = -MG3_MARKER_HEIGHT;
    coordinates[2] = mg3_points[index].z;
    mg3_draw_sprite_at(&mg3_state.sprites[0], coordinates);
}

/* Draw the pause frame (sprites[1]) sized by the display's HWD0 and VWD0, halved. */
static void mg3_draw_frame(void) {
    s32 width;
    s32 height;
    s32 raw_width;
    s32 raw_height;
    u_long *display_buffer;

    anim_select_range(&mg3_state.sprites[1], 0);
    raw_width = HWD0;
    display_buffer = current_frame->otTags[0];
    raw_height = VWD0;
    width = raw_width / 2;
    height = raw_height / 2;
    anim_draw_sprt_cursor(&display_buffer[1], &mg3_state.sprites[1], mg3_state.sprites[1].animation, mg3_state.sprites[1].frame, width, height);
}

/* Project a point (its coordinates moved by a fixed view offset) to the screen and draw the sprite
 * there; records 0, 1, 2 and 5 also get the small icon over it. */
static void mg3_draw_sprite_at(AnimState *object, s16 *coordinates) {
    u_long *drawing_area;

    mg3_offset_vec.vx = coordinates[0] + 150;
    mg3_offset_vec.vy = coordinates[1] - 100;
    mg3_offset_vec.vz = coordinates[2] - 150;
    drawing_area = &current_frame->ot[gte_project_vertex(&mg3_offset_xy, &mg3_offset_vec) >> 2];
    anim_draw_ft4_advance(drawing_area, object, mg3_offset_xy.x, mg3_offset_xy.y);
    if (object == mg3_state.records[0].sprite || object == mg3_state.records[1].sprite ||
        object == mg3_state.records[2].sprite || object == mg3_state.records[5].sprite) {
        ui_draw_icon_18_sub(drawing_area, mg3_offset_xy.x, mg3_offset_xy.y, 0);
    }
}

/* Put the stage camera's angles and distance at their starting values and its centre at the
 * origin. */
static void mg3_scroll_camera_reset(void) {
    farm_area.mode.scroll.z = -9000;
    farm_area.mode.scroll.x = -4500;
    farm_area.mode.scroll.unused8 = 0;
    farm_area.mode.scroll.yaw = -0x200;
    farm_area.mode.scroll.unused10 = 0;
    farm_area.mode.scroll.centre.vx = 0;
    farm_area.mode.scroll.centre.vy = 0;
    farm_area.mode.scroll.centre.vz = 0;
}

/* Turn the stage camera's distance (z) by its yaw to get the eye's x and z, and set the eye's
 * height to x plus the centre's y. The result goes into the shared camera record. */
static void mg3_scroll_camera_rotate(void) {
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
static void mg3_scroll_camera_apply(void) {
    camera_set_eye_roll(farm_area.mode.scroll.a18 + farm_area.mode.scroll.centre.vx, farm_area.mode.scroll.b1A,
                  farm_area.mode.scroll.c1C + farm_area.mode.scroll.centre.vz, 0);
    camera_set_target(farm_area.mode.scroll.centre.vx, farm_area.mode.scroll.centre.vy,
                  farm_area.mode.scroll.centre.vz);
}

/* Draw the stage's model as seen from the stage camera's eye toward its centre. */
static void mg3_scroll_camera_draw(void) {
    VECTOR eye;
    VECTOR target;

    eye.vx = farm_area.mode.scroll.a18 + farm_area.mode.scroll.centre.vx;
    eye.vy = farm_area.mode.scroll.b1A;
    eye.vz = farm_area.mode.scroll.c1C + farm_area.mode.scroll.centre.vz;
    target.vx = farm_area.mode.scroll.centre.vx;
    target.vy = farm_area.mode.scroll.centre.vy;
    target.vz = farm_area.mode.scroll.centre.vz;
    model_draw_visible(&mg3_model, &eye, &target);
}

/* Read the minigame's stage image (A_FILE member 0) and its texture pack, copy the image
 * into our own buffer and into the farm area's staging copy, and bind it as the marker's
 * sprite (sprites[0]): the first step of the load. */
static void mg3_stage_data_load(void) {
    u8 *ready;

    DrawSync(0);
    ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
    afile_read_bytes(0, 0x25000, 0x5800, STAGING_BUFFER, 1, 1);
    do {
        ready = afile_read_bytes(0, 0x25000, 0x5800, STAGING_BUFFER, 1, 0);
    } while (ready == 0);
    texture_pack_load(STAGING_BUFFER + STAGE_DATA_SIZE);
    memcpy(mg3_stage_data, STAGING_BUFFER, STAGE_DATA_SIZE);
    memcpy(farm_area.stageData, STAGING_BUFFER, STAGE_DATA_SIZE);
    anim_bind_file(&mg3_state.sprites[0], (AnimFile *)mg3_stage_data);
}

/* Read the minigame's texture pack into the staging buffer and unpack it. */
static void mg3_staging_load(void) {
    DrawSync(0);
    ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
    afile_read_bytes(0, 0, 0x2000, STAGING_BUFFER, 1, 1);
    do {
    } while (afile_read_bytes(0, 0, 0x2000, STAGING_BUFFER, 1, 0) == 0);
    texture_pack_load(STAGING_BUFFER);
}

/* Load the MG3 stage payload, keep a copy in mg3_payload, and attach its
 * pieces (offsets in mg3_sprite_offsets) to the sprite blocks and every row slot. */
static void mg3_load_payload(void) {
    AnimFile *first;
    Mg3Record *row;
    s32 i;
    s32 slot;

    DrawSync(0);
    ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
    afile_read_bytes(0x16, 0, 0x16188, STAGING_BUFFER, 1, 1);
    do {
    } while (afile_read_bytes(0x16, 0, 0x16188, STAGING_BUFFER, 1, 0) == 0);
    texture_pack_load(STAGING_BUFFER + MG3_PAYLOAD_SIZE);
    memcpy(mg3_payload, STAGING_BUFFER, MG3_PAYLOAD_SIZE);
    first = (AnimFile *)(mg3_payload + mg3_sprite_offsets[0]);
    anim_bind_file(&mg3_state.sprites[2], first);
    anim_bind_file(&mg3_state.sprites[1], (AnimFile *)(mg3_payload + mg3_sprite_offsets[1]));
    for (i = 0; i < 13; i++) {
        anim_bind_file(&mg3_state.sprites[3 + i], (AnimFile *)(mg3_payload + mg3_sprite_offsets[i + 2]));
    }
    for (i = 0; i < 6; i++) {
        row = &mg3_state.records[i];
        for (slot = 0; slot < 3; slot++) {
            anim_bind_file(&row->targets[slot].sprite, first);
        }
    }
}

/* Read the stage's first two file sections (the scene tables, then a texture pack) into the
 * farm area's buffers, keep the table image as the mode script, bind the stage model to
 * the TMD at the section buffer's 0x2000 and set it up for ground tests. */
static void mg3_stage_geometry_load(void) {
    u32 *sections = &section_table[MG3_STAGE_SECTION];

    afile_read_sectors(0, sections[0], sections[1] - sections[0], 0, farm_area.sectionBuffer);
    afile_read_sectors(0, sections[1], sections[2] - sections[1], 0, STAGING_BUFFER);
    texture_pack_load(STAGING_BUFFER);
    *(FarmModeScriptImage *)&farm_area.mode.script = *(FarmModeScriptImage *)farm_area.sectionBuffer;
    model_bind_tmd(&mg3_model, farm_area.sectionBuffer + 0x2000);
    ground_enable_objects(&mg3_model);
    ground_set_tolerance(200);
}

/* Stop the two sound banks and start the minigame's music. */
static void mg3_audio_start(void) {
    sound_release_from(2, 0);
    sound_release_from(1, 0);
    sound_request(SOUND_REQUEST_SEQ_2, 0);
    sound_request(MINIGAME_SEQUENCE, 1);
}

/* Minigame 4: its state machine, the players and records, and the drawing. */

#include <sys/types.h>
#include <memory.h>
#include <abs.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "afile.h"
#include "anim.h"
#include "farm_state.h"
#include "game_types.h"
#include "main.h"
#include "menu_window.h"
#include "mg4.h"
#include "pad.h"
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
/* The dog is the player and the record with index 2. */
#define MG4_DOG 2
#define MG4_RESULT_FRAMES 60
#define MG4_CAMERA_START 0x40000
#define MG4_RECORD_START 0x17000
#define MG4_PLAYER_START (-0x1B000)
/* Where the players and the records stop, in tracks past the finish. */
#define MG4_PLAYER_STOP 55
#define MG4_RECORD_STOP 119
#define MG4_REACH_STEP 50
#define MG4_FULL_TURN 0x1000
#define MG4_TRACK_MIN (-0x30000)
#define MG4_NEAR_BAND 101
/* What a win adds to each villager's friendship. */
#define MG4_FRIENDSHIP_GAIN 20
/* A step of v along one axis when it moves on both: v / sqrt 2. */
#define MG4_DIAGONAL(v) ((v) * 1000 / 1414)

typedef void (*Fn)(void *);

/* Per-player allowed depth range. */
typedef struct Mg4Range {
    s16 min;
    s16 max;
} Mg4Range;

static void mg4_load_steps(void);
static void mg4_step_intro_wait(void);
static void mg4_state_tick(void);
static void mg4_finish(void);
static void mg4_init_state(void);
static void mg4_reset_state(void);
static void mg4_init_records(void);
static void mg4_init_players(void);
static void mg4_state_start(void);
static void mg4_state_intro(void);
static void mg4_state_confirm(void);
static void mg4_state_after_confirm(void);
static void mg4_state_show_results(void);
static void mg4_state_result_wait(void);
static void mg4_play_frame(void);
static void mg4_read_input(void);
static void mg4_records_move(void);
static void mg4_players_think(void);
static void mg4_draw_paused(void);
static void mg4_draw_frame(void);
static void mg4_reset_rings(void);
static void mg4_ring12_push(s32 position, s16 x, s16 y);
static void mg4_draw_entries(void);
static void mg4_shuffle_messages(void);
static void mg4_player_walk_home(Mg4Player *object);
static void mg4_record_walk(Mg4Record *object);
static void mg4_player_move(Mg4Player *mover);
static s32 mg4_pick_direction(Mg4Player *object);
static s32 mg4_move_clear(Mg4Player *player, s16 dir, s16 dist);
static s16 mg4_direction_to_record(Mg4Player *object);
static s16 mg4_angle_to_dir8(u32 value);
static s16 mg4_distance(s16 x1, s16 y1, s16 x2, s16 y2);
static s32 mg4_heading_deg(s32 dx, s32 dz);
static s32 mg4_roll_chance(s16 kind, s16 player);
static s32 mg4_roll_move_kind(s16 player, s16 mode);
static void mg4_record_advance(Mg4Record *record);
static void mg4_player_draw(Mg4Player *actor);
static void mg4_record_draw(Mg4Record *object);
static void mg4_draw_backdrop(void);
static u_long *mg4_ot_slot(s32 x);
static s16 mg4_screen_x(s32 trackX);
static void mg4_player_set_action(Mg4Player *player, s32 kind, s32 dir, s32 timer);
static void mg4_record_set_state(Mg4Record *record, s16 state, s16 direction, s16 timer);
static void mg4_sprites_pause(void);
static void mg4_sprites_resume(void);
static void mg4_draw_staged_sprite(void);
static void mg4_load_sprites(void);
static void mg4_fade_out(void);
static void mg4_stage_data_load(void);
static void mg4_staging_load(void);
static void mg4_audio_start(void);
static void mg4_villager_friendship_add(CharacterId villager);
static void mg4_villager_affection_add(CharacterId villager);

static u16 mg4_message_text[194] = {
    0x0007, 0x0020, 0x002F, 0x002F, 0x0038, 0x0FFD, 0x0005, 0x0020, 0x0031, 0x002C, 0x0051, 0x0FFD,
    0x0012, 0x002C, 0x0028, 0x002B, 0x0024, 0x0038, 0xFFFF, 0x0011, 0x0028, 0x0035, 0x0024, 0x0031,
    0x0FFD, 0x0005, 0x0020, 0x0031, 0x002C, 0x0051, 0x0FFD, 0x000F, 0x0034, 0x0022, 0x0027, 0x0028,
    0xFFFF, 0x0001, 0x0020, 0x002C, 0x0021, 0x0028, 0x0FFD, 0x0005, 0x0020, 0x0031, 0x002C, 0x0051,
    0x0FFD, 0x000E, 0x002D, 0x0038, 0x0037, 0xFFFF, 0x000C, 0x0020, 0x0037, 0x0FFD, 0x0005, 0x0020,
    0x0031, 0x002C, 0x0051, 0x0FFD, 0x0012, 0x002F, 0x0024, 0x0024, 0x0023, 0x0038, 0xFFFF, 0x000C,
    0x0020, 0x0031, 0x002A, 0x0020, 0x0038, 0x0FFD, 0x0005, 0x0020, 0x002C, 0x0028, 0x002B, 0x0038,
    0x0051, 0x0FFD, 0x0003, 0x0038, 0x002B, 0x0020, 0x002D, 0xFFFF, 0x000D, 0x0020, 0x0033, 0x0032,
    0x0034, 0x002C, 0x0024, 0x0FFD, 0x0005, 0x0020, 0x0031, 0x002C, 0x0051, 0x0FFD, 0x000B, 0x0034,
    0x0022, 0x002A, 0x0038, 0x0012, 0x0033, 0x0020, 0x0031, 0xFFFF, 0x0007, 0x0034, 0x002D, 0x0026,
    0x0031, 0x0038, 0x0FFD, 0x0006, 0x0020, 0x0031, 0x0023, 0x0024, 0x002D, 0x0051, 0x0FFD, 0x0009,
    0x0034, 0x002D, 0x0028, 0x002E, 0x0031, 0xFFFF, 0x0013, 0x0069, 0x0001, 0x002E, 0x002D, 0x0024,
    0x0FFD, 0x0005, 0x0020, 0x0031, 0x002C, 0x0051, 0x0FFD, 0x0011, 0x0024, 0x0037, 0xFFFF, 0x0006,
    0x0020, 0x0034, 0x0023, 0x0038, 0x0FFD, 0x0005, 0x0020, 0x002C, 0x0028, 0x002B, 0x0038, 0x0051,
    0x0FFD, 0x0000, 0x002D, 0x0033, 0x002E, 0x002D, 0x0028, 0x002E, 0xFFFF, 0x0006, 0x0031, 0x0024,
    0x0024, 0x002D, 0x0FFD, 0x0005, 0x0020, 0x0031, 0x002C, 0x0051, 0x0FFD, 0x0013, 0x0020, 0x0031,
    0x002E, 0xFFFF,
};

/* The step handlers mg4_run_step dispatches through. */
static Func mg4_step_handlers[5] = {
    mg4_load_steps, mg4_step_intro_wait, mg4_state_tick,
    mg4_fade_out, mg4_finish,
};

/* Mg4State.state values, in mg4_state_handlers order. */
enum Mg4StateId {
    MG4_STATE_START = 0,
    MG4_STATE_INTRO = 1,
    MG4_STATE_CONFIRM = 2,
    MG4_STATE_AFTER_CONFIRM = 3,
    MG4_STATE_SHOW_RESULTS = 4,
    MG4_STATE_RESULT_WAIT = 5,
    MG4_STATE_PLAY_FRAME = 6,
};

/* Indexed by Mg4State.state (mg4_state_tick). */
static Fn mg4_state_handlers[7] = {
    (Fn)mg4_state_start, (Fn)mg4_state_intro, (Fn)mg4_state_confirm, (Fn)mg4_state_after_confirm,
    (Fn)mg4_state_show_results, (Fn)mg4_state_result_wait, (Fn)mg4_play_frame,
};

/* Start offsets of the ten messages in mg4_message_text. */
static s32 mg4_message_offsets[10] = {0x00, 0x26, 0x4A, 0x6C, 0x8E, 0xB8, 0xE8, 0x114, 0x136, 0x162};

/* The steps of mg4_load_steps, in order. */
enum Mg4LoadStep {
    MG4_LOAD_STAGE_DATA = 0,
    MG4_LOAD_STAGING = 1,
    MG4_LOAD_SPRITES = 2,
    MG4_LOAD_AUDIO = 3,
    MG4_LOAD_FADER_WAIT = 4,
    MG4_LOAD_DONE = 5,
};

/* The current step of mg4_load_steps. */
static u8 mg4_load_step = MG4_LOAD_STAGE_DATA;

/* The eight directions of a player's move (Mg4Player.direction), counted clockwise
 * from +z; the diagonals step both axes. */
enum Mg4Direction {
    MG4_DIR_PLUS_Z = 0,
    MG4_DIR_PLUS_Z_PLUS_X = 1,
    MG4_DIR_PLUS_X = 2,
    MG4_DIR_MINUS_Z_PLUS_X = 3,
    MG4_DIR_MINUS_Z = 4,
    MG4_DIR_MINUS_Z_MINUS_X = 5,
    MG4_DIR_MINUS_X = 6,
    MG4_DIR_PLUS_Z_MINUS_X = 7,
};

static s16 mg4_player_z_base[6] = {77, 103, 129, 155, 181, 207};

static Mg4Range mg4_depth_ranges[6] = {
    {64, 116}, {64, 142}, {90, 168}, {116, 194}, {142, 220}, {168, 220},
};

/* Six {clutOffX, clutOffY} pairs, stored flat. */
static u16 mg4_clut_offsets[12] = {
    0x0010, 0x0000, 0x0030, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0001, 0x0020, 0x0001, 0x0010, 0x0001,
};

static s16 mg4_record_clut_y[6] = {0x0004, 0x0002, 0x0000, 0x0000, 0x0005, 0x0001};
static s16 mg4_reload_times[4] = {10, 15, 20, 25};
static u16 mg4_reach_table[6] = {55, 68, 96, 137, 206, 206};
static s16 mg4_hit_points[2][2][3] = {
    {{5, 5, 90}, {60, 35, 5}},
    {{60, 35, 5}, {5, 90, 5}},
};
/* Chance (percent) that a move is taken, indexed [kind][band] (see mg4_roll_chance). */
static s16 mg4_chance_table[4][5] = {
    {100, 100, 100, 100, 100},
    {15, 10, 5, 0, 0},
    {100, 95, 95, 95, 90},
    {100, 95, 95, 90, 90},
};

/* Start offsets of the sprite banks in mg4_sprite_sheet. */
static u32 mg4_sprite_offsets[4] = {0x00000000, 0x00001F50, 0x000022A8, 0x00002600};

static u8 mg4_sprite_sheet[0x29A8];   /* the sprite sheet; mg4_load_sprites copies it in */
static u8 mg4_stage_data[STAGE_DATA_SIZE];   /* mg4_stage_data_load copies this many bytes in */
static Mg4State mg4_state;

/* The minigame's per-frame entry: runs the step handler for entrySubStep with text palette 3. */
void mg4_run_step(void) {
    s32 saved;

    saved = text_get_palette();
    text_set_palette(3);
    mg4_step_handlers[game_state.progress.entrySubStep]();
    text_set_palette(saved);
}
/* Runs one load step per frame. After the last one, sets up the round, fades in and
 * advances the step. */
static void mg4_load_steps(void) {
    switch (mg4_load_step) {
    case MG4_LOAD_STAGE_DATA:
        mg4_stage_data_load();
        mg4_load_step++;
        break;
    case MG4_LOAD_STAGING:
        mg4_staging_load();
        mg4_load_step++;
        break;
    case MG4_LOAD_SPRITES:
        mg4_load_sprites();
        mg4_load_step++;
        break;
    case MG4_LOAD_AUDIO:
        mg4_audio_start();
        mg4_load_step++;
        break;
    case MG4_LOAD_FADER_WAIT:
        if (sound_fader_ready() != 0) {
            mg4_load_step = MG4_LOAD_DONE;
            sound_update();
            sound_seq_start_reverb(0, 0, MINIGAME_SEQUENCE, 127);
        }
        break;
    }
    if (mg4_load_step == MG4_LOAD_DONE) {
        mg4_init_state();
        menu_reset();
        fade_start_in(MINIGAME_FADE_FRAMES, 0, 2);
        sound_fade_in(MINIGAME_FADE_FRAMES);
        game_state.progress.entrySubStep++;
        mg4_load_step = MG4_LOAD_STAGE_DATA;
    }
}
/* Waits for the screen fade and the music fader to settle, then moves on. */
static void mg4_step_intro_wait(void) {
    s32 fade;
    mg4_reset_state();
    mg4_draw_frame();
    fade = fade_is_settled() == 1;
    if (sound_fader_mode() == SOUND_FADER_IDLE) {
        fade = (fade + 1) & 0xFF;
    }
    if (fade == 2) {
        game_state.progress.entrySubStep++;
    }
}
/* Runs the menu, then the handler for mg4_state.state. */
static void mg4_state_tick(void) {
    menu_run_state();
    mg4_state_handlers[mg4_state.state](mg4_state_handlers);
}
/* Start the blend-out of the screen and the music's fade, and move to the next step. */
static void mg4_fade_out(void) {
    fade_start_blend(MINIGAME_FADE_FRAMES);
    sound_fade_out(MINIGAME_FADE_FRAMES);
    game_state.progress.entrySubStep++;
}

/* The exit step: once the fades settle, stops the voice and the sound banks and returns
 * to the farm. */
static void mg4_finish(void) {
    s32 fade;
    fade = fade_is_settled() == 1;
    if (sound_fader_mode() == SOUND_FADER_IDLE) {
        fade = (fade + 1) & 0xFF;
    }
    if (fade == 2) {
        if (mg4_state.voice != SOUND_VOICE_NONE) sound_release_voice(mg4_state.voice);
        sound_seq_stop_first(0);
        sound_release_from(2, 0);
        sound_release_from(1, 0);
        game_state.progress.entrySubStep = 0xFF;
        main_request_scene(SCENE_FARM_MINIGAME_EXIT);
        game_state.globalFlags |= GLOBAL_FLAG_MINIGAME_EXIT;
    }
}
/* Sets the race tuning (finish, spacing, steps, intervals, speeds and band edges) and
 * resets the round. */
static void mg4_init_state(void) {
    mg4_state.state = MG4_STATE_START;
    mg4_state.finishX = 2048;
    mg4_state.spacing = 32;
    mg4_state.recordWalkStep = 35;
    mg4_state.recordRunStep = 65;
    mg4_state.thinkInterval = 2;
    mg4_state.actionInterval = 30;
    mg4_state.playerSpeed = 10;
    mg4_state.walkScale = 4;
    mg4_state.bandEdge0 = 160;
    mg4_state.bandEdge1 = 80;
    mg4_state.bandEdge2 = -80;
    mg4_state.bandEdge3 = -160;
    mg4_reset_state();
    mg4_shuffle_messages();
}
/* Clears the round's flags and camera, then resets the records, players and trails. */
static void mg4_reset_state(void) {
    mg4_state.paused = 0;
    mg4_state.resultLatched = 0;
    mg4_state.finishing = 0;
    mg4_state.voice = SOUND_VOICE_NONE;
    mg4_state.cameraX = MG4_CAMERA_START;
    mg4_state.frameCount = 0;
    mg4_init_records();
    mg4_init_players();
    mg4_reset_rings();
}
/* Sets up the six records: slot, start, clut row and reload time. */
static void mg4_init_records(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
        mg4_state.records[i].slot = i;
        mg4_state.records[i].mode = MG4_MODE_ACTIVE;
        mg4_state.records[i].state = MG4_RECORD_STAND;
        mg4_state.records[i].direction = MG4_DIR_PLUS_X;
        mg4_state.records[i].timer = 0;
        mg4_state.records[i].trackX = MG4_RECORD_START;
        mg4_state.records[i].clutOffY = mg4_record_clut_y[i];
        mg4_state.records[i].reloadTime = mg4_reload_times[rand_range(4)];
    }
}

/* Sets up the six players: ids, starting positions, bands and speeds. */
static void mg4_init_players(void) {
    Mg4State *state = &mg4_state;
    s32 i;

    for (i = 0; i < 6; i++) {
        state->players[i].index = i;
        state->players[i].mode = MG4_MODE_ACTIVE;
        state->players[i].kind = MG4_PLAYER_KIND_STAND;
        state->players[i].direction = MG4_DIR_PLUS_X;
        state->players[i].timer = 0;
        state->players[i].trackX = MG4_PLAYER_START;
        state->players[i].depth = mg4_player_z_base[i] * MG4_UNIT;
        state->players[i].clutOffX = mg4_clut_offsets[i * 2];
        state->players[i].clutOffY = mg4_clut_offsets[i * 2 + 1];
        if (i == 4) {
            state->players[i].x = 0xFF;
            state->players[i].y = 0xFF;
        } else if (i != MG4_DOG) {
            state->players[i].x = rand_range(96) + 160;
            state->players[i].y = rand_range(156) + 100;
        } else {
            state->players[i].x = game_state.animals.dog.actor.affection;
            state->players[i].y = game_state.animals.dog.training;
        }
        state->players[i].reach = mg4_reach_table[state->players[i].y / MG4_REACH_STEP];
        state->players[i].speed = state->walkScale * (state->players[i].x + 128) * 16;
    }
}

/* Resets and draws the round, then moves to the intro. */
static void mg4_state_start(void) {
    mg4_reset_state();
    mg4_draw_frame();
    mg4_state.state = MG4_STATE_INTRO;
}
/* Shows the intro text, then moves to the confirm when its window closes. */
static void mg4_state_intro(void) {
    s32 result;
    mg4_draw_frame();
    menu_request_open(4, 0x2A);
    result = menu_get_phase();
    if (result == TEXT_WINDOW_CLOSED) mg4_state.state = result;
}
/* Asks whether to play: yes starts the cue and goes on, no returns to the intro. */
static void mg4_state_confirm(void) {
    mg4_draw_frame();
    menu_request_open(4, 0x2B);
    if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
        if (menu_choice() == 1) {
            mg4_state.voice = sound_play(0x41);
            mg4_state.state = MG4_STATE_AFTER_CONFIRM;
        } else {
            mg4_state.state = MG4_STATE_INTRO;
        }
    }
}
/* Shows the text after a yes, then moves on to the results. */
static void mg4_state_after_confirm(void) {
    mg4_draw_frame();
    menu_request_open(4, 0x2C);
    if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
        mg4_state.state = MG4_STATE_SHOW_RESULTS;
    }
}

/* Shows the results: the message texts of players 0, 1, 3 and 5, then the window. Closing
 * it moves to MG4_STATE_RESULT_WAIT. */
static void mg4_state_show_results(void) {
    mg4_draw_frame();
    text_set_source(0, (u16 *)((u8 *)mg4_message_text + mg4_message_offsets[mg4_state.messageIds[0]]));
    text_set_source(1, (u16 *)((u8 *)mg4_message_text + mg4_message_offsets[mg4_state.messageIds[1]]));
    text_set_source(2, (u16 *)((u8 *)mg4_message_text + mg4_message_offsets[mg4_state.messageIds[3]]));
    text_set_source(3, (u16 *)((u8 *)mg4_message_text + mg4_message_offsets[mg4_state.messageIds[5]]));
    menu_request_open(4, 0x2D);
    if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
        menu_begin_close();
        text_set_input(0);
        menu_set_sound_enabled(0);
        mg4_state.state = MG4_STATE_RESULT_WAIT;
        mg4_state.resultFrames = 0;
    }
}

/* Result screen: its first frame opens the closing window; after MG4_RESULT_FRAMES frames it
 * plays the cue and hands over to mg4_play_frame. */
static void mg4_state_result_wait(void) {
    mg4_draw_frame();
    if (mg4_state.resultFrames == 0) {
        menu_begin_close();
        text_set_input(0);
        menu_set_sound_enabled(0);
        menu_request_open(4, 0x2E);
    }
    mg4_state.resultFrames++;
    if (mg4_state.resultFrames >= MG4_RESULT_FRAMES) {
        mg4_state.voice = sound_play(0x38);
        menu_begin_close();
        mg4_state.state = MG4_STATE_PLAY_FRAME;
        mg4_state.resultFrames = 0;
    }
}

/* One frame of play: START toggles the pause. Otherwise it steps the records and players,
 * marks each player who passes the finish, and once the dog reaches its stop fades out and
 * exits with the result. */
static void mg4_play_frame(void) {
    s16 settled;
    s32 i;
    s32 limit;

    if (mg4_state.finishing == 0 && (input_pad1->pressed & PAD_START)) {
        mg4_state.paused = mg4_state.paused != 1;
        if (mg4_state.paused == 1) {
            mg4_sprites_pause();
            mg4_state.voice = sound_play(0x36);
        } else {
            mg4_sprites_resume();
        }
    }
    if (mg4_state.paused == 1) {
        mg4_draw_paused();
        return;
    }
    settled = 0;
    mg4_state.frameCount++;
    mg4_read_input();
    mg4_records_move();
    mg4_players_think();
    for (i = 0; i < 6; i++) {
        if (mg4_state.players[i].trackX < mg4_state.finishX * MG4_UNIT) {
            continue;
        }
        mg4_state.players[i].mode = MG4_MODE_FINISHED;
        mg4_state.records[i].mode = MG4_MODE_FINISHED;
        if (mg4_state.resultLatched == 0) {
            mg4_state.resultLatched = 1;
            mg4_state.voice = sound_play(0x43);
            if (i == MG4_DOG) {
                game_state.minigames.mg4PlayerWon = 1;
            } else {
                game_state.minigames.mg4PlayerWon = 0;
                game_state.minigames.mg4Winner = mg4_state.messageIds[i];
            }
        }
        if (i == MG4_DOG && mg4_state.players[MG4_DOG].trackX == (mg4_state.finishX + MG4_PLAYER_STOP) * MG4_UNIT) {
            if (mg4_state.finishing == 0) {
                mg4_state.finishing = 1;
                fade_start_blend(MINIGAME_FADE_FRAMES);
                sound_fade_out(MINIGAME_FADE_FRAMES);
            } else {
                if (fade_is_settled() == 1) {
                    settled++;
                }
                if (sound_fader_mode() == SOUND_FADER_IDLE) {
                    settled++;
                }
                if (settled == 2) {
                    if (mg4_state.voice != SOUND_VOICE_NONE) {
                        sound_release_voice(mg4_state.voice);
                    }
                    sound_seq_stop_first(0);
                    sound_release_from(2, 0);
                    sound_release_from(1, 0);
                    game_state.progress.entrySubStep = 0xFF;
                    main_request_scene(SCENE_FARM_MINIGAME_EXIT);
                    game_state.globalFlags |= GLOBAL_FLAG_MINIGAME_EXIT;
                    if (game_state.minigames.mg4PlayerWon == 1) {
                        mg4_villager_friendship_add(CHARACTER_JEFF);
                        mg4_villager_friendship_add(CHARACTER_SASHA);
                        mg4_villager_friendship_add(CHARACTER_SAIBARA);
                        mg4_villager_friendship_add(CHARACTER_DOUG);
                        mg4_villager_friendship_add(CHARACTER_BARLEY);
                        mg4_villager_friendship_add(CHARACTER_MAY);
                        mg4_villager_friendship_add(CHARACTER_MAYOR_THOMAS);
                        mg4_villager_friendship_add(CHARACTER_RICK);
                        mg4_villager_friendship_add(CHARACTER_DUKE);
                        mg4_villager_friendship_add(CHARACTER_MANNA);
                        mg4_villager_friendship_add(CHARACTER_UNNAMED_26);
                        mg4_villager_friendship_add(CHARACTER_HARRIS);
                        mg4_villager_affection_add(CHARACTER_KAREN);
                        mg4_villager_affection_add(CHARACTER_ANN);
                        mg4_villager_affection_add(CHARACTER_POPURI);
                    }
                }
            }
        }
    }
    if (mg4_state.records[MG4_DOG].trackX >= MG4_CAMERA_START) {
        mg4_state.cameraX = mg4_state.records[MG4_DOG].trackX;
    }
    if (mg4_state.cameraX <= MG4_CAMERA_START) {
        mg4_state.cameraX = MG4_CAMERA_START;
    } else {
        limit = mg4_state.finishX * MG4_UNIT;
        if (mg4_state.cameraX >= limit) {
            mg4_state.cameraX = limit;
        }
    }
    mg4_draw_frame();
}

/* Sets the dog's record from the pad: CIRCLE runs, LEFT and RIGHT walk, and with no
 * direction held it stands. */
static void mg4_read_input(void) {
    s32 flags;
    s16 state;
    s16 direction;
    flags = input_pad1->held;
    if (flags & PAD_CIRCLE)
        state = MG4_RECORD_RUN;
    else
        state = MG4_RECORD_WALK;
    /* PAD_LEFT sends the dog toward the finish (+x). */
    if (flags & PAD_LEFT)
        direction = MG4_DIR_PLUS_X;
    else if (flags & PAD_RIGHT)
        direction = MG4_DIR_MINUS_X;
    else {
        if (state == MG4_RECORD_WALK) state = MG4_RECORD_STAND;
        direction = mg4_state.records[MG4_DOG].direction;
    }
    mg4_record_set_state(&mg4_state.records[MG4_DOG], state, direction, 0);
    mg4_record_advance(&mg4_state.records[MG4_DOG]);
}

/* Each active record counts down its delay, then picks its next move. Then active records
 * advance and finished ones walk to their stop. */
static void mg4_records_move(void)
{
    s32 i;
    s16 dx;
    s32 result;

    for (i = 0; i < 6; i++) {
        if (i == MG4_DOG) {
            continue;
        }
        if (mg4_state.records[i].mode != MG4_MODE_ACTIVE) {
            continue;
        }
        if (mg4_state.records[i].timer > 0) {
            mg4_state.records[i].timer--;
            continue;
        }
        mg4_state.records[i].timer = mg4_state.records[i].reloadTime;
        dx = mg4_state.players[i].trackX / MG4_UNIT - mg4_state.records[i].trackX / MG4_UNIT;
        if (abs(dx) < mg4_state.players[i].reach) {
            result = mg4_roll_move_kind(i, 0);
            switch (result) {
            case MG4_RECORD_STAND:
                if (dx > 0) {
                    mg4_record_set_state(&mg4_state.records[i], MG4_RECORD_STAND, MG4_DIR_PLUS_X, mg4_state.records[i].timer);
                } else {
                    mg4_record_set_state(&mg4_state.records[i], MG4_RECORD_STAND, MG4_DIR_MINUS_X, mg4_state.records[i].timer);
                }
                break;
            case MG4_RECORD_WALK:
                if (dx > 0) {
                    if (mg4_roll_chance(0, i) == 1) {
                        mg4_record_set_state(&mg4_state.records[i], MG4_RECORD_WALK, MG4_DIR_PLUS_X, mg4_state.records[i].timer);
                    } else {
                        mg4_record_set_state(&mg4_state.records[i], MG4_RECORD_WALK, MG4_DIR_MINUS_X, mg4_state.records[i].timer);
                    }
                } else {
                    if (mg4_roll_chance(1, i) == 1) {
                        mg4_record_set_state(&mg4_state.records[i], MG4_RECORD_WALK, MG4_DIR_MINUS_X, mg4_state.records[i].timer);
                    } else {
                        mg4_record_set_state(&mg4_state.records[i], MG4_RECORD_WALK, MG4_DIR_PLUS_X, mg4_state.records[i].timer);
                    }
                }
                break;
            case MG4_RECORD_RUN:
                if (dx > 0) {
                    if (mg4_roll_chance(0, i) == 1) {
                        mg4_record_set_state(&mg4_state.records[i], MG4_RECORD_RUN, MG4_DIR_PLUS_X, mg4_state.records[i].timer);
                    } else {
                        mg4_record_set_state(&mg4_state.records[i], MG4_RECORD_RUN, MG4_DIR_MINUS_X, mg4_state.records[i].timer);
                    }
                } else {
                    if (mg4_roll_chance(1, i) == 1) {
                        mg4_record_set_state(&mg4_state.records[i], MG4_RECORD_RUN, MG4_DIR_MINUS_X, mg4_state.records[i].timer);
                    } else {
                        mg4_record_set_state(&mg4_state.records[i], MG4_RECORD_RUN, MG4_DIR_PLUS_X, mg4_state.records[i].timer);
                    }
                }
                break;
            }
        } else {
            result = mg4_roll_move_kind(i, 1);
            switch (result) {
            case MG4_RECORD_STAND:
                if (dx > 0) {
                    mg4_record_set_state(&mg4_state.records[i], MG4_RECORD_STAND, MG4_DIR_PLUS_X, mg4_state.records[i].timer);
                } else {
                    mg4_record_set_state(&mg4_state.records[i], MG4_RECORD_STAND, MG4_DIR_MINUS_X, mg4_state.records[i].timer);
                }
                break;
            case MG4_RECORD_WALK:
                if (dx > 0) {
                    if (mg4_roll_chance(2, i) == 1) {
                        mg4_record_set_state(&mg4_state.records[i], MG4_RECORD_WALK, MG4_DIR_PLUS_X, mg4_state.records[i].timer);
                    } else {
                        mg4_record_set_state(&mg4_state.records[i], MG4_RECORD_WALK, MG4_DIR_MINUS_X, mg4_state.records[i].timer);
                    }
                } else {
                    if (mg4_roll_chance(3, i) == 1) {
                        mg4_record_set_state(&mg4_state.records[i], MG4_RECORD_WALK, MG4_DIR_MINUS_X, mg4_state.records[i].timer);
                    } else {
                        mg4_record_set_state(&mg4_state.records[i], MG4_RECORD_WALK, MG4_DIR_PLUS_X, mg4_state.records[i].timer);
                    }
                }
                break;
            case MG4_RECORD_RUN:
                if (dx > 0) {
                    if (mg4_roll_chance(2, i) == 1) {
                        mg4_record_set_state(&mg4_state.records[i], MG4_RECORD_RUN, MG4_DIR_PLUS_X, mg4_state.records[i].timer);
                    } else {
                        mg4_record_set_state(&mg4_state.records[i], MG4_RECORD_RUN, MG4_DIR_MINUS_X, mg4_state.records[i].timer);
                    }
                } else {
                    if (mg4_roll_chance(3, i) == 1) {
                        mg4_record_set_state(&mg4_state.records[i], MG4_RECORD_RUN, MG4_DIR_MINUS_X, mg4_state.records[i].timer);
                    } else {
                        mg4_record_set_state(&mg4_state.records[i], MG4_RECORD_RUN, MG4_DIR_PLUS_X, mg4_state.records[i].timer);
                    }
                }
                break;
            }
        }
    }
    for (i = 0; i < 6; i++) {
        if (i == MG4_DOG) {
            continue;
        }
        if (mg4_state.records[i].mode == MG4_MODE_ACTIVE) {
            mg4_record_advance(&mg4_state.records[i]);
        } else if (mg4_state.records[i].mode == MG4_MODE_FINISHED) {
            mg4_record_walk(&mg4_state.records[i]);
        }
    }
}

/* Each active player counts down its timer. On a think frame it picks its next action: a walk
 * or stand towards its record when it is within reach, or a random action otherwise. Runners
 * turn every actionInterval frames. Then active players move and finished ones walk home. */
static void mg4_players_think(void) {
    s16 dx;
    s16 dz;
    s16 dist;
    s16 kind;
    s16 dir;
    s32 root;
    s32 i;

    for (i = 0; i < 6; i++) {
        if (mg4_state.players[i].mode != MG4_MODE_ACTIVE) {
            continue;
        }
        if (mg4_state.players[i].timer > 0) {
            mg4_state.players[i].timer--;
            continue;
        }
        if (mg4_state.frameCount != mg4_state.frameCount / mg4_state.thinkInterval * mg4_state.thinkInterval) {
            continue;
        }
        dx = mg4_state.players[i].trackX / MG4_UNIT - mg4_state.records[i].trackX / MG4_UNIT;
        dz = mg4_state.players[i].depth / MG4_UNIT - mg4_player_z_base[i];
        root = SquareRoot0(dx * dx + dz * dz);
        dist = root < 0 ? -root : root;
        if ((dx < 0 ? -dx : dx) < mg4_state.players[i].reach) {
            if (dist <= mg4_state.spacing) {
                mg4_player_set_action(&mg4_state.players[i], MG4_PLAYER_KIND_STAND, mg4_direction_to_record(&mg4_state.players[i]), mg4_state.thinkInterval);
            } else {
                mg4_player_set_action(&mg4_state.players[i], MG4_PLAYER_KIND_WALK, mg4_pick_direction(&mg4_state.players[i]), mg4_state.thinkInterval);
            }
        } else {
            kind = rand_range(8) + 3;
            if (kind != MG4_PLAYER_KIND_RUN) {
                dir = (rand_range(4) * 2) | 1;
            } else {
                dir = rand_range(5) + 4;
                dir = dir != 8 ? dir : 0;
            }
            mg4_player_set_action(&mg4_state.players[i], kind, dir, 60);
        }
    }
    for (i = 0; i < 6; i++) {
        if (mg4_state.players[i].kind == MG4_PLAYER_KIND_RUN
            && mg4_state.players[i].timer == mg4_state.players[i].timer / mg4_state.actionInterval * mg4_state.actionInterval) {
            dir = rand_range(5) + 4;
            dir = dir != 8 ? dir : 0;
            mg4_player_set_action(&mg4_state.players[i], mg4_state.players[i].kind, dir, mg4_state.players[i].timer);
        }
        if (mg4_state.players[i].mode == MG4_MODE_ACTIVE) {
            mg4_player_move(&mg4_state.players[i]);
        } else if (mg4_state.players[i].mode == MG4_MODE_FINISHED) {
            mg4_player_walk_home(&mg4_state.players[i]);
        }
    }
}

/* While paused: the staged sprite, then the frame. */
static void mg4_draw_paused(void) {
    mg4_draw_staged_sprite();
    mg4_draw_frame();
}
/* Draws the backdrop, then each player and its record, then the trail entries. */
static void mg4_draw_frame(void) {
    s32 i;
    mg4_draw_backdrop();
    for (i = 0; i < 6; i++) {
        mg4_player_draw(&mg4_state.players[i]);
        mg4_record_draw(&mg4_state.records[i]);
    }
    mg4_draw_entries();
}

/* Empties both trail rings (x = -1), seeding the first six slots with the players' depths. */
static void mg4_reset_rings(void) {
    s16 i;

    for (i = 0; i < 54; i++) {
        mg4_state.recordTrail[i].position = MG4_RECORD_START;
        mg4_state.recordTrail[i].x = -1;
        if (i < 6) {
            mg4_state.recordTrail[i].x = mg4_player_z_base[i];
        }
    }
    mg4_state.recordTrailIndex = 6;
    for (i = 0; i < 12; i++) {
        mg4_state.playerTrail[i].position = MG4_PLAYER_START;
        mg4_state.playerTrail[i].x = -1;
        if (i < 6) {
            mg4_state.playerTrail[i].x = mg4_player_z_base[i];
        }
    }
    mg4_state.playerTrailIndex = 6;
}

/* Appends a record's trail point, wrapping after 54 entries. */
static void mg4_ring54_push(s32 position, s16 x, s16 y) {
    if (mg4_state.recordTrailIndex >= 54) mg4_state.recordTrailIndex = 0;
    mg4_state.recordTrail[mg4_state.recordTrailIndex].position = position;
    mg4_state.recordTrail[mg4_state.recordTrailIndex].x = x;
    mg4_state.recordTrail[mg4_state.recordTrailIndex].y = y;
    mg4_state.recordTrailIndex++;
}
/* Appends a player's trail point, wrapping after 12 entries. */
static void mg4_ring12_push(s32 position, s16 x, s16 y) {
    if (mg4_state.playerTrailIndex >= 12) {
        mg4_state.playerTrailIndex = 0;
    }
    mg4_state.playerTrail[mg4_state.playerTrailIndex].position = position;
    mg4_state.playerTrail[mg4_state.playerTrailIndex].x = x;
    mg4_state.playerTrail[mg4_state.playerTrailIndex].y = y;
    mg4_state.playerTrailIndex = mg4_state.playerTrailIndex + 1;
}

/* The backdrop sprite. */
#define MG4_SPRITE (&mg4_state.backdropSprite)
/* Whole-pixel distance of a 20.12 position from the scroll position. */
#define MG4_SCROLL_DX(position) ((position) / MG4_UNIT - mg4_state.cameraX / MG4_UNIT)
/* Draws every live entry of both ring buffers that lies within half a screen of the scroll position. */
static void mg4_draw_entries(void)
{
    s16 i;
    s16 frame;
    s16 screen_x;
    s16 second_half;

    for (i = 0; i < 12; i++) {
        if (mg4_state.playerTrail[i].x != -1) {
            frame = mg4_state.playerTrail[i].y;
            second_half = i - (i / 12) * 12 >= 6;
            frame += second_half * 8;
            if (abs(MG4_SCROLL_DX(mg4_state.playerTrail[i].position)) <= HWD0 / 2) {
                screen_x = HWD0 / 2 - MG4_SCROLL_DX(mg4_state.playerTrail[i].position);
                anim_select_range(MG4_SPRITE, 4);
                anim_set_frame(MG4_SPRITE, frame);
                anim_draw_ft4_advance(&current_frame->otTags[0][52], MG4_SPRITE, screen_x,
                              mg4_state.playerTrail[i].x);
            }
        }
    }
    for (i = 0; i < 54; i++) {
        if (mg4_state.recordTrail[i].x != -1) {
            frame = mg4_state.recordTrail[i].y == 6;
            second_half = i - (i / 12) * 12 >= 6;
            frame |= second_half << 1;
            if (abs(MG4_SCROLL_DX(mg4_state.recordTrail[i].position)) <= HWD0 / 2) {
                screen_x = HWD0 / 2 - MG4_SCROLL_DX(mg4_state.recordTrail[i].position);
                anim_select_range(MG4_SPRITE, 3);
                anim_set_frame(MG4_SPRITE, frame);
                anim_draw_ft4_advance(&current_frame->otTags[0][52], MG4_SPRITE, screen_x,
                              mg4_state.recordTrail[i].x);
            }
        }
    }
}
#undef MG4_SPRITE
#undef MG4_SCROLL_DX

/* Gives each player a message id 0..9, redrawing on a clash. The clash check skips slot 0,
 * so a repeat of slot 0's id can get through. */
static void mg4_shuffle_messages(void) {
    s32 candidate;
    s32 j;
    s32 i;
    u16 *p;
    i = 0;
    p = mg4_state.messageIds;
    do {
        p[i] = 0xFF;
        candidate = rand_range(10);
        j = 0;
        while (j <= i) {
            if (mg4_state.messageIds[j] == candidate) {
                candidate++;
                if (candidate >= 10) candidate = 0;
                j = 0;
            }
            j++;
        }
        p[i] = candidate;
        i++;
    } while (i < 6);
}
/* Walks a finished player to its stop past the finish and back to its depth; it stands once
 * both are reached. */
static void mg4_player_walk_home(Mg4Player *object) {
    s32 target14;
    s32 target18;
    s32 step;
    s32 depth;
    step = (mg4_state.playerSpeed * MG4_UNIT) / 10;
    target14 = (mg4_state.finishX + MG4_PLAYER_STOP) * MG4_UNIT;
    if (object->trackX < target14) {
        mg4_player_set_action(object, MG4_PLAYER_KIND_WALK, MG4_DIR_PLUS_X, 0);
        object->trackX += step;
        if (object->trackX > target14) object->trackX = target14;
    } else if (target14 < object->trackX) {
        mg4_player_set_action(object, MG4_PLAYER_KIND_WALK, MG4_DIR_MINUS_X, 0);
        object->trackX -= step;
        if (object->trackX < target14) object->trackX = target14;
    }
    target18 = (mg4_player_z_base[object->index] * MG4_UNIT);
    depth = object->depth;
    if (depth < target18) {
        object->depth = depth + step;
        if (target18 < object->depth) object->depth = target18;
    } else if (target18 < depth) {
        object->depth = depth - step;
        if (object->depth < target18) object->depth = target18;
    }
    if (object->depth == target18 && object->trackX == target14) mg4_player_set_action(object, MG4_PLAYER_KIND_STAND, MG4_DIR_PLUS_X, 0);
}
/* Walks a finished record to its stop past the finish, then stands it turned back. */
static void mg4_record_walk(Mg4Record *object) {
    s32 current;
    s32 target;
    current = object->trackX;
    target = (mg4_state.finishX + MG4_RECORD_STOP) * MG4_UNIT;
    if (current < target) {
        mg4_record_set_state(object, MG4_RECORD_WALK, MG4_DIR_PLUS_X, 0);
        object->trackX = object->trackX + ((mg4_state.recordWalkStep * MG4_UNIT) / 10);
        if (target < object->trackX) object->trackX = target;
    } else if (target < current) {
        mg4_record_set_state(object, MG4_RECORD_WALK, MG4_DIR_MINUS_X, 0);
        object->trackX = object->trackX - ((mg4_state.recordWalkStep * MG4_UNIT) / 10);
        if (object->trackX < target) object->trackX = target;
    }
    if (object->trackX == target) mg4_record_set_state(object, MG4_RECORD_STAND, MG4_DIR_MINUS_X, 0);
}

/* Moves a player along its direction at its walk or run speed (diagonals at 1000/1414 of it),
 * unless mg4_move_clear refuses, and clamps it to the track. */
static void mg4_player_move(Mg4Player *mover)
{
    s32 speed;
    s32 diagonal;
    s32 step18;
    s32 step14;
    s32 limit;

    switch (mover->kind) {
    case MG4_PLAYER_KIND_WALK:
        speed = mover->speed;
        break;
    case MG4_PLAYER_KIND_RUN:
        speed = (mg4_state.playerSpeed * MG4_UNIT) / 10;
        break;
    default:
        return;
    }
    diagonal = speed * 1000 / 1414;
    if (mg4_move_clear(mover, mover->direction, mg4_state.spacing) == 1) {
        step14 = 0;
        step18 = 0;
        switch (mover->direction) {
        case MG4_DIR_PLUS_Z:
            step18 = speed;
            break;
        case MG4_DIR_PLUS_Z_PLUS_X:
            step18 = diagonal;
            step14 = diagonal;
            break;
        case MG4_DIR_PLUS_X:
            step14 = speed;
            break;
        case MG4_DIR_MINUS_Z_PLUS_X:
            step18 = -diagonal;
            step14 = diagonal;
            break;
        case MG4_DIR_MINUS_Z:
            step18 = -speed;
            break;
        case MG4_DIR_MINUS_Z_MINUS_X:
            step18 = -diagonal;
            step14 = -diagonal;
            break;
        case MG4_DIR_MINUS_X:
            step14 = -speed;
            break;
        case MG4_DIR_PLUS_Z_MINUS_X:
            step18 = diagonal;
            step14 = -diagonal;
            break;
        }
        mover->trackX += step14;
        mover->depth += step18;
        limit = (mg4_state.finishX + MG4_PLAYER_STOP) * MG4_UNIT;
        if (mover->trackX >= limit) {
            mover->trackX = limit;
        }
        if (mover->trackX <= MG4_TRACK_MIN) {
            mover->trackX = MG4_TRACK_MIN;
        }
    }
}

/* Picks the object's next direction: the bucket towards its record, or a random turn of 2 or 4
 * steps (mod 8) when mg4_move_clear does not accept it. */
static s32 mg4_pick_direction(Mg4Player *object) {
    s32 dx;
    s32 dy;
    s16 direction;

    dx = (object->trackX / MG4_UNIT) - (mg4_state.records[object->index].trackX / MG4_UNIT);
    dy = (object->depth / MG4_UNIT) - mg4_player_z_base[object->index];
    SquareRoot0(dx * dx + dy * dy);
    direction = mg4_direction_to_record(object);
    if (mg4_move_clear(object, direction, (mg4_state.spacing * 3) / 2) != 1) {
        direction = direction + (2 + rand_range(2) * 2);
        direction = direction % 8;
    }
    return direction;
}

/* 1 if the player can step dist in dir: its depth stays in its range, and no other record or
 * player within dist is on that bearing. */
static s32 mg4_move_clear(Mg4Player *player, s16 dir, s16 dist) {
    s16 z;
    s32 i;

    switch (dir) {
    case MG4_DIR_MINUS_Z_PLUS_X:
    case MG4_DIR_MINUS_Z_MINUS_X:
        z = player->depth / MG4_UNIT - MG4_DIAGONAL(dist);
        break;
    case MG4_DIR_MINUS_Z:
        z = player->depth / MG4_UNIT - dist;
        break;
    case MG4_DIR_PLUS_Z_PLUS_X:
    case MG4_DIR_PLUS_Z_MINUS_X:
        z = player->depth / MG4_UNIT + MG4_DIAGONAL(dist);
        break;
    case MG4_DIR_PLUS_Z:
        z = dist + player->depth / MG4_UNIT;
        break;
    default:
        z = player->depth / MG4_UNIT;
        break;
    }
    if (z < mg4_depth_ranges[player->index].min) {
        return 0;
    }
    if (mg4_depth_ranges[player->index].max < z) {
        return 0;
    }
    for (i = 0; i < 6; i++) {
        if (i == player->index) {
            continue;
        }
        if (dist < mg4_distance(player->trackX / MG4_UNIT, player->depth / MG4_UNIT,
                                 mg4_state.records[i].trackX / MG4_UNIT, mg4_player_z_base[i])) {
            continue;
        }
        if (dir == mg4_angle_to_dir8(mg4_heading_deg(player->trackX / MG4_UNIT - mg4_state.records[i].trackX / MG4_UNIT,
                                        player->depth / MG4_UNIT - mg4_player_z_base[i]))) {
            return 0;
        }
    }
    for (i = 0; i < 6; i++) {
        if (i == player->index) {
            continue;
        }
        if (dist < mg4_distance(player->trackX / MG4_UNIT, player->depth / MG4_UNIT,
                                 mg4_state.players[i].trackX / MG4_UNIT, mg4_state.players[i].depth / MG4_UNIT)) {
            continue;
        }
        if (dir == mg4_angle_to_dir8(mg4_heading_deg(player->trackX / MG4_UNIT - mg4_state.players[i].trackX / MG4_UNIT,
                                        player->depth / MG4_UNIT - mg4_state.players[i].depth / MG4_UNIT))) {
            return 0;
        }
    }
    return 1;
}

/* Direction bucket from the object's record to its position. */
static s16 mg4_direction_to_record(Mg4Player *object) {
    return mg4_angle_to_dir8(mg4_heading_deg(
        (object->trackX / MG4_UNIT) - (mg4_state.records[object->index].trackX / MG4_UNIT),
        (object->depth / MG4_UNIT) - mg4_player_z_base[object->index]));
}

/* Maps a heading in degrees to a direction 0..7: 45-degree buckets, each centred on a
 * multiple of 45. */
static s16 mg4_angle_to_dir8(u32 value) {
    if (value - 23 < 45) return 5;
    if (value - 68 < 45) return 6;
    if (value - 113 < 45) return 7;
    if (value - 158 < 45) return 0;
    if (value - 203 < 45) return 1;
    if (value - 248 < 45) return 2;
    if (value - 293 < 45) return 3;
    return 4;
}
static s16 mg4_distance(s16 x1, s16 y1, s16 x2, s16 y2) {
    s32 dx = x1 - x2;
    s32 dy = y1 - y2;
    return SquareRoot0(dx * dx + dy * dy);
}
/* The heading from (dx, dz) in degrees, 0..359. */
static s32 mg4_heading_deg(s32 dx, s32 dz) {
    s32 angle;

    angle = ratan2(dx, dz);
    if (angle < 0) {
        angle += MG4_FULL_TURN;
    }
    return angle * 360 / MG4_FULL_TURN;
}

/* Bands the difference against four thresholds, then rolls 0..99 against that band's chance
 * for this player. */
static s32 mg4_roll_chance(s16 kind, s16 player)
{
    s16 difference;
    s32 band;
    s16 chance;

    difference = mg4_state.players[player].x - mg4_state.players[player].y;
    if (difference > mg4_state.bandEdge0) {
        band = 0;
    } else if (difference > mg4_state.bandEdge1) {
        band = 1;
    } else if (difference > mg4_state.bandEdge2) {
        band = 2;
    } else if (difference > mg4_state.bandEdge3) {
        band = 3;
    } else {
        band = 4;
    }
    chance = mg4_chance_table[kind][band];
    return rand_range(100) < chance;
}

/* Rolls a record's next move (RUN, WALK or STAND) from mg4_hit_points, for mode 0 (its player
 * is in reach) or 1 (it is not). */
static s32 mg4_roll_move_kind(s16 player, s16 mode) {
    s16 sum = 0;
    s16 roll;
    s32 near;
    s32 delta;
    s32 i;

    roll = rand_range(100);
    if (mode == 0) {
        near = mg4_state.players[player].kind == MG4_PLAYER_KIND_WALK || mg4_state.players[player].kind == MG4_PLAYER_KIND_STAND;
    } else {
        delta = mg4_state.records[player].trackX / MG4_UNIT - mg4_state.players[player].trackX / MG4_UNIT;
        near = (delta < 0 ? -delta : delta) - mg4_state.players[player].reach < MG4_NEAR_BAND;
    }
    for (i = 0; i < 3; i++) {
        sum += mg4_hit_points[mode][near][i];
        if (roll < sum) {
            break;
        }
    }
    if (i == 0) {
        return MG4_RECORD_RUN;
    }
    if (i == 1) {
        return MG4_RECORD_WALK;
    }
    if (i == 2) {
        return MG4_RECORD_STAND;
    }
    return -1;
}

/* Moves a record along its direction. At either end of the walk it stops and turns back, except
 * the dog's record, which is left at the limit. */
static void mg4_record_advance(Mg4Record *record) {
    s32 delta;
    s32 limit;

    switch (record->state) {
    case MG4_RECORD_STAND:
        return;
    case MG4_RECORD_WALK:
        delta = (mg4_state.recordWalkStep * MG4_UNIT) / 10;
        break;
    case MG4_RECORD_RUN:
        delta = (mg4_state.recordRunStep * MG4_UNIT) / 10;
        break;
    default:
        return;
    }
    if (record->direction == MG4_DIR_MINUS_X) {
        record->trackX -= delta;
    } else {
        record->trackX += delta;
    }
    limit = (mg4_state.finishX + MG4_RECORD_STOP) * MG4_UNIT;
    if (record->trackX >= limit) {
        record->trackX = limit;
        if (record->slot != MG4_DOG) {
            mg4_record_set_state(record, MG4_RECORD_STAND, MG4_DIR_MINUS_X, record->timer);
        }
    }
    if (record->trackX <= MG4_TRACK_MIN) {
        record->trackX = MG4_TRACK_MIN;
        if (record->slot != MG4_DOG) {
            mg4_record_set_state(record, MG4_RECORD_STAND, MG4_DIR_PLUS_X, record->timer);
        }
    }
}

/* Pick the actor's animation frame from its kind, update its sprite, and every
 * 30th tick report its position to mg4_ring12_push. */
static void mg4_player_draw(Mg4Player *actor) {
    s16 frame;
    s32 frame2;
    s16 screenX;
    s16 depthY;
    u_long *image;

    frame2 = -1;
    switch (actor->kind) {
    case MG4_PLAYER_KIND_WALK:
        frame = actor->direction + 13;
        break;
    case MG4_PLAYER_KIND_STAND:
        frame = actor->direction + 21;
        break;
    case MG4_PLAYER_KIND_RUN:
        frame = actor->direction + 1;
        break;
    case 4:
        if (actor->timer >= 31) {
            frame = 0x29;
            frame2 = actor->direction / 2;
        } else {
            frame = actor->direction / 2 + 9;
        }
        break;
    case 5:
        frame = actor->direction / 2 + 29;
        break;
    case 6:
        frame = actor->direction / 2 + 33;
        break;
    case 7:
        frame = 0x29;
        frame2 = actor->direction / 2;
        break;
    case 8:
        if (actor->timer >= 31) {
            frame = 0x29;
            frame2 = actor->direction / 2;
        } else {
            frame = actor->direction / 2 + 42;
        }
        break;
    case 9:
        frame = 0x32;
        frame2 = actor->direction / 2;
        break;
    case 10:
        if (actor->timer >= 31) {
            frame = 0x32;
            frame2 = actor->direction / 2;
        } else {
            frame = actor->direction / 2 + 51;
        }
        break;
    default:
        return;
    }
    if (actor->sprite.animation != frame) {
        anim_select_range(&actor->sprite, frame);
    }
    if (frame2 != -1) {
        anim_set_frame(&actor->sprite, frame2);
    }
    screenX = mg4_screen_x(actor->trackX);
    depthY = actor->depth / MG4_UNIT;
    anim_set_offsets(&actor->sprite, 0, 0, actor->clutOffX, actor->clutOffY);
    image = mg4_ot_slot(actor->depth);
    if (frame2 == -1) {
        anim_draw_ft4_advance(image, &actor->sprite, screenX, depthY);
    } else {
        anim_draw_sprt_cursor(image, &actor->sprite, actor->sprite.animation, actor->sprite.frame, screenX, depthY);
    }
    ui_draw_icon_18_sub(image, screenX, depthY, 0);
    if (mg4_state.paused != 1) {
        u16 counter = mg4_state.frameCount;
        if (counter == (counter + 1) / 30 * 30 - 1) {
            mg4_ring12_push(actor->trackX, depthY, actor->direction);
        }
    }
}

/* Draws a record, and every tenth frame pushes its position onto the record trail. */
static void mg4_record_draw(Mg4Record *object) {
    s16 state;
    s16 depthY;
    u_long *image;
    s16 screenX;
    s32 mode;
    u16 counter;
    state = object->state;
    if (state == MG4_RECORD_STAND) {
        mode = 0;
    } else if (state == MG4_RECORD_WALK) {
        mode = 2;
    } else if (state == MG4_RECORD_RUN) {
        mode = 4;
    } else {
        return;
    }
    if (object->direction == MG4_DIR_MINUS_X) {
        mode = (mode + 1) & 0xFF;
    }

    if (object->sprite.animation != mode) {
        anim_select_range(&object->sprite, mode);
    }
    screenX = mg4_screen_x(object->trackX);
    depthY = mg4_player_z_base[object->slot];
    image = mg4_ot_slot(depthY * MG4_UNIT);
    anim_set_offsets(&object->sprite, 0, 0, 0, object->clutOffY);
    anim_draw_ft4_advance(image, &object->sprite, screenX, depthY);
    ui_draw_icon_18_sub(image, screenX, depthY, 0);
    if (mg4_state.paused != 1) {
        counter = mg4_state.frameCount;
        if (counter == ((((counter + 1) / 10) * 10) - 1)) {
            mg4_ring54_push(object->trackX, depthY, object->direction);
        }
    }
}

/* The backdrop sprite. */
#define MG4_SPRITE (&mg4_state.backdropSprite)

/* The camera's scroll position, in whole pixels. */
#define MG4_SCROLL_PX (mg4_state.cameraX / MG4_UNIT)

/* The scroll positions half a screen plus a margin either side of centre. */
static inline s32 mg4_view_left(s32 span, s32 centre, s32 margin)
{
    return -span / 2 + (centre - margin);
}

static inline s32 mg4_view_right(s32 span, s32 centre, s32 margin)
{
    return span / 2 + (centre + margin);
}

/* Draw the scrolling backdrop: two tiled layers (64 px tiles at 1x, 76 px tiles
 * at 3x the scroll), then the two-part sprite at x = 0 and the one at
 * finishX, each only while within half a screen (HWD0 / 2) plus 32 px. */
static void mg4_draw_backdrop(void)
{
    s16 x;
    s32 span;

    anim_select_range(MG4_SPRITE, 0);
    anim_set_frame(MG4_SPRITE, 0);
    for (x = MG4_SCROLL_PX - MG4_SCROLL_PX / 64 * 64 - 64; x < HWD0 + 64; x += 64) {
        anim_draw_sprt_cursor(&current_frame->otTags[0][54], MG4_SPRITE,
                      mg4_state.backdropSprite.animation, mg4_state.backdropSprite.frame, x, 0);
    }
    anim_select_range(MG4_SPRITE, 1);
    for (x = MG4_SCROLL_PX * 3 - MG4_SCROLL_PX * 3 / 76 * 76 - 76; x < HWD0 + 76; x += 76) {
        anim_draw_sprt_cursor(&current_frame->otTags[0][51], MG4_SPRITE,
                      mg4_state.backdropSprite.animation, mg4_state.backdropSprite.frame, x, 0);
    }
    span = HWD0;
    if (mg4_view_left(span, 0, 32) < MG4_SCROLL_PX && MG4_SCROLL_PX < mg4_view_right(span, 0, 32)) {
        anim_select_range(MG4_SPRITE, 0);
        anim_set_frame(MG4_SPRITE, 1);
        x = HWD0 / 2 + MG4_SCROLL_PX;
        anim_draw_sprt_cursor(&current_frame->otTags[0][53], MG4_SPRITE,
                      mg4_state.backdropSprite.animation, mg4_state.backdropSprite.frame, x - 32, 0);
        anim_select_range(MG4_SPRITE, 2);
        x = HWD0 / 2 + MG4_SCROLL_PX + 64;
        anim_draw_sprt_cursor(&current_frame->otTags[0][53], MG4_SPRITE,
                      mg4_state.backdropSprite.animation, mg4_state.backdropSprite.frame, x - 32, 0);
    }
    span = HWD0;
    if (mg4_view_left(span, mg4_state.finishX, 32) < MG4_SCROLL_PX &&
        MG4_SCROLL_PX < mg4_view_right(span, mg4_state.finishX, 32)) {
        anim_select_range(MG4_SPRITE, 0);
        anim_set_frame(MG4_SPRITE, 1);
        x = HWD0 / 2 + MG4_SCROLL_PX - mg4_state.finishX;
        anim_draw_sprt_cursor(&current_frame->otTags[0][53], MG4_SPRITE,
                      mg4_state.backdropSprite.animation, mg4_state.backdropSprite.frame, x - 32, 0);
    }
}

#undef MG4_SPRITE
#undef MG4_SCROLL_PX

/* The ordering-table slot for a depth: slot 40 at depth 64, one slot lower for each 4 more
 * units. */
static u_long *mg4_ot_slot(s32 x) {
    s32 offset;
    s32 result;
    u_long *p;
    if (x < 0) x += 0xFFF;
    x >>= 12;
    offset = x - 64;
    if (offset >= 0) {
        offset >>= 2;
    } else {
        offset = x;
        offset -= 61;
        offset >>= 2;
    }
    result = 40;
    p = current_frame->otTags[0];
    result -= offset;
    return p + result;
}
/* Screen x of a track position at the camera, or -HWD0 when it is more than a screen away. */
static s16 mg4_screen_x(s32 trackX) {
    s32 span;
    s16 position;

    span = HWD0;
    position = span / 2 - (trackX / MG4_UNIT - mg4_state.cameraX / MG4_UNIT);
    if (position < -span || span < position) {
        return -HWD0;
    }
    return position;
}
/* Sets the action fields of a player. */
static void mg4_player_set_action(Mg4Player *player, s32 kind, s32 dir, s32 timer) {
    player->kind = kind;
    player->direction = dir;
    player->timer = timer;
}
static void mg4_record_set_state(Mg4Record *record, s16 state, s16 direction, s16 timer) {
    record->state = state;
    record->direction = direction;
    record->timer = timer;
}

/* Pauses the animation of every player and record sprite. */
static void mg4_sprites_pause(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
        bits16_set(&mg4_state.records[i].sprite.status, ANIM_STATUS_PAUSED);
        bits16_set(&mg4_state.players[i].sprite.status, ANIM_STATUS_PAUSED);
    }
}
/* Resumes the animation of every player and record sprite. */
static void mg4_sprites_resume(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
        bits16_clear(&mg4_state.records[i].sprite.status, ANIM_STATUS_PAUSED);
        bits16_clear(&mg4_state.players[i].sprite.status, ANIM_STATUS_PAUSED);
    }
}

/* Draws animation 5 of backdropSprite at the origin; it is drawn while paused. */
static void mg4_draw_staged_sprite(void) {
    anim_select_range(&mg4_state.backdropSprite, 5);
    anim_draw_sprt_cursor(&current_frame->otTags[0][1], &mg4_state.backdropSprite, mg4_state.backdropSprite.animation,
                  mg4_state.backdropSprite.frame, 0, 0);
}

/* Reads the stage image (A_FILE member 0) and its texture pack, keeps a copy of the image
 * and binds it as the stage sprite. First load step. */
static void mg4_stage_data_load(void) {
    u8 *ready;

    DrawSync(0);
    ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
    afile_read_bytes(0, 0x25000, 0x5800, STAGING_BUFFER, 1, 1);
    do {
        ready = afile_read_bytes(0, 0x25000, 0x5800, STAGING_BUFFER, 1, 0);
    } while (ready == 0);
    texture_pack_load(STAGING_BUFFER + STAGE_DATA_SIZE);
    memcpy(mg4_stage_data, STAGING_BUFFER, STAGE_DATA_SIZE);
    memcpy(farm_area.stageData, STAGING_BUFFER, STAGE_DATA_SIZE);
    anim_bind_file(&mg4_state.stageSprite, (AnimFile *)mg4_stage_data);
}

/* Read the minigame's texture pack into the staging buffer and unpack it: the second step
 * of the load. */
static void mg4_staging_load(void) {
    DrawSync(0);
    ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
    afile_read_bytes(0, 0, 0x2000, STAGING_BUFFER, 1, 1);
    do {
    } while (afile_read_bytes(0, 0, 0x2000, STAGING_BUFFER, 1, 0) == 0);
    texture_pack_load(STAGING_BUFFER);
}

/* Reads the sprite sheet (A_FILE member 0x18) and its texture pack, copies the sheet in, and
 * binds the backdrop, player and record sprites to it. */
static void mg4_load_sprites(void) {
    u8 *ready;
    s32 i;

    DrawSync(0);
    ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
    afile_read_bytes(0x18, 0, 0x1EC1C, &region_base[0x4290], 1, 1);
    do {
        ready = afile_read_bytes(0x18, 0, 0x1EC1C, &region_base[0x4290], 1, 0);
    } while (ready == 0);
    texture_pack_load(&region_base[0x6C38]);
    memcpy(mg4_sprite_sheet, &region_base[0x4290], sizeof(mg4_sprite_sheet));
    anim_bind_file(&mg4_state.backdropSprite, (AnimFile *)(mg4_sprite_sheet + mg4_sprite_offsets[3]));
    for (i = 0; i < 6; i++) {
        anim_bind_file(&mg4_state.players[i].sprite, (AnimFile *)(mg4_sprite_sheet + mg4_sprite_offsets[0]));
        anim_bind_file(&mg4_state.records[i].sprite, (AnimFile *)(mg4_sprite_sheet + mg4_sprite_offsets[i == MG4_DOG ? 1 : 2]));
    }
}

/* Stop the two sound banks and start the minigame's music. */
static void mg4_audio_start(void) {
    sound_release_from(2, 0);
    sound_release_from(1, 0);
    sound_request(SOUND_REQUEST_SEQ_2, 0);
    sound_request(MINIGAME_SEQUENCE, 1);
}

/* Raise a villager's friendship by 20, up to 255. The ending rewards the villagers who
 * watched. */
static void mg4_villager_friendship_add(CharacterId villager) {
    if (game_state.villagers[villager].friendship < FRIENDSHIP_MAX - MG4_FRIENDSHIP_GAIN) {
        game_state.villagers[villager].friendship += MG4_FRIENDSHIP_GAIN;
    } else {
        game_state.villagers[villager].friendship = FRIENDSHIP_MAX;
    }
}

/* Raise a villager's affection by 500, capped at 65535. */
static void mg4_villager_affection_add(CharacterId villager) {
    if (game_state.villagers[villager].affection <= 65034) {
        game_state.villagers[villager].affection += 500;
    } else {
        game_state.villagers[villager].affection = 65535;
    }
}

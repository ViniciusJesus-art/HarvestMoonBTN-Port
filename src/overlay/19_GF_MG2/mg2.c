/* Overlay 19_GF_MG2: minigame 2. */

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
#include "mg2.h"
#include "model.h"
#include "pad.h"
#include "scene.h"
#include "screen_fade.h"
#include "sound.h"
#include "text.h"
#include "texture_pack.h"
#include "ui_icons.h"
#include "villager.h"

typedef struct {
    s16 mode;
    u8  pad2[2];
    Mg2Vector vector;
} Mg2Placement;

/* Stage payload: A_FILE member MG2_PAYLOAD_FILE, read whole; its first MG2_PAYLOAD_SIZE bytes are kept. */
#define MG2_PAYLOAD_FILE 0x14
#define MG2_PAYLOAD_READ 0x1AE04
/* The minigame's music, the size of its stage image, and the frames of its fades. */
#define MINIGAME_SEQUENCE 15
#define STAGE_DATA_SIZE 0x13DC
#define MINIGAME_FADE_FRAMES 10
/* First stage section in the farm data file's section table. */
#define MG2_STAGE_SECTION 896
#define MG2_PAYLOAD_SIZE 0x104C
/* The pick table holds MG2_PICK_COUNT distinct values drawn from 0..MG2_PICK_POOL-1. */
#define MG2_PICK_COUNT 8
#define MG2_PICK_POOL 20
/* Frames the intro message stays up, and the wait before the next message. */
#define MG2_INTRO_FRAMES 60
#define MG2_WAIT_FRAMES 45
/* A falling player keeps stepping while its height is below this. */
#define MG2_FALL_Y -10
/* How far a facing step looks ahead for the ground. */
#define MG2_STEP_LENGTH 200
/* Object shot roll, out of MG2_ROLL_MAX. Player 1 is hit on a roll above its low threshold when its skill is at or below MG2_SKILL_SPLIT, else on a roll below its high threshold. */
#define MG2_ROLL_MAX 128
/* Angles are 4096 to a turn; a facing is a quarter turn. */
#define MG2_FULL_TURN 0x1000
/* The vector's y on the ground. */
#define MG2_GROUND_Y -100
/* The skill at or below which player 1's shot uses the low threshold (mg2_object_roll). */
#define MG2_SKILL_SPLIT 80
/* The handlers of Mg2State.index, in mg2_state_handlers' order. */
enum Mg2HandlerIndex {
    MG2_HANDLER_WAIT_PRESS = 0,
    MG2_HANDLER_WAIT_LOOKUP = 1,
    MG2_HANDLER_RUN_SUB = 2,
    MG2_HANDLER_INTRO = 3,
    MG2_HANDLER_WAIT_TIMER = 4,
    MG2_HANDLER_PLAY = 5,
    MG2_HANDLER_RESULT = 6,
    MG2_HANDLER_DONE = 7,
};

/* The steps of Mg2State.sub, in mg2_sub_handlers' order; MG2_SUB_DONE ends the run. */
enum Mg2SubStep {
    MG2_SUB_SETUP = 0,
    MG2_SUB_WAIT_ANIM = 1,
    MG2_SUB_START_ROUND = 2,
    MG2_SUB_SET_RESULTS = 3,
    MG2_SUB_STAGE_FRAME = 4,
    MG2_SUB_RESTART = 5,
    MG2_SUB_WAIT_BOARD = 6,
    MG2_SUB_DONE = 7,
};
/* Menu text ids (menu_request_open) by the step that shows them; the intro ids follow mg2_menu_ids. */
#define MG2_MENU_LOOKUP1_PROMPT 0xF
#define MG2_MENU_LOOKUP2_PROMPT 0x10
#define MG2_MENU_INTRO_LOOKUP0 0x11
#define MG2_MENU_INTRO_LOOKUP1 0x13
#define MG2_MENU_INTRO_LOOKUP2 0x14
#define MG2_MENU_WAIT_TIMER 0x12
#define MG2_MENU_HIT 0x15
#define MG2_MENU_RESULT_NONE 0x16
#define MG2_MENU_RESULT_MESSAGE 0x17
#define MG2_MENU_WAIT_ANIM 9
#define MG2_MENU_ROUND_START 0xA
#define MG2_MENU_SET_RESULTS 0xB
#define MG2_MENU_STAGE 0xC
#define MG2_MENU_RESTART 0xD
#define MG2_MENU_BOARD 0xE

static void mg2_load_phase(void);
static void mg2_step_intro_wait(void);
static void mg2_state_run(void);
static void mg2_end_phase(void);
static void mg2_leave_minigame(void);
static void mg2_reset_state(void);
static void mg2_reset_round(void);
static void mg2_init_players(void);
static void mg2_init_objects(void);
static s32 mg2_roll_skill(void);
static void mg2_state_wait_press(void);
static void mg2_state_wait_lookup(void);
static void mg2_run_sub(void);
static void mg2_state_intro_message(void);
static void mg2_state_wait_timer(void);
static void mg2_play_frame(void);
static void mg2_state_result(void);
static void mg2_mark_done(void);
static void mg2_sub_setup(void);
static void mg2_sub_wait_anim(void);
static void mg2_sub_start_round(void);
static void mg2_sub_set_results(void);
static void mg2_sub_stage_frame(void);
static void mg2_sub_restart(void);
static void mg2_sub_wait_board(void);
static void mg2_draw_pause(void);
static void mg2_object_roll(void);
static void mg2_resolve_attacks(void);
static void mg2_dispatch_records(void);
static void mg2_actor_think(Mg2Player *actor);
static void mg2_actor_step_none(Mg2Player *unused0);
static void mg2_actor_step_idle(Mg2Player *obj);
static void mg2_attack_hit_tick(Mg2Player *obj);
static void mg2_struck_moving_tick(Mg2Player *obj);
static void mg2_struck_idle_tick(Mg2Player *obj);
static void mg2_attack_tick(Mg2Player *obj);
static void mg2_retreat_tick(Mg2Player *obj);
static void mg2_fall_tick(Mg2Player *obj);
static s32 mg2_check_fallen(void);
static int mg2_pick_winner(void);
static s16 mg2_roll_move(Mg2Player *actor);
static s32 mg2_in_attack_arc(Mg2Player *first, Mg2Player *second);
static s32 mg2_other_in_front(Mg2Player *self, Mg2Player *other);
static s32 mg2_distance(Mg2Player *a, Mg2Player *b);
static int mg2_relative_side(Mg2Player *self, Mg2Player *other);
static s32 mg2_bearing(Mg2Player *a, Mg2Player *b);
static s32 mg2_step_has_ground(Mg2Player *object);
static u32 mg2_is_idle(Mg2Player *player);
static int mg2_is_ready(Mg2Player *p);
static void mg2_draw_all(void);
static void mg2_draw_player(Mg2Player *self);
static void mg2_draw_object(Mg2Object *actor);
static void mg2_object_set(Mg2Object *object, s16 state, s16 timer);
static void mg2_pause_sprites(void);
static void mg2_resume_sprites(void);
static void mg2_sprites_apply(s32 mode);
static void mg2_draw_pause_icon(void);
static s32 mg2_message_offset(s32 index);
static void mg2_sprite_submit(u_long *ot, AnimState *sprite, Mg2Vector *vector, s16 bias);
static void mg2_draw_scenery_piece(u_long *ot, AnimState *sprite, Mg2Vector *vector, s16 mode);
static void mg2_draw_results_board(void);
static void mg2_snap_to_ground(Mg2Vector *p);
static void mg2_load_payload(void);
static void mg2_scroll_camera_reset(void);
static void mg2_scroll_camera_rotate(void);
static void mg2_scroll_camera_apply(void);
static void mg2_scroll_camera_draw(void);
static void mg2_stage_data_load(void);
static void mg2_staging_load(void);
static void mg2_stage_geometry_load(void);
static void mg2_audio_start(void);
static void mg2_villager_friendship_add(CharacterId villager);

#include "overlay/common/farm_section_table.h"
/* The minigame's messages, read at the offsets mg2_message_offset returns
 * (mg2_state_result, mg2_draw_results_board): halfword text codes, 0xFFFF-terminated. */
static u8 mg2_message_text[0x12C] = {
    0x0B, 0x00, 0x20, 0x00, 0x0D, 0x00, 0x24, 0x00, 0x33, 0x00, 0x33, 0x00, 0x24, 0x00, 0xFF, 0xFF,
    0x0C, 0x00, 0x20, 0x00, 0x23, 0x00, 0x03, 0x00, 0x2E, 0x00, 0x26, 0x00, 0xFF, 0xFF, 0x18, 0x00,
    0x2E, 0x00, 0x2A, 0x00, 0x2E, 0x00, 0x39, 0x00, 0x34, 0x00, 0x2D, 0x00, 0x20, 0x00, 0xFF, 0xFF,
    0x01, 0x00, 0x24, 0x00, 0x31, 0x00, 0x33, 0x00, 0x27, 0x00, 0x38, 0x00, 0xFF, 0xFF, 0x02, 0x00,
    0x27, 0x00, 0x34, 0x00, 0x2D, 0x00, 0x2A, 0x00, 0x38, 0x00, 0xFF, 0xFF, 0x05, 0x00, 0x2E, 0x00,
    0x32, 0x00, 0x33, 0x00, 0x24, 0x00, 0x31, 0x00, 0xFF, 0xFF, 0x0F, 0x00, 0x31, 0x00, 0x28, 0x00,
    0x2D, 0x00, 0x22, 0x00, 0x24, 0x00, 0x32, 0x00, 0x32, 0x00, 0xFF, 0xFF, 0x12, 0x00, 0x20, 0x00,
    0x22, 0x00, 0x27, 0x00, 0x28, 0x00, 0x69, 0x00, 0x0A, 0x00, 0x0E, 0x00, 0xFF, 0xFF, 0x01, 0x00,
    0x28, 0x00, 0x26, 0x00, 0x0C, 0x00, 0x2E, 0x00, 0x2C, 0x00, 0x2C, 0x00, 0x20, 0x00, 0xFF, 0xFF,
    0x13, 0x00, 0x36, 0x00, 0x24, 0x00, 0x24, 0x00, 0x23, 0x00, 0x38, 0x00, 0xFF, 0xFF, 0x05, 0x00,
    0x2B, 0x00, 0x2E, 0x00, 0x36, 0x00, 0x2B, 0x00, 0x24, 0x00, 0x31, 0x00, 0xFF, 0xFF, 0x02, 0x00,
    0x2E, 0x00, 0x2D, 0x00, 0x20, 0x00, 0x2D, 0x00, 0xFF, 0xFF, 0x01, 0x00, 0x28, 0x00, 0x26, 0x00,
    0xFD, 0x0F, 0x00, 0x00, 0x2F, 0x00, 0x24, 0x00, 0xFF, 0xFF, 0x13, 0x00, 0x31, 0x00, 0x28, 0x00,
    0x2F, 0x00, 0x2B, 0x00, 0x24, 0x00, 0xFD, 0x0F, 0x02, 0x00, 0xFF, 0xFF, 0x13, 0x00, 0x27, 0x00,
    0x24, 0x00, 0xFD, 0x0F, 0x07, 0x00, 0x24, 0x00, 0x2D, 0x00, 0xFF, 0xFF, 0x12, 0x00, 0x2E, 0x00,
    0x2D, 0x00, 0x29, 0x00, 0x20, 0x00, 0xFF, 0xFF, 0x13, 0x00, 0x20, 0x00, 0x2D, 0x00, 0x2A, 0x00,
    0xFF, 0xFF, 0x12, 0x00, 0x34, 0x00, 0x2F, 0x00, 0x24, 0x00, 0x31, 0x00, 0xFD, 0x0F, 0x02, 0x00,
    0xFF, 0xFF, 0x10, 0x00, 0x34, 0x00, 0x24, 0x00, 0x24, 0x00, 0x2D, 0x00, 0xFF, 0xFF, 0x12, 0x00,
    0x36, 0x00, 0x24, 0x00, 0x24, 0x00, 0x33, 0x00, 0x38, 0x00, 0xFF, 0xFF,
};

/* The step handlers the phase runner dispatches through. */
static void (*mg2_step_handlers[5])(void) = {
    mg2_load_phase, mg2_step_intro_wait, mg2_state_run,
    mg2_end_phase, mg2_leave_minigame,
};

/* Indexed by Mg2State.index (mg2_state_run). */
static void (*mg2_state_handlers[8])(void) = {
    mg2_state_wait_press, mg2_state_wait_lookup, mg2_run_sub, mg2_state_intro_message,
    mg2_state_wait_timer, mg2_play_frame, mg2_state_result, mg2_mark_done,
};

/* Indexed by Mg2State.sub (mg2_run_sub). */
static void (*mg2_sub_handlers[7])(void) = {
    mg2_sub_setup, mg2_sub_wait_anim, mg2_sub_start_round, mg2_sub_set_results,
    mg2_sub_stage_frame, mg2_sub_restart, mg2_sub_wait_board,
};

/* The steps of mg2_load_phase, in order; MG2_LOAD_DONE leaves the phase. */
enum Mg2LoadStep {
    MG2_LOAD_STAGE_DATA = 0,
    MG2_LOAD_STAGING = 1,
    MG2_LOAD_PAYLOAD = 2,
    MG2_LOAD_GEOMETRY = 3,
    MG2_LOAD_AUDIO = 4,
    MG2_LOAD_FADER_WAIT = 5,
    MG2_LOAD_DONE = 6,
};

/* An object's animation bank (Mg2Object.state). ACTIVE and ACTIVE_ALT both play the ACTIVE bank. */
enum Mg2ObjectBank {
    MG2_OBJECT_BANK_IDLE = 0,
    MG2_OBJECT_BANK_WAIT_PRESS = 1,
    MG2_OBJECT_BANK_ACTIVE = 2,
    MG2_OBJECT_BANK_HIT = 3,
    MG2_OBJECT_BANK_ACTIVE_ALT = 4,
};

/* A player's state (Mg2Player.state). Each action runs for its timer, then the
 * player goes back to IDLE, or THINK picks the next move. */
enum Mg2PlayerState {
    MG2_STATE_NONE = 0,
    MG2_STATE_READY = 1,
    MG2_STATE_IDLE = 2,
    MG2_STATE_THINK = 3,
    MG2_STATE_ATTACK = 4,
    MG2_STATE_MISS = 5,
    MG2_STATE_ATTACK_HIT = 6,
    MG2_STATE_STRUCK_MOVING = 7,
    MG2_STATE_STRUCK_IDLE = 8,
    MG2_STATE_WIN = 9,
    MG2_STATE_LOSE = 10,
    MG2_STATE_FALL = 11,
};

/* Current step of the stage load phase. */
static u8 mg2_step = MG2_LOAD_STAGE_DATA;

/* A player's facing (Mg2Player.dir, 0..3); the move steps one unit along it. */
enum Mg2Direction {
    MG2_DIR_MINUS_Z = 0,
    MG2_DIR_MINUS_X = 1,
    MG2_DIR_PLUS_Z = 2,
    MG2_DIR_PLUS_X = 3,
};

static u16 mg2_clut_x[4] = {0x0020, 0x0010, 0x0020, 0x0030};
static u16 mg2_index_table[6] = {0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005};

/* The players' start positions (mg2_init_players). */
static Mg2Vector mg2_player_positions[2] = {{0, MG2_GROUND_Y, 220, 0}, {0, MG2_GROUND_Y, -220, 0}};

/* The objects' start positions (mg2_init_objects). */
static Mg2Vector mg2_object_positions[3] = {{0, 0, 1250, 0}, {0, 0, -1250, 0}, {-1250, 0, 0, 0}};

/* Index into Mg2State.picks per lookupIndex: the pick whose message a fall shows (mg2_state_result). */
static u8 mg2_message_slots[4] = {1, 3, 6, 0};

/* Move weights: [skill band][winning != 0][slot], the slot mapped to a move in mg2_roll_move. */
static s16 mg2_band_points[3][2][4] = {
    {{35, 20, 20, 25}, {30, 15, 15, 40}},
    {{40, 15, 15, 30}, {35, 10, 10, 45}},
    {{45, 10, 10, 35}, {40, 5, 5, 50}},
};

/* Turn weights: [unlucky][relative side][heading step] (mg2_actor_think). */
static u8 mg2_unlucky_points[2][4][4] = {
    {{90, 5, 0x00, 5}, {5, 90, 5, 0x00}, {0x00, 5, 90, 5}, {5, 0x00, 5, 90}},
    {{75, 10, 5, 10}, {10, 75, 10, 5}, {5, 10, 75, 10}, {10, 5, 10, 75}},
};

/* The nine scenery pieces mg2_draw_all draws. */
static Mg2Placement mg2_scenery_placements[9] = {
    {0, {0}, {1300, 0, -500, 0}},
    {1, {0}, {-373, 0, -1472, 0}},
    {2, {0}, {-1316, 0, -488, 0}},
    {3, {0}, {1300, 0, 150, 0}},
    {4, {0}, {1400, 0, -50, 0}},
    {5, {0}, {-1357, 0, 496, 0}},
    {6, {0}, {652, 0, -1472, 0}},
    {7, {0}, {631, 0, 1378, 0}},
    {8, {0}, {-742, 0, 1378, 0}},
};

/* Skill roll ranges: two years of three lookups each (mg2_roll_skill). */
static u16 mg2_roll_ranges[6] = {85, 170, 255, 127, 255, 84};
/* The intro message's menu id per Mg2State.lookupIndex (mg2_state_intro_message). */
static s16 mg2_menu_ids[4] = {MG2_MENU_INTRO_LOOKUP0, MG2_MENU_INTRO_LOOKUP1, MG2_MENU_INTRO_LOOKUP2, 0x0000};

/* Offsets of the messages in mg2_message_text (mg2_message_offset). */
static s32 mg2_index_values[20] = {
    0x00, 0x10, 0x1E, 0x30, 0x3E, 0x4C, 0x5A, 0x6C, 0x7E, 0x90,
    0x9E, 0xAE, 0xBA, 0xCA, 0xDC, 0xEC, 0xF8, 0x102, 0x112, 0x11E,
};

static FarmScreenXY mg2_project_xy = {0};
static FarmScreenXY mg2_project_xy2 = {0};

/* Offsets of the sprite banks in mg2_payload (mg2_load_payload). */
static u32 mg2_sprite_offsets[6] = {
    0x00000000, 0x00000310, 0x00000B4C, 0x00000C68, 0x00000DE0, 0x00000E88};

static s16 mg2_hit_slot;
static u32 mg2_round_frames;
static u32 mg2_frame;
static Bits16Fn mg2_bits_fn;
static SVECTOR mg2_project_vec;
static SVECTOR mg2_project_vec2;
static u8 mg2_scene_data[STAGE_DATA_SIZE];   /* the stage data mg2_stage_data_load reads in */
static u8 mg2_payload[MG2_PAYLOAD_SIZE];   /* the stage payload (mg2_load_payload) */
static Mg2State mg2_state;
static Model mg2_model;

#include "sound_request.h"
#include "farm_load.h"

/* The phase's entry: runs the step for its entry sub-step with text palette 3. */
void mg2_phase_run(void) {
    s32 saved;

    saved = text_get_palette();
    text_set_palette(3);
    mg2_step_handlers[game_state.progress.entrySubStep]();
    text_set_palette(saved);
}
/* One stage-load step per call; once the fader is ready the state is reset and the scene fades in. */
static void mg2_load_phase(void) {
    switch (mg2_step) {
    case MG2_LOAD_STAGE_DATA:
        mg2_stage_data_load();
        mg2_step = mg2_step + 1;
        break;
    case MG2_LOAD_STAGING:
        mg2_staging_load();
        mg2_step = mg2_step + 1;
        break;
    case MG2_LOAD_PAYLOAD:
        mg2_load_payload();
        mg2_step = mg2_step + 1;
        break;
    case MG2_LOAD_GEOMETRY:
        mg2_stage_geometry_load();
        mg2_step = mg2_step + 1;
        break;
    case MG2_LOAD_AUDIO:
        mg2_audio_start();
        mg2_step = mg2_step + 1;
        break;
    case MG2_LOAD_FADER_WAIT:
        if (sound_fader_ready() != 0) {
            mg2_step = MG2_LOAD_DONE;
            sound_update();
            sound_seq_start_reverb(0, 0, MINIGAME_SEQUENCE, 127);
        }
        break;
    }
    if (mg2_step == MG2_LOAD_DONE) {
        mg2_reset_state();
        mg2_scroll_camera_reset();
        menu_reset();
        fade_start_in(MINIGAME_FADE_FRAMES, 0, 2);
        sound_fade_in(MINIGAME_FADE_FRAMES);
        game_state.progress.entrySubStep++;
        mg2_step = MG2_LOAD_STAGE_DATA;
    }
}
/* Draw the stage until the fades settle, then hand over to the state machine. */
static void mg2_step_intro_wait(void) {
    s32 ready;
    mg2_scroll_camera_rotate();
    mg2_scroll_camera_apply();
    model_load_lighting();
    mg2_scroll_camera_draw();
    mg2_draw_results_board();
    ready = fade_is_settled() == 1;
    if (sound_fader_mode() == SOUND_FADER_IDLE) ready = (ready + 1) & 0xFF;
    if (ready == 2) game_state.progress.entrySubStep++;
}
/* Menu, stage camera and draw, then the handler of the current Mg2State.index. */
static void mg2_state_run(void) {
    menu_run_state();
    mg2_scroll_camera_rotate();
    mg2_scroll_camera_apply();
    model_load_lighting();
    mg2_scroll_camera_draw();
    mg2_state_handlers[mg2_state.index]();
}

/* Fade out the scene and its sounds, draw it, and move on to the exit. */
static void mg2_end_phase(void) {
    fade_start_blend(MINIGAME_FADE_FRAMES);
    sound_fade_out(MINIGAME_FADE_FRAMES);
    mg2_draw_all();
    game_state.progress.entrySubStep++;
}

/* Leave the minigame once the fade settles: release the sounds, exit to the farm, and grant the
 * friendships of a completed round. */
static void mg2_leave_minigame(void) {
    s32 done;
    s32 fade;
    mg2_draw_all();
    done = 1;
    fade = fade_is_settled() == 1;
    if (sound_fader_mode() == SOUND_FADER_IDLE) {
        fade = (fade + 1) & 0xFF;
    }
    if (fade != 2) {
        return;
    }
    if (mg2_state.soundVoice != SOUND_VOICE_NONE) {
        sound_release_voice(mg2_state.soundVoice);
    }
    sound_seq_stop_first(0);
    sound_release_from(2, 0);
    sound_release_from(1, 0);
    game_state.progress.entrySubStep = 0xFF;
    main_request_scene(SCENE_FARM_MINIGAME_EXIT);
    game_state.globalFlags |= GLOBAL_FLAG_MINIGAME_EXIT;
    if (game_state.minigames.mg2Done != done) {
        return;
    }
    mg2_villager_friendship_add(CHARACTER_JEFF);
    mg2_villager_friendship_add(CHARACTER_SAIBARA);
    mg2_villager_friendship_add(CHARACTER_GRAY);
    mg2_villager_friendship_add(CHARACTER_DOUG);
    mg2_villager_friendship_add(CHARACTER_DOCTOR);
    mg2_villager_friendship_add(CHARACTER_BARLEY);
    mg2_villager_friendship_add(CHARACTER_MAYOR_THOMAS);
    mg2_villager_friendship_add(CHARACTER_RICK);
    mg2_villager_friendship_add(CHARACTER_DUKE);
    mg2_villager_friendship_add(CHARACTER_KAI);
    mg2_villager_friendship_add(CHARACTER_HARRIS);
    mg2_villager_friendship_add(CHARACTER_LOUIS);
}

/* Reset the run: draw eight distinct random picks in 0..19 and set the tuning values. */
static void mg2_reset_state(void) {
    s32 i;
    s32 j;
    s32 value;

    mg2_state.index = MG2_HANDLER_WAIT_PRESS;
    mg2_state.sub = MG2_SUB_SETUP;
    mg2_state.lookupIndex = 0;
    mg2_state.frameCount = 0;
    for (i = 0; i < MG2_PICK_COUNT; i++) {
        /* 0xFF marks an empty slot, which the duplicate scan skips. */
        mg2_state.picks[i] = 0xFF;
        value = rand_range(MG2_PICK_POOL);
        for (j = 0; j <= i; j++) {
            if (mg2_state.picks[j] == value) {
                value++;
                if (value >= MG2_PICK_POOL) {
                    value = 0;
                }
                j = 0;
            }
        }
        mg2_state.picks[i] = value;
    }
    mg2_reset_round();
    mg2_state.attackAngle = 45;
    mg2_state.attackRange = 400;
    mg2_state.speedScale = 6;
    mg2_state.busyLow = 80;
    mg2_state.busyHigh = 10;
    mg2_state.sideLow = 10;
    mg2_state.sideHigh = 125;
    mg2_state.frontLow = 50;
    mg2_state.frontHigh = 115;
    mg2_state.arcLow = 60;
    mg2_state.attackInterval = 2;
    mg2_state.arcHigh = 115;
    mg2_state.moveDelay = 90;
    mg2_state.rollInterval = 15;
    mg2_state.missDuration = 60;
    mg2_state.hitDuration = 90;
    mg2_state.attackDuration = 120;
    mg2_state.retreatDuration = 90;
    mg2_state.thinkInterval = 30;
    mg2_state.missSpeed = 10;
    mg2_state.attackSpeed = 10;
    mg2_state.hitSpeedScale = 11;
    mg2_state.idleHitSpeedScale = 10;
    mg2_state.retreatSpeedScale = 11;
}

/* Clear the round: no pause or voices, nobody fallen, players and objects back at their starts. */
static void mg2_reset_round(void) {
    mg2_state.paused = 0;
    mg2_state.soundVoice = SOUND_VOICE_NONE;
    mg2_state.hitVoice = SOUND_VOICE_NONE;
    mg2_state.fallenPlayer = -1;
    mg2_init_players();
    mg2_init_objects();
}

static void mg2_init_players(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        mg2_state.players[i].index = i;
        mg2_state.players[i].state = MG2_STATE_IDLE;
        mg2_state.players[i].timer = 0;
        mg2_state.players[i].winning = 0;
        mg2_state.players[i].position = mg2_player_positions[i];
    }
    mg2_state.players[0].dir = MG2_DIR_MINUS_Z;
    mg2_state.players[1].dir = MG2_DIR_PLUS_Z;
    mg2_state.players[0].skill = game_state.minigames.chickenSkill;
    mg2_state.players[1].skill = mg2_roll_skill();
    mg2_state.players[1].clutX = mg2_clut_x[mg2_state.lookupIndex + 1];
}

/* The farm's chicken skill; mg2_init_objects takes its low bit. */
#define MG2_CHICKEN_SKILL ((s16)game_state.minigames.chickenSkill)
static void mg2_init_objects(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        mg2_state.objects[i].id = i;
        mg2_state.objects[i].state = MG2_OBJECT_BANK_IDLE;
        mg2_state.objects[i].timer = 0;
        mg2_state.objects[i].position = mg2_object_positions[i];
    }
    mg2_state.objects[1].clutRow =
        mg2_index_table[mg2_state.lookupIndex * 2 + (MG2_CHICKEN_SKILL - MG2_CHICKEN_SKILL / 2 * 2)];
}
#undef MG2_CHICKEN_SKILL

/* Roll the opponent's skill (player 1) from the range for the year and lookup; in later years the third lookup adds 171. */
static s32 mg2_roll_skill(void) {
    s16 year;
    s16 lookup;
    s16 roll;
    u16 *ranges;
    u16 range;

    year = game_state.progress.clock.date.year;
    lookup = mg2_state.lookupIndex;
    if (year >= 2) year = 1;
    if (lookup >= 3) year = 2;
    ranges = mg2_roll_ranges;
    range = ranges[year * 3 + lookup];
    roll = rand_range(range);
    if (year != 0 && lookup == 2) roll += 171;
    return roll;
}

static void mg2_state_wait_press(void) {
    mg2_draw_all();
    mg2_draw_results_board();
    if (input_pad1->pressed & (PAD_CIRCLE | PAD_CROSS)) {
        mg2_state.index = MG2_HANDLER_WAIT_LOOKUP;
    }
}

/* Wait for the menu choice of the current lookup, then move to the chosen state. */
static void mg2_state_wait_lookup(void) {
    s16 lookup;
    s32 phase;
    s32 choice;

    mg2_draw_all();
    lookup = mg2_state.lookupIndex;
    switch (lookup) {
    case 0:
        mg2_state.index = MG2_HANDLER_RUN_SUB;
        mg2_state.sub = MG2_SUB_SETUP;
        return;
    case 1:
        menu_request_open(4, MG2_MENU_LOOKUP1_PROMPT);
        break;
    case 2:
        menu_request_open(4, MG2_MENU_LOOKUP2_PROMPT);
        break;
    }
    phase = menu_get_phase();
    if (phase == TEXT_WINDOW_CLOSED) {
        choice = menu_choice();
        if (choice == 1) {
            mg2_state.waitTimer = 0;
            mg2_state.index = MG2_HANDLER_INTRO;
        } else if (choice == phase) {
            mg2_state.index = choice;
            mg2_state.sub = MG2_SUB_SETUP;
        }
    }
}

/* Run the current sub step and draw; when it reports MG2_SUB_DONE, go back to the intro. */
static void mg2_run_sub(void) {
    s16 sub = mg2_state.sub;
    if (sub == MG2_SUB_DONE) {
        mg2_state.sub = MG2_SUB_SETUP;
        mg2_state.index = MG2_HANDLER_INTRO;
        return;
    }
    mg2_sub_handlers[sub]();
    mg2_dispatch_records();
    mg2_draw_all();
}

/* Show the lookup's intro message; after its wait the objects are set and the wait timer starts. */
static void mg2_state_intro_message(void) {
    mg2_reset_round();
    mg2_draw_all();
    text_set_input(0);
    menu_set_sound_enabled(0);
    menu_request_open(4, mg2_menu_ids[mg2_state.lookupIndex]);
    if (menu_get_phase() == TEXT_WINDOW_END_WAIT) {
        mg2_state.waitTimer++;
        if (mg2_state.waitTimer >= MG2_INTRO_FRAMES) {
            mg2_object_set(&mg2_state.objects[0], MG2_OBJECT_BANK_WAIT_PRESS, 0);
            mg2_object_set(&mg2_state.objects[1], MG2_OBJECT_BANK_WAIT_PRESS, 0);
            mg2_object_set(&mg2_state.objects[2], MG2_OBJECT_BANK_ACTIVE_ALT, 0);
            menu_begin_close();
            mg2_state.waitTimer = 0;
            mg2_state.index = MG2_HANDLER_WAIT_TIMER;
        }
    }
}

/* Show the wait message; after MG2_WAIT_FRAMES the index takes the window's END_WAIT value, which is MG2_HANDLER_PLAY. */
static void mg2_state_wait_timer(void) {
    s32 state;
    mg2_draw_all();
    text_set_input(0);
    menu_set_sound_enabled(0);
    menu_request_open(4, MG2_MENU_WAIT_TIMER);
    state = menu_get_phase();
    if (state == TEXT_WINDOW_END_WAIT) {
        mg2_state.waitTimer++;
        if (mg2_state.waitTimer >= MG2_WAIT_FRAMES) {
            mg2_object_set(&mg2_state.objects[0], MG2_OBJECT_BANK_WAIT_PRESS, 0);
            mg2_object_set(&mg2_state.objects[1], MG2_OBJECT_BANK_WAIT_PRESS, 0);
            mg2_object_set(&mg2_state.objects[2], MG2_OBJECT_BANK_IDLE, 0);
            menu_begin_close();
            mg2_state.waitTimer = 0;
            mg2_state.index = state;
        }
    }
}

/* MG2 per-frame update: the pause toggle, the fall check (which starts the result), object 0's wait and the idle cue every 30 frames. */
static void mg2_play_frame(void) {
    s32 hit;

    if (input_pad1->pressed & PAD_START) {
        mg2_state.paused = mg2_state.paused != 1;
        if (mg2_state.paused == 1) {
            mg2_pause_sprites();
            mg2_state.soundVoice = sound_play(0x36);
        } else {
            mg2_resume_sprites();
        }
    }
    if (mg2_state.paused == 1) {
        mg2_draw_pause();
        return;
    }
    mg2_state.frameCount++;
    if (mg2_state.frameCount == 10000) {
        mg2_state.frameCount = 0;
    }
    hit = mg2_check_fallen();
    if (hit != -1) {
        if (mg2_hit_slot == -1) {
            mg2_hit_slot = hit;
            menu_request_open(4, MG2_MENU_HIT);
            mg2_state.hitVoice = sound_play(0x43);
            mg2_object_set(&mg2_state.objects[hit], MG2_OBJECT_BANK_HIT, 0);
            mg2_object_set(hit == 0 ? &mg2_state.objects[1] : &mg2_state.objects[0], MG2_OBJECT_BANK_IDLE, 0);
            mg2_object_set(&mg2_state.objects[2], MG2_OBJECT_BANK_ACTIVE_ALT, 0);
        }
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            sound_release_voice(mg2_state.hitVoice);
            mg2_state.index = MG2_HANDLER_RESULT;
            mg2_state.fallenPlayer = mg2_hit_slot;
            mg2_state.hitVoice = sound_play(0x43);
        } else {
            mg2_dispatch_records();
        }
    } else {
        mg2_hit_slot = hit;
        if (mg2_state.objects[0].state == MG2_OBJECT_BANK_WAIT_PRESS) {
            if (input_pad1->pressed & PAD_CROSS) {
                mg2_object_set(&mg2_state.objects[0], MG2_OBJECT_BANK_ACTIVE, 30);
            }
        } else if (--mg2_state.objects[0].timer <= 0) {
            mg2_object_set(&mg2_state.objects[0], MG2_OBJECT_BANK_WAIT_PRESS, 0);
        }
        mg2_object_roll();
        mg2_resolve_attacks();
        mg2_dispatch_records();
        if (mg2_is_idle(&mg2_state.players[0]) == 1) {
            s16 tick = mg2_state.frameCount;

            /* Every 30th tick. */
            if (tick == tick / 30 * 30) {
                mg2_state.soundVoice = sound_play(0x3A);
            }
        }
    }
    mg2_draw_all();
}

/* Show the round's result. Player 0 falling shows the farm's text and moves to the next lookup (the phase ends after the third); any other value shows the pick's message and ends the phase with mg2Done cleared. */
static void mg2_state_result(void) {
    u8 active;
    s32 result;
    s32 offset;
    u8 *message;

    mg2_draw_all();
    active = mg2_state.fallenPlayer;
    if (active == 0) {
        text_set_source(0, game_state.minigames.textSource);
        menu_request_open(4, MG2_MENU_RESULT_NONE);
    } else {
        offset = mg2_message_slots[mg2_state.lookupIndex];
        message = mg2_message_text + mg2_message_offset(mg2_state.picks[offset]);
        text_set_source(0, (u16 *)message);
        menu_request_open(4, MG2_MENU_RESULT_MESSAGE);
    }
    result = menu_get_phase();
    if (result == TEXT_WINDOW_CLOSED) {
        mg2_state.lookupIndex++;
        sound_release_voice(mg2_state.hitVoice);
        if (active == 0) {
            if (mg2_state.lookupIndex < 3) {
                mg2_reset_round();
                mg2_state.index = MG2_HANDLER_WAIT_PRESS;
            } else {
                mg2_state.index = MG2_HANDLER_DONE;
            }
        } else {
            game_state.minigames.mg2Done = 0;
            game_state.progress.entrySubStep++;
        }
    }
}
/* Mark the minigame as done and move on to the exit. */
static void mg2_mark_done(void) {
    mg2_draw_all();
    game_state.minigames.mg2Done = 1;
    game_state.progress.entrySubStep++;
}

/* Put both players at their starts and the objects idle, then wait for the menu. */
static void mg2_sub_setup(void) {
    Mg2Vector *p = &mg2_state.players[0].position;
    Mg2Vector *q = &mg2_state.players[1].position;
    mg2_reset_round();
    p->x = 0;
    p->y = MG2_GROUND_Y;
    p->z = 190;
    q->x = 0;
    q->y = MG2_GROUND_Y;
    q->z = -190;
    mg2_object_set(&mg2_state.objects[0], MG2_OBJECT_BANK_IDLE, 0);
    mg2_object_set(&mg2_state.objects[1], MG2_OBJECT_BANK_IDLE, 0);
    mg2_player_set_action(&mg2_state.players[0], MG2_STATE_READY, MG2_DIR_MINUS_X, 0);
    mg2_player_set_action(&mg2_state.players[1], MG2_STATE_READY, MG2_DIR_PLUS_X, 0);
    mg2_state.sub = MG2_SUB_WAIT_ANIM;
}

/* Wait for the start menu to close, then set the players going. */
static void mg2_sub_wait_anim(void) {
    s32 state;

    menu_request_open(4, MG2_MENU_WAIT_ANIM);
    state = menu_get_phase();
    if (state == TEXT_WINDOW_CLOSED) {
        mg2_object_set(&mg2_state.objects[0], MG2_OBJECT_BANK_WAIT_PRESS, 0);
        mg2_object_set(&mg2_state.objects[1], MG2_OBJECT_BANK_WAIT_PRESS, 0);
        mg2_state.soundVoice = sound_play(0x3A);
        mg2_player_set_action(&mg2_state.players[0], MG2_STATE_IDLE, MG2_DIR_MINUS_Z, 0);
        mg2_player_set_action(&mg2_state.players[1], MG2_STATE_READY, MG2_DIR_PLUS_X, 0);
        mg2_state.sub = state;
    }
}

/* Count the round's frames (a cue every 30) under the round-start message; when it closes, start the first attack and move on to the results. */
static void mg2_sub_start_round(void) {
    s32 count;
    mg2_round_frames++;
    count = mg2_round_frames;
    if (count == (count / 30) * 30) mg2_state.soundVoice = sound_play(0x3A);
    menu_request_open(4, MG2_MENU_ROUND_START);
    if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
        mg2_object_set(&mg2_state.objects[0], MG2_OBJECT_BANK_ACTIVE, 30);
        mg2_object_set(&mg2_state.objects[1], MG2_OBJECT_BANK_WAIT_PRESS, 0);
        mg2_player_set_action(&mg2_state.players[0], MG2_STATE_ATTACK, MG2_DIR_MINUS_Z, 30);
        mg2_player_set_action(&mg2_state.players[1], MG2_STATE_STRUCK_MOVING, MG2_DIR_MINUS_Z, 90);
        mg2_state.soundVoice = sound_play(0x3B);
        mg2_state.soundVoice = sound_play(0x3C);
        mg2_state.soundVoice = sound_play(0x44);
        mg2_round_frames = 0;
        mg2_state.subStep = 0;
        mg2_state.sub = MG2_SUB_SET_RESULTS;
    }
}

/* Show the attack's result panel; the attacker's swing that lands switches to ATTACK_HIT. */
static void mg2_sub_set_results(void) {
    s32 state;
    menu_request_open(4, MG2_MENU_SET_RESULTS);
    if ((mg2_state.players[0].state == MG2_STATE_ATTACK) && (mg2_state.players[0].timer < 2)) {
        mg2_player_set_action(&mg2_state.players[0], MG2_STATE_ATTACK_HIT, MG2_DIR_MINUS_Z, 60);
    }
    state = menu_get_phase();
    if (state == TEXT_WINDOW_CLOSED) {
        mg2_object_set(&mg2_state.objects[0], MG2_OBJECT_BANK_WAIT_PRESS, 0);
        mg2_object_set(&mg2_state.objects[1], MG2_OBJECT_BANK_WAIT_PRESS, 0);
        mg2_player_set_action(&mg2_state.players[0], MG2_STATE_THINK, MG2_DIR_PLUS_X, 30);
        mg2_player_set_action(&mg2_state.players[1], MG2_STATE_READY, MG2_DIR_PLUS_X, 30);
        mg2_state.subStep = 0;
        mg2_state.sub = MG2_SUB_STAGE_FRAME;
    }
}

/* One stage frame: a cue every 30 frames and the stage menu once. Until player 0 has ground ahead it keeps thinking; then, at the menu's end wait, it attacks and the sub step moves on. */
static void mg2_sub_stage_frame(void) {
    s32 frame;
    s32 result;

    frame = ++mg2_frame;
    if (frame == (frame / 30) * 30) {
        mg2_state.soundVoice = sound_play(0x3A);
    }
    if (mg2_state.subStep == 0) {
        text_set_input(0);
        menu_set_sound_enabled(0);
        menu_request_open(4, MG2_MENU_STAGE);
        mg2_state.subStep++;
    }
    if (mg2_step_has_ground(&mg2_state.players[0]) == 1) {
        mg2_player_set_action(&mg2_state.players[0], MG2_STATE_IDLE, MG2_DIR_PLUS_X, 30);
        result = menu_get_phase();
        if (result == TEXT_WINDOW_END_WAIT) {
            mg2_object_set(&mg2_state.objects[0], MG2_OBJECT_BANK_ACTIVE, 30);
            mg2_player_set_action(&mg2_state.players[0], MG2_STATE_ATTACK, MG2_DIR_PLUS_X, 30);
            mg2_state.soundVoice = sound_play(0x3B);
            mg2_state.soundVoice = sound_play(0x44);
            menu_begin_close();
            mg2_frame = 0;
            mg2_state.subStep = 0;
            mg2_state.sub = result;
        }
    } else {
        mg2_player_set_action(&mg2_state.players[0], MG2_STATE_THINK, MG2_DIR_PLUS_X, 6);
    }
}

/* Show the restart message. A fall ends the round: it resets, the objects go idle and the sub step waits for the results board. */
static void mg2_sub_restart(void) {
    text_set_input(0);
    menu_set_sound_enabled(0);
    menu_request_open(4, MG2_MENU_RESTART);
    if ((mg2_state.players[0].state == MG2_STATE_ATTACK) && (mg2_state.players[0].timer < 2)) {
        mg2_player_set_action(&mg2_state.players[0], MG2_STATE_MISS, MG2_DIR_PLUS_X, 60);
    }
    if (mg2_check_fallen() == 1) {
        if (menu_get_phase() == TEXT_WINDOW_END_WAIT) {
            text_set_input(1);
            menu_set_sound_enabled(1);
        }
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            mg2_reset_round();
            mg2_object_set(&mg2_state.objects[0], MG2_OBJECT_BANK_IDLE, 0);
            mg2_object_set(&mg2_state.objects[1], MG2_OBJECT_BANK_IDLE, 0);
            mg2_player_set_action(&mg2_state.players[0], MG2_STATE_READY, MG2_DIR_MINUS_X, 0);
            mg2_player_set_action(&mg2_state.players[1], MG2_STATE_READY, MG2_DIR_PLUS_X, 0);
            menu_begin_close();
            mg2_state.subStep = 0;
            mg2_state.sub = MG2_SUB_WAIT_BOARD;
        }
    }
}

/* Show the results board until it closes, then go back to the intro (choice 1) or to the state the choice names. */
static void mg2_sub_wait_board(void) {
    s32 phase;
    s32 choice;
    menu_request_open(4, MG2_MENU_BOARD);
    phase = menu_get_phase();
    if (phase == TEXT_WINDOW_CLOSED) {
        choice = menu_choice();
        mg2_state.sub = MG2_SUB_SETUP;
        if (choice == 1) {
            mg2_state.waitTimer = 0;
            mg2_state.index = MG2_HANDLER_INTRO;
        } else if (choice == phase) {
            mg2_state.index = choice;
        }
    }
}
/* Draw the pause icon over the frame. */
static void mg2_draw_pause(void) {
    mg2_draw_pause_icon();
    mg2_draw_all();
}

/* Object 1's shot: after a hit its ACTIVE bank lasts 30 frames, then it returns to WAIT_PRESS. Otherwise it rolls every rollInterval frames against player 1; the odds depend on player 1's state and where player 0 stands. */
static void mg2_object_roll(void) {
    Mg2Object *object = &mg2_state.objects[1];
    Mg2Player *player1 = &mg2_state.players[1];
    Mg2Player *player0 = &mg2_state.players[0];
    s32 hit;
    s32 roll;
    u32 idle;
    s16 high;
    s16 low;

    if (object->timer > 0) {
        object->timer--;
        if (object->timer <= 0) {
            object->timer = 0;
            mg2_object_set(object, MG2_OBJECT_BANK_WAIT_PRESS, 0);
        }
        return;
    }
    if (mg2_state.frameCount != mg2_state.frameCount / mg2_state.rollInterval * mg2_state.rollInterval) {
        return;
    }
    roll = rand_range(MG2_ROLL_MAX);
    hit = 0;
    idle = mg2_is_idle(player1);
    if (idle == 1) {
        if (mg2_in_attack_arc(player1, player0) == 1) {
            high = mg2_state.arcHigh;
            low = mg2_state.arcLow;
        } else if (mg2_other_in_front(player1, player0) == 1) {
            high = mg2_state.frontHigh;
            low = mg2_state.frontLow;
        } else {
            high = mg2_state.sideHigh;
            low = mg2_state.sideLow;
        }
    } else {
        high = mg2_state.busyHigh;
        low = mg2_state.busyLow;
    }
    if (player1->skill <= MG2_SKILL_SPLIT) {
        if (low < roll) {
            hit = 1;
        }
    } else if (roll < high) {
        hit = 1;
    }
    if (hit) {
        mg2_object_set(object, MG2_OBJECT_BANK_ACTIVE, 30);
    }
}

/* Resolve both MG2 players' attacks for this frame, then let ready players pick a move. */
static void mg2_resolve_attacks(void) {
    s32 i;
    Mg2Player *self;
    Mg2Player *other;
    Mg2Player *winner;
    Mg2Player *loser;
    s16 dir;
    s16 move;
    s32 hitMove;
    s32 first;
    s32 chance;

    for (i = 0; i < 2; i++) {
        self = &mg2_state.players[i];
        dir = MG2_DIR_MINUS_Z;
        other = (i == 0) ? &mg2_state.players[1] : &mg2_state.players[0];
        if (mg2_is_idle(self) == 1 && mg2_state.objects[i].state == MG2_OBJECT_BANK_ACTIVE) {
            mg2_player_set_action(self, MG2_STATE_ATTACK, self->dir, 30);
            mg2_state.soundVoice = sound_play(0x3B);
            mg2_state.soundVoice = sound_play(0x44);
            continue;
        }
        if (self->state == MG2_STATE_ATTACK) {
            if (self->timer != self->timer / mg2_state.attackInterval * mg2_state.attackInterval) {
                continue;
            }
            if (self->timer > 0) {
                if (other->state == MG2_STATE_ATTACK && mg2_in_attack_arc(other, self) == 1 &&
                    mg2_in_attack_arc(self, other) == 1) {
                    first = mg2_pick_winner();
                    winner = &mg2_state.players[first];
                    loser = (first == 0) ? &mg2_state.players[1] : &mg2_state.players[0];
                    dir = winner->dir + mg2_relative_side(winner, loser);
                    mg2_player_set_action(winner, MG2_STATE_WIN, dir % 4, mg2_state.attackDuration);
                    winner->winning = 1;
                    mg2_player_set_action(loser, MG2_STATE_LOSE, loser->dir, mg2_state.retreatDuration);
                    loser->winning = 0;
                    continue;
                }
            } else {
                if (mg2_in_attack_arc(self, other) == 1) {
                    dir = self->dir + mg2_relative_side(self, other);
                    mg2_player_set_action(self, MG2_STATE_ATTACK_HIT, dir % 4, mg2_state.hitDuration);
                    self->winning = 1;
                    hitMove = MG2_STATE_STRUCK_MOVING;
                    if (mg2_is_idle(other) == 1) {
                        hitMove = MG2_STATE_STRUCK_IDLE;
                    }
                    mg2_player_set_action(other, hitMove, other->dir, mg2_state.hitDuration);
                    mg2_state.soundVoice = sound_play(0x3C);
                } else {
                    mg2_player_set_action(self, MG2_STATE_MISS, self->dir, mg2_state.missDuration);
                    self->winning = 0;
                }
                continue;
            }
        }
        if (mg2_is_ready(self) == 1) {
            move = mg2_roll_move(self);
            switch (move) {
            case MG2_STATE_NONE:
            case MG2_STATE_READY:
                dir = rand_range(4);
                break;
            case MG2_STATE_THINK:
                dir = mg2_state.players[i].dir;
                break;
            case MG2_STATE_IDLE:
                chance = rand_range(101);
                if (self->skill * 100 / 255 >= chance) {
                    dir = mg2_state.players[i].dir + mg2_relative_side(self, other);
                    dir = dir % 4;
                } else {
                    dir = self->dir;
                }
                break;
            }
            mg2_player_set_action(self, move, dir, mg2_state.moveDelay);
        }
    }
}

/* Run each player's state tick, then count its timer down. */
static void mg2_dispatch_records(void) {
    s32 slot;
    s32 kind;
    s16 remaining_signed;
    u16 remaining_unsigned;
    Mg2Player *record;
    slot = 0;
    do {
        record = &mg2_state.players[slot];
        kind = record->state;
        switch (kind) {
        case MG2_STATE_NONE:
        case MG2_STATE_READY:
        case MG2_STATE_IDLE:
            break;
        case MG2_STATE_THINK:
            mg2_actor_think(record);
            break;
        case MG2_STATE_ATTACK:
            mg2_actor_step_none(record);
            break;
        case MG2_STATE_MISS:
            mg2_actor_step_idle(record);
            break;
        case MG2_STATE_ATTACK_HIT:
            mg2_attack_hit_tick(record);
            break;
        case MG2_STATE_STRUCK_MOVING:
            mg2_struck_moving_tick(record);
            break;
        case MG2_STATE_STRUCK_IDLE:
            mg2_struck_idle_tick(record);
            break;
        case MG2_STATE_WIN:
            mg2_attack_tick(record);
            break;
        case MG2_STATE_LOSE:
            mg2_retreat_tick(record);
            break;
        case MG2_STATE_FALL:
            mg2_fall_tick(record);
            break;
        }
        remaining_signed = record->timer;
        remaining_unsigned = record->timer;
        if (remaining_signed > 0) {
            record->timer = remaining_unsigned - 1;
        }
        slot++;
    } while (slot < 2);
}

/* One player's think. Every thinkInterval frames (and at moveDelay) it turns by a weighted roll. On even frames it attacks when the other is in front and close, otherwise it idles, or turns on the other's side when there is ground ahead; a THINK state then moves it. */
static void mg2_actor_think(Mg2Player *actor) {
    Mg2Player *other;
    s16 sum;
    s32 side;
    s32 unlucky;
    s16 threshold;
    s32 turn;
    s16 heading;
    s32 i;

    other = &mg2_state.players[0];
    if (actor == &mg2_state.players[0]) {
        other = &mg2_state.players[1];
    }
    if (actor->timer == actor->timer / mg2_state.thinkInterval * mg2_state.thinkInterval
        || actor->timer == mg2_state.moveDelay) {
        unlucky = rand_range(100) > actor->skill * 100 / 255;
        side = mg2_relative_side(actor, other);
        sum = 0;
        threshold = rand_range(100);
        for (i = 0; i < 4; i++) {
            sum += mg2_unlucky_points[unlucky][side][i];
            if (sum >= threshold) {
                heading = actor->dir + i;
                turn = heading % 4;
                mg2_player_set_action(actor, actor->state, turn, actor->timer);
                break;
            }
        }
    }
    if (actor->timer == actor->timer / 2 * 2) {
        if (mg2_relative_side(actor, other) == 0 && mg2_distance(actor, other) < 201) {
            if (actor->skill >= 240 && rand_range(128) >= 111U) {
                mg2_state.soundVoice = sound_play(0x3B);
                mg2_player_set_action(actor, MG2_STATE_ATTACK, actor->dir, 30);
            } else {
                mg2_player_set_action(actor, MG2_STATE_IDLE, actor->dir, actor->timer);
            }
        } else if (mg2_step_has_ground(actor) == 1) {
            if (rand_range(128) < actor->skill) {
                heading = actor->dir + mg2_relative_side(actor, other);
                turn = heading % 4;
                mg2_player_set_action(actor, MG2_STATE_IDLE, turn, actor->timer);
            } else if (rand_range(128) < 111U) {
                mg2_player_set_action(actor, MG2_STATE_IDLE, actor->dir, actor->timer);
            }
        }
        if (actor->state == MG2_STATE_THINK) {
            mg2_move_step(actor);
        }
    }
}

/* A swinging player stands still; its timer ends the swing. */
static void mg2_actor_step_none(Mg2Player *unused0) {
}
/* A missed swing: step along the facing, then go idle when the timer ends. */
static void mg2_actor_step_idle(Mg2Player *obj) {
    mg2_move_step(obj);
    if (obj->timer == 0) {
        mg2_player_set_action(obj, MG2_STATE_IDLE, obj->dir, 0);
    }
}

/* One player's per-frame tick, shared by the three hit and struck states. */
static inline void mg2_obj_tick(Mg2Player *obj) {
    if (mg2_state.hitDuration - 30 >= obj->timer)
        mg2_move_step(obj);
    if (obj->timer == 0)
        mg2_player_set_action(obj, MG2_STATE_IDLE, obj->dir, 0);
}

static void mg2_attack_hit_tick(Mg2Player *obj) {
    mg2_obj_tick(obj);
}
static void mg2_struck_moving_tick(Mg2Player *obj) {
    mg2_obj_tick(obj);
}
static void mg2_struck_idle_tick(Mg2Player *obj) {
    mg2_obj_tick(obj);
}

/* The winner's swing: steps while its timer is below attackDuration - 60, then goes idle. */
static void mg2_attack_tick(Mg2Player *obj) {
    if (obj->timer < mg2_state.attackDuration - 30 && obj->timer < mg2_state.attackDuration - 60) {
        mg2_move_step(obj);
    }
    if (obj->timer == 0) {
        mg2_player_set_action(obj, MG2_STATE_IDLE, obj->dir, 0);
    }
}

/* The loser's retreat: steps while its timer is below retreatDuration - 45, plays the strike sound at retreatDuration - 30, and goes idle when the timer ends. */
static void mg2_retreat_tick(Mg2Player *obj) {
    if (obj->timer < mg2_state.retreatDuration - 30 && obj->timer < mg2_state.retreatDuration - 45)
        mg2_move_step(obj);
    if (obj->timer == 0) mg2_player_set_action(obj, MG2_STATE_IDLE, obj->dir, 0);
    if (obj->timer == mg2_state.retreatDuration - 30) mg2_state.soundVoice = sound_play(0x3C);
}
/* A fallen player keeps stepping while it is below MG2_FALL_Y; otherwise it goes idle. */
static void mg2_fall_tick(Mg2Player *obj) {
    if (obj->position.y < MG2_FALL_Y) {
        mg2_move_step(obj);
        return;
    }
    mg2_player_set_action(obj, MG2_STATE_IDLE, obj->dir, obj->timer);
}

/* Move the object one step in its facing direction at its kind's speed. */
void mg2_move_step(Mg2Player *obj) {
    s32 speed = 0;
    s32 dx;
    s32 dz;

    switch (obj->state) {
    case MG2_STATE_MISS:
        speed = mg2_state.speedScale * mg2_state.missSpeed / 10;
        break;
    case MG2_STATE_STRUCK_MOVING:
        speed = mg2_state.speedScale * mg2_state.attackSpeed * mg2_state.hitSpeedScale / 100;
        break;
    case MG2_STATE_STRUCK_IDLE:
        speed = mg2_state.speedScale * mg2_state.attackSpeed * mg2_state.idleHitSpeedScale / 100;
        break;
    case MG2_STATE_ATTACK_HIT:
    case MG2_STATE_WIN:
        speed = mg2_state.speedScale * mg2_state.attackSpeed / 10;
        break;
    case MG2_STATE_LOSE:
        speed = mg2_state.speedScale * mg2_state.attackSpeed * mg2_state.retreatSpeedScale / 100;
        break;
    case MG2_STATE_THINK:
    case MG2_STATE_FALL:
        speed = mg2_state.speedScale;
        break;
    }
    dz = 0;
    dx = 0;
    switch (obj->dir) {
    case MG2_DIR_MINUS_Z:
        dz = -speed;
        break;
    case MG2_DIR_MINUS_X:
        dx = -speed;
        break;
    case MG2_DIR_PLUS_Z:
        dz = speed;
        break;
    case MG2_DIR_PLUS_X:
        dx = speed;
        break;
    }
    obj->position.x += dx;
    obj->position.z += dz;
}

/* Send the player at or above height -90 into FALL, player 1 first. Returns the other player's index, or -1 when neither is at that height. */
static s32 mg2_check_fallen(void) {
    if (mg2_state.players[1].position.y >= -90) {
        mg2_player_set_action(&mg2_state.players[1], MG2_STATE_FALL, mg2_state.players[1].dir, 2);
        return 0;
    }
    if (mg2_state.players[0].position.y < -90) return -1;
    mg2_player_set_action(&mg2_state.players[0], MG2_STATE_FALL, mg2_state.players[0].dir, 2);
    return 1;
}
/* Pick the winner of a clash: the higher skill wins, with a 9% upset; equal skills flip a coin. Returns 1 for player 1. */
static int mg2_pick_winner(void) {
    Mg2Player *players = mg2_state.players;
    s16 a;
    s16 b;
    unsigned int result;
    a = players[0].skill;
    b = players[1].skill;
    if (a != b) {
        result = b >= a;
        if (rand_range(100) >= 91U) result = result < 1;
    } else {
        result = rand_range(2) != 0;
    }
    return result;
}

/* Roll the actor's next move (a player state) from weights that depend on its skill band and on whether it is winning. */
static s16 mg2_roll_move(Mg2Player *actor) {
    s32 band;
    s32 result;
    s16 sum;
    s32 flag;
    s16 roll;
    s32 i;

    if (actor->skill < 81) {
        band = 0;
    } else if (actor->skill < 241) {
        band = 1;
    } else {
        band = 2;
    }
    result = 0;
    sum = 0;
    flag = actor->winning != 0;
    roll = rand_range(100);
    for (i = 0; i < 4; i++) {
        sum += mg2_band_points[band][flag][i];
        if (roll < sum) {
            switch (i) {
            case 0:
                result = 3;
                break;
            case 1:
                result = 1;
                break;
            case 2:
                result = 0;
                break;
            case 3:
                result = 2;
                break;
            }
            break;
        }
    }
    return result;
}

/* Whether second is in first's attack cone: within attackRange, and its bearing within attackAngle either side of first's facing. */
static s32 mg2_in_attack_arc(Mg2Player *first, Mg2Player *second) {
    s32 range;
    s32 difference;
    s16 angle;

    range = mg2_distance(first, second);
    if (mg2_state.attackRange < range) return 0;
    difference = mg2_bearing(first, second) - (first->dir << 10);
    if (difference < 0) difference += MG2_FULL_TURN;
    angle = mg2_state.attackAngle;
    if ((angle << 12) / 360 < difference) {
        if (difference < ((360 - angle) << 12) / 360) return 0;
    }
    return 1;
}
static s32 mg2_other_in_front(Mg2Player *self, Mg2Player *other) {
    return mg2_relative_side(self, other) == 0;
}

static s32 mg2_distance(Mg2Player *a, Mg2Player *b) {
    s32 dx = a->position.x - b->position.x;
    s32 dy = a->position.z - b->position.z;
    return SquareRoot0(dx * dx + dy * dy);
}
/* Where other lies relative to self's facing: 0 in front, 1 and 3 to either side, 2 behind. */
static int mg2_relative_side(Mg2Player *self, Mg2Player *other) {
    s32 diff;
    diff = mg2_bearing(self, other) - (self->dir << 10);
    if (diff < 0) diff += MG2_FULL_TURN;
    if ((diff - 513U) < 0x400) return 1;
    if ((diff - 1548U) < 1013) return 2;
    if ((diff - 2572U) < 1013) return 3;
    return 0;
}

/* The angle from b toward a, 4096 to a turn. */
static s32 mg2_bearing(Mg2Player *a, Mg2Player *b) {
    return ratan2(a->position.x - b->position.x, a->position.z - b->position.z);
}

/* The player's facing step, checked against the ground. */
static s32 mg2_step_has_ground(Mg2Player *object) {
    Mg2Vector vector;
    s16 dir;

    vector = object->position;
    dir = object->dir;
    switch (dir) {
    case MG2_DIR_MINUS_Z:
        vector.z -= MG2_STEP_LENGTH;
        break;
    case MG2_DIR_MINUS_X:
        vector.x -= MG2_STEP_LENGTH;
        break;
    case MG2_DIR_PLUS_Z:
        vector.z += MG2_STEP_LENGTH;
        break;
    case MG2_DIR_PLUS_X:
        vector.x += MG2_STEP_LENGTH;
        break;
    }
    mg2_snap_to_ground(&vector);
    return vector.y != object->position.y;
}

/* True while the player is IDLE or THINK. */
static u32 mg2_is_idle(Mg2Player *player) {
    return (u32)(u16)(player->state - 2) < 2;
}
/* True when the player's timer is 0 and its state comes before ATTACK. */
static int mg2_is_ready(Mg2Player *p) {
    int result = (u16)p->state < MG2_STATE_ATTACK;
    if (p->timer > 0) result = 0;
    return result;
}

/* Draw both players, the three objects and the nine scenery pieces. */
static void mg2_draw_all(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        mg2_draw_player(&mg2_state.players[i]);
    }
    for (i = 0; i < 3; i++) {
        mg2_draw_object(&mg2_state.objects[i]);
    }
    for (i = 0; i < 9; i++) {
        mg2_draw_scenery_piece(current_frame->ot, &mg2_state.scenerySprite, &mg2_scenery_placements[i].vector,
                      mg2_scenery_placements[i].mode);
    }
}

/* Draw one player: pick its frame from its state, and the shadow when it is struck or retreating. */
static void mg2_draw_player(Mg2Player *self) {
    Mg2Player *other;
    Mg2Vector position;
    s16 bias;
    s32 shadow;
    s16 frame;
    s32 alternate;

    other = &mg2_state.players[0];
    if (self == other) {
        other = self + 1;
    }
    bias = 0;
    shadow = 0;
    if (self->index == 0) {
        anim_set_offsets(&self->sprite, 0, 0, 0x20, 1);
    } else if (self->index == 1) {
        anim_set_offsets(&self->sprite, 0, 0, self->clutX, 0);
    }
    frame = 0;
    alternate = -1;
    switch (self->state) {
    case MG2_STATE_NONE:
        frame = self->dir + 5;
        break;
    case MG2_STATE_READY:
        frame = self->dir + 9;
        break;
    case MG2_STATE_IDLE:
        alternate = self->dir;
        frame = 0;
        break;
    case MG2_STATE_ATTACK:
        bias = -10;
        frame = self->dir + 17;
        break;
    case MG2_STATE_ATTACK_HIT:
        if (mg2_state.hitDuration - 30 < self->timer) {
            bias = -10;
            frame = self->dir + 17;
        } else {
            frame = self->dir + 1;
        }
        break;
    case MG2_STATE_STRUCK_MOVING:
    case MG2_STATE_STRUCK_IDLE:
        if (mg2_state.hitDuration - 30 >= self->timer) {
            frame = other->dir + 1;
            self->dir = other->dir;
        } else {
            frame = self->dir + 13;
        }
        shadow = 1;
        break;
    case MG2_STATE_WIN:
        if (self->timer >= mg2_state.attackDuration - 30) {
            bias = -10;
            frame = self->dir + 17;
        } else if (self->timer >= mg2_state.attackDuration - 60) {
            bias = -10;
            frame = self->dir + 17;
        } else {
            frame = self->dir + 1;
        }
        break;
    case MG2_STATE_LOSE:
        if (self->timer >= mg2_state.retreatDuration - 30) {
            bias = -10;
            frame = self->dir + 17;
        } else {
            if (self->timer >= mg2_state.retreatDuration - 45) {
                frame = other->dir + 13;
                self->dir = other->dir;
                shadow = 1;
            } else {
                frame = other->dir + 1;
                self->dir = other->dir;
                shadow = 1;
            }
        }
        break;
    case MG2_STATE_THINK:
    case MG2_STATE_MISS:
    case MG2_STATE_FALL:
        frame = self->dir + 1;
        break;
    }
    if (self->sprite.animation != frame) {
        anim_select_range(&self->sprite, frame);
    }
    if (alternate != -1) {
        anim_set_frame(&self->sprite, alternate);
    }
    mg2_sprite_submit(current_frame->ot, &self->sprite, &self->position, bias);
    if (shadow == 1) {
        if (mg2_state.shadow.animation != 0x24) {
            anim_select_range(&mg2_state.shadow, 0x24);
        }
        position = self->position;
        mg2_sprite_submit(&current_frame->otTags[0][2], &mg2_state.shadow, &position, -24);
    }
}

/* Pick the actor's animation bank from its state and draw it. */
static void mg2_draw_object(Mg2Object *actor) {
    s32 bank;
    s32 frame;

    if (actor->id == 1) {
        anim_set_offsets(&actor->sprite, 0, 0, 0, actor->clutRow);
    }
    bank = MG2_OBJECT_BANK_IDLE;
    frame = -1;
    switch (actor->state) {
    case MG2_OBJECT_BANK_IDLE:
        bank = MG2_OBJECT_BANK_IDLE;
        break;
    case MG2_OBJECT_BANK_WAIT_PRESS:
        bank = MG2_OBJECT_BANK_WAIT_PRESS;
        break;
    case MG2_OBJECT_BANK_HIT:
        bank = MG2_OBJECT_BANK_HIT;
        break;
    case MG2_OBJECT_BANK_ACTIVE:
    case MG2_OBJECT_BANK_ACTIVE_ALT:
        bank = MG2_OBJECT_BANK_ACTIVE;
        break;
    }
    if (actor->sprite.animation != bank) {
        anim_select_range(&actor->sprite, bank);
    }
    if (frame != -1) {
        anim_set_frame(&actor->sprite, frame);
    }
    mg2_sprite_submit(current_frame->ot, &actor->sprite, &actor->position, 0);
}

void mg2_player_set_action(Mg2Player *player, s16 state, s16 dir, s16 timer) {
    player->state = state;
    player->dir = dir;
    player->timer = timer;
}
static void mg2_object_set(Mg2Object *object, s16 state, s16 timer) {
    object->state = state;
    object->timer = timer;
}
static void mg2_pause_sprites(void) {
    mg2_sprites_apply(0);
}
static void mg2_resume_sprites(void) {
    mg2_sprites_apply(1);
}

/* Set the paused bit on the objects' and players' sprites (mode 0), or clear it. */
static void mg2_sprites_apply(s32 mode) {
    s32 i;

    mg2_bits_fn = (mode == 0) ? bits16_set : bits16_clear;
    for (i = 0; i < 3; i++) {
        mg2_bits_fn(&mg2_state.objects[i].sprite.status, ANIM_STATUS_PAUSED);
    }
    for (i = 0; i < 2; i++) {
        mg2_bits_fn(&mg2_state.players[i].sprite.status, ANIM_STATUS_PAUSED);
    }
}

static void mg2_draw_pause_icon(void) {
    anim_select_range(&mg2_state.iconSprite, 9);
    anim_draw_sprt_cursor(&current_frame->otTags[0][1], &mg2_state.iconSprite, mg2_state.iconSprite.animation,
                  mg2_state.iconSprite.frame, 0, 0);
}
/* The offset in mg2_message_text of a pick's message; the index is clamped to 0..19. */
static s32 mg2_message_offset(s32 index) {
    s16 value = index;
    s16 idx;
    if (value >= 0) {
        idx = (value < 20) ? value : 19;
    } else {
        idx = 0;
    }
    return mg2_index_values[idx];
}

/* Snap the vector to the ground, project it, and draw the sprite there, shifted by bias. */
static void mg2_sprite_submit(u_long *ot, AnimState *sprite, Mg2Vector *vector, s16 bias) {
    s32 slot;
    u_long *target;
    u_long *mainSlot;

    mg2_snap_to_ground(vector);
    mg2_project_vec.vx = vector->x;
    mg2_project_vec.vy = vector->y;
    mg2_project_vec.vz = vector->z;
    slot = gte_project_vertex(&mg2_project_xy, &mg2_project_vec);
    mg2_project_xy.y += bias;
    target = &ot[slot >> 3];
    mainSlot = &current_frame->ot[slot >> 3];
    if (sprite == &mg2_state.objects[0].sprite || sprite == &mg2_state.objects[1].sprite ||
        sprite == &mg2_state.objects[2].sprite) {
        anim_draw_ft4_advance(target, sprite, mg2_project_xy.x, mg2_project_xy.y);
        ui_draw_icon_18_sub(mainSlot, mg2_project_xy.x, mg2_project_xy.y, 0);
    } else if (sprite == &mg2_state.shadow) {
        anim_draw_ft4_advance(ot, sprite, mg2_project_xy.x, mg2_project_xy.y);
    } else {
        if (vector->y != MG2_GROUND_Y && vector->z - vector->x >= 0) {
            target = &ot[slot >> 2];
            mainSlot = &current_frame->ot[slot >> 2];
        }
        anim_draw_ft4_advance(target, sprite, mg2_project_xy.x, mg2_project_xy.y);
        ui_draw_icon_18_sub(mainSlot, mg2_project_xy.x, mg2_project_xy.y - bias, 0);
    }
}

/* Snap and project a scenery piece, set its frame from mode, and draw it. */
static void mg2_draw_scenery_piece(u_long *ot, AnimState *sprite, Mg2Vector *vector, s16 mode) {
    s32 slot;

    mg2_snap_to_ground(vector);
    mg2_project_vec2.vx = vector->x;
    mg2_project_vec2.vy = vector->y;
    mg2_project_vec2.vz = vector->z;
    slot = gte_project_vertex(&mg2_project_xy2, &mg2_project_vec2);
    anim_select_range(sprite, mode & 0xFF);
    anim_draw_ft4_advance(&ot[slot >> 3], sprite, mg2_project_xy2.x, mg2_project_xy2.y);
    ui_draw_icon_18_sub(&current_frame->ot[slot >> 3], mg2_project_xy2.x, mg2_project_xy2.y, 0);
}

/* The results board: a 2x4 grid of labels (the farm's text in the first cell, then the messages of picks 1..7), then the markers for the rounds reached. */
static void mg2_draw_results_board(void) {
    s32 saved;
    s32 row;
    s32 col;
    u16 *text;

    /* Draw frame `frame` of sprite bank `bank`. */
    void mg2_draw_board_marker(s32 bank, s32 frame) {
        anim_select_range(&mg2_state.iconSprite, bank & 0xFF);
        anim_set_frame(&mg2_state.iconSprite, frame);
        anim_draw_sprt_cursor(&current_frame->otTags[0][5], &mg2_state.iconSprite,
                      mg2_state.iconSprite.animation, mg2_state.iconSprite.frame, 0, 0);
    }

    anim_select_range(&mg2_state.iconSprite, 0);
    anim_draw_sprt_cursor(&current_frame->otTags[0][6], &mg2_state.iconSprite,
                  mg2_state.iconSprite.animation, mg2_state.iconSprite.frame, 0, 0);
    saved = text_get_palette();
    text_set_palette(1);
    for (row = 0; row < 2; row++) {
        for (col = 0; col < 4; col++) {
            if (row == 0 && col == 0) {
                text = game_state.minigames.textSource;
            } else {
                text = (u16 *)(mg2_message_text + mg2_message_offset(mg2_state.picks[row * 4 + col]));
            }
            text_draw_plain(&current_frame->otTags[0][3], text, row * 200 + 21,
                          col * 45 + 57 + (col > 0 ? 3 : 0));
        }
    }
    text_set_palette(saved);
    if (mg2_state.lookupIndex > 0) {
        mg2_draw_board_marker(1, 0);
        mg2_draw_board_marker(4, 0);
        mg2_draw_board_marker(5, 0);
        mg2_draw_board_marker(7, 0);
    }
    if (mg2_state.lookupIndex >= 2) {
        mg2_draw_board_marker(1, 1);
        mg2_draw_board_marker(7, 1);
    }
}

/* Put the stage camera's angles and distance at their starting values and its centre at the
 * origin. */
static void mg2_scroll_camera_reset(void) {
    farm_area.mode.scroll.z = -9000;
    farm_area.mode.scroll.x = -4500;
    farm_area.mode.scroll.unused8 = 0;
    farm_area.mode.scroll.yaw = -0x200;
    farm_area.mode.scroll.unused10 = 0;
    farm_area.mode.scroll.centre.vx = 0;
    farm_area.mode.scroll.centre.vy = 0;
    farm_area.mode.scroll.centre.vz = 0;
}

/* Rotate the stage camera's distance by its yaw and store the eye position in the scroll record. The GTE takes the angles as 16-bit values. */
static void mg2_scroll_camera_rotate(void) {
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
static void mg2_scroll_camera_apply(void) {
    camera_set_eye_roll(farm_area.mode.scroll.a18 + farm_area.mode.scroll.centre.vx, farm_area.mode.scroll.b1A,
                  farm_area.mode.scroll.c1C + farm_area.mode.scroll.centre.vz, 0);
    camera_set_target(farm_area.mode.scroll.centre.vx, farm_area.mode.scroll.centre.vy,
                  farm_area.mode.scroll.centre.vz);
}

/* Draw the stage's model as seen from the stage camera's eye toward its centre. */
static void mg2_scroll_camera_draw(void) {
    VECTOR eye;
    VECTOR target;

    eye.vx = farm_area.mode.scroll.a18 + farm_area.mode.scroll.centre.vx;
    eye.vy = farm_area.mode.scroll.b1A;
    eye.vz = farm_area.mode.scroll.c1C + farm_area.mode.scroll.centre.vz;
    target.vx = farm_area.mode.scroll.centre.vx;
    target.vy = farm_area.mode.scroll.centre.vy;
    target.vz = farm_area.mode.scroll.centre.vz;
    model_draw_visible(&mg2_model, &eye, &target);
}

/* Set the vector's height to the stage ground under it, if there is ground there. */
static void mg2_snap_to_ground(Mg2Vector *p) {
    VECTOR v;
    v.vx = p->x;
    v.vy = p->y;
    v.vz = p->z;
    v.pad = p->w;
    ground_set_skip_mask(0xC000);
    if (ground_find_object(&mg2_model, &v)) v.vy = ground_height_at();
    p->x = v.vx;
    p->y = v.vy;
    p->z = v.vz;
    p->w = v.pad;
}

/* Read the minigame's stage image (A_FILE member 0) and its texture pack, copy the image
 * into our own buffer and into the farm area's staging copy, and bind it as the shadow
 * sprite: the first step of the load. */
static void mg2_stage_data_load(void) {
    u8 *ready;

    DrawSync(0);
    ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
    afile_read_bytes(0, 0x25000, 0x5800, STAGING_BUFFER, 1, 1);
    do {
        ready = afile_read_bytes(0, 0x25000, 0x5800, STAGING_BUFFER, 1, 0);
    } while (ready == 0);
    texture_pack_load(STAGING_BUFFER + STAGE_DATA_SIZE);
    memcpy(mg2_scene_data, STAGING_BUFFER, STAGE_DATA_SIZE);
    memcpy(farm_area.stageData, STAGING_BUFFER, STAGE_DATA_SIZE);
    anim_bind_file(&mg2_state.shadow, (AnimFile *)mg2_scene_data);
}

/* Read the texture pack into the staging buffer and unpack it. */
static void mg2_staging_load(void) {
    DrawSync(0);
    ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
    afile_read_bytes(0, 0, 0x2000, STAGING_BUFFER, 1, 1);
    do {
    } while (afile_read_bytes(0, 0, 0x2000, STAGING_BUFFER, 1, 0) == 0);
    texture_pack_load(STAGING_BUFFER);
}

/* Load the MG2 stage payload, keep a copy in mg2_payload, and attach its pieces
 * to the state's sprite blocks. */
static void mg2_load_payload(void) {
    s32 i;

    DrawSync(0);
    ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
    afile_read_bytes(MG2_PAYLOAD_FILE, 0, MG2_PAYLOAD_READ, STAGING_BUFFER, 1, 1);
    do {
    } while (afile_read_bytes(MG2_PAYLOAD_FILE, 0, MG2_PAYLOAD_READ, STAGING_BUFFER, 1, 0) == 0);
    texture_pack_load(STAGING_BUFFER + MG2_PAYLOAD_SIZE);
    memcpy(mg2_payload, STAGING_BUFFER, MG2_PAYLOAD_SIZE);
    anim_bind_file(&mg2_state.iconSprite, (AnimFile *)(mg2_payload + mg2_sprite_offsets[0]));
    for (i = 0; i < 3; i++) {
        anim_bind_file(&mg2_state.objects[i].sprite, (AnimFile *)(mg2_payload + mg2_sprite_offsets[i + 2]));
    }
    for (i = 0; i < 2; i++) {
        anim_bind_file(&mg2_state.players[i].sprite, (AnimFile *)(mg2_payload + mg2_sprite_offsets[1]));
    }
    anim_bind_file(&mg2_state.scenerySprite, (AnimFile *)(mg2_payload + mg2_sprite_offsets[5]));
}

/* Read the stage's first two file sections (the scene tables, then a texture pack) into the
 * farm area's buffers, keep the table image as the mode script, bind the stage model to
 * the TMD in the section buffer and set it up for ground tests. */
static void mg2_stage_geometry_load(void) {
    u32 *sections = &section_table[MG2_STAGE_SECTION];

    afile_read_sectors(0, sections[0], sections[1] - sections[0], 0, farm_area.sectionBuffer);
    afile_read_sectors(0, sections[1], sections[2] - sections[1], 0, STAGING_BUFFER);
    texture_pack_load(STAGING_BUFFER);
    *(FarmModeScriptImage *)&farm_area.mode.script = *(FarmModeScriptImage *)farm_area.sectionBuffer;
    model_bind_tmd(&mg2_model, farm_area.sectionBuffer + 0x2000);
    ground_enable_objects(&mg2_model);
    ground_set_tolerance(200);
}

/* Stop the two sound banks and start the minigame's music. */
static void mg2_audio_start(void) {
    sound_release_from(2, 0);
    sound_release_from(1, 0);
    sound_request(SOUND_REQUEST_SEQ_2, 0);
    sound_request(MINIGAME_SEQUENCE, 1);
}

/* Raise a villager's friendship by 20, clamped to FRIENDSHIP_MAX. */
static void mg2_villager_friendship_add(CharacterId villager) {
    if (game_state.villagers[villager].friendship < FRIENDSHIP_MAX - 20) {
        game_state.villagers[villager].friendship += 20;
    } else {
        game_state.villagers[villager].friendship = FRIENDSHIP_MAX;
    }
}

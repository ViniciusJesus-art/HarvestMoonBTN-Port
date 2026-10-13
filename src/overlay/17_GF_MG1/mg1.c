/* Minigame 1: the race, its results table and its payload. */

#include <sys/types.h>
#include <memory.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "afile.h"
#include "anim.h"
#include "farm_state.h"
#include "farm_tables.h"
#include "game_types.h"
#include "horse_names.h"
#include "main.h"
#include "math_util.h"
#include "menu_window.h"
#include "mg1.h"
#include "pad.h"
#include "pad_vibration.h"
#include "screen_fade.h"
#include "sound.h"
#include "sound_request.h"
#include "text.h"
#include "texture_pack.h"
#include "ui_icons.h"
#include "villager.h"

/* The minigame's music, the size of its stage image, and the frames of its fades. */
#define MINIGAME_SEQUENCE 21
#define STAGE_DATA_SIZE 0x13DC
#define MINIGAME_FADE_FRAMES 10
/* Race positions are 20.12 fixed point: 0x40000 is the camera's start, 0x800000 the course end. */
#define MG1_CAMERA_START 0x40000
#define MG1_COURSE_END 0x800000
#define MG1_FINISH_LIMIT 0x940000
/* The horse race is race 3 in the bets. */
#define MG1_HORSE_RACE 3
/* Frames per second: the race speeds and the stamina ticks count in these units. */
#define MG1_FPS 30
#define MG1_NOT_FINISHED -1
#define MG1_STAMINA_MAX 255
#define MG1_STAMINA_RECOVER 5
#define MG1_STAMINA_TICK_COST 100
#define MG1_BOOST_SCALE 255
#define MG1_CROWD_CHANCE 34
#define MG1_FOND_AFFECTION 81
#define MG1_CHANCE_MAX 127
#define MG1_TRAINING_SCALE 25500
#define MG1_TRACK_Y 160
#define MG1_START_POSITION -131072
#define MG1_FIRST_CODE 77
#define MG1_CODE_STEP 26
#define MG1_EMPTY_RECORD -1
#define MG1_INTRO_FRAMES 60
#define MG1_FINISH_VOLUME_MIN 33
#define MG1_PACE_AHEAD 80
#define MG1_PACE_BEHIND 60
#define MG1_PACE_STEP 8
#define MG1_RIVAL_RANGE 368640
#define MG1_STAMINA_BAR_OFFSET 56
#define MG1_PAYLOAD_SIZE 0xC28
#define MG1_PAYLOAD_FILE 0x12
#define MG1_PAYLOAD_READ 123048
/* A horse-race win's gain per villager: friendship, then affection. */
#define MG1_FRIENDSHIP_GAIN 20
#define MG1_AFFECTION_GAIN 500
/* The kind that ends the course marker table. */
#define MG1_MARKER_END 0xFF

/* The frame timer's modes (see mg1_draw_timer). */
enum Mg1TimerMode {
    MG1_TIMER_RESET = -1,
    MG1_TIMER_TICK_DRAW = 0,
    MG1_TIMER_DRAW = 1,
    MG1_TIMER_TICK = 2,
};

/* Indices into mg1_glyph_texts. */
enum Mg1Text {
    MG1_TEXT_MEDALS = 0,
    MG1_TEXT_RACE_BETS = 23,
    MG1_TEXT_ODDS = 35,
    MG1_TEXT_TICKETS = 40,
    MG1_TEXT_CAN_WIN = 48,
};

static void mg1_load_sequence_step(void);
static void mg1_step_intro_wait(void);
static void mg1_run_screen(void);
static void mg1_leave_minigame(void);
static void mg1_setup_round(void);
static void mg1_bind_entries(void);
static void mg1_screen_names(void);
static void mg1_screen_stamina_tick(void);
static void mg1_screen_confirm_entries(void);
static void mg1_screen_entries_done(void);
static void mg1_screen_race_roster(void);
static void mg1_start_race(void);
static void mg1_race_frame(void);
static void mg1_results_screen(void);
static s32 mg1_draw_award_screen(void);
static void mg1_show_total(void);
static void mg1_init_records(void);
static void mg1_record_add(s32 value, s16 code);
static void mg1_draw_records(void);
static void mg1_draw_paused(void);
static void mg1_score_entries(void);
static void mg1_entries_boost(void);
static void mg1_entries_decay_speed(void);
static void mg1_update_finish_order(void);
static void mg1_pace_course(void);
static s32 mg1_check_race_end(void);
static s32 mg1_draw_results_table(s32 show_total);
static s16 mg1_training_speed(s16 training);
static void mg1_recover_stamina(void);
static s32 mg1_classify_entry(s16 index);
static s32 mg1_roll_chance_ahead(s16 weight);
static s32 mg1_roll_chance_behind(s16 weight);
static s32 mg1_roll_rival_chance(s16 weight);
static s32 mg1_roll_chance(s32 weight);
static s32 mg1_has_close_rival(s16 index);
static s32 mg1_stamina_above(s16 a, s16 b);
static s32 mg1_entry_is_fond(s16 index);
static s32 mg1_roll_above(s16 chance);
static void mg1_load_payload(void);
static void mg1_reset_state(void);
static void mg1_init_values(void);
static void mg1_init_slots(void);
static void mg1_reset_entries(void);
static void mg1_draw_background(s32 frame);
static void mg1_draw_entry(Mg1Entry *entry, s32 frame);
static void mg1_draw_entry_plain(Mg1Entry *entry, s32 frame);
static void mg1_update_anim_mode(void);
static void mg1_draw_course(void);
static void mg1_entries_sprite_mode2(void);
static void mg1_entry_sprite_mode0(Mg1Entry *entry);
static void mg1_entries_sprite_mode0(void);
static void mg1_entry_sprite_mode1(Mg1Entry *entry);
static void mg1_draw_timer(s16 mode);
static void mg1_fade_out(void);
static void mg1_stage_data_load(void);
static void mg1_staging_load(void);
static void mg1_audio_start(void);
static void mg1_villager_friendship_add(CharacterId villager);
static void mg1_villager_affection_add(CharacterId villager);

/* The glyph texts the screens show: 0x0FFD is a space, 0xFFFE a new line, 0xFFFF the end,
 * and 0x4000 shows message argument 0. */
static u16 mg1_glyph_texts[72] = {
    /* [0] "You have\n       medals" */
    0x0018, 0x002E, 0x0034, 0x0FFD, 0x0027, 0x0020, 0x0035, 0x0024, 0xFFFE, 0x0FFD, 0x0FFD, 0x0FFD,
    0x0FFD, 0x0FFD, 0x0FFD, 0x0FFD, 0x002C, 0x0024, 0x0023, 0x0020, 0x002B, 0x0032, 0xFFFF,
    /* [23] "Race <race> bets", <race> is the race number */
    0x0011, 0x0020, 0x0022, 0x0024, 0x0FFD, 0x4000, 0x0FFD, 0x0021, 0x0024, 0x0033, 0x0032, 0xFFFF,
    /* [35] "Odds" */
    0x000E, 0x0023, 0x0023, 0x0032, 0xFFFF,
    /* [40] "Tickets" */
    0x0013, 0x0028, 0x0022, 0x002A, 0x0024, 0x0033, 0x0032, 0xFFFF,
    /* [48] "You can win      medals" */
    0x0018, 0x002E, 0x0034, 0x0FFD, 0x0022, 0x0020, 0x002D, 0x0FFD, 0x0036, 0x0028, 0x002D, 0x0FFD,
    0x0FFD, 0x0FFD, 0x0FFD, 0x0FFD, 0x0FFD, 0x002C, 0x0024, 0x0023, 0x0020, 0x002B, 0x0032, 0xFFFF,
};
/* Run by mg1_run_state, one per entry sub-step. */
static Func mg1_step_handlers[5] = {
    mg1_load_sequence_step, mg1_step_intro_wait, mg1_run_screen,
    mg1_fade_out, mg1_leave_minigame,
};

/* Run by mg1_run_screen, one per mg1_state.step. */
static Func mg1_screen_handlers[11] = {
    mg1_setup_round, mg1_screen_names, mg1_screen_stamina_tick, mg1_screen_confirm_entries,
    mg1_screen_entries_done, mg1_screen_race_roster, mg1_start_race, mg1_race_frame,
    mg1_results_screen, (Func)mg1_draw_award_screen, mg1_show_total,
};
/* The steps of mg1_load_sequence_step, in order. */
enum Mg1LoadStep {
    MG1_LOAD_STAGE_DATA = 0,
    MG1_LOAD_STAGING = 1,
    MG1_LOAD_PAYLOAD = 2,
    MG1_LOAD_AUDIO = 3,
    MG1_LOAD_FADER_WAIT = 4,
    MG1_LOAD_DONE = 5,
};
static u8 mg1_load_state = MG1_LOAD_STAGE_DATA;
/* The race numbers "1", "2" and "3" as one-glyph texts, each ended by 0xFFFF. */
static u16 mg1_race_numbers[3][2] = {{0x41, 0xFFFF}, {0x42, 0xFFFF}, {0x43, 0xFFFF}};
static u16 mg1_shop_name_offsets[40][2] = {
    {0x0000, 0x0000}, {0x000E, 0x0000}, {0x001C, 0x0000},
    {0x0026, 0x0000}, {0x0034, 0x0000}, {0x0040, 0x0000},
    {0x004E, 0x0000}, {0x005C, 0x0000}, {0x0068, 0x0000},
    {0x0074, 0x0000}, {0x0082, 0x0000}, {0x0090, 0x0000},
    {0x009C, 0x0000}, {0x00A8, 0x0000}, {0x00B4, 0x0000},
    {0x00C2, 0x0000}, {0x00D0, 0x0000}, {0x00DE, 0x0000},
    {0x00EA, 0x0000}, {0x00F6, 0x0000}, {0x0104, 0x0000},
    {0x0112, 0x0000}, {0x0120, 0x0000}, {0x012A, 0x0000},
    {0x0138, 0x0000}, {0x0144, 0x0000}, {0x0152, 0x0000},
    {0x0160, 0x0000}, {0x016C, 0x0000}, {0x017A, 0x0000},
    {0x0186, 0x0000}, {0x0192, 0x0000}, {0x019C, 0x0000},
    {0x01AA, 0x0000}, {0x01B4, 0x0000}, {0x01C2, 0x0000},
    {0x01CC, 0x0000}, {0x01DA, 0x0000}, {0x01E6, 0x0000},
    {0x01F0, 0x0000},
};

static unsigned short mg1_kind_frames[14] = {
    0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006,
    0x0007, 0x0008, 0x0009, 0x000A, 0x000B, 0x000C, 0x0000,
};
static Mg1Marker mg1_markers[31] = {
    {8, 0, 819200}, {7, 0, 1146880}, {10, 0, 1638400},
    {9, 0, 1884160}, {6, 0, 2211840}, {4, 0, 2703360},
    {11, 0, 4096000}, {12, 0, 5160960}, {1, 0, 6553600},
    {0, 0, 6963200}, {5, 0, 7864320}, {2, 0, 9420800},
    {3, 0, 9912320}, {8, 0, 11223040}, {7, 0, 11550720},
    {10, 0, 12615680}, {9, 0, 12861440}, {6, 0, 13352960},
    {4, 0, 13762560}, {11, 0, 14581760}, {12, 0, 14991360},
    {1, 0, 15564800}, {0, 0, 15974400}, {5, 0, 16384000},
    {2, 0, 16711680}, {7, 0, 16957440}, {3, 0, 17448960},
    {8, 0, 17940480}, {10, 0, 18350080}, {9, 0, 18595840},
    {MG1_MARKER_END, 0, 0},
};
/* ":" as a glyph text. */
static u16 mg1_colon_text[2] = {0x66, 0xFFFF};
/* The glyph of each runner's number in the odds list, runner 0 first. */
static unsigned short mg1_glyph_codes[6] = {0x0079, 0x007A, 0x0091, 0x0092, 0x0093, 0x0094};
/* Frames counted on the stamina screen. Each MG1_FPS of them is a second, which drains the
 * player's horse by MG1_STAMINA_TICK_COST. */
static u8 mg1_second_ticks = 0;
/* The start of each of the payload's five parts, as mg1_load_payload binds them. */
static u32 mg1_payload_offsets[5] = {
    0x00000000, 0x000002EC, 0x000004D4, 0x000006BC, 0x0000082C};
/* The score weights: the row comes from mg1_classify_entry, and the column from the runner's
 * stamina band (high, middle, low) and whether it is fond (+1 when it is not). */
static u8 mg1_chance_table[3][6] = {
    {0, 80, 60, 90, 115, 100},
    {30, 100, 90, 110, 120, 110},
    {125, 127, 125, 127, 127, 120},
};
/* The award and timer frame counter; mg1_state.frameCount counts the race's frames separately. */
static u32 mg1_timer_frames = 0;

static u8 mg1_stage_data[STAGE_DATA_SIZE];
/* The payload copy, MG1_PAYLOAD_SIZE bytes. */
static u8 mg1_payload[3120];
static u16 mg1_number_text[10];
static u8 *mg1_part_ptr;
static u8 *mg1_part_ptrs[2];
/* Only the first six are used. */
static Mg1Slot mg1_slots[40];
/* The minigame's state block. */
static Mg1State mg1_state;
/* The sprite mg1_load_payload attaches to the payload's fifth part. */
static AnimState mg1_sprite;

void mg1_run_state(void) {
    mg1_step_handlers[game_state.progress.entrySubStep]();
}

/* Advance the load by one step. When the last step is done, reset the state, start the
 * fade-in and the music, and move on to the intro. */
static void mg1_load_sequence_step(void) {
    switch (mg1_load_state) {
    case MG1_LOAD_STAGE_DATA:
        mg1_stage_data_load();
        mg1_load_state++;
        break;
    case MG1_LOAD_STAGING:
        mg1_staging_load();
        mg1_load_state++;
        break;
    case MG1_LOAD_PAYLOAD:
        mg1_load_payload();
        mg1_load_state++;
        break;
    case MG1_LOAD_AUDIO:
        mg1_audio_start();
        mg1_load_state++;
        break;
    case MG1_LOAD_FADER_WAIT:
        if (sound_fader_ready() != 0) {
            mg1_load_state = MG1_LOAD_DONE;
            sound_update();
            sound_seq_start_reverb(0, 0, MINIGAME_SEQUENCE, 0x7F);
        }
        break;
    }
    if (mg1_load_state == MG1_LOAD_DONE) {
        mg1_state.step = 0;
        mg1_reset_state();
        menu_reset();
        fade_start_in(MINIGAME_FADE_FRAMES, 0, 2);
        sound_fade_in(MINIGAME_FADE_FRAMES);
        mg1_load_state = MG1_LOAD_STAGE_DATA;
        game_state.progress.entrySubStep++;
    }
}

/* Hold the start pose while the fade-in runs, then move on. */
static void mg1_step_intro_wait(void) {
    s32 i;
    s32 fade;
    mg1_draw_background(MG1_CAMERA_START);
    mg1_entries_sprite_mode2();
    for (i = 0; i < 6; i++) {
        mg1_draw_entry(&mg1_state.entries[i], MG1_CAMERA_START);
    }
    fade = fade_is_settled() == 1;
    if (sound_fader_mode() == SOUND_FADER_IDLE) {
        fade = (fade + 1) & 0xFF;
    }
    if (fade == 2) {
        game_state.progress.entrySubStep++;
    }
}
/* Run the menu, the handler of the current screen, and the frame timer. */
static void mg1_run_screen(void) {
    menu_run_state();
    mg1_screen_handlers[mg1_state.step]();
    mg1_draw_timer(MG1_TIMER_DRAW);
}

/* Start the blend-out of the screen and the music's fade, and move to the next step. */
static void mg1_fade_out(void) {
    fade_start_blend(MINIGAME_FADE_FRAMES);
    sound_fade_out(MINIGAME_FADE_FRAMES);
    game_state.progress.entrySubStep++;
}

/* Leave the minigame once the fade settles: release the sounds, grant the friendship and
 * affection of a horse-race win, and exit to the farm. */
static void mg1_leave_minigame(void) {
    s32 fade;
    mg1_draw_background(MG1_COURSE_END);
    if (fade_is_settled() == 1) {
        fade = 1;
    } else {
        fade = 0;
    }
    if (sound_fader_mode() == SOUND_FADER_IDLE) {
        fade = (fade + 1) & 0xFF;
    }
    if (fade == 2) {
        if (mg1_state.soundVoice != SOUND_VOICE_NONE) sound_release_voice(mg1_state.soundVoice);
        sound_seq_stop_first(0);
        sound_release_from(2, 0);
        sound_release_from(1, 0);
        if (game_state.raceBets.horseWon == 1) {
            mg1_villager_friendship_add(CHARACTER_JEFF);
            mg1_villager_friendship_add(CHARACTER_SASHA);
            mg1_villager_friendship_add(CHARACTER_SAIBARA);
            mg1_villager_friendship_add(CHARACTER_DOUG);
            mg1_villager_friendship_add(CHARACTER_BARLEY);
            mg1_villager_friendship_add(CHARACTER_MAY);
            mg1_villager_friendship_add(CHARACTER_MAYOR_THOMAS);
            mg1_villager_friendship_add(CHARACTER_RICK);
            mg1_villager_friendship_add(CHARACTER_DUKE);
            mg1_villager_friendship_add(CHARACTER_MANNA);
            mg1_villager_friendship_add(CHARACTER_UNNAMED_26);
            mg1_villager_friendship_add(CHARACTER_HARRIS);
            mg1_villager_affection_add(CHARACTER_KAREN);
            mg1_villager_affection_add(CHARACTER_ANN);
            mg1_villager_affection_add(CHARACTER_POPURI);
        }
        game_state.progress.entrySubStep = 0xFF;
        game_state.globalFlags |= GLOBAL_FLAG_MINIGAME_EXIT;
        main_request_scene(SCENE_FARM_MINIGAME_EXIT);
    }
}

/* Bind the entries for this round, then go to the stamina screen for the horse race or to
 * the names screen for any other race. */
static void mg1_setup_round(void) {
    s32 i;

    mg1_draw_background(MG1_CAMERA_START);
    mg1_entries_sprite_mode2();
    for (i = 0; i < 6; i++) {
        mg1_draw_entry(&mg1_state.entries[i], MG1_CAMERA_START);
    }
    mg1_bind_entries();
    if (game_state.raceBets.race == MG1_HORSE_RACE) {
        mg1_state.step = 2;
    } else {
        mg1_state.step = 1;
    }
}

/* Bind the six entries to their slots and to the runners of this round's bet row (the
 * player's horse row for the horse race), set the source of entry 2's sprite, and clear the
 * finish timer and the trail. */
static void mg1_bind_entries(void) {
    s32 index;
    s32 i;
    u8 *source;

    mg1_reset_entries();
    index = MG1_PLAYER_ENTRY;
    if (game_state.raceBets.race != MG1_HORSE_RACE) {
        index = game_state.raceBets.race;
    }
    for (i = 0; i < 6; i++) {
        mg1_state.entries[i].flags = 0;
        mg1_state.entries[i].slot = &mg1_slots[i];
        mg1_state.entries[i].nameOffset = mg1_shop_name_offsets[game_state.raceBets.rows[index][i].runner][0];
    }
    if (game_state.raceBets.race == MG1_HORSE_RACE) {
        source = mg1_part_ptr;
        mg1_state.entries[MG1_PLAYER_ENTRY].flags |= 1;
    } else {
        source = mg1_part_ptrs[0];
    }
    anim_bind_file(&mg1_state.entries[MG1_PLAYER_ENTRY].sprite, (AnimFile *)source);
    mg1_state.finishTimer = MG1_NOT_FINISHED;
    mg1_init_records();
}

static void mg1_screen_names(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
        text_set_source((u16)i, HORSE_NAME(mg1_state.entries[i].nameOffset));
    }
    mg1_entries_sprite_mode2();
    for (i = 0; i < 6; i++) {
        mg1_draw_entry(&mg1_state.entries[i], MG1_CAMERA_START);
    }
    mg1_draw_background(MG1_CAMERA_START);
    menu_request_open(4, 3);
    if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
        mg1_state.startTimer = 0;
        mg1_state.step = i;
    }
}

/* Count the seconds on the stamina screen, draining the player's horse each one. */
static void mg1_screen_stamina_tick(void) {
    s32 temp;
    s32 i;
    temp = mg1_second_ticks + 1;
    mg1_second_ticks = temp;
    if ((s8)temp >= MG1_FPS) {
        mg1_second_ticks = 0;
        mg1_state.entries[MG1_PLAYER_ENTRY].stamina -= MG1_STAMINA_TICK_COST;
        if (mg1_state.entries[MG1_PLAYER_ENTRY].stamina < 0) mg1_state.entries[MG1_PLAYER_ENTRY].stamina = MG1_STAMINA_MAX;
    }
    mg1_entries_sprite_mode2();
    for (i = 0; i < 6; i++) {
        mg1_draw_entry(&mg1_state.entries[i], MG1_CAMERA_START);
    }
    mg1_draw_background(MG1_CAMERA_START);
    menu_request_open(4, 0);
    if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
        mg1_state.step = 3;
    }
}
/* Ask the player to confirm the entries. Choice 1 goes on to the entries-done screen;
 * choice 2 resets the entries and goes back to the stamina screen. */
static void mg1_screen_confirm_entries(void) {
    s32 index;
    s32 status;

    for (index = 0; index < 6; index++) {
        if (index == 2) {
            index = 3;
        }
        mg1_draw_entry(&mg1_state.entries[index], MG1_CAMERA_START);
    }
    mg1_draw_entry(&mg1_state.entries[MG1_PLAYER_ENTRY], MG1_CAMERA_START);
    mg1_draw_background(MG1_CAMERA_START);
    menu_request_open(4, 1);
    index = menu_get_phase();
    if (index == TEXT_WINDOW_CLOSED) {
        status = menu_choice();
        if (status == 1) {
            mg1_state.step = 4;
            return;
        }
        if (status == index) {
            mg1_reset_entries();
            mg1_state.step = status;
        }
    }
}

/* Show the entries with the player's horse last, then reset them and move on to the roster. */
static void mg1_screen_entries_done(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
        if (i != 2) {
            mg1_draw_entry(&mg1_state.entries[i], MG1_CAMERA_START);
        }
    }
    mg1_draw_entry(&mg1_state.entries[MG1_PLAYER_ENTRY], MG1_CAMERA_START);
    mg1_draw_background(MG1_CAMERA_START);
    menu_request_open(4, 2);
    if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
        mg1_reset_entries();
        mg1_state.step = 5;
    }
}

static void mg1_screen_race_roster(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
        text_set_source((u16)i, HORSE_NAME(mg1_state.entries[i].nameOffset));
    }
    mg1_entries_sprite_mode2();
    for (i = 0; i < 6; i++) {
        mg1_draw_entry(&mg1_state.entries[i], MG1_CAMERA_START);
    }
    mg1_draw_background(MG1_CAMERA_START);
    menu_request_open(4, 4);
    if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
        mg1_state.startTimer = 0;
        mg1_state.step = 6;
    }
}

/* Count down the intro window after the roster, then start the race. */
static void mg1_start_race(void) {
    s32 i;
    s32 phase;
    s32 timer;
    mg1_entries_sprite_mode2();
    for (i = 0; i < 6; i++) {
        mg1_draw_entry(&mg1_state.entries[i], MG1_CAMERA_START);
    }
    mg1_draw_background(MG1_CAMERA_START);
    text_set_input(0);
    menu_set_sound_enabled(0);
    menu_request_open(4, 5);
    phase = menu_get_phase();
    if (phase == TEXT_WINDOW_END_WAIT) {
        timer = mg1_state.startTimer;
        timer++;
        mg1_state.startTimer = timer;
        if ((s16)timer < MG1_INTRO_FRAMES) {
            return;
        }
        for (i = 0; i < 6; i++) {
            mg1_state.entries[i].baseSpeed = mg1_training_speed(mg1_state.entries[i].training);
            mg1_state.entries[i].speed = mg1_state.entries[i].baseSpeed;
        }
        menu_begin_close();
        mg1_state.soundVoice = sound_play(0x38);
        mg1_entries_sprite_mode0();
        mg1_state.step = 7;
        mg1_state.finishTimer = MG1_NOT_FINISHED;
        mg1_draw_timer(MG1_TIMER_RESET);
    }
}

/* One frame of the minigame: pause toggle, periodic updates, movement, and the finish. */
static void mg1_race_frame(void) {
    s32 i;
    s32 speed;
    s16 level;

    mg1_draw_records();
    if (input_pad1->pressed & PAD_START) {
        mg1_state.paused = mg1_state.paused != 1;
        if (mg1_state.paused == 1) {
            mg1_state.soundVoice = sound_play(0x36);
        }
    }
    if (mg1_state.paused == 1) {
        mg1_draw_paused();
        return;
    }
    if (mg1_state.entries[mg1_state.order[0]].position <= MG1_COURSE_END) {
        mg1_draw_timer(MG1_TIMER_TICK);
    }
    mg1_state.frameCount++;
    for (i = 0; i < 6; i++) {
        speed = mg1_training_speed(mg1_state.entries[i].training);
        mg1_state.entries[i].baseSpeed = speed;
        mg1_state.entries[i].speed = speed;
    }
    if (game_state.raceBets.race == MG1_HORSE_RACE && (input_pad1->pressed & PAD_CROSS)) {
        mg1_state.entries[MG1_PLAYER_ENTRY].score = 1;
        pad_vibration_start(0, 0);
        mg1_state.soundVoice = sound_play(0x3D);
    }
    if (mg1_state.frameCount == mg1_state.frameCount / mg1_state.updateInterval * mg1_state.updateInterval) {
        mg1_recover_stamina();
        mg1_score_entries();
        mg1_entries_boost();
        mg1_entries_decay_speed();
        if (game_state.raceBets.race == MG1_HORSE_RACE) {
            mg1_state.entries[MG1_PLAYER_ENTRY].score = 0;
        }
    }
    if (mg1_state.coursePosition <= MG1_COURSE_END - 1) {
        mg1_pace_course();
    }
    for (i = 0; i < 6; i++) {
        mg1_state.entries[i].position += (mg1_state.entries[i].speed << 12) / MG1_FPS;
    }
    mg1_update_finish_order();
    mg1_state.coursePosition += (mg1_state.courseSpeed << 12) / MG1_FPS;
    for (i = 0; i < 6; i++) {
        mg1_draw_entry(&mg1_state.entries[i], mg1_state.coursePosition);
    }
    if ((mg1_state.soundPercent << 23) / 100 < mg1_state.coursePosition && mg1_state.finishTimer == MG1_NOT_FINISHED) {
        mg1_state.finishVoice = sound_play(0x43);
        mg1_state.finishTimer = 0;
    }
    if (mg1_state.coursePosition > MG1_COURSE_END - 1) {
        mg1_state.coursePosition = MG1_COURSE_END;
        mg1_state.courseSpeed = 0;
        mg1_state.courseSpeedFloor = 0;
        mg1_state.finishTimer++;
        level = 100 - mg1_state.finishTimer;
        if (level < MG1_FINISH_VOLUME_MIN) {
            level = MG1_FINISH_VOLUME_MIN;
        }
        sound_voice_set_volume(mg1_state.finishVoice, level, level);
    }
    if (mg1_check_race_end() == 1) {
        mg1_state.finishTimer = 0;
        mg1_state.step = 8;
    }
    mg1_draw_background(mg1_state.coursePosition);
}

/* The results screen: draw every entry's name in its finishing seat, the
 * player's own seat (entry 2) last, then wait for the dialog to close. */
static void mg1_results_screen(void) {
    s32 playerSeat;
    s32 i;
    s16 who;
    u16 *name;

    mg1_draw_background(MG1_COURSE_END);
    mg1_draw_records();
    playerSeat = 0;
    for (i = 0; i < 6; i++) {
        who = mg1_state.order[i];
        if (who == MG1_PLAYER_ENTRY) {
            playerSeat = i;
        } else {
            text_set_source((u16)i, HORSE_NAME(mg1_state.entries[who].nameOffset));
        }
    }
    if (game_state.raceBets.race == MG1_HORSE_RACE) {
        name = game_state.animals.horse.actor.name;
    } else {
        name = HORSE_NAME(mg1_state.entries[mg1_state.order[playerSeat]].nameOffset);
    }
    text_set_source((u16)playerSeat, name);
    menu_request_open(4, 6);
    if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
        sound_release_voice(mg1_state.finishVoice);
        mg1_state.step = 9;
    }
}

/* Draw the award window for the round's race, and play the award sound when the leader's bets
 * pay out. Once the window closes, move on to the total. */
static s32 mg1_draw_award_screen(void) {
    u_long *base;
    u_long *text_base;
    u_long *sub_object;
    s32 nextStep;
    s32 leaderTotal;
    u16 mode;
    u16 *digits;
    base = current_frame->otTags[0];
    sub_object = &base[3];
    text_base = &base[2];
    mode = 2;
    if (game_state.raceBets.race != MG1_HORSE_RACE) {
        mode = game_state.raceBets.race;
    }
    mg1_draw_records();
    mg1_draw_background(MG1_COURSE_END);
    screen_fill_quad(0x40, 0x40, 0x40);
    text_set_source(0, mg1_race_numbers[mode]);
    window_frame_draw_at(sub_object, 1, 40, 48, 244, 114);
    text_draw_plain(text_base, &mg1_glyph_texts[MG1_TEXT_RACE_BETS], 61, 50);
    leaderTotal = mg1_draw_results_table(1);
    window_frame_draw_at(sub_object, 1, 195, 16, 111, 30);
    text_draw_plain(text_base, mg1_glyph_texts, 199, 18);
    digits = text_format_number(game_state.player.medals, mg1_number_text, 7);
    *digits = -1;
    text_draw_plain(text_base, mg1_number_text, 197, 30);
    menu_request_open(4, 7);
    if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
        nextStep = 10;
        if (leaderTotal != 0) {
            mg1_state.soundVoice = sound_play(0x3E);
        }
        mg1_state.step = nextStep;
        return nextStep;
    }
}

/* Show the round's total, the results table plus the player's medals (capped at 99999), and
 * record it and whether the horse won in the farm's race bets. */
static void mg1_show_total(void) {
    u_long *base;
    u_long *sub_object;
    u_long *text_base;
    s32 total;
    u16 *digits;
    FarmState *state;

    base = current_frame->otTags[0];
    sub_object = &base[3];
    text_base = &base[2];
    mg1_draw_records();
    mg1_draw_background(MG1_COURSE_END);
    screen_fill_quad(0x40, 0x40, 0x40);
    window_frame_draw_at(sub_object, 1, 40, 48, 244, 114);
    text_draw_plain(text_base, &mg1_glyph_texts[MG1_TEXT_RACE_BETS], 61, 50);
    state = &game_state;
    total = mg1_draw_results_table(0) + state->player.medals;
    if (total > 99999) {
        total = 99999;
    }
    window_frame_draw_at(sub_object, 1, 195, 16, 111, 30);
    text_draw_plain(text_base, mg1_glyph_texts, 199, 18);
    digits = text_format_number(total, mg1_number_text, 7);
    *digits = -1;
    text_draw_plain(text_base, mg1_number_text, 197, 30);
    if (game_state.raceBets.race == MG1_HORSE_RACE) {
        if (mg1_state.order[0] == MG1_PLAYER_ENTRY) {
            game_state.raceBets.horseWon = 1;
        } else {
            game_state.raceBets.horseWon = 0;
        }
    } else {
        game_state.raceBets.horseWon = 0;
    }
    game_state.raceBets.medalTotal = total;
    menu_request_open(4, 8);
    if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
        game_state.progress.entrySubStep++;
    }
}

/* Reset the trail: the six runners start with a record, the rest are empty. */
static void mg1_init_records(void) {
    s16 i;

    i = 0;
    do {
        mg1_state.records[i].position = MG1_START_POSITION;
        mg1_state.records[i].code = MG1_EMPTY_RECORD;
        if (i < MG1_ENTRY_COUNT) {
            mg1_state.records[i].code = MG1_FIRST_CODE + i * MG1_CODE_STEP;
        }
        i++;
    } while (i < MG1_RECORD_COUNT);
    mg1_state.recordCount = MG1_ENTRY_COUNT;
}
/* Add a trail record at a position with a sprite code, overwriting the oldest once the ring
 * is full. */
static void mg1_record_add(s32 value, s16 code) {
    if (mg1_state.recordCount >= MG1_RECORD_COUNT) mg1_state.recordCount = 0;
    mg1_state.records[mg1_state.recordCount].position = value;
    mg1_state.records[mg1_state.recordCount].code = code;
    mg1_state.recordCount++;
}

/* Draw the trail records that are set, each at its place on the course relative to the
 * camera. */
static void mg1_draw_records(void) {
    s16 i;
    s32 delta;
    u8 flip;
    s16 y;

    for (i = 0; i < MG1_RECORD_COUNT; i++) {
        if (mg1_state.records[i].code != MG1_EMPTY_RECORD) {
            delta = mg1_state.records[i].position - mg1_state.coursePosition;
            flip = i - i / 12 * 12 >= 6;
            y = MG1_TRACK_Y - delta / 4096;
            anim_draw_sprt_cursor(&current_frame->otTags[0][21], &mg1_state.courseSprite, 3, flip, y,
                          mg1_state.records[i].code);
        }
    }
}

/* Draw the course and the runners while paused, without adding trail records. */
static void mg1_draw_paused(void) {
    s32 i;
    mg1_draw_course();
    for (i = 0; i < 6; i++) {
        mg1_draw_entry_plain(&mg1_state.entries[i], mg1_state.coursePosition);
    }
    mg1_draw_background(mg1_state.coursePosition);
}

/* Score each entry for this round: the player's entry sits out when the player bet on the
 * horse race, an entry flagged 1 is cleared instead, and the rest roll a score from the
 * chance table at the row from mg1_classify_entry and the column for its stamina and fondness. */
static void mg1_score_entries(void) {
    s32 i;
    s16 slot;
    s32 row;
    s32 fond;
    s32 column;

    for (i = 0; i < 6; i++) {
        if (i == MG1_PLAYER_ENTRY && game_state.raceBets.race == MG1_HORSE_RACE) {
            continue;
        }
        if (mg1_state.entries[i].score == 1) {
            mg1_state.entries[i].score = 0;
            continue;
        }
        slot = i;
        row = mg1_classify_entry(slot);
        fond = mg1_entry_is_fond(slot);
        if (mg1_stamina_above(slot, mg1_state.staminaBound1)) {
            column = 0;
        } else if (mg1_stamina_above(slot, mg1_state.staminaBound2)) {
            column = 2;
        } else {
            column = 4;
        }
        if (!fond) {
            column++;
        }
        mg1_state.entries[i].score = mg1_roll_above(mg1_state.chances[row][column]);
    }
}

/* A scoring entry with the stamina for a dash spends it and gains speed. On the first dash of
 * the update the crowd may cheer, unless the player bet on the horse race. */
static void mg1_entries_boost(void) {
    s32 i;
    s32 soundDue;
    s16 step;

    soundDue = 1;
    for (i = 0; i < 6; i++) {
        if (mg1_state.entries[i].score != 0 && mg1_state.entries[i].stamina >= mg1_state.dashCost) {
            mg1_state.entries[i].stamina -= mg1_state.dashCost;
            step = (mg1_state.entries[i].speed * (mg1_state.entries[i].affection + mg1_state.dashBoost)) / (mg1_state.dashDivisor * MG1_BOOST_SCALE);
            mg1_state.entries[i].speed += step;
            if (soundDue == 1) {
                if (game_state.raceBets.race != MG1_HORSE_RACE &&
                    (u32)rand_range(100) < MG1_CROWD_CHANCE) {
                    mg1_state.soundVoice = sound_play(0x3D);
                }
                soundDue = 0;
            }
        }
    }
}

/* Each update, the speed above the base falls back by a share that grows as the stamina runs
 * down; the speed never drops below the base. */
static void mg1_entries_decay_speed(void) {
    s32 i;
    s16 stamina;
    s16 decay;

    for (i = 0; i < 6; i++) {
        stamina = mg1_state.entries[i].stamina;
        /* The + 1 keeps the divisor off zero when the stamina is spent. */
        decay = ((mg1_state.entries[i].speed - mg1_state.entries[i].baseSpeed) * MG1_BOOST_SCALE) /
                  ((stamina + 1) * mg1_state.decayDivisor);
        mg1_state.entries[i].speed = mg1_state.entries[i].speed - decay;
        if (mg1_state.entries[i].baseSpeed >= mg1_state.entries[i].speed) {
            mg1_state.entries[i].speed = mg1_state.entries[i].baseSpeed;
        }
    }
}

/* order queues the entries that have reached the course end, in the order they got
 * there. Until one has, order[0] is the entry furthest along (the leader). */
static void mg1_update_finish_order(void) {
    s32 i;
    s32 j;
    s16 best;

    if (mg1_state.orderCount == 0)
        mg1_state.order[0] = -1;
    for (i = 0; i < 6; i++) {
        if (mg1_state.entries[i].position > MG1_COURSE_END - 1) {
            for (j = 0; j < 6; j++) {
                if (mg1_state.order[j] == i)
                    break;
            }
            if (j == 6) {
                mg1_state.order[mg1_state.orderCount] = i;
                mg1_state.orderCount++;
            }
        }
    }
    if (mg1_state.orderCount == 0) {
        best = 0;
        for (i = 0; i < 6; i++) {
            for (j = 0; j < 6; j++) {
                if (mg1_state.entries[best].position < mg1_state.entries[j].position)
                    best = j;
            }
        }
        mg1_state.order[0] = best;
    }
}

/* Pace the course to the race leader (the player's entry when the race is the horse race):
 * speed up by an eighth when the leader is more than 80 ahead of the camera, slow down by an
 * eighth when more than 60 behind, else take the leader's base speed. */
static void mg1_pace_course(void) {
    s32 index;
    u16 floorSpeed;
    s32 leaderPosition;

    if (game_state.raceBets.race == MG1_HORSE_RACE) {
        index = MG1_PLAYER_ENTRY;
    } else {
        index = mg1_state.order[0];
    }
    floorSpeed = mg1_state.entries[index].baseSpeed;
    mg1_state.courseSpeedFloor = floorSpeed;
    leaderPosition = mg1_state.entries[index].position;
    if (leaderPosition / 4096 > mg1_state.coursePosition / 4096 + MG1_PACE_AHEAD) {
        mg1_state.courseSpeed += mg1_state.courseSpeed / MG1_PACE_STEP;
    } else if (leaderPosition / 4096 < mg1_state.coursePosition / 4096 - MG1_PACE_BEHIND) {
        mg1_state.courseSpeed -= mg1_state.courseSpeed / MG1_PACE_STEP;
    } else {
        mg1_state.courseSpeed = floorSpeed;
    }
}

static s32 mg1_check_race_end(void) {
    s32 i;
    u8 unfinished;

    if (mg1_state.entries[mg1_state.order[0]].position > MG1_COURSE_END)
        mg1_entry_sprite_mode1(&mg1_state.entries[mg1_state.order[0]]);
    for (i = 0; i < 6; i++) {
        if (mg1_state.entries[i].position > MG1_FINISH_LIMIT - 1)
            mg1_state.entries[i].position = MG1_FINISH_LIMIT;
    }
    unfinished = 0;
    for (i = 0; i < 6; i++) {
        if (mg1_state.entries[i].position <= MG1_FINISH_LIMIT - 1)
            unfinished++;
    }
    return unfinished == 0;
}

static inline s16 leader_seat(void) {
    return mg1_state.order[0];
}

static inline s32 is_leader(s32 i) {
    return leader_seat() == i;
}

/* Draw the six result rows from y = 64, 13 pixels apart: the seat marker, the name, and the
 * odds and tickets of each entry. Returns the leader's odds times tickets, the amount the
 * player can win, and shows it only when show_total is set. */
static s32 mg1_draw_results_table(s32 show_total) {
    u_long *text_base;
    u16 total;
    u16 row;
    s32 i;
    s16 y;
    u16 *label;

    text_base = &current_frame->otTags[0][2];
    row = 2;
    if (game_state.raceBets.race != MG1_HORSE_RACE) {
        row = game_state.raceBets.race;
    }
    y = 64;
    total = 0;
    for (i = 0; i < 6; i++) {
        if (is_leader(i)) {
            mg1_number_text[0] = 0x90;
            mg1_number_text[1] = 0xFFFF;
            text_draw_plain(text_base, mg1_number_text, 46, y);
        }
        mg1_number_text[1] = 0xFFFF;
        mg1_number_text[0] = mg1_glyph_codes[i];
        text_draw_plain(text_base, mg1_number_text, 56, y);
        if (game_state.raceBets.race == MG1_HORSE_RACE && i == MG1_PLAYER_ENTRY) {
            label = game_state.animals.horse.actor.name;
        } else {
            label = HORSE_NAME(mg1_state.entries[i].nameOffset);
        }
        text_draw_plain(text_base, label, 70, y);
        text_draw_plain(text_base, &mg1_glyph_texts[MG1_TEXT_ODDS], 142, y);
        *text_format_number(game_state.raceBets.rows[row][i].odds, mg1_number_text, 2) = 0xFFFF;
        text_draw_plain(text_base, mg1_number_text, 178, y);
        *text_format_number(game_state.raceBets.rows[row][i].tickets, mg1_number_text, 2) = 0xFFFF;
        text_draw_plain(text_base, mg1_number_text, 202, y);
        text_draw_plain(text_base, &mg1_glyph_texts[MG1_TEXT_TICKETS], 222, y);
        if (is_leader(i)) {
            total += game_state.raceBets.rows[row][i].odds * game_state.raceBets.rows[row][i].tickets;
        }
        y += 13;
    }
    text_draw_plain(text_base, &mg1_glyph_texts[MG1_TEXT_CAN_WIN], 78, 144);
    *text_format_number(show_total ? total : 0, mg1_number_text, 4) = 0xFFFF;
    text_draw_plain(text_base, mg1_number_text, 182, 144);
    return (s16)total;
}

/* The speed a runner's training gives: the base speed plus a share of it that grows with the
 * training. */
static s16 mg1_training_speed(s16 training) {
    s32 product;

    product = mg1_state.trainingBase * (training + mg1_state.trainingAddend) * mg1_state.trainingMultiplier;
    return mg1_state.trainingBase + product / MG1_TRAINING_SCALE;
}

/* Each update, every runner below full stamina recovers MG1_STAMINA_RECOVER. */
static void mg1_recover_stamina(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
        if (mg1_state.entries[i].stamina < MG1_STAMINA_MAX) {
            mg1_state.entries[i].stamina = (u16)mg1_state.entries[i].stamina + MG1_STAMINA_RECOVER;
        }
    }
}

/* Classify an entry into a row (0..2) of the chance table: the race's leader sets the
 * weighting (ahead or behind), the roll gives row 1 or 2, and a close rival that wins its
 * roll lowers the row by one. */
static s32 mg1_classify_entry(s16 index) {
    s16 row;
    s32 (*roll_chance)(s16);

    if (mg1_state.entries[mg1_state.order[0]].position >= (mg1_state.leaderThreshold << 12)) {
        roll_chance = mg1_roll_chance_ahead;
    } else {
        roll_chance = mg1_roll_chance_behind;
    }
    row = roll_chance(mg1_state.entries[index].chance) != 0 ? 1 : 2;
    if (mg1_has_close_rival(index) != 0) {
        if (mg1_roll_rival_chance(mg1_state.entries[index].rivalChance) != 0) {
            row--;
        }
    }
    return row;
}

static s32 mg1_roll_chance_ahead(s16 weight) {
    return mg1_roll_chance(weight + 1);
}
static s32 mg1_roll_chance_behind(s16 weight) {
    return mg1_roll_chance(MG1_CHANCE_MAX - weight);
}
static s32 mg1_roll_rival_chance(s16 weight) {
    return mg1_roll_chance(weight + 1);
}
static s32 mg1_roll_chance(s32 weight) {
    u16 percent = (weight * 100) / MG1_CHANCE_MAX;
    u32 roll = rand_range(100);
    return roll < percent;
}
static s32 mg1_has_close_rival(s16 index) {
    Mg1State *state;
    s32 i;
    s32 diff;

    state = &mg1_state;
    for (i = 0; i < 6; i++) {
        if (i == index) {
            continue;
        }
        diff = state->entries[i].position - state->entries[index].position;
        if (diff >= 0) {
            if (diff < MG1_RIVAL_RANGE) {
                return 1;
            }
        }
    }
    return 0;
}
static s32 mg1_stamina_above(s16 index, s16 bound) {
    return bound < mg1_state.entries[index].stamina;
}
static s32 mg1_entry_is_fond(s16 index) {
    return mg1_state.entries[index].affection >= MG1_FOND_AFFECTION;
}
static s32 mg1_roll_above(s16 chance) {
    s16 value = chance;
    return value < rand_range(MG1_CHANCE_MAX);
}

/* Read the stage image (A_FILE member 0) and its texture pack, copy the image into our
 * buffer and into the farm area's copy, and bind it as the stage sprite. */
static void mg1_stage_data_load(void) {
    u8 *ready;

    DrawSync(0);
    ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
    afile_read_bytes(0, 0x25000, 0x5800, STAGING_BUFFER, 1, 1);
    do {
        ready = afile_read_bytes(0, 0x25000, 0x5800, STAGING_BUFFER, 1, 0);
    } while (ready == 0);
    texture_pack_load(STAGING_BUFFER + STAGE_DATA_SIZE);
    memcpy(mg1_stage_data, STAGING_BUFFER, STAGE_DATA_SIZE);
    memcpy(farm_area.stageData, STAGING_BUFFER, STAGE_DATA_SIZE);
    anim_bind_file(&mg1_state.stageSprite, (AnimFile *)mg1_stage_data);
}

/* Read the texture pack into the staging buffer and unpack it. */
static void mg1_staging_load(void) {
    DrawSync(0);
    ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
    afile_read_bytes(0, 0, 0x2000, STAGING_BUFFER, 1, 1);
    do {
    } while (afile_read_bytes(0, 0, 0x2000, STAGING_BUFFER, 1, 0) == 0);
    texture_pack_load(STAGING_BUFFER);
}

/* Read the payload file, copy it into mg1_payload, and attach each object to its part of the copy. */
static void mg1_load_payload(void) {
    s32 i;

    DrawSync(0);
    ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
    afile_read_bytes(MG1_PAYLOAD_FILE, 0, MG1_PAYLOAD_READ, STAGING_BUFFER, 1, 1);
    while (afile_read_bytes(MG1_PAYLOAD_FILE, 0, MG1_PAYLOAD_READ, STAGING_BUFFER, 1, 0) == 0) {
    }
    texture_pack_load(STAGING_BUFFER + MG1_PAYLOAD_SIZE);
    memcpy(mg1_payload, STAGING_BUFFER, MG1_PAYLOAD_SIZE);
    mg1_part_ptr = mg1_payload + mg1_payload_offsets[1];
    mg1_part_ptrs[0] = mg1_payload + mg1_payload_offsets[2];
    anim_bind_file(&mg1_sprite, (AnimFile *)(mg1_payload + mg1_payload_offsets[4]));
    anim_bind_file(&mg1_state.courseSprite, (AnimFile *)(mg1_payload + mg1_payload_offsets[0]));
    anim_bind_file(&mg1_state.staminaAnimation, (AnimFile *)(mg1_payload + mg1_payload_offsets[3]));
    for (i = 0; i < 6; i++) {
        anim_bind_file(&mg1_state.entries[i].sprite, (AnimFile *)(mg1_payload + mg1_payload_offsets[2]));
    }
}

/* Stop the two sound banks and start the minigame's music. */
static void mg1_audio_start(void) {
    sound_release_from(2, 0);
    sound_release_from(1, 0);
    sound_request(SOUND_REQUEST_SEQ_2, 0);
    sound_request(MINIGAME_SEQUENCE, 1);
}

/* Reset the minigame state block and the round values. */
static void mg1_reset_state(void) {
    s32 i;

    mg1_state.paused = 0;
    mg1_state.step = 0;
    mg1_state.updateInterval = 6;
    mg1_state.trainingBase = 128;
    mg1_state.trainingAddend = 0;
    mg1_state.trainingMultiplier = 4;
    mg1_state.dashBoost = 150;
    mg1_state.dashDivisor = 1;
    mg1_state.dashCost = 40;
    mg1_state.decayDivisor = 18;
    mg1_state.staminaBound1 = 170;
    mg1_state.staminaBound2 = 100;
    mg1_state.frameCount = 0;
    mg1_state.courseSpeed = 0;
    mg1_state.courseSpeedFloor = 0;
    mg1_state.coursePosition = MG1_CAMERA_START;
    for (i = 0; i < 6; i++) {
        mg1_state.order[i] = -1;
    }
    mg1_state.soundVoice = SOUND_VOICE_NONE;
    mg1_state.soundPercent = 50;
    mg1_state.leaderThreshold = 1024;
    mg1_state.orderCount = 0;
    mg1_init_values();
    mg1_init_slots();
    mg1_reset_entries();
    mg1_draw_timer(MG1_TIMER_RESET);
}

/* Copy the default chance table into the state. */
static void mg1_init_values(void) {
    s32 i = 0;
    do {
        s32 j = 0;
        do {
            mg1_state.chances[i][j] = mg1_chance_table[i][j];
            j++;
        } while (j < 6);
        i++;
    } while (i < 3);
}
/* Give each slot its sprite kind, and the code and draw order of its runner. */
static void mg1_init_slots(void) {
    s32 i;

    mg1_slots[0].kind = 5;
    mg1_slots[1].kind = 0;
    mg1_slots[2].kind = 4;
    mg1_slots[3].kind = 3;
    mg1_slots[4].kind = 1;
    mg1_slots[5].kind = 2;
    for (i = 0; i < 6; i++) {
        mg1_slots[i].code = MG1_FIRST_CODE + i * MG1_CODE_STEP;
        mg1_slots[i].order = 20 - i;
    }
}

/* Reset the six runners for a new round. Each takes its training, affection and chances from
 * this race's bet row; the player's horse takes its training and affection from the horse. */
static void mg1_reset_entries(void) {
    u16 mode;
    s32 i;

    mode = 2;
    if (game_state.raceBets.race != MG1_HORSE_RACE) {
        mode = game_state.raceBets.race;
    }
    for (i = 0; i < 6; i++) {
        mg1_state.entries[i].flags = 0;
        mg1_state.entries[i].stamina = MG1_STAMINA_MAX;
        mg1_state.entries[i].baseSpeed = 0;
        mg1_state.entries[i].speed = 0;
        mg1_state.entries[i].score = 0;
        mg1_state.entries[i].training = game_state.raceBets.rows[mode][i].training;
        mg1_state.entries[i].affection = game_state.raceBets.rows[mode][i].affection;
        mg1_state.entries[i].chance = game_state.raceBets.rows[mode][i].chance;
        mg1_state.entries[i].rivalChance = game_state.raceBets.rows[mode][i].rivalChance;
        mg1_state.entries[i].position = MG1_START_POSITION;
        mg1_state.entries[i].slot = &mg1_slots[i];
    }
    if (game_state.raceBets.race == MG1_HORSE_RACE) {
        mg1_state.entries[MG1_PLAYER_ENTRY].flags = 1;
        mg1_state.entries[MG1_PLAYER_ENTRY].training = game_state.animals.horse.training;
        mg1_state.entries[MG1_PLAYER_ENTRY].affection = game_state.animals.horse.actor.affection;
    }
}

#define MG1_COURSE_SPRITE (&mg1_state.courseSprite)
/* Draw the course sprite centred on x. */
static inline void mg1_draw_centred(u_long *ot, s16 x) {
    anim_draw_sprt_cursor(ot, MG1_COURSE_SPRITE, mg1_state.courseSprite.animation, mg1_state.courseSprite.frame, x - 32, 0);
}
/* Draw the course background for the camera at frame: the two scrolling rows of tiles, the
 * scenery pieces at the camera, and the track markers near it. */
static void mg1_draw_background(s32 frame) {
    u_long *ot;
    s16 x;
    s32 scroll;
    s32 i;

    ot = &current_frame->otTags[0][22];
    anim_select_range(MG1_COURSE_SPRITE, 0);
    anim_set_frame(MG1_COURSE_SPRITE, 0);
    for (x = frame / 4096 - frame / 262144 * 64 - 64; x < 384; x += 64) {
        anim_draw_sprt_cursor(&current_frame->otTags[0][23], MG1_COURSE_SPRITE, mg1_state.courseSprite.animation,
                      mg1_state.courseSprite.frame, x, 0);
    }
    anim_select_range(MG1_COURSE_SPRITE, 1);
    for (x = frame / 4096 * 2 - frame / 155648 * 76 - 76; x < 396; x += 76) {
        anim_draw_sprt_cursor(&current_frame->otTags[0][6], MG1_COURSE_SPRITE, mg1_state.courseSprite.animation,
                      mg1_state.courseSprite.frame, x, 0);
    }
    scroll = frame / 4096;
    if (scroll >= -191 && scroll < 192) {
        anim_select_range(MG1_COURSE_SPRITE, 0);
        anim_set_frame(MG1_COURSE_SPRITE, 1);
        mg1_draw_centred(ot, scroll + 160);
        anim_select_range(MG1_COURSE_SPRITE, 2);
        mg1_draw_centred(ot, scroll + 224);
    }
    if (scroll > 1856 && scroll < 2240) {
        anim_select_range(MG1_COURSE_SPRITE, 0);
        anim_set_frame(MG1_COURSE_SPRITE, 1);
        mg1_draw_centred(ot, scroll - 1888);
    }
    for (i = 0; mg1_markers[i].kind != MG1_MARKER_END; i++) {
        if (frame / 4096 * 2 + HWD0 > mg1_markers[i].position / 4096 &&
            frame / 4096 * 2 - HWD0 < mg1_markers[i].position / 4096) {
            x = HWD0 / 2 + frame / 4096 * 2 - mg1_markers[i].position / 4096;
            anim_select_range(&mg1_sprite, mg1_kind_frames[mg1_markers[i].kind]);
            /* the argument is the flip: 1 = mirrored */
            anim_set_frame(&mg1_sprite, x < HWD0 / 2);
            anim_draw_ft4_cursor(&current_frame->otTags[0][5], &mg1_sprite, mg1_sprite.animation,
                          mg1_sprite.frame, x, VWD0, FIXED_ONE, 0, 0);
        }
    }
}
#undef MG1_COURSE_SPRITE

/* Draw one runner. Every tenth frame it also adds a trail record of its position, and
 * for the player's runner it updates the stamina bar's range. */
static void mg1_draw_entry(Mg1Entry *entry, s32 frame) {
    s32 delta;
    s32 depth;
    s16 z;
    s16 code;
    Mg1Slot *slot;
    u_long *buffer;
    u32 count;
    delta = entry->position - frame;
    depth = delta >> 12;
    if (delta < 0) {
        /* Negative depths round toward zero. */
        depth = (delta + 0xFFF) >> 12;
    }
    z = MG1_TRACK_Y - depth;
    slot = entry->slot;
    code = slot->code;
    buffer = &current_frame->otTags[0][slot->order];
    if (!(entry->flags & 1)) {
        anim_set_offsets(&entry->sprite, 0, 0, 0, slot->kind);
    }
    anim_draw_ft4_advance(buffer, &entry->sprite, z, code);
    ui_draw_icon_18_sub(buffer, z, code, 1);
    if (entry->flags & 1) {
        mg1_update_anim_mode();
        anim_draw_ft4_advance(&current_frame->otTags[0][4], &mg1_state.staminaAnimation, z - MG1_STAMINA_BAR_OFFSET, code - MG1_STAMINA_BAR_OFFSET);
    }
    count = mg1_state.frameCount;
    if (count == (((count + 1) / 10) * 10) - 1) {
        mg1_record_add(entry->position, code);
    }
}

/* Draw one runner without the record, and with its stamina bar when it is the player's. */
static void mg1_draw_entry_plain(Mg1Entry *entry, s32 frame) {
    s32 delta;
    s32 depth;
    s16 z;
    s16 code;
    u_long *buffer;
    Mg1Slot *slot;

    delta = entry->position - frame;
    depth = delta >> 12;
    if (delta < 0) {
        depth = (delta + 0xFFF) >> 12;
    }
    z = MG1_TRACK_Y - depth;
    slot = entry->slot;
    code = slot->code;
    buffer = &current_frame->otTags[0][slot->order];
    if (!(entry->flags & 1)) {
        anim_set_offsets(&entry->sprite, 0, 0, 0, slot->kind);
    }
    anim_draw_sprt_cursor(buffer, &entry->sprite, entry->sprite.animation, entry->sprite.frame, z, code);
    ui_draw_icon_18_sub(buffer, z, code, 1);
    if (entry->flags & 1) {
        mg1_update_anim_mode();
        anim_draw_sprt_cursor(&current_frame->otTags[0][4], &mg1_state.staminaAnimation, mg1_state.staminaAnimation.animation,
                      mg1_state.staminaAnimation.frame, z - MG1_STAMINA_BAR_OFFSET, code - MG1_STAMINA_BAR_OFFSET);
    }
}

/* Pick the stamina bar's range from the player's stamina: 170 or more is range 0, 3 or more
 * is range 1, and less is range 2. */
static void mg1_update_anim_mode(void) {
    s16 stamina;
    s32 range;

    stamina = mg1_state.entries[MG1_PLAYER_ENTRY].stamina;
    if (stamina >= 170)
        range = 0;
    else if (stamina >= 3)
        range = 1;
    else
        range = 2;
    if (mg1_state.staminaAnimation.animation != range)
        anim_select_range(&mg1_state.staminaAnimation, range);
}

static void mg1_draw_course(void) {
    anim_select_range(&mg1_state.courseSprite, 4);
    anim_draw_sprt_cursor(&current_frame->otTags[0][1], &mg1_state.courseSprite, mg1_state.courseSprite.animation,
                  mg1_state.courseSprite.frame, 0, 0);
}

static void mg1_entry_sprite_mode2(Mg1Entry *entry) {
    if (entry->sprite.animation != 2) {
        anim_select_range(&entry->sprite, 2);
    }
}

static void mg1_entries_sprite_mode2(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
        mg1_entry_sprite_mode2(&mg1_state.entries[i]);
    }
}

static void mg1_entry_sprite_mode0(Mg1Entry *entry) {
    if (entry->sprite.animation != 0) {
        anim_select_range(&entry->sprite, 0);
    }
}

static void mg1_entries_sprite_mode0(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
        mg1_entry_sprite_mode0(&mg1_state.entries[i]);
    }
}
static void mg1_entry_sprite_mode1(Mg1Entry *entry) {
    if (entry->sprite.animation != 1) {
        anim_select_range(&entry->sprite, 1);
    }
}

/* Frame timer: MG1_TIMER_RESET clears it, MG1_TIMER_TICK counts a frame, MG1_TIMER_DRAW
 * draws it as minutes, seconds and hundredths at 30 fps, and MG1_TIMER_TICK_DRAW does both. */
static void mg1_draw_timer(s16 mode) {
    u_long *ot;

    ot = &current_frame->otTags[0][2];
    if (mode == MG1_TIMER_RESET)
        mg1_timer_frames = 0;
    if (mode == MG1_TIMER_TICK_DRAW || mode == MG1_TIMER_TICK)
        mg1_timer_frames++;
    if ((u16)mode < 2) {
        window_frame_draw_at(&current_frame->otTags[0][3], 1, 10, 20, 107, 17);
        text_draw_number(mg1_timer_frames / 1800 - mg1_timer_frames / 108000 * 60, 2, ot, 12, 22);
        text_draw_plain(ot, mg1_colon_text, 38, 22);
        text_draw_number(mg1_timer_frames / 300 - mg1_timer_frames / 1800 * 6, 1, ot, 51, 22);
        text_draw_number(mg1_timer_frames / 30 - mg1_timer_frames / 300 * 10, 1, ot, 64, 22);
        text_draw_plain(ot, mg1_colon_text, 77, 22);
        text_draw_number(mg1_timer_frames / 3 - mg1_timer_frames / 30 * 10, 1, ot, 90, 22);
        text_draw_number(mg1_timer_frames * 100 / 30 - mg1_timer_frames / 3 * 10, 1, ot, 103, 22);
    }
}

/* Raise a villager's friendship by MG1_FRIENDSHIP_GAIN, up to FRIENDSHIP_MAX. */
static void mg1_villager_friendship_add(CharacterId villager) {
    if (game_state.villagers[villager].friendship < FRIENDSHIP_MAX - MG1_FRIENDSHIP_GAIN) {
        game_state.villagers[villager].friendship += MG1_FRIENDSHIP_GAIN;
    } else {
        game_state.villagers[villager].friendship = FRIENDSHIP_MAX;
    }
}

/* Raise a villager's affection by MG1_AFFECTION_GAIN, capped at 65535. */
static void mg1_villager_affection_add(CharacterId villager) {
    if (game_state.villagers[villager].affection <= 65034) {
        game_state.villagers[villager].affection += MG1_AFFECTION_GAIN;
    } else {
        game_state.villagers[villager].affection = 65535;
    }
}

/* Overlay 30_GF_swind: the debug settings window. Each editor page is defined
 * just ahead of the function that shows it. The strings are Shift-JIS; each
 * label's comment gives its English meaning. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "types.h"
#include "actor.h"
#include "afile.h"
#include "animals.h"
#include "cd_audio.h"
#include "estate.h"
#include "event_commands.h"
#include "event_flags.h"
#include "farm_data_size.h"
#include "farm_state.h"
#include "farm_tables.h"
#include "field.h"
#include "harvest_sprites.h"
#include "items.h"
#include "main.h"
#include "pad.h"
#include "pad_vibration.h"
#include "picture_cells.h"
#include "player.h"
#include "screen_fade.h"
#include "sound.h"
#include "sound_request.h"
#include "swind.h"
#include "text.h"
#include "villager.h"

typedef s32 (*PageHandler)(s32);

/* A view of one FarmLivestock record with every field this debug page edits.
 */
typedef struct ItemStats {
    u16 kind;
    u8  pad02[0x16 - 0x02];
    u16 affection;
    u8  pad18[0x1A - 0x18];
    u16 date[3];
    u8  pad20[0x24 - 0x20];
    u16 action;
    u16 taskNo;
    u16 taskId;
    u16 taskTime;
    u16 interruptTime;
    u16 place[2];
    u8  pad32[0x34 - 0x32];
    s32 flags;
    s32 position[3];
    u8  pad44[0x48 - 0x44];
    s32 speed[3];
    u8  pad54[0x58 - 0x54];
    s32 destination[3];
    u8  pad64[0xAC - 0x64];
    u16 grazingTime;
    u16 daysSick;
    u16 growthDays;
    u16 pregnancy;
    u16 daysSinceShearing;
    u8  padB6[0xB8 - 0xB6];
} ItemStats;

/* The width of a debug row's value in bytes: 0 is a label-only row (no variable). */
enum SwindWidth {
    SWIND_WIDTH_NONE = 0,
    SWIND_WIDTH_BYTE = 1,
    SWIND_WIDTH_HALF = 2,
    SWIND_WIDTH_WORD = 4,
};

/* View of one FarmChicken record: the fields this editor exposes. */
typedef struct SwindRecord {
    u16 kind;
    u8 pad02[0x16 - 0x02];
    u16 affection;
    u8 pad18[0x1A - 0x18];
    u16 date[3];
    u8 pad20[0x24 - 0x20];
    u16 action;
    u16 taskNo;
    u16 taskId;
    u16 taskTime;
    u16 interruptTime;
    u8 bytes2E[0x34 - 0x2E];
    s32 flags;
    s32 position[3];
    u8 pad44[0x48 - 0x44];
    s32 speed[3];
    u8 pad54[0x58 - 0x54];
    s32 destination[3];
    u8 pad64[0xAC - 0x64];
    u16 grazingTime;
    u16 daysSick;
    u16 pregnancy;
    u8 padB2[0xB4 - 0xB2];
} SwindRecord;

/* View of one extra chicken record (SWIND_ANIMALS): the fields the poultry page exposes. */
typedef struct FarmExtraChicken {
    s16 kind;
    u8 pad02[0x16 - 0x02];
    s16 affection;
    u8 pad18[0x1A - 0x18];
    s16 date[3];
    u8 pad20[0x24 - 0x20];
    s16 action;
    s16 taskNo;
    s16 taskId;
    s16 taskTime;
    s16 interruptTime;
    u8 raw2E[4];
    u8 pad32[0x34 - 0x32];
    s32 flags;
    VECTOR position;
    VECTOR speed;
    VECTOR destination;
    u8 pad68[0xAC - 0x68];
    s16 grazingTime;
    s16 daysSick;
    s16 pregnancy;
    u8 padB2[0xB4 - 0xB2];
} FarmExtraChicken;

/* One editable line of a debug edit page: the value it edits, its range, its
 * width in bytes and its row kind, and the label shown. */
typedef struct SwindItem {
    void *data;
    s16 min;        /* the lowest value: 0 reads the value unsigned, below 0 signed */
    s32 limit;
    u8 size;        /* a SwindWidth: the value's width in bytes, or SWIND_WIDTH_NONE for a label-only line */
    s8 kind;        /* SWIND_ITEM_READ_ONLY: read-only; SWIND_ITEM_EDITABLE: editable */
    char *label;
} SwindItem;

/* One row of a flag-editor page: a bit mask applied to a 1-, 2- or 4-byte variable. */
typedef struct SwindFlagEntry {
    void *target;
    s32 unused4;
    s32 mask;
    u8 width;      /* 1, 2 or 4 bytes; 0: no variable (prints NULL) */
    s8 id;         /* SWIND_ITEM_READ_ONLY: not editable */
    u8 pad0E[2];
    char *label;
} SwindFlagEntry;

#define SELECTED_ITEM (((ItemStats *)game_state.animals.livestock)[swind_livestock_index])
#define RECORD (((SwindRecord *)game_state.animals.chickens)[swind_chicken_index])
#define FARM_RECORD (game_state.villagers[game_state.villagerCursor])

#define SWIND_ANIMALS ((FarmExtraChicken *)game_state.animals.extraChickens)

#define SWIND_COUNT(array) (sizeof(array) / sizeof((array)[0]))
#define SWIND_NO_VOICE -1
#define SWIND_ITEM_EDITABLE -1
#define SWIND_ITEM_READ_ONLY -2
#define SWIND_ROWS_PER_PAGE 9
#define SWIND_FIELD_ROWS 25
#define SWIND_FIELD_COLS 45
#define SWIND_BYTE_MAX 255U
#define SWIND_TOOL_COUNT 5
#define SWIND_TOOL_LAST 4
#define SWIND_PAGE_COUNT 18
#define SWIND_PAGE_LAST 17
#define SWIND_FADE_FRAMES 10
#define SWIND_MODEL_COUNT 256
#define SWIND_MIDI_SE_COUNT 77
#define SWIND_BGM_COUNT 37
#define SWIND_MGAME_SE_COUNT 16
#define SWIND_MGAME_SE_BASE 0x36
#define SWIND_XA_COUNT 0x1A
#define SWIND_PRICE_COUNT 379
#define SWIND_PRICE_LAST 378
#define SWIND_PRICE_FIELDS 4
#define SWIND_PRICE_FIELD_LAST 3
#define EVENT_FLAG_COUNT 1184
#define EVENT_FLAG_OFFSET 0xFB

/* Row labels of the record editors. Shift-JIS, padded with full-width spaces. */
#define LABEL_ANIMAL_NO           "\223\256\225\250\202m\202n\201@" /* 動物ＮＯ -- animal no. */
#define LABEL_AFFECTION           "\210\244\217\356\223x\201@\201@" /* 愛情度 -- affection */
#define LABEL_BIRTH_MONTH         "\222a\220\266\214\216\201@\201@" /* 誕生月 -- birth month */
#define LABEL_BIRTHDAY            "\222a\220\266\223\372\201@\201@" /* 誕生日 -- birthday */
#define LABEL_GRAZING_TIME        "\225\372\226q\216\236\212\324\201@" /* 放牧時間 -- grazing time */
#define LABEL_DAYS_SICK           "\225a\213C\212\372\212\324\201@" /* 病気期間 -- days sick */
#define LABEL_GROWTH_DAYS         "\220\254\222\267\212\372\212\324\201@" /* 成長期間 -- growth days */
#define LABEL_PREGNANCY           "\224D\220P\212\372\212\324\201@" /* 妊娠期間 -- pregnancy */
#define LABEL_DAYS_SINCE_SHEARING "\212\240\214\343\223\372\220\224\201@" /* 刈後日数 -- days since shearing */
#define LABEL_ACTION              "\215s\223\256\201@\201@\201@" /* 行動 -- action */
#define LABEL_ITEM                "\203A\203C\203e\203\200\201@" /* アイテム -- item */
#define LABEL_TASK_NO             "\217\210\227\235\202m\202n\201@" /* 処理ＮＯ -- task no. */
#define LABEL_TASK_ID             "\217\210\227\235\202h\202c\201@" /* 処理ＩＤ -- task id */
#define LABEL_TASK_TIME           "\217\210\227\235\216\236\212\324\201@" /* 処理時間 -- task time */
#define LABEL_INTERRUPT_TIME      "\212\204\215\236\202\335\216\236\212\324" /* 割込み時間 -- interrupt time */
#define LABEL_LOCATION            "\214\273\215\335\222n\201@\201@" /* 現在地 -- location */
#define LABEL_FLAGS               "\203t\203\211\203O\201@\201@" /* フラグ -- flags */
#define LABEL_X                   "\202w\215\300\225W\201@\201@" /* Ｘ座標 -- x */
#define LABEL_Y                   "\202x\215\300\225W\201@\201@" /* Ｙ座標 -- y */
#define LABEL_Z                   "\202y\215\300\225W\201@\201@" /* Ｚ座標 -- z */
#define LABEL_SPEED_X             "\203X\203s\201[\203h\202w" /* スピードＸ -- speed x */
#define LABEL_SPEED_Y             "\203X\203s\201[\203h\202x" /* スピードＹ -- speed y */
#define LABEL_SPEED_Z             "\203X\203s\201[\203h\202y" /* スピードＺ -- speed z */
#define LABEL_DESTINATION_X       "\226\332\223I\222n\202w\201@" /* 目的地Ｘ -- destination x */
#define LABEL_DESTINATION_Y       "\226\332\223I\222n\202x\201@" /* 目的地Ｙ -- destination y */
#define LABEL_DESTINATION_Z       "\226\332\223I\222n\202y\201@" /* 目的地Ｚ -- destination z */

/* One row of an editor page (swind_item_list, swind_flag_list): the cursor, the
 * row's mark, its label and its value. */
#define ROW_LABEL_FORMAT "%s\201@%s%s\201@\201\204\202m\202t\202k\202k\n" /* "%s　%s%s　＞ＮＵＬＬ\n": a row with no value */
#define ROW_VALUE_FORMAT "%s\201@%s%s\201@\201\204%d\n" /* "%s　%s%s　＞%d\n" */
#define SWIND_CURSOR_ON  "\201\204" /* ＞ */
#define SWIND_CURSOR_OFF "\201@"    /* 　 */
#define ROW_CURSOR(i) ((i) == swind_cursor ? SWIND_CURSOR_ON : SWIND_CURSOR_OFF)

/* Page functions. */

static void swind_reset(void);
static void swind_state_reset_if_zero(void);
static s32 swind_stub_1(void);
static void swind_stub_2(void);
static void swind_switch_to_seq2(void);
static void swind_switch_to_seq1(void);
static void swind_page_begin(void);
static s32 swind_main_page(s32 state);
static void swind_model_select(void);
static void swind_set_debug_flag_bit(u32 bit, u32 value);
static void swind_font_setup(void);
static s32 swind_main_data_page(s32 value);
static s32 swind_player_page(s32 value);
static s32 swind_run_program_page(s32 value);
static s32 swind_pet_page(s32 value);
static s32 swind_livestock_record_page(s32 value);
static s32 swind_chicken_record_page(s32 arg);
static s32 swind_poultry_record_page(s32 value);
static s32 swind_villager_record_page(s32 arg);
static s32 swind_event_flag_page(s32 page);
static s32 swind_farm_record_list(void);
static s32 swind_harvest_sprite_page(s32 value);
static s32 swind_flag_page(s32 value);
static s32 swind_day_count0_page(s32 page);
static s32 swind_day_count1_page(s32 page);
static s32 swind_day_values_page(s32 page);
static s32 swind_pad_step(s32 value);
static s32 swind_price_page(s32 page);
static s32 swind_tool_stamina_page(s32 page);
static void swind_item_list(SwindItem *items, s32 count);
static void swind_step_value(void *value, s32 size, s32 max, s32 min, s32 delta);
static void swind_flag_list(SwindFlagEntry *entries, s32 count);

/* Scalars and page state, declared ahead of the functions that use them. */

static s32 swind_closable;
static s32 swind_page;
static s32 swind_page_state;
static s32 swind_main_cursor;
static s32 swind_midi_se_no;
static s32 swind_mgame_se_no;
static s32 swind_bgm_no;
static s32 swind_xa_no;
static s32 swind_sound_voice;
static s32 swind_vibration_no;
static s16 swind_get_item_no;
static s32 swind_rival_mode;
static s16 swind_time_advance;
/* The commands of swind_run_program_page, the row kinds of swind_run_program_items
 * (SWIND_ROW_NONE is also the quit row, and the state of "no row chosen"). */
enum SwindRow {
    SWIND_ROW_NONE = -1,
    SWIND_ROW_VIBRATION_TEST = 0,
    SWIND_ROW_VIBRATION_STOP = 1,
    SWIND_ROW_ADD_COW = 2,
    SWIND_ROW_ADD_YOUNG_COW = 3,
    SWIND_ROW_ADD_CALF = 4,
    SWIND_ROW_ADD_SHEEP = 5,
    SWIND_ROW_ADD_LAMB = 6,
    SWIND_ROW_ADD_CHICKEN = 7,
    SWIND_ROW_ADD_CHICK = 8,
    SWIND_ROW_ADD_FOAL = 9,
    SWIND_ROW_ADD_HORSE = 10,
    SWIND_ROW_CAMERA_FIXED = 11,
    SWIND_ROW_CAMERA_PLAYER = 12,
    SWIND_ROW_SPECIAL_0 = 13,
    SWIND_ROW_SPECIAL_1 = 14,
    SWIND_ROW_SPECIAL_2 = 15,
    SWIND_ROW_GET_ITEM = 16,
    SWIND_ROW_SPRITE_SPECIAL_0 = 17,
    SWIND_ROW_SPRITE_SPECIAL_1 = 18,
    SWIND_ROW_SPRITE_SPECIAL_2 = 19,
    SWIND_ROW_ENGAGE_KAREN = 20,
    SWIND_ROW_ENGAGE_ANN = 21,
    SWIND_ROW_ENGAGE_ELLI = 22,
    SWIND_ROW_ENGAGE_MARY = 23,
    SWIND_ROW_ENGAGE_POPURI = 24,
    SWIND_ROW_MARRY_BRIDE = 25,
    SWIND_ROW_RIVAL_ENGAGEMENT = 26,
    SWIND_ROW_ADVANCE_TIME = 27,
};

/* The girl whose engagement SWIND_ROW_RIVAL_ENGAGEMENT sets up (swind_rival_mode). */
enum SwindRival {
    SWIND_RIVAL_KAREN = 0,
    SWIND_RIVAL_ANN = 1,
    SWIND_RIVAL_ELLI = 2,
    SWIND_RIVAL_MARY = 3,
    SWIND_RIVAL_POPURI = 4,
};

/* The window's own steps (swind_state): open the window, load the font and fade
 * in, run the pages, then close and hand the new settings back to the farm. */
enum SwindWindowState {
    SWIND_STATE_OPEN = 0,
    SWIND_STATE_FONT = 2,
    SWIND_STATE_RUN = 3,
    SWIND_STATE_CLOSE = 4,
};

static s32 swind_state;
static s32 swind_row_kind;

/* The main page's row values, swind_main_page's `on[]`: "X" off, "O" on. */
static char *swind_onoff_text[2] = { "X", "O" };
/* The main page's rows, in display order. */
static char *swind_main_rows[15] = {
    "CAMERA CONTROL",
    "MUTEKI",
    "ENTER ARTR",
    "MYPOS PUT",
    "SCR LIMIT",
    "MODEL CHANGE",
    "MIDI SE TEST",
    "BGM TEST",
    "MGAME SE TEST",
    "XA PLAY TEST",
    "DOOR SCHE DIS",
    "3D PUT DIS",
    "PEOPLE PRO DIS",
    "HISPEED TIME",
    "PUT TIME",
};
static char **swind_main_rows_ptr = swind_main_rows;

/* The debug window's row count. */
static u32 swind_main_row_count = 15;
static s16 swind_unused_80133FBC = 0;
static void *swind_page_table[SWIND_PAGE_COUNT] = {
    swind_main_page, swind_main_data_page, swind_player_page, swind_run_program_page, swind_flag_page, swind_villager_record_page,
    swind_event_flag_page, swind_farm_record_list, swind_pet_page, swind_livestock_record_page, swind_chicken_record_page, swind_harvest_sprite_page,
    swind_poultry_record_page, swind_day_count0_page, swind_day_count1_page, swind_day_values_page, swind_price_page, swind_tool_stamina_page,
};

/* The change flag: the close step publishes it as EVENT_FLAG_SWIND_CHANGED. */
static s32 swind_changed = 0;
/* The VRAM rectangle swind_window_tick saves under the window and restores. */
static RECT swind_save_rect = {448, 0, 64, 256};
/* The cursor: the row the page highlights. Each page keeps it between frames. */
static s32 swind_cursor = 0;

/* The model the window shows (farm_area.mode.area). swind_model_select steps it
 * with PAD_RIGHT and PAD_LEFT when triggered; going left from 0 it goes to 256. */
static s32 swind_model_index;
/* The handler of the page swind_page selects, set by swind_window_tick each frame. */
static PageHandler swind_page_fn;

/* Unused. */
static void swind_reset(void) {
    swind_changed = 0;
    swind_state = SWIND_STATE_OPEN;
}
/* Unused. */
static void swind_state_reset_if_zero(void) {
    if (swind_state == SWIND_STATE_OPEN) {
        swind_state = SWIND_STATE_OPEN;
    }
}
/* Unused stub. */
static s32 swind_stub_1(void) {
    return 0;
}
/* Unused stub. */
static void swind_stub_2(void) {
}

/* One frame of the window, dispatched on swind_state. */
void swind_window_tick(void) {
    /* Save (0) or restore (1) the screen area behind the window. */
    void swind_screen_area_copy(s32 restore) {
        u_long *buffer = (u_long *)STAGING_BUFFER;

        if (restore == 0) {
            StoreImage(&swind_save_rect, buffer);
        } else {
            LoadImage(&swind_save_rect, buffer);
        }
        DrawSync(0);
    }

    switch (swind_state) {
    case SWIND_STATE_OPEN:
        swind_state = SWIND_STATE_FONT;
        swind_closable = 1;
        swind_page = 0;
        swind_page_state = 0;
        swind_unused_80133FBC = 0;
        swind_page_fn = swind_page_table[0];
        swind_screen_area_copy(0);
        break;
    case SWIND_STATE_FONT:
        swind_state = SWIND_STATE_RUN;
        KanjiFntClose();
        KanjiFntOpen(8, 16, 320, 240, 448, 0, 320, 255, 0, 255);
        FntFlush(-1);
        swind_font_setup();
        fade_start_in(SWIND_FADE_FRAMES, 0, 2);
        sound_fade_in(SWIND_FADE_FRAMES);
        break;
    case SWIND_STATE_RUN:
        if (input_pad1->pressed & PAD_R2) {
            swind_page_state = 0;
            swind_row_kind = SWIND_ROW_NONE;
            swind_page++;
            swind_page = ((u32)swind_page > SWIND_PAGE_LAST) ? SWIND_PAGE_LAST : (u32)swind_page;
            sound_play(4);
        } else if (input_pad1->pressed & PAD_L2) {
            swind_page_state = 0;
            swind_row_kind = SWIND_ROW_NONE;
            swind_page--;
            swind_page = (swind_page < 0) ? 0 : swind_page;
            sound_play(4);
        }
        swind_page_fn = swind_page_table[swind_page];
        swind_page_state = swind_page_fn(swind_page_state);
        if (fade_is_settled() == 1) {
            if (swind_closable == 1) {
                if (input_pad1->pressed & PAD_START) {
                    fade_start_out(SWIND_FADE_FRAMES, 0, 2);
                    sound_fade_out(SWIND_FADE_FRAMES);
                    swind_closable = 0;
                }
            } else {
                swind_state = SWIND_STATE_CLOSE;
            }
        }
        KanjiFntFlush(0);
        break;
    case SWIND_STATE_CLOSE:
        swind_screen_area_copy(1);
        event_flag_set(EVENT_FLAG_SWIND_MODEL_CHANGED, 0);
        if (swind_model_index != farm_area.mode.area) {
            event_flag_set(EVENT_FLAG_SWIND_MODEL_CHANGED, 1);
            farm_swind_size = swind_model_index;
        }
        swind_state = SWIND_STATE_OPEN;
        fade_start_in(SWIND_FADE_FRAMES, 0, 2);
        sound_fade_in(SWIND_FADE_FRAMES);
        game_state.globalFlags &= ~GLOBAL_FLAG_PAUSED;
        main_request_scene(SCENE_FARM);
        event_flag_set(EVENT_FLAG_SWIND_CHANGED, 0);
        if (swind_changed != 0) {
            event_flag_set(EVENT_FLAG_SWIND_CHANGED, 1);
        }
        break;
    }
    cd_audio_fade_tick();
}

/* Entering the mini-game SE row: releases sound slots 1 and 2, then requests sound 999. */
static void swind_switch_to_seq2(void) {
    sound_release_from(2, 0);
    sound_release_from(1, 0);
    sound_request(999, 0);
}
/* Leaving the mini-game SE row: releases sound slots 1 and 2, then requests sound 1000. */
static void swind_switch_to_seq1(void) {
    sound_release_from(2, 0);
    sound_release_from(1, 0);
    sound_request(1000, 0);
}
/* Page-top hook: nothing to set up yet. */
static void swind_page_begin(void) {
}

/* The rows of the main page, in the order of swind_main_rows. */
enum SwindMainRow {
    SWIND_MAIN_CAMERA_CONTROL = 0,
    SWIND_MAIN_MUTEKI = 1,
    SWIND_MAIN_ENTER_ARTR = 2,
    SWIND_MAIN_MYPOS_PUT = 3,
    SWIND_MAIN_SCR_LIMIT = 4,
    SWIND_MAIN_MODEL_CHANGE = 5,
    SWIND_MAIN_MIDI_SE_TEST = 6,
    SWIND_MAIN_BGM_TEST = 7,
    SWIND_MAIN_MGAME_SE_TEST = 8,
    SWIND_MAIN_XA_PLAY_TEST = 9,
    SWIND_MAIN_DOOR_SCHE_DIS = 10,
    SWIND_MAIN_PUT_3D_DIS = 11,
    SWIND_MAIN_PEOPLE_PRO_DIS = 12,
    SWIND_MAIN_HISPEED_TIME = 13,
    SWIND_MAIN_PUT_TIME = 14,
};

/* The main page's steps (its `state` argument). */
enum SwindPageState {
    SWIND_PAGE_INIT = 0,
    SWIND_PAGE_ROWS = 1,
    SWIND_PAGE_BGM_RESTART = 999,
    SWIND_PAGE_SOUND_WAIT = 1000,
};

/* The debug window's state machine. State 0 resets it, state 1 draws the rows and
 * handles the pad. 999 restarts the BGM once the sound fader is ready; 1000 waits for it.
 * Returns the next state. */
static s32 swind_main_page(s32 state) {
    s32 on[15];
    u32 i;

    swind_page_begin();
    for (i = 0; i < 15; i++) {
        on[i] = 0;
    }
    if (game_state.debugFlags & DEBUG_FLAG_CAMERA_CONTROL) {
        on[0] = 1;
    }
    if (game_state.debugFlags & DEBUG_FLAG_MUTEKI) {
        on[1] = 1;
    }
    if (!(game_state.debugFlags & DEBUG_FLAG_ENTER_ARTR)) {
        on[2] = 1;
    }
    if (game_state.debugFlags & DEBUG_FLAG_MYPOS_PUT) {
        on[3] = 1;
    }
    if (game_state.debugFlags & DEBUG_FLAG_SCR_LIMIT) {
        on[4] = 1;
    }
    if (game_state.debugFlags & DEBUG_FLAG_DOOR_SCHEDULE) {
        on[10] = 1;
    }
    if (game_state.globalFlags & GLOBAL_FLAG_TRANSITION) {
        on[11] = 1;
    }
    if (game_state.debugFlags & DEBUG_FLAG_PEOPLE_PRO) {
        on[12] = 1;
    }
    if (game_state.debugFlags & DEBUG_FLAG_HISPEED_TIME) {
        on[13] = 1;
    }
    if (game_state.debugFlags & DEBUG_FLAG_PUT_TIME) {
        on[14] = 1;
    }

    switch (state) {
    case SWIND_PAGE_INIT:
        state = SWIND_PAGE_ROWS;
        swind_main_cursor = SWIND_MAIN_CAMERA_CONTROL;
        swind_midi_se_no = 0;
        swind_mgame_se_no = 0;
        swind_bgm_no = 0;
        swind_xa_no = 0;
        swind_sound_voice = SWIND_NO_VOICE;
        swind_model_index = farm_area.mode.area;
        break;
    case SWIND_PAGE_ROWS:
        FntPrint("\n\n --- DEBUG WINDOW ---\n");
        FntPrint("Model No %3d\n", swind_model_index);
        FntPrint(" MIDI SE NO %d\n", swind_midi_se_no);
        FntPrint(" MGAME SE NO %d\n", swind_mgame_se_no);
        FntPrint(" BGM NO %d\n", swind_bgm_no);
        FntPrint(" XA NO %d\n", swind_xa_no);
        if (input_pad1->triggered & PAD_UP) {
            if (swind_main_cursor == SWIND_MAIN_MGAME_SE_TEST) {
                swind_switch_to_seq1();
                state = SWIND_PAGE_SOUND_WAIT;
            }
            if (swind_main_cursor == SWIND_MAIN_CAMERA_CONTROL) {
                swind_main_cursor = swind_main_row_count - 1;
            } else {
                swind_main_cursor--;
            }
            if (swind_main_cursor == SWIND_MAIN_MGAME_SE_TEST) {
                swind_switch_to_seq2();
                state = SWIND_PAGE_SOUND_WAIT;
            }
            break;
        }
        if (input_pad1->triggered & PAD_DOWN) {
            if (swind_main_cursor == SWIND_MAIN_MGAME_SE_TEST) {
                swind_switch_to_seq1();
                state = SWIND_PAGE_SOUND_WAIT;
            }
            if (swind_main_cursor == swind_main_row_count - 1) {
                swind_main_cursor = SWIND_MAIN_CAMERA_CONTROL;
            } else {
                swind_main_cursor++;
            }
            if (swind_main_cursor == SWIND_MAIN_MGAME_SE_TEST) {
                swind_switch_to_seq2();
                state = SWIND_PAGE_SOUND_WAIT;
            }
            break;
        }
        for (i = 0; i < swind_main_row_count; i++) {
            if (i == swind_main_cursor) {
                FntPrint(" *");
            } else {
                FntPrint("  ");
            }
            FntPrint(" %s %s\n", swind_main_rows_ptr[i], swind_onoff_text[on[i]]);
        }
        switch (swind_main_cursor) {
        case SWIND_MAIN_DOOR_SCHE_DIS:
            if (input_pad1->pressed & PAD_RIGHT) {
                swind_set_debug_flag_bit(0x1A, 1);
            }
            if (input_pad1->pressed & PAD_LEFT) {
                swind_set_debug_flag_bit(0x1A, 0);
            }
            break;
        case SWIND_MAIN_PUT_3D_DIS:
            if (input_pad1->pressed & PAD_RIGHT) {
                game_state.globalFlags |= GLOBAL_FLAG_TRANSITION;
            }
            if (input_pad1->pressed & PAD_LEFT) {
                game_state.globalFlags &= ~GLOBAL_FLAG_TRANSITION;
            }
            break;
        case SWIND_MAIN_PEOPLE_PRO_DIS:
            if (input_pad1->pressed & PAD_RIGHT) {
                game_state.debugFlags |= DEBUG_FLAG_PEOPLE_PRO;
            }
            if (input_pad1->pressed & PAD_LEFT) {
                game_state.debugFlags &= ~DEBUG_FLAG_PEOPLE_PRO;
            }
            break;
        case SWIND_MAIN_HISPEED_TIME:
            if (input_pad1->pressed & PAD_RIGHT) {
                game_state.debugFlags |= DEBUG_FLAG_HISPEED_TIME;
            }
            if (input_pad1->pressed & PAD_LEFT) {
                game_state.debugFlags &= ~DEBUG_FLAG_HISPEED_TIME;
            }
            break;
        case SWIND_MAIN_PUT_TIME:
            if (input_pad1->pressed & PAD_RIGHT) {
                game_state.debugFlags |= DEBUG_FLAG_PUT_TIME;
            }
            if (input_pad1->pressed & PAD_LEFT) {
                game_state.debugFlags &= ~DEBUG_FLAG_PUT_TIME;
            }
            break;
        default: /* rows 0..4: the flag bits */
            if (input_pad1->pressed & PAD_RIGHT) {
                swind_set_debug_flag_bit(0x1F - swind_main_cursor, 1);
            }
            if (input_pad1->pressed & PAD_LEFT) {
                swind_set_debug_flag_bit(0x1F - swind_main_cursor, 0);
            }
            break;
        case SWIND_MAIN_MODEL_CHANGE:
            swind_model_select();
            break;
        case SWIND_MAIN_MIDI_SE_TEST:
            if (input_pad1->pressed & PAD_CIRCLE) {
                if (swind_sound_voice != SWIND_NO_VOICE) {
                    sound_release_voice(swind_sound_voice);
                }
                swind_sound_voice = sound_play(swind_midi_se_no);
                break;
            }
            if (input_pad1->triggered & PAD_RIGHT) {
                swind_midi_se_no++;
                if (swind_midi_se_no >= SWIND_MIDI_SE_COUNT) {
                    swind_midi_se_no = 0;
                }
            }
            if (input_pad1->triggered & PAD_LEFT) {
                swind_midi_se_no--;
                if (swind_midi_se_no < 0) {
                    swind_midi_se_no = SWIND_MIDI_SE_COUNT - 1;
                }
            }
            break;
        case SWIND_MAIN_BGM_TEST:
            if (input_pad1->pressed & PAD_CIRCLE) {
                state = SWIND_PAGE_BGM_RESTART;
                sound_seq_stop_first(0);
                sound_update();
                sound_release_from(2, 0);
                sound_request((s16)(swind_bgm_no + 1), 0);
                break;
            }
            if (input_pad1->triggered & PAD_RIGHT) {
                swind_bgm_no++;
                if (swind_bgm_no >= SWIND_BGM_COUNT) {
                    swind_bgm_no = 0;
                }
            }
            if (input_pad1->triggered & PAD_LEFT) {
                swind_bgm_no--;
                if (swind_bgm_no < 0) {
                    swind_bgm_no = 0x23;
                }
            }
            break;
        case SWIND_MAIN_MGAME_SE_TEST:
            if (input_pad1->pressed & PAD_CIRCLE) {
                if (swind_sound_voice != SWIND_NO_VOICE) {
                    sound_release_voice(swind_sound_voice);
                }
                swind_sound_voice = sound_play(swind_mgame_se_no + SWIND_MGAME_SE_BASE);
                break;
            }
            if (input_pad1->triggered & PAD_RIGHT) {
                swind_mgame_se_no++;
                if (swind_mgame_se_no >= SWIND_MGAME_SE_COUNT) {
                    swind_mgame_se_no = 0;
                }
            }
            if (input_pad1->triggered & PAD_LEFT) {
                swind_mgame_se_no--;
                if (swind_mgame_se_no < 0) {
                    swind_mgame_se_no = SWIND_MGAME_SE_COUNT - 1;
                }
            }
            break;
        case SWIND_MAIN_XA_PLAY_TEST:
            if (input_pad1->pressed & PAD_CIRCLE) {
                cd_audio_play_track(swind_xa_no);
                break;
            }
            if (input_pad1->triggered & PAD_RIGHT) {
                swind_xa_no++;
                if (swind_xa_no >= SWIND_XA_COUNT) {
                    swind_xa_no = 0;
                }
            }
            if (input_pad1->triggered & PAD_LEFT) {
                swind_xa_no--;
                if (swind_xa_no < 0) {
                    swind_xa_no = SWIND_XA_COUNT - 1;
                }
            }
            break;
        }
        break;
    case SWIND_PAGE_BGM_RESTART:
        if (sound_fader_ready() != 0) {
            sound_update();
            state = SWIND_PAGE_ROWS;
            sound_seq_start_reverb(0, 0, swind_bgm_no, 127);
        }
        break;
    case SWIND_PAGE_SOUND_WAIT:
        if (sound_fader_ready() != 0) {
            sound_update();
            state = SWIND_PAGE_ROWS;
        }
        break;
    }
    return state;
}

static void swind_model_select(void) {
    if (input_pad1->triggered & PAD_RIGHT) {
        swind_model_index++;
        if (swind_model_index == SWIND_MODEL_COUNT) {
            swind_model_index = 0;
        }
    }

    if (input_pad1->triggered & PAD_LEFT) {
        if (swind_model_index == 0) {
            swind_model_index = SWIND_MODEL_COUNT;
        } else {
            swind_model_index--;
        }
    }
}

/* Sets (value 1) or clears (value 0) one bit of game_state.debugFlags. */
static void swind_set_debug_flag_bit(u32 bit, u32 value) {
    game_state.debugFlags = (game_state.debugFlags & ~(1 << bit)) | (value << bit);
}
/* Loads the debug font and sets the text origin of the window. */
static void swind_font_setup(void) {
    s32 font;
    FntLoad(0x2C0, 0x100);
    font = FntOpen(24, 0, 256, 200, 0, 512);
    SetDumpFnt(font);
    text_set_origin(24, 24);
}

/* Main data page: the calendar and the clock, the
 * weather, the farm buildings, the stores, the wife and the child. Each row:
 * the value, signed (0: read unsigned), its maximum, its width in bytes, -1
 * editable or -2 read-only, and its label (swind_item_list). */
static SwindItem swind_main_data_items[22] = {
    { &game_state.progress.clock.date.year,    0, 99, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\224N\220\224\201@\201@\201@\201@\201@\201@\201@" }, /* 年数 -- year */
    { &game_state.progress.clock.date.season,  0, 3, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, "\213G\220\337\201@\201@\201@\201@\201@\201@\201@" }, /* 季節 -- season */
    { &game_state.progress.clock.date.day,     0, 29, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\223\372\201@\201@\201@\201@\201@\201@\201@\201@" }, /* 日 -- day */
    { &game_state.progress.clock.date.weekday, 0, 6, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\227j\223\372\201@\201@\201@\201@\201@\201@\201@" }, /* 曜日 -- weekday */
    { &game_state.progress.clock.time.hm.hour,    0, 23, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\216\236\212\324\201@\201@\201@\201@\201@\201@\201@" }, /* 時間 -- hour */
    { &game_state.progress.clock.time.hm.minute,  0, 59, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\225\252\201@\201@\201@\201@\201@\201@\201@\201@" }, /* 分 -- minute */
    { &game_state.world.weather,            0, 4, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\215\241\223\372\201i\220\260\211J\220\341\227\222\220\201\201j" }, /* 今日（晴雨雪嵐吹） -- today (sun rain snow storm blizzard) */
    { &game_state.world.weatherTomorrow,    0, 4, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\226\276\223\372\201i\220\260\211J\220\341\227\222\220\201\201j" }, /* 明日（晴雨雪嵐吹） -- tomorrow (sun rain snow storm blizzard) */
    { &game_state.progress.estate.houseLevel,           0, 2, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\211\306\202\314\202k\202u\201@\201@\201@\201@\201@" }, /* 家のＬＶ -- house level */
    { &game_state.progress.estate.barnLevel,           0, 3, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\211\306\222{\217\254\211\256\202\314\202k\202u\201@\201@" }, /* 家畜小屋のＬＶ -- barn level */
    { &game_state.progress.estate.coopLevel,           0, 2, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\214{\217\254\211\256\202\314\202k\202u\201@\201@\201@" }, /* 鶏小屋のＬＶ -- chicken coop level */
    { &game_state.progress.estate.greenhouseLevel,          0, 1, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\203n\203E\203X\202\314\202k\202u\201@\201@\201@" }, /* ハウスのＬＶ -- greenhouse level */
    { &game_state.progress.estate.utensils,           0, 255, SWIND_WIDTH_BYTE, SWIND_ITEM_EDITABLE, "\222\262\227\235\223\271\213\357\201i\202w\201j\201@\201@" }, /* 調理道具（Ｘ） -- kitchen tools (hex) */
    { &game_state.progress.estate.lumber,          0, 65535, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\216\221\215\336\216c\202\350\201@\201@\201@\201@\201@" }, /* 資材残り -- lumber left */
    { &game_state.progress.estate.fodder,          0, 65535, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\216\224\227t\216c\202\350\201@\201@\201@\201@\201@" }, /* 飼葉残り -- fodder left */
    { &game_state.progress.estate.fishFood,          0, 65535, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\213\233\202\314\211a\216c\202\350\201@\201@\201@\201@" }, /* 魚の餌残り -- fish food left */
    { &game_state.progress.estate.chickenFeed,          0, 65535, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\214{\202\314\211a\216c\202\350\201@\201@\201@\201@" }, /* 鶏の餌残り -- chicken feed left */
    { &game_state.family.wifeIndex,          0, 4, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\211\305\202\314\216\355\227\336\201@\201@\201@\201@\201@" }, /* 嫁の種類 -- which wife */
    { &game_state.family.wifeIgnoredDays,        0, 200, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\211\305\202\314\226\263\216\213\201@\201@\201@\201@\201@" }, /* 嫁の無視 -- wife ignored */
    { &game_state.family.childGrowthDays,          0, 255, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\216q\213\237\202\314\220\254\222\267\223\372\220\224\201@\201@" }, /* 子供の成長日数 -- child's growth days */
    { &game_state.family.childLevel,          0, 3, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\216q\213\237\202\314\203\214\203x\203\213\201@\201@\201@" }, /* 子供のレベル -- child's level */
    { &game_state.family.pregnancyDays,            0, 360, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\224D\220P\212\372\212\324\201@\201@\201@\201@\201@" }, /* 妊娠期間 -- pregnancy */
};

static s32 swind_main_data_page(s32 value) {
    s32 result;

    if (fade_is_settled() != 0) {
        KanjiFntPrint("\n\201@\201@\201@\201@\201@\203\201\203C\203\223\203f\201[\203^\212\326\214W\n\n"); /* メインデータ関係 -- main data */
        swind_cursor = value;
        swind_item_list(swind_main_data_items, SWIND_COUNT(swind_main_data_items));
        result = swind_cursor;
    } else {
        result = value;
    }
    return result;
}
/* Player data page: the player, the villagers' speeds, the clock rate and the item flag. */
static SwindItem swind_player_items[23] = {
    { &game_state.player.stamina,       0, 255, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\221\314\227\315\201@\201@\201@\201@" }, /* 体力 -- stamina */
    { &game_state.player.maxStamina,    0, 255, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\215\305\221\345\221\314\227\315\201@\201@" }, /* 最大体力 -- max stamina */
    { &game_state.player.fatigue,       0, 100, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\224\346\230J\223x\201@\201@\201@" }, /* 疲労度 -- fatigue */
    { &game_state.player.rucksackLevel, 0, 2, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\203\212\203\205\203b\203N\202k\202u" }, /* リュックＬＶ -- rucksack level */
    { &game_state.player.talkVillager,  -1, 40, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\230b\202\265\202\304\202\351\220l\201@" }, /* 話してる人 -- person talked to */
    { &game_state.player.talkAnimal,    0, 10, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\230b\202\265\202\304\202\351\223\256\225\250" }, /* 話してる動物 -- animal talked to */
    { &game_state.player.happiness,     0, 65535, SWIND_WIDTH_WORD, SWIND_ITEM_EDITABLE, "\215K\202\271\216w\220\224\201@\201@" }, /* 幸せ指数 -- happiness */
    { &game_state.player.gold,          0, FARM_MONEY_MAX, SWIND_WIDTH_WORD, SWIND_ITEM_EDITABLE, "\202\250\213\340\201@\201@\201@\201@" }, /* お金 -- money */
    { &game_state.player.medals,        0, 9999, SWIND_WIDTH_WORD, SWIND_ITEM_EDITABLE, "\203R\203C\203\223\201@\201@\201@" }, /* コイン -- coins */
    { &game_state.player.birthday.season,   0, 3, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\222a\220\266\214\216\201@\201@\201@" }, /* 誕生月 -- birth month */
    { &game_state.player.birthday.day,      0, 29, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\222a\220\266\223\372\201@\201@\201@" }, /* 誕生日 -- birthday */
    { &game_state.player.toolExp[0],    0, 65535, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\212\231\214o\214\261\222l\201@\201@" }, /* 鎌経験値 -- sickle experience */
    { &game_state.player.toolExp[1],    0, 65535, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\214L\214o\214\261\222l\201@\201@" }, /* 鍬経験値 -- hoe experience */
    { &game_state.player.toolExp[2],    0, 65535, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\225\200\214o\214\261\222l\201@\201@" }, /* 斧経験値 -- axe experience */
    { &game_state.player.toolExp[3],    0, 65535, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\222\306\214o\214\261\222l\201@\201@" }, /* 槌経験値 -- hammer experience */
    { &game_state.player.toolExp[4],    0, 65535, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\224@\211J\230I\214o\214\261\222l" }, /* 如雨露経験値 -- watering can experience */
    { &game_state.player.chickToy,      0, 1, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\202\320\202\346\202\261\202\314\202\250\202\340\202\277\202\341" }, /* ひよこのおもちゃ -- chick's toy */
    { &game_state.player.cowReins,      0, 1, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\202\244\202\265\202\314\202\275\202\303\202\310" }, /* うしのたづな -- cow's reins */
    { &villager_speed_outdoors,                      0, 9999999, SWIND_WIDTH_WORD, SWIND_ITEM_EDITABLE, "\217Z\226\257\212O\221\254\223x" }, /* 住民外速度 -- villager speed outdoors */
    { &villager_speed_indoors,                      0, 9999999, SWIND_WIDTH_WORD, SWIND_ITEM_EDITABLE, "\217Z\226\257\222\206\221\254\223x" }, /* 住民中速度 -- villager speed indoors */
    { &position_correction,                      0, 256, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\215\300\225W\225\342\220\263\222l" }, /* 座標補正値 -- position correction */
    { &one_minute,                      0, 99999, SWIND_WIDTH_WORD, SWIND_ITEM_EDITABLE, "\202P\225\252" }, /* １分 -- one minute */
    { &swind_changed,                      0, 1, SWIND_WIDTH_WORD, SWIND_ITEM_EDITABLE, "\203A\203C\203e\203\200\202l\202`\202w" }, /* アイテムＭＡＸ -- item max */
};

static s32 swind_player_page(s32 value) {
    s32 result;

    if (fade_is_settled() != 0) {
        KanjiFntPrint("\n\201@\201@\201@\201@\201@\216\345\220l\214\366\203f\201[\203^\212\326\214W\n\n"); /* 主人公データ関係 -- player data */
        swind_cursor = value;
        swind_item_list(swind_player_items, SWIND_COUNT(swind_player_items));
        result = swind_cursor;
    } else {
        result = value;
    }
    return result;
}

/* "Running the program" page. Rows with no value are commands; the four rows
 * with a value are their arguments. */
static SwindItem swind_run_program_items[29] = {
    { NULL,        0, 0, SWIND_WIDTH_NONE, SWIND_ROW_NONE, "\203v\203\215\203O\203\211\203\200\217I\227\271" }, /* プログラム終了 -- quit the program */
    { &swind_vibration_no, 0, 10, SWIND_WIDTH_WORD, SWIND_ROW_VIBRATION_TEST, "\220U\223\256\203e\203X\203g\201@\201@" }, /* 振動テスト -- vibration test */
    { NULL,        0, 0, SWIND_WIDTH_NONE, SWIND_ROW_VIBRATION_STOP, "\220U\223\256\222\342\216~\201@\201@\201@" }, /* 振動停止 -- stop vibration */
    { NULL,        0, 0, SWIND_WIDTH_NONE, SWIND_ROW_ADD_COW, "\221\345\213\215\222\307\211\301\201@\201@\201@" }, /* 大牛追加 -- add a cow */
    { NULL,        0, 0, SWIND_WIDTH_NONE, SWIND_ROW_ADD_YOUNG_COW, "\222\206\213\215\222\307\211\301\201@\201@\201@" }, /* 中牛追加 -- add a young cow */
    { NULL,        0, 0, SWIND_WIDTH_NONE, SWIND_ROW_ADD_CALF, "\217\254\213\215\222\307\211\301\201@\201@\201@" }, /* 小牛追加 -- add a calf */
    { NULL,        0, 0, SWIND_WIDTH_NONE, SWIND_ROW_ADD_SHEEP, "\220e\227r\222\307\211\301\201@\201@\201@" }, /* 親羊追加 -- add a sheep */
    { NULL,        0, 0, SWIND_WIDTH_NONE, SWIND_ROW_ADD_LAMB, "\217\254\227r\222\307\211\301\201@\201@\201@" }, /* 小羊追加 -- add a lamb */
    { NULL,        0, 0, SWIND_WIDTH_NONE, SWIND_ROW_ADD_CHICKEN, "\214{\222\307\211\301\201@\201@\201@\201@" }, /* 鶏追加 -- add a chicken */
    { NULL,        0, 0, SWIND_WIDTH_NONE, SWIND_ROW_ADD_CHICK, "\202\320\202\346\202\261\222\307\211\301\201@\201@" }, /* ひよこ追加 -- add a chick */
    { NULL,        0, 0, SWIND_WIDTH_NONE, SWIND_ROW_ADD_FOAL, "\217\254\224n\222\307\211\301\201@\201@\201@" }, /* 小馬追加 -- add a foal */
    { NULL,        0, 0, SWIND_WIDTH_NONE, SWIND_ROW_ADD_HORSE, "\224n\222\307\211\301\201@\201@\201@\201@" }, /* 馬追加 -- add a horse */
    { NULL,        0, 0, SWIND_WIDTH_NONE, SWIND_ROW_CAMERA_FIXED, "\203J\203\201\203\211\214\305\222\350\201@\201@" }, /* カメラ固定 -- fixed camera */
    { NULL,        0, 0, SWIND_WIDTH_NONE, SWIND_ROW_CAMERA_PLAYER, "\203J\203\201\203\211\216\345\220l\214\366\201@" }, /* カメラ主人公 -- camera on the player */
    { NULL,        0, 0, SWIND_WIDTH_NONE, SWIND_ROW_SPECIAL_0, "\203X\203y\203V\203\203\203\213\202O\201@" }, /* スペシャル０ -- special 0 */
    { NULL,        0, 0, SWIND_WIDTH_NONE, SWIND_ROW_SPECIAL_1, "\203X\203y\203V\203\203\203\213\202P\201@" }, /* スペシャル１ -- special 1 */
    { NULL,        0, 0, SWIND_WIDTH_NONE, SWIND_ROW_SPECIAL_2, "\203X\203y\203V\203\203\203\213\202Q\201@" }, /* スペシャル２ -- special 2 */
    { &swind_get_item_no, 0, 500, SWIND_WIDTH_WORD, SWIND_ROW_GET_ITEM, "\203A\203C\203e\203\200\223\374\216\350\201@" }, /* アイテム入手 -- get item */
    { NULL,        0, 0, SWIND_WIDTH_NONE, SWIND_ROW_SPRITE_SPECIAL_0, "\203R\203\215\203X\203y\221\264\202\314\227\353" }, /* コロスペ其の零 -- sprite special 0 */
    { NULL,        0, 0, SWIND_WIDTH_NONE, SWIND_ROW_SPRITE_SPECIAL_1, "\203R\203\215\203X\203y\221\264\202\314\210\353" }, /* コロスペ其の壱 -- sprite special 1 */
    { NULL,        0, 0, SWIND_WIDTH_NONE, SWIND_ROW_SPRITE_SPECIAL_2, "\203R\203\215\203X\203y\221\264\202\314\223\363" }, /* コロスペ其の弐 -- sprite special 2 */
    { NULL,        0, 0, SWIND_WIDTH_NONE, SWIND_ROW_ENGAGE_KAREN, "\203J\203\214\203\223\202\306\215\245\226\361\201@" }, /* カレンと婚約 -- engaged to Karen */
    { NULL,        0, 0, SWIND_WIDTH_NONE, SWIND_ROW_ENGAGE_ANN, "\203\211\203\223\201@\202\306\215\245\226\361\201@" }, /* ラン　と婚約 -- engaged to Ann */
    { NULL,        0, 0, SWIND_WIDTH_NONE, SWIND_ROW_ENGAGE_ELLI, "\203G\203\212\203B\202\306\215\245\226\361\201@" }, /* エリィと婚約 -- engaged to Elli */
    { NULL,        0, 0, SWIND_WIDTH_NONE, SWIND_ROW_ENGAGE_MARY, "\203}\203\212\201[\202\306\215\245\226\361\201@" }, /* マリーと婚約 -- engaged to Mary */
    { NULL,        0, 0, SWIND_WIDTH_NONE, SWIND_ROW_ENGAGE_POPURI, "\203|\203v\203\212\202\306\215\245\226\361\201@" }, /* ポプリと婚約 -- engaged to Popuri */
    { NULL,        0, 0, SWIND_WIDTH_NONE, SWIND_ROW_MARRY_BRIDE, "\201H\201H\201H\202\306\214\213\215\245\201@" }, /* ？？？と結婚 -- marry ??? */
    { &swind_rival_mode, 0, 5, SWIND_WIDTH_WORD, SWIND_ROW_RIVAL_ENGAGEMENT, "\203\211\203C\203o\203\213\202\314\215\245\226\361" }, /* ライバルの婚約 -- rival's engagement */
    { &swind_time_advance, 0, 23, SWIND_WIDTH_WORD, SWIND_ROW_ADVANCE_TIME, "\216\236\212\324\202\360\220i\202\337\202\351\201@" }, /* 時間を進める -- advance the time */
};

/* Carries out the command in swind_row_kind (a SwindRow) and shows the program list; the
 * commands set it back to SWIND_ROW_NONE when done. */
static s32 swind_run_program_page(s32 value) {
    FarmActor *cue;
    FarmHelper *object;
    s16 id;
    s32 i;
    s32 x;
    s32 y;
    s32 k;
    s32 suitor;
    s32 girl;

    if (fade_is_settled() != 0) {
        if (swind_row_kind < 0) {
            KanjiFntPrint("\n\201@\201@\201@\201@\201@\203v\203\215\203O\203\211\203\200\216\300\215s\212\326\214W\n\n"); /* プログラム実行関係 -- running the program */
            swind_cursor = value;
            swind_item_list(swind_run_program_items, SWIND_COUNT(swind_run_program_items));
            value = swind_cursor;
            KanjiFntPrint("\201@\201@\202o\202q\202n\202f\202q\202`\202l\201@%s", swind_row_kind < 0 ? "OFF" : "ON"); /* ＰＲＯＧＲＡＭ　%s -- PROGRAM %s */
            if (swind_row_kind > 0) {
                sound_play(SOUND_MENU_CONFIRM);
            }
        }
        switch (swind_row_kind) {
        case SWIND_ROW_VIBRATION_TEST:
            pad_vibration_start(0, swind_vibration_no);
            swind_row_kind = SWIND_ROW_NONE;
            break;
        case SWIND_ROW_VIBRATION_STOP:
            pad_vibration_stop(0);
            swind_row_kind = SWIND_ROW_NONE;
            break;
        case SWIND_ROW_ADD_COW:
            id = ITEM_COW;
            goto spawn;
        case SWIND_ROW_ADD_YOUNG_COW:
            id = ITEM_YOUNG_COW;
            goto spawn;
        case SWIND_ROW_ADD_CALF:
            id = ITEM_CALF;
            goto spawn;
        case SWIND_ROW_ADD_SHEEP:
            id = ITEM_SHEEP;
            goto spawn;
        case SWIND_ROW_ADD_LAMB:
            id = ITEM_LAMB;
            goto spawn;
        case SWIND_ROW_ADD_CHICKEN:
            id = ITEM_CHICKEN;
            goto spawn;
        case SWIND_ROW_ADD_CHICK:
            id = ITEM_CHICK;
            goto spawn;
        case SWIND_ROW_ADD_FOAL:
            id = ITEM_PONY;
            goto spawn;
        case SWIND_ROW_ADD_HORSE:
            id = ITEM_HORSE;
        spawn:
            cue = farm_action_create(id, -1);
            swind_row_kind = SWIND_ROW_NONE;
            for (i = 0; i < NAME_CHARS; i++) {
                cue->name[i] = rand_range(148);
            }
            cue->name[NAME_CHARS] = 0xFFFF;
            break;
        case SWIND_ROW_CAMERA_FIXED:
            swind_row_kind = SWIND_ROW_NONE;
            game_state.globalFlags |= GLOBAL_FLAG_CAMERA_BUSY;
            break;
        case SWIND_ROW_CAMERA_PLAYER:
            swind_row_kind = SWIND_ROW_NONE;
            game_state.globalFlags &= ~GLOBAL_FLAG_CAMERA_BUSY;
            break;
        case SWIND_ROW_SPECIAL_0:
            cue = farm_action_create(ITEM_COW, -1);
            cue->place.location = 1;
            cue->position.vx = -2000;
            cue->position.vz = 0;
            cue = farm_action_create(ITEM_YOUNG_COW, -1);
            cue->place.location = 1;
            cue->position.vx = -1000;
            cue->position.vz = 0;
            cue = farm_action_create(ITEM_CALF, -1);
            cue->place.location = 1;
            cue->position.vx = 0;
            cue->position.vz = 0;
            cue = farm_action_create(ITEM_CALF, -1);
            cue->place.location = 1;
            cue->position.vx = 1000;
            cue->position.vz = 0;
            cue = farm_action_create(ITEM_LAMB, -1);
            cue->place.location = 1;
            cue->position.vx = 2000;
            cue->position.vz = 0;
            cue = farm_action_create(ITEM_SHEEP, -1);
            cue->place.location = 1;
            cue->position.vx = 3000;
            cue->position.vz = 0;
            swind_row_kind = SWIND_ROW_NONE;
            break;
        case SWIND_ROW_SPECIAL_1:
            for (y = 0; y < SWIND_FIELD_ROWS; y++) {
                for (x = 0; x < SWIND_FIELD_COLS; x++) {
                    field_gate_open(0, x, y, 0);
                }
            }
            swind_row_kind = SWIND_ROW_NONE;
            break;
        case SWIND_ROW_SPECIAL_2:
            swind_row_kind = SWIND_ROW_NONE;
            game_state.player.stateFlags &= ~(PLAYER_FLAG_SCENE_CHANGE | PLAYER_FLAG_SCRIPTED);
            break;
        case SWIND_ROW_GET_ITEM:
            id = item_picture_row(swind_get_item_no);
            switch (id) {
            case PICTURE_ROW_TOOL:
                event_item_store(swind_get_item_no, 0, 1, 0);
                break;
            case PICTURE_ROW_PICKUP:
                if (event_cmd_has_room(1, -1, 0, 0) == 1) {
                    event_cue_issue(swind_get_item_no, 0, 1, 0);
                } else {
                    event_cue_issue(swind_get_item_no, 0, 0, 0);
                }
                break;
            }
            swind_row_kind = SWIND_ROW_NONE;
            break;
        case SWIND_ROW_SPRITE_SPECIAL_0:
            for (k = 0; k < HELPER_COUNT; k++) {
                object = &game_state.helpers[k];
                object->flags |= HELPER_HARVEST;
                harvest_helper_set_action(object, HELPER_HANDLER_PLACE);
            }
            swind_row_kind = SWIND_ROW_NONE;
            break;
        case SWIND_ROW_SPRITE_SPECIAL_1:
            for (k = 0; k < HELPER_COUNT; k++) {
                object = &game_state.helpers[k];
                object->flags |= HELPER_WATER;
                harvest_helper_set_action(object, HELPER_HANDLER_PLACE);
            }
            swind_row_kind = SWIND_ROW_NONE;
            break;
        case SWIND_ROW_SPRITE_SPECIAL_2:
            for (k = 0; k < HELPER_COUNT; k++) {
                object = &game_state.helpers[k];
                object->flags |= HELPER_ANIMALS;
                harvest_helper_set_action(object, HELPER_HANDLER_PLACE);
            }
            swind_row_kind = SWIND_ROW_NONE;
            break;
        case SWIND_ROW_ENGAGE_KAREN:
            event_cmd_init_mode(1, 0);
            swind_row_kind = SWIND_ROW_NONE;
            break;
        case SWIND_ROW_ENGAGE_ANN:
            event_cmd_init_mode(1, 1);
            swind_row_kind = SWIND_ROW_NONE;
            break;
        case SWIND_ROW_ENGAGE_ELLI:
            event_cmd_init_mode(1, 2);
            swind_row_kind = SWIND_ROW_NONE;
            break;
        case SWIND_ROW_ENGAGE_MARY:
            event_cmd_init_mode(1, 3);
            swind_row_kind = SWIND_ROW_NONE;
            break;
        case SWIND_ROW_ENGAGE_POPURI:
            event_cmd_init_mode(1, 4);
            swind_row_kind = SWIND_ROW_NONE;
            break;
        case SWIND_ROW_MARRY_BRIDE:
            wedding_hold(-1);
            switch (game_state.player.wedding.bride) {
            case CHARACTER_KAREN:
                id = 0xA4;
                break;
            case CHARACTER_ANN:
                id = 0xA3;
                break;
            case CHARACTER_ELLI:
                id = 0xA5;
                break;
            case CHARACTER_MARY:
                id = 0xA6;
                break;
            case CHARACTER_POPURI:
            default:
                id = 0xA2;
                break;
            }
            game_state.villagers[game_state.player.wedding.bride].affection = 65535;
            event_flag_set(id + EVENT_FLAG_OFFSET, 1);
            game_state.progress.estate.houseLevel = 2;
            game_state.progress.estate.barnLevel = 1;
            game_state.progress.estate.coopLevel = 1;
            game_state.family.addressMode = rand_range(3);
            swind_row_kind = SWIND_ROW_NONE;
            break;
        case SWIND_ROW_RIVAL_ENGAGEMENT:
            event_cmd_init_mode(0, swind_rival_mode);
            switch (swind_rival_mode) {
            case SWIND_RIVAL_KAREN:
                girl = CHARACTER_KAREN;
                suitor = CHARACTER_RICK;
                break;
            case SWIND_RIVAL_ANN:
                girl = CHARACTER_ANN;
                suitor = CHARACTER_CLIFF;
                break;
            case SWIND_RIVAL_ELLI:
                girl = CHARACTER_ELLI;
                suitor = CHARACTER_DOCTOR;
                break;
            case SWIND_RIVAL_MARY:
                girl = CHARACTER_MARY;
                suitor = CHARACTER_GRAY;
                break;
            case SWIND_RIVAL_POPURI:
            default:
                girl = CHARACTER_POPURI;
                suitor = CHARACTER_KAI;
                break;
            }
            game_state.villagers[suitor].friendship = FRIENDSHIP_MAX;
            game_state.villagers[girl].affection = 0;
            game_state.villagers[suitor].flags |= VILLAGER_FLAG_BIT10;
            swind_row_kind = SWIND_ROW_NONE;
            break;
        case SWIND_ROW_ADVANCE_TIME:
            event_cmd_skip_to_time(swind_time_advance, 0, 0, 0);
            swind_row_kind = SWIND_ROW_NONE;
            break;
        }
    }
    return value;
}

/* swind_pet_page's page, "pet data": the dog's record and the horse's. */
static SwindItem swind_pet_items[48] = {
    { &game_state.animals.dog.actor.kind,             0, 0, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, "\214\242\201E\223\256\225\250\202m\202n\201@" }, /* 犬・動物ＮＯ -- dog: animal no. */
    { &game_state.animals.dog.actor.affection,        0, 255, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\214\242\201E\210\244\217\356\223x\201@\201@" }, /* 犬・愛情度 -- dog: affection */
    { &game_state.animals.dog.actor.date.season,      0, 3, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\214\242\201E\222a\220\266\214\216\201@\201@" }, /* 犬・誕生月 -- dog: birth month */
    { &game_state.animals.dog.actor.date.day,         0, 29, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\214\242\201E\222a\220\266\223\372\201@\201@" }, /* 犬・誕生日 -- dog: birthday */
    { &game_state.animals.dog.training,         0, 255, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\214\242\201E\214P\227\373\223x\201@\201@" }, /* 犬・訓練度 -- dog: training */
    { &game_state.animals.dog.actor.action,           0, 99, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, "\214\242\201E\215s\223\256\201@\201@\201@" }, /* 犬・行動 -- dog: action */
    { &game_state.animals.dog.actor.taskNo,           0, 0, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, "\214\242\201E\217\210\227\235\202m\202n\201@" }, /* 犬・処理ＮＯ -- dog: task no. */
    { &game_state.animals.dog.actor.taskId,           0, 0, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, "\214\242\201E\217\210\227\235\202h\202c\201@" }, /* 犬・処理ＩＤ -- dog: task id */
    { &game_state.animals.dog.actor.taskTime,         0, 0, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, "\214\242\201E\217\210\227\235\216\236\212\324\201@" }, /* 犬・処理時間 -- dog: task time */
    { &game_state.animals.dog.actor.interruptTime,    0, 0, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, "\214\242\201E\212\204\215\236\202\335\216\236\212\324" }, /* 犬・割込み時間 -- dog: interrupt time */
    { &game_state.animals.dog.actor.place.location,         0, 0, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, "\214\242\201E\214\273\215\335\222n\201@\201@" }, /* 犬・現在地 -- dog: location */
    { &game_state.animals.dog.actor.flags,            0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, "\214\242\201E\203t\203\211\203O\201@\201@" }, /* 犬・フラグ -- dog: flags */
    { &game_state.animals.dog.actor.position.vx,      0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, "\214\242\201E\202w\215\300\225W\201@\201@" }, /* 犬・Ｘ座標 -- dog: x */
    { &game_state.animals.dog.actor.position.vy,      0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, "\214\242\201E\202x\215\300\225W\201@\201@" }, /* 犬・Ｙ座標 -- dog: y */
    { &game_state.animals.dog.actor.position.vz,      0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, "\214\242\201E\202y\215\300\225W\201@\201@" }, /* 犬・Ｚ座標 -- dog: z */
    { &game_state.animals.dog.actor.speed.vx,         0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, "\214\242\201E\203X\203s\201[\203h\202w" }, /* 犬・スピードＸ -- dog: speed x */
    { &game_state.animals.dog.actor.speed.vy,         0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, "\214\242\201E\203X\203s\201[\203h\202x" }, /* 犬・スピードＹ -- dog: speed y */
    { &game_state.animals.dog.actor.speed.vz,         0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, "\214\242\201E\203X\203s\201[\203h\202y" }, /* 犬・スピードＺ -- dog: speed z */
    { &game_state.animals.dog.actor.destination.vx,   0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, "\214\242\201E\226\332\223I\222n\202w\201@" }, /* 犬・目的地Ｘ -- dog: destination x */
    { &game_state.animals.dog.actor.destination.vy,   0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, "\214\242\201E\226\332\223I\222n\202x\201@" }, /* 犬・目的地Ｙ -- dog: destination y */
    { &game_state.animals.dog.actor.destination.vz,   0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, "\214\242\201E\226\332\223I\222n\202y\201@" }, /* 犬・目的地Ｚ -- dog: destination z */
    /* Empty rows (空き) between the dog and horse rows. */
    { NULL,                             0, 0, SWIND_WIDTH_NONE, SWIND_ITEM_READ_ONLY, "\213\363\202\253\201@\201@\201@\201@\201@" }, /* 空き -- empty */
    { NULL,                             0, 0, SWIND_WIDTH_NONE, SWIND_ITEM_READ_ONLY, "\213\363\202\253\201@\201@\201@\201@\201@" }, /* 空き -- empty */
    { NULL,                             0, 0, SWIND_WIDTH_NONE, SWIND_ITEM_READ_ONLY, "\213\363\202\253\201@\201@\201@\201@\201@" }, /* 空き -- empty */
    { NULL,                             0, 0, SWIND_WIDTH_NONE, SWIND_ITEM_READ_ONLY, "\213\363\202\253\201@\201@\201@\201@\201@" }, /* 空き -- empty */
    { NULL,                             0, 0, SWIND_WIDTH_NONE, SWIND_ITEM_READ_ONLY, "\213\363\202\253\201@\201@\201@\201@\201@" }, /* 空き -- empty */
    { NULL,                             0, 0, SWIND_WIDTH_NONE, SWIND_ITEM_READ_ONLY, "\213\363\202\253\201@\201@\201@\201@\201@" }, /* 空き -- empty */
    { &game_state.animals.horse.actor.kind,           0, 0, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, "\224n\201E\223\256\225\250\202m\202n\201@" }, /* 馬・動物ＮＯ -- horse: animal no. */
    { &game_state.animals.horse.actor.affection,      0, 255, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\224n\201E\210\244\217\356\223x\201@\201@" }, /* 馬・愛情度 -- horse: affection */
    { &game_state.animals.horse.actor.date.season,    0, 3, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\224n\201E\222a\220\266\214\216\201@\201@" }, /* 馬・誕生月 -- horse: birth month */
    { &game_state.animals.horse.actor.date.day,       0, 29, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\224n\201E\222a\220\266\223\372\201@\201@" }, /* 馬・誕生日 -- horse: birthday */
    { &game_state.animals.horse.training,       0, 255, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\224n\201E\214P\227\373\223x\201@\201@" }, /* 馬・訓練度 -- horse: training */
    { &game_state.animals.horse.actor.action,         0, 99, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, "\224n\201E\215s\223\256\201@\201@\201@" }, /* 馬・行動 -- horse: action */
    { &game_state.animals.horse.actor.taskNo,         0, 0, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, "\224n\201E\217\210\227\235\202m\202n\201@" }, /* 馬・処理ＮＯ -- horse: task no. */
    { &game_state.animals.horse.actor.taskId,         0, 0, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, "\224n\201E\217\210\227\235\202h\202c\201@" }, /* 馬・処理ＩＤ -- horse: task id */
    { &game_state.animals.horse.actor.taskTime,       0, 0, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, "\224n\201E\217\210\227\235\216\236\212\324\201@" }, /* 馬・処理時間 -- horse: task time */
    { &game_state.animals.horse.actor.interruptTime,  0, 0, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, "\224n\201E\212\204\215\236\202\335\216\236\212\324" }, /* 馬・割込み時間 -- horse: interrupt time */
    { &game_state.animals.horse.actor.place.location,       0, 0, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, "\224n\201E\214\273\215\335\222n\201@\201@" }, /* 馬・現在地 -- horse: location */
    { &game_state.animals.horse.actor.flags,          0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, "\224n\201E\203t\203\211\203O\201@\201@" }, /* 馬・フラグ -- horse: flags */
    { &game_state.animals.horse.actor.position.vx,    0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, "\224n\201E\202w\215\300\225W\201@\201@" }, /* 馬・Ｘ座標 -- horse: x */
    { &game_state.animals.horse.actor.position.vy,    0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, "\224n\201E\202x\215\300\225W\201@\201@" }, /* 馬・Ｙ座標 -- horse: y */
    { &game_state.animals.horse.actor.position.vz,    0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, "\224n\201E\202y\215\300\225W\201@\201@" }, /* 馬・Ｚ座標 -- horse: z */
    { &game_state.animals.horse.actor.speed.vx,       0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, "\224n\201E\203X\203s\201[\203h\202w" }, /* 馬・スピードＸ -- horse: speed x */
    { &game_state.animals.horse.actor.speed.vy,       0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, "\224n\201E\203X\203s\201[\203h\202x" }, /* 馬・スピードＹ -- horse: speed y */
    { &game_state.animals.horse.actor.speed.vz,       0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, "\224n\201E\203X\203s\201[\203h\202y" }, /* 馬・スピードＺ -- horse: speed z */
    { &game_state.animals.horse.actor.destination.vx, 0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, "\224n\201E\226\332\223I\222n\202w\201@" }, /* 馬・目的地Ｘ -- horse: destination x */
    { &game_state.animals.horse.actor.destination.vy, 0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, "\224n\201E\226\332\223I\222n\202x\201@" }, /* 馬・目的地Ｙ -- horse: destination y */
    { &game_state.animals.horse.actor.destination.vz, 0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, "\224n\201E\226\332\223I\222n\202y\201@" }, /* 馬・目的地Ｚ -- horse: destination z */
};

/* The selected record of each record editor: swind_livestock_record_page (cows and sheep),
 * swind_chicken_record_page (chickens), swind_poultry_record_page (poultry), swind_event_flag_page (the
 * event flag), swind_farm_record_list (the first farm record shown) and swind_harvest_sprite_page
 * (the harvest sprite). */
static s32 swind_livestock_index = 0;
static s32 swind_chicken_index = 0;
static s32 swind_record_index = 0;
static s32 swind_event_index = 0;
static s32 swind_list_row = 0;
static s32 swind_helper_index = 0;

/* Pet data page: heading and item list, shown once the screen has faded in. */
static s32 swind_pet_page(s32 value) {
    s32 result;

    if (fade_is_settled() != 0) {
        KanjiFntPrint("\n\201@\201@\201@\201@\201@\203y\203b\203g\203f\201[\203^\212\326\214W\n\n"); /* ペットデータ関係 -- pet data */
        swind_cursor = value;
        swind_item_list(swind_pet_items, SWIND_COUNT(swind_pet_items));
        result = swind_cursor;
    } else {
        result = value;
    }
    return result;
}

/* Livestock-record debug page (cows and sheep): edit the 25 fields of the selected record;
 * PAD_L1/PAD_R1 step the selection swind_livestock_index by -1/+1 (swind_step_value, bounds 19 and 0). */
static s32 swind_livestock_record_page(s32 value) {
    SwindItem menu[25] = {
        { &SELECTED_ITEM.kind,       0, 0, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, LABEL_ANIMAL_NO },
        { &SELECTED_ITEM.affection,  0, 255, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, LABEL_AFFECTION },
        { &SELECTED_ITEM.date[1],    0, 3, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, LABEL_BIRTH_MONTH },
        { &SELECTED_ITEM.date[2],    0, 29, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, LABEL_BIRTHDAY },
        { &SELECTED_ITEM.grazingTime, 0, 31, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, LABEL_GRAZING_TIME },
        { &SELECTED_ITEM.daysSick,   0, 31, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, LABEL_DAYS_SICK },
        { &SELECTED_ITEM.growthDays, 0, 31, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, LABEL_GROWTH_DAYS },
        { &SELECTED_ITEM.pregnancy,  0, 31, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, LABEL_PREGNANCY },
        { &SELECTED_ITEM.daysSinceShearing, 0, 31, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, LABEL_DAYS_SINCE_SHEARING },
        { &SELECTED_ITEM.action,     0, 99, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, LABEL_ACTION },
        { &SELECTED_ITEM.taskNo,     0, 0, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, LABEL_TASK_NO },
        { &SELECTED_ITEM.taskId,     0, 0, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, LABEL_TASK_ID },
        { &SELECTED_ITEM.taskTime,   0, 0, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, LABEL_TASK_TIME },
        { &SELECTED_ITEM.interruptTime, 0, 0, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, LABEL_INTERRUPT_TIME },
        { &SELECTED_ITEM.place[1],   0, 0x40, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, LABEL_LOCATION },
        { &SELECTED_ITEM.flags,      0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_FLAGS },
        { &SELECTED_ITEM.position[0], 0, 9999999, SWIND_WIDTH_WORD, SWIND_ITEM_EDITABLE, LABEL_X },
        { &SELECTED_ITEM.position[1], 0, 9999999, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_Y },
        { &SELECTED_ITEM.position[2], 0, 9999999, SWIND_WIDTH_WORD, SWIND_ITEM_EDITABLE, LABEL_Z },
        { &SELECTED_ITEM.speed[0],   0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_SPEED_X },
        { &SELECTED_ITEM.speed[1],   0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_SPEED_Y },
        { &SELECTED_ITEM.speed[2],   0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_SPEED_Z },
        { &SELECTED_ITEM.destination[0], 0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_DESTINATION_X },
        { &SELECTED_ITEM.destination[1], 0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_DESTINATION_Y },
        { &SELECTED_ITEM.destination[2], 0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_DESTINATION_Z },
    };

    if (fade_is_settled() != 0) {
        KanjiFntPrint("\n\201@\201@\201@\201@\201@\213\215\201E\227r\203f\201[\203^\212\326\214W\201\204%2d\n\n", swind_livestock_index); /* 牛・羊データ関係＞%2d -- cow and sheep data > %2d */
        swind_cursor = value;
        swind_item_list(menu, 25);
        value = swind_cursor;
        if (input_pad1->triggered & PAD_L1) {
            swind_step_value(&swind_livestock_index, SWIND_WIDTH_WORD, MAX_LIVESTOCK - 1, 0, -1);
        } else if (input_pad1->triggered & PAD_R1) {
            swind_step_value(&swind_livestock_index, SWIND_WIDTH_WORD, MAX_LIVESTOCK - 1, 0, 1);
        }
    }
    return value;
}

/* Value editor for the selected FarmChicken record: PAD_L1/PAD_R1 step the
 * selection swind_chicken_index by -1/+1 (swind_step_value, bounds 9 and 0). */
static s32 swind_chicken_record_page(s32 arg) {
    SwindItem fields[23] = {
        { &RECORD.kind, 0, 0, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, LABEL_ANIMAL_NO },
        { &RECORD.affection, 0, 255, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, LABEL_AFFECTION },
        { &RECORD.date[1], 0, 3, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, LABEL_BIRTH_MONTH },
        { &RECORD.date[2], 0, 29, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, LABEL_BIRTHDAY },
        { &RECORD.grazingTime, 0, 31, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, LABEL_GRAZING_TIME },
        { &RECORD.daysSick, 0, 31, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, LABEL_DAYS_SICK },
        { &RECORD.pregnancy, 0, 31, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, LABEL_PREGNANCY },
        { &RECORD.action, 0, 99, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, LABEL_ACTION },
        { &RECORD.taskNo, 0, 0, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, LABEL_TASK_NO },
        { &RECORD.taskId, 0, 0, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, LABEL_TASK_ID },
        { &RECORD.taskTime, 0, 0, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, LABEL_TASK_TIME },
        { &RECORD.interruptTime, 0, 0, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, LABEL_INTERRUPT_TIME },
        { &RECORD.bytes2E[2], 0, 0, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, LABEL_LOCATION },
        { &RECORD.flags, 0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_FLAGS },
        { &RECORD.position[0], 0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_X },
        { &RECORD.position[1], 0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_Y },
        { &RECORD.position[2], 0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_Z },
        { &RECORD.speed[0], 0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_SPEED_X },
        { &RECORD.speed[1], 0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_SPEED_Y },
        { &RECORD.speed[2], 0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_SPEED_Z },
        { &RECORD.destination[0], 0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_DESTINATION_X },
        { &RECORD.destination[1], 0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_DESTINATION_Y },
        { &RECORD.destination[2], 0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_DESTINATION_Z },
    };

    if (fade_is_settled() != 0) {
        KanjiFntPrint("\n\201@\201@\201@\201@\201@\214{\203f\201[\203^\212\326\214W\201\204%2d\n\n", swind_chicken_index); /* 鶏データ関係＞%2d -- chicken data > %2d */
        swind_cursor = arg;
        swind_item_list(fields, 23);
        arg = swind_cursor;
        if (input_pad1->triggered & PAD_L1) {
            swind_step_value(&swind_chicken_index, SWIND_WIDTH_WORD, MAX_CHICKENS - 1, 0, -1);
        } else if (input_pad1->triggered & PAD_R1) {
            swind_step_value(&swind_chicken_index, SWIND_WIDTH_WORD, MAX_CHICKENS - 1, 0, 1);
        }
    }
    return arg;
}

/* Poultry record editor (the extra chickens in SWIND_ANIMALS): PAD_L1/PAD_R1 step
 * swind_record_index by -1/+1 (swind_step_value, bounds 2 and 0). */
static s32 swind_poultry_record_page(s32 value) {
    SwindItem items[23] = {
        {&SWIND_ANIMALS[swind_record_index].kind, 0, 0, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, LABEL_ANIMAL_NO},
        {&SWIND_ANIMALS[swind_record_index].affection, 0, 255, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, LABEL_AFFECTION},
        {&SWIND_ANIMALS[swind_record_index].date[1], 0, 3, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, LABEL_BIRTH_MONTH},
        {&SWIND_ANIMALS[swind_record_index].date[2], 0, 29, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, LABEL_BIRTHDAY},
        {&SWIND_ANIMALS[swind_record_index].grazingTime, 0, 31, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, LABEL_GRAZING_TIME},
        {&SWIND_ANIMALS[swind_record_index].daysSick, 0, 31, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, LABEL_DAYS_SICK},
        {&SWIND_ANIMALS[swind_record_index].pregnancy, 0, 31, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, LABEL_PREGNANCY},
        {&SWIND_ANIMALS[swind_record_index].action, 0, 99, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, LABEL_ACTION},
        {&SWIND_ANIMALS[swind_record_index].taskNo, 0, 0, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, LABEL_TASK_NO},
        {&SWIND_ANIMALS[swind_record_index].taskId, 0, 0, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, LABEL_TASK_ID},
        {&SWIND_ANIMALS[swind_record_index].taskTime, 0, 0, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, LABEL_TASK_TIME},
        {&SWIND_ANIMALS[swind_record_index].interruptTime, 0, 0, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, LABEL_INTERRUPT_TIME},
        {&SWIND_ANIMALS[swind_record_index].raw2E[2], 0, 0, SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, LABEL_LOCATION},
        {&SWIND_ANIMALS[swind_record_index].flags, 0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_FLAGS},
        {&SWIND_ANIMALS[swind_record_index].position.vx, 0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_X},
        {&SWIND_ANIMALS[swind_record_index].position.vy, 0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_Y},
        {&SWIND_ANIMALS[swind_record_index].position.vz, 0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_Z},
        {&SWIND_ANIMALS[swind_record_index].speed.vx, 0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_SPEED_X},
        {&SWIND_ANIMALS[swind_record_index].speed.vy, 0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_SPEED_Y},
        {&SWIND_ANIMALS[swind_record_index].speed.vz, 0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_SPEED_Z},
        {&SWIND_ANIMALS[swind_record_index].destination.vx, 0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_DESTINATION_X},
        {&SWIND_ANIMALS[swind_record_index].destination.vy, 0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_DESTINATION_Y},
        {&SWIND_ANIMALS[swind_record_index].destination.vz, 0, 0, SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_DESTINATION_Z},
    };

    if (fade_is_settled() != 0) {
        KanjiFntPrint("\n\201@\201@\201@\201@\201@\227{\214{\201E\214{\203f\201[\203^\212\326\214W\201\204%2d\n\n", swind_record_index); /* 養鶏・鶏データ関係＞%2d -- poultry data > %2d */
        swind_cursor = value;
        swind_item_list(items, 23);
        value = swind_cursor;
        if (input_pad1->triggered & PAD_L1) {
            swind_step_value(&swind_record_index, SWIND_WIDTH_WORD, EXTRA_CHICKENS - 1, 0, -1);
        } else if (input_pad1->triggered & PAD_R1) {
            swind_step_value(&swind_record_index, SWIND_WIDTH_WORD, EXTRA_CHICKENS - 1, 0, 1);
        }
    }
    return value;
}

/* Value editor for the selected farm record: PAD_L1/PAD_R1 step the selection
 * by -1/+1 (swind_step_value, bounds 36 and 0). */
static s32 swind_villager_record_page(s32 arg) {
    SwindItem fields[32] = {
        { &FARM_RECORD.id,     0, 39,     SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, "\226\274\221O\202m\202n\201@\201@\201@\201@" }, /* 名前ＮＯ -- name no. */
        { &FARM_RECORD.id,     0, 1,      SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, "\220\253\225\312\201@\201@\201@\201@\201@\201@" }, /* 性別 -- sex */
        { &FARM_RECORD.friendship,  0, FRIENDSHIP_MAX,    SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\227F\215D\223x\201@\201@\201@\201@\201@" }, /* 友好度 -- friendship */
        { &FARM_RECORD.affection,   0, 65535, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\210\244\217\356\223x\201@\201@\201@\201@\201@" }, /* 愛情度 -- affection */
        { &FARM_RECORD.id,     0, 31,     SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, "\211\357\230b\212\324\212u\201@\201@\201@\201@" }, /* 会話間隔 -- talk interval */
        { &FARM_RECORD.facing,       0, 3,      SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, "\214\374\202\253\201@\201@\201@\201@\201@\201@" }, /* 向き -- facing */
        { &FARM_RECORD.lockCount,      0, 0,      SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, "\221\200\215\354\213\326\216~\202c\202b\201@\201@" }, /* 操作禁止ＤＣ -- control lock DC */
        { &FARM_RECORD.delay,      0, 0,      SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, "\202r\222x\211\204\216\236\212\324\201@\201@\201@" }, /* Ｓ遅延時間 -- S delay time */
        { &FARM_RECORD.id,     0, 31,     SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, "\211\357\230b\212\324\212u\201@\201@\201@\201@" }, /* 会話間隔 -- talk interval */
        { &FARM_RECORD.column,      0, 0,      SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, "\202k\202r\201@\202m\202n\201@\201@\201@" }, /* ＬＳ　ＮＯ -- LS no. */
        { &FARM_RECORD.action,      0, 0,      SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, "\215s\223\256\201@\201@\201@\201@\201@\201@" }, /* 行動 -- action */
        { &FARM_RECORD.prevAction,      0, 0,      SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, "\220\314\202\314\215s\223\256\201@\201@\201@\201@" }, /* 昔の行動 -- previous action */
        { &FARM_RECORD.step,      0, 0,      SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, "\202k\202r\201@\216\236\212\324\201@\201@\201@" }, /* ＬＳ　時間 -- LS time */
        { &FARM_RECORD.flags,      0, 0,      SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, "\203t\203\211\203O\201i\202w\201j\201@\201@" }, /* フラグ（Ｘ） -- flags (hex) */
        { &FARM_RECORD.place.value,  0, 0,      SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, "\217\352\217\212\201i\221\345\210\346\201j\201@\201@" }, /* 場所（大域） -- place (map) */
        { &FARM_RECORD.place.location,  0, 0,      SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, "\217\352\217\212\201i\222n\210\346\201j\201@\201@" }, /* 場所（地域） -- place (area) */
        { &FARM_RECORD.position.vx,  0, 0,      SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, "\202w\215\300\225W\201@\201@\201@\201@\201@" }, /* Ｘ座標 -- x */
        { &FARM_RECORD.position.vy,  0, 0,      SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, "\202x\215\300\225W\201@\201@\201@\201@\201@" }, /* Ｙ座標 -- y */
        { &FARM_RECORD.position.vz,  0, 0,      SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, "\202y\215\300\225W\201@\201@\201@\201@\201@" }, /* Ｚ座標 -- z */
        { &FARM_RECORD.speed.vx,      0, 0,      SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, "\203X\203s\201[\203h\202w\201@\201@\201@" }, /* スピードＸ -- speed x */
        { &FARM_RECORD.speed.vx,      0, 0,      SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, "\203X\203s\201[\203h\202x\201@\201@\201@" }, /* スピードＹ -- speed y */
        { &FARM_RECORD.speed.vx,      0, 0,      SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, "\203X\203s\201[\203h\202y\201@\201@\201@" }, /* スピードＺ -- speed z */
        { &FARM_RECORD.ePosition.vx,  0, 0,      SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, "\202d\202w\215\300\225W\201@\201@\201@\201@" }, /* ＥＸ座標 -- FarmHourEvent x */
        { &FARM_RECORD.ePosition.vy,  0, 0,      SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, "\202d\202x\215\300\225W\201@\201@\201@\201@" }, /* ＥＹ座標 -- FarmHourEvent y */
        { &FARM_RECORD.ePosition.vz,  0, 0,      SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, "\202d\202y\215\300\225W\201@\201@\201@\201@" }, /* ＥＺ座標 -- FarmHourEvent z */
        { &FARM_RECORD.eSpeed.vx,  0, 0,      SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, "\202d\203X\203s\201[\203h\202w\201@\201@" }, /* ＥスピードＸ -- FarmHourEvent speed x */
        { &FARM_RECORD.eSpeed.vy,  0, 0,      SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, "\202d\203X\203s\201[\203h\202x\201@\201@" }, /* ＥスピードＹ -- FarmHourEvent speed y */
        { &FARM_RECORD.eSpeed.vz,  0, 0,      SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, "\202d\203X\203s\201[\203h\202y\201@\201@" }, /* ＥスピードＺ -- FarmHourEvent speed z */
        { &FARM_RECORD.birthday.season,      0, 0,      SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, "\222a\220\266\214\216\201@\201@\201@\201@\201@" }, /* 誕生月 -- birth month */
        { &FARM_RECORD.birthday.day,      0, 0,      SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, "\222a\220\266\223\372\201@\201@\201@\201@\201@" }, /* 誕生日 -- birthday */
        { &FARM_RECORD.destination,      0, 100,    SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\226\332\223I\222n\201@\201@\201@\201@\201@" }, /* 目的地 -- destination */
        { &FARM_RECORD.prevDestination,      0, 100,    SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\221O\211\361\202\314\226\332\223I\222n\201@\201@" }, /* 前回の目的地 -- previous destination */
    };

    if (fade_is_settled() != 0) {
        KanjiFntPrint("\n\201@\201@\201@\201@\201@\217Z\220l\203f\201[\203^\212\326\214W\201\204%2d\n\n", game_state.villagerCursor); /* 住人データ関係＞%2d -- villager data > %2d */
        swind_cursor = arg;
        swind_item_list(fields, 32);
        arg = swind_cursor;
        if (input_pad1->triggered & PAD_L1) {
            swind_step_value(&game_state.villagerCursor, SWIND_WIDTH_HALF, CHARACTER_COUNT - 1, 0, -1);
        } else if (input_pad1->triggered & PAD_R1) {
            swind_step_value(&game_state.villagerCursor, SWIND_WIDTH_HALF, CHARACTER_COUNT - 1, 0, 1);
        }
    }
    return arg;
}

/* Event-flag debug page: the pad scrolls the selected flag (0..EVENT_FLAG_COUNT-1; circle
 * and cross move it by 5, triangle and square by 50, up and down by 1), PAD_LEFT sets it and
 * PAD_RIGHT clears it, and the selected flag plus the next ten are printed. */
static s32 swind_event_flag_page(s32 page)
{
    s32 value;
    u32 i;

    if (input_pad1->triggered & PAD_CIRCLE) {
        value = swind_event_index + 5;
        swind_event_index = value;
        if (value >= EVENT_FLAG_COUNT) {
            swind_event_index = 0;
        }
    }
    if (input_pad1->triggered & PAD_CROSS) {
        value = swind_event_index - 5;
        swind_event_index = value;
        if (value <= 0) {
            swind_event_index = EVENT_FLAG_COUNT - 1;
        }
    }
    if (input_pad1->triggered & PAD_TRIANGLE) {
        value = swind_event_index + 50;
        swind_event_index = value;
        if (value >= EVENT_FLAG_COUNT) {
            swind_event_index = 0;
        }
    }
    if (input_pad1->triggered & PAD_SQUARE) {
        value = swind_event_index - 50;
        swind_event_index = value;
        if (value <= 0) {
            swind_event_index = EVENT_FLAG_COUNT - 1;
        }
    }
    if (input_pad1->triggered & PAD_DOWN) {
        value = swind_event_index + 1;
        swind_event_index = value;
        if (value >= EVENT_FLAG_COUNT) {
            swind_event_index = 0;
        }
    }
    if (input_pad1->triggered & PAD_UP) {
        value = swind_event_index;
        if (value > 0) {
            value--;
        } else {
            value = EVENT_FLAG_COUNT - 1;
        }
        swind_event_index = value;
    }
    if (input_pad1->pressed & PAD_LEFT) {
        event_flag_set(swind_event_index, 1);
    }
    if (input_pad1->pressed & PAD_RIGHT) {
        event_flag_set(swind_event_index, 0);
    }
    if (event_flag_get(swind_event_index) == 1) {
        FntPrint("\nEDIT.%d ON \n\n", swind_event_index);
    } else {
        FntPrint("\nEDIT.%d OFF \n\n", swind_event_index);
    }
    for (i = 0; i < 10; i++) {
        if (event_flag_get(swind_event_index + i - (swind_event_index + i) / EVENT_FLAG_COUNT * EVENT_FLAG_COUNT) == 1) {
            FntPrint("EVNT.%d(%d) ON \n", swind_event_index + i, swind_event_index + i - EVENT_FLAG_OFFSET);
        } else {
            FntPrint("EVNT.%d(%d) OFF \n", swind_event_index + i, swind_event_index + i - EVENT_FLAG_OFFSET);
        }
    }
    return page;
}

/* Debug view of the farm records: PAD_L1/PAD_R1 scroll the first shown record by
 * 5 and PAD_UP/PAD_DOWN by 1 (wrapping over the CHARACTER_COUNT records), then ten records
 * from there are printed as index and script offset. */
static s32 swind_farm_record_list(void) {
    u32 i;
    u32 index;
    FarmVillager *item;

    if (input_pad1->triggered & PAD_L1) {
        swind_list_row -= 5;
        if (swind_list_row <= 0) {
            swind_list_row = CHARACTER_COUNT - 1;
        }
    }
    if (input_pad1->triggered & PAD_R1) {
        swind_list_row += 5;
        if (swind_list_row > CHARACTER_COUNT - 1) {
            swind_list_row = 0;
        }
    }
    if (input_pad1->triggered & PAD_UP) {
        swind_list_row -= 1;
        if (swind_list_row <= 0) {
            swind_list_row = CHARACTER_COUNT - 1;
        }
    }
    if (input_pad1->triggered & PAD_DOWN) {
        swind_list_row += 1;
        if (swind_list_row > CHARACTER_COUNT - 1) {
            swind_list_row = 0;
        }
    }
    for (i = 0; i < 10; i++) {
        index = swind_list_row + i;
        item = &game_state.villagers[index % CHARACTER_COUNT];
        FntPrint("P %d ADR %x\n", index % CHARACTER_COUNT, item->scriptOffset);
    }
    return 0;
}

/* The harvest sprite data window: edit sprite swind_helper_index's record, and step
 * the selected sprite with PAD_L1/PAD_R1. */
static s32 swind_harvest_sprite_page(s32 value) {
    SwindItem items[29] = {
        { &game_state.helpers[swind_helper_index].harvestSkill,       0, 255,  SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\216\373\212n\214P\227\373\223x" }, /* 収穫訓練度 -- harvesting skill */
        { &game_state.helpers[swind_helper_index].waterSkill,      0, 255,  SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\216U\220\205\214P\227\373\223x" }, /* 散水訓練度 -- watering skill */
        { &game_state.helpers[swind_helper_index].animalSkill,          0, 255,  SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\220\242\230b\214P\227\373\223x" }, /* 世話訓練度 -- animal care skill */
        { &game_state.helpers[swind_helper_index].jobDays,         0, 31,  SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\216d\216\226\212\372\212\324\201@" }, /* 仕事期間 -- work period */
        { &game_state.helpers[swind_helper_index].item,               0, 0,     SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, LABEL_ITEM }, /* アイテム -- item */
        { &game_state.helpers[swind_helper_index].flags,              0, 0,     SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, LABEL_FLAGS },
        { &game_state.helpers[swind_helper_index].targetAnimal,       0, 0,     SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, "\226\332\223I\211\306\222{\201@" }, /* 目的家畜 -- target animal */
        { &game_state.helpers[swind_helper_index].actor.affection,          0, 255,  SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, LABEL_AFFECTION },
        { &game_state.helpers[swind_helper_index].actor.action,             0, 99,  SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, LABEL_ACTION },
        { &game_state.helpers[swind_helper_index].actor.taskNo,             0, 0,     SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, LABEL_TASK_NO },
        { &game_state.helpers[swind_helper_index].actor.taskId,             0, 0,     SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, LABEL_TASK_ID },
        { &game_state.helpers[swind_helper_index].actor.taskTime,           0, 0,     SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, LABEL_TASK_TIME },
        { &game_state.helpers[swind_helper_index].actor.interruptTime,      0, 0,     SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, LABEL_INTERRUPT_TIME },
        { &game_state.helpers[swind_helper_index].actor.place.location,           0, 0,     SWIND_WIDTH_HALF, SWIND_ITEM_READ_ONLY, LABEL_LOCATION },
        { &game_state.helpers[swind_helper_index].actor.flags,            0, 0,     SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_FLAGS },
        { &game_state.helpers[swind_helper_index].actor.position.vx,        0, 0,     SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_X },
        { &game_state.helpers[swind_helper_index].actor.position.vy,        0, 0,     SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_Y },
        { &game_state.helpers[swind_helper_index].actor.position.vz,        0, 0,     SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_Z },
        { &game_state.helpers[swind_helper_index].actor.speed.vx,           0, 0,     SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_SPEED_X },
        { &game_state.helpers[swind_helper_index].actor.speed.vy,           0, 0,     SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_SPEED_Y },
        { &game_state.helpers[swind_helper_index].actor.speed.vz,           0, 0,     SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_SPEED_Z },
        { &game_state.helpers[swind_helper_index].actor.destination.vx,     0, 0,     SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_DESTINATION_X },
        { &game_state.helpers[swind_helper_index].actor.destination.vy,     0, 0,     SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_DESTINATION_Y },
        { &game_state.helpers[swind_helper_index].actor.destination.vz,     0, 0,     SWIND_WIDTH_WORD, SWIND_ITEM_READ_ONLY, LABEL_DESTINATION_Z },
        { &game_state.animals.moveSpeeds[0],                     0, 999, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\210\332\223\256\202r\202o\202O" }, /* 移動ＳＰ０ -- move speed 0 */
        { &game_state.animals.moveSpeeds[1],                     0, 999, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\210\332\223\256\202r\202o\202P" }, /* 移動ＳＰ１ -- move speed 1 */
        { &game_state.animals.moveSpeeds[2],                     0, 999, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\210\332\223\256\202r\202o\202Q" }, /* 移動ＳＰ２ -- move speed 2 */
        { &game_state.animals.moveSpeeds[3],                     0, 999, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\210\332\223\256\202r\202o\202R" }, /* 移動ＳＰ３ -- move speed 3 */
        { &game_state.animals.moveSpeeds[4],                     0, 999, SWIND_WIDTH_HALF, SWIND_ITEM_EDITABLE, "\210\332\223\256\202r\202o\202S" }, /* 移動ＳＰ４ -- move speed 4 */
    };

    if (fade_is_settled() != 0) {
        KanjiFntPrint("\n\201@\201@\201@\201@\201@\203R\203\215\203{\203b\203N\203\213\203f\201[\203^\212\326\214W\201\204%2d\n\n", swind_helper_index); /* コロボックルデータ関係＞%2d -- harvest sprite data > %2d */
        swind_cursor = value;
        swind_item_list(items, 29);
        value = swind_cursor;
        if (input_pad1->triggered & PAD_L1) {
            swind_step_value(&swind_helper_index, SWIND_WIDTH_WORD, HELPER_COUNT - 1, 0, -1);
        } else if (input_pad1->triggered & PAD_R1) {
            swind_step_value(&swind_helper_index, SWIND_WIDTH_WORD, HELPER_COUNT - 1, 0, 1);
        }
    }
    return value;
}

/* swind_flag_page's page, "flags": one bit of a flag word (status, debug or estate flags) per row. */
static SwindFlagEntry swind_flag_entries[12] = {
    { &game_state.statusFlags, 0, STATUS_FLAG_DEBUG_EVENTS_300_AND_UP, SWIND_WIDTH_WORD, SWIND_ITEM_EDITABLE, {0}, "\203C\203x\203\223\203g\202R\202O\202O\210\310\215~\202\314\202\335" }, /* イベント３００以降のみ -- events 300 and up only */
    { &game_state.debugFlags,   0, DEBUG_FLAG_DRAW_MESSAGE_NUMBERS, SWIND_WIDTH_WORD, SWIND_ITEM_EDITABLE, {0}, "\203\201\203b\203Z\201[\203W\202m\202\217\225`\211\346\201@\201@" }, /* メッセージＮｏ描画 -- draw message numbers */
    { &game_state.debugFlags,   0, DEBUG_FLAG_NO_2D_FIXTURES, SWIND_WIDTH_WORD, SWIND_ITEM_EDITABLE, {0}, "\202Q\202c\214\305\222\350\225\250\225`\211\346\202\265\202\310\202\242\201@" }, /* ２Ｄ固定物描画しない -- don't draw 2D fixtures */
    { &game_state.debugFlags,   0, DEBUG_FLAG_NO_ANIMALS, SWIND_WIDTH_WORD, SWIND_ITEM_EDITABLE, {0}, "\223\256\225\250\217\210\227\235\202\265\202\310\202\242\201@\201@\201@\201@" }, /* 動物処理しない -- no animals */
    { &game_state.debugFlags,   0, DEBUG_FLAG_NO_EVENTS, SWIND_WIDTH_WORD, SWIND_ITEM_EDITABLE, {0}, "\203C\203x\203\223\203g\217\210\227\235\202\265\202\310\202\242\201@\201@" }, /* イベント処理しない -- no events */
    { &game_state.debugFlags,   0, DEBUG_FLAG_NO_EVENT_TRIGGERS, SWIND_WIDTH_WORD, SWIND_ITEM_EDITABLE, {0}, "\203C\203x\203\223\203g\224\255\220\266\217\210\227\235\202\265\202\310\202\242" }, /* イベント発生処理しない -- no event triggers */
    { &game_state.debugFlags,   0, DEBUG_FLAG_NO_BREAKING_ANIMATIONS, SWIND_WIDTH_WORD, SWIND_ITEM_EDITABLE, {0}, "\224j\211\363\203A\203j\203\201\217\210\227\235\202\265\202\310\202\242\201@" }, /* 破壊アニメ処理しない -- no breaking animations */
    { &game_state.debugFlags,   0, DEBUG_FLAG_NO_FIELD_DRAW, SWIND_WIDTH_WORD, SWIND_ITEM_EDITABLE, {0}, "\224\250\225`\211\346\202\265\202\310\202\242\201@\201@\201@\201@\201@" }, /* 畑描画しない -- don't draw the fields */
    { &game_state.debugFlags,   0, DEBUG_FLAG_NO_HARVEST_SPRITES, SWIND_WIDTH_WORD, SWIND_ITEM_EDITABLE, {0}, "\203R\203\215\203{\203b\203N\203\213\217\210\227\235\202\265\202\310\202\242" }, /* コロボックル処理しない -- no harvest sprites */
    { &game_state.progress.estate.estateFlags, 0, ESTATE_FLAG_STRAY_DOGS, SWIND_WIDTH_WORD, SWIND_ITEM_EDITABLE, {0}, "\226\354\214\242\217o\214\273\202\267\202\351\201@\201@\201@\201@\201@" }, /* 野犬出現する -- stray dogs appear */
    { &game_state.progress.estate.estateFlags, 0, ESTATE_FLAG_VASE_SET, SWIND_WIDTH_WORD, SWIND_ITEM_EDITABLE, {0}, "\211\324\225r\203Z\203b\203g\201@\201@\201@\201@\201@\201@" }, /* 花瓶セット -- vase set */
    { &game_state.debugFlags,   0, DEBUG_FLAG_DRAW_EVENT_NUMBERS, SWIND_WIDTH_WORD, SWIND_ITEM_EDITABLE, {0}, "\203C\203x\203\223\203g\202m\202n\225`\211\346\201@\201@\201@" }, /* イベントＮＯ描画 -- draw event numbers */
};

/* The cursors of swind_day_count0_page, swind_day_count1_page and swind_day_values_page (an index
 * into each byte table), swind_price_page (a FarmState.itemInfo record and its field)
 * and swind_tool_stamina_page (a tool and its level). */
static s32 swind_day_count0_index = 0;
static s32 swind_day_count1_index = 0;
static s32 swind_day_values_index = 0;
static s32 swind_price_index = 0;
static s32 swind_price_field = 0;
static s32 swind_tool_index = 0;
static s32 swind_tool_level = 0;

/* Flag page: heading and flag list, shown once the screen has faded in. */
static s32 swind_flag_page(s32 value) {
    s32 result;

    if (fade_is_settled() != 0) {
        KanjiFntPrint("\n\201@\201@\201@\201@\201@\203t\203\211\203O\212\326\214W\n\n"); /* フラグ関係 -- flags */
        swind_cursor = value;
        swind_flag_list(swind_flag_entries, 12);
        result = swind_cursor;
    } else {
        result = value;
    }
    return result;
}

/* Debug editor for the day counters: left/right move the cursor by 10, up/down
 * by 1 (wrapping over 0..0x7F); the face buttons add or subtract 1 or 10 from
 * the selected byte (wrapping at 0xFF). Then print the cursor's entry and the
 * ten entries from it on. */
static s32 swind_day_count0_page(s32 page) {
    s32 value;
    s32 index;
    u32 count;

    if (input_pad1->triggered & PAD_RIGHT) {
        value = swind_day_count0_index + 10;
        swind_day_count0_index = value;
        if (value >= 128) {
            swind_day_count0_index = 0;
        }
    }
    if (input_pad1->triggered & PAD_LEFT) {
        value = swind_day_count0_index - 10;
        swind_day_count0_index = value;
        if (value <= 0) {
            swind_day_count0_index = 127;
        }
    }
    if (input_pad1->triggered & PAD_DOWN) {
        value = swind_day_count0_index + 1;
        swind_day_count0_index = value;
        if (value >= 128) {
            swind_day_count0_index = 0;
        }
    }
    if (input_pad1->triggered & PAD_UP) {
        value = swind_day_count0_index;
        if (value > 0) {
            value--;
        } else {
            value = 127;
        }
        swind_day_count0_index = value;
    }
    if (input_pad1->triggered & PAD_CIRCLE) {
        game_state.days.count0[swind_day_count0_index] = game_state.days.count0[swind_day_count0_index] + 1;
        if (game_state.days.count0[swind_day_count0_index] >= SWIND_BYTE_MAX) {
            game_state.days.count0[swind_day_count0_index] = 0;
        }
    }
    if (input_pad1->triggered & PAD_CROSS) {
        if (game_state.days.count0[swind_day_count0_index] != 0) {
            game_state.days.count0[swind_day_count0_index] = game_state.days.count0[swind_day_count0_index] - 1;
        } else {
            game_state.days.count0[swind_day_count0_index] = SWIND_BYTE_MAX;
        }
    }
    if (input_pad1->triggered & PAD_TRIANGLE) {
        game_state.days.count0[swind_day_count0_index] = game_state.days.count0[swind_day_count0_index] + 10;
        if (game_state.days.count0[swind_day_count0_index] >= SWIND_BYTE_MAX) {
            game_state.days.count0[swind_day_count0_index] = 0;
        }
    }
    if (input_pad1->triggered & PAD_SQUARE) {
        if (game_state.days.count0[swind_day_count0_index] != 0) {
            game_state.days.count0[swind_day_count0_index] = game_state.days.count0[swind_day_count0_index] - 10;
        } else {
            game_state.days.count0[swind_day_count0_index] = SWIND_BYTE_MAX;
        }
    }
    FntPrint("\n\nEDIT.[%d] = %d \n\n", swind_day_count0_index, game_state.days.count0[swind_day_count0_index]);
    count = 0;
    do {
        index = swind_day_count0_index;
        if (swind_day_count0_index + count < 128U) {
            FntPrint("Ke-Day-Cnt0.[%d] = %d \n", index + count, game_state.days.count0[index + count]);
        }
        count++;
    } while (count < 10);
    return page;
}

/* The same editor as swind_day_count0_page, over the 8 entries of days.count1. */
static s32 swind_day_count1_page(s32 page) {
    s32 value;
    s32 index;
    u32 count;

    if (input_pad1->triggered & PAD_RIGHT) {
        value = swind_day_count1_index + 10;
        swind_day_count1_index = value;
        if (value >= 8) {
            swind_day_count1_index = 0;
        }
    }
    if (input_pad1->triggered & PAD_LEFT) {
        value = swind_day_count1_index - 10;
        swind_day_count1_index = value;
        if (value <= 0) {
            swind_day_count1_index = 7;
        }
    }
    if (input_pad1->triggered & PAD_DOWN) {
        value = swind_day_count1_index + 1;
        swind_day_count1_index = value;
        if (value >= 8) {
            swind_day_count1_index = 0;
        }
    }
    if (input_pad1->triggered & PAD_UP) {
        value = swind_day_count1_index;
        if (value > 0) {
            value--;
        } else {
            value = 7;
        }
        swind_day_count1_index = value;
    }
    if (input_pad1->triggered & PAD_CIRCLE) {
        game_state.days.count1[swind_day_count1_index] = game_state.days.count1[swind_day_count1_index] + 1;
        if (game_state.days.count1[swind_day_count1_index] >= SWIND_BYTE_MAX) {
            game_state.days.count1[swind_day_count1_index] = 0;
        }
    }
    if (input_pad1->triggered & PAD_CROSS) {
        if (game_state.days.count1[swind_day_count1_index] != 0) {
            game_state.days.count1[swind_day_count1_index] = game_state.days.count1[swind_day_count1_index] - 1;
        } else {
            game_state.days.count1[swind_day_count1_index] = SWIND_BYTE_MAX;
        }
    }
    if (input_pad1->triggered & PAD_TRIANGLE) {
        game_state.days.count1[swind_day_count1_index] = game_state.days.count1[swind_day_count1_index] + 10;
        if (game_state.days.count1[swind_day_count1_index] >= SWIND_BYTE_MAX) {
            game_state.days.count1[swind_day_count1_index] = 0;
        }
    }
    if (input_pad1->triggered & PAD_SQUARE) {
        if (game_state.days.count1[swind_day_count1_index] != 0) {
            game_state.days.count1[swind_day_count1_index] = game_state.days.count1[swind_day_count1_index] - 10;
        } else {
            game_state.days.count1[swind_day_count1_index] = SWIND_BYTE_MAX;
        }
    }
    FntPrint("\n\nEDIT.[%d] = %d \n\n", swind_day_count1_index, game_state.days.count1[swind_day_count1_index]);
    count = 0;
    do {
        index = swind_day_count1_index;
        if (swind_day_count1_index + count < 8U) {
            FntPrint("Ke-Day-CNT1.[%d] = %d \n", index + count, game_state.days.count1[index + count]);
        }
        count++;
    }
    while (count < 10);
    return page;
}

/* The same editor as swind_day_count0_page, over the 128 entries of days.values. */
static s32 swind_day_values_page(s32 page) {
    s32 value;
    s32 index;
    u32 count;
    if (input_pad1->triggered & PAD_RIGHT) {
        value = swind_day_values_index + 10;
        swind_day_values_index = value;
        if (value >= 128) {
            swind_day_values_index = 0;
        }
    }
    if (input_pad1->triggered & PAD_LEFT) {
        value = swind_day_values_index - 10;
        swind_day_values_index = value;
        if (value <= 0) {
            swind_day_values_index = 127;
        }
    }
    if (input_pad1->triggered & PAD_DOWN) {
        value = swind_day_values_index + 1;
        swind_day_values_index = value;
        if (value >= 128) {
            swind_day_values_index = 0;
        }
    }
    if (input_pad1->triggered & PAD_UP) {
        value = swind_day_values_index;
        if (value > 0) {
            value--;
        } else {
            value = 127;
        }
        swind_day_values_index = value;
    }
    if (input_pad1->triggered & PAD_CIRCLE) {
        game_state.days.values[swind_day_values_index] = game_state.days.values[swind_day_values_index] + 1;
        if (game_state.days.values[swind_day_values_index] >= SWIND_BYTE_MAX) {
            game_state.days.values[swind_day_values_index] = 0;
        }
    }
    if (input_pad1->triggered & PAD_CROSS) {
        if (game_state.days.values[swind_day_values_index] != 0) {
            game_state.days.values[swind_day_values_index] = game_state.days.values[swind_day_values_index] - 1;
        } else {
            game_state.days.values[swind_day_values_index] = SWIND_BYTE_MAX;
        }
    }
    if (input_pad1->triggered & PAD_TRIANGLE) {
        game_state.days.values[swind_day_values_index] = game_state.days.values[swind_day_values_index] + 10;
        if (game_state.days.values[swind_day_values_index] >= SWIND_BYTE_MAX) {
            game_state.days.values[swind_day_values_index] = 0;
        }
    }
    if (input_pad1->triggered & PAD_SQUARE) {
        if (game_state.days.values[swind_day_values_index] != 0) {
            game_state.days.values[swind_day_values_index] = game_state.days.values[swind_day_values_index] - 10;
        } else {
            game_state.days.values[swind_day_values_index] = SWIND_BYTE_MAX;
        }
    }
    FntPrint("\n\nEDIT.[%d] = %d \n\n", swind_day_values_index, game_state.days.values[swind_day_values_index]);
    count = 0;
    do {
        index = swind_day_values_index;
        if (swind_day_values_index + count < 128U) {
            FntPrint("Ke-N-VAL.[%d] = %d \n", index + count, game_state.days.values[index + count]);
        }
        count++;
    }
    while (count < 10);
    return page;
}

/* Edits a value from the pad: SQUARE subtracts 1, CIRCLE adds 1. */
static s32 swind_pad_step(s32 value) {
    s32 triggered = input_pad1->triggered;
    if (triggered & PAD_SQUARE) {
        value--;
    }
    if (triggered & PAD_CIRCLE) {
        value++;
    }
    return value;
}
/* Price editor: L1/R1 step the item index by 10, LEFT/RIGHT by 1, UP/DOWN pick the field
 * (buy, sell, life, tired), and swind_pad_step edits the selected field. */
static s32 swind_price_page(s32 page) {
    s32 value;
    ItemInfo *record;
    if (input_pad1->triggered & PAD_R1) {
        value = swind_price_index + 10;
        swind_price_index = value;
        if (value >= SWIND_PRICE_COUNT) {
            swind_price_index = 0;
        }
    }
    if (input_pad1->triggered & PAD_L1) {
        value = swind_price_index - 10;
        swind_price_index = value;
        if (value <= 0) {
            swind_price_index = SWIND_PRICE_LAST;
        }
    }
    if (input_pad1->triggered & PAD_RIGHT) {
        value = swind_price_index + 1;
        swind_price_index = value;
        if (value >= SWIND_PRICE_COUNT) {
            swind_price_index = 0;
        }
    }
    if (input_pad1->triggered & PAD_LEFT) {
        value = swind_price_index - 1;
        swind_price_index = value;
        if (value <= 0) {
            swind_price_index = SWIND_PRICE_LAST;
        }
    }
    if (input_pad1->triggered & PAD_UP) {
        value = swind_price_field - 1;
        swind_price_field = value;
        if (value < 0) swind_price_field = SWIND_PRICE_FIELD_LAST;
    }
    if (input_pad1->triggered & PAD_DOWN) {
        value = swind_price_field + 1;
        swind_price_field = value;
        if (value >= SWIND_PRICE_FIELDS) {
            swind_price_field = 0;
        }
    }
    record = game_state.itemInfo + swind_price_index;
    FntPrint("\n\n\n");
    FntPrint("EDIT. %d\n", swind_price_index);
    if (swind_price_field == 0) {
        FntPrint(" *");
        record->buyPrice = swind_pad_step(record->buyPrice);
    } else {
        FntPrint("  ");
    }
    FntPrint(" BUY   %d\n", record->buyPrice);
    if (swind_price_field == 1) {
        FntPrint(" *");
        record->sellPrice = swind_pad_step(record->sellPrice);
    } else {
        FntPrint("  ");
    }
    FntPrint(" SELL  %d\n", record->sellPrice);
    if (swind_price_field == 2) {
        FntPrint(" *");
        record->stamina = swind_pad_step(record->stamina);
    } else {
        FntPrint("  ");
    }
    FntPrint(" LIFE  %d\n", record->stamina);
    if (swind_price_field == 3) {
        FntPrint(" *");
        record->fatigue = swind_pad_step(record->fatigue);
    } else {
        FntPrint("  ");
    }
    FntPrint(" TIRED %d\n", record->fatigue);
    return page;
}

/* swind_tool_stamina_page's tools, in the order of tool_stamina_costs's rows. */
static char *swind_tool_names[SWIND_TOOL_COUNT] = { "SICKLE", "HOE", "AX", "HAMMER", "WATER_CAN" };

/* The window's state, the page cursor, sounds and program arguments. */
static s32 swind_closable = 0;
static s32 swind_page = 0;
static s32 swind_page_state = 0;
static s32 swind_main_cursor = 0;
static s32 swind_midi_se_no = 0;
static s32 swind_mgame_se_no = 0;
static s32 swind_bgm_no = 0;
static s32 swind_xa_no = 0;
static s32 swind_sound_voice = 0;
static s32 swind_vibration_no = 0;
static s16 swind_get_item_no = 0;
static s32 swind_rival_mode = SWIND_RIVAL_KAREN;
static s16 swind_time_advance = 0;
static s32 swind_state = SWIND_STATE_OPEN;
static s32 swind_row_kind = 0;

/* Tool stamina editor: LEFT/RIGHT pick the tool, UP/DOWN the level row, and swind_pad_step
 * edits tool_stamina_costs[tool][level]. */
static s32 swind_tool_stamina_page(s32 page) {
    s32 value;
    s32 i;

    if (input_pad1->triggered & PAD_RIGHT) {
        value = swind_tool_index + 1;
        swind_tool_index = value;
        if (value >= SWIND_TOOL_COUNT) {
            swind_tool_index = 0;
        }
    }
    if (input_pad1->triggered & PAD_LEFT) {
        value = swind_tool_index - 1;
        swind_tool_index = value;
        if (value < 0) {
            swind_tool_index = SWIND_TOOL_LAST;
        }
    }
    if (input_pad1->triggered & PAD_UP) {
        value = swind_tool_level - 1;
        swind_tool_level = value;
        if (value < 0) {
            swind_tool_level = SWIND_TOOL_LAST;
        }
    }
    if (input_pad1->triggered & PAD_DOWN) {
        value = swind_tool_level + 1;
        swind_tool_level = value;
        if (value >= SWIND_TOOL_COUNT) {
            swind_tool_level = 0;
        }
    }
    FntPrint("\n\n\n");
    FntPrint("No. %d %s\n", swind_tool_index, swind_tool_names[swind_tool_index]);
    for (i = 0; i < SWIND_TOOL_COUNT; i++) {
        if (i == swind_tool_level) {
            tool_stamina_costs[swind_tool_index][i] = swind_pad_step(tool_stamina_costs[swind_tool_index][i]);
            FntPrint(" *");
        } else {
            FntPrint("  ");
        }
        FntPrint(" PRM %d\n", tool_stamina_costs[swind_tool_index][i]);
    }
    return page;
}

/* One nine-row page of an item editor: each row's mark, label and value. Up/down move
 * a row, left/right a page. Circle/cross step a value by 1/-1, triangle/square by 10/-10
 * (100/-100 with SELECT held, 1000/-1000 with pad 2's R1 held). SELECT publishes the
 * row kind in swind_row_kind. */
static void swind_item_list(SwindItem *items, s32 count)
{
    char *marks[2] = { "\201E", "\201\226" }; /* "・" editable, "＊" read-only */
    s32 i;
    s32 triggered;
    s32 readOnly;

    for (i = swind_cursor / SWIND_ROWS_PER_PAGE * SWIND_ROWS_PER_PAGE; i < swind_cursor / SWIND_ROWS_PER_PAGE * SWIND_ROWS_PER_PAGE + SWIND_ROWS_PER_PAGE; i++)
    {
        if (i < count)
        {
            readOnly = items[i].kind == SWIND_ITEM_READ_ONLY;
            switch (items[i].size)
            {
            case SWIND_WIDTH_NONE:
            default:
                KanjiFntPrint(ROW_LABEL_FORMAT, ROW_CURSOR(i), marks[readOnly], items[i].label);
                break;
            case SWIND_WIDTH_BYTE:
                if (items[i].min == 0)
                    KanjiFntPrint(ROW_VALUE_FORMAT, ROW_CURSOR(i), marks[readOnly], items[i].label, *(u8 *)items[i].data);
                else
                    KanjiFntPrint(ROW_VALUE_FORMAT, ROW_CURSOR(i), marks[readOnly], items[i].label, *(s8 *)items[i].data);
                break;
            case SWIND_WIDTH_HALF:
                if (items[i].min == 0)
                    KanjiFntPrint(ROW_VALUE_FORMAT, ROW_CURSOR(i), marks[readOnly], items[i].label, *(u16 *)items[i].data);
                else
                    KanjiFntPrint(ROW_VALUE_FORMAT, ROW_CURSOR(i), marks[readOnly], items[i].label, *(s16 *)items[i].data);
                break;
            case SWIND_WIDTH_WORD:
                if (items[i].min == 0)
                    KanjiFntPrint(ROW_VALUE_FORMAT, ROW_CURSOR(i), marks[readOnly], items[i].label, *(u32 *)items[i].data);
                else
                    KanjiFntPrint(ROW_VALUE_FORMAT, ROW_CURSOR(i), marks[readOnly], items[i].label, *(s32 *)items[i].data);
                break;
            }
        }
        else
        {
            KanjiFntPrint("\n");
        }
    }

    triggered = input_pad1->triggered;
    if (triggered & PAD_UP)
    {
        swind_cursor = swind_cursor - 1;
        if (swind_cursor < 0)
            swind_cursor = count - 1;
        sound_play(SOUND_MENU_CURSOR);
        return;
    }
    if (triggered & PAD_DOWN)
    {
        swind_cursor = swind_cursor + 1;
        if (swind_cursor > count - 1)
            swind_cursor = 0;
        sound_play(SOUND_MENU_CURSOR);
        return;
    }
    if (triggered & PAD_LEFT)
    {
        swind_cursor = swind_cursor - 9;
        if (swind_cursor < 0)
            swind_cursor = 0;
        sound_play(SOUND_MENU_CURSOR);
        return;
    }
    if (triggered & PAD_RIGHT)
    {
        swind_cursor = swind_cursor + 9;
        if (swind_cursor > count - 1)
            swind_cursor = count - 1;
        sound_play(SOUND_MENU_CURSOR);
        return;
    }
    if (input_pad1->pressed & PAD_SELECT)
    {
        swind_row_kind = items[swind_cursor].kind;
        return;
    }
    if (items[swind_cursor].kind == -2)
        return;
    if (triggered & PAD_CIRCLE)
        swind_step_value(items[swind_cursor].data, items[swind_cursor].size, items[swind_cursor].limit, items[swind_cursor].min, 1);
    else if (triggered & PAD_CROSS)
        swind_step_value(items[swind_cursor].data, items[swind_cursor].size, items[swind_cursor].limit, items[swind_cursor].min, -1);
    else if (triggered & PAD_TRIANGLE)
    {
        if (input_pad1->held & PAD_SELECT)
            swind_step_value(items[swind_cursor].data, items[swind_cursor].size, items[swind_cursor].limit, items[swind_cursor].min, 100);
        else if (input_pad2->held & PAD_R1)
            swind_step_value(items[swind_cursor].data, items[swind_cursor].size, items[swind_cursor].limit, items[swind_cursor].min, 1000);
        else
            swind_step_value(items[swind_cursor].data, items[swind_cursor].size, items[swind_cursor].limit, items[swind_cursor].min, 10);
    }
    else if (triggered & PAD_SQUARE)
    {
        if (input_pad1->held & PAD_SELECT)
            swind_step_value(items[swind_cursor].data, items[swind_cursor].size, items[swind_cursor].limit, items[swind_cursor].min, -100);
        else if (input_pad2->held & PAD_R1)
            swind_step_value(items[swind_cursor].data, items[swind_cursor].size, items[swind_cursor].limit, items[swind_cursor].min, -1000);
        else
            swind_step_value(items[swind_cursor].data, items[swind_cursor].size, items[swind_cursor].limit, items[swind_cursor].min, -10);
    }
}

/* Step a 1-, 2- or 4-byte value by delta, wrapping to min above max and to
 * max below min. min == 0 selects an unsigned read. */
static void swind_step_value(void *value, s32 size, s32 max, s32 min, s32 delta) {
    s32 current;

    if (size == SWIND_WIDTH_NONE) {
        return;
    }
    current = 0;
    switch (size) {
    case SWIND_WIDTH_BYTE:
        if (min == 0) {
            current = *(u8 *)value;
        } else {
            current = *(s8 *)value;
        }
        break;
    case SWIND_WIDTH_HALF:
        if (min == 0) {
            current = *(u16 *)value;
        } else {
            current = *(s16 *)value;
        }
        break;
    case SWIND_WIDTH_WORD:
        current = *(s32 *)value;
        break;
    }
    current += delta;
    if (max < 0) {
        if (current > max) {
            current = min;
        } else if (current < min) {
            current = max;
        }
    } else {
        if (current > max) {
            current = min;
        } else if (current < min) {
            current = max;
        }
    }
    switch (size) {
    case SWIND_WIDTH_BYTE:
        *(u8 *)value = current;
        break;
    case SWIND_WIDTH_HALF:
        *(u16 *)value = current;
        break;
    case SWIND_WIDTH_WORD:
        *(s32 *)value = current;
        break;
    }
}

/* Draw one page (nine rows) of a flag editor and handle its input: up/down move
 * the cursor by one row, left/right by a page (with the cursor sound). With PAD_SELECT
 * pressed the cursor row's id is published in swind_row_kind; otherwise PAD_CIRCLE sets
 * and PAD_CROSS clears the row's bit. A width of 0 means the row has no variable ("NULL"). */
static void swind_flag_list(SwindFlagEntry *entries, s32 count) {
    char *marks[2] = { "\201E", "\201\226" }; /* "・" editable, "＊" readOnly */
    SwindFlagEntry *row;
    SwindFlagEntry *entry;
    s32 readOnly;
    s32 i;
    s32 triggered;

    for (i = swind_cursor / SWIND_ROWS_PER_PAGE * SWIND_ROWS_PER_PAGE; i < swind_cursor / SWIND_ROWS_PER_PAGE * SWIND_ROWS_PER_PAGE + SWIND_ROWS_PER_PAGE; i++) {
        row = &entries[i];
        if (i < count) {
            readOnly = row->id == -2;
            switch (row->width) {
            case SWIND_WIDTH_NONE:
            default:
                KanjiFntPrint(ROW_LABEL_FORMAT, ROW_CURSOR(i),
                              marks[readOnly], row->label);
                break;
            case SWIND_WIDTH_BYTE:
                if (*(u8 *)row->target & row->mask) {
                    KanjiFntPrint(ROW_VALUE_FORMAT, ROW_CURSOR(i),
                                  marks[readOnly], row->label, 1);
                } else {
                    KanjiFntPrint(ROW_VALUE_FORMAT, ROW_CURSOR(i),
                                  marks[readOnly], row->label, 0);
                }
                break;
            case SWIND_WIDTH_HALF:
                if (*(s16 *)row->target & row->mask) {
                    KanjiFntPrint(ROW_VALUE_FORMAT, ROW_CURSOR(i),
                                  marks[readOnly], row->label, 1);
                } else {
                    KanjiFntPrint(ROW_VALUE_FORMAT, ROW_CURSOR(i),
                                  marks[readOnly], row->label, 0);
                }
                break;
            case SWIND_WIDTH_WORD:
                if (*(s32 *)row->target & row->mask) {
                    KanjiFntPrint(ROW_VALUE_FORMAT, ROW_CURSOR(i),
                                  marks[readOnly], row->label, 1);
                } else {
                    KanjiFntPrint(ROW_VALUE_FORMAT, ROW_CURSOR(i),
                                  marks[readOnly], row->label, 0);
                }
                break;
            }
        } else {
            KanjiFntPrint("\n");
        }
    }

    triggered = input_pad1->triggered;
    if (triggered & PAD_UP) {
        swind_cursor--;
        if (swind_cursor < 0) {
            swind_cursor = count - 1;
        }
        sound_play(SOUND_MENU_CURSOR);
    } else if (triggered & PAD_DOWN) {
        swind_cursor++;
        if (swind_cursor > count - 1) {
            swind_cursor = 0;
        }
        sound_play(SOUND_MENU_CURSOR);
    } else if (triggered & PAD_LEFT) {
        swind_cursor -= 9;
        if (swind_cursor < 0) {
            swind_cursor = 0;
        }
        sound_play(SOUND_MENU_CURSOR);
    } else if (triggered & PAD_RIGHT) {
        swind_cursor += 9;
        if (swind_cursor > count - 1) {
            swind_cursor = count - 1;
        }
        sound_play(SOUND_MENU_CURSOR);
    } else if (input_pad1->pressed & PAD_SELECT) {
        swind_row_kind = entries[swind_cursor].id;
    } else if (entries[swind_cursor].id != SWIND_ITEM_READ_ONLY) {
        entry = &entries[swind_cursor];
        if (triggered & PAD_CIRCLE) {
            switch (entry->width) {
            case SWIND_WIDTH_NONE:
                break;
            case SWIND_WIDTH_BYTE:
                *(u8 *)entry->target |= entry->mask;
                break;
            case SWIND_WIDTH_HALF:
                *(u16 *)entry->target |= entry->mask;
                break;
            case SWIND_WIDTH_WORD:
                *(s32 *)entry->target |= entry->mask;
                break;
            }
        } else if (triggered & PAD_CROSS) {
            switch (entry->width) {
            case SWIND_WIDTH_NONE:
                break;
            case SWIND_WIDTH_BYTE:
                *(u8 *)entry->target &= ~entry->mask;
                break;
            case SWIND_WIDTH_HALF:
                *(u16 *)entry->target &= ~entry->mask;
                break;
            case SWIND_WIDTH_WORD:
                *(s32 *)entry->target &= ~entry->mask;
                break;
            }
        }
    }
}

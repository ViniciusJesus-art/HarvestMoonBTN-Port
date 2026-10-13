/* Overlay 08_DEBUG: the debug jump menu (scenes, the farm houses and save/load)
 * and the pad vibration test. The strings are Shift-JIS; each one's comment gives
 * the decoded text. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "types.h"
#include "debug.h"
#include "farm_state.h"
#include "main.h"
#include "pad.h"
#include "pad_vibration.h"
#include "screen_fade.h"
#include "shop_restock.h"
#include "sound_request.h"

/* One row of the debug jump menu: the id to jump to (-1: no entry) and its label. */
typedef struct DebugMenuEntry {
    s32 id;
    const char *name;
} DebugMenuEntry;

#define DEBUG_MENU_ROWS 32
#define DEBUG_MENU_PAGE 6
#define DEBUG_ROW_EMPTY -1
#define DEBUG_VIBRATION_COUNT 11
#define DEBUG_BG_R 0x40
#define DEBUG_BG_G 0x40
#define DEBUG_BG_B 0x80

static void debug_screen_setup(void);
static void debug_wait_ready(void);
static void debug_menu_draw(void);
static void debug_select_tick(void);

/* debug_tick's handlers, indexed by debug_step: set the screen up, wait until
   sound_fader_ready() reports true, then run the menu every frame. */
static void (*const debug_handlers[3])(void) = {
    debug_screen_setup,
    debug_wait_ready,
    debug_menu_draw,
};

/* debug_tick's handler index (into debug_handlers) and the menu cursor. */
static s32 debug_step = 0;
static s32 debug_cursor = 0;

/* The rows of debug_menu that choose a season house (see debug_menu_draw). */
enum DebugMenuRow {
    DEBUG_ROW_SPRING_HOUSE = 13,
    DEBUG_ROW_SUMMER_HOUSE = 14,
    DEBUG_ROW_AUTUMN_HOUSE = 15,
    DEBUG_ROW_WINTER_HOUSE = 16,
};

/* The menu, in display order. Rows 13..16 are the four season houses
   debug_menu_draw flags on selection. The unused rows at the end all carry the
   same dash label. */
static DebugMenuEntry debug_menu[DEBUG_MENU_ROWS] = {
    {SCENE_DEBUG, "\202c\202d\202a\202t\202f"}, /* ＤＥＢＵＧ — "DEBUG" */
    {SCENE_PDA_SAMPLE, "\202o\202c\202`\203T\203\223\203v\203\213"}, /* ＰＤＡサンプル — "PDA sample" */
    {SCENE_TITLE, "\216\251\225\252\202\314\211\306"}, /* 自分の家 — "own house" */
    {SCENE_MG1, "\203~\203j\203Q\201[\203\20001\201F\221\220\213\243\224n"}, /* ミニゲーム01：草競馬 — "minigame 01: horse race" */
    {SCENE_MG2, "\203~\203j\203Q\201[\203\20002\201F\223\254\214\173"}, /* ミニゲーム02：闘鶏 — "minigame 02: cockfight" */
    {SCENE_MG3, "\203~\203j\203Q\201[\203\20003\201F\203g\203\175\203g"}, /* ミニゲーム03：トマト — "minigame 03: tomato" */
    {SCENE_FARM_MINIGAME_EXIT, "\203q\201E\203~\201E\203c\201|\201|\201|\201|\201|"}, /* ヒ・ミ・ツ－－－－－ — "se-cre-t", spelled out */
    {SCENE_SAVE, "\203Z\201[\203u"}, /* セーブ — "save" */
    {SCENE_LOAD, "\203\215\201[\203h"}, /* ロード — "load" */
    {SCENE_PDA, "\202o\202c\202`"}, /* ＰＤＡ — "PDA" */
    {SCENE_LIVESTOCK_TRADE, "\211\306\222\173\214\360\212\267"}, /* 家畜交換 — "livestock trade" */
    {SCENE_MG4, "\203~\203j\203Q\201[\203\20004\201F\214\242"}, /* ミニゲーム04：犬 — "minigame 04: dog" */
    {SCENE_MG5, "\203~\203j\203Q\201[\203\20005\201F\213\243\211j"}, /* ミニゲーム05：競泳 — "minigame 05: swimming race" */
    {SCENE_TITLE, "\217t\202\314\211\306"}, /* 春の家 — "spring house" */
    {SCENE_TITLE, "\211\304\202\314\211\306"}, /* 夏の家 — "summer house" */
    {SCENE_TITLE, "\217H\202\314\211\306"}, /* 秋の家 — "autumn house" */
    {SCENE_TITLE, "\223~\202\314\211\306"}, /* 冬の家 — "winter house" */
    {SCENE_TITLE, "\203^\203C\203g\203\213"}, /* タイトル — "title" */
    {SCENE_STAFF_ROLL, "\203X\203^\203b\203t\203\215\201[\203\213"}, /* スタッフロール — "staff roll" */
    {SCENE_MG_DEBUG, "\202l\202f\203f\203o\203b\203O\211\346\226\312"}, /* ＭＧデバッグ画面 — "MG debug screen" */
    {SCENE_MEMCARD, "\203\201\203\202\203\212\201[\203J\201[\203h\211]\201X"}, /* メモリーカード云々 — "memory card and so on" */
    {SCENE_FARM_MENU_RETURN, "\223\344"}, /* 謎 — "mystery" */
    {SCENE_FARM_STAFF_EXIT, "\223\3442"}, /* 謎2 — "mystery 2" */
    {DEBUG_ROW_EMPTY, "\201|\201|\201|\201|\201|\201|\201|\201|\201|\201|"}, /* －－－－－－－－－－ — a row of dashes, the label of every unused row */
    {DEBUG_ROW_EMPTY, "\201|\201|\201|\201|\201|\201|\201|\201|\201|\201|"},
    {DEBUG_ROW_EMPTY, "\201|\201|\201|\201|\201|\201|\201|\201|\201|\201|"},
    {DEBUG_ROW_EMPTY, "\201|\201|\201|\201|\201|\201|\201|\201|\201|\201|"},
    {DEBUG_ROW_EMPTY, "\201|\201|\201|\201|\201|\201|\201|\201|\201|\201|"},
    {DEBUG_ROW_EMPTY, "\201|\201|\201|\201|\201|\201|\201|\201|\201|\201|"},
    {DEBUG_ROW_EMPTY, "\201|\201|\201|\201|\201|\201|\201|\201|\201|\201|"},
    {DEBUG_ROW_EMPTY, "\201|\201|\201|\201|\201|\201|\201|\201|\201|\201|"},
    {DEBUG_ROW_EMPTY, "\201|\201|\201|\201|\201|\201|\201|\201|\201|\201|"},
};

/* The vibration test's selection, 0..DEBUG_VIBRATION_COUNT - 1. */
static s32 debug_vibration_index;

void debug_tick(void) {
    debug_handlers[debug_step]();
}

/* Set up the debug screen: the font and fade, both frames' backgrounds, and a shop restock. */
static void debug_screen_setup(void) {
    FrameState *framebuffer;

    game_state.progress.entrySubStep = 0;
    KanjiFntClose();
    KanjiFntOpen(8, 0, 320, 240, 448, 0, 320, 255, 0, 255);
    fade_start_in(1, 0, 2);
    framebuffer = frame_records;
    framebuffer->drawEnv.r0 = DEBUG_BG_R;
    framebuffer->drawEnv.g0 = DEBUG_BG_G;
    framebuffer->drawEnv.b0 = DEBUG_BG_B;
    framebuffer++;
    framebuffer->drawEnv.r0 = DEBUG_BG_R;
    framebuffer->drawEnv.g0 = DEBUG_BG_G;
    framebuffer->drawEnv.b0 = DEBUG_BG_B;
    shop_restock();
    debug_step++;
}

/* Wait until the sound fader is ready, then move on to the menu. */
static void debug_wait_ready(void) {
    if (sound_fader_ready()) {
        debug_step++;
    }
}

/* Draw the six-row page of the debug menu around the cursor, then move the
   cursor with the d-pad (up/down one row, left/right one page) and act on
   the selected row. */
static void debug_menu_draw(void) {
    s32 i;

    /* \n\n　　　　　ＤＥＢＵＧ　ＭＯＤＥ\n — title line, "DEBUG MODE" */
    KanjiFntPrint("\n\n\201@\201@\201@\201@\201@\202c\202d\202a\202t\202f\201@\202l\202n\202c\202d\n");
    /* 　　　　　「プログラム 選択」\n\n — "[program select]" */
    KanjiFntPrint("\201@\201@\201@\201@\201@\201u\203v\203\215\203O\203\211\203\200 \221I\221\360\201v\n\n");
    for (i = debug_cursor / DEBUG_MENU_PAGE * DEBUG_MENU_PAGE;
         i < debug_cursor / DEBUG_MENU_PAGE * DEBUG_MENU_PAGE + DEBUG_MENU_PAGE; i++) {
        if (i >= 0 && i < DEBUG_MENU_ROWS) {
            /* 　%s　　%2d・%s\n — one row: cursor (＞ or 　), row number, label */
            KanjiFntPrint("\201@%s\201@\201@%2d\201E%s\n", i == debug_cursor ? "\201\204" : "\201@", i,
                          debug_menu[i].name);
        } else {
            KanjiFntPrint("\n"); /* an empty row */
        }
    }
    /* \n   ＳＴＡＲＴ ＝プログラム実行 — footer, "START = run program" */
    KanjiFntPrint("\n   \202r\202s\202`\202q\202s \201\201\203v\203\215\203O\203\211\203\200\216\300\215s");

    if (fade_is_settled() != 0) {
        if (input_pad1->triggered & PAD_UP) {
            debug_cursor--;
            if (debug_cursor < 0) {
                debug_cursor = DEBUG_MENU_ROWS - 1;
            }
        } else if (input_pad1->triggered & PAD_DOWN) {
            debug_cursor++;
            if (debug_cursor < 0 || debug_cursor >= DEBUG_MENU_ROWS) {
                debug_cursor = 0;
            }
        } else if (input_pad1->triggered & PAD_LEFT) {
            debug_cursor -= DEBUG_MENU_PAGE;
            if (debug_cursor < 0) {
                debug_cursor = 0;
            }
        } else if (input_pad1->triggered & PAD_RIGHT) {
            debug_cursor += DEBUG_MENU_PAGE;
            if (debug_cursor < 0 || debug_cursor >= DEBUG_MENU_ROWS) {
                debug_cursor = DEBUG_MENU_ROWS - 1;
            }
        } else if (input_pad1->pressed & PAD_CIRCLE) {
            if (debug_menu[debug_cursor].id != DEBUG_ROW_EMPTY) {
                main_request_scene(debug_menu[debug_cursor].id);
                switch (debug_cursor) {
                case DEBUG_ROW_SPRING_HOUSE:
                    game_state.debugFlags |= DEBUG_FLAG_SPRING_HOUSE;
                    break;
                case DEBUG_ROW_SUMMER_HOUSE:
                    game_state.debugFlags |= DEBUG_FLAG_SUMMER_HOUSE;
                    break;
                case DEBUG_ROW_AUTUMN_HOUSE:
                    game_state.debugFlags |= DEBUG_FLAG_AUTUMN_HOUSE;
                    break;
                case DEBUG_ROW_WINTER_HOUSE:
                    game_state.debugFlags |= DEBUG_FLAG_WINTER_HOUSE;
                    break;
                }
            }
        }
        KanjiFntFlush(-1);
    }
}

/* Vibration test on the pads: R1 and R2 step the motor index, cross starts the
 * motor and square stops it. Unused: not in debug_handlers, so the menu never
 * reaches it. */
static void debug_select_tick(void) {
    if (input_pad1->pressed & PAD_R1) {
        debug_vibration_index++;
        debug_vibration_index = (debug_vibration_index < DEBUG_VIBRATION_COUNT) ? debug_vibration_index : 0;
    }
    if (input_pad1->pressed & PAD_R2) {
        debug_vibration_index--;
        debug_vibration_index = (debug_vibration_index < 0) ? DEBUG_VIBRATION_COUNT - 1 : debug_vibration_index;
    }
    if (input_pad1->pressed & PAD_CROSS) {
        pad_vibration_start(0, debug_vibration_index);
    }
    if (input_pad1->pressed & PAD_SQUARE) {
        pad_vibration_stop(0);
    }
    FntPrint("PADNO=%d", debug_vibration_index);
}

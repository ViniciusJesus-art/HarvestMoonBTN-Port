/* The UI icons: the sprite sheet they are drawn from and the numbered icons. */
#include "types.h"
#include "anim.h"
#include "farm_state.h"
#include "ui_icons.h"

static void ui_draw_icon(u_long *ot, s32 icon, s32 frame, s16 x, s16 y);

u8 ui_icon_cycle;
/* Frame counter kept as a seconds field: 0..59. */
u8 ui_icon_tick;

/* The blink cycle is UI_BLINK_CYCLE steps, and the icons change phase every UI_BLINK_PHASE. */
#define UI_BLINK_CYCLE 120
#define UI_BLINK_PHASE 30
#define UI_TICK_SECONDS 60

void ui_draw_icon_2a(u_long *ot, signed short x, signed short y) {
    ui_draw_icon(ot, 0x2A, 0, x, y);
}

void ui_draw_icon_27_0(u_long *ot, short x, short y) {
    ui_draw_icon(ot, 0x27, 0, x, y);
}

void ui_draw_icon_27_2(u_long *ot, signed short x, signed short y) {
    ui_draw_icon(ot, 0x27, 2, x, y);
}

void ui_draw_icon_27_1(u_long *ot, signed short x, signed short y) {
    ui_draw_icon(ot, 0x27, 1, x, y);
}

void ui_draw_icon_27_3(u_long *ot, signed short x, signed short y) {
    ui_draw_icon(ot, 0x27, 3, x, y);
}

/* Advance the blink cycle one step, or reset it when advance is -1. */
void ui_blink_tick(signed short advance) {
    if (advance != -1) {
        ui_icon_cycle = ui_icon_cycle + 1;
        ui_icon_cycle = ui_icon_cycle - ((ui_icon_cycle / UI_BLINK_CYCLE) * UI_BLINK_CYCLE);
    } else {
        ui_icon_cycle = 0;
    }
}

void ui_draw_icon_29_phase(u_long *ot, signed short x, signed short y) {
    ui_draw_icon(ot, 0x29, (ui_icon_cycle / UI_BLINK_PHASE) & 0xFF, x, y);
}

void ui_draw_icon_28_phase(u_long *ot, signed short x, signed short y) {
    ui_draw_icon(ot, 0x28, (ui_icon_cycle / UI_BLINK_PHASE) & 0xFF, x, y);
}

void ui_draw_icon_0d_phase(u_long *ot, signed short x, signed short y) {
    ui_draw_icon(ot, 0xD, (ui_icon_cycle / UI_BLINK_PHASE) & 0xFF, x, y);
}

void ui_draw_icon_0c_phase(u_long *ot, signed short x, signed short y) {
    ui_draw_icon(ot, 0xC, (ui_icon_cycle / UI_BLINK_PHASE) & 0xFF, x, y);
}

void ui_draw_icon_0b_1(u_long *ot, signed short x, signed short y) {
    ui_draw_icon(ot, 0xB, 1, x, y);
}

void ui_draw_icon_18_sub(u_long *ot, s16 x, s16 y, s16 sub) {
    ui_draw_icon(ot, 0x18, sub, x, y);
}

void ui_draw_icon_1e(u_long *ot, s16 x, s16 y) {
    ui_draw_icon(ot, 0x1E, 0, x, y);
}

/* Advance the seconds tick, or reset it when advance is -1. */
void ui_blink_tick_60(signed short advance) {
    if (advance != -1) {
        ui_icon_tick = (ui_icon_tick + 1) & 0xFF;
        ui_icon_tick = ui_icon_tick - ((ui_icon_tick / UI_TICK_SECONDS) * UI_TICK_SECONDS);
    } else {
        ui_icon_tick = 0;
    }
}

void ui_draw_icon_07_phase(u_long *ot, signed short x, signed short y) {
    ui_draw_icon(ot, 7, (ui_icon_tick / 30) & 0xFF, x, y);
}

void ui_draw_icon_2b_sub(u_long *ot, s16 x, s16 y, s16 sub) {
    ui_draw_icon(ot, 0x2B, sub, x, y);
}

AnimState ui_icon_anim;

/* Draw one UI icon from the shared icon animation, binding its file on first use. */
static void ui_draw_icon(u_long *ot, s32 icon, s32 frame, s16 x, s16 y) {
    if (ui_icon_anim.source != (AnimFile *)farm_area.stageData) {
        anim_bind_file(&ui_icon_anim, (AnimFile *)farm_area.stageData);
    }
    anim_draw_sprt_cursor(ot, &ui_icon_anim, icon & 0xFF, frame & 0xFF, x, y);
}

#ifndef UI_ICONS_H
#define UI_ICONS_H


/* Numbered icons of the UI sprite sheet. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "anim.h"

void ui_draw_icon_2a(u_long *ot, s16 x, s16 y);
void ui_draw_icon_27_0(u_long *ot, s16 x, s16 y);
void ui_draw_icon_27_2(u_long *ot, s16 x, s16 y);
void ui_draw_icon_27_1(u_long *ot, s16 x, s16 y);
void ui_draw_icon_27_3(u_long *ot, s16 x, s16 y);
void ui_blink_tick(s16 advance);
void ui_draw_icon_29_phase(u_long *ot, s16 x, s16 y);
void ui_draw_icon_28_phase(u_long *ot, s16 x, s16 y);
void ui_draw_icon_0d_phase(u_long *ot, s16 x, s16 y);
void ui_draw_icon_0c_phase(u_long *ot, s16 x, s16 y);
void ui_draw_icon_0b_1(u_long *ot, s16 x, s16 y);
void ui_draw_icon_18_sub(u_long *ot, s16 x, s16 y, s16 sub);
void ui_draw_icon_1e(u_long *ot, s16 x, s16 y);
void ui_blink_tick_60(s16 advance);
void ui_draw_icon_07_phase(u_long *ot, s16 x, s16 y);
void ui_draw_icon_2b_sub(u_long *ot, s16 x, s16 y, s16 sub);

/* The shared sprite every "draw one item" path stages into. */
extern AnimState ui_icon_anim;

#endif /* UI_ICONS_H */

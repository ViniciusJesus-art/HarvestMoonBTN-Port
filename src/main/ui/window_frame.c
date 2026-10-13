#include "types.h"
#include "prim.h"
#include "window_frame.h"

/* Edge colours, 0xBBGGRR. */
#define FRAME_LIGHT 0xFDECD2
#define FRAME_DARK 0xBA8B47

/* Draw a bevelled window frame at (x, y), w by h pixels: corner pixels, a
 * light and a dark edge pair on each side, and a gradient-filled interior. */
void window_frame_draw(u_long *ot, s16 x, s16 y, s16 w, s16 h) {
    s16 right;
    s16 outer_right;
    s16 inner_right;
    s16 bottom;
    s16 outer_bottom;
    s16 left;
    s16 top;

    w--;
    h--;
    right = x + (w - 1);
    prim_add_pixel(ot, right, y, 0xF8DBAF, 0);
    top = y + 1;
    prim_add_pixel(ot, right, top, 0xDEBB84, 0);
    outer_right = x + w;
    prim_add_pixel(ot, outer_right, top, 0xC89F60, 0);
    left = x + 1;
    bottom = y + (h - 1);
    prim_add_pixel(ot, left, bottom, 0xE2C08B, 0);
    outer_bottom = y + h;
    prim_add_pixel(ot, left, outer_bottom, 0xC89F60, 0);
    prim_add_pixel(ot, x, bottom, 0xF8DBAF, 0);
    inner_right = x + (w - 2);
    prim_add_line(ot, left, top, inner_right, top, FRAME_LIGHT, 0);
    prim_add_line(ot, left, top, left, bottom, FRAME_LIGHT, 0);
    prim_add_line(ot, right, top, right, bottom, FRAME_DARK, 0);
    prim_add_line(ot, left, bottom, inner_right, bottom, FRAME_DARK, 0);
    prim_add_line(ot, left, y, right, y, FRAME_LIGHT, 0);
    prim_add_line(ot, x, top, x, bottom, FRAME_LIGHT, 0);
    prim_add_line(ot, outer_right, top, outer_right, bottom, FRAME_DARK, 0);
    prim_add_line(ot, left, outer_bottom, right, outer_bottom, FRAME_DARK, 0);
    prim_add_gouraud_rect(ot, x + 2, y + 2, w - 3, h - 3, 0xF4DFBD, 0xF4DFBD, 0xF9CF8F, 0xF9CF8F, 0);
}

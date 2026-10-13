/* The background tile screens: tiling the screen with the staging sprite and its draw callbacks. */
#include <sys/types.h>
#include <libgte.h>
#include "types.h"
#include "anim.h"
#include "bg_tile.h"
#include "farm_state.h"
#include "main.h"
#include "prim.h"
#include "ui_icons.h"

typedef void (*TileDrawFn)(u_long *ot, AnimState *sprite, s16 frame, s32 x, s32 y);

static void bg_tile_screen(u_long *ot, void *drawArg);
static void bg_tile_draw_sprite(u_long *ot, struct AnimState *sprite, int frame, short x, short y);
static void bg_tile_draw_cursor(u_long *ot, struct AnimState *sprite, signed short frame, signed short x, signed short y);

/* Tile the screen with plain sprites of the staging sprite. */
void bg_tile_mode_sprite(u_long *arg) {
    bg_tile_screen(arg, bg_tile_draw_sprite);
}

/* Tile the screen with the staging sprite drawn through the frame's primitive cursor. */
void bg_tile_mode_cursor(u_long *arg) {
    bg_tile_screen(arg, bg_tile_draw_cursor);
}

static u32 bg_scroll_frames = 0;

static s16 bg_tile_frame_order[] = {0, 2, 1, 3};

/* Tile the screen with the staging sprite on a 24-pixel grid, scrolling it
 * diagonally one pixel every four calls and cycling frames 0,2,1,3. */
static void bg_tile_screen(u_long *ot, void *drawArg) {
    TileDrawFn draw;
    u32 y;
    s32 x;
    s32 scroll;
    s32 frame;

    anim_bind_file(&ui_icon_anim, (AnimFile *)farm_area.stageData);
    bg_scroll_frames++;
    scroll = bg_scroll_frames / 4 - bg_scroll_frames / 96 * 24;
    frame = 0;
    draw = drawArg;
    for (y = 0; y < 264; y += 24) {
        for (x = -12; x < 344; x += 24) {
            frame++;
            frame &= 3;
            draw(ot, &ui_icon_anim, bg_tile_frame_order[frame], x - scroll, y - scroll);
        }
    }
}

/* Draw one tile as a plain sprite of the staging animation. */
static void bg_tile_draw_sprite(u_long *ot, struct AnimState *sprite, int frame, short x, short y) {
    anim_draw_sprt_cursor(ot, sprite, 0x1D, frame & 0xFF, x, y);
}

/* Draw one tile through the current frame's primitive cursor. */
static void bg_tile_draw_cursor(u_long *ot, struct AnimState *sprite, signed short frame, signed short x, signed short y) {
    PrimCursor *cursor;
    cursor = &current_frame->primCursor;
    anim_draw_sprt(ot, &cursor->sprt, &cursor->mode, sprite, 0x1D, frame & 0xFF, x, y);
}

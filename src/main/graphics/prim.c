/* Primitive builders: the wrappers that take a primitive from the prim buffer and fill in a tile, line or rectangle. */
#include "types.h"
#include "main.h"
#include "prim.h"

/* The GP0 command codes of the primitives built here; bit 1 makes one semi-transparent. */
#define PRIM_CODE_DOT 0x68
#define PRIM_CODE_LINE 0x40
#define PRIM_CODE_GOURAUD_RECT 0x38
#define PRIM_CODE_RECT 0x28
#define PRIM_CODE_TILE 0x60
#define PRIM_SEMI_TRANS 2

/* Add a primitive to `ot`. Each takes its packet from the work buffer (gs_work_alloc)
 * and links it in with its prim_make_* builder. */
void prim_add_pixel(u_long *ot, signed short x, signed short y, signed int color,
                    signed short semi_trans) {
    prim_make_pixel(gs_work_alloc(0xC), ot, x, y, color, semi_trans);
}

void prim_add_line(u_long *ot, signed short x0, signed short y0, signed short x1,
                   signed short y1, signed int color, signed short semi_trans) {
    prim_make_line(gs_work_alloc(0x10), ot, x0, y0, x1, y1, color, semi_trans);
}

void prim_add_gouraud_rect(u_long *ot, signed short x, signed short y, signed short w,
                           signed short h, signed int c0, signed int c1, signed int c2,
                           signed int c3, signed short semi_trans) {
    prim_make_gouraud_rect(gs_work_alloc(0x24), ot, x, y, w, h, c0,
                           c1, c2, c3, semi_trans);
}

void prim_add_rect(u_long *ot, s16 x, s16 y, s16 w, s16 h, s32 color, s16 semi_trans) {
    prim_make_rect(gs_work_alloc(0x18), ot, x, y, w, h, color, semi_trans);
}

void prim_add_tile(u_long *ot, s16 x, s16 y, s16 w, s16 h, s32 packed, s16 flag) {
    prim_make_tile(gs_work_alloc(0x10), ot, x, y, w, h, packed, flag);
}

void prim_add_rect_mode(u_long *ot, s16 x, s16 y, s16 w, s16 h, s32 color, s16 semi_trans, u8 flags) {
    gs_work_release(prim_make_rect_mode(gs_work_alloc(0), ot, x, y, w, h, color, semi_trans, flags));
}

/* The one-pixel TILE primitive. */
typedef struct Tile1 {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
} Tile1;

/* Build a one-pixel tile at (x, y) in colour 0xRRGGBB, optionally
 * semi-transparent, link it into `ot` and return the next free packet. */
Tile1 *prim_make_pixel(Tile1 *tile, u_long *ot, s16 x, s16 y, s32 color, s16 semi_trans) {
    setlen(tile, 2);
    tile->code = PRIM_CODE_DOT;
    tile->code = semi_trans ? tile->code | PRIM_SEMI_TRANS : tile->code & ~PRIM_SEMI_TRANS;
    tile->x0 = x;
    tile->y0 = y;
    tile->r0 = color >> 16;
    tile->g0 = color >> 8;
    tile->b0 = color;
    addPrim(ot, tile);
    return tile + 1;
}

/* The LINE_F2 primitive. */
typedef struct LineF2 {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 x1, y1;
} LineF2;

/* Build a flat line from (x0, y0) to (x1, y1) in colour 0xRRGGBB, optionally
 * semi-transparent, link it into `ot` and return the next free packet. */
LineF2 *prim_make_line(LineF2 *line, u_long *ot, s16 x0, s16 y0, s16 x1, s16 y1, s32 color,
                      s16 semi_trans) {
    setlen(line, 3);
    line->code = PRIM_CODE_LINE;
    line->code = semi_trans ? line->code | PRIM_SEMI_TRANS : line->code & ~PRIM_SEMI_TRANS;
    line->x0 = x0;
    line->y0 = y0;
    line->x1 = x1;
    line->y1 = y1;
    line->r0 = color >> 16;
    line->g0 = color >> 8;
    line->b0 = color;
    addPrim(ot, line);
    return line + 1;
}

/* The POLY_G4 primitive. */
typedef struct PolyG4 {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 r1, g1, b1, pad1;
    s16 x1, y1;
    u8 r2, g2, b2, pad2;
    s16 x2, y2;
    u8 r3, g3, b3, pad3;
    s16 x3, y3;
} PolyG4;

/* Build a Gouraud-shaded w x h rectangle at (x, y) with corner colours
 * 0xRRGGBB c0..c3, optionally semi-transparent, link it into `ot` and
 * return the next free packet. */
PolyG4 *prim_make_gouraud_rect(PolyG4 *poly, u_long *ot, s16 x, s16 y, s16 w, s16 h, s32 c0, s32 c1, s32 c2,
                      s32 c3, s16 semi_trans) {
    setlen(poly, 8);
    poly->code = PRIM_CODE_GOURAUD_RECT;
    poly->code = semi_trans ? poly->code | PRIM_SEMI_TRANS : poly->code & ~PRIM_SEMI_TRANS;
    poly->x0 = x;
    poly->y0 = y;
    poly->x1 = x + w;
    poly->y1 = y;
    poly->x2 = x;
    poly->y2 = y + h;
    poly->x3 = x + w;
    poly->y3 = y + h;
    poly->r0 = c0 >> 16;
    poly->g0 = c0 >> 8;
    poly->b0 = c0;
    poly->r1 = c1 >> 16;
    poly->g1 = c1 >> 8;
    poly->b1 = c1;
    poly->r2 = c2 >> 16;
    poly->g2 = c2 >> 8;
    poly->b2 = c2;
    poly->r3 = c3 >> 16;
    poly->g3 = c3 >> 8;
    poly->b3 = c3;
    addPrim(ot, poly);
    return poly + 1;
}

/* The POLY_F4 primitive. */
typedef struct PolyF4 {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 x1, y1;
    s16 x2, y2;
    s16 x3, y3;
} PolyF4;

/* Build a flat w x h rectangle at (x, y) in colour 0xRRGGBB, optionally
 * semi-transparent, link it into `ot` and return the next free packet. */
void *prim_make_rect(PolyF4 *poly, u_long *ot, s16 x, s16 y, s16 w, s16 h, s32 color, s16 semi_trans) {
    setlen(poly, 5);
    poly->code = PRIM_CODE_RECT;
    poly->r0 = color >> 16;
    poly->g0 = color >> 8;
    poly->b0 = color;
    poly->code = semi_trans ? poly->code | PRIM_SEMI_TRANS : poly->code & ~PRIM_SEMI_TRANS;
    poly->x0 = x;
    poly->y0 = y;
    poly->x1 = x + w;
    poly->y1 = y;
    poly->x2 = x;
    poly->y2 = y + h;
    poly->x3 = x + w;
    poly->y3 = y + h;
    addPrim(ot, poly);
    return poly + 1;
}

/* Build a w x h tile at (x, y) in colour 0xRRGGBB, optionally semi-transparent, link it
 * into `src` and return the next free packet. */
TILE *prim_make_tile(TILE *dst, u_long *src, s16 x, s16 y, s16 w, s16 h, s32 packed, s16 flag) {
    setlen(dst, 3);
    dst->code = PRIM_CODE_TILE;
    dst->r0 = packed >> 16;
    dst->g0 = packed >> 8;
    dst->b0 = packed;
    dst->code = flag ? dst->code | PRIM_SEMI_TRANS : dst->code & ~PRIM_SEMI_TRANS;
    dst->h = h;
    dst->w = w;
    dst->x0 = x;
    dst->y0 = y;
    addPrim(src, dst);
    /* TILE is 0x10 bytes: the next packet starts there. */
    return dst + 1;
}

/* Build a flat rectangle as prim_make_rect does, then a draw-mode packet that sets its
 * semi-transparency mode (bits 5 and 6) from flags. Returns the next free packet. */
void *prim_make_rect_mode(void *first, u_long *source, s16 x, s16 y, s16 w, s16 h, s32 color,
                    s16 semi_trans, u8 flags) {
    DR_TPAGE *mode;
    mode = prim_make_rect(first, source, x, y, w, h, color, semi_trans);
    setlen(mode, 1);
    mode->code[0] = ((flags & 3) << 5) | 0xE1000200;
    addPrim(source, mode);
    mode++;
    return mode;
}

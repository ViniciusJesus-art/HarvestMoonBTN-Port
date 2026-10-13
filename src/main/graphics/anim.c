/* Animations: binding an .ANI file, the frame clock and the frame lookups, drawing the
 * frames as SPRTs and POLY_FT4s, queueing them at the primitive cursor, and text drawn
 * through sprite parts. */
#include <memory.h>
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "types.h"
#include "anim.h"
#include "cpu_asm.h"
#include "farm_state.h"
#include "main.h"
#include "math_util.h"
#include "prim.h"
#include "system.h"
#include "texture_pack.h"

/* Stop here for good: print the error and keep the frame loop running. */
#define ANI_FAIL()                                   \
    do {                                             \
        FntPrint("ANI INIT ERROR \n");                \
        do {                                         \
        } while (host_poll() != 0);              \
    } while (0)

/* The magic word of an .ANI file. */
#define ANI_MAGIC 0xE

/* The file stores its sections as offsets from its own start. */
static inline void *at_offset(void *file, s32 offset) {
    return file + offset;
}

/* Bind an animation state to a loaded .ANI file: check the file, reset the
 * playback fields and resolve the file's five section offsets. */
void anim_bind_file(AnimState *state, AnimFile *file) {
    u32 caller;

    SAVE_RETURN_ADDRESS(&caller);
    if ((u32)file != ((u32)file >> 2) << 2) {
        ANI_FAIL();
    }
    if (file->magic != ANI_MAGIC) {
        ANI_FAIL();
    }
    state->ticks = 0;
    state->status = 0;
    state->flags = 0;
    state->animation = 0;
    state->frame = 0;
    state->scale = FIXED_ONE;
    state->pageOffX = 0;
    state->pageOffY = 0;
    state->clutOffX = 0;
    state->clutOffY = 0;
    state->source = 0;
    state->bank = 0;
    state->images = at_offset(file, file->section[0]);
    state->partLists = at_offset(file, file->section[1]);
    state->parts = at_offset(file, file->section[2]);
    state->ranges = at_offset(file, file->section[3]);
    state->frames = at_offset(file, file->section[4]);
    anim_set_source(state, file, 0);
}

/* Set and clear bits in a 16-bit flag word. */

void bits16_set(unsigned short *flags, signed int bits) {
    *flags |= bits;
}

void bits16_clear(unsigned short *flags, signed int bits) {
    bits &= 0xFFFF;
    *flags &= ~bits;
}

/* The frame clock (durations and control-byte jumps) and the frame lookups in an .ANI file. */

s16 text_sprite_key;
/* Screen-state flag: anim_set_tick_flag stores its argument into it, and anim_tick sets it to 1. */
s32 anim_screen_flag;

/* Switch to animation `anim` at its first frame, clearing the ended and jumped flags. */
void anim_select_range(AnimState *state, u8 anim)
{
    u16 flags = state->flags;
    state->animation = anim;
    state->frame = 0;
    state->ticks = 0;
    state->flags = flags & ~(ANIM_FLAG_ENDED | ANIM_FLAG_JUMPED);
}

void anim_set_frame(AnimState *state, u8 frame)
{
    state->frame = frame;
    state->ticks = 0;
}

/* Set the VRAM offsets added to the pages and palettes of the animation's images. */
void anim_set_offsets(AnimState *state, s32 pageX, s32 pageY, s32 clutX, s16 clutY)
{
    state->pageOffX = pageX;
    state->pageOffY = pageY;
    state->clutOffY = clutY;
    state->clutOffX = clutX;
}

void anim_set_source(AnimState *state, AnimFile *source, ImageBank *bank)
{
    state->source = source;
    state->bank = bank;
}

static void anim_stub_1(void)
{
}

void anim_set_bank(AnimState *state, ImageBank *bank)
{
    state->bank = bank;
}

/* Whether the current frame's control byte is a jump. */
int anim_frame_jumps(AnimState *state)
{
    AnimRange *anim = &state->ranges[state->animation];
    AnimFrame *frame = ANIM_FRAMES(state, anim);

    frame += state->frame;
    return frame->control >> 7;
}

void anim_set_tick_flag(signed int flag)
{
    anim_screen_flag = flag;
}

/* Advance an animation by one tick. A frame lasts half its duration byte (rounded
 * up), then the next frame starts. Its control byte then decides: 0xFF holds the
 * frame, a byte with 0x80 set jumps to frame `control & 0x7F`, and other bytes do
 * nothing, except past the last frame, where playback jumps to frame `control & 0x7F`.
 * Passing the last frame sets the ended flag; a jump sets the jumped flag. Paused
 * while the status has ANIM_STATUS_PAUSED set. */
void anim_tick(AnimState *state)
{
    AnimRange *anim;
    AnimFrame *frame;
    u16 flags;
    s16 count;
    s32 ticks;
    u8 next;

    if (state->status & ANIM_STATUS_PAUSED)
    {
        return;
    }
    anim = &state->ranges[state->animation];
    frame = &ANIM_FRAMES(state, anim)[state->frame];
    flags = state->flags & ~(ANIM_FLAG_ENDED | ANIM_FLAG_JUMPED);
    state->flags = flags;
    state->ticks++;
    count = state->ticks;
    ticks = (frame->duration + 1U) >> 1;
    if (count >= ticks)
    {
        state->ticks = 0;
        next = ++state->frame;
        if (next >= anim->frames)
        {
            flags |= ANIM_FLAG_ENDED;
            state->flags = flags;
            if (!(frame->control & ANIM_CONTROL_JUMP))
            {
                state->frame = frame->control & ANIM_CONTROL_FRAME_MASK;
                state->flags = (flags & ~ANIM_FLAG_ENDED) | ANIM_FLAG_JUMPED;
            }
        }
        if (frame->control != 0)
        {
            if (frame->control == ANIM_CONTROL_HOLD)
            {
                state->ticks = ticks;
                state->frame--;
                state->flags |= ANIM_FLAG_ENDED;
            }
            else if (frame->control & ANIM_CONTROL_JUMP)
            {
                state->frame = frame->control & ANIM_CONTROL_FRAME_MASK;
                state->flags = (state->flags & ~ANIM_FLAG_ENDED) | ANIM_FLAG_JUMPED;
            }
        }
    }
    anim_screen_flag = 1;
}

unsigned short anim_range_frames(AnimState *state, s16 index)
{
    AnimRange *range = &state->ranges[index];

    return range->frames;
}

/* The frames of animation `anim`. */
static inline AnimFrame *anim_frames(AnimState *state, s32 anim)
{
    AnimRange *range = &state->ranges[anim];

    return ANIM_FRAMES(state, range);
}

/* The parts of part list `list`. */
static inline AnimPart *anim_parts(AnimState *state, s32 list)
{
    AnimPartList *entry = &state->partLists[list];

    return ANIM_PARTS(state, entry);
}

/* The image of the first part of frame `second` of animation `first`; 0 when
 * the frame draws nothing. */
static s32 anim_part_image(AnimState *state, s32 first, s32 second)
{
    AnimFrame *frame = &anim_frames(state, first & 0xFF)[second & 0xFF];

    if (frame->parts > ANIM_NO_PARTS)
    {
        return 0;
    }
    return anim_parts(state, frame->parts)->image;
}

/* As anim_part_image, but returns the part's palette. */
s32 anim_part_palette(AnimState *state, s32 first, s32 second)
{
    AnimFrame *frame = &anim_frames(state, first & 0xFF)[second & 0xFF];

    if (frame->parts > ANIM_NO_PARTS)
    {
        return 0;
    }
    return anim_parts(state, frame->parts)->palette;
}

/* The index in `bank` of the image whose rectangle is image `key` of the animation:
 * loops forever when there is none. */
u16 anim_find_bank_image(AnimState *source, ImageBank *bank, s32 key)
{
    BankImage *image;
    u32 flags;
    s32 count;
    u16 limit;
    u32 mask;

    image = bank->images;
    count = 0;
    mask = 0xFFFFFF;
    limit = bank->count;
    key = key & 0xFFFF;
    while (1)
    {
        if (count >= limit)
        {
            break;
        }
        flags = image->sizeFlags & mask;
        if (memcmp(&source->images[key], &image->x, 8) == 0)
        {
            break;
        }
        image = BANK_NEXT(image, flags);
        count++;
    }
    if (count == limit)
    {
        do
        {
        } while (host_poll() != 0);
    }
    return count;
}

void anim_draw_ft4_advance(u_long *ot, AnimState *p, s16 x, s16 y)
{
    anim_draw_ft4_cursor(ot, p, p->animation, p->frame, x, y, p->scale, 0, 0);
    anim_tick(p);
}

void anim_draw_sprt_advance(u_long *ot, AnimState *p, s16 x, s16 y)
{
    anim_draw_sprt_cursor(ot, p, p->animation, p->frame, x, y);
    anim_tick(p);
}

static POLY_FT4 *anim_draw_ft4_prim_advance(u_long *ot, POLY_FT4 *prim, AnimState *sprite, s16 x, s16 y)
{
    POLY_FT4 *result;

    result = anim_draw_ft4(ot, prim, sprite, sprite->animation, sprite->frame, x, y, sprite->scale, 0, 0,
                           -1, -1);
    anim_tick(sprite);
    return result;
}

void anim_draw_sprt_slots_advance(u_long *ot, SPRT **sprites, DR_MODE **modes, AnimState *p, s16 x,
                                  s16 y)
{
    anim_draw_sprt(ot, sprites, modes, p, p->animation, p->frame, x, y);
    anim_tick(p);
}

static void anim_draw_ft4_sized_advance(u_long *ot, AnimState *sprite, s16 x, s16 y, signed int scale,
                                        signed int keepPosition, u16 flags)
{
    anim_draw_ft4_cursor(ot, sprite, sprite->animation, sprite->frame, x, y, scale, keepPosition,
                         flags);
    anim_tick(sprite);
}

/* Draw one text sprite with the frame's primitive cursor. Always returns 0. */
s32 text_queue_sprite(u_long *ot, struct AnimState *sprite, s32 x, s32 y, s32 unused4, s32 unused5, s32 unused6, u8 *base,
                  struct VramPlace *places) {
    text_sprite_draw(ot, current_frame->primCursor.ft4, sprite, x, y, ANIM_DRAW_CURSOR, base, places, -1, -1,
                  -1);
    return 0;
}

void anim_draw_sprt(u_long *ot, SPRT **sprites, DR_MODE **modes, struct AnimState *sprite,
                    unsigned char anim, unsigned char frame, signed short x, signed short y) {
    anim_draw_sprt_parts(ot, sprites, modes, sprite, anim, frame, x, y);
}

POLY_FT4 *anim_draw_ft4(u_long *ot, POLY_FT4 *prim, struct AnimState *sprite, signed int anim, unsigned char frame,
                        signed short x, signed short y, signed int scale, signed int keepPosition,
                        unsigned short flags, signed short pageX, signed short pageY) {
    return sprite_draw_frame(ot, prim, sprite, anim & 0xff, frame, x, y,
                             scale, scale, keepPosition, flags, pageX, pageY, -1, -1);
}

/* The work area holds a sprite and its draw mode in turn. */
void anim_draw_sprt_cursor(u_long *ot, struct AnimState *sprite, u8 anim, u8 frame, s16 x, s16 y) {
    PrimCursor work;
    work.bytes = gs_work_alloc(0);
    anim_draw_sprt_parts(ot, &work.sprt, &work.mode, sprite, anim, frame, x, y);
    gs_work_release(work.bytes);
}

void anim_draw_ft4_cursor(u_long *ot, struct AnimState *sprite, u8 anim, u8 frame, s16 x, s16 y, s32 scale, s32 keepPosition,
                          u16 flags) {
    gs_work_release(sprite_draw_frame(ot, gs_work_alloc(0), sprite, anim, frame, x, y, scale, scale,
                                      keepPosition, flags, -1, -1, -1, -1));
}

/* Sprite drawing through the work queue: a sprite at the primitive cursor, and the traced variant. */

void sprite_draw_traced(u_long *ot, POLY_FT4 *prim, AnimState *sprite, u8 anim, u8 frame, s16 x, s16 y, s32 scale,
                   s32 unused, u16 flags) {
    /* Remember the caller's return address in the farm state. */
    SAVE_RETURN_ADDRESS(&game_state.lastCaller);
    sprite_draw_frame(ot, prim, sprite, anim, frame, x, y, scale, scale, 0, flags, -1, -1, -1, -1);
}

static void sprite_draw_at_cursor(u_long *ot, AnimState *sprite, u8 anim, u8 frame, s16 x, s16 y, s32 scale, s32 keepPosition,
                   u16 flags, s16 pageX, s16 pageY, s16 clutX, s16 clutY) {
    gs_work_release(sprite_draw_frame(ot, gs_work_alloc(0), sprite, anim, frame, x, y, scale, scale,
                                keepPosition, flags, pageX, pageY, clutX, clutY));
}

static void sprite_draw_at_cursor_simple(u_long *ot, AnimState *sprite, u8 anim, u8 frame, s16 x, s16 y, s32 scaleX, s32 scaleY,
                   u16 flags) {
    gs_work_release(sprite_draw_frame(ot, gs_work_alloc(0), sprite, anim, frame, x, y, scaleX, scaleY, 0,
                                flags, -1, -1, -1, -1));
}

/* A CLUT id as the GPU packs it: the palette's x in 16-pixel units in the low 6 bits, its y in
 * the high 10. */
typedef union {
    struct {
        u16 x : 6;
        u16 y : 10;
    } pos;
    u16 id;
} ClutId;

/* No tint, and the SPRT code, as one word. */
#define SPRT_NEUTRAL_RGBC 0x64808080

/* Queue frame `frameIndex` of animation `anim` as SPRTs, each with its texture page's
 * DR_MODE, last part first. */
void anim_draw_sprt_parts(u_long *ot, SPRT **sprites, DR_MODE **modes, AnimState *state, u8 anim,
                   u8 frameIndex, s16 x, s16 y) {
    AnimRange *range;
    AnimFrame *frame;
    AnimPartList *list;
    AnimPart *part;
    AnimImage *image;
    AnimImage *palette;
    s32 n;
    s32 depth;
    s16 u, v, w, h;
    s16 dx, dy;

    range = &state->ranges[anim];
    frame = &ANIM_FRAMES(state, range)[frameIndex];
    if (frame->parts > ANIM_NO_PARTS) {
        return;
    }
    dx = frame->dx;
    dy = frame->dy;
    list = &state->partLists[frame->parts];
    part = ANIM_PARTS(state, list);
    part += list->count - 1;
    for (n = list->count - 1; n >= 0; n--) {
        image = &state->images[part->image];
        palette = &state->images[part->palette];
        u = (image->x + state->pageOffX) & 0x3F;
        v = (image->y + state->pageOffY) & 0xFF;
        w = image->w;
        h = image->h;
        depth = part->attr >> 7;
        if (depth) {
            u <<= 1;
            w <<= 1;
        } else {
            u <<= 2;
            w <<= 2;
        }

        setSprt(*sprites);
        *(u_long *)&(*sprites)->r0 = SPRT_NEUTRAL_RGBC;
        setSemiTrans(*sprites, (state->flags & ANIM_FLAG_SEMI_TRANSPARENT) || ((part->attr >> 4) & 1));
        setShadeTex(*sprites, 0);
        setXY0(*sprites, x + dx + part->dx, y + dy + part->dy);
        setWH(*sprites, w, h);
        setUV0(*sprites, u, v);
        {
            ClutId clut = {{0, palette->y + state->clutOffY}};

            clut.pos.x = (palette->x + state->clutOffX) >> 4;
            (*sprites)->clut = clut.id;
        }
        addPrim(ot, *sprites);
        (*sprites)++;

        setDrawTPage(
            *modes, 1, 1,
            getTPage(depth, 0, (image->x + state->pageOffX) & ~0x3F, image->y + state->pageOffY));
        addPrim(ot, *modes);
        (*modes)++;
        part--;
    }
}

/* Set a quad's UVs for orientation 0..3 (bit 0 mirrors U, bit 1 mirrors V). */
static inline void setOrientedUV(POLY_FT4 *prim, s32 u, s32 v, s32 w, s32 h, s16 orient) {
    if (orient == 0) {
        prim->u0 = u;
        prim->v0 = v;
        prim->u1 = u + (w - 1);
        prim->v1 = v;
        prim->u2 = u;
        prim->v2 = v + (h - 1);
        prim->u3 = u + (w - 1);
        prim->v3 = v + (h - 1);
    } else if (orient == 1) {
        prim->u0 = u + (w - 1);
        prim->v0 = v;
        prim->u1 = u;
        prim->v1 = v;
        prim->u2 = u + (w - 1);
        prim->v2 = v + (h - 1);
        prim->u3 = u;
        prim->v3 = v + (h - 1);
    } else if (orient == 2) {
        prim->u0 = u;
        prim->v0 = v + (h - 1);
        prim->u1 = u + (w - 1);
        prim->v1 = v + (h - 1);
        prim->u2 = u;
        prim->v2 = v;
        prim->u3 = u + (w - 1);
        prim->v3 = v;
    } else {
        prim->u0 = u + (w - 1);
        prim->v0 = v + (h - 1);
        prim->u1 = u;
        prim->v1 = v + (h - 1);
        prim->u2 = u + (w - 1);
        prim->v2 = v;
        prim->u3 = u;
        prim->v3 = v;
    }
}

/* Queue frame `frameIndex` of animation `anim` of `sprite` at (x, y) as
 * POLY_FT4s, last part first, and return the next free primitive. Positions
 * scale by scaleX/scaleY (20.12) unless scaleX + scaleY is 2.0 (0x2000), and are
 * left untouched with `keepPosition`; pageX/clutX other than -1 override the VRAM
 * position of the pixels/palette; ANIM_DRAW_FLIP_X and _Y mirror the sprite, and
 * ANIM_DRAW_CURSOR links the primitive as a POLY_FT4 in the cursor pool. */
POLY_FT4 *sprite_draw_frame(u_long *ot, POLY_FT4 *prim, AnimState *sprite, u8 anim, u8 frameIndex, s16 x,
                        s16 y, s32 scaleX, s32 scaleY, s32 keepPosition, u16 flags, s16 pageX,
                        s16 pageY, s16 clutX, s16 clutY) {
    AnimFrame *frame;
    AnimPart *part;
    AnimImage *tex;
    AnimImage *palette;
    s16 n;
    s32 count;
    s32 vx, vy;
    s16 px, py;
    s32 u, v, h;
    s32 tv;
    s32 left, right, top, bottom;
    s16 x0, x1, y0, y1;
    u8 attr;
    s32 depth;
    s32 orient;

    frame = &ANIM_FRAMES(sprite, &sprite->ranges[anim])[frameIndex];
    if (frame->parts > ANIM_NO_PARTS) {
        return prim;
    }
    count = sprite->partLists[frame->parts].count;
    part = ANIM_PARTS(sprite, &sprite->partLists[frame->parts]);
    part += count - 1;
    for (n = count - 1; n >= 0; n--) {
        tex = &sprite->images[part->image];
        palette = &sprite->images[part->palette];
        setPolyFT4(prim);
        setSemiTrans(prim, (sprite->flags & ANIM_FLAG_SEMI_TRANSPARENT) || ((part->attr >> 4) & 1));
        prim->r0 = 0x80;
        prim->g0 = 0x80;
        prim->b0 = 0x80;
        setShadeTex(prim, 0);
        if (clutX == -1) {
            vx = palette->x + sprite->clutOffX;
            vy = palette->y + sprite->clutOffY;
        } else {
            vx = clutX;
            vy = clutY;
        }
        prim->clut = getClut(vx, vy);
        attr = part->attr;
        depth = attr >> 7;
        orient = attr & 3;
        if (pageX == -1) {
            vx = tex->x + sprite->pageOffX;
            px = vx & ~0x3F;
            u = vx & 0x3F;
            vx = tex->y + sprite->pageOffY;
            py = vx & ~0xFF;
            v = vx & 0xFF;
        } else {
            u = pageX;
            v = pageY;
            px = u;
            py = v;
        }
        vx = u & 0x3F;
        prim->tpage = getTPage(depth, 0, px, py);
        px = tex->w;
        h = tex->h;
        tv = v & 0xFF;
        if (depth == 0) {
            vx = vx << 2;
            px = px << 2;
        } else {
            vx = (s16)(vx << 1);
            px = px << 1;
        }
        setOrientedUV(prim, vx, tv, px, h, orient);
        if (keepPosition == 0) {
            left = frame->dx + part->dx;
            right = left + px;
            top = frame->dy + part->dy;
            bottom = top + h;
            if (scaleX + scaleY != FIXED_ONE * 2) {
                x0 = x + left * scaleX / FIXED_ONE;
                x1 = x + right * scaleX / FIXED_ONE;
                y0 = y + top * scaleY / FIXED_ONE;
                y1 = y + bottom * scaleY / FIXED_ONE;
            } else {
                x0 = x + left;
                x1 = x + right;
                y0 = y + top;
                y1 = y + bottom;
            }
            if (flags & (ANIM_DRAW_FLIP_X | ANIM_DRAW_FLIP_Y)) {
                prim->x0 = x1;
                prim->y0 = y1;
                prim->x1 = x0;
                prim->y1 = y1;
                prim->x2 = x1;
                prim->y2 = y0;
                prim->x3 = x0;
                prim->y3 = y0;
            } else if (flags & ANIM_DRAW_FLIP_X) {
                prim->x0 = x1;
                prim->y0 = y0;
                prim->x1 = x0;
                prim->y1 = y0;
                prim->x2 = x1;
                prim->y2 = y1;
                prim->x3 = x0;
                prim->y3 = y1;
            } else if (flags & ANIM_DRAW_FLIP_Y) {
                prim->x0 = x0;
                prim->y0 = y1;
                prim->x1 = x1;
                prim->y1 = y1;
                prim->x2 = x0;
                prim->y2 = y0;
                prim->x3 = x1;
                prim->y3 = y0;
            } else {
                prim->x0 = x0;
                prim->y0 = y0;
                prim->x1 = x1;
                prim->y1 = y0;
                prim->x2 = x0;
                prim->y2 = y1;
                prim->x3 = x1;
                prim->y3 = y1;
            }
        }
        if (flags & ANIM_DRAW_CURSOR) {
            LINK_PRIM(ot, prim, POLY_FT4);
            current_frame->primCursor.ft4++;
        } else {
            addPrim(ot, prim);
        }
        prim++;
        part--;
    }
    return prim;
}

/* Text drawn through sprite parts: the width of a text run through its glyph table, and
 * the POLY_FT4 draw of each part. */

/* Width of a text run in the source's glyph table, rounded to a multiple of four
 * (a negative width is raised by 3 first, so it rounds toward zero), plus a margin of 32. */
static s32 sprite_measure_text(AnimState *source, s32 key, u8 *base, s32 x, s32 y) {
    s32 result;
    s32 value;

    result = anim_find_bank_image(source, source->bank, key & 0xFFFF);
    value = image_bank_upload(source->bank, base, result & 0xFFFF, x, y);
    if (value < 0) {
        value += 3;
    }
    value = (value >> 2) << 2;
    return value + 32;
}

s16 text_sprite_key;

/* Queue the current frame of `sprite` at (x, y) as POLY_FT4s, last part first,
 * and return the next free primitive. Each part's texture page and CLUT come
 * from `places`; the first time a place is used this frame its pixels and
 * palette are uploaded at `base`, which advances by what was sent. A place 0x80
 * part may take the caller's palette key `matchPlace`, and a part keyed
 * `matchKey` takes `replaceKey` (when `matchPlace` is not -1). Flags 8 and 0x10
 * mirror the quad. */
POLY_FT4 *text_sprite_draw(u_long *ot, POLY_FT4 *prim, AnimState *sprite, s16 x, s16 y, u16 flags, u8 *base,
                        VramPlace *places, s16 matchPlace, s16 matchKey, s16 replaceKey) {
    AnimFrame *frame;
    s32 count;
    AnimPart *part;
    AnimImage *tex;
    VramPlace *place;
    s16 seen[10];
    s32 seenCount;
    s32 n;
    s32 i;
    s32 k;
    s32 found;
    s32 width;
    s16 u, v, w, h;
    s16 left, right, top, bottom;
    s32 code;
    s16 pageX;
    s16 pageY;
    u8 attr;
    s32 depth;
    s32 orient;
    s32 originX;
    s32 originY;

    frame = &ANIM_FRAMES(sprite, &sprite->ranges[sprite->animation])[sprite->frame];
    if (frame->parts > ANIM_NO_PARTS) {
        return prim;
    }
    seenCount = 0;
    for (k = 0; k < 10; k++) {
        seen[k] = -1;
    }
    count = sprite->partLists[frame->parts].count;
    part = ANIM_PARTS(sprite, &sprite->partLists[frame->parts]);
    part += count - 1;
    for (n = count - 1; n >= 0; n--, prim++, part--) {
        width = 0;
        found = 0;
        tex = &sprite->images[part->image];
        place = &places[part->place - 0x80];
        code = part->place - 128;
        for (i = 0; i < 10; i++) {
            if (seen[i] == code) {
                found = 1;
                break;
            }
        }
        if (!found) {
            seen[seenCount++] = part->place - 128;
            width = sprite_measure_text(sprite, part->image, base, place->pageX,
                                  place->pageY);
            text_sprite_key = part->palette;
            if (matchPlace != -1) {
                if (part->place == 0x80) {
                    text_sprite_key = matchPlace;
                }
                if (part->palette == matchKey) {
                    text_sprite_key = replaceKey;
                }
            }
            width += sprite_measure_text(sprite, text_sprite_key, base + width,
                                   place->clutX, place->clutY);
        }
        setPolyFT4(prim);
        setSemiTrans(prim, (part->attr >> 4) & 1);
        prim->r0 = 0x80;
        prim->g0 = 0x80;
        prim->b0 = 0x80;
        setShadeTex(prim, 0);
        prim->clut = getClut(place->clutX, place->clutY);
        pageX = place->pageX;
        attr = part->attr;
        pageY = place->pageY;
        depth = attr >> 7;
        prim->tpage = getTPage(depth, 0, pageX, pageY);
        u = pageX & 0x3F;
        w = tex->w;
        orient = attr & 3;
        if (depth == 0) {
            u = u << 2;
            w = w << 2;
        } else {
            u = u << 1;
            w = w << 1;
        }
        v = pageY & 0xFF;
        h = tex->h;
        if (orient == 0) {
            prim->u0 = u;
            prim->v0 = v;
            prim->u1 = u + (w - 1);
            prim->v1 = v;
            prim->u2 = u;
            prim->v2 = v + (h - 1);
            prim->u3 = u + (w - 1);
            prim->v3 = v + (h - 1);
        } else if (orient == 1) {
            prim->u0 = u + (w - 1);
            prim->v0 = v;
            prim->u1 = u;
            prim->v1 = v;
            prim->u2 = u + (w - 1);
            prim->v2 = v + (h - 1);
            prim->u3 = u;
            prim->v3 = v + (h - 1);
        } else if (orient == 2) {
            prim->u0 = u;
            prim->v0 = v + (h - 1);
            prim->u1 = u + (w - 1);
            prim->v1 = v + (h - 1);
            prim->u2 = u;
            prim->v2 = v;
            prim->u3 = u + (w - 1);
            prim->v3 = v;
        } else {
            prim->u0 = u + (w - 1);
            prim->v0 = v + (h - 1);
            prim->u1 = u;
            prim->v1 = v + (h - 1);
            prim->u2 = u + (w - 1);
            prim->v2 = v;
            prim->u3 = u;
            prim->v3 = v;
        }
        originX = x + (u16)frame->dx;
        left = part->dx + originX;
        right = left + w;
        originY = y + (u16)frame->dy;
        top = part->dy + originY;
        bottom = top + h;
        if (flags & (ANIM_DRAW_FLIP_X | ANIM_DRAW_FLIP_Y)) {
            prim->x0 = right;
            prim->y0 = bottom;
            prim->x1 = left;
            prim->y1 = bottom;
            prim->x2 = right;
            prim->y2 = top;
            prim->x3 = left;
            prim->y3 = top;
        } else if (flags & ANIM_DRAW_FLIP_X) {
            prim->x0 = right;
            prim->y0 = top;
            prim->x1 = left;
            prim->y1 = top;
            prim->x2 = right;
            prim->y2 = bottom;
            prim->x3 = left;
            prim->y3 = bottom;
        } else if (flags & ANIM_DRAW_FLIP_Y) {
            prim->x0 = left;
            prim->y0 = bottom;
            prim->x1 = right;
            prim->y1 = bottom;
            prim->x2 = left;
            prim->y2 = top;
            prim->x3 = right;
            prim->y3 = top;
        } else {
            prim->x0 = left;
            prim->y0 = top;
            prim->x1 = right;
            prim->y1 = top;
            prim->x2 = left;
            prim->y2 = bottom;
            prim->x3 = right;
            prim->y3 = bottom;
        }
        LINK_PRIM(ot, prim, POLY_FT4);
        base += width;
        current_frame->primCursor.ft4++;
    }
    return prim;
}

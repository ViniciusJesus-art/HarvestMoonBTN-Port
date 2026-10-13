#ifndef ANIM_H
#define ANIM_H

/* Animation: .ANI files, the animation state, and drawing its frames as sprites. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "texture_pack.h"

typedef struct AnimFile {
    s32 magic;
    u8 pad04[0xC];
    s32 section[5];     /* section offsets from the file start: images, part lists, parts, ranges, frames */
} AnimFile;

/* A rectangle in VRAM: a part's pixels, or its palette. */
typedef struct AnimImage {
    s16 x;
    s16 y;
    s16 w;              /* in 16-bit units */
    s16 h;
} AnimImage;

/* How many parts a frame draws and where the first one starts in `parts`. */
typedef struct AnimPartList {
    s16 count;
    u16 offset;
} AnimPartList;

/* One sprite of a frame. */
typedef struct AnimPart {
    u16 image;          /* AnimImage index of the pixels */
    u16 palette;        /* AnimImage index of the palette */
    s16 dx;
    s16 dy;
    u8 attr;            /* bits 0-1 UV orientation, bit 4 semi-transparent, bit 7 8-bit */
    u8 place;           /* VramPlace index + 0x80 */
    u8 pad[2];
} AnimPart;

/* One animation: its frame count and where its frames start in `frames`. */
typedef struct AnimRange {
    s16 frames;
    u16 offset;
} AnimRange;

/* A frame's parts index above this (0xFFFE, 0xFFFF) means nothing to draw. */
#define ANIM_NO_PARTS 0xFFFD

/* AnimFrame.control: 0 does nothing; ANIM_CONTROL_JUMP sends playback to frame
 * control & ANIM_CONTROL_FRAME_MASK; ANIM_CONTROL_HOLD holds the frame. */
#define ANIM_CONTROL_JUMP 0x80
#define ANIM_CONTROL_HOLD 0xFF
#define ANIM_CONTROL_FRAME_MASK 0x7F

/* One frame of an animation. */
typedef struct AnimFrame {
    u16 parts;          /* AnimPartList index, > 0xFFFD = nothing to draw */
    s16 dx;
    s16 dy;
    u8 duration;        /* in half ticks */
    u8 control;         /* 0x80 jumps to frame control & 0x7F, 0xFF holds */
    u8 pad[2];
} AnimFrame;

/* AnimState.status bits. */
enum AnimStatus {
    ANIM_STATUS_PAUSED = 4,     /* the sprite is paused (set while the minigame is paused) */
};

typedef struct AnimState {
    u16 status;         /* ANIM_STATUS_PAUSED */
    u16 flags;          /* ANIM_FLAG_ENDED past the end or held, ANIM_FLAG_JUMPED, ANIM_FLAG_SEMI_TRANSPARENT */
    u8 animation;       /* index into ranges */
    u8 frame;
    u16 ticks;          /* ticks into the frame */
    s32 scale;          /* 1.12 scale of the image (0x1000 = 1.0), both axes */
    s16 pageOffX;       /* added to an image's VRAM x */
    s16 pageOffY;       /* added to an image's VRAM y */
    s16 clutOffX;       /* added to a palette image's VRAM x */
    s16 clutOffY;       /* added to a palette image's VRAM y */
    AnimFile *source;   /* the file this state was bound to */
    ImageBank *bank;    /* the pixels and palettes `images` name */
    AnimImage *images;
    AnimPartList *partLists;
    u8 *parts;          /* AnimPart runs, at AnimPartList.offset */
    AnimRange *ranges;  /* one per animation */
    u8 *frames;         /* AnimFrame runs, at AnimRange.offset */
} AnimState;

/* AnimState.flags. */
enum AnimFlag {
    ANIM_FLAG_SEMI_TRANSPARENT = 0x20,
    ANIM_FLAG_ENDED = 0x100,        /* past the last frame, or held */
    ANIM_FLAG_JUMPED = 0x200,       /* the last frame jumped back */
};

/* The `flags` argument of the sprite and text draws. */
enum AnimDrawFlag {
    ANIM_DRAW_FLIP_X = 0x8,
    ANIM_DRAW_FLIP_Y = 0x10,
    ANIM_DRAW_CURSOR = 0x40,        /* the primitive goes to the cursor pool */
};

/* The frames of animation `range` and the parts of part list `list`. */
#define ANIM_FRAMES(state, range) ((AnimFrame *)((state)->frames + (range)->offset))

#define ANIM_PARTS(state, list) ((AnimPart *)((state)->parts + (list)->offset))

/* Where a sprite part's pixels and palette go in VRAM. */
typedef struct VramPlace {
    s16 clutX;
    s16 clutY;
    s16 pageX;
    s16 pageY;
} VramPlace;

/* The table of contents of an animation pack: a file of `count` .ANI files followed by
 * one or two texture packs. .ANI k starts at start[k] (bytes from the pack's start);
 * start[count] is where the .ANI files end and the first texture pack begins. A few
 * packs carry one more word, start[count + 1], the start of a second texture pack. */
typedef struct AnimPackToc {
    s32 count;
    s32 start[0];
} AnimPackToc;

/* The count and the offsets are one run of words, so the k-th start is reached by
 * index, not by a struct field. */
#define ANIM_PACK_START(toc, k) ((&(toc)->count)[(k) + 1])

void anim_bind_file(AnimState *state, AnimFile *file);
void anim_draw_sprt(u_long *ot, SPRT **sprites, DR_MODE **modes, AnimState *sprite, u8 anim, u8 frame, s16 x, s16 y);
POLY_FT4 *anim_draw_ft4(u_long *ot, POLY_FT4 *prim, AnimState *sprite, s32 anim, u8 frame, s16 x, s16 y, s32 scale, s32 keepPosition, u16 flags, s16 pageX, s16 pageY);
void anim_draw_sprt_cursor(u_long *ot, AnimState *sprite, u8 anim, u8 frame, s16 x, s16 y);
void anim_draw_ft4_cursor(u_long *ot, AnimState *sprite, u8 anim, u8 frame, s16 x, s16 y, s32 scale, s32 keepPosition, u16 flags);
void anim_draw_sprt_parts(u_long *ot, SPRT **sprites, DR_MODE **modes, AnimState *state, u8 anim,
                          u8 frame, s16 x, s16 y);
void anim_select_range(AnimState *state, u8 anim);
void anim_set_frame(AnimState *state, u8 frame);
void anim_set_offsets(AnimState *state, s32 pageX, s32 pageY, s32 clutX, s16 clutY);
void anim_set_source(AnimState *state, AnimFile *source, ImageBank *bank);
void anim_set_bank(AnimState *state, ImageBank *bank);
s32 anim_frame_jumps(AnimState *state);
void anim_set_tick_flag(s32 flag);
void anim_tick(AnimState *state);
u16 anim_range_frames(AnimState *state, s16 index);
s32 anim_part_palette(AnimState *state, s32 first, s32 second);
u16 anim_find_bank_image(AnimState *source, ImageBank *bank, s32 key);
void anim_draw_ft4_advance(u_long *ot, AnimState *p, s16 x, s16 y);
void anim_draw_sprt_advance(u_long *ot, AnimState *p, s16 x, s16 y);
void anim_draw_sprt_slots_advance(u_long *ot, SPRT **sprites, DR_MODE **modes, AnimState *p, s16 x, s16 y);

void bits16_set(u16 *flags, s32 bits);
void bits16_clear(u16 *flags, s32 bits);
s32 text_queue_sprite(u_long *ot, AnimState *sprite, s32 x, s32 y, s32 unused4, s32 unused5, s32 unused6, u8 *base, VramPlace *places);
POLY_FT4 *text_sprite_draw(u_long *ot, POLY_FT4 *prim, AnimState *sprite, s16 x, s16 y, u16 flags,
                               u8 *base, VramPlace *places, s16 matchPlace, s16 matchKey,
                               s16 replaceKey);
POLY_FT4 *sprite_draw_frame(u_long *ot, POLY_FT4 *prim, AnimState *sprite, u8 anim, u8 frameIndex, s16 x,
                               s16 y, s32 scaleX, s32 scaleY, s32 keepPosition, u16 flags, s16 pageX,
                               s16 pageY, s16 clutX, s16 clutY);
void sprite_draw_traced(u_long *ot, POLY_FT4 *prim, AnimState *sprite, u8 anim, u8 frameIndex, s16 x, s16 y, s32 scale, s32 unused, u16 flags);

#endif /* ANIM_H */

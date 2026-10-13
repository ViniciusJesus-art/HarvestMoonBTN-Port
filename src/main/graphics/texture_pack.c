/* Texture packs and image banks: uploading them to VRAM. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "types.h"
#include "anim.h"
#include "texture_pack.h"

static void texture_pack_upload(void *pack, s16 x, s16 y, s16 altX, s16 altY, s32 mode);

static char texture_upload_buffer[1024];
static u16 texture_row_buffer[256];

void texture_pack_load(u8 *pack) {
    texture_pack_upload(pack, 0, 0, 0, 0, 0);
}

/* Load a pack with the STP bit set on the pixels of its blocks that have flag bit 1. */
static void texture_pack_load_stp(void *pack) {
    texture_pack_upload(pack, 0, 0, 0, 0, 1);
}

void texture_pack_load_at(void *pack, s16 x, s16 y, s16 altX, s16 altY) {
    texture_pack_upload(pack, x, y, altX, altY, 0);
}

/* As texture_pack_load_at, but blocks 256 pixels wide or more stay where they are. */
void texture_pack_load_keep_wide(void *pack, signed short x, signed short y, signed short altX,
                   signed short altY) {
    texture_pack_upload(pack, x, y, altX, altY, 2);
}

/* Expand the bank image that the sprite's palette part picks for `anim` and `frame` into
 * texture_upload_buffer, and upload it at (x, y). */
static void sprite_load_bank_image(AnimState *sprite, ImageBank *bank, s32 anim, s32 frame, s16 x, s16 y) {
    image_bank_upload(bank, texture_upload_buffer,
                  anim_find_bank_image(sprite, bank, anim_part_palette(sprite, anim & 255, frame & 255)), x,
                  y);
}

/* Upload an image bank (a 0x0D texture pack) to VRAM block by block through the line buffer
 * texture_row_buffer, one LoadImage per row (or per column when flag bit 3 is set).
 * Flag bits 2-3 pick the payload: 0 and 8 are run-length coded (a signed count byte,
 * negative = that many literal bytes, else repeat the next byte), 4 is raw. Blocks with
 * flag bits 0-1 == 2 are placed at (altX, altY) unless `mode` 2 keeps wide ones (256
 * pixels or more) in place. `mode` 1 sets the STP bit (0x8000) on pixels 1..w-1 of the
 * blocks that have flag bit 1 set. */
static void texture_pack_upload(void *pack, s16 x, s16 y, s16 altX, s16 altY, s32 mode) {
    ImageBank *header = pack;
    BankImage *block;
    RECT rect;
    u8 *src;
    u8 *dst;
    u32 size;
    u32 flags;
    u16 count;
    u16 i;
    u16 rows;
    u16 rowBytes;
    s32 row;
    s32 col;
    u16 j;
    u16 half;
    s8 run;
    u16 left;
    u16 top;
    u16 width;
    u16 height;
    s32 repeat;
    u8 fill;

    fill = 0;
    if (header->type != 0x0D) {
        return;
    }
    count = header->count;
    pack = header->images;
    i = 0;
    if (count == 0) {
        return;
    }
    do {
        block = pack;
        size = block->sizeFlags & 0xFFFFFF;
        flags = block->sizeFlags >> 24;
        left = block->x;
        top = block->y;
        width = block->w;
        height = block->h;
        rect.x = left;
        rect.y = top;
        rect.w = width;
        rect.h = height;
        src = block->data;
        if ((flags & 3) != 2) {
            rect.x += x;
            rect.y += y;
        } else if ((s16)width < 256 || mode != 2) {
            rect.x += altX;
            rect.y += altY;
        }
        run = 0;
        repeat = 0;
        rows = rect.h;
        rowBytes = rect.w * 2;
        if ((flags & 0xC) == 0) {
            rect.h = 1;
            for (row = 0; row < rows; row++) {
                dst = (u8 *)texture_row_buffer;
                for (col = 0; col < rowBytes; col++, run--) {
                    if (run <= 0) {
                        run = *src++;
                        if (run < 0) {
                            repeat = 0;
                            run = -run;
                        } else {
                            repeat = 1;
                            fill = *src++;
                        }
                    }
                    if (repeat == 0) {
                        *dst = *src++;
                    } else {
                        *dst = fill;
                    }
                    dst++;
                }
                if ((flags & 2) && mode == 1) {
                    for (j = 1; j < rect.w; j++) {
                        texture_row_buffer[j] |= 0x8000;
                    }
                }
                LoadImage(&rect, (u_long *)texture_row_buffer);
                rect.y++;
            }
        } else if (flags & 0x8) {
            rect.w = 1;
            for (col = 0; col < rowBytes; col += 2) {
                for (half = 0; half < 2; half++) {
                    /* The low byte of each texel word first, then the high byte. */
                    dst = (u8 *)texture_row_buffer + (half != 0);
                    for (row = 0; row < rows; row++, run--) {
                        if (run <= 0) {
                            run = *src++;
                            if (run < 0) {
                                repeat = 0;
                                run = -run;
                            } else {
                                repeat = 1;
                                fill = *src++;
                            }
                        }
                        if (repeat == 0) {
                            *dst = *src++;
                        } else {
                            *dst = fill;
                        }
                        dst += 2;
                    }
                }
                if ((flags & 2) && mode == 1) {
                    for (j = 1; j < rect.w; j++) {
                        texture_row_buffer[j] |= 0x8000;
                    }
                }
                LoadImage(&rect, (u_long *)texture_row_buffer);
                rect.x++;
            }
        } else {
            rect.h = 1;
            for (row = 0; row < rows; row++) {
                dst = (u8 *)texture_row_buffer;
                for (col = 0; col < rowBytes; col++) {
                    *dst++ = *src++;
                }
                if ((flags & 2) && mode == 1) {
                    for (j = 1; j < rect.w; j++) {
                        texture_row_buffer[j] |= 0x8000;
                    }
                }
                LoadImage(&rect, (u_long *)texture_row_buffer);
                rect.y++;
            }
        }
        while (DrawSync(1) != 0) {
        }
        pack = BANK_NEXT(block, size);
        i++;
    } while (i < count);
}

/* Upload image `index` of `bank` to VRAM, at its own position when x is -1 and
 * at (x, y) otherwise, and return its size in bytes. Images without flag bits
 * 0xC are run-length coded and expanded into `buffer` first: a negative
 * control byte -n copies n literal bytes, a positive n repeats the next byte n
 * times. Returns 0 when `bank` is not an image bank. */
s32 image_bank_upload(void *bank, u8 *buffer, u16 index, s16 x, s16 y) {
    ImageBank *header = bank;
    BankImage *image;
    RECT rect;
    u8 *src;
    u16 i;
    u16 count;
    u32 word;
    u32 size;
    u32 flags;
    u16 w;
    u16 rowBytes;
    u8 *dst;
    s32 row;
    u16 height;
    s32 n;
    s8 run;
    s32 repeat;
    u8 value;

    if (header->type != 0xD) {
        return 0;
    }
    count = header->count;
    bank = header->images;
    for (i = 0; i < count; i++) {
        image = bank;
        word = image->sizeFlags;
        size = word & 0xFFFFFF;
        if (i != index) {
            bank = BANK_NEXT(image, size);
        } else {
            flags = word >> 24;
            if (x == -1) {
                rect.x = image->x;
                rect.y = image->y;
            } else {
                rect.x = x;
                rect.y = y;
            }
            src = image->data;
            w = image->w;
            rect.w = w;
            rect.h = image->h;
            if (!(flags & 0xC)) {
                run = 0;
                repeat = 0;
                value = 0;
                dst = buffer;
                rowBytes = rect.w * 2;
                height = rect.h;
                for (row = 0; row < height; row++) {
                    for (n = 0; n < rowBytes; n++, run--) {
                        if (run <= 0) {
                            run = *src++;
                            if (run < 0) {
                                repeat = 0;
                                run = -run;
                            } else {
                                repeat = 1;
                                value = *src++;
                            }
                        }
                        if (repeat == 0) {
                            *dst++ = *src++;
                        } else {
                            *dst++ = value;
                        }
                    }
                }
                LoadImage(&rect, (u_long *)buffer);
            } else {
                LoadImage(&rect, (u_long *)src);
            }
            break;
        }
    }
    return rect.w * (rect.h << 1);
}

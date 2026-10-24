/* Host adaptation of formats/operations documented in graphics/texture_pack.c and
 * graphics/anim.c. Serialized fields are decoded explicitly, never cast to structs. */
#include "asset_decode.h"
#include "psx_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FAIL(message) do { snprintf(error, error_size, "%s", message); return false; } while (0)

static bool span(size_t size, size_t start, size_t length)
{
    return start <= size && length <= size - start;
}

static int signed16(const uint8_t *p)
{
    const uint16_t value = hmbtn_read_le16(p);
    return value < 32768 ? (int)value : (int)value - 65536;
}

static bool expand_image(const uint8_t *data, size_t size, uint8_t *out, size_t needed,
    bool raw)
{
    if (raw) {
        if (size < needed) return false;
        memcpy(out, data, needed);
        return true;
    }
    size_t input = 0, output = 0;
    while (output < needed) {
        if (input >= size) return false;
        const unsigned control = data[input++];
        const size_t run = control >= 128 ? 256 - control : (control ? control : 1);
        if (run > needed - output) return false;
        if (control >= 128) {
            if (!span(size, input, run)) return false;
            memcpy(out + output, data + input, run);
            input += run;
        } else {
            if (input >= size) return false;
            memset(out + output, data[input++], run);
        }
        output += run;
    }
    return true;
}

bool hmbtn_bank_decode(const uint8_t *data, size_t size, HmbtnImageAtlas *atlas,
    char *error, size_t error_size)
{
    if (!atlas) FAIL("Invalid image atlas");
    memset(atlas, 0, sizeof(*atlas));
    return hmbtn_bank_apply(data, size, atlas, error, error_size);
}

bool hmbtn_bank_apply(const uint8_t *data, size_t size, HmbtnImageAtlas *atlas,
    char *error, size_t error_size)
{
    return hmbtn_bank_apply_at(data,size,atlas,0,0,0,0,error,error_size);
}
bool hmbtn_bank_apply_at(const uint8_t *data, size_t size, HmbtnImageAtlas *atlas,
    int shift_x, int shift_y, int palette_x, int palette_y, char *error, size_t error_size)
{
    if (!data || !atlas || size < 4 || data[0] != 0x0D) FAIL("Invalid image bank header");
    const unsigned count = hmbtn_read_le16(data + 2);
    if (!count || count > 4096) FAIL("Invalid image bank count");
    size_t offset = 4;
    for (unsigned i = 0; i < count; ++i) {
        if (!span(size, offset, 12)) FAIL("Truncated image bank entry");
        const uint32_t tag = hmbtn_read_le32(data + offset);
        const size_t payload = tag & UINT32_C(0xFFFFFF);
        const unsigned flags = tag >> 24;
        const int64_t moved_x=(int64_t)hmbtn_read_le16(data+offset+4)+((flags&3)==2 ? palette_x : shift_x);
        const int64_t moved_y=(int64_t)hmbtn_read_le16(data+offset+6)+((flags&3)==2 ? palette_y : shift_y);
        if(moved_x<0 || moved_x>=HMBTN_ATLAS_WIDTH || moved_y<0 || moved_y>=HMBTN_ATLAS_HEIGHT)
            FAIL("Relocated image bank outside atlas");
        const unsigned x=(unsigned)moved_x, y=(unsigned)moved_y;
        const unsigned w = hmbtn_read_le16(data + offset + 8);
        const unsigned h = hmbtn_read_le16(data + offset + 10);
        const size_t padded = (payload + 3) & ~(size_t)3;
        if (!w || !h || x >= HMBTN_ATLAS_WIDTH || y >= HMBTN_ATLAS_HEIGHT
                || w > HMBTN_ATLAS_WIDTH - x || h > HMBTN_ATLAS_HEIGHT - y
                || (flags & ~15u)) FAIL("Unsupported image bank rectangle or flags");
        if (!span(size, offset + 12, padded)) FAIL("Truncated image bank payload");
        const size_t decoded_size = (size_t)w * h * 2;
        uint8_t *decoded = malloc(decoded_size);
        if (!decoded) FAIL("Cannot allocate image buffer");
        const bool columns = (flags & 8) != 0;
        if (!expand_image(data + offset + 12, payload, decoded, decoded_size,
                !columns && (flags & 4) != 0)) {
            free(decoded);
            FAIL("Invalid or truncated image compression");
        }
        for (unsigned row = 0; row < h; ++row) {
            for (unsigned col = 0; col < w; ++col) {
                const size_t at = (size_t)(y + row) * HMBTN_ATLAS_WIDTH + x + col;
                /* Column encoding stores each word's low-byte column, then high. */
                const size_t low = columns ? (size_t)col * h * 2 + row
                                          : ((size_t)row * w + col) * 2;
                const size_t high = columns ? low + h : low + 1;
                atlas->words[at] = (uint16_t)(decoded[low] | ((uint16_t)decoded[high] << 8));
                atlas->present[at] = 1;
            }
        }
        free(decoded);
        offset += 12 + padded;
    }
    return true;
}

bool hmbtn_animation_open(HmbtnAnimation *a, const uint8_t *data, size_t size,
    char *error, size_t error_size)
{
    static const unsigned stride[5] = {8, 4, 12, 4, 10};
    if (!a || !data || size < 36 || hmbtn_read_le32(data) != 0x0E) FAIL("Invalid ANI header");
    memset(a, 0, sizeof(*a));
    a->data = data;
    a->size = size;
    size_t end = 36;
    for (unsigned i = 0; i < 5; ++i) {
        a->count[i] = hmbtn_read_le16(data + 4 + i * 2);
        a->section[i] = hmbtn_read_le32(data + 16 + i * 4);
        const size_t length = (size_t)a->count[i] * stride[i];
        if (!a->count[i] || a->section[i] < end || !span(size, a->section[i], length)) {
            FAIL("ANI section outside file or overlapping");
        }
        end = a->section[i] + length;
    }
    for (unsigned i = 0; i < a->count[1]; ++i) {
        const uint8_t *list = data + a->section[1] + i * 4;
        const unsigned count = hmbtn_read_le16(list), offset = hmbtn_read_le16(list + 2);
        if (count > 32767 || offset % 12 || !span((size_t)a->count[2] * 12, offset, (size_t)count * 12)) {
            FAIL("ANI part list outside section");
        }
    }
    for (unsigned i = 0; i < a->count[2]; ++i) {
        const uint8_t *part = data + a->section[2] + i * 12;
        if (hmbtn_read_le16(part) >= a->count[0] || hmbtn_read_le16(part + 2) >= a->count[0]) {
            FAIL("ANI image or palette index outside section");
        }
    }
    for (unsigned i = 0; i < a->count[4]; ++i) {
        const unsigned list = hmbtn_read_le16(data + a->section[4] + i * 10);
        if (list <= 0xFFFD && list >= a->count[1]) FAIL("ANI frame references an invalid part list");
    }
    for (unsigned i = 0; i < a->count[3]; ++i) {
        const uint8_t *range = data + a->section[3] + i * 4;
        const unsigned count = hmbtn_read_le16(range), offset = hmbtn_read_le16(range + 2);
        /* Some original NPC files reserve empty animation ranges. They may
         * be imported, but frame_at still rejects attempts to play them. */
        if (count > 256 || offset % 10
                || !span((size_t)a->count[4] * 10, offset, (size_t)count * 10)) FAIL("ANI range outside frames section");
        for (unsigned f = 0; f < count; ++f) {
            const unsigned control = data[a->section[4] + offset + f * 10 + 7];
            if (control != 0xFF && ((control & 0x80) || f + 1 == count)
                    && (control & 0x7F) >= count) FAIL("ANI jump outside animation range");
        }
    }
    return true;
}

static const uint8_t *frame_at(const HmbtnAnimation *a, const HmbtnAnimationPlayer *p)
{
    if (p->animation >= a->count[3]) return NULL;
    const uint8_t *range = a->data + a->section[3] + p->animation * 4;
    if (p->frame >= hmbtn_read_le16(range)) return NULL;
    return a->data + a->section[4] + hmbtn_read_le16(range + 2) + p->frame * 10;
}

bool hmbtn_animation_tick(const HmbtnAnimation *a, HmbtnAnimationPlayer *p,
    char *error, size_t error_size)
{
    const uint8_t *frame = frame_at(a, p);
    if (!frame) FAIL("Animation player outside range");
    const unsigned duration = ((unsigned)frame[6] + 1) / 2;
    if (++p->ticks < duration) return true;
    p->ticks = 0;
    if (frame[7] == 0xFF) return true;
    const uint8_t *range = a->data + a->section[3] + p->animation * 4;
    const unsigned count = hmbtn_read_le16(range);
    if ((frame[7] & 0x80) || p->frame + 1u >= count) {
        p->frame = frame[7] & 0x7F;
    } else {
        ++p->frame;
    }
    return true;
}

static bool atlas_word(const HmbtnImageAtlas *atlas, unsigned x, unsigned y, uint16_t *word)
{
    if (x >= HMBTN_ATLAS_WIDTH || y >= HMBTN_ATLAS_HEIGHT) return false;
    const size_t at = (size_t)y * HMBTN_ATLAS_WIDTH + x;
    if (!atlas->present[at]) return false;
    *word = atlas->words[at];
    return true;
}

bool hmbtn_texture_draw(const HmbtnImageAtlas *atlas, unsigned tx, unsigned ty,
    unsigned width, unsigned height, unsigned depth, unsigned cx, unsigned cy,
    int x, int y, uint8_t *rgba, unsigned output_width, unsigned output_height,
    char *error, size_t error_size)
{
    if (!atlas || !rgba || (depth != 4 && depth != 8) || !width || !height || width > 256 || height > 256
            || !output_width || !output_height || output_width > 4096 || output_height > 4096)
        FAIL("Invalid indexed sprite");
    const unsigned per_word = 16 / depth;
    if (tx >= HMBTN_ATLAS_WIDTH || ty >= HMBTN_ATLAS_HEIGHT
            || (width + per_word - 1) / per_word > HMBTN_ATLAS_WIDTH - tx
            || height > HMBTN_ATLAS_HEIGHT - ty || cy >= HMBTN_ATLAS_HEIGHT
            || cx > HMBTN_ATLAS_WIDTH - (1u << depth)) FAIL("Indexed sprite outside atlas");
    for (unsigned row = 0; row < height; ++row) {
        const int64_t dy = (int64_t)y + row;
        if (dy < 0 || dy >= output_height) continue;
        for (unsigned col = 0; col < width; ++col) {
            const int64_t dx = (int64_t)x + col;
            if (dx < 0 || dx >= output_width) continue;
            uint16_t word, color;
            if (!atlas_word(atlas, tx + col / per_word, ty + row, &word)) FAIL("Missing sprite texture");
            const unsigned index = (word >> ((col % per_word) * depth)) & ((1u << depth) - 1);
            if (!atlas_word(atlas, cx + index, cy, &color)) FAIL("Missing sprite palette");
            if (!color) continue;
            uint8_t *pixel = rgba + ((size_t)dy * output_width + (size_t)dx) * 4;
            for (unsigned c = 0; c < 3; ++c) {
                const unsigned value = (color >> (c * 5)) & 31;
                pixel[c] = (uint8_t)((value << 3) | (value >> 2));
            }
            pixel[3] = 255;
        }
    }
    return true;
}

bool hmbtn_animation_draw(const HmbtnAnimation *a, const HmbtnImageAtlas *atlas,
    const HmbtnAnimationPlayer *p, int x, int y, uint8_t *rgba,
    unsigned width, unsigned height, size_t rgba_size, char *error, size_t error_size)
{
    if (!width || !height || width > 4096 || height > 4096
            || !rgba || rgba_size < (size_t)width * height * 4) FAIL("Invalid output image");
    const uint8_t *frame = frame_at(a, p);
    if (!frame) FAIL("Animation player outside range");
    const unsigned index = hmbtn_read_le16(frame);
    if (index > 0xFFFD) return true;
    const uint8_t *list = a->data + a->section[1] + index * 4;
    const unsigned count = hmbtn_read_le16(list);
    const uint8_t *parts = a->data + a->section[2] + hmbtn_read_le16(list + 2);
    /* The PS1 code inserts parts backwards into a prepend-only ordering table;
     * the resulting display order is the forward order used here. */
    for (unsigned i = 0; i < count; ++i) {
        const uint8_t *part = parts + i * 12;
        const uint8_t *image = a->data + a->section[0] + hmbtn_read_le16(part) * 8;
        const uint8_t *palette = a->data + a->section[0] + hmbtn_read_le16(part + 2) * 8;
        const unsigned attr = part[8], depth = (attr & 0x80) ? 8 : 4, per_word = 16 / depth;
        const unsigned tx = hmbtn_read_le16(image), ty = hmbtn_read_le16(image + 2);
        const unsigned w = (unsigned)hmbtn_read_le16(image + 4) * per_word;
        const unsigned h = hmbtn_read_le16(image + 6);
        const unsigned cx = hmbtn_read_le16(palette) & ~15u, cy = hmbtn_read_le16(palette + 2);
        if (!w || w > 256 || !h || h > 256 || tx >= HMBTN_ATLAS_WIDTH
                || ty >= HMBTN_ATLAS_HEIGHT) FAIL("Unsupported ANI image dimensions");
        if (hmbtn_read_le16(palette + 4) < (1u << depth)
                || !hmbtn_read_le16(palette + 6) || cy >= HMBTN_ATLAS_HEIGHT
                || cx > HMBTN_ATLAS_WIDTH - (1u << depth)) FAIL("Invalid ANI palette dimensions");
        const int64_t left = (int64_t)x + signed16(frame + 2) + signed16(part + 4);
        const int64_t top = (int64_t)y + signed16(frame + 4) + signed16(part + 6);
        for (unsigned row = 0; row < h; ++row) {
            const int64_t dy = top + row;
            if (dy < 0 || dy >= (int64_t)height) continue;
            for (unsigned col = 0; col < w; ++col) {
                const int64_t dx = left + col;
                if (dx < 0 || dx >= (int64_t)width) continue;
                const unsigned u = ((tx & 63) * per_word + ((attr & 1) ? w - 1 - col : col)) & 255;
                const unsigned v = ((ty & 255) + ((attr & 2) ? h - 1 - row : row)) & 255;
                uint16_t texel, color;
                if (!atlas_word(atlas, (tx & ~63u) + u / per_word, (ty & ~255u) + v, &texel)) {
                    FAIL("ANI image reads missing atlas data");
                }
                const unsigned entry = (texel >> ((u % per_word) * depth)) & ((1u << depth) - 1);
                if (!atlas_word(atlas, cx + entry, cy, &color)) FAIL("ANI palette reads missing atlas data");
                if (!color) continue;
                uint8_t *pixel = rgba + ((size_t)dy * width + (size_t)dx) * 4;
                for (unsigned c = 0; c < 3; ++c) {
                    unsigned channel = (color >> (c * 5)) & 31;
                    if ((attr & 0x10) && (color & 0x8000)) channel = channel / 2 + (pixel[c] >> 3) / 2;
                    pixel[c] = (uint8_t)((channel << 3) | (channel >> 2));
                }
                pixel[3] = 255;
            }
        }
    }
    return true;
}

#ifndef HMBTN_ASSET_DECODE_H
#define HMBTN_ASSET_DECODE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { HMBTN_ATLAS_WIDTH = 1024, HMBTN_ATLAS_HEIGHT = 512 };

/* CPU image atlas in original word coordinates, not executable PS1 memory. */
typedef struct HmbtnImageAtlas {
    uint16_t words[HMBTN_ATLAS_WIDTH * HMBTN_ATLAS_HEIGHT];
    uint8_t present[HMBTN_ATLAS_WIDTH * HMBTN_ATLAS_HEIGHT];
} HmbtnImageAtlas;

typedef struct HmbtnAnimation {
    const uint8_t *data; /* Borrowed; caller keeps the decoded file alive. */
    size_t size;
    uint16_t count[5];
    uint32_t section[5];
} HmbtnAnimation;

typedef struct HmbtnAnimationPlayer {
    uint16_t animation, frame, ticks;
} HmbtnAnimationPlayer;

bool hmbtn_bank_decode(const uint8_t *data, size_t size, HmbtnImageAtlas *atlas,
    char *error, size_t error_size);
/* Load another texture pack without erasing the previously loaded rectangles. */
bool hmbtn_bank_apply(const uint8_t *data, size_t size, HmbtnImageAtlas *atlas,
    char *error, size_t error_size);
bool hmbtn_bank_apply_at(const uint8_t *data, size_t size, HmbtnImageAtlas *atlas,
    int x, int y, int palette_x, int palette_y, char *error, size_t error_size);
bool hmbtn_texture_draw(const HmbtnImageAtlas *atlas, unsigned tx, unsigned ty,
    unsigned width, unsigned height, unsigned depth, unsigned cx, unsigned cy,
    int x, int y, uint8_t *rgba, unsigned output_width, unsigned output_height,
    char *error, size_t error_size);
bool hmbtn_animation_open(HmbtnAnimation *animation, const uint8_t *data, size_t size,
    char *error, size_t error_size);
bool hmbtn_animation_tick(const HmbtnAnimation *animation, HmbtnAnimationPlayer *player,
    char *error, size_t error_size);
bool hmbtn_animation_draw(const HmbtnAnimation *animation, const HmbtnImageAtlas *atlas,
    const HmbtnAnimationPlayer *player, int x, int y, uint8_t *rgba,
    unsigned width, unsigned height, size_t rgba_size, char *error, size_t error_size);

#endif

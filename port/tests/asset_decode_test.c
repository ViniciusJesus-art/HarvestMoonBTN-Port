#include "asset_decode.h"
#include "psx_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "Failed at line %d: %s (%s)\n", __LINE__, #condition, error); \
    free(atlas); return 1; } } while (0)

/* Entirely synthetic bank: 256-color palette and a 1x2-word compressed image. */
static void make_bank(uint8_t bank[548])
{
    memset(bank, 0, 548);
    bank[0] = 0x0D;
    hmbtn_write_le16(bank + 2, 2);
    hmbtn_write_le32(bank + 4, UINT32_C(0x06000200));
    hmbtn_write_le16(bank + 10, 4);
    hmbtn_write_le16(bank + 12, 256);
    hmbtn_write_le16(bank + 14, 1);
    hmbtn_write_le16(bank + 16 + 1 * 2, 0x001F);
    hmbtn_write_le16(bank + 16 + 2 * 2, 0x03E0);
    hmbtn_write_le16(bank + 16 + 3 * 2, 0x7C00);
    hmbtn_write_le16(bank + 16 + 4 * 2, 0x8000);
    hmbtn_write_le32(bank + 528, 5);
    hmbtn_write_le16(bank + 536, 1);
    hmbtn_write_le16(bank + 538, 2);
    bank[540] = 0xFC; /* -4: literal run */
    bank[541] = 0x10; bank[542] = 0x32;
    bank[543] = 0x34; bank[544] = 0x12;
}

/* Two images (pixels/palette), one part, two frames with a loop to frame zero. */
static void make_animation(uint8_t ani[92])
{
    static const uint16_t counts[5] = {2, 1, 1, 1, 2};
    static const uint32_t offsets[5] = {36, 52, 56, 68, 72};
    memset(ani, 0, 92);
    hmbtn_write_le32(ani, 0x0E);
    for (unsigned i = 0; i < 5; ++i) {
        hmbtn_write_le16(ani + 4 + i * 2, counts[i]);
        hmbtn_write_le32(ani + 16 + i * 4, offsets[i]);
    }
    hmbtn_write_le16(ani + 40, 1); /* image 0: width in words */
    hmbtn_write_le16(ani + 42, 2);
    hmbtn_write_le16(ani + 46, 4); /* image 1: palette at (0,4) */
    hmbtn_write_le16(ani + 48, 256);
    hmbtn_write_le16(ani + 50, 1);
    hmbtn_write_le16(ani + 52, 1); /* part list */
    hmbtn_write_le16(ani + 58, 1); /* part palette index */
    hmbtn_write_le16(ani + 68, 2); /* range frame count */
    ani[78] = 4; ani[88] = 4; ani[89] = 0x80;
}

int main(void)
{
    char error[256] = {0};
    uint8_t bank[548], ani[92], pixels[32];
    HmbtnImageAtlas *atlas = malloc(sizeof(*atlas));
    CHECK(atlas != NULL);
    make_bank(bank);
    make_animation(ani);
    CHECK(hmbtn_bank_decode(bank, sizeof(bank), atlas, error, sizeof(error)));
    CHECK(hmbtn_bank_apply_at(bank,sizeof(bank),atlas,320,1,320,-4,error,sizeof(error)));
    CHECK(atlas->present[1*1024+320] && atlas->words[1*1024+320]==0x3210);
    CHECK(atlas->present[321] && atlas->words[321]==0x001F); /* Separate CLUT offset. */
    CHECK(!hmbtn_bank_apply_at(bank,sizeof(bank),atlas,-1,0,0,0,error,sizeof(error)));
    CHECK(!hmbtn_bank_apply_at(bank,sizeof(bank),atlas,1024,0,0,0,error,sizeof(error)));
    CHECK(atlas->words[0] == 0x3210 && atlas->words[1024] == 0x1234);
    HmbtnAnimation animation;
    HmbtnAnimationPlayer player = {0};
    CHECK(hmbtn_animation_open(&animation, ani, sizeof(ani), error, sizeof(error)));
    memset(pixels, 91, sizeof(pixels));
    CHECK(hmbtn_animation_draw(&animation, atlas, &player, 0, 0, pixels, 4, 2,
        sizeof(pixels), error, sizeof(error)));
    CHECK(pixels[0] == 91); /* zero color is transparent */
    CHECK(pixels[4] == 255 && pixels[5] == 0 && pixels[6] == 0);
    CHECK(pixels[8] == 0 && pixels[9] == 255 && pixels[10] == 0);
    CHECK(pixels[14] == 255);
    CHECK(pixels[16] == 0 && pixels[17] == 0 && pixels[18] == 0 && pixels[19] == 255);
    ani[64] = 3; /* mirror X and Y: first output pixel is bottom-right red */
    CHECK(hmbtn_animation_draw(&animation, atlas, &player, 0, 0, pixels, 4, 2,
        sizeof(pixels), error, sizeof(error)));
    CHECK(pixels[0] == 255 && pixels[1] == 0 && pixels[2] == 0);
    ani[64] = 0x80; /* 8-bit indexed pixels */
    atlas->words[4 * 1024 + 16] = 0x001F;
    atlas->words[4 * 1024 + 50] = 0x03E0;
    CHECK(hmbtn_animation_draw(&animation, atlas, &player, 0, 0, pixels, 4, 2,
        sizeof(pixels), error, sizeof(error)));
    CHECK(pixels[0] == 255 && pixels[5] == 255);
    ani[64] = 0x10;
    atlas->words[4 * 1024 + 1] = 0x801F;
    memset(pixels, 0, sizeof(pixels));
    for (unsigned i = 2; i < sizeof(pixels); i += 4) pixels[i] = 255;
    CHECK(hmbtn_animation_draw(&animation, atlas, &player, 0, 0, pixels, 4, 2,
        sizeof(pixels), error, sizeof(error)));
    CHECK(pixels[4] == 123 && pixels[5] == 0 && pixels[6] == 123); /* STP averaging */
    CHECK(hmbtn_animation_draw(&animation, atlas, &player, -2, -1, pixels, 4, 2,
        sizeof(pixels), error, sizeof(error))); /* clipping */
    atlas->present[0] = 0;
    CHECK(!hmbtn_animation_draw(&animation, atlas, &player, 0, 0, pixels, 4, 2,
        sizeof(pixels), error, sizeof(error)));

    make_animation(ani);
    CHECK(hmbtn_animation_open(&animation, ani, sizeof(ani), error, sizeof(error)));
    CHECK(hmbtn_animation_tick(&animation, &player, error, sizeof(error)) && player.frame == 0);
    CHECK(hmbtn_animation_tick(&animation, &player, error, sizeof(error)) && player.frame == 1);
    CHECK(hmbtn_animation_tick(&animation, &player, error, sizeof(error)));
    CHECK(hmbtn_animation_tick(&animation, &player, error, sizeof(error)) && player.frame == 0);
    ani[79] = 0xFF; /* hold first frame */
    for (unsigned i = 0; i < 8; ++i) CHECK(hmbtn_animation_tick(&animation, &player, error, sizeof(error)));
    CHECK(player.frame == 0);

    make_animation(ani);
    for (size_t n = 0; n < sizeof(ani); ++n) {
        CHECK(!hmbtn_animation_open(&animation, ani, n, error, sizeof(error)));
    }
    ani[89] = 0x83;
    CHECK(!hmbtn_animation_open(&animation, ani, sizeof(ani), error, sizeof(error)));
    make_animation(ani);
    hmbtn_write_le16(ani+68,0); /* Reserved empty range, as in Duke's night pack. */
    CHECK(hmbtn_animation_open(&animation,ani,sizeof(ani),error,sizeof(error)));
    CHECK(!hmbtn_animation_tick(&animation,&player,error,sizeof(error)));
    hmbtn_write_le16(ani+70,30); /* Empty ranges still need a bounded offset. */
    CHECK(!hmbtn_animation_open(&animation,ani,sizeof(ani),error,sizeof(error)));
    make_animation(ani);
    hmbtn_write_le16(ani + 56, 99);
    CHECK(!hmbtn_animation_open(&animation, ani, sizeof(ani), error, sizeof(error)));
    make_animation(ani);
    hmbtn_write_le32(ani + 20, 36); /* overlapping sections */
    CHECK(!hmbtn_animation_open(&animation, ani, sizeof(ani), error, sizeof(error)));

    make_bank(bank);
    for (size_t n = 0; n < sizeof(bank); ++n) {
        CHECK(!hmbtn_bank_decode(bank, n, atlas, error, sizeof(error)));
    }
    bank[531] = 8; /* column RLE order */
    CHECK(hmbtn_bank_decode(bank, sizeof(bank), atlas, error, sizeof(error)));
    CHECK(atlas->words[0] == 0x3410 && atlas->words[1024] == 0x1232);
    make_bank(bank);
    bank[540] = 127; /* run exceeds image */
    CHECK(!hmbtn_bank_decode(bank, sizeof(bank), atlas, error, sizeof(error)));
    make_bank(bank);
    hmbtn_write_le16(bank + 532, 1024); /* out-of-atlas rectangle */
    CHECK(!hmbtn_bank_decode(bank, sizeof(bank), atlas, error, sizeof(error)));
    make_bank(bank);
    hmbtn_write_le32(bank + 528, 4);
    bank[540] = 4; bank[541] = 0x22; /* repeated byte */
    CHECK(hmbtn_bank_decode(bank, sizeof(bank), atlas, error, sizeof(error)));
    CHECK(atlas->words[0] == 0x2222 && atlas->words[1024] == 0x2222);
    hmbtn_write_le32(bank + 528, UINT32_C(0x04000004));
    bank[540] = 0x10; bank[541] = 0x32; bank[542] = 0x34; bank[543] = 0x12;
    CHECK(hmbtn_bank_decode(bank, sizeof(bank), atlas, error, sizeof(error)));
    CHECK(atlas->words[0] == 0x3210 && atlas->words[1024] == 0x1234);
    /* Loading a message pack preserves the shared font/icon rectangles. */
    atlas->words[7*1024+7] = 0x1234; atlas->present[7*1024+7] = 1;
    CHECK(hmbtn_bank_apply(bank,sizeof(bank),atlas,error,sizeof(error)));
    CHECK(atlas->words[7*1024+7] == 0x1234 && atlas->present[7*1024+7]);
    atlas->words[4*1024+1] = 0x001f;
    memset(pixels,0,sizeof(pixels));
    CHECK(hmbtn_texture_draw(atlas,0,0,4,1,4,0,4,0,0,pixels,4,2,error,sizeof(error)));
    CHECK(pixels[4] == 255 && pixels[5] == 0 && pixels[6] == 0);
    CHECK(!hmbtn_texture_draw(atlas,0,0,4,1,3,0,4,0,0,pixels,4,2,error,sizeof(error)));
    CHECK(!hmbtn_texture_draw(atlas,1024,0,4,1,4,0,4,0,0,pixels,4,2,error,sizeof(error)));
    CHECK(hmbtn_bank_decode(bank,sizeof(bank),atlas,error,sizeof(error)));
    CHECK(!atlas->present[7*1024+7]);
    free(atlas);
    return 0;
}

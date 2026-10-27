#ifndef HMBTN_LIVESTOCK_ART_H
#define HMBTN_LIVESTOCK_ART_H
#include "asset_decode.h"
typedef struct HmbtnLivestockArt {
    uint8_t *data;
    HmbtnImageAtlas *atlas;
    HmbtnAnimation animation[10];
} HmbtnLivestockArt;
bool hmbtn_livestock_art_load(HmbtnLivestockArt *,const char *root,char *,size_t);
void hmbtn_livestock_art_close(HmbtnLivestockArt *);
#endif

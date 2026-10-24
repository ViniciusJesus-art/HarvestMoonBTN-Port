#ifndef HMBTN_ESTATE_ART_H
#define HMBTN_ESTATE_ART_H
#include "asset_decode.h"
typedef struct HmbtnEstateArt { uint8_t *data;HmbtnImageAtlas *atlas;HmbtnAnimation sprites[45];unsigned row; } HmbtnEstateArt;
void hmbtn_estate_art_close(HmbtnEstateArt *art);
bool hmbtn_estate_art_update(HmbtnEstateArt *art,const char *root,unsigned row,char *error,size_t size);
const HmbtnAnimation *hmbtn_estate_art_sprite(const HmbtnEstateArt *art,unsigned sprite);
#endif

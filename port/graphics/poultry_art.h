#ifndef HMBTN_POULTRY_ART_H
#define HMBTN_POULTRY_ART_H
#include "asset_decode.h"
typedef struct HmbtnPoultryArt { uint8_t *data;HmbtnImageAtlas *atlas;HmbtnAnimation adult,chick; } HmbtnPoultryArt;
bool hmbtn_poultry_art_load(HmbtnPoultryArt *art,const char *root,char *error,size_t size);
void hmbtn_poultry_art_close(HmbtnPoultryArt *art);
#endif

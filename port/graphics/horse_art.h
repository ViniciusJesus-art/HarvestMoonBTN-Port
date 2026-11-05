#ifndef HMBTN_HORSE_ART_H
#define HMBTN_HORSE_ART_H
#include "asset_decode.h"
typedef struct HmbtnHorseArt { uint8_t *data[2];HmbtnImageAtlas *atlas[2];HmbtnAnimation animation[2]; } HmbtnHorseArt;
bool hmbtn_horse_art_load(HmbtnHorseArt *,const char *,char *,size_t);
void hmbtn_horse_art_close(HmbtnHorseArt *);
#endif

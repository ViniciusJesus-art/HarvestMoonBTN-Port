#ifndef HMBTN_VILLAGE_ART_H
#define HMBTN_VILLAGE_ART_H
#include "village.h"
typedef struct HmbtnVillageArt {
    uint8_t *blocks[2];HmbtnImageAtlas *atlases[2];
    HmbtnAnimation animations[2][37];
    unsigned keys[2];
} HmbtnVillageArt;
void hmbtn_village_art_close(HmbtnVillageArt *art);
bool hmbtn_village_art_update(HmbtnVillageArt *art,const char *root,unsigned primary,
    const HmbtnFarmClock *clock,char *error,size_t size);
const HmbtnAnimation *hmbtn_village_art_actor(const HmbtnVillageArt *art,unsigned id,const HmbtnImageAtlas **atlas);
#endif

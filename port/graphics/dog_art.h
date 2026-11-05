#ifndef HMBTN_DOG_ART_H
#define HMBTN_DOG_ART_H
#include "asset_decode.h"
typedef struct HmbtnDogArt { uint8_t *data[2];HmbtnImageAtlas *atlas[2];HmbtnAnimation animation[2]; } HmbtnDogArt;
bool hmbtn_dog_art_load(HmbtnDogArt *,const char *,char *,size_t);
void hmbtn_dog_art_close(HmbtnDogArt *);
#endif

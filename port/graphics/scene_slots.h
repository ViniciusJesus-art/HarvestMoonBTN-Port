#ifndef HMBTN_SCENE_SLOTS_H
#define HMBTN_SCENE_SLOTS_H
#include "asset_decode.h"
typedef struct HmbtnSceneFrame { uint16_t id,duration,next,x,y,sx,sy,w,h; } HmbtnSceneFrame;
typedef struct HmbtnSceneSlot { HmbtnSceneFrame frame;unsigned timer,count;bool active; } HmbtnSceneSlot;
typedef struct HmbtnSceneSlots { HmbtnSceneFrame frames[8];HmbtnSceneSlot slots[8];unsigned location,scene,background_step;bool background_reversed;uint8_t background[3]; } HmbtnSceneSlots;
bool hmbtn_scene_slots_open(HmbtnSceneSlots *,const uint8_t *,size_t,unsigned,char *,size_t);
void hmbtn_scene_slots_tick(HmbtnSceneSlots *,HmbtnImageAtlas *,bool,bool);
#endif

#ifndef HMBTN_AREA_DOORS_H
#define HMBTN_AREA_DOORS_H
#include "scene_decode.h"
/* Native adaptation of world/doors.c and farm_zone_trigger. Entries are
 * indexed by the decoded area's models, not the original global 10-slot pool.
 * Only transient door poses live here; routes remain owned by the village. */
typedef enum HmbtnDoorPhase { HMBTN_DOOR_CLOSED,HMBTN_DOOR_OPENING,HMBTN_DOOR_HELD,HMBTN_DOOR_CLOSING } HmbtnDoorPhase;
typedef struct HmbtnAreaDoor {
    HmbtnDoorPhase phase;
    int turn,speed,twin;
    unsigned facing,hold;
    bool opening_sound,closing_sound;
} HmbtnAreaDoor;
typedef struct HmbtnAreaDoors {
    HmbtnAreaDoor models[HMBTN_SCENE_MODELS];
    uint16_t base[HMBTN_SCENE_MODELS];
    bool bound;
} HmbtnAreaDoors;
typedef enum HmbtnDoorPassage { HMBTN_DOOR_UNHANDLED,HMBTN_DOOR_WAIT,HMBTN_DOOR_READY } HmbtnDoorPassage;
void hmbtn_area_doors_bind(HmbtnAreaDoors *,const HmbtnSceneData *);
HmbtnDoorPassage hmbtn_area_doors_request(HmbtnAreaDoors *,const uint8_t *,size_t,unsigned event,unsigned facing);
void hmbtn_area_doors_tick(HmbtnAreaDoors *,bool paused);
void hmbtn_area_doors_apply(const HmbtnAreaDoors *,HmbtnSceneData *);
/* Player transitions take exclusive ownership of their model (and twin). */
void hmbtn_area_doors_claim(HmbtnAreaDoors *,HmbtnSceneData *,unsigned model);
#endif

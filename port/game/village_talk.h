#ifndef HMBTN_VILLAGE_TALK_H
#define HMBTN_VILLAGE_TALK_H
#include "village.h"
#include "event_script.h"
#include "new_game.h"
#include "house_interaction.h"
/* Common empty-handed conversations, from event scripts 351..386 and Won 400. Story/shop
 * actions without a native implementation stop at an explicit boundary. */
typedef struct HmbtnVillageTalk {
    uint8_t code[3296];
    HmbtnEventScript script;
    int32_t slots[32];
    bool saved_flags[HMBTN_VILLAGE_FLAGS];
    HmbtnEventDays saved_days;
    uint16_t saved_talk_flags;
    unsigned saved_affection,saved_friendship;
    unsigned target,saved_facing,request,bank,message_pc;
    bool active,pending,loaded,message_done,released;
    unsigned result,boundary_type;
    HmbtnVillage *village;
    const HmbtnFarmClock *clock;
    const HmbtnNewGame *registration;
    const HmbtnIntroActor *player;
    const HmbtnHouseInteraction *inventory;
} HmbtnVillageTalk;
bool hmbtn_village_talk_range(unsigned id,uint32_t *offset,size_t *size);
bool hmbtn_village_talk_open(HmbtnVillageTalk *talk,unsigned id,const uint8_t *block,size_t size,
    HmbtnVillage *village,const HmbtnIntroActor *player,const HmbtnFarmClock *clock,
    const HmbtnNewGame *registration,const HmbtnHouseInteraction *inventory,char *error,size_t error_size);
bool hmbtn_village_talk_tick(HmbtnVillageTalk *talk,char *error,size_t error_size);
#endif

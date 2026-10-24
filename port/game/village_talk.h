#ifndef HMBTN_VILLAGE_TALK_H
#define HMBTN_VILLAGE_TALK_H
#include "village.h"
#include "event_script.h"
#include "new_game.h"
#include "house_interaction.h"
/* Common conversations and gifts, from event scripts 351..386 and Won 400. Story/shop
 * actions without a native implementation stop at an explicit boundary. */
typedef struct HmbtnVillageTalk {
    uint8_t code[3296];
    HmbtnEventScript script;
    int32_t slots[32];
    bool saved_flags[HMBTN_VILLAGE_FLAGS];
    HmbtnEventDays saved_days;
    HmbtnStoredTool saved_equipped,saved_bag[8],saved_toolbox[64];
    HmbtnStoredTool saved_held_item,saved_items[8];
    struct { uint16_t flags;unsigned affection,friendship; } saved_people[HMBTN_VILLAGERS];
    unsigned target,saved_facing,request,bank,message_pc;
    unsigned saved_event_count1;
    bool active,pending,loaded,message_done,released;
    unsigned result,boundary_type;
    /* Presentation request from event_item_store; consumed by the world UI. */
    unsigned show_tool,show_frames;
    HmbtnVillage *village;
    const HmbtnFarmClock *clock;
    const HmbtnNewGame *registration;
    const HmbtnIntroActor *player;
    HmbtnHouseInteraction *inventory;
} HmbtnVillageTalk;
bool hmbtn_village_talk_range(unsigned id,uint32_t *offset,size_t *size);
bool hmbtn_village_talk_open(HmbtnVillageTalk *talk,unsigned id,const uint8_t *block,size_t size,
    HmbtnVillage *village,const HmbtnIntroActor *player,const HmbtnFarmClock *clock,
    const HmbtnNewGame *registration,HmbtnHouseInteraction *inventory,char *error,size_t error_size);
bool hmbtn_village_talk_tick(HmbtnVillageTalk *talk,char *error,size_t error_size);
#endif

#ifndef HMBTN_VILLAGE_TALK_H
#define HMBTN_VILLAGE_TALK_H
#include "village.h"
#include "event_script.h"
#include "new_game.h"
#include "house_interaction.h"
/* Common conversations and gifts, from event scripts 351..386 and Won 400. Story/shop
 * actions without a native implementation stop at an explicit boundary. */
struct HmbtnFeedShop;
typedef struct HmbtnVillageTalk {
    uint8_t code[3296];
    HmbtnEventScript script;
    int32_t slots[32];
    bool saved_flags[HMBTN_VILLAGE_FLAGS];
    HmbtnEventDays saved_days;
    /* Rollback checkpoint only, never read by gameplay as a second inventory.
     * Unsupported script actions retain the existing atomic rollback. */
    HmbtnInventory saved_inventory;
    struct { uint16_t flags;unsigned affection,friendship; } saved_people[HMBTN_VILLAGERS];
    unsigned target,saved_facing,request,bank,message_pc;
    unsigned saved_event_count1;
    HmbtnGameProgress saved_progress; /* Rollback checkpoint, not gameplay state. */
    bool active,pending,loaded,message_done,released;
    unsigned result,boundary_type;
    /* Presentation request from event_item_store; consumed by the world UI. */
    unsigned show_tool,show_frames;
    unsigned show_item; /* event_cue_issue: carry ANI, separate from tool display. */
    HmbtnVillage *village;
    const HmbtnFarmClock *clock;
    const HmbtnRegistration *registration;
    const HmbtnIntroActor *player;
    HmbtnInventory *inventory; /* Live authoritative stores, borrowed from the game. */
    struct HmbtnFeedShop *shop; /* Borrowed optional session adapter. */
    HmbtnHouseInteraction *house; /* Non-inventory event counter; not migrated in MA-01. */
} HmbtnVillageTalk;
bool hmbtn_village_talk_range(unsigned id,uint32_t *offset,size_t *size);
bool hmbtn_village_talk_open(HmbtnVillageTalk *talk,unsigned id,const uint8_t *block,size_t size,
    HmbtnVillage *village,const HmbtnIntroActor *player,const HmbtnFarmClock *clock,
    const HmbtnRegistration *registration,HmbtnHouseInteraction *inventory,char *error,size_t error_size);
bool hmbtn_village_talk_tick(HmbtnVillageTalk *talk,char *error,size_t error_size);
#endif

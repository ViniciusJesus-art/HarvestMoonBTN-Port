#ifndef HMBTN_EVENT_ITEM_H
#define HMBTN_EVENT_ITEM_H
#include "farm_action.h"
typedef enum HmbtnEventItemPhase { HMBTN_EVENT_ITEM_IDLE,HMBTN_EVENT_ITEM_SHOW,HMBTN_EVENT_ITEM_EAT } HmbtnEventItemPhase;
typedef struct HmbtnEventItem { HmbtnEventItemPhase phase;unsigned shown;HmbtnStoredTool saved;int sound; } HmbtnEventItem;
/* event_items_take for the represented hand/rucksack/toolbox stores. Returns
 * copies seen, removes at most count records, and -1 for unavailable stores. */
int hmbtn_event_items_take(HmbtnInventory *inventory,int id,int where,int which,unsigned count);
int hmbtn_event_items_count(const HmbtnInventory *inventory,int id,int where,int which);
/* Original event_cue_issue(204,0,0,0) and receive_item(225,0,2,0).
 * Both reuse player base ANI and the existing item-picture interface. */
bool hmbtn_event_item_begin(HmbtnEventItem *item,HmbtnIntroActor *player,
    HmbtnInventory *inventory,unsigned id,bool eat);
void hmbtn_event_item_tick(HmbtnEventItem *item,HmbtnIntroActor *player,
    HmbtnInventory *inventory,HmbtnFarmAction *status);
#endif

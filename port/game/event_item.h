#ifndef HMBTN_EVENT_ITEM_H
#define HMBTN_EVENT_ITEM_H
#include "farm_action.h"
typedef enum HmbtnEventItemPhase { HMBTN_EVENT_ITEM_IDLE,HMBTN_EVENT_ITEM_SHOW,HMBTN_EVENT_ITEM_EAT,HMBTN_EVENT_ITEM_RUCKSACK,
    HMBTN_EVENT_ITEM_USE_FOOD,HMBTN_EVENT_ITEM_USE_DRINK,
    HMBTN_EVENT_ITEM_SERVE_FOOD,HMBTN_EVENT_ITEM_SERVE_DRINK } HmbtnEventItemPhase;
typedef struct HmbtnEventItem { HmbtnEventItemPhase phase;unsigned shown;HmbtnStoredTool saved;int sound;
    const HmbtnFarmClock *clock; /* Live loan for direct-use completion, not serialized. */
} HmbtnEventItem;
/* player_use_held_item(1)/stored_item_kind, ordinary foods with status callbacks.
 * Scripted receive/eat continues to restore the item set aside by that event. */
bool hmbtn_event_item_use(HmbtnEventItem *,HmbtnIntroActor *,HmbtnInventory *,const HmbtnFarmClock *);
bool hmbtn_event_item_consuming(const HmbtnEventItem *);
bool hmbtn_event_item_drinking(const HmbtnEventItem *);
/* player_receive_item(2/3): category-3 meals restore the original hand after use.
 * Served drink uses action row 17 (mode 0x4c), not direct-use row 14. */
bool hmbtn_event_item_receive(HmbtnEventItem *,HmbtnIntroActor *,HmbtnInventory *,
    const HmbtnFarmClock *,unsigned id);
/* event_items_take for the represented hand/rucksack/toolbox stores. Returns
 * copies seen, removes at most count records, and -1 for unavailable stores. */
int hmbtn_event_items_take(HmbtnInventory *inventory,int id,int where,int which,unsigned count);
int hmbtn_event_items_count(const HmbtnInventory *inventory,int id,int where,int which);
/* Original event_cue_issue(204,0,0,0) and receive_item(225,0,2,0).
 * Both reuse player base ANI and the existing item-picture interface. */
bool hmbtn_event_item_begin(HmbtnEventItem *item,HmbtnIntroActor *player,
    HmbtnInventory *inventory,unsigned id,bool eat);
/* player_show_item: shared base-ANI presentation, inventory receives once. */
bool hmbtn_event_item_show(HmbtnEventItem *,HmbtnIntroActor *,HmbtnInventory *,unsigned id);
/* player_show_item takes a record: wrapped gifts retain their contents modifier. */
bool hmbtn_event_item_show_record(HmbtnEventItem *,HmbtnIntroActor *,HmbtnInventory *,HmbtnStoredTool);
/* player_start_rucksack: base ANI 29..32 after the shop commits the purchase. */
bool hmbtn_event_item_rucksack(HmbtnEventItem *,HmbtnIntroActor *,HmbtnInventory *);
void hmbtn_event_item_tick(HmbtnEventItem *item,HmbtnIntroActor *player,
    HmbtnInventory *inventory,HmbtnFarmAction *status);
#endif

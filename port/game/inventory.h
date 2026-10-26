#ifndef HMBTN_INVENTORY_H
#define HMBTN_INVENTORY_H
#include "game_state.h"
#include <stddef.h>

typedef enum HmbtnInventoryStore { HMBTN_STORE_EQUIPPED,HMBTN_STORE_BAG,HMBTN_STORE_TOOLBOX } HmbtnInventoryStore;
typedef struct HmbtnInventoryPlacement { HmbtnInventoryStore store;unsigned index; } HmbtnInventoryPlacement;

/* event_items_take's represented stores only: where=0 tools, where=1 goods;
 * which=0 hand, 1 hand+bag, 2 tools+toolbox, -1 bag starting at slot ONE.
 * Count matching records, not stack units; remove at most count records.
 * Unsupported stores/capacity return -1 before mutation. */
int hmbtn_inventory_count(const HmbtnInventory *inventory,int id,int where,int which);
int hmbtn_inventory_take(HmbtnInventory *inventory,int id,int where,int which,unsigned count);

/* event_item_store's placement algorithm, without script/VRAM/pose effects.
 * Equipment, then first free accessible bag cell, then toolbox if spill=true.
 * Record is already resolved by the caller; preserve id/modifier/count exactly.
 * On failure neither inventory nor optional placement is modified. */
bool hmbtn_inventory_store_tool(HmbtnInventory *inventory,HmbtnStoredTool item,
    bool spill,HmbtnInventoryPlacement *placement);
/* shop_item_room / shop_item_add, mode=0. Quantity 1..99; if accessible
 * space cannot hold the entire lot, store it in the first free toolbox slot.
 * Failure is atomic. Placement reports the first record changed. */
unsigned hmbtn_inventory_seed_room(const HmbtnInventory *inventory,unsigned seed,bool accessible_only);
bool hmbtn_inventory_add_seeds(HmbtnInventory *inventory,unsigned seed,unsigned quantity,
    HmbtnInventoryPlacement *placement);
/* Convenience wrapper; same algorithm, no separate placement path. */
bool hmbtn_inventory_add_seed_packet(HmbtnInventory *inventory,unsigned seed,HmbtnInventoryPlacement *placement);
/* Empty-hand collection shared by field, gather and eggs; no pose effects. */
bool hmbtn_inventory_hold(HmbtnInventory *inventory,HmbtnStoredTool item);
void hmbtn_inventory_compact(HmbtnStoredTool *items,size_t count);
bool hmbtn_inventory_swap(HmbtnInventory *inventory,unsigned from,unsigned to);
bool hmbtn_inventory_cycle(HmbtnInventory *inventory,bool tools,bool reverse);
#endif

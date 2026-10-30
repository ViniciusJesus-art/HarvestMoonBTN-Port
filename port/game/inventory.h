#ifndef HMBTN_INVENTORY_H
#define HMBTN_INVENTORY_H
#include "game_state.h"
/* Existing native harvesting capacity, shared with carried lumber transfers.
 * The carpenter's purchase ceiling is a separate existing shop rule. */
unsigned hmbtn_inventory_lumber_limit(void);
/* shop_grant_rucksack_upgrade/milestone_set_status_bits: no extra capacity owner. */
unsigned hmbtn_inventory_rucksack_offer(const HmbtnInventory *);
bool hmbtn_inventory_buy_rucksack(HmbtnInventory *,HmbtnEconomy *,unsigned item);
void hmbtn_inventory_rucksack_new_day(HmbtnInventory *);
#include <stddef.h>

typedef enum HmbtnInventoryStore { HMBTN_STORE_EQUIPPED,HMBTN_STORE_BAG,HMBTN_STORE_TOOLBOX } HmbtnInventoryStore;
typedef struct HmbtnInventoryPlacement { HmbtnInventoryStore store;unsigned index; } HmbtnInventoryPlacement;

/* event_items_take's represented stores only: where=0 tools, where=1 goods;
 * which=0 hand, 1 hand+bag, 2 adds toolbox/icebox, 3 adds cabinet when where=0;
 * -1 bag starting at slot ONE.
 * Count matching records, not stack units; remove at most count records.
 * Invalid operands/capacity return -1 before mutation. */
int hmbtn_inventory_count(const HmbtnInventory *inventory,int id,int where,int which);
int hmbtn_inventory_take(HmbtnInventory *inventory,int id,int where,int which,unsigned count);

/* event_item_store's placement algorithm, without script/VRAM/pose effects.
 * Equipment, then first free accessible bag cell, then toolbox if spill=true.
 * Record is already resolved by the caller; preserve id/modifier/count exactly.
 * On failure neither inventory nor optional placement is modified. */
bool hmbtn_inventory_store_tool(HmbtnInventory *inventory,HmbtnStoredTool item,
    bool spill,HmbtnInventoryPlacement *placement);
bool hmbtn_inventory_can_store_tool(const HmbtnInventory *inventory,bool spill);
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
/* storage_item_message/cooking_item_prompt_tick: bank 3, wrapped contents + 1000. */
unsigned hmbtn_inventory_item_message(const HmbtnStoredTool *);
/* item_storage.c modes: toolbox=0, cabinet=1, icebox=2. Cells 0=hand,
 * 1..capacity=bag, 9..16=current page. Move/split/merge rules share live stores. */
HmbtnStoredTool *hmbtn_inventory_storage_slot(HmbtnInventory *,unsigned store,unsigned cell,unsigned page);
const HmbtnStoredTool *hmbtn_inventory_storage_slot_const(const HmbtnInventory *,unsigned store,unsigned cell,unsigned page);
bool hmbtn_inventory_storage_swap(HmbtnInventory *,unsigned store,unsigned from,unsigned from_page,unsigned to,unsigned to_page);
bool hmbtn_inventory_storage_accepts(unsigned store,unsigned item);
void hmbtn_inventory_compact(HmbtnStoredTool *items,size_t count);
bool hmbtn_inventory_swap(HmbtnInventory *inventory,unsigned from,unsigned to);
bool hmbtn_inventory_cycle(HmbtnInventory *inventory,bool tools,bool reverse);
#endif

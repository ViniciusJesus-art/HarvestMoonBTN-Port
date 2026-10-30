#ifndef HMBTN_COOKING_H
#define HMBTN_COOKING_H
#include "game_state.h"
/* menu/house.c: ordered recipe matching, ingredient groups, quality bonuses,
 * consumption from the live rucksack before the icebox, and saved recipes.
 * No SDL, rendering, global farm state or copied inventory. */
const HmbtnStoredTool *hmbtn_cooking_source(const HmbtnInventory *,unsigned page,unsigned cell);
/* cooking_item_stored, for record-view missing-ingredient marks. Empty is valid. */
bool hmbtn_cooking_item_stored(const HmbtnInventory *,unsigned id);
unsigned hmbtn_cooking_option_item(unsigned bit);
const uint16_t *hmbtn_cooking_dish_name(unsigned id);
unsigned hmbtn_cooking_allowed_options(const HmbtnCooking *);
unsigned hmbtn_cooking_record_count(const HmbtnCooking *);
unsigned hmbtn_cooking_record_slot(const HmbtnCooking *,unsigned id);
bool hmbtn_cooking_evaluate(unsigned options,const uint16_t values[8],HmbtnRecipeRecord *result);
/* Missing/repeated ingredients, unowned tools or occupied hands refuse atomically. */
bool hmbtn_cooking_available(const HmbtnCooking *,const HmbtnInventory *,unsigned options,const uint16_t values[8]);
bool hmbtn_cooking_make(const HmbtnCooking *,HmbtnInventory *,unsigned options,const uint16_t values[8],HmbtnRecipeRecord *result);
bool hmbtn_cooking_remember(HmbtnCooking *,const HmbtnRecipeRecord *);
#endif

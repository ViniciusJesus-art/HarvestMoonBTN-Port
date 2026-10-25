#ifndef HMBTN_INVENTORY_CHECKS_H
#define HMBTN_INVENTORY_CHECKS_H
#include "game_state.h"
#include <stdbool.h>

/* Compare domain values, not pointers, padding or host struct representation. */
static inline bool inventory_item_equal(HmbtnStoredTool a,HmbtnStoredTool b)
{
    return a.id==b.id && a.modifier==b.modifier && a.count==b.count;
}
static inline bool inventory_equal(const HmbtnInventory *a,const HmbtnInventory *b)
{
    if(!inventory_item_equal(a->equipped,b->equipped)
            || !inventory_item_equal(a->held_item,b->held_item)
            || a->rucksack_level!=b->rucksack_level) return false;
    for(unsigned i=0;i<8;++i)
        if(!inventory_item_equal(a->bag[i],b->bag[i]) || !inventory_item_equal(a->items[i],b->items[i])) return false;
    for(unsigned i=0;i<64;++i) if(!inventory_item_equal(a->toolbox[i],b->toolbox[i])) return false;
    for(unsigned i=0;i<4;++i) if(a->supplies[i]!=b->supplies[i]) return false;
    return true;
}
#endif

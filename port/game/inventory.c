/* Source base: src/overlay/11_GF_FARM/, reference d3c0f38 (src unchanged):
 * event/event_commands.c: event_items_take, event_item_store. Adapted traversal
 * and placement use explicit state, bounded native stores and no VRAM/pose calls.
 * menu/storage_swap.c: storage_try_swap, storage_stack_items/stack_count;
 * menu/item_storage.c: item_list_compact; people/player.c: exchange operations.
 * Retain host swap/cycle behavior, live-animal rejection, seed limit 99 and
 * zeroed empty records. Compact/cycle only accessible cells (inactive cells are
 * not inventory capacity). The native linear compaction is equivalent for live
 * records to the original hole-filling loop, and clears stale empty metadata.
 */
#include "inventory.h"
#include "farm_field.h"
#include <string.h>

static int scan_items(const HmbtnInventory *h,HmbtnInventory *write,
    int id,int where,int which,unsigned count)
{
    if(!hmbtn_inventory_capacity(h) || (where!=0 && where!=1)
            || which < -1 || which>2 || (where && which==2)) return -1;
    const unsigned first=which==-1 ? 1u : 0u,last=which==-1 ? 1u : (unsigned)which;
    int found=0;
    for(unsigned kind=first;kind<=last;++kind) {
        const HmbtnStoredTool *list=kind==0 ? (where ? &h->held_item : &h->equipped)
            : kind==1 ? (where ? h->items : h->bag) : h->toolbox;
        HmbtnStoredTool *output=!write ? NULL : kind==0 ? (where ? &write->held_item : &write->equipped)
            : kind==1 ? (where ? write->items : write->bag) : write->toolbox;
        const unsigned n=kind==0 ? 1u : kind==1 ? hmbtn_inventory_capacity(h) : hmbtn_inventory_toolbox_capacity(h);
        /* The original also starts the rucksack index at one for which=-1. */
        for(unsigned i=first;i<n;++i) if(list[i].id==id) {
            if(count && output) { output[i]=(HmbtnStoredTool){0};--count; }
            ++found;
        }
    }
    return found;
}
int hmbtn_inventory_take(HmbtnInventory *inventory,int id,int where,int which,unsigned count)
{
    return scan_items(inventory,inventory,id,where,which,count);
}
int hmbtn_inventory_count(const HmbtnInventory *inventory,int id,int where,int which)
{
    /* Same traversal/sentinels as the existing command, without a shadow store. */
    return scan_items(inventory,NULL,id,where,which,0);
}
bool hmbtn_inventory_store_tool(HmbtnInventory *h,HmbtnStoredTool item,
    bool spill,HmbtnInventoryPlacement *placement)
{
    const unsigned capacity=hmbtn_inventory_capacity(h);
    if(!capacity || !item.id) return false;
    HmbtnStoredTool *slot=NULL;
    HmbtnInventoryPlacement found={HMBTN_STORE_EQUIPPED,0};
    if(!h->equipped.id) slot=&h->equipped;
    for(unsigned i=0;!slot && i<capacity;++i)
        if(!h->bag[i].id) { slot=&h->bag[i];found=(HmbtnInventoryPlacement){HMBTN_STORE_BAG,i}; }
    if(spill) for(unsigned i=0;!slot && i<hmbtn_inventory_toolbox_capacity(h);++i)
        if(!h->toolbox[i].id) { slot=&h->toolbox[i];found=(HmbtnInventoryPlacement){HMBTN_STORE_TOOLBOX,i}; }
    if(!slot) return false;
    *slot=item;
    if(placement) *placement=found;
    return true;
}
/* Adapted shop_dialogues.c:shop_item_room/shop_item_add, mode=0 (d3c0f38).
 * Explicit shared state; picture/VRAM calls belong to presentation. */
unsigned hmbtn_inventory_seed_room(const HmbtnInventory *h,unsigned seed,bool accessible_only)
{
    const unsigned capacity=hmbtn_inventory_capacity(h);
    const HmbtnFarmItem *item=hmbtn_farm_item(seed);
    if(!capacity || !seed || !item || item->category!=2) return 0;
    if(!accessible_only) for(unsigned i=0;i<hmbtn_inventory_toolbox_capacity(h);++i)
        if(!h->toolbox[i].id) return 99;
    unsigned room=0;
    for(unsigned i=0;i<=capacity;++i) {
        const HmbtnStoredTool *slot=i ? &h->bag[i-1] : &h->equipped;
        if(!slot->id) room+=99;
        else if(slot->id==seed && slot->count<99) room+=99-slot->count;
    }
    return room;
}
bool hmbtn_inventory_add_seeds(HmbtnInventory *h,unsigned seed,unsigned quantity,
    HmbtnInventoryPlacement *placement)
{
    if(!quantity || quantity>99 || hmbtn_inventory_seed_room(h,seed,false)<quantity) return false;
    if(hmbtn_inventory_seed_room(h,seed,true)<quantity) {
        for(unsigned i=0;i<hmbtn_inventory_toolbox_capacity(h);++i) if(!h->toolbox[i].id) {
            h->toolbox[i]=(HmbtnStoredTool){(uint16_t)seed,0,(uint16_t)quantity};
            if(placement) *placement=(HmbtnInventoryPlacement){HMBTN_STORE_TOOLBOX,i};
            return true;
        }
        return false;
    }
    bool first=true;
    const unsigned capacity=hmbtn_inventory_capacity(h);
    for(unsigned i=0;quantity && i<=capacity;++i) {
        HmbtnStoredTool *slot=i ? &h->bag[i-1] : &h->equipped;
        if(slot->id && (slot->id!=seed || slot->count>=99)) continue;
        const unsigned old=slot->id ? slot->count : 0,space=99-old;
        const unsigned fill=quantity<space ? quantity : space;
        *slot=(HmbtnStoredTool){(uint16_t)seed,0,(uint16_t)(old+fill)};
        quantity-=fill;
        if(first && placement) *placement=(HmbtnInventoryPlacement){i ? HMBTN_STORE_BAG : HMBTN_STORE_EQUIPPED,i ? i-1 : 0};
        first=false;
    }
    return true;
}
bool hmbtn_inventory_add_seed_packet(HmbtnInventory *h,unsigned seed,HmbtnInventoryPlacement *placement)
{
    return hmbtn_inventory_add_seeds(h,seed,1,placement);
}
bool hmbtn_inventory_hold(HmbtnInventory *h,HmbtnStoredTool item)
{
    if(!h || !item.id || h->held_item.id) return false;
    h->held_item=item;return true;
}

static bool allowed(const HmbtnStoredTool *item,unsigned dest)
{
    if(!item->id) return true;
    if(item->id==0x109 || item->id==0x10A) return false; /* A live carried animal keeps its reserved record. */
    const HmbtnFarmItem *info=hmbtn_farm_item(item->id);
    return info && (dest==0 || dest==9 || (info->class_flags&0x80000000u));
}
bool hmbtn_inventory_swap(HmbtnInventory *h,unsigned from,unsigned to)
{
    HmbtnStoredTool *a=hmbtn_inventory_slot(h,from),*b=hmbtn_inventory_slot(h,to);
    if(!a || !b || a==b || !allowed(a,to) || !allowed(b,from)) return false;
    const HmbtnFarmItem *info=hmbtn_farm_item(a->id);
    if(a->id && a->id==b->id) {
        if(!info || info->category!=2) return false;
        unsigned sum=(unsigned)a->count+b->count;
        b->count=(uint16_t)(sum>99 ? 99 : sum);
        if(sum>99) a->count=(uint16_t)(sum-99); else *a=(HmbtnStoredTool){0};
        return true;
    }
    /* Original rejects exchanging a stack of crop goods with a nonempty slot. */
    const HmbtnFarmItem *other=hmbtn_farm_item(b->id);
    if(a->id && b->id && ((info && info->category==0 && a->count>=2)
            || (other && other->category==0 && b->count>=2))) return false;
    HmbtnStoredTool tmp=*a;*a=*b;*b=tmp;return true;
}
void hmbtn_inventory_compact(HmbtnStoredTool *items,size_t count)
{
    if(!items) return;
    size_t n=0;for(size_t i=0;i<count;++i) if(items[i].id) items[n++]=items[i];
    while(n<count) items[n++]=(HmbtnStoredTool){0};
}
bool hmbtn_inventory_cycle(HmbtnInventory *h,bool tools,bool reverse)
{
    const unsigned capacity=hmbtn_inventory_capacity(h);
    if(!capacity) return false;
    HmbtnStoredTool *slots=tools ? h->bag : h->items,*hand=tools ? &h->equipped : &h->held_item;
    const HmbtnFarmItem *info=hmbtn_farm_item(hand->id);
    if(hand->id && (!info || !(info->class_flags&0x80000000u))) return false;
    if(!tools && hand->id) {
        hmbtn_inventory_compact(slots,capacity);unsigned n=0;while(n<capacity && slots[n].id) ++n;
        if(n>=capacity) return false;
        slots[n]=*hand;*hand=(HmbtnStoredTool){0};return true;
    }
    hmbtn_inventory_compact(slots,capacity);
    if(!slots[0].id && !hand->id) return false;
    HmbtnStoredTool tmp;
    if(reverse) {
        unsigned n=0;while(n<capacity && slots[n].id) ++n;
        if(!n) { slots[0]=*hand;*hand=(HmbtnStoredTool){0};return true; }
        tmp=slots[n-1];memmove(slots+1,slots,(n-1)*sizeof(*slots));slots[0]=*hand;
    } else {
        tmp=slots[0];slots[0]=(HmbtnStoredTool){0};hmbtn_inventory_compact(slots,capacity);slots[capacity-1]=*hand;
    }
    *hand=tmp;hmbtn_inventory_compact(slots,capacity);return true;
}

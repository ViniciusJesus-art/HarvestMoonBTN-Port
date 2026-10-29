#include "event_item.h"
#include "inventory.h"
/* Compatibility entry points for current script consumers; all traversal and
 * removal is authoritative in inventory.c. */
int hmbtn_event_items_take(HmbtnInventory *inventory,int id,int where,int which,unsigned count)
{
    return hmbtn_inventory_take(inventory,id,where,which,count);
}
int hmbtn_event_items_count(const HmbtnInventory *inventory,int id,int where,int which)
{
    return hmbtn_inventory_count(inventory,id,where,which);
}
bool hmbtn_event_item_begin(HmbtnEventItem *item,HmbtnIntroActor *p,HmbtnInventory *h,unsigned id,bool eat)
{
    if(!eat) return id==204 && hmbtn_event_item_show(item,p,h,id);
    if(!item || !p || !h || p->facing>3 || item->phase!=HMBTN_EVENT_ITEM_IDLE
            || id!=225) return false;
    item->saved=h->held_item;item->shown=id;item->sound=74;
    item->phase=HMBTN_EVENT_ITEM_EAT;
    h->held_item=(HmbtnStoredTool){(uint16_t)id,0,1};
    p->pose=true;p->animation_done=false;
    p->sprite=(HmbtnAnimationPlayer){5,0,0};return true;
}
bool hmbtn_event_item_show(HmbtnEventItem *item,HmbtnIntroActor *p,HmbtnInventory *h,unsigned id)
{
    const HmbtnFarmItem *info=hmbtn_farm_item(id);
    if(!item || !p || !h || p->facing>3 || item->phase!=HMBTN_EVENT_ITEM_IDLE
            || h->held_item.id || !id || !info || info->category!=0 || info->picture<0) return false;
    item->saved=h->held_item;item->shown=id;item->sound=-1;item->phase=HMBTN_EVENT_ITEM_SHOW;
    p->pose=true;p->animation_done=false;p->sprite=(HmbtnAnimationPlayer){(uint16_t)(11+p->facing),0,0};return true;
}
void hmbtn_event_item_tick(HmbtnEventItem *item,HmbtnIntroActor *p,HmbtnInventory *h,HmbtnFarmAction *status)
{
    if(item->phase==HMBTN_EVENT_ITEM_IDLE || !p->animation_done) return;
    if(item->phase==HMBTN_EVENT_ITEM_SHOW) h->held_item=(HmbtnStoredTool){(uint16_t)item->shown,0,1};
    else {
        const HmbtnFarmItem *row=hmbtn_farm_item(item->shown);
        int stamina=(int)status->stamina+row->stamina,fatigue=(int)status->fatigue+row->fatigue;
        status->stamina=stamina<0 ? 0 : (unsigned)stamina>status->max_stamina ? status->max_stamina : (unsigned)stamina;
        status->fatigue=(unsigned)(fatigue<0 ? 0 : fatigue>100 ? 100 : fatigue);
        h->held_item=item->saved;
    }
    item->phase=HMBTN_EVENT_ITEM_IDLE;item->shown=0;p->pose=false;p->animation_done=false;
    p->sprite=(HmbtnAnimationPlayer){0,(uint16_t)p->facing,0};
}

#include "event_item.h"
static int scan_items(const HmbtnInventory *h,HmbtnInventory *write,
    int id,int where,int which,unsigned count)
{
    if(!h || h->rucksack_level>2 || (where!=0 && where!=1)
            || which < -1 || which>2 || (where && which==2)) return -1;
    const unsigned first=which==-1 ? 1u : 0u,last=which==-1 ? 1u : (unsigned)which;
    int found=0;
    for(unsigned kind=first;kind<=last;++kind) {
        const HmbtnStoredTool *list=kind==0 ? (where ? &h->held_item : &h->equipped)
            : kind==1 ? (where ? h->items : h->bag) : h->toolbox;
        HmbtnStoredTool *output=!write ? NULL : kind==0 ? (where ? &write->held_item : &write->equipped)
            : kind==1 ? (where ? write->items : write->bag) : write->toolbox;
        const unsigned n=kind==0 ? 1u : kind==1 ? 2u<<h->rucksack_level : 64u;
        /* The original also starts the rucksack index at one for which=-1. */
        for(unsigned i=first;i<n;++i) if(list[i].id==id) {
            if(count && output) { output[i]=(HmbtnStoredTool){0};--count; }
            ++found;
        }
    }
    return found;
}
int hmbtn_event_items_take(HmbtnInventory *inventory,int id,int where,int which,unsigned count)
{
    return scan_items(inventory,inventory,id,where,which,count);
}
int hmbtn_event_items_count(const HmbtnInventory *inventory,int id,int where,int which)
{
    /* Same traversal/sentinels as the existing command, without a shadow store. */
    return scan_items(inventory,NULL,id,where,which,0);
}
bool hmbtn_event_item_begin(HmbtnEventItem *item,HmbtnIntroActor *p,HmbtnInventory *h,unsigned id,bool eat)
{
    if(!item || !p || !h || p->facing>3 || item->phase!=HMBTN_EVENT_ITEM_IDLE
            || (eat ? id!=225 : id!=204 || h->held_item.id!=0)) return false;
    item->saved=h->held_item;item->shown=id;item->sound=eat ? 74 : -1;
    item->phase=eat ? HMBTN_EVENT_ITEM_EAT : HMBTN_EVENT_ITEM_SHOW;
    if(eat) h->held_item=(HmbtnStoredTool){(uint16_t)id,0,1};
    p->pose=true;p->animation_done=false;
    p->sprite=(HmbtnAnimationPlayer){(uint16_t)(eat ? 5 : 11+p->facing),0,0};return true;
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

#include "event_item.h"
#include "inventory.h"
bool hmbtn_event_item_consuming(const HmbtnEventItem *item)
{ return item && (item->phase==HMBTN_EVENT_ITEM_USE_FOOD || item->phase==HMBTN_EVENT_ITEM_USE_DRINK); }
bool hmbtn_event_item_drinking(const HmbtnEventItem *item)
{ return item && (item->phase==HMBTN_EVENT_ITEM_USE_DRINK || item->phase==HMBTN_EVENT_ITEM_SERVE_DRINK); }
bool hmbtn_event_item_receive(HmbtnEventItem *item,HmbtnIntroActor *p,HmbtnInventory *h,
    const HmbtnFarmClock *clock,unsigned id)
{
    const HmbtnFarmItem *row=hmbtn_farm_item(id);
    if(!item || !p || !h || !clock || p->facing>3 || item->phase!=HMBTN_EVENT_ITEM_IDLE
            || !row || row->category!=3 || !row->status_use || (row->flags&0x1200u)!=0x1200u) return false;
    const bool drink=(row->flags&0x1600u)==0x1600u;
    item->saved=h->held_item;item->shown=id;item->clock=clock;
    item->phase=drink ? HMBTN_EVENT_ITEM_SERVE_DRINK : HMBTN_EVENT_ITEM_SERVE_FOOD;
    item->sound=drink ? 75 : 74;h->held_item=(HmbtnStoredTool){(uint16_t)id,0,1};
    p->pose=true;p->animation_done=false;p->sprite=(HmbtnAnimationPlayer){(uint16_t)(drink ? 0 : 5),0,0};return true;
}
bool hmbtn_event_item_use(HmbtnEventItem *item,HmbtnIntroActor *p,HmbtnInventory *h,const HmbtnFarmClock *clock)
{
    if(!item || !p || !h || !clock || p->facing>3 || item->phase!=HMBTN_EVENT_ITEM_IDLE || !h->held_item.id || !h->held_item.count) return false;
    const HmbtnFarmItem *row=hmbtn_farm_item(h->held_item.id);
    /* Direct-use scope: carried foods/drinks using farm_clamp_status. Power
     * berries retain their existing automatic receipt/use flow. Oil, flour,
     * curry powder and wrapped contents do not acquire edible flags. */
    if(!row || row->category!=0 || !row->status_use || !(row->flags&0x1c00u)) return false;
    const bool drink=(row->flags&0x400u)!=0;
    item->saved=h->held_item;item->shown=h->held_item.id;item->clock=clock;
    item->phase=drink ? HMBTN_EVENT_ITEM_USE_DRINK : HMBTN_EVENT_ITEM_USE_FOOD;item->sound=drink ? 75 : 74;
    p->pose=true;p->animation_done=false;p->sprite=(HmbtnAnimationPlayer){(uint16_t)(drink ? 0 : 5),0,0};return true;
}
static unsigned clamp_status(unsigned current,int delta,unsigned limit)
{
    const int64_t value=(int64_t)current+delta;
    return value<0 ? 0 : value>limit ? limit : (unsigned)value;
}
static void use_status(const HmbtnFarmClock *clock,const HmbtnStoredTool *record,HmbtnFarmAction *status)
{
    const HmbtnFarmItem *row=hmbtn_farm_item(record->id);
    const unsigned raw=record->modifier&255u;
    const int first=row->flags&0x800u ? (raw<128 ? (int)raw : (int)raw-256) : row->stamina;
    status->stamina=clamp_status(status->stamina,first,status->max_stamina);
    if(row->fatigue) {
        status->fatigue=clamp_status(status->fatigue,row->fatigue,100);
        if(clock->hour==6 && !clock->minute && status->fatigue==100) status->fatigue=99;
    }
    /* ItemInfo's third callback argument also serves as the seed ANI field. */
    status->happiness=clamp_status(status->happiness,row->plant_animation,65535);
}
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
            || (id!=225 && id!=0x67)) return false;
    item->saved=h->held_item;item->shown=id;item->sound=74;
    item->phase=HMBTN_EVENT_ITEM_EAT;
    h->held_item=(HmbtnStoredTool){(uint16_t)id,0,1};
    p->pose=true;p->animation_done=false;
    p->sprite=(HmbtnAnimationPlayer){5,0,0};return true;
}
bool hmbtn_event_item_show(HmbtnEventItem *item,HmbtnIntroActor *p,HmbtnInventory *h,unsigned id)
{
    if(id>=380) return false;
    return hmbtn_event_item_show_record(item,p,h,(HmbtnStoredTool){(uint16_t)id,0,1});
}
bool hmbtn_event_item_show_record(HmbtnEventItem *item,HmbtnIntroActor *p,HmbtnInventory *h,HmbtnStoredTool record)
{
    const unsigned id=record.id;
    const HmbtnFarmItem *info=hmbtn_farm_item(id);
    const bool stock_cue=id==0x6d || id==0x6e || id==100 || id==108;
    /* Gift has picture=-1 in item_info, but a real per-id ANI/BANK block,
     * loaded by picture_load_to_vram just like the ordinary carried goods. */
    const bool gift=id==164 && record.modifier && record.modifier!=164 && hmbtn_farm_item(record.modifier);
    if(!item || !p || !h || p->facing>3 || item->phase!=HMBTN_EVENT_ITEM_IDLE
            || h->held_item.id || !id || !record.count || !info || (info->category!=0 && id!=0x63 && id!=0x66 && id!=0x67 && !stock_cue) || (info->picture<0 && id!=0x66 && id!=0x67 && id!=0x138 && !stock_cue && !gift)) return false;
    item->saved=record;item->shown=id;item->sound=-1;item->phase=HMBTN_EVENT_ITEM_SHOW;
    p->pose=true;p->animation_done=false;p->sprite=(HmbtnAnimationPlayer){(uint16_t)(11+p->facing),0,0};return true;
}
bool hmbtn_event_item_rucksack(HmbtnEventItem *item,HmbtnIntroActor *p,HmbtnInventory *h)
{
    if(!item || !p || !h || p->facing>3 || item->phase!=HMBTN_EVENT_ITEM_IDLE
            || (h->held_item.id!=0x6d && h->held_item.id!=0x6e)
            || h->rucksack_level!=(unsigned)h->held_item.id-0x6cu) return false;
    item->phase=HMBTN_EVENT_ITEM_RUCKSACK;item->shown=0;item->sound=8;
    p->pose=true;p->animation_done=false;p->sprite=(HmbtnAnimationPlayer){(uint16_t)(29+p->facing),0,0};return true;
}
void hmbtn_event_item_tick(HmbtnEventItem *item,HmbtnIntroActor *p,HmbtnInventory *h,HmbtnFarmAction *status)
{
    if(item->phase==HMBTN_EVENT_ITEM_IDLE || !p->animation_done) return;
    if(hmbtn_event_item_consuming(item)) {
        /* Input is blocked during this action; reject a stale hand if another
         * subsystem changed it rather than consuming an unrelated record. */
        if(h->held_item.id==item->saved.id && h->held_item.modifier==item->saved.modifier && h->held_item.count==item->saved.count) {
            use_status(item->clock,&item->saved,status);h->held_item=(HmbtnStoredTool){0};
        }
    } else if(item->phase==HMBTN_EVENT_ITEM_SERVE_FOOD || item->phase==HMBTN_EVENT_ITEM_SERVE_DRINK) {
        const HmbtnStoredTool received={(uint16_t)item->shown,0,1};
        use_status(item->clock,&received,status);h->held_item=item->saved;
    } else if(item->phase==HMBTN_EVENT_ITEM_SHOW) h->held_item=item->saved;
    else if(item->phase==HMBTN_EVENT_ITEM_RUCKSACK) h->held_item=(HmbtnStoredTool){0};
    else {
        const HmbtnFarmItem *row=hmbtn_farm_item(item->shown);
        if(item->shown==0x67) {
            status->max_stamina=status->max_stamina+10>255 ? 255 : status->max_stamina+10;
            status->stamina=status->max_stamina; /* farm_adjust_stamina_fatigue refills. */
        }
        int stamina=(int)status->stamina+row->stamina,fatigue=(int)status->fatigue+row->fatigue;
        status->stamina=stamina<0 ? 0 : (unsigned)stamina>status->max_stamina ? status->max_stamina : (unsigned)stamina;
        status->fatigue=(unsigned)(fatigue<0 ? 0 : fatigue>100 ? 100 : fatigue);
        h->held_item=item->saved;
    }
    item->phase=HMBTN_EVENT_ITEM_IDLE;item->shown=0;item->clock=NULL;p->pose=false;p->animation_done=false;
    p->sprite=(HmbtnAnimationPlayer){0,(uint16_t)p->facing,0};
}

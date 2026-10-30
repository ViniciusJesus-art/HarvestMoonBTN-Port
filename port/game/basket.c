/* estate_basket_add/merge/marker_stamp; player_use_held_item and mode finish.
 * One object, shared hand/economy, original strict probes and base ANI. */
#include "basket.h"
#include "farm_shipping.h"
#include "economy.h"
#include "world_map.h"
#include "pad_buttons.h"
#include <stdlib.h>
#include <string.h>
bool hmbtn_basket_buy(HmbtnBasket *b,HmbtnInventory *h,HmbtnEconomy *e)
{
    const HmbtnFarmItem *item=hmbtn_farm_item(100);
    if(!b || !h || !e || b->owned || h->held_item.id!=100 || e->gold<(unsigned)item->buy_price) return false;
    hmbtn_economy_gold_change(e,-(int64_t)item->buy_price,true);
    *b=(HmbtnBasket){.owned=true,.placed=true,.area=0,.position={-5000,0,2300}};
    h->held_item=(HmbtnStoredTool){0};return true;
}
bool hmbtn_basket_add(HmbtnBasket *b,const HmbtnEconomy *e,unsigned id)
{
    const unsigned price=hmbtn_farm_shipping_price(e,id);
    if(!b || !b->owned || !price || b->count>=30) return false;
    ++b->count;b->value+=price;
    const int bin=hmbtn_farm_shipping_bin(id);if(bin>=0) ++b->shipments[bin];return true;
}
bool hmbtn_basket_merge(HmbtnBasket *b,HmbtnEconomy *e)
{
    if(!b || !b->owned || !b->count || !e) return false;
    hmbtn_farm_shipping_merge(e,b->shipments,b->value);
    b->count=b->value=0;memset(b->shipments,0,sizeof(b->shipments));return true;
}
bool hmbtn_basket_near(const HmbtnBasket *b,unsigned area,const int32_t point[3])
{
    return b && b->owned && b->placed && b->area==area && abs(point[0]-b->position[0])<200
        && abs(point[2]-b->position[2])<200 && abs(point[1]-b->position[1])<20;
}
bool hmbtn_basket_blocked(const HmbtnBasket *b,unsigned area,const int32_t from[3],const int32_t to[3])
{
    if(!b || !b->owned || !b->placed || b->area!=area || abs(to[1]-b->position[1])>=200) return false;
    const int dx=abs(to[0]-b->position[0]),dz=abs(to[2]-b->position[2]);
    return dx<400 && dz<400 && (dx<abs(from[0]-b->position[0]) || dz<abs(from[2]-b->position[2]));
}
bool hmbtn_basket_valid(const HmbtnBasket *b,const HmbtnInventory *h)
{
    if(!b || !h || b->count>30) return false;
    unsigned sum=0,max=0;for(unsigned i=0;i<25;++i) { if(b->shipments[i]>b->count) return false;sum+=b->shipments[i]; }
    for(unsigned i=0;i<380;++i) { const HmbtnFarmItem *item=hmbtn_farm_item(i);if(item && item->sell_price>0 && (unsigned)item->sell_price>max) max=(unsigned)item->sell_price; }
    if((b->count && !b->value) || sum>b->count || b->value>b->count*(max+10) || (!b->count && b->value)) return false;
    for(unsigned i=0;i<8;++i) if(h->bag[i].id==100 || h->items[i].id==100) return false;
    for(unsigned i=0;i<64;++i) if(h->toolbox[i].id==100 || h->icebox[i].id==100 || h->cabinet[i].id==100) return false;
    if(h->equipped.id==100) return false;
    if(!b->owned) return !b->placed && !b->count && !b->value && h->held_item.id!=100 && !b->area
        && !b->position[0] && !b->position[1] && !b->position[2];
    if(b->placed==(h->held_item.id==100)) return false;
    unsigned logical,resource;
    return hmbtn_world_area(b->area,0,&logical,&resource) && logical==b->area
        && b->position[0]>=-1000000 && b->position[0]<=1000000 && b->position[1]>=-1000000 && b->position[1]<=1000000
        && b->position[2]>=-1000000 && b->position[2]<=1000000;
}
static void front(const HmbtnIntroActor *p,int32_t point[3],int reach)
{
    static const int direction[4][2]={{0,-1},{-1,0},{0,1},{1,0}};
    memcpy(point,p->position,3*sizeof(*point));point[0]+=direction[p->facing&3][0]*reach;point[2]+=direction[p->facing&3][1]*reach;
}
static bool drop(const HmbtnIntroActor *p,const HmbtnModel *ground,const HmbtnFarmField *field,int32_t point[3],
    bool (*blocked)(const void *,const int32_t[3],const int32_t[3]),const void *context)
{
    front(p,point,200);return !hmbtn_farm_field_probe(field,point)
        && (!blocked || !blocked(context,p->position,point)) && hmbtn_farm_settle(field,ground,point,0xc087,200);
}
static bool box(const HmbtnIntroActor *p,const HmbtnModel *ground)
{
    int32_t point[3];front(p,point,224);HmbtnGroundHit hit;
    return ground && hmbtn_ground_find(ground,point,0,960,&hit) && (hit.flags&0x80u);
}
bool hmbtn_basket_tick(HmbtnBasket *b,HmbtnBasketAction *a,HmbtnInventory *h,HmbtnEconomy *e,
    HmbtnIntroActor *p,unsigned area,const HmbtnModel *ground,const HmbtnFarmField *field,uint32_t pressed,
    bool (*blocked)(const void *,const int32_t[3],const int32_t[3]),const void *context)
{
    a->request=0;a->sound=-1;if(!b->owned || p->facing>3) return false;
    if(a->phase) {
        if(!p->animation_done) return true;
        if(a->phase==HMBTN_BASKET_DROP) {
            int32_t point[3];if(drop(p,ground,field,point,blocked,context)) {
                b->placed=true;b->area=area;memcpy(b->position,point,sizeof(b->position));h->held_item=(HmbtnStoredTool){0};a->sound=7;
            }
        } else if(a->phase==HMBTN_BASKET_ADD) {
            if(!hmbtn_basket_add(b,e,h->held_item.id)) a->request=0x383;
            /* Original clears the attempted goods even when all 30 slots are full. */
            h->held_item=(HmbtnStoredTool){0};a->sound=9;a->bounce=6;
        } else if(a->phase==HMBTN_BASKET_SHIP && box(p,ground) && hmbtn_basket_merge(b,e)) a->sound=0x13;
        a->phase=HMBTN_BASKET_FREE;p->pose=false;p->animation_done=false;
        p->sprite=(HmbtnAnimationPlayer){h->held_item.id ? 27 : 0,(uint16_t)p->facing,0};return true;
    }
    if(h->held_item.id==100) {
        if(pressed&HMBTN_PAD_SQUARE) {
            a->phase=HMBTN_BASKET_SHIP;p->sprite=(HmbtnAnimationPlayer){0,(uint16_t)p->facing,0};
        } else if(pressed&HMBTN_PAD_CROSS) {
            int32_t point[3];if(!drop(p,ground,field,point,blocked,context)) return true;
            a->phase=HMBTN_BASKET_DROP;p->sprite=(HmbtnAnimationPlayer){(uint16_t)(23+p->facing),0,0};
        } else return false;
    } else if(pressed&HMBTN_PAD_CROSS) {
        int32_t point[3];front(p,point,224);
        /* farm_record_hit_test uses the catalogue footprint for the player's
         * target; estate_request_near_active uses a smaller box for flights. */
        const HmbtnFarmItem *info=hmbtn_farm_item(100);
        if(!b->placed || b->area!=area || abs(point[0]-b->position[0])>=info->half_width
                || abs(point[2]-b->position[2])>=info->half_depth) return false;
        if(!h->held_item.id) {
            b->placed=false;h->held_item=(HmbtnStoredTool){100,0,1};a->sound=7;
            a->phase=HMBTN_BASKET_PICKUP;p->sprite=(HmbtnAnimationPlayer){(uint16_t)(11+p->facing),0,0};
        } else {
            if(h->held_item.count!=1 || !hmbtn_farm_shipping_price(e,h->held_item.id)) return false;
            a->phase=HMBTN_BASKET_ADD;p->sprite=(HmbtnAnimationPlayer){(uint16_t)(23+p->facing),0,0};
        }
    } else return false;
    p->pose=true;p->animation_done=false;return true;
}

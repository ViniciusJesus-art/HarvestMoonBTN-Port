/* Shared economy/container, original full-basket loss, probes, ANI and codec. */
#include "save_fixture.h"
#include "inventory.h"
#include "psx_data.h"
#include <stdio.h>
#include <stdlib.h>
static void floor_bytes(uint8_t b[88],unsigned flags)
{
    memset(b,0,88);hmbtn_write_le32(b,0x42);hmbtn_write_le32(b+8,1);hmbtn_write_le32(b+12,28);hmbtn_write_le32(b+16,4);
    hmbtn_write_le32(b+28,60);hmbtn_write_le32(b+32,1);
    for(unsigned i=0;i<4;++i) { hmbtn_write_le16(b+40+i*8,(uint16_t)(i&1 ? 2000 : -2000));hmbtn_write_le16(b+44+i*8,(uint16_t)(i<2 ? -2000 : 2000)); }
    hmbtn_write_le16(b+72,(uint16_t)flags);hmbtn_write_le16(b+74,0x2901);for(unsigned i=0;i<4;++i) hmbtn_write_le16(b+80+i*2,(uint16_t)i);
}
static bool solid(const void *context,const int32_t from[3],const int32_t to[3])
{ return hmbtn_basket_blocked(context,0,from,to); }
static void input(HmbtnSession *s,uint32_t pad)
{
    s->feed_shop.loaded=s->feed_shop.request;s->feed_shop.message.phase=HMBTN_MESSAGE_WAIT_END;
    hmbtn_feed_shop_tick(&s->feed_shop,&s->house,&s->game.economy,&s->menu.stats,pad,pad);
}
static void until(HmbtnSession *s,HmbtnFeedShopPhase phase)
{
    for(unsigned i=0;i<30 && (s->feed_shop.phase!=phase || s->feed_shop.closing);++i) {
        s->feed_shop.loaded=s->feed_shop.request;s->feed_shop.message.phase=HMBTN_MESSAGE_CLOSED;
        hmbtn_feed_shop_tick(&s->feed_shop,&s->house,&s->game.economy,&s->menu.stats,0,0);
    }
    assert(s->feed_shop.phase==phase && !s->feed_shop.closing);
}
static HmbtnSession *reload(HmbtnSession *s)
{
    char error[256];uint8_t *bytes=NULL;size_t n=0;assert(hmbtn_save_encode(s,&bytes,&n,error,sizeof(error)));
    HmbtnSession *next=hmbtn_save_decode(bytes,n,error,sizeof(error));assert(next && next->carry.basket==&next->game.basket && next->feed_shop.basket_state==&next->game.basket);
    free(bytes);hmbtn_session_destroy(s);static const uint8_t schedule[1]={0};assert(hmbtn_session_bind_schedules(next,schedule,1));return next;
}
static bool action(HmbtnSession *s,const HmbtnModel *ground,uint32_t pad)
{
    return hmbtn_basket_tick(&s->game.basket,&s->basket_action,&s->game.inventory,&s->game.economy,&s->intro.player,0,ground,NULL,pad,NULL,NULL);
}
int main(void)
{
    HmbtnSession *s=save_fixture(1);HmbtnInventory *h=&s->game.inventory;memset(h,0,sizeof(*h));h->rucksack_level=2;
    s->game.economy.gold=10000;s->game.clock.hour=12;s->game.clock.weekday=3;s->village.people[1].actor.visible=true;s->village.people[1].location=30;
    s->intro.player.position[0]=911;s->intro.player.position[1]=0;s->intro.player.position[2]=-600;s->intro.player.facing=3;
    uint8_t section[0x2000]={0},*z=section+16;hmbtn_write_le16(section+2,30);hmbtn_write_le16(z,0x4000);hmbtn_write_le16(z+2,37);
    hmbtn_write_le16(z+14,1000);hmbtn_write_le16(z+20,1600);hmbtn_write_le16(z+16,(uint16_t)-200);hmbtn_write_le16(z+18,(uint16_t)-800);hmbtn_write_le16(z+24,(uint16_t)-400);
    for(unsigned attempt=0;attempt<3;++attempt) {
        assert(hmbtn_basket_shop_open(&s->feed_shop,&s->village,&s->intro.player,&s->game.clock,180,section,sizeof(section),h));
        assert(s->feed_shop.item==100 && s->feed_shop.basket && hmbtn_farm_item(100)->buy_price==5000);
        assert(hmbtn_event_item_show(&s->event_item,&s->intro.player,h,100));s->intro.player.animation_done=true;
        hmbtn_event_item_tick(&s->event_item,&s->intro.player,h,&s->farm_action);s->feed_shop.cue_started=true;
        until(s,HMBTN_SHOP_OFFER);assert(s->feed_shop.request==0xd5);input(s,HMBTN_PAD_CROSS);
        if(!attempt) input(s,HMBTN_PAD_CIRCLE);
        else { s->game.economy.gold=attempt==1 ? 4999 : 10000;input(s,HMBTN_PAD_CROSS); }
        until(s,HMBTN_SHOP_RESULT);assert(s->feed_shop.request==(!attempt ? 0xba : attempt==1 ? 0x4f : 0xbb));
        until(s,HMBTN_SHOP_OFF);assert(!h->held_item.id && s->game.basket.owned==(attempt==2));
    }
    assert(s->game.economy.gold==5000 && s->game.basket.placed && s->game.basket.area==0);
    assert(s->game.basket.position[0]==-5000 && s->game.basket.position[2]==2300);
    assert(!hmbtn_basket_shop_open(&s->feed_shop,&s->village,&s->intro.player,&s->game.clock,180,section,sizeof(section),h));
    s=reload(s);h=&s->game.inventory;HmbtnBasket *b=&s->game.basket;
    uint8_t floor[88];floor_bytes(floor,0);HmbtnModel ground;char error[256];assert(hmbtn_model_open(&ground,floor,sizeof(floor),error,sizeof(error)));
    memset(b->position,0,sizeof(b->position));s->intro.player.position[0]=0;s->intro.player.position[2]=224;s->intro.player.facing=0;
    const int32_t edge[3]={200,0,0};assert(!hmbtn_basket_near(b,0,edge) && !hmbtn_basket_near(b,80,b->position));
    for(unsigned dir=0;dir<4;++dir) {
        static const int d[4][2]={{0,1},{1,0},{0,-1},{-1,0}};
        s->intro.player.facing=dir;s->intro.player.position[0]=b->position[0]+d[dir][0]*224;s->intro.player.position[2]=b->position[2]+d[dir][1]*(dir ? 224 : 623);
        assert(action(s,&ground,HMBTN_PAD_CROSS) && h->held_item.id==100 && !b->placed && s->basket_action.phase==HMBTN_BASKET_PICKUP);
        assert(!hmbtn_inventory_cycle(h,false,false) && !hmbtn_inventory_swap(h,9,0) && !hmbtn_inventory_swap(h,9,10));
        assert(!hmbtn_inventory_storage_accepts(0,100));s->intro.player.animation_done=true;assert(action(s,&ground,0));
        assert(hmbtn_basket_valid(b,h));s=reload(s);h=&s->game.inventory;b=&s->game.basket;
        floor_bytes(floor,0x80);assert(action(s,&ground,HMBTN_PAD_CROSS) && !s->basket_action.phase && h->held_item.id==100);
        floor_bytes(floor,0);assert(action(s,&ground,HMBTN_PAD_CROSS) && s->basket_action.phase==HMBTN_BASKET_DROP);
        s->intro.player.animation_done=true;assert(action(s,&ground,0) && b->placed && !h->held_item.id);
    }
    /* Running must respect the physical basket, independently of NPC routes. */
    for(unsigned run=0;run<2;++run) {
        HmbtnIntroActor actor={0};actor.position[0]=b->position[0];actor.position[2]=b->position[2]+600;
        HmbtnHouseControl control={.enabled=true,.object_blocked=solid,.collision_context=b};
        for(unsigned tick=0;tick<40;++tick) hmbtn_farm_control_tick(&control,&actor,&ground,NULL,HMBTN_PAD_DOWN|(run ? HMBTN_PAD_CIRCLE : 0));
        assert(actor.position[2]>=b->position[2]+400 && actor.position[2]<b->position[2]+600);
    }
    unsigned expected=0;const unsigned goods[]={0x41,0xf7,0xa1,0x5c};
    s->game.economy.shipping.honey_bonus=true;
    for(unsigned i=0;i<30;++i) { const unsigned id=goods[i%4];expected+=hmbtn_farm_shipping_price(&s->game.economy,id);assert(hmbtn_basket_add(b,&s->game.economy,id)); }
    assert(b->count==30 && b->value==expected && hmbtn_basket_valid(b,h));HmbtnBasket before=*b;
    assert(!hmbtn_basket_add(b,&s->game.economy,0x41) && !memcmp(&before,b,sizeof(before)));
    s->intro.player.facing=0;s->intro.player.position[0]=b->position[0];s->intro.player.position[2]=b->position[2]+224;
    h->held_item=(HmbtnStoredTool){0x41,0,1};assert(action(s,&ground,HMBTN_PAD_CROSS));s->intro.player.animation_done=true;
    assert(action(s,&ground,0) && s->basket_action.request==0x383 && !h->held_item.id && !memcmp(&before,b,sizeof(before)));
    s->basket_action.request=0;s=reload(s);h=&s->game.inventory;b=&s->game.basket;
    assert(action(s,&ground,HMBTN_PAD_CROSS));s->intro.player.animation_done=true;assert(action(s,&ground,0));
    assert(action(s,&ground,HMBTN_PAD_SQUARE));s->intro.player.animation_done=true;assert(action(s,&ground,0) && b->count==30);
    floor_bytes(floor,0x80);s->game.economy.shipments[0]=999;const unsigned pending=s->game.economy.shipping.pending_value,gold=s->game.economy.gold;
    assert(action(s,&ground,HMBTN_PAD_SQUARE));s->intro.player.animation_done=true;assert(action(s,&ground,0) && !b->count && !b->value);
    assert(s->game.economy.shipping.pending_value==pending+expected && s->game.economy.gold==gold && s->game.economy.shipments[0]==999);
    assert(!hmbtn_basket_merge(b,&s->game.economy) && h->held_item.id==100);
    s=reload(s);s->game.basket.count=31;uint8_t *bytes=NULL;size_t n=0;assert(!hmbtn_save_encode(s,&bytes,&n,error,sizeof(error)));
    hmbtn_session_destroy(s);puts("Basket: original purchase, unique carry/drop, full loss, counts/value, bulk transfer and resume passed");return 0;
}

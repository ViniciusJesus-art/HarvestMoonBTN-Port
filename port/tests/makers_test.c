/* Source-derived transaction/deadline/quality vectors, shared inventory and
 * strict map probes. No second purse, building capacity or ingredient store. */
#include "save_fixture.h"
#include "inventory.h"
#include "psx_data.h"
#include "farm_shipping.h"
#include <stdlib.h>
#include <stdio.h>
static void input(HmbtnSession *s,uint32_t pad)
{
    HmbtnFeedShop *shop=&s->feed_shop;shop->loaded=shop->request;shop->message.phase=HMBTN_MESSAGE_WAIT_END;
    hmbtn_feed_shop_tick(shop,&s->house,&s->game.economy,&s->menu.stats,pad,pad);
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
    char error[256];uint8_t *data=NULL;size_t bytes=0;
    assert(hmbtn_save_encode(s,&data,&bytes,error,sizeof(error)));
    HmbtnSession *next=hmbtn_save_decode(data,bytes,error,sizeof(error));assert(next);
    assert(next->feed_shop.makers==&next->makers && next->village.makers==&next->makers);
    free(data);hmbtn_session_destroy(s);
    static const uint8_t schedules[1]={0};assert(hmbtn_session_bind_schedules(next,schedules,1));return next;
}
int main(void)
{
    HmbtnSession *s=save_fixture(0);HmbtnMakers *m=&s->makers;
    memset(&s->game.inventory,0,sizeof(s->game.inventory));s->game.inventory.rucksack_level=2;
    s->game.economy.gold=100000;
    assert(!hmbtn_makers_offer(m,&s->construction,&s->poultry));
    s->construction.house_level=1;s->construction.barn_level=1;
    assert(!hmbtn_makers_offer(m,&s->construction,&s->poultry));
    s->poultry.upgraded=true;
    for(unsigned order=0x13f;order<=0x141;++order) {
        assert(hmbtn_makers_offer(m,&s->construction,&s->poultry)==order);
        assert(hmbtn_farm_item(order)->buy_price==20000);
        memset(s->game.inventory.items,0,sizeof(s->game.inventory.items));
        s->game.inventory.cabinet[0]=(HmbtnStoredTool){0x62,0,1};
        assert(!hmbtn_makers_order(m,&s->construction,&s->poultry,&s->game.inventory,&s->game.economy,order));
        s->game.inventory.cabinet[0]=(HmbtnStoredTool){0};
        s->game.inventory.held_item=(HmbtnStoredTool){0x62,0,1};
        assert(!hmbtn_makers_order(m,&s->construction,&s->poultry,&s->game.inventory,&s->game.economy,order));
        s->game.inventory.held_item=(HmbtnStoredTool){0};
        assert(hmbtn_blacksmith_shop_open(&s->feed_shop,&s->game.clock,&s->game.inventory));
        until(s,HMBTN_SHOP_SELECT);assert(hmbtn_blacksmith_rows(&s->feed_shop)==4);
        input(s,HMBTN_PAD_DOWN);input(s,HMBTN_PAD_DOWN);assert(s->feed_shop.selection==order-0x13f+2);
        input(s,HMBTN_PAD_CROSS);assert(s->feed_shop.next_request==0xa4);until(s,HMBTN_SHOP_SELECT);
        /* Two copies: consume only one. No requirement for an empty bag cell. */
        for(unsigned i=0;i<8;++i) s->game.inventory.items[i]=(HmbtnStoredTool){i<2 ? 0x62 : 0x125,0,1};
        const unsigned gold=s->game.economy.gold;s->game.economy.gold=19999;
        input(s,HMBTN_PAD_CROSS);assert(s->feed_shop.next_request==0xa4);until(s,HMBTN_SHOP_SELECT);
        assert(!m->pending && hmbtn_inventory_count(&s->game.inventory,0x62,1,1)==2);s->game.economy.gold=gold;
        input(s,HMBTN_PAD_CROSS);until(s,HMBTN_SHOP_MAKER_CONFIRM);
        input(s,HMBTN_PAD_DOWN);input(s,HMBTN_PAD_CROSS);assert(s->feed_shop.next_request==0x93);
        until(s,HMBTN_SHOP_SELECT);assert(!m->pending && s->game.economy.gold==gold);
        input(s,HMBTN_PAD_CROSS);until(s,HMBTN_SHOP_MAKER_CONFIRM);input(s,HMBTN_PAD_CROSS);until(s,HMBTN_SHOP_MAKER_AGAIN);
        input(s,HMBTN_PAD_CIRCLE);until(s,HMBTN_SHOP_SELECT);assert(!m->pending);
        input(s,HMBTN_PAD_CROSS);until(s,HMBTN_SHOP_MAKER_CONFIRM);input(s,HMBTN_PAD_CROSS);until(s,HMBTN_SHOP_MAKER_AGAIN);
        input(s,HMBTN_PAD_CROSS);until(s,HMBTN_SHOP_MAKER_CUE);
        assert(m->pending==order && m->mornings_left==5 && s->game.economy.gold==gold-20000);
        assert(hmbtn_inventory_count(&s->game.inventory,0x62,1,1)==1);
        s->feed_shop.cue_started=true;s->game.inventory.held_item=(HmbtnStoredTool){0x62,0,1};
        until(s,HMBTN_SHOP_OFF);s->feed_shop.departure=false;
        assert(!s->game.inventory.held_item.id && !hmbtn_makers_deliver(m));
        assert(!hmbtn_makers_order(m,&s->construction,&s->poultry,&s->game.inventory,&s->game.economy,order));
        assert(!hmbtn_blacksmith_shop_open(&s->feed_shop,&s->game.clock,&s->game.inventory));
        for(unsigned day=0;day<5;++day) {
            assert(hmbtn_village_flag(&s->village,0x37));
            /* The daily observer is idempotent for a repeated same date. */
            hmbtn_session_advance_minutes(s,1440);
            assert(m->mornings_left==4-day);
            hmbtn_session_clock_events(s,&s->game.clock,HMBTN_CLOCK_DAY);
            assert(m->mornings_left==4-day);
            s=reload(s);m=&s->makers;
        }
        assert(!hmbtn_village_flag(&s->village,0x37) && hmbtn_makers_deliver(m) && !hmbtn_makers_deliver(m));
        assert(s->construction.barn_level==1 && s->poultry.upgraded && hmbtn_makers_valid(m,&s->construction,&s->poultry));
    }
    assert(m->owned==7 && !hmbtn_makers_offer(m,&s->construction,&s->poultry));
    for(unsigned season=0;season<4;++season) {
        assert(hmbtn_makers_scene(m,112,116+season)==120+season);
        assert(hmbtn_makers_scene(m,96,100+season)==108+season);
    }
    for(unsigned i=0;i<8;++i) s->game.inventory.items[i]=(HmbtnStoredTool){0x125,0,1};
    static const unsigned pairs[][3]={{14,0x122,0x88},{14,0x123,0x89},{14,0x124,0x8a},{14,0x125,0x8a},{14,0xe8,0xe5},
        {33,0x8b,0x8e},{33,0x8c,0x8f},{33,0x8d,0x90},{33,0xe3,0xe4},
        {15,0x91,0x94},{15,0x92,0x95},{15,0x93,0x96},{15,0xe7,0xe6}};
    uint8_t section[0x2000]={0};uint8_t *z=section+16;
    hmbtn_write_le16(z,0x4000);hmbtn_write_le16(z+8,2);
    hmbtn_write_le16(z+14,(uint16_t)-100);hmbtn_write_le16(z+20,100);
    hmbtn_write_le16(z+18,100);hmbtn_write_le16(z+24,500);
    for(unsigned i=0;i<sizeof(pairs)/sizeof(*pairs);++i) {
        const unsigned kind=pairs[i][0],in=pairs[i][1],out=pairs[i][2],area=kind==14 ? 112 : 96;
        assert(hmbtn_makers_product(kind,in)==out && !hmbtn_makers_product(kind,0x62));
        hmbtn_write_le16(z+2,kind);HmbtnIntroActor *p=&s->intro.player;
        memset(&s->feed,0,sizeof(s->feed));p->position[0]=0;p->position[1]=0;p->position[2]=0;p->facing=2;
        s->game.inventory.held_item=(HmbtnStoredTool){(uint16_t)in,0,1};
        assert(hmbtn_farm_feed_makers_tick(&s->feed,m,&s->house,p,area,section,sizeof(section),HMBTN_PAD_CROSS));
        assert(s->feed.phase>=3 && p->sprite.animation==25 && s->game.inventory.held_item.id==in);
        char error[128];assert(!hmbtn_save_available(s,error,sizeof(error)));
        p->animation_done=true;
        assert(hmbtn_farm_feed_makers_tick(&s->feed,m,&s->house,p,area,section,sizeof(section),HMBTN_PAD_CROSS));
        assert(s->game.inventory.held_item.id==out && s->feed.phase==1 && p->sprite.animation==13 && s->feed.sound==7);
        p->animation_done=true;assert(hmbtn_farm_feed_tick(&s->feed,&s->house,p,section,sizeof(section),0));
        assert(!s->feed.phase && s->game.inventory.held_item.id==out);
        const unsigned value=s->game.economy.shipping.pending_value;
        assert(hmbtn_farm_shipping_add(&s->game.economy,out));
        assert(s->game.economy.shipping.pending_value==value+(unsigned)hmbtn_farm_item(out)->sell_price);
    }
    s->game.inventory.held_item=(HmbtnStoredTool){0x91,0,1};
    s->intro.player.facing=0;s->intro.player.position[2]=448;
    assert(hmbtn_farm_feed_makers_tick(&s->feed,m,&s->house,&s->intro.player,96,section,sizeof(section),HMBTN_PAD_CROSS));
    assert(!s->feed.phase && s->game.inventory.held_item.id==0x91);
    s->intro.player.facing=2;s->intro.player.position[2]=0;
    s->game.inventory.held_item=(HmbtnStoredTool){0};
    assert(hmbtn_farm_feed_makers_tick(&s->feed,m,&s->house,&s->intro.player,96,section,sizeof(section),HMBTN_PAD_CROSS));
    assert(s->feed.request==0x1a4 && !s->feed.phase);
    s->intro.player.position[0]=-100;
    assert(!hmbtn_farm_feed_makers_tick(&s->feed,m,&s->house,&s->intro.player,96,section,sizeof(section),HMBTN_PAD_CROSS));
    m->owned=1;assert(!hmbtn_farm_feed_makers_tick(&s->feed,m,&s->house,&s->intro.player,96,section,sizeof(section),HMBTN_PAD_CROSS));
    m->owned=7;s=reload(s);hmbtn_session_destroy(s);
    puts("Saibara orders, five mornings, saves, capacity and all maker qualities passed");return 0;
}

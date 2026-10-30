#include "save_fixture.h"
#include "psx_data.h"
#include "farm_gather.h"
#include <stdio.h>
static void tick(HmbtnSession *s,uint32_t pad,HmbtnMessagePhase phase)
{
    s->feed_shop.loaded=s->feed_shop.request;s->feed_shop.message.phase=phase;
    hmbtn_feed_shop_tick(&s->feed_shop,&s->house,&s->game.economy,&s->menu.stats,pad,pad);
}
static void closed(HmbtnSession *s,HmbtnFeedShopPhase phase)
{
    for(unsigned i=0;i<30 && (s->feed_shop.phase!=phase || s->feed_shop.closing);++i) tick(s,0,HMBTN_MESSAGE_CLOSED);
    assert(s->feed_shop.phase==phase && !s->feed_shop.closing);
}
static bool open_shop(HmbtnSession *s,uint8_t *section)
{ return hmbtn_fish_food_shop_open(&s->feed_shop,&s->village,&s->intro.player,&s->game.clock,180,section,0x2000,&s->game.inventory); }
static void cue(HmbtnSession *s)
{
    assert(hmbtn_event_item_show(&s->event_item,&s->intro.player,&s->game.inventory,108));
    s->intro.player.animation_done=true;hmbtn_event_item_tick(&s->event_item,&s->intro.player,&s->game.inventory,&s->farm_action);
    s->feed_shop.cue_started=true;closed(s,HMBTN_SHOP_GREETING);
}
int main(void)
{
    HmbtnSession *s=save_fixture(1);HmbtnInventory *h=&s->game.inventory;memset(h,0,sizeof(*h));
    s->village.people[1].location=30;s->village.people[1].actor.visible=true;s->game.clock.weekday=3;s->game.clock.hour=12;
    uint8_t section[0x2000]={0},*z=section+16;hmbtn_write_le16(section+2,30);hmbtn_write_le16(z,0x4000);hmbtn_write_le16(z+2,37);
    const HmbtnGatherRecord *record=NULL;
    for(unsigned i=0;i<HMBTN_GATHER_RECORDS;++i) { const HmbtnGatherRecord *r=hmbtn_farm_gather_record(i);if(r->location==30 && r->item==132) { record=r;break; } }
    assert(record);memcpy(s->intro.player.position,record->position,sizeof(record->position));s->intro.player.position[0]-=400;s->intro.player.position[1]=0;s->intro.player.facing=3;
    hmbtn_write_le16(z+14,(uint16_t)(record->position[0]-600));hmbtn_write_le16(z+20,(uint16_t)record->position[0]);
    hmbtn_write_le16(z+18,(uint16_t)(record->position[2]-200));hmbtn_write_le16(z+24,(uint16_t)(record->position[2]+200));
    for(unsigned day=0;day<7;++day) { s->game.clock.weekday=day;assert(open_shop(s,section)==(day!=0 && day!=2));s->feed_shop.phase=HMBTN_SHOP_OFF; }
    s->game.clock.weekday=3;s->village.people[1].actor.visible=false;assert(!open_shop(s,section));s->village.people[1].actor.visible=true;
    s->game.clock.hour=17;assert(!open_shop(s,section));s->game.clock.hour=12;
    h->held_item=(HmbtnStoredTool){132,0,1};assert(!open_shop(s,section));h->held_item=(HmbtnStoredTool){0};
    assert(hmbtn_farm_item(108)->buy_price==20);
    for(unsigned attempt=0;attempt<5;++attempt) {
        h->supplies[3]=attempt==4 ? 999 : 0;s->game.economy.gold=1000;
        const unsigned spent=s->game.economy.ledger[0][1];assert(open_shop(s,section) && s->feed_shop.fish_food);cue(s);
        if(attempt==4) { assert(s->feed_shop.request==0xd1);closed(s,HMBTN_SHOP_OFF);assert(h->supplies[3]==999 && !h->held_item.id);continue; }
        assert(s->feed_shop.request==0xd0);closed(s,HMBTN_SHOP_COUNT);
        tick(s,HMBTN_PAD_LEFT,HMBTN_MESSAGE_WAIT_END);assert(s->feed_shop.quantity==999);
        tick(s,HMBTN_PAD_RIGHT,HMBTN_MESSAGE_WAIT_END);assert(s->feed_shop.quantity==1);
        tick(s,HMBTN_PAD_UP,HMBTN_MESSAGE_WAIT_END);assert(s->feed_shop.quantity==11);
        tick(s,HMBTN_PAD_TRIANGLE,HMBTN_MESSAGE_WAIT_END);closed(s,HMBTN_SHOP_INFO);assert(s->feed_shop.request==0x25b);closed(s,HMBTN_SHOP_COUNT);
        s->game.economy.gold=219;tick(s,HMBTN_PAD_CROSS,HMBTN_MESSAGE_WAIT_END);closed(s,HMBTN_SHOP_RESULT);assert(s->feed_shop.request==0x4f);closed(s,HMBTN_SHOP_COUNT);
        assert(!h->supplies[3] && s->game.economy.gold==219);s->game.economy.gold=1000;
        h->supplies[3]=989;tick(s,HMBTN_PAD_CROSS,HMBTN_MESSAGE_WAIT_END);closed(s,HMBTN_SHOP_RESULT);assert(s->feed_shop.request==0xd2);closed(s,HMBTN_SHOP_COUNT);
        assert(h->supplies[3]==989 && s->game.economy.gold==1000);h->supplies[3]=0;
        tick(s,attempt ? HMBTN_PAD_CROSS : HMBTN_PAD_CIRCLE,HMBTN_MESSAGE_WAIT_END);closed(s,attempt ? HMBTN_SHOP_RESULT : HMBTN_SHOP_LEAVE);
        assert(s->feed_shop.request==(attempt ? 0xd3u : 0xbau) && !h->supplies[3] && s->game.economy.gold==1000);
        if(attempt==2) s->game.economy.gold=0;
        if(attempt==3) h->supplies[3]=999;
        if(attempt==2 || attempt==3) {
            closed(s,HMBTN_SHOP_COUNT);assert(s->game.economy.ledger[0][1]==spent);
            tick(s,HMBTN_PAD_CIRCLE,HMBTN_MESSAGE_WAIT_END);
        }
        closed(s,HMBTN_SHOP_OFF);assert(!h->held_item.id);
        assert(h->supplies[3]==(attempt==1 ? 11u : attempt==3 ? 999u : 0u));
        assert(s->game.economy.gold==(attempt==1 ? 780u : attempt==2 ? 0u : 1000u));
        assert(s->game.economy.ledger[0][1]==spent+(attempt==1 ? 220u : 0u));
    }
    hmbtn_session_destroy(s);puts("Fish food: original counter/hours, cue, 1..999 quantity/info/cancel, poor/full stock, delayed single charge and stale-acceptance guards passed");return 0;
}

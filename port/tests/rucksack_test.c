/* Original counter/key, purchase timing, inventory ownership and morning gate. */
#include "save_fixture.h"
#include "inventory.h"
#include "event_item.h"
#include "farm_gather.h"
#include "psx_data.h"
#include <stdio.h>
#include <stdlib.h>
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
    HmbtnSession *next=hmbtn_save_decode(bytes,n,error,sizeof(error));assert(next);
    free(bytes);hmbtn_session_destroy(s);static const uint8_t schedules[1]={0};
    assert(hmbtn_session_bind_schedules(next,schedules,1));return next;
}
static bool open(HmbtnSession *s,uint8_t *section)
{
    return hmbtn_rucksack_shop_open(&s->feed_shop,&s->village,&s->intro.player,&s->game.clock,180,section,0x2000,&s->game.inventory);
}
static void shown(HmbtnSession *s,uint8_t *section,unsigned item)
{
    assert(open(s,section) && s->feed_shop.rucksack && s->feed_shop.item==item);
    assert(hmbtn_event_item_show(&s->event_item,&s->intro.player,&s->game.inventory,item));
    s->intro.player.animation_done=true;hmbtn_event_item_tick(&s->event_item,&s->intro.player,&s->game.inventory,&s->farm_action);
    s->feed_shop.cue_started=true;s->feed_shop.loaded=0;s->feed_shop.request=0;
    hmbtn_feed_shop_tick(&s->feed_shop,&s->house,&s->game.economy,&s->menu.stats,0,0);
    assert(s->feed_shop.phase==HMBTN_SHOP_OFFER && s->feed_shop.request==(item==0x6d ? 0xd4u : 0x248u));
}
int main(void)
{
    HmbtnSession *s=save_fixture(1);memset(&s->game.inventory,0,sizeof(s->game.inventory));
    s->game.economy.gold=10000;s->game.clock.hour=12;s->game.clock.weekday=3;
    s->village.people[1].actor.visible=true;s->village.people[1].location=30;
    const HmbtnGatherRecord *record=hmbtn_farm_gather_record(115);assert(record && record->flags&0x8000u);
    memcpy(s->intro.player.position,record->position,sizeof(record->position));s->intro.player.position[0]-=400;
    s->intro.player.position[1]=0;s->intro.player.facing=3;
    uint8_t section[0x2000]={0},*z=section+16;hmbtn_write_le16(section+2,30);
    hmbtn_write_le16(z,0x4000);hmbtn_write_le16(z+2,37);
    hmbtn_write_le16(z+14,1000);hmbtn_write_le16(z+20,1600);
    hmbtn_write_le16(z+16,(uint16_t)-200);hmbtn_write_le16(z+22,0);
    hmbtn_write_le16(z+18,(uint16_t)-1200);hmbtn_write_le16(z+24,(uint16_t)-800);
    const unsigned prices[]={3000,5000};
    assert(hmbtn_inventory_capacity(&s->game.inventory)==2);
    for(unsigned level=0;level<2;++level) {
        HmbtnInventory *h=&s->game.inventory;const unsigned item=0x6d+level;
        assert(hmbtn_inventory_rucksack_offer(h)==item && hmbtn_farm_item(item)->buy_price==(int)prices[level]);
        s->game.clock.hour=8;assert(!open(s,section));s->game.clock.hour=17;assert(!open(s,section));s->game.clock.hour=12;
        for(unsigned day=0;day<3;day+=2) { s->game.clock.weekday=day;assert(!open(s,section)); }s->game.clock.weekday=3;
        s->village.people[1].actor.visible=false;assert(!open(s,section));s->village.people[1].actor.visible=true;
        hmbtn_write_le16(z+2,36);assert(!open(s,section));hmbtn_write_le16(z+2,37);
        s->intro.player.position[0]+=150;assert(!open(s,section));s->intro.player.position[0]-=150;
        h->held_item=(HmbtnStoredTool){0x61,0,1};assert(!open(s,section));h->held_item=(HmbtnStoredTool){0};
        /* A full accessible bag must neither block the upgrade nor lose records. */
        const unsigned capacity=hmbtn_inventory_capacity(h);
        h->equipped=(HmbtnStoredTool){21,0,37};
        for(unsigned i=0;i<capacity;++i) { h->bag[i]=(HmbtnStoredTool){34,0,(uint16_t)(11+i)};h->items[i]=(HmbtnStoredTool){0x61,(uint16_t)i,1}; }
        const HmbtnInventory before=*h;
        shown(s,section,item);input(s,HMBTN_PAD_CIRCLE);assert(s->feed_shop.phase==HMBTN_SHOP_MAKER_CONFIRM);
        input(s,HMBTN_PAD_DOWN);input(s,HMBTN_PAD_CROSS);assert(s->feed_shop.next_request==0xba);until(s,HMBTN_SHOP_OFF);
        assert(!memcmp(&before,h,sizeof(before)));
        shown(s,section,item);input(s,HMBTN_PAD_CROSS);s->game.economy.gold=prices[level]-1;
        input(s,HMBTN_PAD_CROSS);assert(s->feed_shop.next_request==0x4f);until(s,HMBTN_SHOP_OFF);
        assert(h->rucksack_level==level && !h->rucksack_mornings_left && !h->held_item.id);
        s->game.economy.gold=10000;
        shown(s,section,item);input(s,HMBTN_PAD_CROSS);input(s,HMBTN_PAD_CROSS);until(s,HMBTN_SHOP_RESULT);
        assert(s->feed_shop.request==0xbb && h->rucksack_level==level && s->game.economy.gold==10000);
        until(s,HMBTN_SHOP_RUCKSACK_UPGRADE);
        assert(s->game.economy.gold==10000-prices[level] && h->rucksack_level==level+1 && h->rucksack_mornings_left==10);
        assert(!hmbtn_inventory_buy_rucksack(h,&s->game.economy,item));
        assert(!memcmp(before.bag,h->bag,sizeof(h->bag)) && !memcmp(before.items,h->items,sizeof(h->items))
            && !memcmp(&before.equipped,&h->equipped,sizeof(h->equipped)));
        assert(hmbtn_event_item_rucksack(&s->event_item,&s->intro.player,h));
        assert(s->intro.player.sprite.animation==32 && s->event_item.sound==8);
        s->intro.player.animation_done=true;hmbtn_event_item_tick(&s->event_item,&s->intro.player,h,&s->farm_action);
        s->feed_shop.cue_started=true;until(s,HMBTN_SHOP_OFF);
        assert(!h->held_item.id && hmbtn_inventory_capacity(h)==(4u<<level));
        if(!level) for(unsigned day=0;day<10;++day) {
            assert(!hmbtn_inventory_rucksack_offer(h) && !open(s,section));s=reload(s);h=&s->game.inventory;
            hmbtn_session_advance_minutes(s,1440);assert(h->rucksack_mornings_left==9-day);
            hmbtn_session_clock_events(s,&s->game.clock,HMBTN_CLOCK_DAY);assert(h->rucksack_mornings_left==9-day);
        }
        s->game.clock.hour=12;s->game.clock.weekday=3;
    }
    assert(!hmbtn_inventory_rucksack_offer(&s->game.inventory) && !open(s,section));
    s=reload(s);assert(s->game.inventory.rucksack_level==2 && s->game.inventory.rucksack_mornings_left==10);
    s->game.inventory.rucksack_mornings_left=11;char error[128];uint8_t *bytes=NULL;size_t n=0;
    assert(!hmbtn_save_encode(s,&bytes,&n,error,sizeof(error)));hmbtn_session_destroy(s);
    puts("Rucksack: original counter, both purchases/cancel/refusal, 2/4/8 capacity, ten mornings and resume passed");return 0;
}

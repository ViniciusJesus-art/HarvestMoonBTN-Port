/* shop_buy_item_step: held record removal, gift contents and delayed debit. */
#include "save_fixture.h"
#include "inventory.h"
#include "psx_data.h"
#include <stdio.h>
#include <stdlib.h>
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
static bool open(HmbtnSession *s,uint8_t *section,unsigned area)
{
    return hmbtn_wrapping_shop_open(&s->feed_shop,&s->village,&s->intro.player,&s->game.clock,area,section,0x2000,&s->game.inventory);
}
int main(void)
{
    HmbtnSession *s=save_fixture(1);HmbtnInventory *h=&s->game.inventory;memset(h,0,sizeof(*h));
    s->game.clock.hour=12;s->game.clock.weekday=3;s->daily_date=hmbtn_session_date(&s->game.clock);
    s->village.people[1].actor.visible=true;s->village.people[1].location=30;s->game.economy.gold=1000;
    const HmbtnGatherRecord *r=hmbtn_farm_gather_record(118);assert(r->location==30 && r->item==0x177);
    memcpy(s->intro.player.position,r->position,sizeof(r->position));s->intro.player.position[0]-=400;s->intro.player.position[1]=0;s->intro.player.facing=3;
    uint8_t section[0x2000]={0},*z=section+16;hmbtn_write_le16(section+2,30);hmbtn_write_le16(z,0x4000);hmbtn_write_le16(z+2,37);
    hmbtn_write_le16(z+14,1000);hmbtn_write_le16(z+20,1600);hmbtn_write_le16(z+16,(uint16_t)-200);
    hmbtn_write_le16(z+18,(uint16_t)-1600);hmbtn_write_le16(z+24,(uint16_t)-1200);
    assert(hmbtn_farm_item(164)->buy_price==100 && hmbtn_farm_item(164)->picture==-1);
    const HmbtnStoredTool refused[]={{0},{164,154,1},{34,7,9},{100,0,1},{0x109,0,1}};
    const unsigned messages[]={0xb8,0xbc,0xb9,0xb9,0xb9};
    for(unsigned i=0;i<sizeof(refused)/sizeof(*refused);++i) {
        h->held_item=refused[i];const HmbtnInventory before=*h;const HmbtnEconomy money=s->game.economy;
        assert(open(s,section,180) && s->feed_shop.wrapping && s->feed_shop.phase==HMBTN_SHOP_RESULT && s->feed_shop.request==messages[i]);
        closed(s,HMBTN_SHOP_OFF);assert(!memcmp(h,&before,sizeof(before)) && !memcmp(&money,&s->game.economy,sizeof(money)));
    }
    h->held_item=(HmbtnStoredTool){154,7,5};
    assert(!open(s,section,80));s->game.clock.hour=17;assert(!open(s,section,180));s->game.clock.hour=12;
    s->game.clock.weekday=2;assert(!open(s,section,180));s->game.clock.weekday=3;
    s->village.people[1].actor.visible=false;assert(!open(s,section,180));s->village.people[1].actor.visible=true;
    hmbtn_write_le16(z+2,36);assert(!open(s,section,180));hmbtn_write_le16(z+2,37);
    s->intro.player.position[2]+=150;assert(!open(s,section,180));s->intro.player.position[2]-=150;
    /* Other stock keeps its empty-hand restriction at the same counter. */
    const HmbtnGatherRecord *bread=hmbtn_farm_gather_record(124);s->intro.player.position[0]=bread->position[0]+400;s->intro.player.position[2]=bread->position[2];s->intro.player.facing=1;
    assert(!hmbtn_grocery_shop_open(&s->feed_shop,&s->village,&s->intro.player,&s->game.clock,180,section,sizeof(section),h));
    s->intro.player.position[0]=r->position[0]-400;s->intro.player.position[2]=r->position[2];s->intro.player.facing=3;
    for(unsigned attempt=0;attempt<7;++attempt) {
        h->held_item=(HmbtnStoredTool){154,7,5};h->items[0]=(HmbtnStoredTool){154,9,1};h->items[1]=(HmbtnStoredTool){153,0,1};
        s->game.economy.gold=attempt==2 ? 99 : 1000;
        const HmbtnInventory before=*h;const unsigned gold=s->game.economy.gold,spent=s->game.economy.ledger[0][1];
        assert(open(s,section,180) && s->feed_shop.request==0xb6 && s->feed_shop.gift==154 && hmbtn_feed_shop_cost(&s->feed_shop)==100);
        tick(s,0,HMBTN_MESSAGE_WAIT_END);assert(s->feed_shop.phase==HMBTN_SHOP_MAKER_CONFIRM);
        if(attempt==0) tick(s,HMBTN_PAD_CIRCLE,HMBTN_MESSAGE_WAIT_END);
        else { if(attempt==1) tick(s,HMBTN_PAD_DOWN,HMBTN_MESSAGE_WAIT_END);tick(s,HMBTN_PAD_CROSS,HMBTN_MESSAGE_WAIT_END); }
        closed(s,HMBTN_SHOP_RESULT);assert(s->feed_shop.request==(attempt<2 ? 0xb7 : attempt==2 ? 0x4f : 0xbb));
        assert(!memcmp(h,&before,sizeof(before)) && s->game.economy.gold==gold && s->game.economy.ledger[0][1]==spent);
        if(attempt==4) s->game.economy.gold=0;
        if(attempt==5) h->held_item=(HmbtnStoredTool){153,0,1};
        if(attempt!=3 && attempt!=6) {
            closed(s,HMBTN_SHOP_OFF);
            assert(s->game.economy.gold==(attempt==4 ? 0 : gold) && s->game.economy.ledger[0][1]==spent);
            if(attempt==5) assert(h->held_item.id==153);else assert(!memcmp(h,&before,sizeof(before)));
            continue;
        }
        closed(s,HMBTN_SHOP_RUCKSACK_CUE);
        assert(!h->held_item.id && s->game.economy.gold==gold-100 && s->game.economy.ledger[0][1]==spent+100);
        assert(!memcmp(h->items,before.items,sizeof(h->items)) && h->rucksack_level==before.rucksack_level);
        const HmbtnStoredTool gift={164,154,1};
        assert(hmbtn_inventory_item_message(&gift)==1154 && hmbtn_inventory_item_message(&before.held_item)==154 && !hmbtn_inventory_item_message(NULL));
        assert(!hmbtn_event_item_show_record(&s->event_item,&s->intro.player,h,(HmbtnStoredTool){164,0,1}));
        assert(!hmbtn_event_item_show_record(&s->event_item,&s->intro.player,h,(HmbtnStoredTool){164,400,1}));
        assert(hmbtn_event_item_show_record(&s->event_item,&s->intro.player,h,gift));s->feed_shop.cue_started=true;
        assert(s->intro.player.sprite.animation==14 && !h->held_item.id);
        tick(s,HMBTN_PAD_CROSS|HMBTN_PAD_CIRCLE,HMBTN_MESSAGE_CLOSED);assert(s->feed_shop.phase==HMBTN_SHOP_RUCKSACK_CUE && !h->held_item.id);
        s->intro.player.animation_done=true;hmbtn_event_item_tick(&s->event_item,&s->intro.player,h,&s->farm_action);
        closed(s,HMBTN_SHOP_OFF);assert(!memcmp(&h->held_item,&gift,sizeof(gift)) && s->game.economy.ledger[0][1]==spent+100);
        assert(!hmbtn_inventory_cycle(h,false,false) && h->held_item.modifier==154);h->items[1]=(HmbtnStoredTool){0};
        assert(hmbtn_inventory_cycle(h,false,false) && !h->held_item.id && h->items[1].id==164 && h->items[1].modifier==154);
        assert(hmbtn_inventory_swap(h,9,11) && h->held_item.id==164 && h->held_item.modifier==154);
        for(unsigned store=0;store<3;++store) {
            const HmbtnInventory snapshot=*h;assert(!hmbtn_inventory_storage_accepts(store,164));
            if(store) assert(!hmbtn_inventory_storage_swap(h,store,0,0,9,0) && !memcmp(h,&snapshot,sizeof(snapshot)));
        }
        s->daily_date=hmbtn_session_date(&s->game.clock);
        char error[256];uint8_t *data=NULL;size_t size=0;assert(hmbtn_save_encode(s,&data,&size,error,sizeof(error)));
        HmbtnSession *loaded=hmbtn_save_decode(data,size,error,sizeof(error));assert(loaded && !memcmp(&loaded->game.inventory,h,sizeof(*h)) && loaded->game.economy.ledger[0][1]==spent+100);
        hmbtn_session_destroy(loaded);free(data);
    }
    hmbtn_session_destroy(s);puts("Wrapping: original counter/guards, refusals, deferred ledger, whole hand record, contents ANI, bag/deposit rules and resume passed");return 0;
}

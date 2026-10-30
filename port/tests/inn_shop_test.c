/* Original Doug delivery zone, frozen day/night menus, served meals and
 * receive_item restoration share the existing economy/status/inventory. */
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
static bool open_shop(HmbtnSession *s,uint8_t *section,unsigned area)
{ return hmbtn_inn_shop_open(&s->feed_shop,&s->village,&s->intro.player,&s->game.clock,area,section,0x2000,&s->game.inventory); }
int main(void)
{
    HmbtnSession *s=save_fixture(1);HmbtnInventory *h=&s->game.inventory;HmbtnFeedShop *shop=&s->feed_shop;
    memset(h,0,sizeof(*h));h->rucksack_level=2;h->held_item=(HmbtnStoredTool){164,154,3};
    for(unsigned i=0;i<8;++i) h->items[i]=(HmbtnStoredTool){153,0,1};
    uint8_t section[0x2000]={0},*z=section+16;hmbtn_write_le16(section+2,27);
    hmbtn_write_le16(z,0x4000);hmbtn_write_le16(z+2,36);hmbtn_write_le16(z+4,6);hmbtn_write_le16(z+8,0);
    hmbtn_write_le16(z+20,400);hmbtn_write_le16(z+24,400);hmbtn_write_le16(z+26,200);hmbtn_write_le16(z+30,100);
    s->intro.player.position[0]=200;s->intro.player.position[1]=0;s->intro.player.position[2]=424;s->intro.player.facing=0;
    HmbtnIntroActor *doug=&s->village.people[6].actor;doug->visible=true;doug->position[0]=200;doug->position[1]=0;doug->position[2]=100;
    assert(!open_shop(s,section,208));hmbtn_write_le16(z+2,37);assert(!open_shop(s,section,204));hmbtn_write_le16(z+2,36);
    hmbtn_write_le16(z+4,19);assert(!open_shop(s,section,204));hmbtn_write_le16(z+4,6);
    doug->visible=false;assert(!open_shop(s,section,204));doug->visible=true;doug->position[0]+=200;
    assert(!open_shop(s,section,204));doug->position[0]-=200;
    s->intro.player.facing=1;assert(!open_shop(s,section,204));s->intro.player.facing=0;
    const unsigned hours[]={0,5,6,17,18,23};
    for(unsigned i=0;i<6;++i) {
        s->game.clock.hour=hours[i];const bool night=hours[i]<6 || hours[i]>=18;const HmbtnInventory before=*h;
        assert(open_shop(s,section,204) && shop->inn_drinks==night && shop->request==(night ? 0x10c : 0x10b));
        assert(hmbtn_meal_shop_rows(shop)==(night ? 4u : 6u));s->game.clock.hour=night ? 12 : 20;
        assert(hmbtn_meal_shop_rows(shop)==(night ? 4u : 6u));closed(s,HMBTN_SHOP_SELECT);
        tick(s,HMBTN_PAD_CIRCLE,HMBTN_MESSAGE_WAIT_END);closed(s,HMBTN_SHOP_LEAVE);assert(shop->request==0x116);closed(s,HMBTN_SHOP_OFF);
        assert(!memcmp(h,&before,sizeof(before)));
    }
    const unsigned ids[2][6]={{168,304,305,306,307,171},{169,308,170,171}},price[2][6]={{500,300,300,250,200,0},{500,300,200,0}};
    const unsigned stamina[2][6]={{50,10,30,20,10,1},{0,0,0,1}},fatigue[2][6]={{10,20,1,1,1,0},{20,15,10,0}};
    for(unsigned night=0;night<2;++night) for(unsigned row=0;row<(night ? 4u : 6u);++row) {
        s->game.clock.hour=night ? 18 : 12;s->daily_date=hmbtn_session_date(&s->game.clock);s->intro.player.facing=0;
        s->game.economy.gold=1000;const HmbtnInventory inventory=*h;const unsigned spent=s->game.economy.ledger[0][1];
        assert(open_shop(s,section,204));closed(s,HMBTN_SHOP_SELECT);assert(!open_shop(s,section,204));
        const HmbtnSeedShopEntry *entry=hmbtn_meal_shop_entry(shop,row);assert(entry && entry->item==ids[night][row] && !hmbtn_meal_shop_entry(shop,hmbtn_meal_shop_rows(shop)));
        for(unsigned n=0;n<row;++n) tick(s,HMBTN_PAD_DOWN,HMBTN_MESSAGE_WAIT_END);
        assert(hmbtn_feed_shop_cost(shop)==price[night][row]);
        tick(s,HMBTN_PAD_TRIANGLE,HMBTN_MESSAGE_WAIT_END);closed(s,HMBTN_SHOP_INFO);assert(shop->request==entry->description);closed(s,HMBTN_SHOP_SELECT);
        if(price[night][row]) {
            s->game.economy.gold=price[night][row]-1;tick(s,HMBTN_PAD_CROSS,HMBTN_MESSAGE_WAIT_END);closed(s,HMBTN_SHOP_RESULT);assert(shop->request==0x4f);
            closed(s,HMBTN_SHOP_INFO);assert(shop->request==0x117);closed(s,HMBTN_SHOP_SELECT);
            assert(s->game.economy.gold==price[night][row]-1 && s->game.economy.ledger[0][1]==spent && !memcmp(h,&inventory,sizeof(inventory)));
        }
        s->game.economy.gold=price[night][row];tick(s,HMBTN_PAD_CROSS,HMBTN_MESSAGE_WAIT_END);
        assert(!s->game.economy.gold && s->game.economy.ledger[0][1]==spent+price[night][row]);closed(s,HMBTN_SHOP_RESULT);assert(shop->request==0x118);
        for(unsigned n=0;n<10;++n) tick(s,0,HMBTN_MESSAGE_WAIT_END);
        assert(!memcmp(h,&inventory,sizeof(inventory)));tick(s,0,HMBTN_MESSAGE_CLOSED);assert(shop->request==0x119);
        closed(s,HMBTN_SHOP_SERVE);assert(!shop->cue_started && !memcmp(h,&inventory,sizeof(inventory)));
        assert(!hmbtn_save_available(s,(char[256]){0},256));
        for(unsigned facing=0;facing<4;++facing) {
            s->intro.player.facing=facing;s->farm_action.max_stamina=100;s->farm_action.stamina=60;s->farm_action.fatigue=30;s->farm_action.happiness=10;
            const HmbtnFarmAction status=s->farm_action;
            assert(hmbtn_event_item_receive(&s->event_item,&s->intro.player,h,&s->game.clock,entry->item));
            const bool drink=(hmbtn_farm_item(entry->item)->flags&0x400u)!=0;
            assert(hmbtn_event_item_drinking(&s->event_item)==drink && !hmbtn_event_item_consuming(&s->event_item));
            assert(s->event_item.sound==(drink ? 75 : 74) && s->intro.player.sprite.animation==(drink ? 0 : 5));
            assert(h->held_item.id==entry->item && h->held_item.count==1 && !h->held_item.modifier);
            assert(!hmbtn_event_item_receive(&s->event_item,&s->intro.player,h,&s->game.clock,entry->item));
            for(unsigned n=0;n<5;++n) { hmbtn_event_item_tick(&s->event_item,&s->intro.player,h,&s->farm_action);assert(!memcmp(&status,&s->farm_action,sizeof(status))); }
            s->intro.player.animation_done=true;hmbtn_event_item_tick(&s->event_item,&s->intro.player,h,&s->farm_action);
            const unsigned expected=60+stamina[night][row]>100 ? 100 : 60+stamina[night][row];
            assert(!s->event_item.phase && s->farm_action.stamina==expected && s->farm_action.fatigue==30-fatigue[night][row] && s->farm_action.happiness==10);
            assert(!memcmp(h,&inventory,sizeof(inventory)));hmbtn_event_item_tick(&s->event_item,&s->intro.player,h,&s->farm_action);assert(s->farm_action.stamina==expected);
        }
        shop->cue_started=true;closed(s,HMBTN_SHOP_OFF);
        assert(s->game.economy.ledger[0][1]==spent+price[night][row] && !s->game.economy.gold);
    }
    const unsigned refused[]={0,153,159,180,225,380};
    for(unsigned i=0;i<sizeof(refused)/sizeof(*refused);++i) assert(!hmbtn_event_item_receive(&s->event_item,&s->intro.player,h,&s->game.clock,refused[i]));
    uint8_t *data=NULL;size_t bytes=0;char error[256];assert(hmbtn_save_encode(s,&data,&bytes,error,sizeof(error)));
    HmbtnSession *loaded=hmbtn_save_decode(data,bytes,error,sizeof(error));assert(loaded && !memcmp(h,&loaded->game.inventory,sizeof(*h)) && loaded->farm_action.fatigue==30);
    hmbtn_session_destroy(loaded);free(data);hmbtn_session_destroy(s);
    puts("Inn: original zone/presence, day/night bounds, 10 rows/free water, refusal/unique payment, receive food/drink, full-record restoration and resume passed");return 0;
}

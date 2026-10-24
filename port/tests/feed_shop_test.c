#include "feed_shop.h"
#include "psx_data.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Feed shop line %d: %s\n",__LINE__,#x);return 1; } } while(0)
static void ready(HmbtnFeedShop *s)
{
    s->loaded=s->request;s->message=(HmbtnMessage){0};
    s->message.words[0]=0xffff;s->message.count=1;s->message.phase=HMBTN_MESSAGE_WAIT_END;
}
static void tick(HmbtnFeedShop *s,HmbtnHouseInteraction *h,HmbtnFarmStatistics *stats,uint32_t pressed)
{
    hmbtn_feed_shop_tick(s,h,stats,pressed,pressed);
}
static bool advance(HmbtnFeedShop *s,HmbtnHouseInteraction *h,HmbtnFarmStatistics *stats,HmbtnFeedShopPhase target)
{
    for(unsigned i=0;i<200;++i) {
        if(s->phase==target && !s->closing) { if(target!=HMBTN_SHOP_OFF) ready(s);return true; }
        if(s->request!=s->loaded) ready(s);
        tick(s,h,stats,HMBTN_PAD_CROSS);
    }
    return false;
}
int main(void)
{
    HmbtnVillage v;hmbtn_village_init(&v,NULL,0);HmbtnHouseInteraction h;hmbtn_house_interaction_init(&h);
    HmbtnFarmStatistics stats={0};stats.gold=500;HmbtnFeedShop s={0};
    HmbtnIntroActor player={0};player.position[2]=224;
    v.people[15].location=9;v.people[15].actor.visible=true;
    v.people[18].location=v.people[20].location=49;v.people[18].actor.visible=true;
    HmbtnFarmClock clock={0,0,2,2,12,0,0};
    CHECK(!hmbtn_feed_shop_open(&s,&v,&player,&clock,136,0x68));
    player.facing=2;CHECK(!hmbtn_feed_shop_open(&s,&v,&player,&clock,136,0));player.facing=0;
    clock.weekday=1;CHECK(!hmbtn_feed_shop_open(&s,&v,&player,&clock,136,0));clock.weekday=2;
    clock.hour=8;CHECK(!hmbtn_feed_shop_open(&s,&v,&player,&clock,136,0));
    clock.hour=15;CHECK(!hmbtn_feed_shop_open(&s,&v,&player,&clock,136,0));clock.hour=9;
    CHECK(hmbtn_feed_shop_open(&s,&v,&player,&clock,136,0) && s.request==0x62);
    CHECK(!hmbtn_feed_shop_open(&s,&v,&player,&clock,136,0));
    CHECK(advance(&s,&h,&stats,HMBTN_SHOP_SELECT));tick(&s,&h,&stats,HMBTN_PAD_CROSS);
    CHECK(advance(&s,&h,&stats,HMBTN_SHOP_COUNT));
    tick(&s,&h,&stats,HMBTN_PAD_LEFT);CHECK(s.quantity==99);
    tick(&s,&h,&stats,HMBTN_PAD_RIGHT);CHECK(s.quantity==1);
    tick(&s,&h,&stats,HMBTN_PAD_UP);CHECK(s.quantity==11);
    tick(&s,&h,&stats,HMBTN_PAD_DOWN);CHECK(s.quantity==1);
    tick(&s,&h,&stats,HMBTN_PAD_TRIANGLE);CHECK(advance(&s,&h,&stats,HMBTN_SHOP_COUNT) && s.quantity==1);
    tick(&s,&h,&stats,HMBTN_PAD_CIRCLE);CHECK(advance(&s,&h,&stats,HMBTN_SHOP_OFF));
    CHECK(stats.gold==500 && !stats.ledger[0][1] && !h.supplies[0]);
    CHECK(hmbtn_feed_shop_open(&s,&v,&player,&clock,136,0));
    CHECK(advance(&s,&h,&stats,HMBTN_SHOP_SELECT));tick(&s,&h,&stats,HMBTN_PAD_CROSS);
    CHECK(advance(&s,&h,&stats,HMBTN_SHOP_COUNT));s.quantity=3;
    tick(&s,&h,&stats,HMBTN_PAD_CROSS);
    CHECK(stats.gold==440 && stats.ledger[0][1]==60 && h.supplies[0]==3);
    for(unsigned i=0;i<8;++i) tick(&s,&h,&stats,HMBTN_PAD_CROSS);
    CHECK(stats.gold==440 && h.supplies[0]==3); /* No double commit while closing. */
    CHECK(advance(&s,&h,&stats,HMBTN_SHOP_SELECT));tick(&s,&h,&stats,HMBTN_PAD_CROSS);
    CHECK(advance(&s,&h,&stats,HMBTN_SHOP_COUNT));h.supplies[0]=998;s.quantity=2;
    tick(&s,&h,&stats,HMBTN_PAD_CROSS);CHECK(h.supplies[0]==998 && stats.gold==440);
    CHECK(advance(&s,&h,&stats,HMBTN_SHOP_RESULT) && s.request==0x6D);
    CHECK(advance(&s,&h,&stats,HMBTN_SHOP_COUNT));s.quantity=1;tick(&s,&h,&stats,HMBTN_PAD_CROSS);
    CHECK(h.supplies[0]==999 && stats.gold==420);
    CHECK(advance(&s,&h,&stats,HMBTN_SHOP_SELECT));tick(&s,&h,&stats,HMBTN_PAD_CIRCLE);
    CHECK(advance(&s,&h,&stats,HMBTN_SHOP_OFF));
    clock.hour=10;CHECK(!hmbtn_feed_shop_open(&s,&v,&player,&clock,128,0));
    clock.hour=16;CHECK(!hmbtn_feed_shop_open(&s,&v,&player,&clock,128,0));
    clock.hour=11;clock.weekday=0;CHECK(!hmbtn_feed_shop_open(&s,&v,&player,&clock,128,0));clock.weekday=2;
    CHECK(hmbtn_feed_shop_open(&s,&v,&player,&clock,128,0) && s.request==0x44);
    CHECK(advance(&s,&h,&stats,HMBTN_SHOP_SELECT));tick(&s,&h,&stats,HMBTN_PAD_CROSS);
    CHECK(advance(&s,&h,&stats,HMBTN_SHOP_COUNT));s.quantity=99;
    tick(&s,&h,&stats,HMBTN_PAD_CROSS);CHECK(!h.supplies[2] && stats.gold==420);
    CHECK(advance(&s,&h,&stats,HMBTN_SHOP_RESULT) && s.request==0x4F);
    CHECK(advance(&s,&h,&stats,HMBTN_SHOP_COUNT));s.quantity=2;tick(&s,&h,&stats,HMBTN_PAD_CROSS);
    CHECK(h.supplies[2]==2 && stats.gold==400 && stats.ledger[0][1]==100);
    CHECK(!h.held_item.id && hmbtn_feed_shop_cost(&s)==20);
    CHECK(advance(&s,&h,&stats,HMBTN_SHOP_SELECT));tick(&s,&h,&stats,HMBTN_PAD_CIRCLE);
    CHECK(advance(&s,&h,&stats,HMBTN_SHOP_OFF));
    HmbtnPoultry poultry={0};s.poultry=&poultry;
    const uint16_t animal_name[9]={0,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF};
    for(unsigned i=0;i<5;++i) CHECK(hmbtn_poultry_buy(&poultry,animal_name,&clock));
    stats.gold=5000;const unsigned expenses=stats.ledger[0][1];
    CHECK(hmbtn_feed_shop_open(&s,&v,&player,&clock,128,0) && s.poultry==&poultry);
    CHECK(advance(&s,&h,&stats,HMBTN_SHOP_SELECT));tick(&s,&h,&stats,HMBTN_PAD_DOWN);
    CHECK(s.selection==1 && s.item==0x109 && hmbtn_feed_shop_cost(&s)==1500);
    tick(&s,&h,&stats,HMBTN_PAD_CROSS);CHECK(advance(&s,&h,&stats,HMBTN_SHOP_RESULT));
    CHECK(s.request==0x55 && stats.gold==5000 && stats.ledger[0][1]==expenses && hmbtn_poultry_count(&poultry)==5);
    uint8_t record[8192]={0};HmbtnNewGame registration;hmbtn_new_game_init(&registration);
    registration.player_name[0]=0;registration.farm_name[0]=1;
    hmbtn_write_le16(record+0x1800,0x400A);hmbtn_write_le16(record+0x1802,0xFFD);
    hmbtn_write_le16(record+0x1804,0x400D);hmbtn_write_le16(record+0x1806,0xFFFF);
    CHECK(hmbtn_feed_shop_expand(record,sizeof(record),&registration));
    CHECK(hmbtn_read_le16(record+0x1800)==0 && hmbtn_read_le16(record+0x1804)==1
        && hmbtn_read_le16(record+0x1806)==0xFFFF);
    CHECK(!hmbtn_feed_shop_expand(record,16,&registration));
    for(unsigned i=0;i<8;++i) registration.player_name[i]=0;
    for(unsigned i=0;i<1023;++i) hmbtn_write_le16(record+0x1800+i*2,0x400A);
    hmbtn_write_le16(record+0x1FFE,0xFFFF);
    CHECK(!hmbtn_feed_shop_expand(record,sizeof(record),&registration));
    puts("Feed shops: original hours/presence, quantity wrap, help, cancel, prices, stock cap and atomic ledger commits passed.");return 0;
}

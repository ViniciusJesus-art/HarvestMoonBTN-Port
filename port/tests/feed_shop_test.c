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
static void tick(HmbtnFeedShop *s,HmbtnHouseInteraction *h,HmbtnEconomy *economy,HmbtnFarmStatistics *stats,uint32_t pressed)
{
    hmbtn_feed_shop_tick(s,h,economy,stats,pressed,pressed);
}
static bool advance(HmbtnFeedShop *s,HmbtnHouseInteraction *h,HmbtnEconomy *economy,HmbtnFarmStatistics *stats,HmbtnFeedShopPhase target)
{
    for(unsigned i=0;i<200;++i) {
        if(s->phase==target && !s->closing) { if(target!=HMBTN_SHOP_OFF) ready(s);return true; }
        if(s->request!=s->loaded) ready(s);
        tick(s,h,economy,stats,HMBTN_PAD_CROSS);
    }
    return false;
}
int main(void)
{
    HmbtnVillage v;hmbtn_village_init(&v,NULL,0);HmbtnHouseInteraction h;HmbtnGameState h_state;hmbtn_game_state_init(&h_state);hmbtn_house_interaction_init(&h,&h_state);
    HmbtnFarmStatistics stats={0};HmbtnEconomy *economy=hmbtn_game_economy(&h_state);HmbtnFeedShop s={0};
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
    CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_SELECT));tick(&s,&h,economy,&stats,HMBTN_PAD_CROSS);
    CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_COUNT));
    tick(&s,&h,economy,&stats,HMBTN_PAD_LEFT);CHECK(s.quantity==99);
    tick(&s,&h,economy,&stats,HMBTN_PAD_RIGHT);CHECK(s.quantity==1);
    tick(&s,&h,economy,&stats,HMBTN_PAD_UP);CHECK(s.quantity==11);
    tick(&s,&h,economy,&stats,HMBTN_PAD_DOWN);CHECK(s.quantity==1);
    tick(&s,&h,economy,&stats,HMBTN_PAD_TRIANGLE);CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_COUNT) && s.quantity==1);
    tick(&s,&h,economy,&stats,HMBTN_PAD_CIRCLE);CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_OFF));
    CHECK(economy->gold==500 && !economy->ledger[0][1] && !h.inventory->supplies[0]);
    CHECK(hmbtn_feed_shop_open(&s,&v,&player,&clock,136,0));
    CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_SELECT));tick(&s,&h,economy,&stats,HMBTN_PAD_CROSS);
    CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_COUNT));s.quantity=3;
    tick(&s,&h,economy,&stats,HMBTN_PAD_CROSS);
    CHECK(economy->gold==440 && economy->ledger[0][1]==60 && h.inventory->supplies[0]==3);
    for(unsigned i=0;i<8;++i) tick(&s,&h,economy,&stats,HMBTN_PAD_CROSS);
    CHECK(economy->gold==440 && h.inventory->supplies[0]==3); /* No double commit while closing. */
    CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_SELECT));tick(&s,&h,economy,&stats,HMBTN_PAD_CROSS);
    CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_COUNT));h.inventory->supplies[0]=998;s.quantity=2;
    tick(&s,&h,economy,&stats,HMBTN_PAD_CROSS);CHECK(h.inventory->supplies[0]==998 && economy->gold==440);
    CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_RESULT) && s.request==0x6D);
    CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_COUNT));s.quantity=1;tick(&s,&h,economy,&stats,HMBTN_PAD_CROSS);
    CHECK(h.inventory->supplies[0]==999 && economy->gold==420);
    CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_SELECT));tick(&s,&h,economy,&stats,HMBTN_PAD_CIRCLE);
    CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_OFF));
    clock.hour=10;CHECK(!hmbtn_feed_shop_open(&s,&v,&player,&clock,128,0));
    clock.hour=16;CHECK(!hmbtn_feed_shop_open(&s,&v,&player,&clock,128,0));
    clock.hour=11;clock.weekday=0;CHECK(!hmbtn_feed_shop_open(&s,&v,&player,&clock,128,0));clock.weekday=2;
    CHECK(hmbtn_feed_shop_open(&s,&v,&player,&clock,128,0) && s.request==0x44);
    CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_SELECT));tick(&s,&h,economy,&stats,HMBTN_PAD_CROSS);
    CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_COUNT));s.quantity=99;
    tick(&s,&h,economy,&stats,HMBTN_PAD_CROSS);CHECK(!h.inventory->supplies[2] && economy->gold==420);
    CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_RESULT) && s.request==0x4F);
    CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_COUNT));s.quantity=2;tick(&s,&h,economy,&stats,HMBTN_PAD_CROSS);
    CHECK(h.inventory->supplies[2]==2 && economy->gold==400 && economy->ledger[0][1]==100);
    CHECK(!h.inventory->held_item.id && hmbtn_feed_shop_cost(&s)==20);
    CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_SELECT));tick(&s,&h,economy,&stats,HMBTN_PAD_CIRCLE);
    CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_OFF));
    HmbtnPoultry poultry={0};s.poultry=&poultry;
    const uint16_t animal_name[9]={0,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF};
    for(unsigned i=0;i<5;++i) CHECK(hmbtn_poultry_buy(&poultry,animal_name,&clock));
    economy->gold=5000;const unsigned expenses=economy->ledger[0][1];
    CHECK(hmbtn_feed_shop_open(&s,&v,&player,&clock,128,0) && s.poultry==&poultry);
    CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_SELECT));tick(&s,&h,economy,&stats,HMBTN_PAD_DOWN);
    CHECK(s.selection==1 && s.item==0x109 && hmbtn_feed_shop_cost(&s)==1500);
    tick(&s,&h,economy,&stats,HMBTN_PAD_CROSS);CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_RESULT));
    CHECK(s.request==0x55 && economy->gold==5000 && economy->ledger[0][1]==expenses && hmbtn_poultry_count(&poultry)==5);
    CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_SELECT));tick(&s,&h,economy,&stats,HMBTN_PAD_DOWN);
    CHECK(s.selection==2 && !s.item);
    tick(&s,&h,economy,&stats,HMBTN_PAD_TRIANGLE);
    CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_RESULT) && s.request==0x49);
    CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_SELECT));
    tick(&s,&h,economy,&stats,HMBTN_PAD_CROSS);CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_SELL_LIST));
    /* List wraps and scrolls over the four visible rows. */
    tick(&s,&h,economy,&stats,HMBTN_PAD_UP);CHECK(s.sale_cursor==4 && s.sale_top==1);
    tick(&s,&h,economy,&stats,HMBTN_PAD_DOWN);CHECK(!s.sale_cursor && !s.sale_top);
    tick(&s,&h,economy,&stats,HMBTN_PAD_CIRCLE);CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_RESULT) && s.request==0x5B);
    CHECK(economy->gold==5000 && hmbtn_poultry_count(&poultry)==5);
    CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_SELECT));
    tick(&s,&h,economy,&stats,HMBTN_PAD_CROSS);CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_SELL_LIST));
    tick(&s,&h,economy,&stats,HMBTN_PAD_CROSS);CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_SELL_CONFIRM) && s.request==0x60);
    tick(&s,&h,economy,&stats,HMBTN_PAD_DOWN);CHECK(s.answer==1);
    tick(&s,&h,economy,&stats,HMBTN_PAD_CROSS);CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_SELECT));
    CHECK(economy->gold==5000 && hmbtn_poultry_count(&poultry)==5);
    tick(&s,&h,economy,&stats,HMBTN_PAD_CROSS);CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_SELL_LIST));
    tick(&s,&h,economy,&stats,HMBTN_PAD_DOWN);CHECK(s.sale_cursor==1);
    tick(&s,&h,economy,&stats,HMBTN_PAD_CROSS);CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_SELL_CONFIRM));
    CHECK(s.sale_slot==1 && !s.answer);
    tick(&s,&h,economy,&stats,HMBTN_PAD_CROSS);CHECK(economy->gold==5500 && economy->ledger[0][0]==500);
    for(unsigned i=0;i<8;++i) tick(&s,&h,economy,&stats,HMBTN_PAD_CROSS);
    CHECK(economy->gold==5500 && economy->ledger[0][0]==500 && !poultry.hens[1].occupied);
    CHECK(stats.animals[0]==4 && economy->ledger[0][1]==expenses && hmbtn_poultry_room(&poultry));
    CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_SELECT));
    poultry.hens[4].prize=true;economy->gold=9999900;
    tick(&s,&h,economy,&stats,HMBTN_PAD_CROSS);CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_SELL_LIST));
    tick(&s,&h,economy,&stats,HMBTN_PAD_UP);CHECK(s.sale_cursor==3); /* Sparse record list. */
    tick(&s,&h,economy,&stats,HMBTN_PAD_CROSS);CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_SELL_CONFIRM) && s.request==0x61);
    CHECK(s.sale_slot==4);tick(&s,&h,economy,&stats,HMBTN_PAD_CIRCLE);
    CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_SELECT));CHECK(poultry.hens[4].occupied && economy->gold==9999900);
    tick(&s,&h,economy,&stats,HMBTN_PAD_CROSS);CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_SELL_LIST));
    tick(&s,&h,economy,&stats,HMBTN_PAD_UP);tick(&s,&h,economy,&stats,HMBTN_PAD_CROSS);
    CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_SELL_CONFIRM));tick(&s,&h,economy,&stats,HMBTN_PAD_CROSS);
    CHECK(economy->gold==9999999 && economy->ledger[0][0]==2500 && stats.animals[0]==3 && !poultry.hens[4].occupied);
    CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_SELECT));
    /* Incubation, chicks and deaths must never enter the sale list. */
    poultry.hens[0].stage=HMBTN_HEN_EGG;poultry.hens[2].stage=HMBTN_HEN_CHICK;poultry.hens[3].dead=true;
    tick(&s,&h,economy,&stats,HMBTN_PAD_CROSS);CHECK(advance(&s,&h,economy,&stats,HMBTN_SHOP_RESULT) && s.request==0x59);
    CHECK(!hmbtn_poultry_sell(&poultry,HMBTN_HENS) && !hmbtn_poultry_sell(&poultry,0)
        && !hmbtn_poultry_sell(&poultry,1) && !hmbtn_poultry_sell(&poultry,2) && !hmbtn_poultry_sell(&poultry,3));
    CHECK(economy->gold==9999999 && economy->ledger[0][0]==2500 && economy->ledger[0][1]==expenses);
    CHECK(hmbtn_poultry_buy(&poultry,animal_name,&clock) && poultry.hens[1].occupied);
    CHECK(hmbtn_poultry_sale_slot(&poultry,0)==1 && hmbtn_poultry_sale_slot(&poultry,1)==HMBTN_HENS);
    uint8_t record[8192]={0};HmbtnRegistration registration;hmbtn_registration_init(&registration);
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
    HmbtnHouseInteraction meds;HmbtnGameState meds_state;hmbtn_game_state_init(&meds_state);hmbtn_house_interaction_init(&meds,&meds_state);
    HmbtnFeedShop ms={0};ms.poultry=&poultry;HmbtnFarmStatistics medicine_stats={0};HmbtnEconomy *money=hmbtn_game_economy(&meds_state);money->gold=999;
    meds.inventory->toolbox[63]=(HmbtnStoredTool){0x1E,0,1};
    CHECK(hmbtn_feed_shop_open(&ms,&v,&player,&clock,128,0));
    CHECK(advance(&ms,&meds,money,&medicine_stats,HMBTN_SHOP_SELECT) && !ms.medicine_available);
    tick(&ms,&meds,money,&medicine_stats,HMBTN_PAD_UP);CHECK(ms.selection==2); /* Medicine hidden, even in toolbox. */
    tick(&ms,&meds,money,&medicine_stats,HMBTN_PAD_DOWN);CHECK(!ms.selection);
    meds.inventory->toolbox[63]=(HmbtnStoredTool){0};tick(&ms,&meds,money,&medicine_stats,0);
    tick(&ms,&meds,money,&medicine_stats,HMBTN_PAD_UP);CHECK(ms.selection==3 && ms.item==0x1E && hmbtn_feed_shop_cost(&ms)==1000);
    tick(&ms,&meds,money,&medicine_stats,HMBTN_PAD_TRIANGLE);CHECK(advance(&ms,&meds,money,&medicine_stats,HMBTN_SHOP_RESULT) && ms.request==0x48);
    CHECK(advance(&ms,&meds,money,&medicine_stats,HMBTN_SHOP_SELECT));tick(&ms,&meds,money,&medicine_stats,HMBTN_PAD_CROSS);
    CHECK(advance(&ms,&meds,money,&medicine_stats,HMBTN_SHOP_RESULT) && ms.request==0x4F && money->gold==999 && !money->ledger[0][1]);
    CHECK(advance(&ms,&meds,money,&medicine_stats,HMBTN_SHOP_SELECT));money->gold=1200;
    meds.inventory->equipped=(HmbtnStoredTool){1,0,1};meds.inventory->bag[0]=(HmbtnStoredTool){6,0,1};meds.inventory->bag[1]=(HmbtnStoredTool){11,0,1};
    const HmbtnInventory full=*meds.inventory;
    tick(&ms,&meds,money,&medicine_stats,HMBTN_PAD_CROSS);
    CHECK(advance(&ms,&meds,money,&medicine_stats,HMBTN_SHOP_RESULT) && ms.request==0x54 && !memcmp(&full,meds.inventory,sizeof(full)) && money->gold==1200);
    CHECK(advance(&ms,&meds,money,&medicine_stats,HMBTN_SHOP_SELECT));meds.inventory->bag[1]=(HmbtnStoredTool){0};
    tick(&ms,&meds,money,&medicine_stats,HMBTN_PAD_CROSS);CHECK(money->gold==200 && money->ledger[0][1]==1000 && meds.inventory->bag[1].id==0x1E && meds.inventory->bag[1].count==1);
    for(unsigned i=0;i<8;++i) tick(&ms,&meds,money,&medicine_stats,HMBTN_PAD_CROSS);
    CHECK(money->gold==200 && money->ledger[0][1]==1000 && meds.inventory->bag[1].count==1 && !ms.medicine_available);
    CHECK(advance(&ms,&meds,money,&medicine_stats,HMBTN_SHOP_SELECT));tick(&ms,&meds,money,&medicine_stats,HMBTN_PAD_CIRCLE);
    CHECK(advance(&ms,&meds,money,&medicine_stats,HMBTN_SHOP_OFF));
    CHECK(hmbtn_feed_shop_open(&ms,&v,&player,&clock,136,0));
    CHECK(advance(&ms,&meds,money,&medicine_stats,HMBTN_SHOP_SELECT) && !ms.medicine_available);
    tick(&ms,&meds,money,&medicine_stats,HMBTN_PAD_UP);CHECK(!ms.selection); /* Same ownership in both shops. */
    meds.inventory->bag[1]=(HmbtnStoredTool){0};meds.inventory->equipped=(HmbtnStoredTool){0};money->gold=2000;
    tick(&ms,&meds,money,&medicine_stats,0);tick(&ms,&meds,money,&medicine_stats,HMBTN_PAD_DOWN);CHECK(ms.selection==3);
    tick(&ms,&meds,money,&medicine_stats,HMBTN_PAD_TRIANGLE);CHECK(advance(&ms,&meds,money,&medicine_stats,HMBTN_SHOP_RESULT) && ms.request==0x64);
    CHECK(advance(&ms,&meds,money,&medicine_stats,HMBTN_SHOP_SELECT));tick(&ms,&meds,money,&medicine_stats,HMBTN_PAD_CROSS);
    CHECK(advance(&ms,&meds,money,&medicine_stats,HMBTN_SHOP_RESULT) && ms.request==0x84 && money->gold==1000
        && money->ledger[0][1]==2000 && meds.inventory->equipped.id==0x1E && meds.inventory->equipped.count==1);
    puts("Feed shops: hours/presence, feed, chicken sales, scrolling, refusals, cancellations and atomic capped ledger commits passed.");return 0;
}

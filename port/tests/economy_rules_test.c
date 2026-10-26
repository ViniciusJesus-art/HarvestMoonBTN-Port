#include "economy.h"
#include "farm_shipping.h"
#include "feed_shop.h"
#include "economy_checks.h"
#include <limits.h>
#include <stdio.h>

#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Economy rules line %d: %s\n",__LINE__,#x);return 1; } } while(0)
int main(void)
{
    /* Reviewed vectors from estate.c/estate_gold_change (d3c0f38):
     * return 0 normal, 1 upper clamp (including exact limit), 2 insufficient.
     * Ledger records full earnings, but only actual expenses for reset=true. */
    const struct {
        unsigned gold;int64_t amount;bool reset;
        HmbtnGoldResult result;unsigned after,income,expense;
    } vectors[]={
        {500,0,false,HMBTN_GOLD_OK,500,0,0},
        {500,300,false,HMBTN_GOLD_OK,800,300,0},
        {500,-500,false,HMBTN_GOLD_OK,0,0,500},
        {500,-501,false,HMBTN_GOLD_INSUFFICIENT,500,0,0},
        {500,-501,true,HMBTN_GOLD_INSUFFICIENT,0,0,500},
        {0,-1,true,HMBTN_GOLD_INSUFFICIENT,0,0,0},
        {9999990,9,false,HMBTN_GOLD_LIMIT,9999999,9,0},
        {9999990,300,false,HMBTN_GOLD_LIMIT,9999999,300,0},
        {9999999,1,false,HMBTN_GOLD_LIMIT,9999999,1,0},
        {9999999,-1,false,HMBTN_GOLD_OK,9999998,0,1},
    };
    for(unsigned i=0;i<sizeof(vectors)/sizeof(*vectors);++i) {
        HmbtnEconomy economy={.gold=vectors[i].gold};
        CHECK(hmbtn_economy_gold_change(&economy,vectors[i].amount,vectors[i].reset)==vectors[i].result);
        CHECK(economy.gold==vectors[i].after && economy.ledger[0][0]==vectors[i].income
            && economy.ledger[0][1]==vectors[i].expense);
    }
    HmbtnEconomy economy={.gold=500};
    CHECK(hmbtn_economy_gold_change(&economy,INT64_MIN,false)==HMBTN_GOLD_INSUFFICIENT
        && economy.gold==500 && !economy.ledger[0][1]);
    CHECK(hmbtn_economy_gold_change(&economy,INT64_MIN,true)==HMBTN_GOLD_INSUFFICIENT
        && !economy.gold && economy.ledger[0][1]==500);
    CHECK(hmbtn_economy_gold_change(&economy,INT64_MAX,false)==HMBTN_GOLD_LIMIT
        && economy.gold==9999999 && economy.ledger[0][0]==UINT32_MAX);
    CHECK(hmbtn_economy_gold_change(&economy,1,false)==HMBTN_GOLD_LIMIT && !economy.ledger[0][0]);
    CHECK(hmbtn_economy_gold_change(NULL,1,false)==HMBTN_GOLD_INVALID);
    economy=(HmbtnEconomy){.gold=9999999,.gold_limit=100};
    const HmbtnEconomy invalid=economy;
    CHECK(hmbtn_economy_gold_change(&economy,1,false)==HMBTN_GOLD_INVALID && economy_equal(&economy,&invalid));

    /* A native limit above the base-game cap reaches real shipping and shop
     * consumers, not a separate test-only wallet. UI references stay live. */
    HmbtnGameState game;hmbtn_game_state_init(&game);
    game.economy.gold=9999990;game.economy.gold_limit=12000000;
    HmbtnHouseInteraction house;hmbtn_house_interaction_init(&house,&game);
    HmbtnFarmMenu menu;hmbtn_farm_menu_init(&menu,&game.economy);
    CHECK(hmbtn_farm_shipping_add(&game.economy,0x41));
    const HmbtnFarmClock clock={0,0,2,2,17,58,0};
    hmbtn_farm_shipping_clock(&game.economy,&clock,HMBTN_CLOCK_MINUTE);
    CHECK(menu.economy->gold==10000050 && menu.economy->ledger[0][0]==60);
    hmbtn_farm_shipping_clock(&game.economy,&clock,HMBTN_CLOCK_MINUTE);
    CHECK(game.economy.gold==10000050 && game.economy.ledger[0][0]==60);
    HmbtnFeedShop shop={.merchant=15,.quantity=3,.item=0x68,.phase=HMBTN_SHOP_COUNT};
    shop.message.words[0]=0xffff;shop.message.count=1;shop.message.phase=HMBTN_MESSAGE_WAIT_END;
    hmbtn_feed_shop_tick(&shop,&house,&game.economy,&menu.stats,HMBTN_PAD_CROSS,0);
    CHECK(menu.economy->gold==9999990 && menu.economy->ledger[0][1]==60 && game.inventory.supplies[0]==3);
    for(unsigned i=0;i<10;++i) hmbtn_feed_shop_tick(&shop,&house,&game.economy,&menu.stats,HMBTN_PAD_CROSS,0);
    CHECK(game.economy.gold==9999990 && game.economy.ledger[0][1]==60 && game.inventory.supplies[0]==3);
    puts("Original gold vectors, defined extremes, native limit and shared shipping/shop transactions passed.");
    return 0;
}

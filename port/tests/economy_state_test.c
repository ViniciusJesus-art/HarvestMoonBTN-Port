#include "feed_shop.h"
#include "farm_shipping.h"
#include "economy_checks.h"
#include "inventory_checks.h"
#include <stdio.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Economy state line %d: %s\n",__LINE__,#x);return 1; } } while(0)

static void quantity_panel(HmbtnFeedShop *shop,unsigned merchant,unsigned quantity)
{
    *shop=(HmbtnFeedShop){0};shop->merchant=merchant;shop->quantity=quantity;
    shop->item=merchant==15 ? 0x68 : 0x69;shop->phase=HMBTN_SHOP_COUNT;
    shop->message.words[0]=0xffff;shop->message.count=1;
    shop->message.phase=HMBTN_MESSAGE_WAIT_END;
}
int main(void)
{
    HmbtnGameState game,other;hmbtn_game_state_init(&game);hmbtn_game_state_init(&other);
    HmbtnEconomy *economy=hmbtn_game_economy(&game);
    const HmbtnEconomy defaults={.gold=500};
    CHECK(economy==&game.economy && hmbtn_game_economy_const(&game)==economy);
    CHECK(!hmbtn_game_economy(NULL) && !hmbtn_game_economy_const(NULL));
    CHECK(economy_equal(economy,&defaults) && economy_equal(&other.economy,&defaults));
    HmbtnFarmMenu first,second;
    hmbtn_farm_menu_init(&first,economy);hmbtn_farm_menu_init(&second,economy);
    CHECK(first.economy==second.economy && first.economy==economy);
    HmbtnHouseInteraction house;hmbtn_house_interaction_init(&house,&game);
    HmbtnFeedShop shop;quantity_panel(&shop,15,3);
    hmbtn_feed_shop_tick(&shop,&house,economy,&first.stats,HMBTN_PAD_CROSS,0);
    CHECK(economy->gold==440 && economy->ledger[0][1]==60 && game.inventory.supplies[0]==3);
    CHECK(second.economy->gold==440 && second.economy->ledger[0][1]==60);
    for(unsigned i=0;i<8;++i) hmbtn_feed_shop_tick(&shop,&house,economy,&first.stats,HMBTN_PAD_CROSS,0);
    CHECK(economy->gold==440 && economy->ledger[0][1]==60);
    quantity_panel(&shop,18,2);
    hmbtn_feed_shop_tick(&shop,&house,economy,&second.stats,HMBTN_PAD_CROSS,0);
    CHECK(first.economy->gold==420 && first.economy->ledger[0][1]==80 && game.inventory.supplies[2]==2);
    const HmbtnEconomy purchased=*economy;const HmbtnInventory stores=game.inventory;
    quantity_panel(&shop,15,3);
    hmbtn_feed_shop_tick(&shop,&house,economy,&first.stats,HMBTN_PAD_CIRCLE,0);
    for(unsigned i=0;i<8;++i) hmbtn_feed_shop_tick(&shop,&house,economy,&first.stats,0,0);
    CHECK(economy_equal(economy,&purchased) && inventory_equal(&game.inventory,&stores));

    /* The same wallet receives deferred collection even with a menu open. */
    CHECK(hmbtn_farm_menu_open(&first,HMBTN_PAD_START));
    CHECK(hmbtn_farm_shipping_add(economy,0x41));
    CHECK(first.economy->shipping.pending_value==60 && second.economy->shipments[0]==1);
    HmbtnFarmClock clock={0,0,2,2,17,58,0};
    hmbtn_farm_shipping_clock(economy,&clock,HMBTN_CLOCK_MINUTE);
    CHECK(first.economy->gold==480 && second.economy->ledger[0][0]==60);
    CHECK(economy->shipping.settled && !economy->shipping.pending_value);
    CHECK(hmbtn_farm_shipping_add(economy,0x41));
    const HmbtnEconomy after_cutoff=*economy;
    /* Recreating either view must not clear settlement or the post-cutoff bin. */
    hmbtn_farm_menu_init(&first,economy);hmbtn_farm_menu_init(&second,economy);
    hmbtn_farm_shipping_clock(economy,&clock,HMBTN_CLOCK_MINUTE);
    CHECK(economy_equal(economy,&after_cutoff));
    clock.day=3;clock.hour=6;clock.minute=0;
    hmbtn_farm_shipping_clock(economy,&clock,HMBTN_CLOCK_DAY|HMBTN_CLOCK_MINUTE);
    CHECK(!economy->shipping.settled && economy->shipping.pending_value==60);
    clock.hour=17;clock.minute=58;hmbtn_farm_shipping_clock(economy,&clock,HMBTN_CLOCK_MINUTE);
    CHECK(first.economy->gold==540 && first.economy->ledger[0][0]==120 && first.economy->ledger[0][1]==80);
    CHECK(economy->shipments[0]==2 && inventory_equal(&game.inventory,&stores));
    CHECK(economy_equal(&other.economy,&defaults));
    hmbtn_farm_menu_init(&first,hmbtn_game_economy_const(&other));
    CHECK(first.economy!=second.economy && first.economy->gold==500 && second.economy->gold==540);
    puts("Authoritative wallet, accounts, shared menus/shops, deferred collection and independent games passed.");
    return 0;
}

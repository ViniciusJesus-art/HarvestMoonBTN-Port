#include "save_fixture.h"
#include "farm_shipping.h"
#include "inventory.h"
#include <stdio.h>
#include <stdlib.h>
static void buy(HmbtnSession *s,unsigned seed,unsigned quantity)
{
    HmbtnFeedShop *shop=&s->feed_shop;
    shop->seeds=true;shop->merchant=1;shop->clock=&s->game.clock;
    shop->item=seed;shop->quantity=quantity;shop->selection=4;
    shop->phase=HMBTN_SHOP_COUNT;shop->closing=false;shop->request=shop->loaded=0x4d;
    shop->message=(HmbtnMessage){.phase=HMBTN_MESSAGE_WAIT_END};
    hmbtn_feed_shop_tick(shop,&s->house,&s->game.economy,&s->menu.stats,HMBTN_PAD_CROSS,0);
}
int main(void)
{
    const unsigned products[3][4]={{0x41,0x42,0x43,0x45},{0x46,0x47,0x48,0x4a},{0x4b,0x4c,0x4d,0x4f}};
    const unsigned seeds[]={0x25,0x2a,0x2f},prices[]={150,500,200},descriptions[]={0xc6,0xca,0xce};
    HmbtnSession *s=save_fixture(0);char error[512]={0};
    bool flags[HMBTN_VILLAGE_FLAGS];memcpy(flags,s->village.flags,sizeof(flags));
    for(unsigned season=0;season<3;++season) {
        memset(s->game.economy.shipments,0,sizeof(s->game.economy.shipments));
        s->game.clock.season=season;s->field.season=season;s->daily_date=hmbtn_session_date(&s->game.clock);
        HmbtnFeedShop *shop=&s->feed_shop;shop->seeds=true;shop->merchant=1;shop->clock=&s->game.clock;
        for(unsigned i=0;i<4;++i) s->game.economy.shipments[products[season][i]-0x41]=101;
        for(unsigned missing=0;missing<4;++missing) {
            unsigned *count=&s->game.economy.shipments[products[season][missing]-0x41];*count=100;
            assert(!hmbtn_farm_shipping_seed_unlocked(&s->game.economy,season));
            assert(hmbtn_seed_shop_count(shop)==4 && !hmbtn_seed_shop_choice(shop,4));
            /* The normal shipping path supplies the last unit, not another flag setter. */
            assert(hmbtn_farm_shipping_add(&s->game.economy,products[season][missing]));
            assert(*count==101 && hmbtn_farm_shipping_seed_unlocked(&s->game.economy,season));
        }
        assert(hmbtn_seed_shop_count(shop)==5);
        const HmbtnSeedShopEntry *entry=hmbtn_seed_shop_choice(shop,4);
        assert(entry && entry->item==seeds[season] && entry->description==descriptions[season]);
        assert(entry->name[entry->name_words-1]==0xffff && !hmbtn_seed_shop_choice(shop,5));
        for(unsigned other=0;other<3;++other) if(other!=season)
            assert(!hmbtn_farm_shipping_seed_unlocked(&s->game.economy,other));
        assert(!memcmp(flags,s->village.flags,sizeof(flags))); /* No unrelated flag mutations. */
        /* The counters already persist; restored menus borrow the restored economy. */
        shop->phase=HMBTN_SHOP_OFF;shop->message=(HmbtnMessage){0};
        uint8_t *bytes=NULL;size_t size=0;assert(hmbtn_save_encode(s,&bytes,&size,error,sizeof(error)));
        HmbtnSession *loaded=hmbtn_save_decode(bytes,size,error,sizeof(error));free(bytes);assert(loaded);
        assert(loaded->feed_shop.economy==&loaded->game.economy && loaded->feed_shop.economy!=shop->economy);
        loaded->feed_shop.seeds=true;loaded->feed_shop.clock=&loaded->game.clock;
        assert(hmbtn_seed_shop_count(&loaded->feed_shop)==5);
        assert(hmbtn_seed_shop_choice(&loaded->feed_shop,4)->item==seeds[season]);hmbtn_session_destroy(loaded);
        s->game.inventory=(HmbtnInventory){0};s->game.economy.gold=0;
        buy(s,seeds[season],2);assert(shop->next_request==0x4f && !s->game.inventory.equipped.id);
        s->game.economy.gold=5000;buy(s,seeds[season],2);
        assert(shop->next_request==0xbb && s->game.inventory.equipped.id==seeds[season]
            && s->game.inventory.equipped.count==2 && s->game.economy.gold==5000-prices[season]*2);
        for(unsigned i=0;i<hmbtn_inventory_capacity(&s->game.inventory);++i) s->game.inventory.bag[i]=(HmbtnStoredTool){6,0,1};
        for(unsigned i=0;i<64;++i) s->game.inventory.toolbox[i]=(HmbtnStoredTool){6,0,1};
        s->game.inventory.equipped=(HmbtnStoredTool){(uint16_t)seeds[season],0,99};
        const unsigned gold=s->game.economy.gold;buy(s,seeds[season],3);
        assert(shop->next_request==0xc1 && s->game.economy.gold==gold);
        s->game.inventory.toolbox[63]=(HmbtnStoredTool){0};buy(s,seeds[season],3);
        assert(shop->next_request==0x207 && shop->next_bank==3 && s->game.inventory.toolbox[63].count==3
            && s->game.economy.gold==gold-prices[season]*3);
        shop->phase=HMBTN_SHOP_OFF;shop->closing=false;shop->message=(HmbtnMessage){0};
    }
    assert(!hmbtn_farm_shipping_seed_unlocked(NULL,0) && !hmbtn_farm_shipping_seed_unlocked(&s->game.economy,3));
    hmbtn_session_destroy(s);puts("Seed unlocks: 100/101 per product, isolated groups, conditional stock, shared payment/storage and save loans passed.");return 0;
}

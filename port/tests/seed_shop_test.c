/* Resource-free tests of the adapted original shop flow. Text delivery is a
 * synthetic completed resource; real BIN text/UI is covered by crop_playthrough. */
#include "session.h"
#include "inventory.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Seed shop line %d: %s\n",__LINE__,#x);hmbtn_session_destroy(s);return 1; } } while(0)
static void tick(HmbtnSession *s,uint32_t pressed,uint32_t held)
{
    hmbtn_feed_shop_tick(&s->feed_shop,&s->house,&s->game.economy,&s->menu.stats,pressed,held);
}
static void delivered(HmbtnSession *s)
{
    s->feed_shop.loaded=s->feed_shop.request;
    s->feed_shop.message=(HmbtnMessage){.phase=HMBTN_MESSAGE_WAIT_END,.count=1,.words={0xFFFF}};
}
static void queued(HmbtnSession *s)
{
    for(unsigned i=0;i<12;++i) tick(s,0,0);
}
static void finish(HmbtnSession *s)
{
    delivered(s);tick(s,HMBTN_PAD_CROSS,0);queued(s);
}
static void select_row(HmbtnSession *s,unsigned row)
{
    HmbtnFeedShop *shop=&s->feed_shop;
    *shop=(HmbtnFeedShop){.seeds=true,.phase=HMBTN_SHOP_SELECT,.request=0x46,.bank=2,
        .selection=row,.item=hmbtn_seed_shop_entry(s->game.clock.season,row)->item,.quantity=1,.clock=&s->game.clock};
    delivered(s);
}
static void count_row(HmbtnSession *s,unsigned row)
{
    select_row(s,row);tick(s,HMBTN_PAD_CROSS,0);queued(s);finish(s);delivered(s);
}
int main(void)
{
    HmbtnSession *s=hmbtn_session_create(NULL,0);CHECK(s);
    HmbtnFeedShop *shop=&s->feed_shop;HmbtnInventory *inv=&s->game.inventory;
    HmbtnEconomy *economy=&s->game.economy;
    const unsigned ids[]={54,34,35,36},descriptions[]={0xCF,0xC3,0xC4,0xC5};
    CHECK(hmbtn_seed_shop_entries(0)==4 && !hmbtn_seed_shop_entry(0,4));
    for(unsigned i=0;i<4;++i) {
        const HmbtnSeedShopEntry *entry=hmbtn_seed_shop_entry(0,i);
        CHECK(entry && entry->item==ids[i] && entry->description==descriptions[i]
            && entry->name_words>1 && entry->name[entry->name_words-1]==0xFFFF);
        select_row(s,i);tick(s,HMBTN_PAD_TRIANGLE,0);queued(s);
        CHECK(shop->phase==HMBTN_SHOP_INFO && shop->bank==2 && shop->request==descriptions[i]);
        finish(s);CHECK(shop->phase==HMBTN_SHOP_SELECT && shop->request==0x46 && shop->selection==i);
    }
    select_row(s,0);tick(s,HMBTN_PAD_UP,0);CHECK(shop->selection==3 && shop->item==36);
    tick(s,HMBTN_PAD_DOWN,0);CHECK(shop->selection==0 && shop->item==54);
    tick(s,HMBTN_PAD_DOWN,0);CHECK(shop->selection==1 && shop->item==34);
    /* No selection/payment before a text resource is delivered. */
    shop->loaded=0;tick(s,HMBTN_PAD_CROSS,0);CHECK(!shop->closing);
    delivered(s);tick(s,HMBTN_PAD_CROSS,0);CHECK(shop->closing && shop->quantity==1);
    queued(s);CHECK(shop->phase==HMBTN_SHOP_OFFER && shop->request==0xC0);
    finish(s);CHECK(shop->phase==HMBTN_SHOP_COUNT && shop->request==0x4D);
    delivered(s);tick(s,HMBTN_PAD_LEFT,0);CHECK(shop->quantity==99);
    tick(s,HMBTN_PAD_RIGHT,0);CHECK(shop->quantity==1);
    tick(s,HMBTN_PAD_UP,0);CHECK(shop->quantity==11);
    tick(s,HMBTN_PAD_DOWN,0);CHECK(shop->quantity==1);
    tick(s,HMBTN_PAD_DOWN,0);CHECK(shop->quantity==99);
    tick(s,HMBTN_PAD_RIGHT,0);CHECK(shop->quantity==1);
    tick(s,0,HMBTN_PAD_RIGHT);CHECK(shop->quantity==2);
    for(unsigned i=0;i<12;++i) tick(s,0,HMBTN_PAD_RIGHT);
    CHECK(shop->quantity==3);tick(s,0,0);
    tick(s,HMBTN_PAD_TRIANGLE,0);queued(s);
    CHECK(shop->phase==HMBTN_SHOP_INFO && shop->request==0xC3);
    finish(s);CHECK(shop->phase==HMBTN_SHOP_COUNT && shop->quantity==3);
    /* Quantity cancel has both original responses; selection is retained. */
    delivered(s);tick(s,HMBTN_PAD_CIRCLE,0);queued(s);
    CHECK(shop->phase==HMBTN_SHOP_CANCEL_COUNT && shop->request==0xBA);
    finish(s);CHECK(shop->phase==HMBTN_SHOP_RESULT && shop->request==0xC2);
    finish(s);CHECK(shop->phase==HMBTN_SHOP_SELECT && shop->selection==1);
    delivered(s);tick(s,HMBTN_PAD_CIRCLE,0);queued(s);
    CHECK(shop->phase==HMBTN_SHOP_LEAVE && shop->request==0xBF);
    finish(s);CHECK(shop->phase==HMBTN_SHOP_OFF);
    /* Every name selects its actual stock/price, never a hardcoded turnip. */
    for(unsigned i=0;i<4;++i) {
        *inv=(HmbtnInventory){0};*economy=(HmbtnEconomy){.gold=10000};
        count_row(s,i);CHECK(shop->phase==HMBTN_SHOP_COUNT && shop->item==ids[i]);
        shop->quantity=3;const unsigned cost=hmbtn_feed_shop_cost(shop);
        CHECK(cost==3u*(unsigned)hmbtn_farm_item(ids[i])->buy_price);
        tick(s,HMBTN_PAD_CROSS,0);
        CHECK(inv->equipped.id==ids[i] && inv->equipped.count==3 && economy->gold==10000-cost
            && economy->ledger[0][1]==cost && s->house.inventory==inv && shop->closing);
        /* Waiting/repeated confirm never buys or debits again. */
        for(unsigned j=0;j<12;++j) tick(s,HMBTN_PAD_CROSS,0);
        CHECK(shop->request==0xBB && shop->bank==2 && economy->ledger[0][1]==cost && inv->equipped.count==3);
        finish(s);CHECK(shop->phase==HMBTN_SHOP_SELECT && shop->selection==i);
    }
    /* shop_seed_menu_setup_b: original base summer stock and descriptions.
     * The modal menu and presentation consult the same authoritative clock. */
    const unsigned summer_ids[]={54,39,40,41},summer_info[]={0xCF,0xC7,0xC8,0xC9};
    s->game.clock.season=1;
    CHECK(hmbtn_seed_shop_entries(1)==4 && !hmbtn_seed_shop_entry(1,4));
    for(unsigned row=0;row<4;++row) {
        const HmbtnSeedShopEntry *entry=hmbtn_seed_shop_entry(1,row);
        CHECK(entry && entry->item==summer_ids[row] && entry->description==summer_info[row]
            && entry->name_words>1 && entry->name[entry->name_words-1]==0xFFFF);
        select_row(s,row);tick(s,HMBTN_PAD_TRIANGLE,0);queued(s);
        CHECK(shop->request==summer_info[row] && shop->bank==2);
        finish(s);CHECK(shop->phase==HMBTN_SHOP_SELECT && shop->selection==row);
        *inv=(HmbtnInventory){0};*economy=(HmbtnEconomy){.gold=10000};
        count_row(s,row);CHECK(shop->phase==HMBTN_SHOP_COUNT && shop->item==summer_ids[row]);
        shop->quantity=3;const unsigned cost=hmbtn_feed_shop_cost(shop);
        CHECK(cost==3u*(unsigned)hmbtn_farm_item(summer_ids[row])->buy_price);
        tick(s,HMBTN_PAD_CROSS,0);queued(s);
        CHECK(inv->equipped.id==summer_ids[row] && inv->equipped.count==3 && economy->gold==10000-cost
            && economy->ledger[0][1]==cost && shop->request==0xBB);
        finish(s);CHECK(shop->phase==HMBTN_SHOP_SELECT && shop->selection==row);
    }
    select_row(s,0);tick(s,HMBTN_PAD_UP,0);CHECK(shop->selection==3 && shop->item==41);
    tick(s,HMBTN_PAD_DOWN,0);CHECK(shop->selection==0 && shop->item==54);
    CHECK(hmbtn_seed_shop_entries(2)==4 && !hmbtn_seed_shop_entries(3) && !hmbtn_seed_shop_entry(99,0));
    const unsigned fall_ids[]={54,44,45,46},fall_info[]={0xCF,0xCB,0xCC,0xCD};
    s->game.clock.season=2;
    CHECK(hmbtn_seed_shop_entries(2)==4 && !hmbtn_seed_shop_entry(2,4));
    for(unsigned row=0;row<4;++row) {
        const HmbtnSeedShopEntry *entry=hmbtn_seed_shop_entry(2,row);
        CHECK(entry && entry->item==fall_ids[row] && entry->description==fall_info[row]
            && entry->name_words>1 && entry->name[entry->name_words-1]==0xFFFF);
        select_row(s,row);tick(s,HMBTN_PAD_TRIANGLE,0);queued(s);
        CHECK(shop->request==fall_info[row] && shop->bank==2);
        finish(s);CHECK(shop->phase==HMBTN_SHOP_SELECT && shop->selection==row);
        *inv=(HmbtnInventory){0};*economy=(HmbtnEconomy){.gold=10000};
        count_row(s,row);CHECK(shop->phase==HMBTN_SHOP_COUNT && shop->item==fall_ids[row]);
        shop->quantity=3;const unsigned cost=hmbtn_feed_shop_cost(shop);
        CHECK(cost==3u*(unsigned)hmbtn_farm_item(fall_ids[row])->buy_price);
        tick(s,HMBTN_PAD_CROSS,0);queued(s);
        CHECK(inv->equipped.id==fall_ids[row] && inv->equipped.count==3 && economy->gold==10000-cost
            && economy->ledger[0][1]==cost && shop->request==0xBB);
        finish(s);CHECK(shop->phase==HMBTN_SHOP_SELECT && shop->selection==row);
    }
    select_row(s,0);tick(s,HMBTN_PAD_UP,0);CHECK(shop->selection==3 && shop->item==46);
    tick(s,HMBTN_PAD_DOWN,0);CHECK(shop->selection==0 && shop->item==54);
    s->game.clock.season=0;
    *inv=(HmbtnInventory){0};*economy=(HmbtnEconomy){.gold=359};
    count_row(s,1);shop->quantity=3;tick(s,HMBTN_PAD_CROSS,0);queued(s);
    CHECK(shop->request==0x4F && !inv->equipped.id && economy->gold==359 && !economy->ledger[0][1]);
    finish(s);CHECK(shop->phase==HMBTN_SHOP_COUNT && shop->quantity==3);
    /* Full box, two accessible spaces in an existing stack: one may be bought,
     * three must be rejected atomically and return to quantity. */
    inv->equipped=(HmbtnStoredTool){34,0,97};
    for(unsigned i=0;i<2;++i) inv->bag[i]=(HmbtnStoredTool){6,0,1};
    for(unsigned i=0;i<64;++i) inv->toolbox[i]=(HmbtnStoredTool){6,0,1};
    economy->gold=1000;count_row(s,1);shop->quantity=3;
    const HmbtnInventory full=*inv;
    tick(s,HMBTN_PAD_CROSS,0);queued(s);
    CHECK(shop->request==0xC1 && !memcmp(inv,&full,sizeof(full)) && economy->gold==1000 && !economy->ledger[0][1]);
    finish(s);CHECK(shop->phase==HMBTN_SHOP_COUNT);
    /* All-or-spill: preserve the partial accessible stack, send whole lot to
     * first free toolbox cell; use bank THREE's original storage notice. */
    inv->toolbox[17]=(HmbtnStoredTool){0};delivered(s);tick(s,HMBTN_PAD_CROSS,0);queued(s);
    CHECK(shop->request==0x207 && shop->bank==3 && inv->equipped.count==97
        && inv->toolbox[17].id==34 && inv->toolbox[17].count==3
        && economy->gold==640 && economy->ledger[0][1]==360);
    finish(s);CHECK(shop->request==0xBB && shop->bank==2);
    finish(s);CHECK(shop->phase==HMBTN_SHOP_SELECT);
    inv->equipped.count=99;inv->toolbox[17]=(HmbtnStoredTool){6,0,1};
    select_row(s,1);tick(s,HMBTN_PAD_CROSS,0);queued(s);
    CHECK(shop->request==0x54 && economy->gold==640);finish(s);CHECK(shop->phase==HMBTN_SHOP_OFF);
    /* Shared bulk rule fills in order, respects inactive cells and never edits
     * state/placement on failure. The one-packet API delegates to this rule. */
    *inv=(HmbtnInventory){.equipped={34,0,98},.bag={{34,0,98}}};
    HmbtnInventoryPlacement placement={HMBTN_STORE_TOOLBOX,63};
    CHECK(hmbtn_inventory_add_seeds(inv,34,5,&placement));
    CHECK(inv->equipped.count==99 && inv->bag[0].count==99 && inv->bag[1].count==3
        && placement.store==HMBTN_STORE_EQUIPPED);
    CHECK(hmbtn_inventory_add_seed_packet(inv,34,NULL) && inv->bag[1].count==4);
    const HmbtnInventory before=*inv;placement=(HmbtnInventoryPlacement){HMBTN_STORE_TOOLBOX,63};
    CHECK(!hmbtn_inventory_add_seeds(inv,34,0,&placement) && !hmbtn_inventory_add_seeds(inv,34,100,&placement)
        && !hmbtn_inventory_add_seeds(inv,6,1,&placement) && !memcmp(inv,&before,sizeof(before))
        && placement.store==HMBTN_STORE_TOOLBOX && placement.index==63);
    *inv=(HmbtnInventory){.equipped={34,0,99},.bag={{6,0,1},{6,0,1}}};
    for(unsigned i=0;i<64;++i) inv->toolbox[i]=(HmbtnStoredTool){6,0,1};
    CHECK(!hmbtn_inventory_seed_room(inv,34,false) && !hmbtn_inventory_add_seeds(inv,34,1,NULL));
    inv->rucksack_level=1;CHECK(hmbtn_inventory_seed_room(inv,34,true)==198);
    CHECK(hmbtn_inventory_add_seeds(inv,34,99,&placement) && placement.store==HMBTN_STORE_BAG && placement.index==2);
    hmbtn_session_destroy(s);
    puts("Original seed names, dialogue sequence/banks, quantity, cancellation, prices, atomic payment and stack/whole-lot storage passed.");
    return 0;
}

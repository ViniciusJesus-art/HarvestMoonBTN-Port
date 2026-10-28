#include "feed_shop.h"
#include "inventory.h"
#include "event_variables.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void input(HmbtnFeedShop *s,HmbtnHouseInteraction *h,HmbtnEconomy *e,uint32_t button)
{
    s->loaded=s->request;s->message.phase=HMBTN_MESSAGE_WAIT_END;
    hmbtn_feed_shop_tick(s,h,e,NULL,button,button);
}
static void until(HmbtnFeedShop *s,HmbtnHouseInteraction *h,HmbtnEconomy *e,HmbtnFeedShopPhase phase)
{
    for(unsigned i=0;i<12 && (s->phase!=phase || s->closing);++i) {
        s->loaded=s->request;s->message.phase=HMBTN_MESSAGE_CLOSED;
        hmbtn_feed_shop_tick(s,h,e,NULL,0,0);
    }
    assert(s->phase==phase && !s->closing);
}
int main(void)
{
    HmbtnGameState g;HmbtnHouseInteraction h;HmbtnFeedShop s={0};
    hmbtn_game_state_init(&g);hmbtn_house_interaction_init(&h,&g);
    const unsigned seeds[8]={0x26,0x2b,0x30,0x31,0x32,0x33,0x34,0x35};
    for(unsigned i=0;i<8;++i) {
        const HmbtnSeedShopEntry *entry=hmbtn_won_seed_entry(i);
        assert(entry && entry->item==seeds[i] && entry->description==0x165+i
            && entry->name_words>1 && entry->name[entry->name_words-1]==0xffff);
    }
    assert(!hmbtn_won_seed_entry(8));
    assert(hmbtn_won_seed_shop_open(&s,&g.clock) && s.request==0x164 && s.merchant==35);
    assert(!hmbtn_won_seed_shop_open(&s,&g.clock) && hmbtn_seed_shop_count(&s)==7);
    const unsigned source_rows[]={0,1,2,4,5,6,7};
    for(unsigned i=0;i<7;++i) assert(hmbtn_seed_shop_choice(&s,i)==hmbtn_won_seed_entry(source_rows[i]));
    assert(!hmbtn_seed_shop_choice(&s,7));
    until(&s,&h,&g.economy,HMBTN_SHOP_SELECT);
    input(&s,&h,&g.economy,HMBTN_PAD_UP);assert(s.item==0x35);
    input(&s,&h,&g.economy,HMBTN_PAD_DOWN);assert(s.item==0x26);
    /* Description / back / cancel leave share the real stock/profile. */
    input(&s,&h,&g.economy,HMBTN_PAD_TRIANGLE);assert(s.next_request==0x165);
    until(&s,&h,&g.economy,HMBTN_SHOP_SELECT);
    input(&s,&h,&g.economy,HMBTN_PAD_CROSS);assert(s.next_request==0x16e);
    until(&s,&h,&g.economy,HMBTN_SHOP_COUNT);
    input(&s,&h,&g.economy,HMBTN_PAD_LEFT);assert(s.quantity==99);
    input(&s,&h,&g.economy,HMBTN_PAD_RIGHT);assert(s.quantity==1);
    input(&s,&h,&g.economy,HMBTN_PAD_UP);assert(s.quantity==11);
    input(&s,&h,&g.economy,HMBTN_PAD_DOWN);assert(s.quantity==1);
    input(&s,&h,&g.economy,HMBTN_PAD_RIGHT);assert(s.quantity==2);
    HmbtnInventory inv=g.inventory;const unsigned gold=g.economy.gold;
    input(&s,&h,&g.economy,HMBTN_PAD_CROSS);
    assert(s.next_request==0x4f && g.economy.gold==gold && !memcmp(&inv,&g.inventory,sizeof(inv)));
    until(&s,&h,&g.economy,HMBTN_SHOP_COUNT);
    input(&s,&h,&g.economy,HMBTN_PAD_CIRCLE);assert(s.next_request==0x16f);
    until(&s,&h,&g.economy,HMBTN_SHOP_RESULT);assert(s.request==0x171);
    until(&s,&h,&g.economy,HMBTN_SHOP_SELECT);
    g.economy.gold=5000;
    input(&s,&h,&g.economy,HMBTN_PAD_CROSS);until(&s,&h,&g.economy,HMBTN_SHOP_COUNT);
    input(&s,&h,&g.economy,HMBTN_PAD_CROSS);assert(g.economy.gold==4500 && s.next_request==0x170);
    assert(hmbtn_inventory_count(&g.inventory,0x26,0,2)==1);
    until(&s,&h,&g.economy,HMBTN_SHOP_SELECT);
    /* Overflow goes to the same box and uses Won's own stored-item messages. */
    g.inventory.equipped=(HmbtnStoredTool){6,0,1};
    for(unsigned i=0;i<2;++i) g.inventory.bag[i]=(HmbtnStoredTool){6,0,1};
    input(&s,&h,&g.economy,HMBTN_PAD_CROSS);until(&s,&h,&g.economy,HMBTN_SHOP_COUNT);
    input(&s,&h,&g.economy,HMBTN_PAD_CROSS);assert(s.next_bank==3 && s.next_request==0x261 && g.economy.gold==4000);
    until(&s,&h,&g.economy,HMBTN_SHOP_RESULT);assert(s.bank==3 && s.request==0x261);
    s.message.phase=HMBTN_MESSAGE_CLOSED;s.loaded=s.request;hmbtn_feed_shop_tick(&s,&h,&g.economy,NULL,0,0);
    assert(s.bank==2 && s.request==0x170);
    until(&s,&h,&g.economy,HMBTN_SHOP_SELECT);
    for(unsigned i=0;i<64;++i) g.inventory.toolbox[i]=(HmbtnStoredTool){6,0,1};
    inv=g.inventory;
    input(&s,&h,&g.economy,HMBTN_PAD_CROSS);assert(s.next_request==0x54);
    until(&s,&h,&g.economy,HMBTN_SHOP_OFF);
    assert(g.economy.gold==4000 && !memcmp(&inv,&g.inventory,sizeof(inv)));
    /* Filtered flower rows preserve their own descriptions, not 0x165+row. */
    for(unsigned row=3;row<7;++row) {
        hmbtn_game_state_init(&g);s=(HmbtnFeedShop){0};
        assert(hmbtn_won_seed_shop_open(&s,&g.clock));until(&s,&h,&g.economy,HMBTN_SHOP_SELECT);
        for(unsigned i=0;i<row;++i) input(&s,&h,&g.economy,HMBTN_PAD_DOWN);
        const unsigned seed=row==3 ? 0x32 : row==4 ? 0x33 : row==5 ? 0x34 : 0x35,price=row==3 ? 200 : row==4 ? 600 : row==5 ? 500 : 1000;
        assert(s.item==seed);input(&s,&h,&g.economy,HMBTN_PAD_TRIANGLE);
        assert(s.next_request==(0x166u+row));until(&s,&h,&g.economy,HMBTN_SHOP_SELECT);
        g.economy.gold=0;input(&s,&h,&g.economy,HMBTN_PAD_CROSS);until(&s,&h,&g.economy,HMBTN_SHOP_COUNT);
        input(&s,&h,&g.economy,HMBTN_PAD_CROSS);assert(s.next_request==0x4f && !g.inventory.equipped.id);
        until(&s,&h,&g.economy,HMBTN_SHOP_COUNT);
        input(&s,&h,&g.economy,HMBTN_PAD_CIRCLE);until(&s,&h,&g.economy,HMBTN_SHOP_SELECT);
        g.economy.gold=2000;input(&s,&h,&g.economy,HMBTN_PAD_CROSS);until(&s,&h,&g.economy,HMBTN_SHOP_COUNT);
        input(&s,&h,&g.economy,HMBTN_PAD_CROSS);
        assert(s.next_request==0x170 && g.economy.gold==2000-price && g.inventory.equipped.id==seed);
        until(&s,&h,&g.economy,HMBTN_SHOP_SELECT);
        g.inventory.equipped=(HmbtnStoredTool){6,0,1};
        for(unsigned i=0;i<2;++i) g.inventory.bag[i]=(HmbtnStoredTool){6,0,1};
        input(&s,&h,&g.economy,HMBTN_PAD_CROSS);until(&s,&h,&g.economy,HMBTN_SHOP_COUNT);
        input(&s,&h,&g.economy,HMBTN_PAD_CROSS);
        assert(s.next_request==0x261 && s.next_bank==3 && hmbtn_inventory_count(&g.inventory,(int)seed,0,2)==1
            && g.economy.gold==2000-price*2);
        until(&s,&h,&g.economy,HMBTN_SHOP_RESULT);
        s.message.phase=HMBTN_MESSAGE_CLOSED;s.loaded=s.request;hmbtn_feed_shop_tick(&s,&h,&g.economy,NULL,0,0);
        until(&s,&h,&g.economy,HMBTN_SHOP_SELECT);
        for(unsigned i=0;i<64;++i) g.inventory.toolbox[i]=(HmbtnStoredTool){6,0,1};
        const unsigned before=g.economy.gold;input(&s,&h,&g.economy,HMBTN_PAD_CROSS);
        assert(s.next_request==0x54 && g.economy.gold==before);
    }
    /* Technical support alone never unlocks Moondrop. Shared story command
     * mutates the same progress loan, including through shop resets. */
    hmbtn_game_state_init(&g);s=(HmbtnFeedShop){.progress=&g.progress};
    assert(hmbtn_won_seed_shop_open(&s,&g.clock) && hmbtn_seed_shop_count(&s)==7);
    char error[128]={0};HmbtnEventCommand unlock={.type=81,.command=27};
    unlock.y=1;
    assert(hmbtn_event_progress_command(&g.progress,&unlock,error,sizeof(error))==HMBTN_EVENT_ERROR && !g.progress.moondrop_seeds);
    unlock.y=0;assert(hmbtn_event_progress_command(&g.progress,&unlock,error,sizeof(error))==HMBTN_EVENT_DONE);
    assert(hmbtn_event_progress_command(&g.progress,&unlock,error,sizeof(error))==HMBTN_EVENT_DONE && hmbtn_seed_shop_count(&s)==8);
    for(unsigned row=0;row<8;++row) assert(hmbtn_seed_shop_choice(&s,row)==hmbtn_won_seed_entry(row));
    assert(!hmbtn_seed_shop_choice(&s,8));
    until(&s,&h,&g.economy,HMBTN_SHOP_SELECT);
    for(unsigned i=0;i<3;++i) input(&s,&h,&g.economy,HMBTN_PAD_DOWN);
    assert(s.item==0x31);input(&s,&h,&g.economy,HMBTN_PAD_TRIANGLE);assert(s.next_request==0x168);
    until(&s,&h,&g.economy,HMBTN_SHOP_SELECT);
    inv=g.inventory;
    input(&s,&h,&g.economy,HMBTN_PAD_CROSS);until(&s,&h,&g.economy,HMBTN_SHOP_COUNT);
    input(&s,&h,&g.economy,HMBTN_PAD_CIRCLE);until(&s,&h,&g.economy,HMBTN_SHOP_SELECT);
    assert(!memcmp(&inv,&g.inventory,sizeof(inv)) && g.economy.gold==500);
    input(&s,&h,&g.economy,HMBTN_PAD_CROSS);until(&s,&h,&g.economy,HMBTN_SHOP_COUNT);
    input(&s,&h,&g.economy,HMBTN_PAD_CROSS);assert(g.economy.gold==200 && g.inventory.equipped.id==0x31);
    until(&s,&h,&g.economy,HMBTN_SHOP_SELECT);input(&s,&h,&g.economy,HMBTN_PAD_CIRCLE);
    until(&s,&h,&g.economy,HMBTN_SHOP_OFF);
    assert(hmbtn_won_seed_shop_open(&s,&g.clock) && s.progress==&g.progress && hmbtn_seed_shop_count(&s)==8);
    /* Orangecup is unconditional in source group D, in all four seasons;
     * only Moondrop is controlled by a story unlock. No construction loan. */
    for(unsigned season=0;season<4;++season) {
        g.clock.season=season;s=(HmbtnFeedShop){.progress=&g.progress};
        g.progress.moondrop_seeds=false;
        assert(hmbtn_won_seed_shop_open(&s,&g.clock) && hmbtn_seed_shop_count(&s)==7
            && hmbtn_seed_shop_choice(&s,6)->item==0x35);
        g.progress.moondrop_seeds=true;
        assert(hmbtn_seed_shop_count(&s)==8 && hmbtn_seed_shop_choice(&s,7)->item==0x35);
    }
    /* The seasonal merchant/catalog/profile remains independent of Won's group. */
    assert(hmbtn_seed_shop_entries(0)==4 && hmbtn_seed_shop_entry(0,0)->item==0x36);
    puts("Won: all eight source entries audited, supported stock, descriptions, quantities, funds/space/cancel, shared inventory and distinct receipts passed.");
    return 0;
}

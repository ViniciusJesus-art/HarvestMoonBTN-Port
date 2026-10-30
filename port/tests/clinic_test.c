/* Original doctor menu, diagnosis boundaries, payment timing, XL milestones and
 * medicine receipt/use share the real economy, inventory and status domains. */
#include "save_fixture.h"
#include "farm_shipping.h"
#include "inventory.h"
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
static void open_shop(HmbtnSession *s)
{
    assert(hmbtn_clinic_open(&s->feed_shop,&s->game.clock,&s->game.inventory));
    assert(s->feed_shop.clinic && s->feed_shop.merchant==8 && s->feed_shop.request==0xdd);
    closed(s,HMBTN_SHOP_SELECT);assert(s->feed_shop.request==0x46);
}
static void medicines(HmbtnSession *s)
{
    tick(s,HMBTN_PAD_DOWN,HMBTN_MESSAGE_WAIT_END);tick(s,HMBTN_PAD_CROSS,HMBTN_MESSAGE_WAIT_END);
    closed(s,HMBTN_SHOP_TOOL_LIST);assert(s->feed_shop.request==0x46);
}
int main(void)
{
    HmbtnSession *s=save_fixture(1);HmbtnInventory *h=&s->game.inventory;memset(h,0,sizeof(*h));
    s->farm_action.max_stamina=100;s->game.clock.hour=12;s->daily_date=hmbtn_session_date(&s->game.clock);
    HmbtnFeedShop *shop=&s->feed_shop;HmbtnEconomy *e=&s->game.economy;
    assert(!hmbtn_clinic_open(NULL,&s->game.clock,h));h->held_item=(HmbtnStoredTool){154,0,1};
    assert(!hmbtn_clinic_open(shop,&s->game.clock,h));h->held_item=(HmbtnStoredTool){0};
    const unsigned stamina[]={100,70,69,50,49,20,19,5,4,0},rows[]={0,0,1,1,2,2,3,3,4,4};
    const unsigned fatigue[]={0,49,50,69,70,89,90,100},columns[]={0,0,1,1,2,2,3,3};
    const unsigned messages[5][4]={{0xeb,0xec,0xed,0xee},{0xef,0xf0,0xf1,0xf2},{0xf3,0xf4,0xf5,0xf6},{0x1d9,0x1da,0x1db,0x1dc},{0xf7,0xf8,0xf9,0xfa}};
    for(unsigned i=0;i<10;++i) for(unsigned j=0;j<8;++j) {
        s->farm_action.stamina=stamina[i];s->farm_action.fatigue=fatigue[j];const HmbtnFarmAction before=s->farm_action;
        assert(hmbtn_clinic_diagnosis(&s->farm_action)==messages[rows[i]][columns[j]] && !memcmp(&before,&s->farm_action,sizeof(before)));
    }
    s->farm_action.max_stamina=1000;s->farm_action.stamina=699;assert(hmbtn_clinic_diagnosis(&s->farm_action)==0xf2);
    s->farm_action.max_stamina=0;assert(hmbtn_clinic_diagnosis(&s->farm_action)==0xfa);s->farm_action.max_stamina=100;
    const int blue=hmbtn_farm_shipping_bin(93),green=hmbtn_farm_shipping_bin(95);assert(blue>=0 && green>=0 && blue!=green);
    for(unsigned bits=0;bits<4;++bits) {
        e->shipments[blue]=50+(bits&1);e->shipments[green]=50+((bits>>1)&1);open_shop(s);medicines(s);
        assert(hmbtn_clinic_rows(shop)==2u+(bits&1)+((bits>>1)&1));
        assert(hmbtn_clinic_entry(shop,0)->item==155 && hmbtn_clinic_entry(shop,1)->item==157);
        if(bits&1) assert(hmbtn_clinic_entry(shop,2)->item==156 && hmbtn_clinic_entry(shop,2)->description==0xe2);
        if(bits&2) assert(hmbtn_clinic_entry(shop,bits==3 ? 3 : 2)->item==158 && hmbtn_clinic_entry(shop,bits==3 ? 3 : 2)->description==0xe4);
        assert(!hmbtn_clinic_entry(shop,hmbtn_clinic_rows(shop)));
        tick(s,HMBTN_PAD_CIRCLE,HMBTN_MESSAGE_WAIT_END);closed(s,HMBTN_SHOP_SELECT);
        tick(s,HMBTN_PAD_CIRCLE,HMBTN_MESSAGE_WAIT_END);closed(s,HMBTN_SHOP_OFF);
    }
    for(unsigned attempt=0;attempt<4;++attempt) {
        e->gold=attempt==2 ? 9 : 1000;const HmbtnEconomy money=*e;const HmbtnFarmAction status=s->farm_action;
        open_shop(s);tick(s,HMBTN_PAD_TRIANGLE,HMBTN_MESSAGE_WAIT_END);closed(s,HMBTN_SHOP_INFO);assert(shop->request==0xdf);closed(s,HMBTN_SHOP_SELECT);
        tick(s,HMBTN_PAD_CROSS,HMBTN_MESSAGE_WAIT_END);closed(s,HMBTN_SHOP_MAKER_CONFIRM);assert(shop->request==0xe5);
        if(!attempt) tick(s,HMBTN_PAD_CIRCLE,HMBTN_MESSAGE_WAIT_END);
        else { if(attempt==1) tick(s,HMBTN_PAD_DOWN,HMBTN_MESSAGE_WAIT_END);tick(s,HMBTN_PAD_CROSS,HMBTN_MESSAGE_WAIT_END); }
        closed(s,HMBTN_SHOP_RESULT);assert(shop->request==(attempt<2 ? 0xe8 : attempt==2 ? 0x4f : 0xe7));
        assert(e->gold==money.gold-(attempt==3 ? 10 : 0));
        if(attempt<3) { closed(s,HMBTN_SHOP_SELECT);tick(s,HMBTN_PAD_CIRCLE,HMBTN_MESSAGE_WAIT_END);closed(s,HMBTN_SHOP_OFF);assert(!memcmp(e,&money,sizeof(money))); }
        else {
            closed(s,HMBTN_SHOP_CLINIC_FADE_OUT);unsigned shade=0;
            for(unsigned n=0;n<22;++n) { tick(s,HMBTN_PAD_CROSS,HMBTN_MESSAGE_CLOSED);assert(shop->clinic_shade>=shade);shade=shop->clinic_shade; }
            assert(shop->phase==HMBTN_SHOP_INFO && shop->request==0xe9 && shade==255);
            for(unsigned n=0;n<10;++n) tick(s,0,HMBTN_MESSAGE_WAIT_END);
            assert(shop->clinic_shade==255 && e->gold==990);
            closed(s,HMBTN_SHOP_CLINIC_FADE_IN);
            for(unsigned n=0;n<19;++n) tick(s,0,HMBTN_MESSAGE_CLOSED);
            assert(!shop->clinic_shade && shop->request==0xea);closed(s,HMBTN_SHOP_LEAVE);
            assert(shop->request==hmbtn_clinic_diagnosis(&status));closed(s,HMBTN_SHOP_OFF);
            assert(e->ledger[0][1]==money.ledger[0][1]+10);
        }
        assert(!memcmp(&status,&s->farm_action,sizeof(status)));
    }
    /* Medicines go straight to the hand: a full bag is not a purchase refusal. */
    for(unsigned i=0;i<8;++i) h->items[i]=(HmbtnStoredTool){153,0,1};
    const unsigned ids[]={155,157,156,158},prices[]={500,1000,1000,2000},gains[]={50,0,100,0},loss[]={1,20,1,50};
    for(unsigned row=0;row<4;++row) {
        e->gold=prices[row]-1;open_shop(s);medicines(s);shop->sale_cursor=row;
        tick(s,HMBTN_PAD_TRIANGLE,HMBTN_MESSAGE_WAIT_END);closed(s,HMBTN_SHOP_INFO);
        assert(shop->request==hmbtn_clinic_entry(shop,row)->description);closed(s,HMBTN_SHOP_TOOL_LIST);
        const HmbtnEconomy money=*e;tick(s,HMBTN_PAD_CROSS,HMBTN_MESSAGE_WAIT_END);closed(s,HMBTN_SHOP_RESULT);assert(shop->request==0x4f);
        closed(s,HMBTN_SHOP_INFO);assert(shop->request==0xfc);closed(s,HMBTN_SHOP_SELECT);assert(!memcmp(e,&money,sizeof(money)));
        e->gold=prices[row];tick(s,HMBTN_PAD_CROSS,HMBTN_MESSAGE_WAIT_END);closed(s,HMBTN_SHOP_TOOL_LIST);shop->sale_cursor=row;
        tick(s,HMBTN_PAD_CROSS,HMBTN_MESSAGE_WAIT_END);assert(!e->gold && e->ledger[0][1]==money.ledger[0][1]+prices[row] && !h->held_item.id);
        closed(s,HMBTN_SHOP_RESULT);assert(shop->request==0xfb);
        for(unsigned n=0;n<10;++n) tick(s,0,HMBTN_MESSAGE_WAIT_END);assert(!h->held_item.id && !e->gold);
        closed(s,HMBTN_SHOP_RUCKSACK_CUE);assert(shop->item==ids[row]);
        assert(hmbtn_event_item_show(&s->event_item,&s->intro.player,h,ids[row]));shop->cue_started=true;
        s->intro.player.animation_done=true;hmbtn_event_item_tick(&s->event_item,&s->intro.player,h,&s->farm_action);
        closed(s,HMBTN_SHOP_OFF);assert(h->held_item.id==ids[row] && h->held_item.count==1 && !h->held_item.modifier);
        s->farm_action.stamina=0;s->farm_action.fatigue=60;
        assert(hmbtn_event_item_use(&s->event_item,&s->intro.player,h,&s->game.clock));
        assert(s->event_item.phase==HMBTN_EVENT_ITEM_USE_DRINK && s->event_item.sound==75);
        s->intro.player.animation_done=true;hmbtn_event_item_tick(&s->event_item,&s->intro.player,h,&s->farm_action);
        assert(!h->held_item.id && s->farm_action.stamina==gains[row] && s->farm_action.fatigue==60-loss[row]);
    }
    memset(h->items,0,sizeof(h->items));uint8_t *data=NULL;size_t size=0;char error[256];
    assert(hmbtn_save_encode(s,&data,&size,error,sizeof(error)));HmbtnSession *loaded=hmbtn_save_decode(data,size,error,sizeof(error));
    assert(loaded && loaded->farm_action.stamina==s->farm_action.stamina && loaded->farm_action.fatigue==s->farm_action.fatigue && !memcmp(&loaded->game.economy,e,sizeof(*e)));
    hmbtn_session_destroy(loaded);free(data);hmbtn_session_destroy(s);
    puts("Clinic: original menu/XL bins, 80 diagnosis boundaries, refusal/cancel, charge timing, fades, four receipts/drinks and resume passed");return 0;
}

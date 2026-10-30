/* Original FLAG_ONLY shop contract; no graphics fixtures or parallel inventory. */
#include "save_fixture.h"
#include "inventory.h"
#include "cooking.h"
#include "psx_data.h"
#include <stdio.h>
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
    return hmbtn_grocery_shop_open(&s->feed_shop,&s->village,&s->intro.player,&s->game.clock,area,section,0x2000,&s->game.inventory);
}
int main(void)
{
    HmbtnSession *s=save_fixture(1);HmbtnInventory *h=&s->game.inventory;memset(h,0,sizeof(*h));
    s->game.clock.hour=12;s->game.clock.weekday=3;s->village.people[1].actor.visible=true;s->village.people[1].location=30;
    uint8_t section[0x2000]={0},*z=section+16;hmbtn_write_le16(section+2,30);hmbtn_write_le16(z,0x4000);hmbtn_write_le16(z+2,37);
    hmbtn_write_le16(z+14,(uint16_t)-1600);hmbtn_write_le16(z+20,(uint16_t)-1000);hmbtn_write_le16(z+16,(uint16_t)-200);
    hmbtn_write_le16(z+18,(uint16_t)-2000);hmbtn_write_le16(z+24,0);
    const unsigned ids[]={153,154,165,166,167},descriptions[]={0x1dd,0x1de,0xd9,0xda,0xdb},sprites[]={26,25,12,13,14};
    for(unsigned i=0;i<5;++i) {
        const HmbtnGroceryStock *stock=hmbtn_grocery_stock(i);assert(stock && stock->item==ids[i] && stock->description==descriptions[i] && stock->sprite==sprites[i] && stock->request==4+i);
        const HmbtnFarmItem *info=hmbtn_farm_item(stock->item);assert(info && info->buy_price==(i<2 ? 100 : 50));
        const HmbtnGatherRecord *record=hmbtn_farm_gather_record(i<2 ? 123+i : 118+i);
        memcpy(s->intro.player.position,record->position,sizeof(record->position));s->intro.player.position[0]+=400;s->intro.player.position[1]=0;s->intro.player.facing=1;
        /* Sunday/Tuesday, hours, merchant, counter kind, hands and probe edges. */
        for(unsigned day=0;day<7;++day) {
            s->game.clock.weekday=day;const bool ok=open(s,section,180);assert(ok==(day!=0 && day!=2));s->feed_shop.phase=HMBTN_SHOP_OFF;
        }
        s->game.clock.weekday=3;
        for(unsigned hour=0;hour<24;++hour) { s->game.clock.hour=hour;const bool ok=open(s,section,180);assert(ok==(hour>=9 && hour<17));s->feed_shop.phase=HMBTN_SHOP_OFF; }
        s->game.clock.hour=12;assert(!open(s,section,80));
        s->village.people[1].actor.visible=false;assert(!open(s,section,180));s->village.people[1].actor.visible=true;
        s->village.people[1].location=29;assert(!open(s,section,180));s->village.people[1].location=30;
        hmbtn_write_le16(z+2,36);assert(!open(s,section,180));hmbtn_write_le16(z+2,37);
        h->held_item=(HmbtnStoredTool){0x61,0,1};assert(!open(s,section,180));h->held_item=(HmbtnStoredTool){0};
        s->intro.player.position[2]+=150;assert(!open(s,section,180));s->intro.player.position[2]-=150;
        for(unsigned attempt=0;attempt<5;++attempt) {
            s->game.economy.gold=attempt==1 ? (unsigned)info->buy_price-1 : 1000;const unsigned gold=s->game.economy.gold,spent=s->game.economy.ledger[0][1];
            /* A full bag does not prohibit receiving the product in empty hands. */
            for(unsigned j=0;j<2;++j) h->items[j]=(HmbtnStoredTool){0x61,0,1};
            assert(open(s,section,180) && s->feed_shop.groceries && s->feed_shop.item==ids[i]);
            assert(hmbtn_event_item_show(&s->event_item,&s->intro.player,h,ids[i]));s->intro.player.animation_done=true;
            hmbtn_event_item_tick(&s->event_item,&s->intro.player,h,&s->farm_action);s->feed_shop.cue_started=true;
            closed(s,HMBTN_SHOP_OFFER);assert(s->feed_shop.request==descriptions[i] && h->held_item.id==ids[i]);
            tick(s,attempt==0 ? HMBTN_PAD_CIRCLE : HMBTN_PAD_CROSS,HMBTN_MESSAGE_WAIT_END);assert(s->feed_shop.phase==HMBTN_SHOP_MAKER_CONFIRM);
            if(attempt==0) { tick(s,HMBTN_PAD_DOWN,HMBTN_MESSAGE_WAIT_END);tick(s,HMBTN_PAD_CROSS,HMBTN_MESSAGE_WAIT_END); }
            else tick(s,HMBTN_PAD_CROSS,HMBTN_MESSAGE_WAIT_END);
            closed(s,HMBTN_SHOP_RESULT);assert(s->feed_shop.request==(attempt==0 ? 0xd8 : attempt==1 ? 0x4f : 0xbb));
            assert(s->game.economy.gold==gold && s->game.economy.ledger[0][1]==spent);
            if(attempt==4) s->game.economy.gold=0; /* Recheck funds after delayed acceptance. */
            closed(s,HMBTN_SHOP_OFF);
            const bool bought=attempt==2 || attempt==3;
            assert(h->held_item.id==(bought ? ids[i] : 0));
            assert(s->game.economy.gold==(attempt==4 ? 0 : bought ? gold-(unsigned)info->buy_price : gold));
            assert(s->game.economy.ledger[0][1]==spent+(bought ? (unsigned)info->buy_price : 0));
            for(unsigned n=0;n<5;++n) tick(s,0,HMBTN_MESSAGE_CLOSED);
            assert(s->game.economy.ledger[0][1]==spent+(bought ? (unsigned)info->buy_price : 0));
            if(bought) {
                assert(!hmbtn_inventory_cycle(h,false,false));
                assert(!hmbtn_inventory_storage_accepts(0,ids[i]) && !hmbtn_inventory_storage_accepts(1,ids[i]) && hmbtn_inventory_storage_accepts(2,ids[i]));
                assert(hmbtn_inventory_storage_swap(h,2,0,0,9+i,0));
                assert(h->icebox[i].id==ids[i] && h->icebox[i].count==attempt-1 && !h->held_item.id);
            }
        }
    }
    assert(!hmbtn_grocery_stock(5));
    /* Bought rice/curry, original pot requirement: reject unowned tools atomically. */
    memset(h->items,0,sizeof(h->items));const uint16_t ingredients[8]={167,153};HmbtnRecipeRecord result;
    HmbtnInventory before=*h;s->game.cooking.utensils=0;
    assert(!hmbtn_cooking_make(&s->game.cooking,h,4,ingredients,&result) && !memcmp(h,&before,sizeof(before)));
    s->game.cooking.utensils=4;assert(hmbtn_cooking_make(&s->game.cooking,h,4,ingredients,&result) && result.id==178);
    assert(h->held_item.id==178 && h->icebox[0].count==1 && h->icebox[4].count==1 && h->icebox[2].count==2);
    hmbtn_session_destroy(s);puts("Groceries: five original goods, guards, cue, cancel, delayed unique payment, repeat stock, storage and purchased curry passed");return 0;
}

#include "save_fixture.h"
#include "cooking.h"
#include <stdio.h>
static void confirm(HmbtnSession *s,unsigned options)
{
    s->house.mode=HMBTN_HOUSE_COOKING;s->house.closing=false;
    s->house.kitchen=(HmbtnKitchenMenu){.phase=HMBTN_KITCHEN_CONFIRM,.record=HMBTN_RECIPE_RECORDS,.options=options,.values={0x43}};
    s->house.message=(HmbtnMessage){.phase=HMBTN_MESSAGE_WAIT_END};
    hmbtn_house_interaction_tick(&s->house,HMBTN_PAD_CROSS,0);
}
int main(void)
{
    HmbtnSession *s=save_fixture(1);HmbtnInventory *inv=&s->game.inventory;
    inv->held_item=(HmbtnStoredTool){0};memset(inv->items,0,sizeof(inv->items));memset(inv->icebox,0,sizeof(inv->icebox));
    s->game.cooking.utensils=0x81;inv->items[0]=(HmbtnStoredTool){0x43,0,2};inv->icebox[0]=(HmbtnStoredTool){0x43,0,4};
    assert(hmbtn_cooking_item_stored(inv,0) && hmbtn_cooking_item_stored(inv,0x43) && !hmbtn_cooking_item_stored(inv,0x99));
    inv->items[7]=(HmbtnStoredTool){0x99,0,1};inv->rucksack_level=0;
    assert(!hmbtn_cooking_item_stored(inv,0x99));inv->rucksack_level=2;
    assert(hmbtn_cooking_item_stored(inv,0x99));
    confirm(s,0x100);HmbtnHouseInteraction *h=&s->house;HmbtnKitchenMenu *k=&h->kitchen;
    assert(k->phase==HMBTN_KITCHEN_CLOSE_CONFIRM && inv->items[0].count==2 && !inv->held_item.id);
    for(unsigned i=0;i<12;++i) hmbtn_house_interaction_tick(h,HMBTN_PAD_CROSS|HMBTN_PAD_CIRCLE,0);
    assert(k->phase==HMBTN_KITCHEN_FADE_OUT && k->fade==0);
    for(unsigned i=1;i<12;++i) {
        hmbtn_house_interaction_tick(h,HMBTN_PAD_START|HMBTN_PAD_CROSS,0);
        assert(k->phase==HMBTN_KITCHEN_FADE_OUT && inv->items[0].count==2 && !inv->held_item.id);
        assert(hmbtn_house_kitchen_shade(h)>0 && hmbtn_house_kitchen_shade(h)<255);
    }
    hmbtn_house_interaction_tick(h,0,0);
    assert(k->phase==HMBTN_KITCHEN_TRACK && hmbtn_house_kitchen_shade(h)==255);
    assert(inv->items[0].count==1 && inv->icebox[0].count==4 && inv->held_item.id==0xba && inv->held_item.modifier==20);
    assert(k->result_message==0x3c1 && !hmbtn_cooking_record_count(&s->game.cooking));
    char error[256]={0};assert(!hmbtn_save_available(s,error,sizeof(error)));
    for(unsigned i=0;i<500;++i) hmbtn_house_interaction_tick(h,HMBTN_PAD_CROSS|HMBTN_PAD_START|HMBTN_PAD_CIRCLE,0);
    assert(k->phase==HMBTN_KITCHEN_TRACK && inv->items[0].count==1 && !h->request);
    k->audio_started=k->audio_finished=true;hmbtn_house_interaction_tick(h,0,0);
    assert(k->phase==HMBTN_KITCHEN_FADE_IN && k->fade==9);
    for(unsigned i=0;i<9;++i) hmbtn_house_interaction_tick(h,HMBTN_PAD_CROSS,0);
    assert(k->phase==HMBTN_KITCHEN_RESULT && h->request==0x3c1 && !hmbtn_house_kitchen_shade(h));
    assert(!hmbtn_cooking_record_count(&s->game.cooking));
    h->message.phase=HMBTN_MESSAGE_CLOSED;hmbtn_house_interaction_tick(h,0,0);
    assert(hmbtn_cooking_record_count(&s->game.cooking)==1 && inv->items[0].count==1);
    assert(h->mode==HMBTN_HOUSE_FREE && hmbtn_save_available(s,error,sizeof(error)));

    inv->held_item=(HmbtnStoredTool){0};confirm(s,1);
    inv->items[0]=(HmbtnStoredTool){0};inv->icebox[0]=(HmbtnStoredTool){0};
    for(unsigned i=0;i<24;++i) hmbtn_house_interaction_tick(h,HMBTN_PAD_CROSS,0);
    assert(k->phase==HMBTN_KITCHEN_ITEMS && !inv->held_item.id && h->sound==2);
    h->message.phase=HMBTN_MESSAGE_WAIT_END;k->phase=HMBTN_KITCHEN_TOOLS;k->cursor=0;
    hmbtn_house_interaction_tick(h,HMBTN_PAD_LEFT,0);assert(k->cursor==3);
    hmbtn_house_interaction_tick(h,HMBTN_PAD_RIGHT,0);assert(k->cursor==0);
    hmbtn_house_interaction_tick(h,HMBTN_PAD_UP,0);assert(k->cursor==4);
    hmbtn_house_interaction_tick(h,HMBTN_PAD_LEFT,0);assert(k->cursor==7);
    hmbtn_session_destroy(s);
    puts("Cooking presentation: deferred atomic commit, audio wait, fades, no repeated consumption, records and original grid navigation passed.");return 0;
}

/* Source vectors: pregnancy=1, birth at >=21; 1/2 maternity slots, claimed
 * record independent of live count; original cow/sheep sale asymmetry. */
#include "save_fixture.h"
#include "inventory.h"
#include "psx_data.h"
#include <stdio.h>
#include <stdlib.h>
static const uint16_t name[9]={0x10,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff};
static void input(HmbtnSession *s,uint32_t pad)
{
    HmbtnFeedShop *shop=&s->feed_shop;shop->loaded=shop->request;shop->message.phase=HMBTN_MESSAGE_WAIT_END;
    hmbtn_feed_shop_tick(shop,&s->house,&s->game.economy,&s->menu.stats,pad,pad);
}
static void until(HmbtnSession *s,HmbtnFeedShopPhase phase)
{
    for(unsigned i=0;i<20 && (s->feed_shop.phase!=phase || s->feed_shop.closing);++i) {
        s->feed_shop.loaded=s->feed_shop.request;s->feed_shop.message.phase=HMBTN_MESSAGE_CLOSED;
        hmbtn_feed_shop_tick(&s->feed_shop,&s->house,&s->game.economy,&s->menu.stats,0,0);
    }
    assert(s->feed_shop.phase==phase && !s->feed_shop.closing);
}
static void choose(HmbtnSession *s,unsigned choice)
{
    for(unsigned i=0;i<hmbtn_barn_shop_rows(&s->feed_shop) && s->feed_shop.selection!=choice;++i) input(s,HMBTN_PAD_DOWN);
    assert(s->feed_shop.selection==choice);
}
static void roundtrip(HmbtnSession **s)
{
    uint8_t *data=NULL;size_t bytes=0;char error[256]={0};
    if(!hmbtn_save_encode(*s,&data,&bytes,error,sizeof(error))) { fprintf(stderr,"Breeding save: %s (day %u, pregnancy %u)\n",error,(*s)->game.clock.day,(*s)->livestock.animals[0].pregnancy);abort(); }
    HmbtnSession *loaded=hmbtn_save_decode(data,bytes,error,sizeof(error));assert(loaded);free(data);
    static const uint8_t schedule[1]={0};assert(hmbtn_session_bind_schedules(loaded,schedule,sizeof(schedule)));
    hmbtn_session_destroy(*s);*s=loaded;
}
int main(void)
{
    HmbtnSession *s=save_fixture(96);HmbtnLivestock *p=&s->livestock;
    memset(&s->game.inventory,0,sizeof(s->game.inventory));s->game.inventory.rucksack_level=2;
    assert(hmbtn_livestock_buy(p,0x161,name,&s->game.clock));
    assert(!hmbtn_livestock_breed(p,0,false));p->animals[0].growth=28;
    assert(hmbtn_livestock_buy(p,0x165,name,&s->game.clock));
    assert(!hmbtn_livestock_breed(p,0,true) && !hmbtn_livestock_breed(p,1,false));
    p->animals[0].days_sick=1;assert(!hmbtn_livestock_breed(p,0,false));p->animals[0].days_sick=0;
    p->animals[1].days_since_shearing=6;assert(!hmbtn_livestock_breed(p,1,true));p->animals[1].days_since_shearing=7;
    for(unsigned i=2;i<10;++i) assert(hmbtn_livestock_buy(p,0x165,name,&s->game.clock));
    assert(!hmbtn_livestock_breed(p,0,false));p->animals[9]=(HmbtnFarmAnimal){0};
    assert(hmbtn_livestock_breed(p,0,false) && p->births[0].child==9 && p->animals[9].reserved && !p->animals[9].occupied);
    assert(hmbtn_livestock_free(p)==HMBTN_LIVESTOCK && hmbtn_livestock_count(p,false)==1 && hmbtn_livestock_count(p,true)==8);
    assert(!hmbtn_livestock_breed(p,0,false) && !hmbtn_livestock_buy(p,0x165,name,&s->game.clock));
    /* The actual message insertion format uses digit glyphs 0x40..0x49. */
    uint8_t text[8192]={0};p->talk_slot=0;
    assert(hmbtn_livestock_talk(p,0)==0x6c3);
    hmbtn_write_le16(text+0x1800,0x4001);hmbtn_write_le16(text+0x1802,0xffff);
    assert(hmbtn_livestock_expand(p,text,sizeof(text)) && hmbtn_read_le16(text+0x1800)==0x42 && hmbtn_read_le16(text+0x1802)==0x40);
    assert(!hmbtn_livestock_sale_price(p,0) && !hmbtn_livestock_harvest(p,0,false) && hmbtn_livestock_pack(&p->animals[0])==3);
    p->animals[8]=(HmbtnFarmAnimal){0};assert(!hmbtn_livestock_breed(p,1,true)); /* One maternity pen. */
    s->poultry.upgraded=true;s->construction.house_level=1;s->construction.barn_level=1;
    assert(hmbtn_livestock_breed(p,1,true) && p->births[1].child==8 && hmbtn_livestock_pack(&p->animals[1])==7);
    assert(!hmbtn_livestock_harvest(p,1,true));
    assert(hmbtn_livestock_sale_price(p,1)==2000); /* Exact sheep filter=0. */
    assert(hmbtn_livestock_valid_births(p));p->births[1].child=9;assert(!hmbtn_livestock_valid_births(p));p->births[1].child=8;
    roundtrip(&s);p=&s->livestock;assert(p->births[0].child==9 && p->births[1].child==8);
    /* Full care each night; children are not fed or aged while merely reserved. */
    for(unsigned day=1;day<=20;++day) {
        for(unsigned i=0;i<22;++i) s->feed.barn[i]=true;
        hmbtn_session_advance_minutes(s,1440);
        assert(p->animals[0].fed_yesterday && p->animals[1].fed_yesterday && !p->animals[9].occupied);
        if(day<20) assert(p->animals[0].pregnancy==day+1 && !hmbtn_livestock_birth_ready(p,0));
        if(day==10) { roundtrip(&s);p=&s->livestock;assert(p->animals[0].pregnancy==11); }
    }
    assert(!p->animals[0].pregnancy && hmbtn_livestock_birth_ready(p,0) && hmbtn_livestock_birth_ready(p,1));
    roundtrip(&s);p=&s->livestock;
    hmbtn_livestock_birth_begin(p,&s->game.clock);HmbtnFarmAnimal *calf=hmbtn_livestock_newborn(p,0),*lamb=hmbtn_livestock_newborn(p,1);
    assert(calf && lamb && hmbtn_livestock_kind(calf)==0x160 && hmbtn_livestock_kind(lamb)==0x164
        && calf->affection==10 && !calf->growth && calf->actor.position[2]==p->animals[0].actor.position[2]-200);
    memcpy(calf->name,name,sizeof(name));hmbtn_livestock_birth_begin(p,&s->game.clock);assert(calf->name[0]==0x10);
    memcpy(lamb->name,name,sizeof(name));hmbtn_livestock_birth_end(p);hmbtn_livestock_birth_end(p);
    assert(!p->births[0].active && !p->births[1].active && !calf->reserved && !lamb->reserved && hmbtn_livestock_valid_births(p));
    assert(hmbtn_livestock_count(p,false)==2 && hmbtn_livestock_count(p,true)==8);roundtrip(&s);p=&s->livestock;
    /* Sale of a pregnant sheep releases her reservation, without deleting a neighbor. */
    assert(hmbtn_livestock_breed(p,1,true));unsigned child=p->births[0].child;
    assert(hmbtn_livestock_sell(p,1)==2000 && !p->animals[child].reserved && hmbtn_livestock_valid_births(p));
    /* Uses original potion ANI bank 26, consumes once even on a missed target. */
    s->game.inventory.equipped=(HmbtnStoredTool){0x1f,0,2};s->intro.player.facing=3;
    memcpy(s->intro.player.position,p->animals[0].actor.position,sizeof(s->intro.player.position));s->intro.player.position[0]-=128;
    assert(hmbtn_animal_medicine_tick(&s->poultry,p,96,&s->farm_action,&s->intro.player,&s->house,HMBTN_PAD_SQUARE));
    assert(s->farm_action.bank==26 && !p->animals[0].pregnancy);s->intro.player.animation_done=true;
    assert(hmbtn_animal_medicine_tick(&s->poultry,p,96,&s->farm_action,&s->intro.player,&s->house,0));
    assert(p->animals[0].pregnancy==1 && s->game.inventory.equipped.count==1);
    assert(!hmbtn_animal_medicine_tick(&s->poultry,p,96,&s->farm_action,&s->intro.player,&s->house,0));
    s->intro.player.position[0]=50000;
    assert(hmbtn_animal_medicine_tick(&s->poultry,p,96,&s->farm_action,&s->intro.player,&s->house,HMBTN_PAD_SQUARE));s->intro.player.animation_done=true;
    assert(hmbtn_animal_medicine_tick(&s->poultry,p,96,&s->farm_action,&s->intro.player,&s->house,0) && !s->game.inventory.equipped.id && !s->farm_action.changed);
    /* Shop transactions: funds, room, description, ownership in toolbox, no duplicates. */
    s->feed_shop.phase=HMBTN_SHOP_SELECT;s->feed_shop.merchant=15;s->feed_shop.request=0x46;
    choose(s,8);s->game.economy.gold=2999;input(s,HMBTN_PAD_CROSS);assert(s->feed_shop.next_request==0x4f);until(s,HMBTN_SHOP_SELECT);
    s->game.economy.gold=6000;for(unsigned i=0;i<8;++i) s->game.inventory.bag[i]=(HmbtnStoredTool){1,0,1};
    s->game.inventory.equipped=(HmbtnStoredTool){2,0,1};input(s,HMBTN_PAD_CROSS);assert(s->feed_shop.next_request==0x54 && s->game.economy.gold==6000);
    until(s,HMBTN_SHOP_SELECT);s->game.inventory.bag[0]=(HmbtnStoredTool){0};
    input(s,HMBTN_PAD_TRIANGLE);assert(s->feed_shop.next_request==0x65);until(s,HMBTN_SHOP_SELECT);
    input(s,HMBTN_PAD_CROSS);assert(s->feed_shop.next_request==0x84 && s->game.economy.gold==3000);
    assert(hmbtn_inventory_count(&s->game.inventory,0x1f,0,2)==1);until(s,HMBTN_SHOP_SELECT);
    assert(!s->feed_shop.potion_available[0] && s->feed_shop.potion_available[1]);
    s->game.inventory.toolbox[0]=s->game.inventory.bag[0];s->game.inventory.bag[0]=(HmbtnStoredTool){0};input(s,0);
    assert(!s->feed_shop.potion_available[0]);choose(s,9);input(s,HMBTN_PAD_CROSS);assert(s->game.economy.gold==0);
    assert(hmbtn_inventory_count(&s->game.inventory,0x20,0,2)==1);
    hmbtn_session_destroy(s);puts("Breeding: source refusals/capacity/gestation/birth, dose/stock, products/sales, reservation integrity and v12 roundtrip passed.");return 0;
}

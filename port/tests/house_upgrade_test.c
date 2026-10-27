#include "save_fixture.h"
#include "inventory.h"
#include "world_map.h"
#include "cooking.h"
#include <stdlib.h>
#include <stdio.h>
static void ingredients(HmbtnInventory *h,unsigned a,unsigned b)
{
    memset(h->items,0,sizeof(h->items));memset(h->icebox,0,sizeof(h->icebox));h->held_item=(HmbtnStoredTool){0};
    h->items[0]=(HmbtnStoredTool){(uint16_t)a,0,1};h->icebox[0]=(HmbtnStoredTool){(uint16_t)b,0,3};
}
int main(void)
{
    char error[256]={0};HmbtnSession *s=save_fixture(1);HmbtnInventory *h=&s->game.inventory;
    s->poultry.upgraded=true;s->game.economy.gold=4699;h->supplies[1]=370;
    assert(hmbtn_construction_next(&s->construction,&s->poultry)==1);
    const HmbtnConstructionOffer *offer=hmbtn_construction_offer(1);assert(offer && offer->gold==4700 && offer->lumber==370);
    assert(hmbtn_construction_order(&s->construction,&s->poultry,h,&s->game.economy)==HMBTN_BUILD_GOLD && h->supplies[1]==370);
    s->game.economy.gold=4700;h->supplies[1]=369;
    assert(hmbtn_construction_order(&s->construction,&s->poultry,h,&s->game.economy)==HMBTN_BUILD_LUMBER && s->game.economy.gold==4700);
    h->supplies[1]=370;
    assert(hmbtn_construction_order(&s->construction,&s->poultry,h,&s->game.economy)==HMBTN_BUILD_OK);
    assert(s->construction.pending==HMBTN_BUILD_HOUSE && s->construction.mornings_left==4 && !h->supplies[1] && !s->game.economy.gold);
    assert(hmbtn_construction_order(&s->construction,&s->poultry,h,&s->game.economy)==HMBTN_BUILD_BUSY);
    uint8_t *bytes=NULL;size_t n=0;
    assert(hmbtn_save_encode(s,&bytes,&n,error,sizeof(error)));
    HmbtnSession *loaded=hmbtn_save_decode(bytes,n,error,sizeof(error));assert(loaded);free(bytes);hmbtn_session_destroy(s);s=loaded;h=&s->game.inventory;
    assert(s->construction.pending==HMBTN_BUILD_HOUSE && s->house.cooking==&s->game.cooking && s->menu.construction==&s->construction);
    for(unsigned day=0;day<4;++day) {
        hmbtn_session_advance_minutes(s,1440);assert(s->construction.house_level==(day==3) && s->construction.mornings_left==3-day);
        hmbtn_session_clock_events(s,&s->game.clock,HMBTN_CLOCK_DAY);assert(s->construction.mornings_left==3-day);
    }
    assert(s->construction.pending==HMBTN_BUILD_NONE && hmbtn_construction_next(&s->construction,&s->poultry)==2);
    assert(hmbtn_village_flag(&s->village,0x3b) && !hmbtn_village_flag_set(&s->village,0x3b,false));
    assert(hmbtn_farm_menu_building_score(&s->menu)==40);
    assert(hmbtn_session_morning(s,false));assert(s->intro.player.position[0]==791 && s->intro.player.position[2]==258);hmbtn_session_morning_ready(s);
    s->morning_fade=0; /* Unit fixture: no host presentation. */
    for(unsigned season=0;season<4;++season) {
        unsigned logical,resource;
        assert(hmbtn_world_area_buildings(0,season,true,1,&logical,&resource) && logical==0 && resource==8+season);
        assert(hmbtn_world_area_buildings(80,season,true,1,&logical,&resource) && logical==80 && resource==84+season);
        assert(hmbtn_world_area_buildings(112,season,true,1,&logical,&resource) && logical==112 && resource==116+season);
    }
    /* item_storage rules, exact item classes, stack/split and refusal atomicity. */
    memset(h->items,0,sizeof(h->items));h->held_item=(HmbtnStoredTool){0x43,7,1};
    assert(hmbtn_inventory_storage_accepts(2,0x43) && !hmbtn_inventory_storage_accepts(1,0x43));
    assert(hmbtn_inventory_storage_swap(h,2,0,0,9,0) && !h->held_item.id && h->icebox[0].count==1);
    h->icebox[0].count=99;h->items[0]=(HmbtnStoredTool){0x43,9,1};
    assert(hmbtn_inventory_storage_swap(h,2,1,0,9,0) && h->items[0].count==1 && h->icebox[0].count==99);
    assert(hmbtn_inventory_storage_swap(h,2,9,0,0,0) && h->held_item.count==1 && h->held_item.modifier==7 && h->icebox[0].count==98);
    HmbtnInventory before=*h;assert(!hmbtn_inventory_storage_swap(h,1,0,0,9,0) && !memcmp(h,&before,sizeof(*h)));
    h->held_item=(HmbtnStoredTool){0x7b,0,1}; /* Silver ore belongs to cabinet. */
    assert(hmbtn_inventory_storage_accepts(1,h->held_item.id));
    assert(hmbtn_inventory_storage_swap(h,1,0,0,9,7) && h->cabinet[56].id==0x7b && !h->held_item.id);
    assert(hmbtn_inventory_count(h,0x43,1,2)==2);
    /* Original recipe table priority, grouped requirements and quality bonuses. */
    HmbtnRecipeRecord dish;uint16_t pick[8]={0x58,0x99};
    assert(hmbtn_cooking_evaluate(0,pick,&dish) && dish.id==0xcd && dish.quality==30);
    ingredients(h,0x58,0x99);s->game.cooking.utensils=0;
    before=*h;pick[2]=0x43;
    assert(!hmbtn_cooking_make(&s->game.cooking,h,0,pick,&dish) && !memcmp(h,&before,sizeof(*h)));pick[2]=0;
    assert(hmbtn_cooking_make(&s->game.cooking,h,0,pick,&dish) && !h->items[0].id && h->icebox[0].count==2 && h->held_item.id==0xcd);
    assert(hmbtn_cooking_remember(&s->game.cooking,&dish) && hmbtn_cooking_record_count(&s->game.cooking)==1);
    before=*h;assert(!hmbtn_cooking_make(&s->game.cooking,h,0,pick,&dish) && !memcmp(h,&before,sizeof(*h)));
    uint16_t cucumber[8]={0x43};assert(hmbtn_cooking_evaluate(0x100,cucumber,&dish) && dish.id==0xba && dish.quality==20);
    assert(hmbtn_cooking_evaluate(0,cucumber,&dish) && dish.id==0xe2 && dish.quality==1);
    assert(hmbtn_cooking_evaluate(0x101,cucumber,&dish) && dish.id==0xba && dish.quality==23);
    uint16_t egg[8]={0x122};
    assert(hmbtn_cooking_evaluate(4,egg,&dish) && dish.id!=0xe2);
    ingredients(h,0x43,0x43);before=*h;
    assert(!hmbtn_cooking_make(&s->game.cooking,h,0x100,cucumber,&dish) && !memcmp(h,&before,sizeof(*h)));
    s->game.cooking.utensils=0x80;
    assert(hmbtn_cooking_make(&s->game.cooking,h,0x100,cucumber,&dish) && dish.id==0xba && !h->items[0].id && h->icebox[0].count==3);
    assert(hmbtn_cooking_remember(&s->game.cooking,&dish) && hmbtn_cooking_record_count(&s->game.cooking)==2);
    /* Current saves preserve storage, recipe quality/selection, owners and house level. */
    static const uint8_t schedules[1]={0};assert(hmbtn_session_bind_schedules(s,schedules,1));
    assert(hmbtn_save_encode(s,&bytes,&n,error,sizeof(error)));loaded=hmbtn_save_decode(bytes,n,error,sizeof(error));
    if(!loaded) fprintf(stderr,"%s\n",error);
    assert(loaded && loaded->construction.house_level==1 && loaded->game.inventory.cabinet[56].id==0x7b);
    assert(loaded->game.cooking.utensils==0x80 && hmbtn_cooking_record_count(&loaded->game.cooking)==2
        && loaded->house.inventory==&loaded->game.inventory && loaded->house.cooking==&loaded->game.cooking);
    free(bytes);hmbtn_session_destroy(loaded);hmbtn_session_destroy(s);
    puts("House1: original work/resources, four mornings, stable logical areas, storage classes/stacks, shared cooking/records and current save roundtrip passed.");return 0;
}

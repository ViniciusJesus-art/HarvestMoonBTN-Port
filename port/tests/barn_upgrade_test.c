#include "save_fixture.h"
#include "world_map.h"
#include "psx_data.h"
#include <stdio.h>
#include <stdlib.h>

static void levels(HmbtnSession *s,char *error,size_t size)
{
    uint8_t code[92]={0};hmbtn_write_le16(code+2,1);hmbtn_write_le16(code+4,0x4000);
    hmbtn_write_le32(code+56,32);code[60]=95;code[61]=code[62]=255;
    hmbtn_write_le16(code+64,0);hmbtn_write_le16(code+66,1);
    hmbtn_write_le32(code+68,2);hmbtn_write_le32(code+72,3);code[76]=255;
    assert(hmbtn_village_talk_open(&s->talk,24,code,sizeof(code),&s->village,&s->intro.player,
        &s->game.clock,&s->game.registration,&s->house,error,size));
    assert(hmbtn_village_talk_tick(&s->talk,error,size));
    assert(!s->talk.active && !s->talk.script.boundary);
    assert(s->talk.slots[0]==1 && s->talk.slots[1]==(int32_t)s->construction.barn_level
        && s->talk.slots[2]==1 && !s->talk.slots[3]);
}
int main(void)
{
    char error[256]={0};HmbtnSession *s=save_fixture(1);
    HmbtnInventory *inv=&s->game.inventory;s->poultry.upgraded=true;s->construction.house_level=1;
    const HmbtnConstructionOffer *offer=hmbtn_construction_offer(2);
    assert(offer && offer->gold==6800 && offer->lumber==500);
    assert(hmbtn_construction_next(&s->construction,&s->poultry)==2);
    s->game.economy.gold=6799;inv->supplies[1]=500;
    assert(hmbtn_construction_order(&s->construction,&s->poultry,inv,&s->game.economy)==HMBTN_BUILD_GOLD);
    assert(inv->supplies[1]==500 && !s->construction.mornings_left);
    s->game.economy.gold=6800;inv->supplies[1]=499;
    assert(hmbtn_construction_order(&s->construction,&s->poultry,inv,&s->game.economy)==HMBTN_BUILD_LUMBER);
    assert(s->game.economy.gold==6800 && !s->construction.mornings_left);
    inv->supplies[1]=500;levels(s,error,sizeof(error));
    assert(hmbtn_construction_order(&s->construction,&s->poultry,inv,&s->game.economy)==HMBTN_BUILD_OK);
    assert(s->construction.pending==HMBTN_BUILD_BARN && s->construction.mornings_left==4
        && !s->construction.barn_level && !inv->supplies[1] && !s->game.economy.gold);
    assert(hmbtn_construction_order(&s->construction,&s->poultry,inv,&s->game.economy)==HMBTN_BUILD_BUSY);
    uint8_t *data=NULL;size_t bytes=0;
    for(unsigned day=0;day<4;++day) {
        assert(hmbtn_save_encode(s,&data,&bytes,error,sizeof(error)));
        HmbtnSession *loaded=hmbtn_save_decode(data,bytes,error,sizeof(error));assert(loaded);free(data);data=NULL;
        hmbtn_session_destroy(s);s=loaded;inv=&s->game.inventory;
        static const uint8_t schedules[1]={0};assert(hmbtn_session_bind_schedules(s,schedules,1));
        assert(s->construction.pending==HMBTN_BUILD_BARN && s->construction.mornings_left==4-day);
        hmbtn_session_advance_minutes(s,1440);
        assert(s->construction.barn_level==(day==3) && s->construction.mornings_left==3-day);
        hmbtn_session_clock_events(s,&s->game.clock,HMBTN_CLOCK_DAY);
        assert(s->construction.mornings_left==3-day);
    }
    assert(s->construction.pending==HMBTN_BUILD_NONE && hmbtn_construction_next(&s->construction,&s->poultry)==3);
    assert(hmbtn_village_flag(&s->village,0x3c) && !s->village.flags[0x3c]);
    assert(!hmbtn_village_flag_set(&s->village,0x3c,false));
    assert(!hmbtn_village_flag(&s->village,0x38) && hmbtn_farm_menu_building_score(&s->menu)==60);
    levels(s,error,sizeof(error));
    assert(hmbtn_construction_order(&s->construction,&s->poultry,inv,&s->game.economy)==HMBTN_BUILD_GOLD);
    assert(hmbtn_carpenter_shop_open(&s->feed_shop,&s->game.clock));
    assert(hmbtn_carpenter_shop_rows(&s->feed_shop)==2);s->feed_shop.phase=HMBTN_SHOP_OFF;
    for(unsigned season=0;season<4;++season) {
        unsigned logical,resource;
        assert(hmbtn_world_area_buildings(0,season,true,1,1,false,&logical,&resource) && !logical && resource==12+season);
        assert(hmbtn_world_area_buildings(96,season,true,1,1,false,&logical,&resource) && logical==96 && resource==100+season);
        assert(hmbtn_world_area_buildings(80,season,true,1,1,false,&logical,&resource) && logical==80 && resource==84+season);
        assert(hmbtn_world_area_buildings(112,season,true,1,1,false,&logical,&resource) && logical==112 && resource==116+season);
    }
    unsigned logical,resource;
    assert(!hmbtn_world_area_buildings(96,0,true,1,2,false,&logical,&resource));
    /* Stores and feed stay owned by the existing domains after the upgrade. */
    inv->icebox[4]=(HmbtnStoredTool){0x43,7,20};s->feed.barn[19]=true;s->feed.barn[21]=true;
    assert(hmbtn_save_encode(s,&data,&bytes,error,sizeof(error)));
    HmbtnSession *loaded=hmbtn_save_decode(data,bytes,error,sizeof(error));assert(loaded);free(data);data=NULL;
    assert(loaded->construction.barn_level==1 && loaded->village.construction==&loaded->construction
        && loaded->feed_shop.construction==&loaded->construction && loaded->menu.construction==&loaded->construction);
    assert(loaded->feed.barn[19] && loaded->feed.barn[21] && loaded->game.inventory.icebox[4].count==20);
    hmbtn_session_destroy(loaded);
    /* Coherent current saves only: no completed barn before house1, no stale order. */
    s->construction.house_level=0;assert(!hmbtn_save_encode(s,&data,&bytes,error,sizeof(error)));
    s->construction.house_level=1;s->construction.pending=HMBTN_BUILD_BARN;s->construction.mornings_left=1;
    assert(!hmbtn_save_encode(s,&data,&bytes,error,sizeof(error)));
    s->construction.barn_level=0;s->construction.pending=HMBTN_BUILD_NONE;
    assert(!hmbtn_save_encode(s,&data,&bytes,error,sizeof(error)));
    s->construction.mornings_left=0;s->construction.barn_level=2;
    assert(!hmbtn_save_encode(s,&data,&bytes,error,sizeof(error)));
    hmbtn_session_destroy(s);
    puts("Barn work: original cost, atomic order, four mornings/restarts, derived levels/flags/score, seasonal resources and save integrity passed.");return 0;
}

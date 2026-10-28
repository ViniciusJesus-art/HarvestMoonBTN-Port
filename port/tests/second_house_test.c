#include "save_fixture.h"
#include "world_map.h"
#include "cooking.h"
#include "psx_data.h"
#include <stdio.h>
#include <stdlib.h>
static void roundtrip(HmbtnSession **s)
{
    char error[256]={0};uint8_t *data=NULL;size_t bytes=0;
    assert(hmbtn_save_encode(*s,&data,&bytes,error,sizeof(error)));
    HmbtnSession *loaded=hmbtn_save_decode(data,bytes,error,sizeof(error));assert(loaded);free(data);
    hmbtn_session_destroy(*s);*s=loaded;
    static const uint8_t schedules[1]={0};assert(hmbtn_session_bind_schedules(loaded,schedules,1));
}
static void fireplace(HmbtnSession *s)
{
    uint8_t section[8192]={0};uint8_t *z=section+16;
    hmbtn_write_le16(section+4,91);hmbtn_write_le16(z,0x4000);hmbtn_write_le16(z+2,18);
    hmbtn_write_le16(z+8,2);hmbtn_write_le16(z+14,(uint16_t)-100);hmbtn_write_le16(z+18,100);
    hmbtn_write_le16(z+20,100);hmbtn_write_le16(z+24,350);
    HmbtnHouseLumber *l=&s->house_lumber;HmbtnInventory *h=&s->game.inventory;
    HmbtnIntroActor *p=&s->intro.player;memset(p->position,0,sizeof(p->position));p->facing=2;
    bool *lit=&s->village.flags[0x45];int sound;
    s->farm_action.fatigue=12;
    for(unsigned n=0;n<2;++n) {
        h->held_item=(HmbtnStoredTool){n ? 0x13d : 0x3f,0,1};p->animation_done=false;
        assert(hmbtn_house_lumber_tick(l,h,p,&s->farm_action,lit,section,sizeof(section),HMBTN_PAD_CROSS,&sound));
        assert(l->phase==3 && h->held_item.id && !*lit);
        char error[128];assert(!hmbtn_save_available(s,error,sizeof(error)));
        p->animation_done=true;
        assert(hmbtn_house_lumber_tick(l,h,p,&s->farm_action,lit,section,sizeof(section),0,&sound));
        assert(!l->phase && !h->held_item.id && *lit && l->deliveries==n+1 && s->farm_action.fatigue==7);
        *lit=false; /* Original area-entry latch; daily count survives. */
    }
    h->held_item=(HmbtnStoredTool){0x3f,0,1};hmbtn_write_le16(section+4,88);
    assert(hmbtn_house_lumber_tick(l,h,p,&s->farm_action,lit,section,sizeof(section),HMBTN_PAD_CROSS,&sound));
    assert(!l->phase && h->held_item.id==0x3f && !*lit); /* No accidental throwing. */
    hmbtn_write_le16(section+4,91);h->held_item.id=0x43;
    assert(hmbtn_house_lumber_tick(l,h,p,&s->farm_action,lit,section,sizeof(section),HMBTN_PAD_CROSS,&sound));
    assert(!l->phase && h->held_item.id==0x43);
    hmbtn_write_le16(z+2,13);h->held_item=(HmbtnStoredTool){0};h->supplies[1]=2;
    assert(hmbtn_house_lumber_tick(l,h,p,&s->farm_action,lit,section,sizeof(section),HMBTN_PAD_CROSS,&sound));
    assert(l->phase==1 && h->supplies[1]==1 && h->held_item.id==0x3f && sound==7);
    p->animation_done=true;assert(hmbtn_house_lumber_tick(l,h,p,&s->farm_action,lit,section,sizeof(section),0,&sound));
    assert(hmbtn_house_lumber_tick(l,h,p,&s->farm_action,lit,section,sizeof(section),HMBTN_PAD_CROSS,&sound));
    p->animation_done=true;assert(hmbtn_house_lumber_tick(l,h,p,&s->farm_action,lit,section,sizeof(section),0,&sound));
    assert(h->supplies[1]==2 && !h->held_item.id && !l->phase);
    /* Preserve native stock above the original 999; withdrawing/returning must
     * not destroy one unit from a valid existing harvest stock. */
    h->supplies[1]=9999;
    assert(hmbtn_house_lumber_tick(l,h,p,&s->farm_action,lit,section,sizeof(section),HMBTN_PAD_CROSS,&sound));
    assert(h->supplies[1]==9998 && h->held_item.id==0x3f);
    p->animation_done=true;assert(hmbtn_house_lumber_tick(l,h,p,&s->farm_action,lit,section,sizeof(section),0,&sound));
    assert(hmbtn_house_lumber_tick(l,h,p,&s->farm_action,lit,section,sizeof(section),HMBTN_PAD_CROSS,&sound));
    p->animation_done=true;assert(hmbtn_house_lumber_tick(l,h,p,&s->farm_action,lit,section,sizeof(section),0,&sound));
    assert(h->supplies[1]==9999 && !h->held_item.id);
}
int main(void)
{
    HmbtnSession *s=save_fixture(1);s->poultry.upgraded=true;
    s->construction.house_level=s->construction.barn_level=1;
    const HmbtnConstructionOffer *offer=hmbtn_construction_offer(3);
    assert(offer && offer->gold==10000 && offer->lumber==750 && hmbtn_construction_offer(4) && !hmbtn_construction_offer(5));
    s->game.economy.gold=9999;s->game.inventory.supplies[1]=750;
    assert(hmbtn_construction_order(&s->construction,&s->poultry,&s->game.inventory,&s->game.economy)==HMBTN_BUILD_GOLD);
    assert(s->game.inventory.supplies[1]==750 && !s->construction.mornings_left);
    s->game.economy.gold=10000;s->game.inventory.supplies[1]=749;
    assert(hmbtn_construction_order(&s->construction,&s->poultry,&s->game.inventory,&s->game.economy)==HMBTN_BUILD_LUMBER);
    assert(s->game.economy.gold==10000 && !s->construction.mornings_left);
    s->game.inventory.supplies[1]=750;
    s->game.inventory.icebox[63]=(HmbtnStoredTool){0x43,7,23};
    s->game.inventory.cabinet[63]=(HmbtnStoredTool){0x7b,0,9};
    s->game.inventory.toolbox[63]=(HmbtnStoredTool){0x24,0,7};
    const uint16_t ingredients[8]={0x58,0x99};HmbtnRecipeRecord recipe;
    assert(hmbtn_cooking_evaluate(0,ingredients,&recipe) && hmbtn_cooking_remember(&s->game.cooking,&recipe));
    s->game.cooking.utensils=0x80;
    const uint16_t name[9]={0x100,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff};
    assert(hmbtn_livestock_buy(&s->livestock,0x161,name,&s->game.clock));
    s->livestock.animals[0].affection=200;
    assert(hmbtn_construction_order(&s->construction,&s->poultry,&s->game.inventory,&s->game.economy)==HMBTN_BUILD_OK);
    assert(s->construction.pending==HMBTN_BUILD_HOUSE2 && s->construction.mornings_left==4
        && !s->game.economy.gold && !s->game.inventory.supplies[1]);
    assert(hmbtn_construction_order(&s->construction,&s->poultry,&s->game.inventory,&s->game.economy)==HMBTN_BUILD_BUSY);
    for(unsigned day=0;day<4;++day) {
        roundtrip(&s);assert(s->construction.mornings_left==4-day && s->construction.house_level==1);
        s->feed.barn[0]=true;hmbtn_session_advance_minutes(s,1440);
        assert(s->construction.mornings_left==3-day && s->construction.house_level==(day==3 ? 2u : 1u));
        hmbtn_session_clock_events(s,&s->game.clock,HMBTN_CLOCK_DAY);assert(s->construction.mornings_left==3-day);
    }
    assert(s->construction.pending==HMBTN_BUILD_NONE && hmbtn_construction_next(&s->construction,&s->poultry)==4);
    assert(hmbtn_village_flag(&s->village,0x3d) && !s->village.flags[0x3d] && !hmbtn_village_flag_set(&s->village,0x3d,false));
    assert(hmbtn_farm_menu_building_score(&s->menu)==80 && hmbtn_livestock_capacity(&s->livestock)==20);
    /* The real command 95 must expose the same level, not its old boolean. */
    uint8_t code[92]={0};hmbtn_write_le16(code+2,1);hmbtn_write_le16(code+4,0x4000);
    hmbtn_write_le32(code+56,32);code[60]=95;code[61]=code[62]=255;
    hmbtn_write_le16(code+64,0);hmbtn_write_le16(code+66,1);
    hmbtn_write_le32(code+68,2);hmbtn_write_le32(code+72,3);code[76]=255;
    char error[256];
    assert(hmbtn_village_talk_open(&s->talk,24,code,sizeof(code),&s->village,&s->intro.player,
        &s->game.clock,&s->game.registration,&s->house,error,sizeof(error)));
    assert(hmbtn_village_talk_tick(&s->talk,error,sizeof(error)) && !s->talk.active && !s->talk.script.boundary);
    assert(s->talk.slots[0]==2 && s->talk.slots[1]==1 && s->talk.slots[2]==1 && !s->talk.slots[3]);
    for(unsigned season=0;season<4;++season) {
        unsigned logical,resource;
        assert(hmbtn_world_area_buildings(0,season,true,2,1,false,&logical,&resource) && logical==0 && resource==16+season);
        assert(hmbtn_world_area_buildings(80,season,true,2,1,false,&logical,&resource) && logical==80 && resource==88+season);
        assert(hmbtn_world_area_buildings(96,season,true,2,1,false,&logical,&resource) && logical==96 && resource==100+season);
    }
    assert(hmbtn_session_morning(s,false));assert(s->intro.player.position[0]==902 && s->intro.player.position[2]==1804);
    hmbtn_session_morning_ready(s);s->morning_fade=0;
    HmbtnHouseSleep sleep;HmbtnIntroActor player=s->intro.player;
    hmbtn_house_sleep_start(&sleep,&player,2);assert(sleep.target[0]==-2950 && sleep.target[2]==-2560);
    fireplace(s);s->village.flags[0x45]=true;roundtrip(&s);
    assert(s->construction.house_level==2 && s->house_lumber.deliveries==2 && s->village.flags[0x45]);
    assert(s->game.inventory.icebox[63].count==23 && s->game.inventory.cabinet[63].count==9 && s->game.inventory.toolbox[63].count==7);
    assert(s->game.cooking.utensils==0x80 && hmbtn_cooking_record_count(&s->game.cooking)==1
        && s->game.cooking.records[0].quality==30 && hmbtn_livestock_count(&s->livestock,false)==1);
    assert(s->house.inventory==&s->game.inventory && s->house.cooking==&s->game.cooking
        && s->livestock.construction==&s->construction && s->menu.construction==&s->construction);
    assert(hmbtn_session_morning(s,false) && !s->house_lumber.deliveries);
    hmbtn_session_morning_ready(s);s->morning_fade=0;
    uint8_t *data=NULL;size_t bytes=0;
    s->construction.barn_level=0;assert(!hmbtn_save_encode(s,&data,&bytes,error,sizeof(error)));
    s->construction.barn_level=1;s->construction.house_level=3;assert(!hmbtn_save_encode(s,&data,&bytes,error,sizeof(error)));
    s->construction.house_level=2;s->construction.pending=HMBTN_BUILD_HOUSE2;s->construction.mornings_left=1;
    assert(!hmbtn_save_encode(s,&data,&bytes,error,sizeof(error)));
    hmbtn_session_destroy(s);
    puts("House2: atomic costs, four mornings/restarts, flags/score, resources, bed/wake, wood/fireplace and shared-state persistence passed.");return 0;
}

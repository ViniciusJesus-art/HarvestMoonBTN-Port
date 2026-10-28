#include "save_fixture.h"
#include "world_map.h"
#include "psx_data.h"
#include <stdio.h>
#include <stdlib.h>
static void roundtrip(HmbtnSession **s)
{
    uint8_t *data=NULL;size_t bytes=0;char error[256]={0};
    assert(hmbtn_save_encode(*s,&data,&bytes,error,sizeof(error)));
    HmbtnSession *next=hmbtn_save_decode(data,bytes,error,sizeof(error));
    if(!next) fprintf(stderr,"%s\n",error);
    assert(next);free(data);hmbtn_session_destroy(*s);*s=next;
    static const uint8_t schedules[1]={0};assert(hmbtn_session_bind_schedules(next,schedules,1));
}
static void seasons(HmbtnFarmField *f)
{
    assert(f->indoor && hmbtn_farm_field_cells(f)==49 && hmbtn_farm_field_columns(f)==7);
    for(unsigned row=0;row<7;++row) for(unsigned col=0;col<7;++col) {
        int32_t pos[3];int c,r;
        assert(hmbtn_farm_field_position(f,col,row,pos) && hmbtn_farm_field_world_cell(f,pos,&c,&r));
        assert(c==(int)col && r==(int)row);
    }
    int32_t pos[3]={-1621,0,1020};int c,r;
    assert(!hmbtn_farm_field_world_cell(f,pos,&c,&r) && c==-1);
    pos[0]=1180;assert(!hmbtn_farm_field_world_cell(f,pos,&c,&r) && c==7);
    assert(!hmbtn_farm_field_position(f,7,0,pos));
    for(unsigned season=0;season<4;++season) for(unsigned seed=0x22;seed<=0x35;++seed) {
        hmbtn_farm_field_clear(f);f->season=season;
        assert(hmbtn_farm_field_tool(f,3,3,1,0,1,NULL));
        assert(hmbtn_farm_field_plant(f,3,3,seed));
        const unsigned actual=f->cells[24].value;
        const HmbtnFarmCrop *crop=hmbtn_farm_crop(actual);assert(crop);
        unsigned days=0;for(unsigned i=0;i<crop->grown;++i) days+=crop->days[i] ? crop->days[i] : 1;
        hmbtn_farm_field_new_day(f,season);assert(!f->cells[24].watered_days && !f->cells[24].kind);
        for(unsigned day=0;day<days;++day) {
            assert(hmbtn_farm_field_water(f,24));
            /* Crossing a season never kills the protected crop. */
            hmbtn_farm_field_new_day(f,(season+(day>1))%4);
        }
        assert(f->cells[24].kind==crop->grown && hmbtn_farm_field_harvest_item(f,24)==(unsigned)crop->harvest);
        assert(hmbtn_farm_field_harvest(f,24)==(unsigned)crop->harvest);
        assert(!hmbtn_farm_field_harvest(f,24));
        if(crop->regrow>=0) {
            assert(f->cells[24].kind==(unsigned)crop->regrow && (f->cells[24].growth_flags&0x8000));
            assert(hmbtn_farm_field_water(f,24));hmbtn_farm_field_new_day(f,3);
            assert(!(f->cells[24].growth_flags&0x8000) && !f->cells[24].watered_days);
        }
        assert(!hmbtn_farm_field_plant(f,7,0,seed) && !hmbtn_farm_field_water(f,49));
    }
    hmbtn_farm_field_clear(f);f->season=3;
    assert(hmbtn_farm_field_tool(f,0,0,1,0,1,NULL));
    const HmbtnFarmTile before=f->cells[0];hmbtn_farm_field_rain(f);
    assert(!memcmp(&before,&f->cells[0],sizeof(before)));
    /* Grass uses the same shapes/growth/cut engine; winter reset is outdoor-only. */
    assert(hmbtn_farm_field_plant(f,0,0,0x36));
    for(unsigned day=0;day<11;++day) hmbtn_farm_field_new_day(f,3);
    assert(f->cells[0].kind==3);unsigned stocks[4]={0};
    assert(hmbtn_farm_field_tool(f,0,0,0,0,1,stocks) && stocks[0]==1 && f->cells[0].kind==1);
    hmbtn_farm_field_clear(f);
}
int main(void)
{
    HmbtnSession *s=save_fixture(1);s->poultry.upgraded=true;
    s->construction.house_level=2;s->construction.barn_level=1;
    const HmbtnConstructionOffer *offer=hmbtn_construction_offer(4);
    assert(offer && offer->gold==30000 && offer->lumber==580 && !hmbtn_construction_offer(5));
    s->game.economy.gold=29999;s->game.inventory.supplies[1]=580;
    assert(hmbtn_construction_order(&s->construction,&s->poultry,&s->game.inventory,&s->game.economy)==HMBTN_BUILD_GOLD);
    assert(s->game.inventory.supplies[1]==580 && !s->construction.mornings_left);
    s->game.economy.gold=30000;s->game.inventory.supplies[1]=579;
    assert(hmbtn_construction_order(&s->construction,&s->poultry,&s->game.inventory,&s->game.economy)==HMBTN_BUILD_LUMBER);
    assert(s->game.economy.gold==30000 && !s->construction.mornings_left);
    s->game.inventory.supplies[1]=580;
    assert(hmbtn_construction_order(&s->construction,&s->poultry,&s->game.inventory,&s->game.economy)==HMBTN_BUILD_OK);
    assert(s->construction.pending==HMBTN_BUILD_GREENHOUSE && s->construction.mornings_left==4
        && !s->game.economy.gold && !s->game.inventory.supplies[1]);
    assert(hmbtn_construction_order(&s->construction,&s->poultry,&s->game.inventory,&s->game.economy)==HMBTN_BUILD_BUSY);
    for(unsigned day=0;day<4;++day) {
        roundtrip(&s);assert(s->construction.pending==HMBTN_BUILD_GREENHOUSE && s->construction.mornings_left==4-day);
        s->game.weather.tomorrow=HMBTN_WEATHER_SUNNY;hmbtn_session_advance_minutes(s,1440);
        assert(s->construction.mornings_left==3-day && s->construction.greenhouse==(day==3));
        hmbtn_session_clock_events(s,&s->game.clock,HMBTN_CLOCK_DAY);assert(s->construction.mornings_left==3-day);
    }
    assert(hmbtn_construction_next(&s->construction,&s->poultry)==5 && hmbtn_farm_menu_building_score(&s->menu)==100);
    assert(hmbtn_village_flag(&s->village,0x39) && hmbtn_village_flag(&s->village,0x3e)
        && !s->village.flags[0x39] && !hmbtn_village_flag_set(&s->village,0x3e,false));
    for(unsigned season=0;season<4;++season) {
        unsigned logical,resource;
        assert(hmbtn_world_area_buildings(0,season,true,2,1,true,&logical,&resource) && logical==0 && resource==20+season);
        assert(hmbtn_world_area_buildings(124,season,true,2,1,true,&logical,&resource) && logical==124 && resource==124+season);
        assert(!hmbtn_world_area_buildings(124,season,true,2,1,false,&logical,&resource));
    }
    seasons(&s->greenhouse);
    s->area=s->intro.area=124;assert(hmbtn_session_field(s)==&s->greenhouse);
    s->greenhouse.cells[48]=(HmbtnFarmTile){.value=0x27,.ground=0x16b,.kind=3,.watered_days=1};
    s->greenhouse.plant_random=0x12345678;const uint32_t outside=s->field.plant_random;
    roundtrip(&s);assert(s->area==124 && s->greenhouse.cells[48].watered_days==1 && s->greenhouse.plant_random==0x12345678
        && s->field.plant_random==outside && s->greenhouse.indoor && !s->field.indoor);
    /* Current command 95 exposes a real greenhouse level. */
    uint8_t code[92]={0};hmbtn_write_le16(code+2,1);hmbtn_write_le16(code+4,0x4000);
    hmbtn_write_le32(code+56,32);code[60]=95;code[61]=code[62]=255;
    hmbtn_write_le16(code+64,0);hmbtn_write_le16(code+66,1);
    hmbtn_write_le32(code+68,2);hmbtn_write_le32(code+72,3);code[76]=255;char error[256]={0};
    assert(hmbtn_village_talk_open(&s->talk,24,code,sizeof(code),&s->village,&s->intro.player,
        &s->game.clock,&s->game.registration,&s->house,error,sizeof(error)));
    assert(hmbtn_village_talk_tick(&s->talk,error,sizeof(error)) && !s->talk.active && !s->talk.script.boundary);
    assert(s->talk.slots[0]==2 && s->talk.slots[1]==1 && s->talk.slots[2]==1 && s->talk.slots[3]==1);
    /* Native persistent RNG: a known survivor, then loss in an actual daily notification. */
    s->construction.damage_random=2;assert(!hmbtn_construction_storm(&s->construction));roundtrip(&s);
    s->construction.damage_random=0;s->game.clock=(HmbtnFarmClock){4,1,8,2,12,0,0};s->daily_date=hmbtn_session_date(&s->game.clock);
    s->game.weather.tomorrow=HMBTN_WEATHER_STORM;
    s->area=s->intro.area=80;hmbtn_session_advance_minutes(s,1440);
    assert(!s->construction.greenhouse && !hmbtn_village_flag(&s->village,0x39) && !hmbtn_village_flag(&s->village,0x3e));
    assert(s->greenhouse.cells[48].value==0x167 && s->greenhouse.plant_random==0x12345678);
    const uint32_t rolled=s->construction.damage_random;
    hmbtn_session_clock_events(s,&s->game.clock,HMBTN_CLOCK_DAY);assert(s->construction.damage_random==rolled);
    assert(hmbtn_construction_next(&s->construction,&s->poultry)==4 && hmbtn_farm_menu_building_score(&s->menu)==80);roundtrip(&s);
    uint8_t *data=NULL;size_t bytes=0;s->construction.greenhouse=true;s->construction.house_level=1;
    assert(!hmbtn_save_encode(s,&data,&bytes,error,sizeof(error)));
    s->construction.house_level=2;s->construction.pending=HMBTN_BUILD_GREENHOUSE;s->construction.mornings_left=1;
    assert(!hmbtn_save_encode(s,&data,&bytes,error,sizeof(error)));
    hmbtn_session_destroy(s);
    puts("Greenhouse: work/resources, four mornings/restarts, flags/score/levels, seven-cell layout, year-round shared crop engine, independent fields/RNG/save and storm loss passed.");return 0;
}

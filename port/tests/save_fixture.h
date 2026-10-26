#ifndef HMBTN_TEST_SAVE_FIXTURE_H
#define HMBTN_TEST_SAVE_FIXTURE_H
#include "save.h"
/* Test setup/assertions must also execute in Release builds. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <string.h>
/* Domain-only stable fixture: host resource loans deliberately use test data. */
static HmbtnSession *save_fixture(unsigned identity)
{
    static const uint8_t schedules[1]={0};
    HmbtnSession *s=hmbtn_session_create(NULL,0);assert(s);
    assert(hmbtn_session_bind_schedules(s,schedules,sizeof(schedules)));
    s->completion_reported=true;s->intro.script.finished=true;s->intro.finish_requested=true;s->control.enabled=true;
    s->area=s->intro.area=s->carry.area=80;s->first_visit_started=true;
    s->game.inventory.rucksack_level=2;
    s->game.inventory.equipped=(HmbtnStoredTool){34,7,42};
    s->game.inventory.held_item=(HmbtnStoredTool){0x125,2,1};
    for(unsigned i=0;i<8;++i) {
        s->game.inventory.bag[i]=(HmbtnStoredTool){(uint16_t)(i+1),1,(uint16_t)(i+1)};
        s->game.inventory.items[i]=(HmbtnStoredTool){0x125,0,1};
    }
    s->game.inventory.toolbox[63]=(HmbtnStoredTool){36,0,99};
    for(unsigned i=0;i<4;++i) s->game.inventory.supplies[i]=400+i;
    s->game.economy.gold=0x123456u+identity;s->game.economy.gold_limit=5000000;
    s->game.economy.shipping.pending_value=120;s->game.economy.shipping.fish_shipped=199;
    s->game.economy.shipping.honey_bonus=true;
    for(unsigned i=0;i<25;++i) s->game.economy.shipments[i]=i*2;
    for(unsigned i=0;i<4;++i) for(unsigned j=0;j<2;++j) s->game.economy.ledger[i][j]=i*42+j;
    s->game.clock=(HmbtnFarmClock){4,0,6,2,17,57,14};s->daily_date=hmbtn_session_date(&s->game.clock);
    s->game.registration.player_name[0]=0x100+identity;
    s->game.registration.farm_name[0]=0x101;s->game.registration.dog_name[0]=0x102;
    s->game.registration.birthday_season=3;s->game.registration.birthday_day=29;
    s->field.cells[2]=(HmbtnFarmTile){.value=34,.ground=0x16b,.kind=1,.growth_flags=0x8000,.watered_days=2};
    s->feed.coop[0]=true;s->feed.barn[21]=true;s->gather.taken[140]=true;
    s->village.flags[205]=s->village.flags[391]=true;s->village.random=0x10203040;
    s->village.days.count0[7]=119;s->village.days.count1[3]=255;s->village.days.values[13]=18;
    s->village.date_key=486;s->village.minute=1077;
    s->village.people[21].friendship=123;s->village.people[3].affection=32768;
    s->village.people[21].schedule[29][3]=54;s->village.people[21].delay=29;
    s->village.people[21].actor.position[2]=-700;s->village.people[21].actor.visible=true;
    s->village.people[21].actor.sprite=(HmbtnAnimationPlayer){1,2,3};s->village.people[21].step=4;
    const uint16_t name[9]={0x104,0x105,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff};
    assert(hmbtn_poultry_buy(&s->poultry,name,&s->game.clock));
    s->poultry.hens[0].days_sick=3;s->poultry.hens[0].affection=188;s->poultry.hens[0].fed_today=true;
    assert(!hmbtn_poultry_incubate(&s->poultry,0,0x122,&s->game.clock));
    s->poultry.random=0xfedcba98;s->poultry.eggs[0]=(HmbtnCoopEgg){0x122,{150,0,250},112};
    s->intro.player.position[0]=-90;s->intro.player.position[2]=541;s->intro.player.visible=true;
    s->intro.camera_height=-4500;s->intro.camera_distance=-9000;
    s->farm_action.stamina=50;s->farm_action.fatigue=21;s->farm_action.experience[4]=1700;s->farm_action.strikes=7;s->farm_action.target=2;
    s->carry.discarded=4;s->fishing.random=0x77665544;s->fishing.streak=2;s->fishing.catch_flags=0x10;
    s->house.steps=271;s->house.event_count1=4;s->house.options.layout=2;
    s->menu.stats.helper_affection[6]=100;s->menu.stats.power_berries=3;
    s->runtime.ticks=12345;s->runtime.house_music_ticks=100;
    hmbtn_daylight_select(&s->daylight,&s->game.clock);return s;
}
#endif

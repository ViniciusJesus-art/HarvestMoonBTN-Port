#include "village_talk.h"
#include "calendar_checks.h"
#include "inventory_checks.h"
#include "economy_checks.h"
#include "psx_data.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Calendar state line %d: %s; %s\n",__LINE__,#x,error);return 1; } } while(0)
static void date_script(uint8_t bytes[204])
{
    /* Eight real event-var reads, then terminator, in a synthetic valid block. */
    memset(bytes,0,204);hmbtn_write_le16(bytes+2,1);hmbtn_write_le16(bytes+4,0x4000);
    hmbtn_write_le32(bytes+56,9*16);
    const unsigned vars[8]={3,4,10,11,12,13,14,26};
    for(unsigned i=0;i<8;++i) {
        uint8_t *c=bytes+60+i*16;c[0]=22;c[1]=c[2]=255;
        hmbtn_write_le16(c+4,(uint16_t)vars[i]);hmbtn_write_le32(c+8,i);
    }
    bytes[60+8*16]=255;
}
int main(void)
{
    char error[256]={0};HmbtnGameState game,other;
    hmbtn_game_state_init(&game);hmbtn_game_state_init(&other);
    HmbtnFarmClock *clock=hmbtn_game_clock(&game);
    HmbtnRegistration *registration=hmbtn_game_registration(&game);
    const HmbtnFarmClock defaults={0,0,1,1,6,0,0};
    CHECK(clock==&game.clock && hmbtn_game_clock_const(&game)==clock);
    CHECK(registration==&game.registration && hmbtn_game_registration_const(&game)==registration);
    CHECK(!hmbtn_game_clock(NULL) && !hmbtn_game_clock_const(NULL)
        && !hmbtn_game_registration(NULL) && !hmbtn_game_registration_const(NULL));
    CHECK(clock_equal(clock,&defaults) && registration_equal(registration,&other.registration));
    CHECK(!registration->birthday_season && !registration->birthday_day);
    for(unsigned i=0;i<=HMBTN_NAME_CHARS;++i)
        CHECK(registration->player_name[i]==0xffff && registration->farm_name[i]==0xffff && registration->dog_name[i]==0xffff);
    const HmbtnInventory inventory=game.inventory;const HmbtnEconomy economy=game.economy;
    HmbtnHouseInteraction house,second;
    hmbtn_house_interaction_init(&house,&game);hmbtn_house_interaction_init(&second,&game);
    CHECK(house.clock==second.clock && house.clock==clock);
    CHECK(house.registration==second.registration && house.registration==registration);
    HmbtnNewGame editor,another;hmbtn_new_game_init(&editor,registration);hmbtn_new_game_init(&another,registration);
    CHECK(editor.registration==another.registration && another.registration==house.registration);
    /* Commit eight glyphs with the existing editor; the other view sees them. */
    editor.step=15;editor.length=8;editor.confirming=true;editor.message.phase=HMBTN_MESSAGE_WAIT_END;
    for(unsigned i=0;i<8;++i) editor.name[i]=(uint16_t)(0x20+i);
    hmbtn_new_game_tick(&editor,HMBTN_PAD_CROSS,0);
    CHECK(editor.step==16 && another.registration->player_name[7]==0x27 && registration->player_name[8]==0xffff);
    for(unsigned i=0;i<8;++i) registration->dog_name[i]=(uint16_t)i;
    registration->birthday_season=2;registration->birthday_day=14;
    *clock=(HmbtnFarmClock){4,2,14,3,20,59,14};
    const HmbtnRegistration registered=*registration;const HmbtnFarmClock current=*clock;
    hmbtn_new_game_init(&editor,registration);hmbtn_house_interaction_init(&house,&game);
    CHECK(clock_equal(clock,&current) && registration_equal(registration,&registered));
    hmbtn_name_editor_begin(&editor);
    CHECK(!editor.registration && editor.step==101 && registration_equal(registration,&registered));

    HmbtnVillage village;hmbtn_village_init(&village,NULL,0);HmbtnIntroActor player={0};
    HmbtnVillageTalk talk;uint8_t bytes[204];date_script(bytes);
    CHECK(hmbtn_village_talk_open(&talk,18,bytes,sizeof(bytes),&village,&player,clock,registration,&house,error,sizeof(error)));
    CHECK(talk.clock==house.clock && talk.registration==house.registration);
    CHECK(hmbtn_farm_clock_tick(clock,false)==(HMBTN_CLOCK_MINUTE|HMBTN_CLOCK_HOUR));
    CHECK(second.clock->hour==21 && !second.clock->minute);
    CHECK(hmbtn_village_talk_tick(&talk,error,sizeof(error)) && !talk.active && !talk.script.boundary);
    const int32_t expected[8]={2,14,21,3,5,2,15,0};
    for(unsigned i=0;i<8;++i) CHECK(talk.slots[i]==expected[i]);
    unsigned message,group;hmbtn_house_tv_program(&house,1,&message,&group);
    CHECK(message==0x277+2*30+14 && group==0);
    hmbtn_farm_clock_advance(clock,24*60);
    hmbtn_house_tv_program(&second,1,&message,&group);
    CHECK(second.clock->day==15 && message==0x277+2*30+15 && group==0);
    /* Calendar selection is local UI state, not a second game date. */
    uint8_t section[16+50*44]={0};uint8_t *zone=section+16;
    hmbtn_write_le16(zone,0x4000);hmbtn_write_le16(zone+2,8);
    hmbtn_write_le16(zone+14,(uint16_t)-100);hmbtn_write_le16(zone+20,100);
    hmbtn_write_le16(zone+18,(uint16_t)-300);hmbtn_write_le16(zone+24,(uint16_t)-100);
    CHECK(hmbtn_house_interact(&house,section,sizeof(section),&player) && house.calendar_day==15);
    house.message.phase=HMBTN_MESSAGE_WAIT_END;hmbtn_house_interaction_tick(&house,HMBTN_PAD_RIGHT,0);
    CHECK(house.calendar_day==16 && clock->day==15 && second.clock->day==15);
    uint8_t record[8192]={0};hmbtn_write_le16(record+0x1800,0x400c);hmbtn_write_le16(record+0x1802,0xffff);
    CHECK(hmbtn_house_expand_reading(&second,record,sizeof(record)));
    for(unsigned i=0;i<8;++i) CHECK(hmbtn_read_le16(record+0x1800+i*2)==i);
    CHECK(hmbtn_read_le16(record+0x1810)==0xffff);
    CHECK(inventory_equal(&game.inventory,&inventory) && economy_equal(&game.economy,&economy));
    CHECK(clock_equal(&other.clock,&defaults));
    hmbtn_house_interaction_init(&house,&other);
    CHECK(house.clock!=second.clock && house.registration!=second.registration && second.clock->day==15);
    CHECK(registration_equal(registration,&registered));
    puts("Shared calendar/registration, editor commits, scripts, TV, local calendar cursor and independent games passed.");return 0;
}

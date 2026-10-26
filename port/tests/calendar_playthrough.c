/* Public runtime/session APIs with real installed resources. */
#include "scene_fixture.h"
#include "sdl_host.h"
#include "calendar_checks.h"
#include "inventory_checks.h"
#include "economy_checks.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Calendar route line %d: %s; %s\n",__LINE__,#x,error);goto fail; } } while(0)
static bool step(HmbtnScenePreview *s,char *error,size_t size)
{
    return hmbtn_scene_preview_tick(s,(HmbtnLogicalInput){0,0},error,size);
}
static bool house_draw(HmbtnScenePreview *s,uint8_t *pixels,char *error,size_t size)
{
    memset(pixels,0,FRAME_BYTES);
    return hmbtn_house_ui_draw(hmbtn_scene_preview_resources(s).house_ui,hmbtn_scene_preview_resources(s).ui,&hmbtn_scene_preview_session(s)->house,hmbtn_scene_preview_resources(s).root,0,pixels,error,size);
}
static bool names_draw(HmbtnScenePreview *s,uint8_t *pixels,char *error,size_t size)
{
    memset(pixels,0,FRAME_BYTES);
    return hmbtn_farm_menu_ui_draw(hmbtn_scene_preview_resources(s).menu_ui,hmbtn_scene_preview_resources(s).house_ui,hmbtn_scene_preview_resources(s).ui,&hmbtn_scene_preview_session(s)->menu,&hmbtn_scene_preview_session(s)->house,
        &hmbtn_scene_preview_session(s)->farm_action,hmbtn_game_registration_const(&hmbtn_scene_preview_session(s)->game),hmbtn_scene_preview_resources(s).root,0,pixels,error,size);
}
int main(int argc,char **argv)
{
    if(argc!=2) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL,*other=NULL;char error[512]={0};
    uint8_t *before=NULL,*after=NULL;
    CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock start={2,3,29,6,23,40,7};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&start,error,sizeof(error));CHECK(s);
    HmbtnFarmClock *clock=hmbtn_game_clock(&hmbtn_scene_preview_session(s)->game);
    HmbtnRegistration *registration=hmbtn_game_registration(&hmbtn_scene_preview_session(s)->game);
    CHECK(hmbtn_scene_preview_session(s)->house.clock==clock && hmbtn_scene_preview_session(s)->house.registration==registration && hmbtn_scene_preview_session(s)->dialogue.registration==registration);
    CHECK(clock_equal(clock,&start));
    /* Same one-time domain handoff as app.c; source/editor can then disappear. */
    {
        HmbtnGameState title_game;hmbtn_game_state_init(&title_game);
        for(unsigned i=0;i<8;++i) {
            title_game.registration.player_name[i]=(uint16_t)(0x20+i);
            title_game.registration.farm_name[i]=(uint16_t)(0x10+i);
            title_game.registration.dog_name[i]=(uint16_t)i;
        }
        title_game.registration.birthday_season=2;title_game.registration.birthday_day=14;
        hmbtn_scene_intro_registration(s,hmbtn_game_registration_const(&title_game));
        CHECK(registration_equal(registration,&title_game.registration));
        CHECK(hmbtn_scene_preview_session(s)->dialogue.registration!=&title_game.registration);
        title_game.registration.player_name[0]=0xffff;
    }
    HmbtnRegistration expected=*registration;
    const HmbtnInventory inventory=hmbtn_scene_preview_session(s)->game.inventory;const HmbtnEconomy economy=hmbtn_scene_preview_session(s)->game.economy;
    const unsigned areas[]={80,112,128,0};
    for(unsigned i=0;i<sizeof(areas)/sizeof(*areas);++i) {
        CHECK(hmbtn_scene_preview_load_area(s,areas[i],error,sizeof(error)));hmbtn_scene_preview_session(s)->intro.area=hmbtn_scene_preview_session(s)->area;
        CHECK(clock_equal(clock,&start) && registration_equal(registration,&expected));
    }
    hmbtn_house_interaction_init(&hmbtn_scene_preview_session(s)->house,&hmbtn_scene_preview_session(s)->game);
    hmbtn_new_game_init(&hmbtn_scene_preview_session(s)->dialogue,registration);
    hmbtn_farm_menu_init(&hmbtn_scene_preview_session(s)->menu,&hmbtn_scene_preview_session(s)->game.economy);hmbtn_scene_preview_session(s)->menu.poultry=&hmbtn_scene_preview_session(s)->poultry;
    CHECK(clock_equal(clock,&start) && registration_equal(registration,&expected));
    before=malloc(FRAME_BYTES);after=malloc(FRAME_BYTES);CHECK(before && after);
    /* Actual free-exploration tick holds the central clock while a menu is open. */
    hmbtn_scene_preview_session(s)->menu.mode=HMBTN_MENU_STATUS;hmbtn_scene_preview_session(s)->menu.phase=3;hmbtn_scene_preview_session(s)->menu.page=0;
    for(unsigned i=0;i<30;++i) CHECK(step(s,error,sizeof(error)));
    CHECK(clock_equal(clock,&start));
    CHECK(names_draw(s,before,error,sizeof(error)));
    registration->farm_name[0]=0x30;expected.farm_name[0]=0x30;
    CHECK(names_draw(s,after,error,sizeof(error)) && memcmp(before,after,FRAME_BYTES));
    CHECK(registration_equal(registration,&expected) && clock_equal(clock,&start));
    hmbtn_farm_menu_init(&hmbtn_scene_preview_session(s)->menu,&hmbtn_scene_preview_session(s)->game.economy);hmbtn_scene_preview_session(s)->menu.poultry=&hmbtn_scene_preview_session(s)->poultry;
    clock->frames=14;CHECK(step(s,error,sizeof(error)));
    CHECK(clock->hour==23 && clock->minute==41 && !clock->frames && hmbtn_scene_preview_session(s)->house.clock->minute==41);

    CHECK(hmbtn_scene_preview_load_area(s,80,error,sizeof(error)));hmbtn_scene_preview_session(s)->intro.area=hmbtn_scene_preview_session(s)->area;
    CHECK(house_draw(s,before,error,sizeof(error)));
    hmbtn_farm_clock_advance_notify(clock,10,hmbtn_session_clock_events,hmbtn_scene_preview_session(s));
    const HmbtnFarmClock before_sleep=*clock;
    CHECK(house_draw(s,after,error,sizeof(error)) && memcmp(before,after,FRAME_BYTES));
    CHECK(clock_equal(clock,&before_sleep) && registration_equal(registration,&expected));
    unsigned message,group;hmbtn_house_tv_program(&hmbtn_scene_preview_session(s)->house,1,&message,&group);
    CHECK(message==0x277+3*30+29 && group==0);
    hmbtn_scene_preview_session(s)->house.mode=HMBTN_HOUSE_CALENDAR;hmbtn_scene_preview_session(s)->house.calendar_day=clock->day;
    CHECK(house_draw(s,before,error,sizeof(error)));
    CHECK(clock_equal(clock,&before_sleep));

    /* Loading remains an explicit boundary; sleeping with save does not write. */
    hmbtn_scene_preview_session(s)->house.mode=HMBTN_HOUSE_BED;hmbtn_scene_preview_session(s)->house.bed_choice=2;hmbtn_scene_preview_session(s)->house.message.phase=HMBTN_MESSAGE_WAIT_END;
    hmbtn_house_interaction_tick(&hmbtn_scene_preview_session(s)->house,HMBTN_PAD_CROSS,0);
    CHECK(hmbtn_scene_preview_session(s)->house.boundary==2 && step(s,error,sizeof(error)) && !hmbtn_scene_preview_session(s)->house.boundary);
    CHECK(clock_equal(clock,&before_sleep) && registration_equal(registration,&expected));
    hmbtn_scene_preview_session(s)->house.mode=HMBTN_HOUSE_BED;hmbtn_scene_preview_session(s)->house.bed_choice=1;hmbtn_scene_preview_session(s)->house.message.phase=HMBTN_MESSAGE_WAIT_END;
    hmbtn_house_interaction_tick(&hmbtn_scene_preview_session(s)->house,HMBTN_PAD_CROSS,0);CHECK(hmbtn_scene_preview_session(s)->house.save_requested);
    CHECK(test_morning(s,true,error,sizeof(error)));
    const HmbtnFarmClock morning_time={3,0,0,0,6,0,0};
    CHECK(clock_equal(clock,&morning_time) && hmbtn_scene_preview_session(s)->house.clock==clock && !hmbtn_scene_preview_session(s)->house.save_requested);
    CHECK(registration_equal(registration,&expected) && hmbtn_scene_preview_session(s)->dialogue.registration==registration);
    hmbtn_scene_preview_session(s)->house.mode=HMBTN_HOUSE_CALENDAR;hmbtn_scene_preview_session(s)->house.calendar_day=clock->day;
    CHECK(house_draw(s,after,error,sizeof(error)) && memcmp(before,after,FRAME_BYTES));
    CHECK(clock_equal(clock,&morning_time) && registration_equal(registration,&expected));
    hmbtn_house_tv_program(&hmbtn_scene_preview_session(s)->house,1,&message,&group);CHECK(message==0x277 && group==0);
    /* The visit handler consumes the very date shown by the house calendar. */
    hmbtn_scene_preview_session(s)->visit.active=true;
    const HmbtnEventCommand year={.type=22,.command=12,.x=0};
    const HmbtnEventCommand hour={.type=22,.command=10,.x=1};
    CHECK(test_visit_command(s,&year,error,sizeof(error))==HMBTN_EVENT_DONE && hmbtn_scene_preview_session(s)->intro.slots[0]==4);
    CHECK(test_visit_command(s,&hour,error,sizeof(error))==HMBTN_EVENT_DONE && hmbtn_scene_preview_session(s)->intro.slots[1]==6);
    hmbtn_scene_preview_session(s)->visit.active=false;
    uint8_t record[8192]={0};hmbtn_write_le16(record+0x1800,0x400a);
    hmbtn_write_le16(record+0x1802,0x400d);hmbtn_write_le16(record+0x1804,0xffff);
    CHECK(hmbtn_feed_shop_expand(record,sizeof(record),registration));
    for(unsigned i=0;i<8;++i) CHECK(hmbtn_read_le16(record+0x1800+i*2)==expected.player_name[i]
        && hmbtn_read_le16(record+0x1810+i*2)==expected.farm_name[i]);
    CHECK(inventory_equal(&hmbtn_scene_preview_session(s)->game.inventory,&inventory) && economy_equal(&hmbtn_scene_preview_session(s)->game.economy,&economy));
    other=hmbtn_scene_area_create(host.renderer,argv[1],0,error,sizeof(error));CHECK(other);
    HmbtnGameState defaults;hmbtn_game_state_init(&defaults);
    CHECK(hmbtn_scene_preview_session(other)->house.clock!=clock && hmbtn_scene_preview_session(other)->house.registration!=registration);
    CHECK(clock_equal(hmbtn_scene_preview_session(other)->house.clock,&defaults.clock) && registration_equal(hmbtn_scene_preview_session(other)->house.registration,&defaults.registration));
    CHECK(clock_equal(clock,&morning_time) && registration_equal(registration,&expected));
    free(after);free(before);hmbtn_scene_preview_destroy(other);hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);
    puts("Domain handoff, live clock/name rendering, area persistence, pause, year rollover, scripts and independent scenes passed.");return 0;
fail:
    free(after);free(before);hmbtn_scene_preview_destroy(other);hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

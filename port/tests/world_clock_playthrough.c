/* Optional original-resource integration checks, without a runtime cheat API. */
#include "scene_fixture.h"
#include "sdl_host.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"World clock line %d: %s; %s\n",__LINE__,#x,error);goto fail; } } while(0)
static bool step(HmbtnScenePreview *s,uint32_t pressed,char *error,size_t size)
{
    return hmbtn_scene_preview_tick(s,(HmbtnLogicalInput){pressed,0},error,size);
}
static bool capture(HmbtnScenePreview *s,HmbtnHost *host,const char *dir,const char *name,char *error,size_t size)
{
    if(!hmbtn_scene_preview_draw(s,host->renderer,error,size)) return false;
    if(dir) {
        char path[1024];
        if(snprintf(path,sizeof(path),"%s/%s.bmp",dir,name)>=(int)sizeof(path)) return false;
        SDL_Surface *p=SDL_RenderReadPixels(host->renderer,NULL);if(!p) return false;
        const bool saved=SDL_SaveBMP(p,path);SDL_DestroySurface(p);if(!saved) return false;
    }
    return SDL_RenderPresent(host->renderer);
}
int main(int argc,char **argv)
{
    if(argc<2 || argc>3) { fputs("Usage: world_clock_playthrough data-directory [capture-directory]\n",stderr);return 2; }
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};
    const char *shots=argc==3 ? argv[2] : NULL;
    CHECK(hmbtn_host_open(&host));s=hmbtn_scene_house_create(host.renderer,argv[1],error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<300;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->game.clock.hour==6 && !hmbtn_scene_preview_session(s)->game.clock.minute && !hmbtn_scene_preview_session(s)->game.clock.frames);
    hmbtn_scene_preview_session(s)->first_visit_started=true;
    CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)));hmbtn_scene_preview_session(s)->intro.area=0;
    hmbtn_scene_preview_session(s)->intro.player.position[0]=-4257;hmbtn_scene_preview_session(s)->intro.player.position[2]=3959;
    memcpy(hmbtn_scene_preview_session(s)->intro.camera,hmbtn_scene_preview_session(s)->intro.player.position,sizeof(hmbtn_scene_preview_session(s)->intro.camera));
    hmbtn_scene_preview_session(s)->game.clock.hour=14;hmbtn_scene_preview_session(s)->game.clock.minute=59;hmbtn_scene_preview_session(s)->game.clock.frames=14;
    hmbtn_daylight_select(&hmbtn_scene_preview_session(s)->daylight,&hmbtn_scene_preview_session(s)->game.clock);hmbtn_scene_preview_invalidate(s);
    CHECK(capture(s,&host,shots,"clock-day",error,sizeof(error)));
    /* A previous-frame obstruction fixture must not gain delay when this
     * frame starts running exactly as a minute passes. */
    hmbtn_scene_preview_session(s)->village.people[1].actor.visible=true;hmbtn_scene_preview_session(s)->village.people[1].blocked_by_player=true;
    hmbtn_scene_preview_session(s)->village.player_running=false;hmbtn_scene_preview_session(s)->control.mode=4;
    CHECK(hmbtn_scene_preview_tick(s,(HmbtnLogicalInput){0,HMBTN_PAD_CIRCLE},error,sizeof(error)) && hmbtn_scene_preview_session(s)->game.clock.hour==15 && hmbtn_scene_preview_session(s)->game.clock.minute==0);
    CHECK(hmbtn_scene_preview_session(s)->village.people[1].delay==0);
    CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)) && hmbtn_scene_preview_session(s)->menu.mode==HMBTN_MENU_STATUS);
    HmbtnFarmClock held_clock=hmbtn_scene_preview_session(s)->game.clock;HmbtnDaylight held_light=hmbtn_scene_preview_session(s)->daylight;
    for(unsigned i=0;i<40;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(memcmp(&held_clock,&hmbtn_scene_preview_session(s)->game.clock,sizeof(held_clock))==0 && memcmp(&held_light,&hmbtn_scene_preview_session(s)->daylight,sizeof(held_light))==0);
    CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)));
    for(unsigned i=0;i<300;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->menu.mode==HMBTN_MENU_NONE && hmbtn_scene_preview_session(s)->daylight.taken==256);
    hmbtn_scene_preview_invalidate(s);CHECK(capture(s,&host,shots,"clock-dusk",error,sizeof(error)));
    hmbtn_scene_preview_session(s)->game.clock.hour=17;hmbtn_scene_preview_session(s)->game.clock.minute=0;hmbtn_daylight_select(&hmbtn_scene_preview_session(s)->daylight,&hmbtn_scene_preview_session(s)->game.clock);
    CHECK(step(s,0,error,sizeof(error)) && hmbtn_scene_preview_session(s)->runtime.audio_song==28 && hmbtn_scene_preview_session(s)->intro.music_volume==50);
    hmbtn_scene_preview_invalidate(s);CHECK(capture(s,&host,shots,"clock-night",error,sizeof(error)));
    /* All four seasonal farm/house resources must decode, bind and render. */
    for(unsigned season=0;season<4;++season) {
        hmbtn_scene_preview_session(s)->game.clock.season=season;
        CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)));
        CHECK(hmbtn_scene_preview_draw(s,host.renderer,error,sizeof(error)));
        CHECK(hmbtn_scene_preview_load_area(s,80,error,sizeof(error)));
        CHECK(hmbtn_farm_clock_place_held(hmbtn_scene_preview_resources(s).section,16));
        CHECK(hmbtn_scene_preview_draw(s,host.renderer,error,sizeof(error)));
    }
    /* Actual sleep ANI, bed morph and fade must now lead to a playable morning. */
    hmbtn_scene_preview_session(s)->game.clock=(HmbtnFarmClock){0,3,29,6,23,40,0};
    hmbtn_scene_preview_session(s)->house.inventory->bag[0]=(HmbtnStoredTool){21,0,73};hmbtn_scene_preview_session(s)->carry.discarded=12;
    hmbtn_scene_preview_session(s)->farm_action.stamina=10;hmbtn_scene_preview_session(s)->farm_action.fatigue=0;
    hmbtn_scene_preview_session(s)->field.cells[400].value=hmbtn_scene_preview_session(s)->field.cells[400].ground=0x169;
    hmbtn_scene_preview_session(s)->game.economy.ledger[0][0]=456;
    hmbtn_scene_preview_session(s)->village.days.count0[0]=5;hmbtn_scene_preview_session(s)->village.days.count1[0]=10;hmbtn_scene_preview_session(s)->village.days.values[5]=42;
    for(unsigned i=0;i<HMBTN_VILLAGERS;++i) hmbtn_scene_preview_session(s)->village.people[i].talk_flags=HMBTN_VILLAGER_TALKED_TODAY;
    hmbtn_scene_preview_session(s)->village.people[18].talk_flags=0x610;hmbtn_scene_preview_session(s)->village.people[18].friendship=50;
    hmbtn_scene_preview_session(s)->village.people[3].talk_flags=0x610;hmbtn_scene_preview_session(s)->village.people[3].affection=1400;
    hmbtn_scene_preview_session(s)->house.mode=HMBTN_HOUSE_SLEEP;
    for(unsigned i=0;i<1000 && (hmbtn_scene_preview_session(s)->game.clock.year==0 || hmbtn_scene_preview_session(s)->morning_fade);++i) {
        CHECK(step(s,0,error,sizeof(error)));
        if(i%30==0) CHECK(hmbtn_scene_preview_draw(s,host.renderer,error,sizeof(error)));
    }
    CHECK(hmbtn_scene_preview_session(s)->game.clock.year==1 && hmbtn_scene_preview_session(s)->game.clock.season==0 && hmbtn_scene_preview_session(s)->game.clock.day==0 && hmbtn_scene_preview_session(s)->game.clock.weekday==0);
    CHECK(hmbtn_scene_preview_session(s)->game.clock.hour==6 && hmbtn_scene_preview_session(s)->game.clock.minute==0 && hmbtn_scene_preview_session(s)->area==80 && hmbtn_scene_preview_session(s)->house.mode==HMBTN_HOUSE_FREE && !hmbtn_scene_preview_session(s)->sleep.active);
    CHECK(hmbtn_scene_preview_session(s)->village.days.count0[0]==6 && hmbtn_scene_preview_session(s)->village.days.count1[0]==10 && hmbtn_scene_preview_session(s)->village.days.values[5]==42);
    CHECK(hmbtn_scene_preview_session(s)->village.people[18].friendship==51 && hmbtn_scene_preview_session(s)->village.people[18].talk_flags==0x10
        && hmbtn_scene_preview_session(s)->village.people[3].affection==1600 && hmbtn_scene_preview_session(s)->village.flags[0xE0]);
    CHECK(hmbtn_scene_preview_session(s)->house.inventory->bag[0].id==21 && hmbtn_scene_preview_session(s)->house.inventory->bag[0].count==73 && hmbtn_scene_preview_session(s)->carry.discarded==12);
    CHECK(hmbtn_scene_preview_session(s)->farm_action.stamina==100 && hmbtn_scene_preview_session(s)->farm_action.fatigue==0);
    CHECK(hmbtn_scene_preview_session(s)->field.cells[400].ground==0x168 && hmbtn_scene_preview_session(s)->field.cells[400].value==0x168);
    CHECK(hmbtn_scene_preview_session(s)->game.economy.ledger[0][0]==0 && hmbtn_scene_preview_session(s)->game.economy.ledger[1][0]==456);
    CHECK(hmbtn_scene_preview_session(s)->intro.player.position[0]==-90 && hmbtn_scene_preview_session(s)->intro.player.position[2]==541 && hmbtn_scene_preview_session(s)->control.enabled);
    hmbtn_scene_preview_invalidate(s);CHECK(capture(s,&host,shots,"clock-morning",error,sizeof(error)));
    /* Remaining awake until 06:00 also changes the date and returns home. */
    CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)));hmbtn_scene_preview_session(s)->intro.area=0;
    hmbtn_scene_preview_session(s)->intro.player.position[0]=-4257;hmbtn_scene_preview_session(s)->intro.player.position[1]=0;hmbtn_scene_preview_session(s)->intro.player.position[2]=3959;
    hmbtn_scene_preview_session(s)->game.clock.hour=5;hmbtn_scene_preview_session(s)->game.clock.minute=59;hmbtn_scene_preview_session(s)->game.clock.frames=14;
    for(unsigned i=0;i<HMBTN_VILLAGERS;++i) hmbtn_scene_preview_session(s)->village.people[i].talk_flags|=HMBTN_VILLAGER_TALKED_TODAY;
    hmbtn_scene_preview_session(s)->village.people[18].talk_flags|=HMBTN_VILLAGER_DAILY_GAIN;
    CHECK(step(s,0,error,sizeof(error)) && hmbtn_scene_preview_session(s)->overnight_tick==1);
    for(unsigned i=0;i<100;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->game.clock.day==1 && hmbtn_scene_preview_session(s)->game.clock.hour==6 && !hmbtn_scene_preview_session(s)->game.clock.minute && hmbtn_scene_preview_session(s)->area==80 && !hmbtn_scene_preview_session(s)->overnight_tick && !hmbtn_scene_preview_session(s)->morning_fade);
    CHECK(hmbtn_scene_preview_session(s)->village.days.count0[0]==7 && hmbtn_scene_preview_session(s)->village.days.count1[0]==11 && hmbtn_scene_preview_session(s)->village.days.values[5]==42);
    CHECK(hmbtn_scene_preview_session(s)->village.people[18].friendship==52 && hmbtn_scene_preview_session(s)->village.people[18].talk_flags==0x10
        && hmbtn_scene_preview_session(s)->village.people[3].affection==1600 && hmbtn_scene_preview_session(s)->village.flags[0xE0]);
    CHECK(hmbtn_scene_preview_session(s)->farm_action.fatigue==20); /* 06:00 hourly callback, then morning reset. */
    puts("Original-resource clock, indoor/menu pauses, sunset/night, seasonal scenes, sleep/year rollover and 06:00 handoff passed.");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

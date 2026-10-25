/* Optional original-resource integration checks, without a runtime cheat API. */
#include "scene_preview.c"
#include "sdl_host.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"World clock line %d: %s; %s\n",__LINE__,#x,error);goto fail; } } while(0)
static bool step(HmbtnScenePreview *s,uint32_t pressed,char *error,size_t size)
{
    s->pending_pressed=pressed;
    hmbtn_scene_data_tick(&s->scene);
    const bool ok=opening_tick(s,0,error,size);s->pending_pressed=0;return ok;
}
static bool capture(HmbtnScenePreview *s,HmbtnHost *host,const char *dir,const char *name,char *error,size_t size)
{
    if(!hmbtn_scene_preview_draw(s,host->renderer,0,0,0,error,size)) return false;
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
    CHECK(s->session->game.clock.hour==6 && !s->session->game.clock.minute && !s->session->game.clock.frames);
    s->first_visit_started=true;
    CHECK(load_area(s,s->data_root,0,error,sizeof(error)));s->intro.area=0;
    s->intro.player.position[0]=-4257;s->intro.player.position[2]=3959;
    memcpy(s->intro.camera,s->intro.player.position,sizeof(s->intro.camera));s->centre_x=-4257;s->centre_z=3959;
    s->session->game.clock.hour=14;s->session->game.clock.minute=59;s->session->game.clock.frames=14;
    hmbtn_daylight_select(&s->daylight,&s->session->game.clock);s->dirty=true;
    CHECK(capture(s,&host,shots,"clock-day",error,sizeof(error)));
    /* A previous-frame obstruction fixture must not gain delay when this
     * frame starts running exactly as a minute passes. */
    s->session->village.people[1].actor.visible=true;s->session->village.people[1].blocked_by_player=true;
    s->session->village.player_running=false;s->control.mode=4;
    s->pending_pressed=0;hmbtn_scene_data_tick(&s->scene);
    CHECK(opening_tick(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && s->session->game.clock.hour==15 && s->session->game.clock.minute==0);
    CHECK(s->session->village.people[1].delay==0);
    CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)) && s->menu.mode==HMBTN_MENU_STATUS);
    HmbtnFarmClock held_clock=s->session->game.clock;HmbtnDaylight held_light=s->daylight;
    for(unsigned i=0;i<40;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(memcmp(&held_clock,&s->session->game.clock,sizeof(held_clock))==0 && memcmp(&held_light,&s->daylight,sizeof(held_light))==0);
    CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)));
    for(unsigned i=0;i<300;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(s->menu.mode==HMBTN_MENU_NONE && s->daylight.taken==256);
    s->dirty=true;CHECK(capture(s,&host,shots,"clock-dusk",error,sizeof(error)));
    s->session->game.clock.hour=17;s->session->game.clock.minute=0;hmbtn_daylight_select(&s->daylight,&s->session->game.clock);
    CHECK(step(s,0,error,sizeof(error)) && s->audio_song==28 && s->intro.music_volume==50);
    s->dirty=true;CHECK(capture(s,&host,shots,"clock-night",error,sizeof(error)));
    /* All four seasonal farm/house resources must decode, bind and render. */
    for(unsigned season=0;season<4;++season) {
        s->session->game.clock.season=season;
        CHECK(load_area(s,s->data_root,0,error,sizeof(error)));
        CHECK(hmbtn_scene_preview_draw(s,host.renderer,0,0,0,error,sizeof(error)));
        CHECK(load_area(s,s->data_root,80,error,sizeof(error)));
        CHECK(hmbtn_farm_clock_place_held(s->section,16));
        CHECK(hmbtn_scene_preview_draw(s,host.renderer,0,0,0,error,sizeof(error)));
    }
    /* Actual sleep ANI, bed morph and fade must now lead to a playable morning. */
    s->session->game.clock=(HmbtnFarmClock){0,3,29,6,23,40,0};
    s->house.inventory->bag[0]=(HmbtnStoredTool){21,0,73};s->carry.discarded=12;
    s->farm_action.stamina=10;s->farm_action.fatigue=0;
    s->session->field.cells[400].value=s->session->field.cells[400].ground=0x169;
    s->session->game.economy.ledger[0][0]=456;
    s->session->village.days.count0[0]=5;s->session->village.days.count1[0]=10;s->session->village.days.values[5]=42;
    for(unsigned i=0;i<HMBTN_VILLAGERS;++i) s->session->village.people[i].talk_flags=HMBTN_VILLAGER_TALKED_TODAY;
    s->session->village.people[18].talk_flags=0x610;s->session->village.people[18].friendship=50;
    s->session->village.people[3].talk_flags=0x610;s->session->village.people[3].affection=1400;
    s->house.mode=HMBTN_HOUSE_SLEEP;
    for(unsigned i=0;i<1000 && (s->session->game.clock.year==0 || s->morning_fade);++i) {
        CHECK(step(s,0,error,sizeof(error)));
        if(i%30==0) CHECK(hmbtn_scene_preview_draw(s,host.renderer,0,0,0,error,sizeof(error)));
    }
    CHECK(s->session->game.clock.year==1 && s->session->game.clock.season==0 && s->session->game.clock.day==0 && s->session->game.clock.weekday==0);
    CHECK(s->session->game.clock.hour==6 && s->session->game.clock.minute==0 && s->area==80 && s->house.mode==HMBTN_HOUSE_FREE && !s->sleep.active);
    CHECK(s->session->village.days.count0[0]==6 && s->session->village.days.count1[0]==10 && s->session->village.days.values[5]==42);
    CHECK(s->session->village.people[18].friendship==51 && s->session->village.people[18].talk_flags==0x10
        && s->session->village.people[3].affection==1600 && s->session->village.flags[0xE0]);
    CHECK(s->house.inventory->bag[0].id==21 && s->house.inventory->bag[0].count==73 && s->carry.discarded==12);
    CHECK(s->farm_action.stamina==100 && s->farm_action.fatigue==0);
    CHECK(s->session->field.cells[400].ground==0x168 && s->session->field.cells[400].value==0x168);
    CHECK(s->session->game.economy.ledger[0][0]==0 && s->session->game.economy.ledger[1][0]==456);
    CHECK(s->intro.player.position[0]==-90 && s->intro.player.position[2]==541 && s->control.enabled);
    s->dirty=true;CHECK(capture(s,&host,shots,"clock-morning",error,sizeof(error)));
    /* Remaining awake until 06:00 also changes the date and returns home. */
    CHECK(load_area(s,s->data_root,0,error,sizeof(error)));s->intro.area=0;
    s->intro.player.position[0]=-4257;s->intro.player.position[1]=0;s->intro.player.position[2]=3959;
    s->session->game.clock.hour=5;s->session->game.clock.minute=59;s->session->game.clock.frames=14;
    for(unsigned i=0;i<HMBTN_VILLAGERS;++i) s->session->village.people[i].talk_flags|=HMBTN_VILLAGER_TALKED_TODAY;
    s->session->village.people[18].talk_flags|=HMBTN_VILLAGER_DAILY_GAIN;
    CHECK(step(s,0,error,sizeof(error)) && s->overnight_tick==1);
    for(unsigned i=0;i<100;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(s->session->game.clock.day==1 && s->session->game.clock.hour==6 && !s->session->game.clock.minute && s->area==80 && !s->overnight_tick && !s->morning_fade);
    CHECK(s->session->village.days.count0[0]==7 && s->session->village.days.count1[0]==11 && s->session->village.days.values[5]==42);
    CHECK(s->session->village.people[18].friendship==52 && s->session->village.people[18].talk_flags==0x10
        && s->session->village.people[3].affection==1600 && s->session->village.flags[0xE0]);
    CHECK(s->farm_action.fatigue==20); /* 06:00 hourly callback, then morning reset. */
    puts("Original-resource clock, indoor/menu pauses, sunset/night, seasonal scenes, sleep/year rollover and 06:00 handoff passed.");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

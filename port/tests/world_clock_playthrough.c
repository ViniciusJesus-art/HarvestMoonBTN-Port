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
    CHECK(s->clock.hour==6 && !s->clock.minute && !s->clock.frames);
    s->first_visit_started=true;
    CHECK(load_area(s,s->data_root,0,error,sizeof(error)));s->intro.area=0;
    s->intro.player.position[0]=-4257;s->intro.player.position[2]=3959;
    memcpy(s->intro.camera,s->intro.player.position,sizeof(s->intro.camera));s->centre_x=-4257;s->centre_z=3959;
    s->clock.hour=14;s->clock.minute=59;s->clock.frames=14;sync_clock(s);
    hmbtn_daylight_select(&s->daylight,&s->clock);s->dirty=true;
    CHECK(capture(s,&host,shots,"clock-day",error,sizeof(error)));
    /* A previous-frame obstruction fixture must not gain delay when this
     * frame starts running exactly as a minute passes. */
    s->village.people[1].actor.visible=true;s->village.people[1].blocked_by_player=true;
    s->village.player_running=false;s->control.mode=4;
    s->pending_pressed=0;hmbtn_scene_data_tick(&s->scene);
    CHECK(opening_tick(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && s->clock.hour==15 && s->clock.minute==0);
    CHECK(s->village.people[1].delay==0);
    CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)) && s->menu.mode==HMBTN_MENU_STATUS);
    HmbtnFarmClock held_clock=s->clock;HmbtnDaylight held_light=s->daylight;
    for(unsigned i=0;i<40;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(memcmp(&held_clock,&s->clock,sizeof(held_clock))==0 && memcmp(&held_light,&s->daylight,sizeof(held_light))==0);
    CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)));
    for(unsigned i=0;i<300;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(s->menu.mode==HMBTN_MENU_NONE && s->daylight.taken==256);
    s->dirty=true;CHECK(capture(s,&host,shots,"clock-dusk",error,sizeof(error)));
    s->clock.hour=17;s->clock.minute=0;hmbtn_daylight_select(&s->daylight,&s->clock);
    CHECK(step(s,0,error,sizeof(error)) && s->audio_song==28 && s->intro.music_volume==50);
    s->dirty=true;CHECK(capture(s,&host,shots,"clock-night",error,sizeof(error)));
    /* All four seasonal farm/house resources must decode, bind and render. */
    for(unsigned season=0;season<4;++season) {
        s->clock.season=season;
        CHECK(load_area(s,s->data_root,0,error,sizeof(error)));
        CHECK(hmbtn_scene_preview_draw(s,host.renderer,0,0,0,error,sizeof(error)));
        CHECK(load_area(s,s->data_root,80,error,sizeof(error)));
        CHECK(hmbtn_farm_clock_place_held(s->section,16));
        CHECK(hmbtn_scene_preview_draw(s,host.renderer,0,0,0,error,sizeof(error)));
    }
    /* Actual sleep ANI, bed morph and fade must now lead to a playable morning. */
    s->clock=(HmbtnFarmClock){0,3,29,6,23,40,0};sync_clock(s);
    s->house.bag[0]=(HmbtnStoredTool){21,0,73};s->carry.discarded=12;
    s->farm_action.stamina=10;s->farm_action.fatigue=0;
    s->field.cells[400].value=s->field.cells[400].ground=0x169;
    s->menu.stats.ledger[0][0]=456;
    s->village.days.count0[0]=5;s->village.days.count1[0]=10;s->village.days.values[5]=42;
    s->house.mode=HMBTN_HOUSE_SLEEP;
    for(unsigned i=0;i<1000 && (s->clock.year==0 || s->morning_fade);++i) {
        CHECK(step(s,0,error,sizeof(error)));
        if(i%30==0) CHECK(hmbtn_scene_preview_draw(s,host.renderer,0,0,0,error,sizeof(error)));
    }
    CHECK(s->clock.year==1 && s->clock.season==0 && s->clock.day==0 && s->clock.weekday==0);
    CHECK(s->clock.hour==6 && s->clock.minute==0 && s->area==80 && s->house.mode==HMBTN_HOUSE_FREE && !s->sleep.active);
    CHECK(s->village.days.count0[0]==6 && s->village.days.count1[0]==10 && s->village.days.values[5]==42);
    CHECK(s->house.bag[0].id==21 && s->house.bag[0].count==73 && s->carry.discarded==12);
    CHECK(s->farm_action.stamina==100 && s->farm_action.fatigue==0);
    CHECK(s->field.cells[400].ground==0x168 && s->field.cells[400].value==0x168);
    CHECK(s->menu.stats.ledger[0][0]==0 && s->menu.stats.ledger[1][0]==456);
    CHECK(s->intro.player.position[0]==-90 && s->intro.player.position[2]==541 && s->control.enabled);
    s->dirty=true;CHECK(capture(s,&host,shots,"clock-morning",error,sizeof(error)));
    /* Remaining awake until 06:00 also changes the date and returns home. */
    CHECK(load_area(s,s->data_root,0,error,sizeof(error)));s->intro.area=0;
    s->intro.player.position[0]=-4257;s->intro.player.position[1]=0;s->intro.player.position[2]=3959;
    s->clock.hour=5;s->clock.minute=59;s->clock.frames=14;
    CHECK(step(s,0,error,sizeof(error)) && s->overnight_tick==1);
    for(unsigned i=0;i<100;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(s->clock.day==1 && s->clock.hour==6 && !s->clock.minute && s->area==80 && !s->overnight_tick && !s->morning_fade);
    CHECK(s->village.days.count0[0]==7 && s->village.days.count1[0]==11 && s->village.days.values[5]==42);
    CHECK(s->farm_action.fatigue==20); /* 06:00 hourly callback, then morning reset. */
    puts("Original-resource clock, indoor/menu pauses, sunset/night, seasonal scenes, sleep/year rollover and 06:00 handoff passed.");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

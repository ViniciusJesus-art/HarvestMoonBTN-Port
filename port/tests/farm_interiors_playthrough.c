/* Original-resource checks for the three initial farm buildings. */
#include "scene_preview.c"
#include "sdl_host.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Farm interiors line %d: %s; %s\n",__LINE__,#x,error);goto fail; } } while(0)
static int32_t half(const uint8_t *p)
{
    const unsigned v=hmbtn_read_le16(p);return v<32768 ? (int32_t)v : (int32_t)v-65536;
}
static bool step(HmbtnScenePreview *s,uint32_t pressed,char *error,size_t size)
{
    hmbtn_scene_data_tick(&s->scene);s->pending_pressed=pressed;
    const bool ok=opening_tick(s,0,error,size);s->pending_pressed=0;return ok;
}
int main(int argc,char **argv)
{
    if(argc!=2) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};
    CHECK(hmbtn_host_open(&host));
    s=hmbtn_scene_area_create(host.renderer,argv[1],0,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    s->house.bag[0]=(HmbtnStoredTool){21,0,73};s->shipping.pending_value=60;
    const unsigned buildings[]={92,96,112};
    for(unsigned season=0;season<4;++season) for(unsigned b=0;b<3;++b) {
        s->clock=(HmbtnFarmClock){0,season,1,1,12,0,0};sync_clock(s);
        CHECK(load_area(s,s->data_root,0,error,sizeof(error)));s->intro.area=0;
        const uint8_t *door=NULL;
        for(unsigned i=0;i<50;++i) {
            const uint8_t *p=s->section+16+i*44;
            if((hmbtn_read_le16(p+2)&~3u)==buildings[b]) { door=p;break; }
        }
        CHECK(door);
        for(unsigned a=0;a<3;++a) s->intro.player.position[a]=(half(door+14+a*2)+half(door+20+a*2))/2;
        s->intro.player.facing=hmbtn_read_le16(door+8);s->control.mode=0;
        int32_t start[3];memcpy(start,s->intro.player.position,sizeof(start));
        CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && s->door_tick);
        for(unsigned i=0;i<16;++i) {
            CHECK(step(s,0,error,sizeof(error)));
            CHECK(memcmp(start,s->intro.player.position,sizeof(start))==0);
        }
        for(unsigned i=0;i<100 && (s->area==0 || s->travel_tick);++i) CHECK(step(s,0,error,sizeof(error)));
        CHECK(s->area==buildings[b] && !s->control.exit_pending && !s->door_tick && s->control.enabled);
        CHECK(hmbtn_farm_clock_place_held(s->section,16));
        HmbtnFarmClock clock=s->clock;
        for(unsigned i=0;i<30;++i) CHECK(step(s,0,error,sizeof(error)));
        CHECK(memcmp(&clock,&s->clock,sizeof(clock))==0);
        CHECK(hmbtn_scene_preview_draw(s,host.renderer,0,0,0,error,sizeof(error)));
        const uint8_t *exit=s->section+16;
        for(unsigned a=0;a<3;++a) s->intro.player.position[a]=(half(exit+14+a*2)+half(exit+20+a*2))/2;
        CHECK(step(s,0,error,sizeof(error)) && s->travel_tick);
        for(unsigned i=0;i<30 && s->travel_tick;++i) CHECK(step(s,0,error,sizeof(error)));
        CHECK(s->area==0 && !s->control.exit_pending && !s->first_visit_active);
        CHECK(s->house.bag[0].id==21 && s->house.bag[0].count==73 && s->shipping.pending_value==60);
        printf("Farm building %u season %u: door, terrain, render, paused clock and return passed.\n",buildings[b],season);
    }
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

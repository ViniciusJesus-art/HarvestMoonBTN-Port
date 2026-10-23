/* Optional original-resource test; private state changes are fixtures, never runtime shortcuts. */
#include "scene_preview.c"
#include "sdl_host.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Village interiors line %d area %u season %u: %s; %s\n",__LINE__,area,season,#x,error);goto fail; } } while(0)
static bool step(HmbtnScenePreview *s,uint32_t pressed,char *error,size_t size)
{
    hmbtn_scene_data_tick(&s->scene);s->pending_pressed=pressed;
    const bool ok=opening_tick(s,0,error,size);s->pending_pressed=0;return ok;
}
int main(int argc,char **argv)
{
    if(argc!=2) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};unsigned area=0,season=0;
    CHECK(hmbtn_host_open(&host));
    s=hmbtn_scene_area_create(host.renderer,argv[1],0,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    for(season=0;season<4;++season) for(area=128;area<216;area+=4) {
        HmbtnScenePreview *entry=hmbtn_scene_area_create(host.renderer,argv[1],area,error,sizeof(error));
        CHECK(entry);hmbtn_scene_preview_destroy(entry);
        s->clock=(HmbtnFarmClock){0,season,1,1,12,0,0};sync_clock(s);
        CHECK(load_area(s,s->data_root,area,error,sizeof(error)));s->intro.area=area;
        for(unsigned i=0;i<2;++i) CHECK(step(s,0,error,sizeof(error)));
        CHECK(hmbtn_scene_preview_draw(s,host.renderer,0,0,0,error,sizeof(error)));
        CHECK(hmbtn_farm_clock_place_held(s->section,16));
        const HmbtnFarmClock before=s->clock;
        for(unsigned i=0;i<15;++i) CHECK(step(s,0,error,sizeof(error)));
        CHECK(memcmp(&before,&s->clock,sizeof(before))==0);
        const uint8_t *exit=NULL;
        for(unsigned i=0;i<50;++i) {
            const uint8_t *z=s->section+16+i*44;
            HmbtnHouseExit e;int32_t point[3];
            for(unsigned a=0;a<3;++a) {
                unsigned lo=hmbtn_read_le16(z+14+a*2),hi=hmbtn_read_le16(z+20+a*2);
                point[a]=((lo<32768 ? (int)lo : (int)lo-65536)+(hi<32768 ? (int)hi : (int)hi-65536))/2;
            }
            if((hmbtn_house_exit_at(s->section,0x2000,point,&e)
                    || hmbtn_world_gate_at(s->section,0x2000,point,&e))
                    && hmbtn_village_door(&s->village,hmbtn_read_le16(z+10),&s->clock)<0) { exit=z;break; }
        }
        CHECK(exit);
        for(unsigned a=0;a<3;++a) {
            unsigned lo=hmbtn_read_le16(exit+14+a*2),hi=hmbtn_read_le16(exit+20+a*2);
            s->intro.player.position[a]=((lo<32768 ? (int)lo : (int)lo-65536)+(hi<32768 ? (int)hi : (int)hi-65536))/2;
        }
        CHECK(step(s,0,error,sizeof(error)) && s->travel_tick);
        for(unsigned i=0;i<30 && s->travel_tick;++i) CHECK(step(s,0,error,sizeof(error)));
        CHECK(s->area!=area && !s->control.exit_pending);
        printf("Village interior %u season %u: resources, residents, clock and return passed.\n",area,season);
    }
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

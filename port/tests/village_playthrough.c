/* Optional original-resource test; session state changes are fixtures, never runtime shortcuts. */
#include "scene_fixture.h"
#include "sdl_host.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Village interiors line %d area %u season %u: %s; %s\n",__LINE__,area,season,#x,error);goto fail; } } while(0)
static bool step(HmbtnScenePreview *s,uint32_t pressed,char *error,size_t size)
{
    return hmbtn_scene_preview_tick(s,(HmbtnLogicalInput){pressed,0},error,size);
}
static void at_zone(HmbtnScenePreview *s,const uint8_t *zone)
{
    for(unsigned a=0;a<3;++a) {
        unsigned lo=hmbtn_read_le16(zone+14+a*2),hi=hmbtn_read_le16(zone+20+a*2);
        hmbtn_scene_preview_session(s)->intro.player.position[a]=((lo<32768 ? (int)lo : (int)lo-65536)+(hi<32768 ? (int)hi : (int)hi-65536))/2;
    }
    hmbtn_scene_preview_session(s)->intro.player.facing=hmbtn_read_le16(zone+8)&3u;hmbtn_scene_preview_session(s)->control.mode=0;
}
int main(int argc,char **argv)
{
    if(argc!=2) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};unsigned area=0,season=0;
    CHECK(hmbtn_host_open(&host));
    s=hmbtn_scene_area_create(host.renderer,argv[1],0,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    for(unsigned pack=0;pack<35;++pack) {
        HmbtnSessionFrame f=hmbtn_scene_preview_frame(s);
        CHECK(f.exploration.village_assets(f.exploration.context,pack,&hmbtn_scene_preview_session(s)->game.clock,error,sizeof(error)));
    }
    /* A real exterior door: refusal, complete close, then entry at opening time. */
    area=28;CHECK(hmbtn_scene_preview_load_area(s,area,error,sizeof(error)));hmbtn_scene_preview_session(s)->intro.area=area;
    hmbtn_scene_preview_session(s)->game.clock=(HmbtnFarmClock){0,0,1,1,6,0,0};
    CHECK(hmbtn_village_sync(&hmbtn_scene_preview_session(s)->village,&hmbtn_scene_preview_session(s)->game.clock,error,sizeof(error)));
    at_zone(s,hmbtn_scene_preview_resources(s).section+16);
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(!hmbtn_scene_preview_session(s)->door_tick && !hmbtn_scene_preview_session(s)->travel_tick && hmbtn_scene_preview_session(s)->house.loaded==0x1CD && hmbtn_scene_preview_session(s)->house.mode==HMBTN_HOUSE_BOOK);
    for(unsigned i=0;i<700 && hmbtn_scene_preview_session(s)->house.mode!=HMBTN_HOUSE_FREE;++i) CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->house.mode==HMBTN_HOUSE_FREE);
    /* Barley is away on Monday; Tuesday's real schedule opens the ranch. */
    hmbtn_scene_preview_session(s)->game.clock.day=2;hmbtn_scene_preview_session(s)->game.clock.weekday=2;hmbtn_scene_preview_session(s)->game.clock.hour=10;hmbtn_scene_preview_session(s)->game.clock.minute=hmbtn_scene_preview_session(s)->game.clock.frames=0;
    CHECK(hmbtn_village_sync(&hmbtn_scene_preview_session(s)->village,&hmbtn_scene_preview_session(s)->game.clock,error,sizeof(error)));
    CHECK(hmbtn_village_door(&hmbtn_scene_preview_session(s)->village,8,&hmbtn_scene_preview_session(s)->game.clock)==-1);
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && hmbtn_scene_preview_session(s)->door_tick);
    for(unsigned i=0;i<100 && (hmbtn_scene_preview_session(s)->area==28 || hmbtn_scene_preview_session(s)->travel_tick);++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->area==136 && !hmbtn_scene_preview_session(s)->control.exit_pending);
    for(season=0;season<4;++season) for(area=128;area<216;area+=4) {
        const HmbtnFarmClock preview_clock={0,season,1,1,12,0,0};
        HmbtnScenePreview *entry=hmbtn_scene_area_create_at(host.renderer,argv[1],area,&preview_clock,error,sizeof(error));
        CHECK(entry && hmbtn_scene_preview_session(entry)->game.clock.season==season && hmbtn_scene_preview_session(entry)->game.clock.hour==12);
        CHECK(entry);hmbtn_scene_preview_destroy(entry);
        hmbtn_scene_preview_session(s)->game.clock=(HmbtnFarmClock){0,season,1,1,12,0,0};
        CHECK(hmbtn_scene_preview_load_area(s,area,error,sizeof(error)));hmbtn_scene_preview_session(s)->intro.area=area;
        for(unsigned i=0;i<2;++i) CHECK(step(s,0,error,sizeof(error)));
        CHECK(hmbtn_scene_preview_draw(s,host.renderer,error,sizeof(error)));
        CHECK(hmbtn_farm_clock_place_held(hmbtn_scene_preview_resources(s).section,16));
        const HmbtnFarmClock before=hmbtn_scene_preview_session(s)->game.clock;
        for(unsigned i=0;i<15;++i) CHECK(step(s,0,error,sizeof(error)));
        CHECK(memcmp(&before,&hmbtn_scene_preview_session(s)->game.clock,sizeof(before))==0);
        const uint8_t *exit=NULL;
        for(unsigned i=0;i<50;++i) {
            const uint8_t *z=hmbtn_scene_preview_resources(s).section+16+i*44;
            HmbtnHouseExit e;int32_t point[3];
            for(unsigned a=0;a<3;++a) {
                unsigned lo=hmbtn_read_le16(z+14+a*2),hi=hmbtn_read_le16(z+20+a*2);
                point[a]=((lo<32768 ? (int)lo : (int)lo-65536)+(hi<32768 ? (int)hi : (int)hi-65536))/2;
            }
            if((hmbtn_house_exit_at(hmbtn_scene_preview_resources(s).section,0x2000,point,&e)
                    || hmbtn_world_gate_at(hmbtn_scene_preview_resources(s).section,0x2000,point,&e))
                    && hmbtn_village_door(&hmbtn_scene_preview_session(s)->village,hmbtn_read_le16(z+10),&hmbtn_scene_preview_session(s)->game.clock)<0) { exit=z;break; }
        }
        CHECK(exit);
        for(unsigned a=0;a<3;++a) {
            unsigned lo=hmbtn_read_le16(exit+14+a*2),hi=hmbtn_read_le16(exit+20+a*2);
            hmbtn_scene_preview_session(s)->intro.player.position[a]=((lo<32768 ? (int)lo : (int)lo-65536)+(hi<32768 ? (int)hi : (int)hi-65536))/2;
        }
        CHECK(step(s,0,error,sizeof(error)) && hmbtn_scene_preview_session(s)->travel_tick);
        for(unsigned i=0;i<30 && hmbtn_scene_preview_session(s)->travel_tick;++i) CHECK(step(s,0,error,sizeof(error)));
        CHECK(hmbtn_scene_preview_session(s)->area!=area && !hmbtn_scene_preview_session(s)->control.exit_pending);
        printf("Village interior %u season %u: resources, residents, clock and return passed.\n",area,season);
    }
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

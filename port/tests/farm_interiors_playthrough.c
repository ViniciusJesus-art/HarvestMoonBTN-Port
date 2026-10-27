/* Original-resource checks for the three initial farm buildings. */
#include "scene_fixture.h"
#include "sdl_host.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Farm interiors line %d: %s; %s\n",__LINE__,#x,error);goto fail; } } while(0)
static int32_t half(const uint8_t *p)
{
    const unsigned v=hmbtn_read_le16(p);return v<32768 ? (int32_t)v : (int32_t)v-65536;
}
static bool step(HmbtnScenePreview *s,uint32_t pressed,char *error,size_t size)
{
    return hmbtn_scene_preview_tick(s,(HmbtnLogicalInput){pressed,0},error,size);
}
int main(int argc,char **argv)
{
    if(argc!=2) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};
    CHECK(hmbtn_host_open(&host));
    s=hmbtn_scene_area_create(host.renderer,argv[1],0,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    hmbtn_scene_preview_session(s)->house.inventory->bag[0]=(HmbtnStoredTool){21,0,73};hmbtn_scene_preview_session(s)->game.economy.shipping.pending_value=60;
    const unsigned buildings[]={92,96,112};
    for(unsigned season=0;season<4;++season) for(unsigned b=0;b<3;++b) {
        hmbtn_scene_preview_session(s)->game.clock=(HmbtnFarmClock){0,season,1,1,12,0,0};
        CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)));hmbtn_scene_preview_session(s)->intro.area=0;
        const uint8_t *door=NULL;
        for(unsigned i=0;i<50;++i) {
            const uint8_t *p=hmbtn_scene_preview_resources(s).section+16+i*44;
            if((hmbtn_read_le16(p+2)&~3u)==buildings[b]) { door=p;break; }
        }
        CHECK(door);
        for(unsigned a=0;a<3;++a) hmbtn_scene_preview_session(s)->intro.player.position[a]=(half(door+14+a*2)+half(door+20+a*2))/2;
        hmbtn_scene_preview_session(s)->intro.player.facing=hmbtn_read_le16(door+8);hmbtn_scene_preview_session(s)->control.mode=0;
        int32_t start[3];memcpy(start,hmbtn_scene_preview_session(s)->intro.player.position,sizeof(start));
        CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && hmbtn_scene_preview_session(s)->door_tick);
        /* The native player retreats while opening (user's runtime change).
         * Validate that motion instead of requiring the old stationary pose. */
        const unsigned facing=hmbtn_scene_preview_session(s)->intro.player.facing,axis=facing&1u ? 0 : 2;
        int acceleration=0; /* Preserve the user's tuned door speed; validate its curve. */
        for(unsigned i=0;i<16;++i) {
            CHECK(step(s,0,error,sizeof(error)));
            const int32_t *position=hmbtn_scene_preview_session(s)->intro.player.position;
            CHECK(position[axis]==start[axis]+(facing<2 ? 30 : -30)*(int32_t)(i+1));
            CHECK(position[1]==start[1] && position[2-axis]==start[2-axis]);
            if(!i) acceleration=hmbtn_scene_preview_session(s)->door_speed;
            CHECK(acceleration>0 && hmbtn_scene_preview_session(s)->door_speed==(int)(i+1)*acceleration);
        }
        for(unsigned i=0;i<100 && (hmbtn_scene_preview_session(s)->area==0 || hmbtn_scene_preview_session(s)->travel_tick);++i) CHECK(step(s,0,error,sizeof(error)));
        CHECK(hmbtn_scene_preview_session(s)->area==buildings[b] && !hmbtn_scene_preview_session(s)->control.exit_pending && !hmbtn_scene_preview_session(s)->door_tick && hmbtn_scene_preview_session(s)->control.enabled);
        CHECK(hmbtn_farm_clock_place_held(hmbtn_scene_preview_resources(s).section,16));
        HmbtnFarmClock clock=hmbtn_scene_preview_session(s)->game.clock;
        for(unsigned i=0;i<30;++i) CHECK(step(s,0,error,sizeof(error)));
        CHECK(memcmp(&clock,&hmbtn_scene_preview_session(s)->game.clock,sizeof(clock))==0);
        CHECK(hmbtn_scene_preview_draw(s,host.renderer,error,sizeof(error)));
        const uint8_t *exit=hmbtn_scene_preview_resources(s).section+16;
        for(unsigned a=0;a<3;++a) hmbtn_scene_preview_session(s)->intro.player.position[a]=(half(exit+14+a*2)+half(exit+20+a*2))/2;
        CHECK(step(s,0,error,sizeof(error)) && hmbtn_scene_preview_session(s)->travel_tick);
        for(unsigned i=0;i<30 && hmbtn_scene_preview_session(s)->travel_tick;++i) CHECK(step(s,0,error,sizeof(error)));
        CHECK(hmbtn_scene_preview_session(s)->area==0 && !hmbtn_scene_preview_session(s)->control.exit_pending && !hmbtn_scene_preview_session(s)->first_visit_active);
        CHECK(hmbtn_scene_preview_session(s)->house.inventory->bag[0].id==21 && hmbtn_scene_preview_session(s)->house.inventory->bag[0].count==73 && hmbtn_scene_preview_session(s)->game.economy.shipping.pending_value==60);
        printf("Farm building %u season %u: door, terrain, render, paused clock and return passed.\n",buildings[b],season);
    }
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

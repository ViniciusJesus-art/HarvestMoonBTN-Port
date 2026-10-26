/* Optional test: original zones, ANI frames, floor feed art and daily reset. */
#include "scene_fixture.h"
#include "sdl_host.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Feed integration line %d: %s; %s\n",__LINE__,#x,error);goto fail; } } while(0)
static bool step(HmbtnScenePreview *s,uint32_t pressed,char *error,size_t size)
{
    return hmbtn_scene_preview_tick(s,(HmbtnLogicalInput){pressed,0},error,size);
}
static int half(const uint8_t *p)
{
    unsigned n=hmbtn_read_le16(p);return n<32768 ? (int)n : (int)n-65536;
}
static bool point(HmbtnScenePreview *s,unsigned kind,unsigned bin)
{
    for(unsigned i=0;i<50;++i) {
        const uint8_t *p=hmbtn_scene_preview_resources(s).section+16+44*i;
        if(hmbtn_read_le16(p)!=0x4000 || hmbtn_read_le16(p+2)!=kind || hmbtn_read_le16(p+4)!=bin) continue;
        unsigned facing=hmbtn_read_le16(p+8);
        if(facing==999 || facing==6) facing=3;
        if(facing==7) facing=2;
        static const int delta[4][2]={{0,-224},{-224,0},{0,224},{224,0}};
        for(unsigned a=0;a<3;++a) hmbtn_scene_preview_session(s)->intro.player.position[a]=(half(p+14+2*a)+half(p+20+2*a))/2;
        hmbtn_scene_preview_session(s)->intro.player.position[0]-=delta[facing][0];hmbtn_scene_preview_session(s)->intro.player.position[2]-=delta[facing][1];
        hmbtn_scene_preview_session(s)->intro.player.facing=facing;hmbtn_scene_preview_session(s)->control.mode=0;return true;
    }
    return false;
}
int main(int argc,char **argv)
{
    if(argc<2 || argc>3) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};
    CHECK(hmbtn_host_open(&host));s=hmbtn_scene_area_create(host.renderer,argv[1],96,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    for(unsigned b=0;b<2;++b) {
        unsigned area=b ? 112 : 96,stock=b ? 2 : 0,item=b ? 0x69 : 0x68;
        CHECK(hmbtn_scene_preview_load_area(s,area,error,sizeof(error)));hmbtn_scene_preview_session(s)->intro.area=area;
        hmbtn_scene_preview_session(s)->house.inventory->supplies[stock]=2;CHECK(point(s,b ? 26 : 9,0));
        CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && hmbtn_scene_preview_session(s)->feed.phase==1);
        CHECK(hmbtn_scene_preview_session(s)->house.inventory->held_item.id==item && hmbtn_scene_preview_session(s)->house.inventory->supplies[stock]==1);
        CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)) && hmbtn_scene_preview_session(s)->menu.mode==HMBTN_MENU_NONE);
        for(unsigned i=0;i<90 && hmbtn_scene_preview_session(s)->feed.phase;++i) CHECK(step(s,0,error,sizeof(error)));
        CHECK(!hmbtn_scene_preview_session(s)->feed.phase && hmbtn_scene_preview_session(s)->house.inventory->held_item.id==item);
        CHECK(point(s,b ? 40 : 41,0));
        CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && hmbtn_scene_preview_session(s)->feed.phase==2);
        for(unsigned i=0;i<90 && hmbtn_scene_preview_session(s)->feed.phase;++i) CHECK(step(s,0,error,sizeof(error)));
        CHECK(!hmbtn_scene_preview_session(s)->feed.phase && !hmbtn_scene_preview_session(s)->house.inventory->held_item.id && (b ? hmbtn_scene_preview_session(s)->feed.coop[0] : hmbtn_scene_preview_session(s)->feed.barn[0]));
        CHECK(step(s,0,error,sizeof(error)));
        CHECK(hmbtn_scene_preview_draw(s,host.renderer,error,sizeof(error)));
        if(argc==3) {
            char path[1024];CHECK(snprintf(path,sizeof(path),"%s/feed-%u.bmp",argv[2],area)<(int)sizeof(path));
            SDL_Surface *pixels=SDL_RenderReadPixels(host.renderer,NULL);CHECK(pixels);
            const bool saved=SDL_SaveBMP(pixels,path);SDL_DestroySurface(pixels);CHECK(saved);
        }
        CHECK(point(s,b ? 26 : 9,0));CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
        for(unsigned i=0;i<90 && hmbtn_scene_preview_session(s)->feed.phase;++i) CHECK(step(s,0,error,sizeof(error)));
        CHECK(hmbtn_scene_preview_session(s)->house.inventory->supplies[stock]==0);CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
        for(unsigned i=0;i<90 && hmbtn_scene_preview_session(s)->feed.phase;++i) CHECK(step(s,0,error,sizeof(error)));
        CHECK(hmbtn_scene_preview_session(s)->house.inventory->supplies[stock]==1 && !hmbtn_scene_preview_session(s)->house.inventory->held_item.id);
        CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
        for(unsigned i=0;i<90 && hmbtn_scene_preview_session(s)->feed.phase;++i) CHECK(step(s,0,error,sizeof(error)));
        CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
        for(unsigned i=0;i<90 && hmbtn_scene_preview_session(s)->feed.phase;++i) CHECK(step(s,0,error,sizeof(error)));
        CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
        for(unsigned i=0;i<90 && hmbtn_scene_preview_session(s)->feed.phase;++i) CHECK(step(s,0,error,sizeof(error)));
        /* Put the last feed away, then exercise the actual empty-stock dialogue. */
        CHECK(point(s,b ? 40 : 41,1));CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
        for(unsigned i=0;i<90 && hmbtn_scene_preview_session(s)->feed.phase;++i) CHECK(step(s,0,error,sizeof(error)));
        CHECK(point(s,b ? 26 : 9,0));CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
        CHECK(hmbtn_scene_preview_session(s)->house.mode==HMBTN_HOUSE_BOOK && hmbtn_scene_preview_session(s)->house.loaded==(b ? 0x19Fu : 0x19Cu));
        hmbtn_scene_preview_session(s)->house.mode=HMBTN_HOUSE_FREE;
        printf("Feed area %u: withdraw, original animations, bin rendering, return and empty message passed.\n",area);
    }
    CHECK(hmbtn_scene_preview_session(s)->feed.barn[0] && hmbtn_scene_preview_session(s)->feed.coop[0]);
    hmbtn_farm_clock_advance_notify(&hmbtn_scene_preview_session(s)->game.clock,1440,hmbtn_session_clock_events,hmbtn_scene_preview_session(s));
    CHECK(!hmbtn_scene_preview_session(s)->feed.barn[0] && !hmbtn_scene_preview_session(s)->feed.coop[0]);
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

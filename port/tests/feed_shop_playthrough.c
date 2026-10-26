/* Real-resource purchase -> return across maps -> stock -> bin.
 * Fixtures select a test date and position near original passages/actors;
 * no money, items or feed are granted. */
#include "scene_fixture.h"
#include "sdl_host.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Feed shop integration line %d area %u: %s; %s\n",__LINE__,s ? hmbtn_scene_preview_session(s)->area : 999,#x,error);goto fail; } } while(0)
#define REQUIRE(x) do { if(!(x)) { fprintf(stderr,"Feed shop helper line %d area %u: %s; %s\n",__LINE__,hmbtn_scene_preview_session(s)->area,#x,error);return false; } } while(0)
static bool step(HmbtnScenePreview *s,uint32_t pressed,char *error,size_t size)
{
    return hmbtn_scene_preview_tick(s,(HmbtnLogicalInput){pressed,pressed},error,size);
}
static int half(const uint8_t *p)
{
    unsigned n=hmbtn_read_le16(p);return n<32768 ? (int)n : (int)n-65536;
}
static void at(HmbtnScenePreview *s,const uint8_t *z)
{
    for(unsigned a=0;a<3;++a) hmbtn_scene_preview_session(s)->intro.player.position[a]=(half(z+14+a*2)+half(z+20+a*2))/2;
    hmbtn_scene_preview_session(s)->intro.player.facing=hmbtn_read_le16(z+8)&3u;hmbtn_scene_preview_session(s)->control.mode=0;
}
static bool travel(HmbtnScenePreview *s,unsigned area,char *error,size_t size)
{
    const uint8_t *zone=NULL;bool door=false;
    for(unsigned i=0;i<50;++i) {
        const uint8_t *z=hmbtn_scene_preview_resources(s).section+16+i*44;
        if(hmbtn_read_le16(z+2)!=area) continue;
        at(s,z);HmbtnHouseExit e;unsigned model;
        if(hmbtn_world_door_at(hmbtn_scene_preview_resources(s).section,0x2000,&hmbtn_scene_preview_session(s)->intro.player,&e,&model)) { zone=z;door=true;break; }
        if(hmbtn_world_gate_at(hmbtn_scene_preview_resources(s).section,0x2000,hmbtn_scene_preview_session(s)->intro.player.position,&e)
                || hmbtn_house_exit_at(hmbtn_scene_preview_resources(s).section,0x2000,hmbtn_scene_preview_session(s)->intro.player.position,&e)) { zone=z;break; }
    }
    REQUIRE(zone);REQUIRE(step(s,door ? HMBTN_PAD_CROSS : 0,error,size));
    REQUIRE(hmbtn_scene_preview_session(s)->door_tick || hmbtn_scene_preview_session(s)->travel_tick);
    for(unsigned i=0;i<120 && (hmbtn_scene_preview_session(s)->area!=area || hmbtn_scene_preview_session(s)->travel_tick || hmbtn_scene_preview_session(s)->door_tick);++i) REQUIRE(step(s,0,error,size));
    REQUIRE(hmbtn_scene_preview_session(s)->area==area && !hmbtn_scene_preview_session(s)->control.exit_pending);return true;
}
static bool shop_wait(HmbtnScenePreview *s,HmbtnFeedShopPhase phase,char *error,size_t size)
{
    for(unsigned i=0;i<3000;++i) {
        if(hmbtn_scene_preview_session(s)->feed_shop.phase==phase && !hmbtn_scene_preview_session(s)->feed_shop.closing
                && (phase==HMBTN_SHOP_OFF || hmbtn_scene_preview_session(s)->feed_shop.message.phase==HMBTN_MESSAGE_WAIT_END)) return true;
        REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
    }
    return false;
}
static bool buy(HmbtnScenePreview *s,unsigned id,HmbtnHost *host,const char *captures,char *error,size_t size)
{
    HmbtnIntroActor *merchant=&hmbtn_scene_preview_session(s)->village.people[id].actor;
    REQUIRE(merchant->visible);
    static const int delta[4][2]={{0,-350},{-350,0},{0,350},{350,0}};
    bool reachable=false;
    for(unsigned face=0;face<4 && !reachable;++face) {
        memcpy(hmbtn_scene_preview_session(s)->intro.player.position,merchant->position,sizeof(merchant->position));
        hmbtn_scene_preview_session(s)->intro.player.position[0]-=delta[face][0];hmbtn_scene_preview_session(s)->intro.player.position[2]-=delta[face][1];
        hmbtn_scene_preview_session(s)->intro.player.facing=face;hmbtn_scene_preview_session(s)->control.mode=0;
        reachable=hmbtn_farm_settle(NULL,&(*hmbtn_scene_preview_resources(s).scene).terrain,hmbtn_scene_preview_session(s)->intro.player.position,0xE080,200);
    }
    REQUIRE(reachable);REQUIRE(step(s,0,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && hmbtn_scene_preview_session(s)->feed_shop.phase==HMBTN_SHOP_GREETING);
    const HmbtnFarmClock before=hmbtn_scene_preview_session(s)->game.clock;int32_t position[3];memcpy(position,hmbtn_scene_preview_session(s)->intro.player.position,sizeof(position));
    REQUIRE(step(s,HMBTN_PAD_START,error,size) && hmbtn_scene_preview_session(s)->menu.mode==HMBTN_MENU_NONE);
    REQUIRE(shop_wait(s,HMBTN_SHOP_SELECT,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));REQUIRE(shop_wait(s,HMBTN_SHOP_COUNT,error,size));
    REQUIRE(step(s,HMBTN_PAD_LEFT,error,size) && hmbtn_scene_preview_session(s)->feed_shop.quantity==99);
    const unsigned gold=hmbtn_scene_preview_session(s)->game.economy.gold,stock=id==15 ? 0 : 2;
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));REQUIRE(shop_wait(s,HMBTN_SHOP_RESULT,error,size));
    REQUIRE(hmbtn_scene_preview_session(s)->feed_shop.loaded==0x4F && hmbtn_scene_preview_session(s)->game.economy.gold==gold && !hmbtn_scene_preview_session(s)->house.inventory->supplies[stock]);
    REQUIRE(shop_wait(s,HMBTN_SHOP_COUNT,error,size));
    REQUIRE(step(s,HMBTN_PAD_RIGHT,error,size) && hmbtn_scene_preview_session(s)->feed_shop.quantity==1);
    REQUIRE(step(s,HMBTN_PAD_RIGHT,error,size) && hmbtn_scene_preview_session(s)->feed_shop.quantity==2);
    REQUIRE(hmbtn_scene_preview_draw(s,host->renderer,error,size));
    if(captures) {
        char path[1024];REQUIRE(snprintf(path,sizeof(path),"%s/shop-%u.bmp",captures,id)<(int)sizeof(path));
        SDL_Surface *p=SDL_RenderReadPixels(host->renderer,NULL);REQUIRE(p);
        const bool saved=SDL_SaveBMP(p,path);SDL_DestroySurface(p);REQUIRE(saved);
    }
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
    REQUIRE(hmbtn_scene_preview_session(s)->house.inventory->supplies[stock]==2 && hmbtn_scene_preview_session(s)->game.economy.gold==gold-(id==15 ? 40u : 20u));
    REQUIRE(shop_wait(s,HMBTN_SHOP_SELECT,error,size));
    REQUIRE(step(s,HMBTN_PAD_CIRCLE,error,size));REQUIRE(shop_wait(s,HMBTN_SHOP_OFF,error,size));
    REQUIRE(memcmp(position,hmbtn_scene_preview_session(s)->intro.player.position,sizeof(position))==0);
    REQUIRE(memcmp(&before,&hmbtn_scene_preview_session(s)->game.clock,sizeof(before))==0);return true;
}
static bool feed(HmbtnScenePreview *s,unsigned kind,unsigned bin,char *error,size_t size)
{
    const uint8_t *zone=NULL;
    for(unsigned i=0;i<50;++i) {
        const uint8_t *z=hmbtn_scene_preview_resources(s).section+16+44*i;
        if(hmbtn_read_le16(z)==0x4000 && hmbtn_read_le16(z+2)==kind && hmbtn_read_le16(z+4)==bin) { zone=z;break; }
    }
    REQUIRE(zone);at(s,zone);
    unsigned facing=hmbtn_read_le16(zone+8);if(facing==999 || facing==6) facing=3;if(facing==7) facing=2;
    hmbtn_scene_preview_session(s)->intro.player.facing=facing;hmbtn_scene_preview_session(s)->intro.player.position[facing&1u ? 0 : 2]+=facing<2 ? 224 : -224;
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && hmbtn_scene_preview_session(s)->feed.phase);
    for(unsigned i=0;i<100 && hmbtn_scene_preview_session(s)->feed.phase;++i) REQUIRE(step(s,0,error,size));
    REQUIRE(!hmbtn_scene_preview_session(s)->feed.phase);return true;
}
int main(int argc,char **argv)
{
    if(argc<2 || argc>3) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};
    CHECK(hmbtn_host_open(&host));s=hmbtn_scene_area_create(host.renderer,argv[1],0,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    hmbtn_scene_preview_session(s)->game.clock=(HmbtnFarmClock){0,0,2,2,12,0,0};
    CHECK(hmbtn_scene_preview_session(s)->game.economy.gold==500 && !hmbtn_scene_preview_session(s)->house.inventory->supplies[0] && !hmbtn_scene_preview_session(s)->house.inventory->supplies[2]);
    CHECK(travel(s,32,error,sizeof(error)) && travel(s,24,error,sizeof(error))
        && travel(s,28,error,sizeof(error)) && travel(s,136,error,sizeof(error)));
    CHECK(buy(s,15,&host,argc==3 ? argv[2] : NULL,error,sizeof(error)));
    CHECK(travel(s,28,error,sizeof(error)) && travel(s,24,error,sizeof(error)) && travel(s,128,error,sizeof(error)));
    CHECK(buy(s,18,&host,argc==3 ? argv[2] : NULL,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->game.economy.gold==440 && hmbtn_scene_preview_session(s)->game.economy.ledger[0][1]==60);
    CHECK(travel(s,24,error,sizeof(error)) && travel(s,32,error,sizeof(error)) && travel(s,0,error,sizeof(error)));
    CHECK(travel(s,96,error,sizeof(error)) && feed(s,9,0,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->house.inventory->held_item.id==0x68 && hmbtn_scene_preview_session(s)->house.inventory->supplies[0]==1);
    CHECK(feed(s,41,0,error,sizeof(error)) && !hmbtn_scene_preview_session(s)->house.inventory->held_item.id && hmbtn_scene_preview_session(s)->feed.barn[0]);
    CHECK(travel(s,0,error,sizeof(error)) && travel(s,112,error,sizeof(error)) && feed(s,26,0,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->house.inventory->held_item.id==0x69 && hmbtn_scene_preview_session(s)->house.inventory->supplies[2]==1);
    CHECK(feed(s,40,0,error,sizeof(error)) && !hmbtn_scene_preview_session(s)->house.inventory->held_item.id && hmbtn_scene_preview_session(s)->feed.coop[0]);
    CHECK(hmbtn_scene_preview_session(s)->game.economy.gold==440 && hmbtn_scene_preview_session(s)->game.economy.ledger[0][1]==60 && !hmbtn_scene_preview_session(s)->first_visit_active);
    puts("Original-resource shops: reachable merchants, insufficient funds, real purchases, expenses, map return, stocks and both bins passed.");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

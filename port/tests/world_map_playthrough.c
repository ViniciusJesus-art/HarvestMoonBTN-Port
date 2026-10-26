/* Real-resource navigation checks; fixtures never enter the game runtime. */
#include "scene_fixture.h"
#include "sdl_host.h"
#define REQUIRE(x) do { if(!(x)) { fprintf(stderr,"World navigation line %d: %s; %s (area %u, position %d,%d,%d)\n",__LINE__,#x,error,hmbtn_scene_preview_session(s)->area,hmbtn_scene_preview_session(s)->intro.player.position[0],hmbtn_scene_preview_session(s)->intro.player.position[1],hmbtn_scene_preview_session(s)->intro.player.position[2]);return false; } } while(0)
typedef struct Passage { HmbtnHouseExit exit;int32_t min[3],max[3];unsigned facing; } Passage;
typedef struct Route { bool visited[84],covered[84][50];unsigned passages,signs,boundaries; } Route;
static int32_t half(const uint8_t *p)
{
    const unsigned value=hmbtn_read_le16(p);return value<32768 ? (int32_t)value : (int32_t)value-65536;
}
static bool step(HmbtnScenePreview *s,uint32_t pressed,uint32_t held,char *error,size_t size)
{
    return hmbtn_scene_preview_tick(s,(HmbtnLogicalInput){pressed,held},error,size);
}
static unsigned passages(const HmbtnScenePreview *s,Passage out[50])
{
    unsigned count=0;
    for(unsigned i=0;i<50;++i) {
        const uint8_t *p=hmbtn_scene_preview_resources(s).section+16+i*44;int32_t point[3];
        for(unsigned a=0;a<3;++a) point[a]=(half(p+14+a*2)+half(p+20+a*2))/2;
        HmbtnHouseExit exit;unsigned logical,resource;
        if(!hmbtn_house_exit_at(hmbtn_scene_preview_resources(s).section,(*hmbtn_scene_preview_resources(s).scene).terrain.size+0x2000,point,&exit) || exit.zone!=i
                || !hmbtn_world_area(exit.target,hmbtn_scene_preview_session(s)->game.clock.season,&logical,&resource)) continue;
        Passage *link=&out[count++];link->exit=exit;link->facing=hmbtn_read_le16(p+8);
        for(unsigned a=0;a<3;++a) { link->min[a]=half(p+14+a*2);link->max[a]=half(p+20+a*2); }
    }
    return count;
}
static bool walk(HmbtnScenePreview *s,const Passage *link,char *error,size_t size)
{
    static const uint32_t buttons[4]={HMBTN_PAD_DOWN,HMBTN_PAD_LEFT,HMBTN_PAD_UP,HMBTN_PAD_RIGHT};
    const unsigned from=hmbtn_scene_preview_session(s)->area,face=link->facing;unsigned logical,resource;
    REQUIRE(face<4 && hmbtn_world_area(link->exit.target,hmbtn_scene_preview_session(s)->game.clock.season,&logical,&resource));
    for(unsigned a=0;a<3;++a) hmbtn_scene_preview_session(s)->intro.player.position[a]=(link->min[a]+link->max[a])/2;
    const unsigned axis=face&1u ? 0 : 2;
    hmbtn_scene_preview_session(s)->intro.player.position[axis]=face<2 ? link->max[axis]+82 : link->min[axis]-82;
    HmbtnGroundHit ground;
    REQUIRE(hmbtn_ground_find(&(*hmbtn_scene_preview_resources(s).scene).terrain,hmbtn_scene_preview_session(s)->intro.player.position,0xC080,1720,&ground));
    hmbtn_scene_preview_session(s)->intro.player.position[1]=ground.height;hmbtn_scene_preview_session(s)->intro.player.facing=face;
    hmbtn_scene_preview_session(s)->intro.player.sprite=(HmbtnAnimationPlayer){0,(uint16_t)face,0};hmbtn_scene_preview_session(s)->control.mode=0;
    for(unsigned i=0;i<100 && !hmbtn_scene_preview_session(s)->travel_tick;++i) REQUIRE(step(s,0,buttons[face],error,size));
    REQUIRE(hmbtn_scene_preview_session(s)->travel_tick && hmbtn_scene_preview_session(s)->area==from && hmbtn_scene_preview_session(s)->control.exit_pending);
    const HmbtnFarmClock frozen=hmbtn_scene_preview_session(s)->game.clock;
    for(unsigned i=0;i<30 && hmbtn_scene_preview_session(s)->travel_tick;++i) {
        REQUIRE(step(s,0,0,error,size));
        if(hmbtn_scene_preview_session(s)->travel_tick) REQUIRE(memcmp(&frozen,&hmbtn_scene_preview_session(s)->game.clock,sizeof(frozen))==0);
    }
    REQUIRE(hmbtn_scene_preview_session(s)->area==logical && !hmbtn_scene_preview_session(s)->travel_tick && !hmbtn_scene_preview_session(s)->control.exit_pending && hmbtn_scene_preview_session(s)->control.enabled);
    REQUIRE(hmbtn_scene_preview_session(s)->intro.player.position[0]==link->exit.entry[0] && hmbtn_scene_preview_session(s)->intro.player.position[2]==link->exit.entry[2]);
    REQUIRE(hmbtn_ground_find(&(*hmbtn_scene_preview_resources(s).scene).terrain,hmbtn_scene_preview_session(s)->intro.player.position,0xC080,200,&ground));
    REQUIRE(hmbtn_scene_preview_session(s)->intro.player.position[1]==ground.height);
    HmbtnHouseExit bounce;
    REQUIRE(!hmbtn_house_exit_at(hmbtn_scene_preview_resources(s).section,(*hmbtn_scene_preview_resources(s).scene).terrain.size+0x2000,hmbtn_scene_preview_session(s)->intro.player.position,&bounce));
    return true;
}
static bool inspect(HmbtnScenePreview *s,HmbtnHost *host,Route *route,const char *captures,char *error,size_t size)
{
    hmbtn_scene_preview_invalidate(s);REQUIRE(hmbtn_scene_preview_draw(s,host->renderer,error,size));
    if(captures) {
        char path[1024];snprintf(path,sizeof(path),"%s/region-%u-season-%u.bmp",captures,hmbtn_scene_preview_session(s)->area,hmbtn_scene_preview_session(s)->game.clock.season);
        SDL_Surface *surface=SDL_RenderReadPixels(host->renderer,NULL);REQUIRE(surface);
        const bool saved=SDL_SaveBMP(surface,path);SDL_DestroySurface(surface);REQUIRE(saved);
    }
    REQUIRE(SDL_RenderPresent(host->renderer));
    if(hmbtn_scene_preview_session(s)->area==56) {
        /* The beach has separate day/night SEQs, both using pitch-wheel
         * commands. Decode and mix their actual banks as well as opening them. */
        const HmbtnFarmClock clock=hmbtn_scene_preview_session(s)->game.clock;
        for(unsigned night=0;night<2;++night) {
            hmbtn_scene_preview_session(s)->game.clock.hour=night ? 17u : 12u;
            REQUIRE(step(s,0,0,error,size));
            REQUIRE(hmbtn_scene_preview_session(s)->runtime.audio_song==(night ? 24u : 25u));
            uint64_t energy=0;int16_t pcm[1024];
            for(unsigned i=0;i<64;++i) {
                hmbtn_sound_render((*hmbtn_scene_preview_resources(s).audio).sound,pcm,512);
                for(unsigned j=0;j<1024;++j) energy+=(uint64_t)abs(pcm[j]);
            }
            REQUIRE(night && hmbtn_scene_preview_session(s)->game.clock.season==3 ? energy==0 : energy>0);
        }
        hmbtn_scene_preview_session(s)->game.clock=clock;hmbtn_daylight_select(&hmbtn_scene_preview_session(s)->daylight,&hmbtn_scene_preview_session(s)->game.clock);
        REQUIRE(step(s,0,0,error,size));
    }
    /* Read each real sign, including the personalized farm-direction board. */
    if(hmbtn_scene_preview_session(s)->area==0) return true;
    for(unsigned i=0;i<50;++i) {
        const uint8_t *p=hmbtn_scene_preview_resources(s).section+16+i*44;
        if(hmbtn_read_le16(p)!=0x4000 || hmbtn_read_le16(p+2)!=16) continue;
        const unsigned face=hmbtn_read_le16(p+8)==5 ? 0u : hmbtn_read_le16(p+8);
        REQUIRE(face<4);
        for(unsigned a=0;a<3;++a) hmbtn_scene_preview_session(s)->intro.player.position[a]=(half(p+14+a*2)+half(p+20+a*2))/2;
        hmbtn_scene_preview_session(s)->intro.player.position[face&1u ? 0 : 2]+=face<2 ? 224 : -224;
        hmbtn_scene_preview_session(s)->intro.player.facing=face;hmbtn_scene_preview_session(s)->control.mode=0;
        const HmbtnStoredTool held=hmbtn_scene_preview_session(s)->house.inventory->held_item;hmbtn_scene_preview_session(s)->house.inventory->held_item=(HmbtnStoredTool){0};
        REQUIRE(step(s,HMBTN_PAD_CROSS,0,error,size));
        REQUIRE(hmbtn_scene_preview_session(s)->house.mode==HMBTN_HOUSE_BOOK && hmbtn_scene_preview_session(s)->house.request==hmbtn_read_le16(p+4));
        const HmbtnFarmClock frozen=hmbtn_scene_preview_session(s)->game.clock;
        for(unsigned t=0;t<20;++t) REQUIRE(step(s,0,0,error,size));
        REQUIRE(memcmp(&frozen,&hmbtn_scene_preview_session(s)->game.clock,sizeof(frozen))==0);
        REQUIRE(hmbtn_scene_preview_draw(s,host->renderer,error,size));
        for(unsigned t=0;t<2500 && hmbtn_scene_preview_session(s)->house.mode!=HMBTN_HOUSE_FREE;++t) {
            const bool waiting=hmbtn_scene_preview_session(s)->house.message.phase==HMBTN_MESSAGE_WAIT_PAGE || hmbtn_scene_preview_session(s)->house.message.phase==HMBTN_MESSAGE_WAIT_END;
            REQUIRE(step(s,waiting ? HMBTN_PAD_CROSS : 0,HMBTN_PAD_CROSS,error,size));
        }
        REQUIRE(hmbtn_scene_preview_session(s)->house.mode==HMBTN_HOUSE_FREE && !hmbtn_scene_preview_session(s)->control.exit_pending);
        hmbtn_scene_preview_session(s)->house.inventory->held_item=held;++route->signs;
    }
    /* Mine entrances need their own digging/layer systems. Reaching one must
     * leave the player able to move back rather than latch exit_pending. */
    for(unsigned i=0;i<50;++i) {
        const uint8_t *p=hmbtn_scene_preview_resources(s).section+16+i*44;unsigned logical,resource;
        const unsigned flags=hmbtn_read_le16(p),target=hmbtn_read_le16(p+2);
        if((flags&0x7388u) || hmbtn_read_le16(p+10)!=99 || target>=255
                || hmbtn_world_area(target,hmbtn_scene_preview_session(s)->game.clock.season,&logical,&resource)) continue;
        for(unsigned a=0;a<3;++a) hmbtn_scene_preview_session(s)->intro.player.position[a]=(half(p+14+a*2)+half(p+20+a*2))/2;
        const unsigned area=hmbtn_scene_preview_session(s)->area;
        REQUIRE(step(s,0,0,error,size));
        REQUIRE(hmbtn_scene_preview_session(s)->area==area && hmbtn_scene_preview_session(s)->control.enabled && !hmbtn_scene_preview_session(s)->control.exit_pending && !hmbtn_scene_preview_session(s)->travel_tick);
        ++route->boundaries;
    }
    return true;
}
static bool explore(HmbtnScenePreview *s,HmbtnHost *host,Route *route,const char *captures,char *error,size_t size)
{
    const unsigned from=hmbtn_scene_preview_session(s)->area;route->visited[from]=true;
    REQUIRE(inspect(s,host,route,captures,error,size));
    Passage links[50];const unsigned count=passages(s,links);
    for(unsigned i=0;i<count;++i) {
        if(route->covered[from][links[i].exit.zone]) continue;
        REQUIRE(walk(s,&links[i],error,size));
        route->covered[from][links[i].exit.zone]=true;++route->passages;
        Passage returns[50],back={0};const unsigned total=passages(s,returns);bool found=false;
        for(unsigned j=0;j<total;++j) {
            unsigned logical,resource;
            REQUIRE(hmbtn_world_area(returns[j].exit.target,hmbtn_scene_preview_session(s)->game.clock.season,&logical,&resource));
            if(logical!=from) continue;
            route->covered[hmbtn_scene_preview_session(s)->area][returns[j].exit.zone]=true;
            back=returns[j];found=true;break;
        }
        REQUIRE(found);
        if(!route->visited[hmbtn_scene_preview_session(s)->area]) REQUIRE(explore(s,host,route,captures,error,size));
        REQUIRE(walk(s,&back,error,size));++route->passages;
        REQUIRE(hmbtn_scene_preview_session(s)->area==from);
    }
    return true;
}
int main(int argc,char **argv)
{
    if(argc<2 || argc>3) { fputs("Usage: world_map_playthrough data-directory [capture-directory]\n",stderr);return 2; }
    HmbtnHost host={0};char error[512]={0};HmbtnScenePreview *s=NULL;bool ok=false;
    if(!hmbtn_host_open(&host)) goto done;
    s=hmbtn_scene_house_create(host.renderer,argv[1],error,sizeof(error));if(!s) goto done;
    for(unsigned i=0;i<130;++i) if(!step(s,0,0,error,sizeof(error))) goto done;
    hmbtn_scene_preview_session(s)->first_visit_started=true;
    hmbtn_scene_preview_session(s)->house.inventory->bag[0]=(HmbtnStoredTool){21,0,73};hmbtn_scene_preview_session(s)->house.inventory->held_item=(HmbtnStoredTool){0x3a,0,1};
    for(unsigned i=0;i<8;++i) hmbtn_scene_preview_session(s)->game.registration.farm_name[i]=(uint16_t)(0x20+i);
    hmbtn_scene_preview_session(s)->field.cells[400].value=hmbtn_scene_preview_session(s)->field.cells[400].ground=0x168;
    HmbtnFarmTile field[HMBTN_FIELD_CELLS];memcpy(field,hmbtn_scene_preview_session(s)->field.cells,sizeof(field));
    hmbtn_scene_preview_session(s)->game.economy.shipping.pending_value=60;hmbtn_scene_preview_session(s)->game.economy.shipments[0]=1;
    for(unsigned season=0;season<4;++season) {
        hmbtn_scene_preview_session(s)->game.clock=(HmbtnFarmClock){0,season,1,1,6,0,0};
        if(!hmbtn_scene_preview_load_area(s,0,error,sizeof(error))) goto done;hmbtn_scene_preview_session(s)->intro.area=0;
        Route route={0};
        if(!explore(s,&host,&route,argc==3 ? argv[2] : NULL,error,sizeof(error))) goto done;
        for(unsigned area=24;area<80;area+=4) if(!route.visited[area]) goto done;
        printf("World season %u: 15 regions, %u directed passages, %u signs.\n",season,route.passages,route.signs);
        if(route.passages!=34 || route.signs!=(season==1 ? 17u : 18u) || route.boundaries!=2
                || hmbtn_scene_preview_session(s)->first_visit_active || !hmbtn_scene_preview_session(s)->first_visit_started
                || hmbtn_scene_preview_session(s)->house.inventory->bag[0].id!=21 || hmbtn_scene_preview_session(s)->house.inventory->bag[0].count!=73 || hmbtn_scene_preview_session(s)->house.inventory->held_item.id!=0x3a
                || hmbtn_scene_preview_session(s)->game.economy.shipping.pending_value!=60 || hmbtn_scene_preview_session(s)->game.economy.shipments[0]!=1
                || memcmp(field,hmbtn_scene_preview_session(s)->field.cells,sizeof(field))) goto done;
    }
    ok=true;puts("All outdoor regions, reciprocal walking exits, seasonal terrain/textures, sign reading, clock pauses and inventory preservation passed.");
done:
    if(!ok) fprintf(stderr,"World traversal failed: %s\n",error);
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return ok ? 0 : 1;
}

/* Original scene, actions, icons, NPC script and sound. Positions/dates and the
 * two unique-catch RNG seeds are fixtures; no runtime loot shortcut is exposed. */
#include "scene_fixture.h"
#include "sdl_host.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Fishing integration line %d phase %u: %s; %s\n",__LINE__,s ? hmbtn_scene_preview_session(s)->fishing.phase : 999,#x,error);goto fail; } } while(0)
static bool step(HmbtnScenePreview *s,uint32_t pressed,uint32_t held,char *error,size_t size)
{
    return hmbtn_scene_preview_tick(s,(HmbtnLogicalInput){pressed,held},error,size);
}
static bool capture(HmbtnScenePreview *s,HmbtnHost *host,const char *dir,const char *name,char *error,size_t size)
{
    if(!hmbtn_scene_preview_draw(s,host->renderer,error,size)) return false;
    if(dir) {
        char path[1024];snprintf(path,sizeof(path),"%s/%s.bmp",dir,name);
        SDL_Surface *p=SDL_RenderReadPixels(host->renderer,NULL);if(!p) return false;
        const bool saved=SDL_SaveBMP(p,path);SDL_DestroySurface(p);if(!saved) return false;
    }
    return true;
}
static bool cast_point(HmbtnScenePreview *s,int32_t position[3],unsigned *facing)
{
    const int32_t *g=hmbtn_scene_preview_session(s)->village.people[32].actor.position;
    static const int reach[4][2]={{0,-783},{-783,0},{0,783},{783,0}};
    for(int x=-2400;x<=2400;x+=200) for(int z=-2400;z<=2400;z+=200) {
        int32_t p[3]={g[0]+x,g[1],g[2]+z};HmbtnGroundHit dry,wet;
        if(!hmbtn_ground_find(&(*hmbtn_scene_preview_resources(s).scene).terrain,p,0xE087,1720,&dry)) continue;
        p[1]=dry.height;
        for(unsigned face=0;face<4;++face) {
            int32_t ahead[3]={p[0]+reach[face][0],p[1],p[2]+reach[face][1]};
            if(!hmbtn_ground_find(&(*hmbtn_scene_preview_resources(s).scene).terrain,ahead,0,1720,&wet) || !(wet.flags&2u) || (wet.flags&0x44u)) continue;
            HmbtnFishing f={0};HmbtnIntroActor a={0};memcpy(a.position,p,sizeof(p));a.facing=face;
            HmbtnHouseInteraction h;HmbtnGameState h_state;hmbtn_game_state_init(&h_state);hmbtn_house_interaction_init(&h,&h_state);h.inventory->equipped.id=56;
            HmbtnFarmAction status;hmbtn_farm_action_init(&status);bool flags[512]={0};
            hmbtn_fishing_tick(&f,&a,&h,&status,&(*hmbtn_scene_preview_resources(s).scene).terrain,&hmbtn_scene_preview_session(s)->game.clock,44,flags,HMBTN_PAD_SQUARE,HMBTN_PAD_SQUARE);
            for(unsigned i=0;i<200 && f.phase==HMBTN_FISHING_CAST;++i) {
                a.animation_done=true;hmbtn_fishing_tick(&f,&a,&h,&status,&(*hmbtn_scene_preview_resources(s).scene).terrain,&hmbtn_scene_preview_session(s)->game.clock,44,flags,0,HMBTN_PAD_SQUARE);
            }
            if(f.phase==HMBTN_FISHING_WAIT) { memcpy(position,p,sizeof(p));*facing=face;return true; }
        }
    }
    return false;
}
static bool box(const HmbtnModel *m,int32_t p[3])
{
    for(unsigned i=0;i<m->object_count;++i) {
        HmbtnModelObject o;if(!hmbtn_model_object(m,i,&o)) return false;
        size_t at=12+o.primitives;
        for(unsigned j=0;j<o.primitive_count;++j) {
            HmbtnModelPrimitive q;if(!hmbtn_model_primitive(m,at,&q)) return false;at+=q.bytes;
            if(!(q.flags&0x80)) continue;
            memset(p,0,3*sizeof(*p));
            for(unsigned k=0;k<q.count;++k) { int32_t v[3];hmbtn_model_vertex(m,&o,q.vertex[k],v);for(unsigned a=0;a<3;++a) p[a]+=v[a]; }
            for(unsigned a=0;a<3;++a) p[a]/=(int32_t)q.count;
            return true;
        }
    }
    return false;
}
int main(int argc,char **argv)
{
    if(argc<2 || argc>3) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};const char *shots=argc==3 ? argv[2] : NULL;
    CHECK(hmbtn_host_open(&host));const HmbtnFarmClock date={0,0,5,5,7,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],56,&date,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,0,error,sizeof(error)));
    /* Receive the rod through Greg's actual script. */
    const HmbtnIntroActor *greg=&hmbtn_scene_preview_session(s)->village.people[32].actor;CHECK(greg->visible);
    memcpy(hmbtn_scene_preview_session(s)->intro.player.position,greg->position,sizeof(greg->position));hmbtn_scene_preview_session(s)->intro.player.position[2]+=300;hmbtn_scene_preview_session(s)->intro.player.facing=0;
    CHECK(step(s,HMBTN_PAD_CROSS,0,error,sizeof(error)) && hmbtn_scene_preview_session(s)->talk.active);
    for(unsigned i=0;i<3000 && (hmbtn_scene_preview_session(s)->talk.active || hmbtn_scene_preview_session(s)->quick_ticks);++i) CHECK(step(s,HMBTN_PAD_CROSS,0,error,sizeof(error)));
    CHECK(!hmbtn_scene_preview_session(s)->talk.active && hmbtn_scene_preview_session(s)->house.inventory->equipped.id==56 && hmbtn_scene_preview_session(s)->village.flags[0xE7]);
    const unsigned rows[]={3,4,10,12,15,22,23};
    for(unsigned i=0;i<7;++i) CHECK(test_action(s,rows[i],error,sizeof(error)));
    int32_t shore[3];unsigned facing;CHECK(cast_point(s,shore,&facing));
    printf("Fishing shore (%d,%d,%d) facing %u\n",shore[0],shore[1],shore[2],facing);
    memcpy(hmbtn_scene_preview_session(s)->intro.player.position,shore,sizeof(shore));hmbtn_scene_preview_session(s)->intro.player.facing=facing;
    memcpy(hmbtn_scene_preview_session(s)->intro.camera,shore,sizeof(shore));
    CHECK(step(s,HMBTN_PAD_SQUARE,HMBTN_PAD_SQUARE,error,sizeof(error)) && hmbtn_scene_preview_session(s)->intro.action_active);
    for(unsigned i=0;i<200 && hmbtn_scene_preview_session(s)->fishing.phase==HMBTN_FISHING_CAST;++i) CHECK(step(s,0,HMBTN_PAD_SQUARE,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->fishing.phase==HMBTN_FISHING_WAIT && hmbtn_scene_preview_session(s)->fishing.water);
    CHECK(capture(s,&host,shots,"fishing-wait",error,sizeof(error)));
    CHECK(step(s,0,0,error,sizeof(error)) && hmbtn_scene_preview_session(s)->fishing.phase==HMBTN_FISHING_REEL);
    for(unsigned i=0;i<200 && hmbtn_scene_preview_session(s)->fishing.phase!=HMBTN_FISHING_FREE;++i) CHECK(step(s,0,0,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->fishing.phase==HMBTN_FISHING_FREE && !hmbtn_scene_preview_session(s)->house.inventory->held_item.id);
    unsigned caught=0;
    for(unsigned cast=0;cast<24 && !caught;++cast) {
        memcpy(hmbtn_scene_preview_session(s)->intro.player.position,shore,sizeof(shore));hmbtn_scene_preview_session(s)->intro.player.facing=facing;
        CHECK(step(s,HMBTN_PAD_SQUARE,HMBTN_PAD_SQUARE,error,sizeof(error)));
        for(unsigned i=0;i<5000 && hmbtn_scene_preview_session(s)->fishing.phase!=HMBTN_FISHING_HOOKED;++i) {
            CHECK(step(s,HMBTN_PAD_START,HMBTN_PAD_SQUARE|HMBTN_PAD_UP,error,sizeof(error)));
            CHECK(!memcmp(shore,hmbtn_scene_preview_session(s)->intro.player.position,sizeof(shore)) && hmbtn_scene_preview_session(s)->menu.mode==HMBTN_MENU_NONE);
        }
        CHECK(hmbtn_scene_preview_session(s)->fishing.phase==HMBTN_FISHING_HOOKED);
        CHECK(capture(s,&host,shots,"fishing-hooked",error,sizeof(error)));
        CHECK(step(s,0,0,error,sizeof(error)));
        for(unsigned i=0;i<300 && hmbtn_scene_preview_session(s)->fishing.phase!=HMBTN_FISHING_FREE;++i) CHECK(step(s,0,0,error,sizeof(error)));
        CHECK(hmbtn_scene_preview_session(s)->fishing.phase==HMBTN_FISHING_FREE);
        const unsigned item=hmbtn_scene_preview_session(s)->house.inventory->held_item.id;
        if(item>=161 && item<=163) { caught=item;break; }
        if(item) {
            CHECK(step(s,HMBTN_PAD_CROSS,0,error,sizeof(error)));
            for(unsigned i=0;i<200 && (hmbtn_scene_preview_session(s)->carry.phase || hmbtn_scene_preview_session(s)->carry.thrown[0].item.id);++i) CHECK(step(s,0,0,error,sizeof(error)));
            CHECK(!hmbtn_scene_preview_session(s)->house.inventory->held_item.id);
        }
    }
    CHECK(caught);CHECK(capture(s,&host,shots,"fishing-caught",error,sizeof(error)));
    /* The caught item really reaches a shipping surface and pays at 17:58. */
    const unsigned gold=hmbtn_scene_preview_session(s)->game.economy.gold;const int price=hmbtn_farm_item(caught)->sell_price;
    hmbtn_scene_preview_session(s)->village.flags[0x5E]=true;hmbtn_scene_preview_session(s)->fishing.streak=11;
    CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)));hmbtn_scene_preview_session(s)->intro.area=0;hmbtn_scene_preview_session(s)->first_visit_started=true;
    CHECK(!hmbtn_scene_preview_session(s)->village.flags[0x5E] && !hmbtn_scene_preview_session(s)->fishing.streak);
    int32_t bin[3];CHECK(box(&(*hmbtn_scene_preview_resources(s).scene).terrain,bin));int y=-300,v=0;unsigned ticks=0;
    do { y+=v;v+=9;if(v>45) v=45;++ticks; } while(y<bin[1] && ticks<100);
    hmbtn_scene_preview_session(s)->intro.player.position[0]=bin[0]-(int)ticks*44;hmbtn_scene_preview_session(s)->intro.player.position[1]=0;hmbtn_scene_preview_session(s)->intro.player.position[2]=bin[2];hmbtn_scene_preview_session(s)->intro.player.facing=3;
    CHECK(step(s,HMBTN_PAD_CROSS,0,error,sizeof(error)) && hmbtn_scene_preview_session(s)->carry.phase);
    for(unsigned i=0;i<200 && (hmbtn_scene_preview_session(s)->carry.phase || hmbtn_scene_preview_session(s)->carry.thrown[0].item.id);++i) CHECK(step(s,0,0,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->game.economy.shipping.pending_value==(unsigned)price);
    hmbtn_scene_preview_session(s)->game.clock.hour=17;hmbtn_scene_preview_session(s)->game.clock.minute=57;hmbtn_scene_preview_session(s)->game.clock.frames=14;CHECK(step(s,0,0,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->game.economy.gold==gold+(unsigned)price && hmbtn_scene_preview_session(s)->game.economy.shipping.fish_shipped==1);
    hmbtn_scene_preview_session(s)->game.economy.shipping.fish_shipped=199;hmbtn_session_clock_events(hmbtn_scene_preview_session(s),&hmbtn_scene_preview_session(s)->game.clock,0);CHECK(!hmbtn_scene_preview_session(s)->village.flags[0x5F]);
    hmbtn_scene_preview_session(s)->game.economy.shipping.fish_shipped=200;hmbtn_session_clock_events(hmbtn_scene_preview_session(s),&hmbtn_scene_preview_session(s)->game.clock,0);CHECK(hmbtn_scene_preview_session(s)->village.flags[0x5F]);
    hmbtn_scene_preview_session(s)->game.economy.shipping.fish_shipped=1;hmbtn_scene_preview_session(s)->village.flags[0x5F]=false; /* End the threshold fixture. */
    /* Special-result fixtures run the actual ANI and complete item effects. */
    hmbtn_scene_preview_session(s)->game.clock.season=3;hmbtn_scene_preview_session(s)->game.clock.hour=7;hmbtn_scene_preview_session(s)->game.clock.minute=0;
    CHECK(hmbtn_scene_preview_load_area(s,56,error,sizeof(error)));hmbtn_scene_preview_session(s)->intro.area=56;
    memcpy(hmbtn_scene_preview_session(s)->intro.player.position,shore,sizeof(shore));memcpy(hmbtn_scene_preview_session(s)->intro.camera,shore,sizeof(shore));hmbtn_scene_preview_session(s)->intro.player.facing=facing;
    CHECK(test_action(s,10,error,sizeof(error)));hmbtn_scene_preview_session(s)->intro.action_active=true;
    hmbtn_scene_preview_session(s)->fishing.phase=HMBTN_FISHING_CATCH;hmbtn_scene_preview_session(s)->fishing.kind=0;hmbtn_scene_preview_session(s)->fishing.random=54;hmbtn_scene_preview_session(s)->fishing.using_action=true;
    hmbtn_scene_preview_session(s)->intro.player.sprite=(HmbtnAnimationPlayer){(uint16_t)(facing+4),0,0};hmbtn_scene_preview_session(s)->intro.player.animation_done=false;
    for(unsigned i=0;i<300 && hmbtn_scene_preview_session(s)->fishing.phase!=HMBTN_FISHING_FREE;++i) CHECK(step(s,0,0,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->fishing.phase==HMBTN_FISHING_FREE && hmbtn_scene_preview_session(s)->village.flags[0x53] && hmbtn_scene_preview_session(s)->farm_action.max_stamina==110
        && hmbtn_scene_preview_session(s)->farm_action.stamina==110 && hmbtn_scene_preview_session(s)->menu.stats.power_berries==1 && !hmbtn_scene_preview_session(s)->house.inventory->held_item.id);
    hmbtn_scene_preview_session(s)->game.clock.hour=22;hmbtn_scene_preview_session(s)->game.clock.minute=1;CHECK(test_action(s,10,error,sizeof(error)));hmbtn_scene_preview_session(s)->intro.action_active=true;
    hmbtn_scene_preview_session(s)->fishing.phase=HMBTN_FISHING_CATCH;hmbtn_scene_preview_session(s)->fishing.random=23;hmbtn_scene_preview_session(s)->fishing.using_action=true;
    hmbtn_scene_preview_session(s)->intro.player.sprite=(HmbtnAnimationPlayer){(uint16_t)(facing+4),0,0};hmbtn_scene_preview_session(s)->intro.player.animation_done=false;
    bool print=false;
    for(unsigned i=0;i<500 && hmbtn_scene_preview_session(s)->fishing.phase!=HMBTN_FISHING_FREE;++i) {
        CHECK(step(s,0,0,error,sizeof(error)));
        if(hmbtn_scene_preview_session(s)->fishing.phase==HMBTN_FISHING_PRINT_HOLD && !print) { print=true;CHECK(capture(s,&host,shots,"fishing-print",error,sizeof(error))); }
    }
    CHECK(print && hmbtn_scene_preview_session(s)->fishing.phase==HMBTN_FISHING_FREE && hmbtn_scene_preview_session(s)->village.flags[0x58] && !hmbtn_scene_preview_session(s)->house.inventory->held_item.id);
    /* Dawn interrupts a live cast and preserves permanent stamina/catch state. */
    CHECK(test_action(s,10,error,sizeof(error)));hmbtn_scene_preview_session(s)->intro.action_active=true;
    hmbtn_scene_preview_session(s)->fishing.phase=HMBTN_FISHING_WAIT;hmbtn_scene_preview_session(s)->fishing.using_action=true;hmbtn_scene_preview_session(s)->fishing.marker_visible=true;hmbtn_scene_preview_session(s)->fishing.wait=60;
    hmbtn_scene_preview_session(s)->intro.player.sprite=(HmbtnAnimationPlayer){8,(uint16_t)facing,0};
    hmbtn_scene_preview_session(s)->game.clock.hour=5;hmbtn_scene_preview_session(s)->game.clock.minute=59;hmbtn_scene_preview_session(s)->game.clock.frames=14;
    CHECK(step(s,0,HMBTN_PAD_SQUARE,error,sizeof(error)) && hmbtn_scene_preview_session(s)->overnight_tick);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,0,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->area==80 && hmbtn_scene_preview_session(s)->fishing.phase==HMBTN_FISHING_FREE && !hmbtn_scene_preview_session(s)->fishing.marker_visible
        && !hmbtn_scene_preview_session(s)->intro.action_active && hmbtn_scene_preview_session(s)->farm_action.max_stamina==110 && hmbtn_scene_preview_session(s)->village.flags[0x58]);
    hmbtn_scene_preview_session(s)->farm_action.stamina=1;hmbtn_scene_preview_session(s)->house.mode=HMBTN_HOUSE_SLEEP;const unsigned day=hmbtn_scene_preview_session(s)->game.clock.day;
    for(unsigned i=0;i<1200 && (hmbtn_scene_preview_session(s)->game.clock.day==day || hmbtn_scene_preview_session(s)->morning_fade);++i) CHECK(step(s,0,0,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->game.clock.day!=day && !hmbtn_scene_preview_session(s)->sleep.active && hmbtn_scene_preview_session(s)->farm_action.max_stamina==110 && hmbtn_scene_preview_session(s)->farm_action.stamina==110);
    puts("Greg rod -> real water cast/cancel/bite/catch -> shipping payout; berry and released king/print ANI passed.");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

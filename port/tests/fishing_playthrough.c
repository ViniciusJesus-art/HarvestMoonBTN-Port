/* Original scene, actions, icons, NPC script and sound. Positions/dates and the
 * two unique-catch RNG seeds are fixtures; no runtime loot shortcut is exposed. */
#include "scene_preview.c"
#include "sdl_host.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Fishing integration line %d phase %u: %s; %s\n",__LINE__,s ? s->fishing.phase : 999,#x,error);goto fail; } } while(0)
static bool step(HmbtnScenePreview *s,uint32_t pressed,uint32_t held,char *error,size_t size)
{
    s->pending_pressed=pressed;hmbtn_scene_data_tick(&s->scene);
    const bool ok=opening_tick(s,held,error,size);s->pending_pressed=0;return ok;
}
static bool capture(HmbtnScenePreview *s,HmbtnHost *host,const char *dir,const char *name,char *error,size_t size)
{
    if(!hmbtn_scene_preview_draw(s,host->renderer,0,0,0,error,size)) return false;
    if(dir) {
        char path[1024];snprintf(path,sizeof(path),"%s/%s.bmp",dir,name);
        SDL_Surface *p=SDL_RenderReadPixels(host->renderer,NULL);if(!p) return false;
        const bool saved=SDL_SaveBMP(p,path);SDL_DestroySurface(p);if(!saved) return false;
    }
    return true;
}
static bool cast_point(HmbtnScenePreview *s,int32_t position[3],unsigned *facing)
{
    const int32_t *g=s->village.people[32].actor.position;
    static const int reach[4][2]={{0,-783},{-783,0},{0,783},{783,0}};
    for(int x=-2400;x<=2400;x+=200) for(int z=-2400;z<=2400;z+=200) {
        int32_t p[3]={g[0]+x,g[1],g[2]+z};HmbtnGroundHit dry,wet;
        if(!hmbtn_ground_find(&s->scene.terrain,p,0xE087,1720,&dry)) continue;
        p[1]=dry.height;
        for(unsigned face=0;face<4;++face) {
            int32_t ahead[3]={p[0]+reach[face][0],p[1],p[2]+reach[face][1]};
            if(!hmbtn_ground_find(&s->scene.terrain,ahead,0,1720,&wet) || !(wet.flags&2u) || (wet.flags&0x44u)) continue;
            HmbtnFishing f={0};HmbtnIntroActor a={0};memcpy(a.position,p,sizeof(p));a.facing=face;
            HmbtnHouseInteraction h;hmbtn_house_interaction_init(&h);h.equipped.id=56;
            HmbtnFarmAction status;hmbtn_farm_action_init(&status);bool flags[512]={0};
            hmbtn_fishing_tick(&f,&a,&h,&status,&s->scene.terrain,&s->clock,44,flags,HMBTN_PAD_SQUARE,HMBTN_PAD_SQUARE);
            for(unsigned i=0;i<200 && f.phase==HMBTN_FISHING_CAST;++i) {
                a.animation_done=true;hmbtn_fishing_tick(&f,&a,&h,&status,&s->scene.terrain,&s->clock,44,flags,0,HMBTN_PAD_SQUARE);
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
    const HmbtnIntroActor *greg=&s->village.people[32].actor;CHECK(greg->visible);
    memcpy(s->intro.player.position,greg->position,sizeof(greg->position));s->intro.player.position[2]+=300;s->intro.player.facing=0;
    CHECK(step(s,HMBTN_PAD_CROSS,0,error,sizeof(error)) && s->talk.active);
    for(unsigned i=0;i<3000 && (s->talk.active || s->quick_ticks);++i) CHECK(step(s,HMBTN_PAD_CROSS,0,error,sizeof(error)));
    CHECK(!s->talk.active && s->house.equipped.id==56 && s->village.flags[0xE7]);
    const unsigned rows[]={3,4,10,12,15,22,23};
    for(unsigned i=0;i<7;++i) CHECK(load_action_bank(s,rows[i],error,sizeof(error)));
    int32_t shore[3];unsigned facing;CHECK(cast_point(s,shore,&facing));
    printf("Fishing shore (%d,%d,%d) facing %u\n",shore[0],shore[1],shore[2],facing);
    memcpy(s->intro.player.position,shore,sizeof(shore));s->intro.player.facing=facing;
    memcpy(s->intro.camera,shore,sizeof(shore));
    CHECK(step(s,HMBTN_PAD_SQUARE,HMBTN_PAD_SQUARE,error,sizeof(error)) && s->intro.action_active);
    for(unsigned i=0;i<200 && s->fishing.phase==HMBTN_FISHING_CAST;++i) CHECK(step(s,0,HMBTN_PAD_SQUARE,error,sizeof(error)));
    CHECK(s->fishing.phase==HMBTN_FISHING_WAIT && s->fishing.water);
    CHECK(capture(s,&host,shots,"fishing-wait",error,sizeof(error)));
    CHECK(step(s,0,0,error,sizeof(error)) && s->fishing.phase==HMBTN_FISHING_REEL);
    for(unsigned i=0;i<200 && s->fishing.phase!=HMBTN_FISHING_FREE;++i) CHECK(step(s,0,0,error,sizeof(error)));
    CHECK(s->fishing.phase==HMBTN_FISHING_FREE && !s->house.held_item.id);
    unsigned caught=0;
    for(unsigned cast=0;cast<24 && !caught;++cast) {
        memcpy(s->intro.player.position,shore,sizeof(shore));s->intro.player.facing=facing;
        CHECK(step(s,HMBTN_PAD_SQUARE,HMBTN_PAD_SQUARE,error,sizeof(error)));
        for(unsigned i=0;i<5000 && s->fishing.phase!=HMBTN_FISHING_HOOKED;++i) {
            CHECK(step(s,HMBTN_PAD_START,HMBTN_PAD_SQUARE|HMBTN_PAD_UP,error,sizeof(error)));
            CHECK(!memcmp(shore,s->intro.player.position,sizeof(shore)) && s->menu.mode==HMBTN_MENU_NONE);
        }
        CHECK(s->fishing.phase==HMBTN_FISHING_HOOKED);
        CHECK(capture(s,&host,shots,"fishing-hooked",error,sizeof(error)));
        CHECK(step(s,0,0,error,sizeof(error)));
        for(unsigned i=0;i<300 && s->fishing.phase!=HMBTN_FISHING_FREE;++i) CHECK(step(s,0,0,error,sizeof(error)));
        CHECK(s->fishing.phase==HMBTN_FISHING_FREE);
        const unsigned item=s->house.held_item.id;
        if(item>=161 && item<=163) { caught=item;break; }
        if(item) {
            CHECK(step(s,HMBTN_PAD_CROSS,0,error,sizeof(error)));
            for(unsigned i=0;i<200 && (s->carry.phase || s->carry.thrown[0].item.id);++i) CHECK(step(s,0,0,error,sizeof(error)));
            CHECK(!s->house.held_item.id);
        }
    }
    CHECK(caught);CHECK(capture(s,&host,shots,"fishing-caught",error,sizeof(error)));
    /* The caught item really reaches a shipping surface and pays at 17:58. */
    const unsigned gold=s->menu.stats.gold;const int price=hmbtn_farm_item(caught)->sell_price;
    s->village.flags[0x5E]=true;s->fishing.streak=11;
    CHECK(load_area(s,s->data_root,0,error,sizeof(error)));s->intro.area=0;s->first_visit_started=true;
    CHECK(!s->village.flags[0x5E] && !s->fishing.streak);
    int32_t bin[3];CHECK(box(&s->scene.terrain,bin));int y=-300,v=0;unsigned ticks=0;
    do { y+=v;v+=9;if(v>45) v=45;++ticks; } while(y<bin[1] && ticks<100);
    s->intro.player.position[0]=bin[0]-(int)ticks*44;s->intro.player.position[1]=0;s->intro.player.position[2]=bin[2];s->intro.player.facing=3;
    CHECK(step(s,HMBTN_PAD_CROSS,0,error,sizeof(error)) && s->carry.phase);
    for(unsigned i=0;i<200 && (s->carry.phase || s->carry.thrown[0].item.id);++i) CHECK(step(s,0,0,error,sizeof(error)));
    CHECK(s->shipping.pending_value==(unsigned)price);
    s->clock.hour=17;s->clock.minute=57;s->clock.frames=14;CHECK(step(s,0,0,error,sizeof(error)));
    CHECK(s->menu.stats.gold==gold+(unsigned)price && s->shipping.fish_shipped==1);
    s->shipping.fish_shipped=199;world_clock_events(s,&s->clock,0);CHECK(!s->village.flags[0x5F]);
    s->shipping.fish_shipped=200;world_clock_events(s,&s->clock,0);CHECK(s->village.flags[0x5F]);
    s->shipping.fish_shipped=1;s->village.flags[0x5F]=false; /* End the threshold fixture. */
    /* Special-result fixtures run the actual ANI and complete item effects. */
    s->clock.season=3;s->clock.hour=7;s->clock.minute=0;sync_clock(s);
    CHECK(load_area(s,s->data_root,56,error,sizeof(error)));s->intro.area=56;
    memcpy(s->intro.player.position,shore,sizeof(shore));memcpy(s->intro.camera,shore,sizeof(shore));s->intro.player.facing=facing;
    CHECK(load_action_bank(s,10,error,sizeof(error)));s->intro.action_active=true;
    s->fishing.phase=HMBTN_FISHING_CATCH;s->fishing.kind=0;s->fishing.random=54;s->fishing.using_action=true;
    s->intro.player.sprite=(HmbtnAnimationPlayer){(uint16_t)(facing+4),0,0};s->intro.player.animation_done=false;
    for(unsigned i=0;i<300 && s->fishing.phase!=HMBTN_FISHING_FREE;++i) CHECK(step(s,0,0,error,sizeof(error)));
    CHECK(s->fishing.phase==HMBTN_FISHING_FREE && s->village.flags[0x53] && s->farm_action.max_stamina==110
        && s->farm_action.stamina==110 && s->menu.stats.power_berries==1 && !s->house.held_item.id);
    s->clock.hour=22;s->clock.minute=1;sync_clock(s);CHECK(load_action_bank(s,10,error,sizeof(error)));s->intro.action_active=true;
    s->fishing.phase=HMBTN_FISHING_CATCH;s->fishing.random=23;s->fishing.using_action=true;
    s->intro.player.sprite=(HmbtnAnimationPlayer){(uint16_t)(facing+4),0,0};s->intro.player.animation_done=false;
    bool print=false;
    for(unsigned i=0;i<500 && s->fishing.phase!=HMBTN_FISHING_FREE;++i) {
        CHECK(step(s,0,0,error,sizeof(error)));
        if(s->fishing.phase==HMBTN_FISHING_PRINT_HOLD && !print) { print=true;CHECK(capture(s,&host,shots,"fishing-print",error,sizeof(error))); }
    }
    CHECK(print && s->fishing.phase==HMBTN_FISHING_FREE && s->village.flags[0x58] && !s->house.held_item.id);
    /* Dawn interrupts a live cast and preserves permanent stamina/catch state. */
    CHECK(load_action_bank(s,10,error,sizeof(error)));s->intro.action_active=true;
    s->fishing.phase=HMBTN_FISHING_WAIT;s->fishing.using_action=true;s->fishing.marker_visible=true;s->fishing.wait=60;
    s->intro.player.sprite=(HmbtnAnimationPlayer){8,(uint16_t)facing,0};
    s->clock.hour=5;s->clock.minute=59;s->clock.frames=14;
    CHECK(step(s,0,HMBTN_PAD_SQUARE,error,sizeof(error)) && s->overnight_tick);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,0,error,sizeof(error)));
    CHECK(s->area==80 && s->fishing.phase==HMBTN_FISHING_FREE && !s->fishing.marker_visible
        && !s->intro.action_active && s->farm_action.max_stamina==110 && s->village.flags[0x58]);
    s->farm_action.stamina=1;s->house.mode=HMBTN_HOUSE_SLEEP;const unsigned day=s->clock.day;
    for(unsigned i=0;i<1200 && (s->clock.day==day || s->morning_fade);++i) CHECK(step(s,0,0,error,sizeof(error)));
    CHECK(s->clock.day!=day && !s->sleep.active && s->farm_action.max_stamina==110 && s->farm_action.stamina==110);
    puts("Greg rod -> real water cast/cancel/bite/catch -> shipping payout; berry and released king/print ANI passed.");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

/* Rod, fish and food are obtained by gameplay; only starting money is a fixture. */
#include "poultry_helpers.h"
#include "field_helpers.h"
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
static bool held(HmbtnScenePreview *s,uint32_t pressed,uint32_t buttons,char *error,size_t size)
{ return hmbtn_scene_preview_tick(s,(HmbtnLogicalInput){pressed,buttons},error,size); }
static bool wait_items(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned t=0;t<400 && (g->carry.phase || g->feed.phase || g->carry.thrown[0].item.id || g->quick_ticks);++t) REQUIRE(step(s,0,error,size));
    REQUIRE(!g->carry.phase && !g->feed.phase && !g->carry.thrown[0].item.id && !g->quick_ticks);return true;
}
static bool close_shop(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned t=0;t<3000 && (g->feed_shop.phase || g->event_item.phase);++t) REQUIRE(step(s,g->event_item.phase ? 0 : HMBTN_PAD_CROSS,error,size));
    REQUIRE(!g->feed_shop.phase && !g->event_item.phase && !g->game.inventory.held_item.id);return true;
}
static bool counter(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const HmbtnGatherRecord *record=NULL;
    for(unsigned i=0;i<HMBTN_GATHER_RECORDS;++i) { const HmbtnGatherRecord *r=hmbtn_farm_gather_record(i);if(r->location==30 && r->item==132) { record=r;break; } }
    REQUIRE(record && g->village.people[1].actor.visible);
    memcpy(g->intro.player.position,record->position,sizeof(record->position));g->intro.player.position[0]-=400;g->intro.player.position[1]=0;g->intro.player.facing=3;g->control.mode=0;
    memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));memcpy(g->intro.camera,g->intro.player.position,sizeof(g->intro.camera));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->feed_shop.fish_food && g->event_item.phase==HMBTN_EVENT_ITEM_SHOW);
    REQUIRE(shop_wait(s,HMBTN_SHOP_GREETING,error,size) && g->feed_shop.request==0xd0 && g->game.inventory.held_item.id==108);
    REQUIRE(shop_wait(s,HMBTN_SHOP_COUNT,error,size));return true;
}
static bool object(HmbtnScenePreview *s,unsigned kind,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<50;++i) {
        const uint8_t *z=hmbtn_scene_preview_resources(s).section+16+i*44;
        if(!(hmbtn_read_le16(z)&0x4000u) || hmbtn_read_le16(z+2)!=kind) continue;
        at(s,z);const unsigned face=hmbtn_read_le16(z+8);REQUIRE(face<4);
        g->intro.player.position[face&1 ? 0 : 2]+=face<2 ? 224 : -224;
        memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));memcpy(g->intro.camera,g->intro.player.position,sizeof(g->intro.camera));
        return step(s,HMBTN_PAD_CROSS,error,size) && wait_items(s,error,size);
    }
    return false;
}
static bool pond_throw(HmbtnScenePreview *s,HmbtnHost *host,const char *shots,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const HmbtnModel *m=&hmbtn_scene_preview_resources(s).scene->terrain;
    const unsigned item=g->game.inventory.held_item.id,count=hmbtn_pond_count(&g->pond);REQUIRE(item);
    bool placed=false;
    for(unsigned ob=0;ob<m->object_count && !placed;++ob) {
        HmbtnModelObject o;REQUIRE(hmbtn_model_object(m,ob,&o));size_t at=12+o.primitives;
        for(unsigned n=0;n<o.primitive_count && !placed;++n) {
            HmbtnModelPrimitive p;REQUIRE(hmbtn_model_primitive(m,at,&p));at+=p.bytes;if(!(p.flags&4u)) continue;
            int32_t wet[3]={0};for(unsigned v=0;v<p.count;++v) { int32_t q[3];hmbtn_model_vertex(m,&o,p.vertex[v],q);for(unsigned a=0;a<3;++a) wet[a]+=q[a]; }
            for(unsigned a=0;a<3;++a) wet[a]/=p.count;
            int y=-300,velocity=0;unsigned ticks=0;do { y+=velocity;velocity+=9;if(velocity>45) velocity=45;++ticks; } while(y<wet[1] && ticks<100);
            for(unsigned face=0;face<4 && !placed;++face) {
                int32_t dry[3]={wet[0],0,wet[2]};dry[face&1 ? 0 : 2]+=face<2 ? (int)ticks*44 : -(int)ticks*44;
                HmbtnGroundHit hit;if(!hmbtn_ground_find(m,dry,0xE087,4,&hit) || (hit.flags&0x87u) || hit.height) continue;
                memcpy(g->intro.player.position,dry,sizeof(dry));g->intro.player.facing=face;g->control.mode=0;
                memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));memcpy(g->intro.camera,dry,sizeof(dry));placed=true;
            }
        }
    }
    REQUIRE(placed && step(s,HMBTN_PAD_CROSS,error,size) && g->carry.phase);
    bool splash=false;
    for(unsigned t=0;t<400 && (g->carry.phase || g->carry.thrown[0].item.id);++t) {
        REQUIRE(step(s,0,error,size));
        if(g->carry.thrown[0].phase==3 && g->carry.thrown[0].item.id && !splash) {
            REQUIRE(g->carry.thrown[0].splash.animation==23 && capture(s,host,shots,"pond-splash",error,size));splash=true;
        }
    }
    REQUIRE(splash && !g->game.inventory.held_item.id && !g->carry.thrown[0].item.id);
    REQUIRE(item==132 ? (g->pond.fed && hmbtn_pond_count(&g->pond)==count) : hmbtn_pond_count(&g->pond)==count+1);return true;
}
static bool checkpoint(HmbtnScenePreview **s,HmbtnHost *host,const char *root,const char *base,const char *suffix,char *error,size_t size)
{ char path[1024];if(snprintf(path,sizeof(path),"%s%s",base,suffix)>=(int)sizeof(path)) return false;return field_reload(s,host,root,path,error,size); }
int main(int argc,char **argv)
{
    if(argc<3 || argc>4) return 2;const char *shots=argc==4 ? argv[3] : NULL;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock date={0,0,5,5,7,0,0};s=hmbtn_scene_area_create_at(host.renderer,argv[1],56,&date,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->game.economy.gold=2000;g->first_visit_started=true;
    g->village.flags[251+288]=g->village.flags[251+140]=true;
    const HmbtnIntroActor *greg=&g->village.people[32].actor;CHECK(greg->visible);
    memcpy(g->intro.player.position,greg->position,sizeof(greg->position));g->intro.player.position[2]+=300;g->intro.player.facing=0;
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && g->talk.active);
    for(unsigned i=0;i<3000 && (g->talk.active || g->quick_ticks);++i) CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(!g->talk.active && g->game.inventory.equipped.id==56 && g->village.flags[0xe7]);
    int32_t shore[3];unsigned face;CHECK(cast_point(s,shore,&face));unsigned caught=0;
    for(unsigned cast=0;cast<24 && !caught;++cast) {
        memcpy(g->intro.player.position,shore,sizeof(shore));g->intro.player.facing=face;
        CHECK(held(s,HMBTN_PAD_SQUARE,HMBTN_PAD_SQUARE,error,sizeof(error)));
        for(unsigned t=0;t<5000 && g->fishing.phase!=HMBTN_FISHING_HOOKED;++t) CHECK(held(s,0,HMBTN_PAD_SQUARE,error,sizeof(error)));
        CHECK(g->fishing.phase==HMBTN_FISHING_HOOKED && held(s,0,0,error,sizeof(error)));
        for(unsigned t=0;t<400 && g->fishing.phase;++t) CHECK(step(s,0,error,sizeof(error)));
        CHECK(!g->fishing.phase);const unsigned item=g->game.inventory.held_item.id;
        if(item>=161 && item<=163) { caught=item;break; }
        if(item) CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && wait_items(s,error,sizeof(error)));
    }
    CHECK(caught && capture(s,&host,shots,"pond-caught",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_R2,error,sizeof(error)) && wait_items(s,error,sizeof(error)) && !g->game.inventory.held_item.id);
    /* Advance to a real open supermarket time; no clock/date field is replaced. */
    if(g->game.clock.hour>=17) hmbtn_session_advance_minutes(g,(24-g->game.clock.hour)*60-g->game.clock.minute+600);
    if(g->game.clock.hour<9) hmbtn_session_advance_minutes(g,540-g->game.clock.hour*60-g->game.clock.minute);
    while(g->game.clock.weekday==0 || g->game.clock.weekday==2) hmbtn_session_advance_minutes(g,1440);
    CHECK(hmbtn_scene_preview_load_area(s,180,error,sizeof(error)));for(unsigned t=0;t<110;++t) CHECK(step(s,0,error,sizeof(error)));
    CHECK(counter(s,error,sizeof(error)) && step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && close_shop(s,error,sizeof(error)) && g->game.economy.gold==2000);
    CHECK(counter(s,error,sizeof(error)));g->game.economy.gold=19;
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && g->feed_shop.request==0x4f);
    CHECK(shop_wait(s,HMBTN_SHOP_COUNT,error,sizeof(error)) && !g->game.inventory.supplies[3] && g->game.economy.gold==19);g->game.economy.gold=2000;
    CHECK(step(s,HMBTN_PAD_TRIANGLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_INFO,error,sizeof(error)) && g->feed_shop.request==0x25b && shop_wait(s,HMBTN_SHOP_COUNT,error,sizeof(error)));
    for(unsigned i=0;i<4;++i) CHECK(step(s,HMBTN_PAD_UP,error,sizeof(error)));
    CHECK(g->feed_shop.quantity==41 && capture(s,&host,shots,"pond-food-purchase",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && g->feed_shop.request==0xd3);
    CHECK(g->game.economy.gold==2000 && !g->game.inventory.supplies[3]);
    CHECK(close_shop(s,error,sizeof(error)) && g->game.economy.gold==1180 && g->game.inventory.supplies[3]==41);
    CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)));for(unsigned t=0;t<110;++t) CHECK(step(s,0,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_R2,error,sizeof(error)) && wait_items(s,error,sizeof(error)) && g->game.inventory.held_item.id==caught);
    CHECK(pond_throw(s,&host,shots,error,sizeof(error)) && !g->pond.fed && hmbtn_pond_count(&g->pond)==1);
    /* An unfed night preserves age. */
    const unsigned first_age=g->pond.fish[0].days;CHECK(sleep_day(s,error,sizeof(error)) && g->pond.fish[0].days==first_age);
    CHECK(object(s,10,error,sizeof(error)) && g->game.inventory.held_item.id==132 && g->game.inventory.supplies[3]==40);
    CHECK(pond_throw(s,&host,shots,error,sizeof(error)) && checkpoint(&s,&host,argv[1],argv[2],".fed",error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    unsigned fed_days=0;
    do {
        CHECK(sleep_day(s,error,sizeof(error)) && !g->pond.fed);++fed_days;
        const uint32_t random=g->pond.random;const unsigned age=g->pond.fish[0].days;
        hmbtn_session_clock_events(g,&g->game.clock,HMBTN_CLOCK_DAY);CHECK(g->pond.fish[0].days==age && g->pond.random==random);
        if(fed_days==10 && first_age==0) CHECK(g->pond.fish[0].item==162 && age==10);
        if(fed_days==20 && first_age==0) CHECK(g->pond.fish[0].item==161 && age==20);
        if(hmbtn_pond_count(&g->pond)>1) break;
        CHECK(fed_days<40 && object(s,10,error,sizeof(error)) && pond_throw(s,&host,shots,error,sizeof(error)));
    } while(fed_days<40);
    CHECK(hmbtn_pond_count(&g->pond)>1 && g->pond.fish[0].item==161 && g->game.inventory.supplies[3]==41-fed_days);
    CHECK(checkpoint(&s,&host,argv[1],argv[2],".grown",error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    const unsigned count=hmbtn_pond_count(&g->pond);CHECK(object(s,45,error,sizeof(error)) && g->game.inventory.held_item.id==161 && hmbtn_pond_count(&g->pond)==count-1);
    CHECK(capture(s,&host,shots,"pond-withdrawn",error,sizeof(error)) && ship(s,error,sizeof(error)));
    const unsigned now=g->game.clock.hour*60+g->game.clock.minute,gold=g->game.economy.gold;CHECK(now<1078);
    hmbtn_session_advance_minutes(g,1078-now);CHECK(g->game.economy.gold==gold+200 && g->game.economy.shipping.fish_shipped==1);
    CHECK(checkpoint(&s,&host,argv[1],argv[2],"",error,sizeof(error)));
    printf("Pond: Greg rod -> actual fish %u -> 41 bought food -> %u fed mornings/growth/spawn -> withdrawal/shipping/payment and stable resumes passed\n",caught,fed_days);
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

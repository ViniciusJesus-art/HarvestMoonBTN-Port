/* Begins with the caught fish and purchased feed of pond_playthrough. Money
 * for the next feed lot is explicit; no fish or fishing pole is granted. */
#include "pond_helpers.h"
static unsigned poles(const HmbtnInventory *h) { return (unsigned)hmbtn_inventory_count(h,57,0,2); }
static bool wait_greg(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned t=0;t<2000 && !(g->visit.active && g->visit.event==177 && g->visit.free && !g->intro.message);++t) REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
    REQUIRE(g->visit.active && g->visit.event==177 && g->visit.free && !g->intro.message && !g->intro.script.boundary);
    REQUIRE(hmbtn_village_event_actor(&g->village,32)==&g->visit.actors[32] && !hmbtn_save_available(g,error,size));error[0]=0;return true;
}
static bool receive(HmbtnScenePreview *s,HmbtnHost *host,const char *shots,bool quiet,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const HmbtnStoredTool hand=g->game.inventory.held_item;
    const unsigned friendship=g->village.people[32].friendship;
    HmbtnIntroActor *actor=&g->visit.actors[32];memcpy(g->intro.player.position,actor->position,sizeof(actor->position));g->intro.player.position[2]+=300;
    g->intro.player.facing=0;g->control.mode=0;memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));bool staged=false;unsigned shown=0;
    for(unsigned t=0;t<4000 && g->visit.active;++t) {
        if(g->visit.prop.visible && !staged) { REQUIRE(g->visit.prop.item==57 && capture(s,host,shots,"greg-pole-offer",error,size));staged=true; }
        if(g->event_item.phase==HMBTN_EVENT_ITEM_TOOL) {
            REQUIRE(g->event_item.shown==57 && g->intro.player.sprite.animation==71 && poles(&g->game.inventory)==1);
            if(!shown) REQUIRE(capture(s,host,shots,"greg-pole-received",error,size));++shown;
            REQUIRE(step(s,HMBTN_PAD_START|HMBTN_PAD_R2|HMBTN_PAD_SQUARE,error,size) && g->menu.mode==HMBTN_MENU_NONE);
        } else REQUIRE(step(s,g->intro.message ? HMBTN_PAD_CROSS : 0,error,size));
    }
    REQUIRE(staged && shown==(quiet ? 0u : 11u) && !g->visit.active && !g->intro.script.boundary && !g->event_item.phase);
    REQUIRE(poles(&g->game.inventory)==1 && g->village.flags[0x66] && g->village.flags[251+177] && !hmbtn_village_event_actor(&g->village,32));
    REQUIRE(g->village.people[32].friendship==(friendship>235 ? 255 : friendship+20));
    REQUIRE(g->game.inventory.held_item.id==hand.id && g->game.inventory.held_item.modifier==hand.modifier && g->game.inventory.held_item.count==hand.count);return true;
}
static bool reload(HmbtnScenePreview **scene,HmbtnHost *host,const char *root,const char *path,char *error,size_t size)
{
    hmbtn_scene_preview_destroy(*scene);*scene=hmbtn_scene_saved_create(host->renderer,root,path,error,size);return *scene!=NULL;
}
int main(int argc,char **argv)
{
    if(argc<4 || argc>5) return 2;const char *shots=argc==5 ? argv[4] : NULL;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};CHECK(hmbtn_host_open(&host));
    s=hmbtn_scene_saved_create(host.renderer,argv[1],argv[2],error,sizeof(error));CHECK(s);
    HmbtnSession *g=hmbtn_scene_preview_session(s);CHECK(g->area==0 && hmbtn_pond_count(&g->pond)>=3 && !poles(&g->game.inventory));
    g->game.economy.gold=30000;CHECK(hmbtn_scene_preview_load_area(s,180,error,sizeof(error)));
    /* The checkpoint may be on a closing day; advance normally to an open day. */
    unsigned now=g->game.clock.hour*60+g->game.clock.minute;
    if(now>=1020) hmbtn_session_advance_minutes(g,1440-now+600);
    else if(now<540) hmbtn_session_advance_minutes(g,540-now);
    while(!g->game.clock.weekday || g->game.clock.weekday==2) hmbtn_session_advance_minutes(g,1440);
    for(unsigned t=0;t<110;++t) CHECK(step(s,0,error,sizeof(error)));
    CHECK(counter(s,error,sizeof(error)));
    for(unsigned i=0;i<30;++i) CHECK(step(s,HMBTN_PAD_UP,error,sizeof(error)));
    CHECK(g->feed_shop.quantity==301 && step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && close_shop(s,error,sizeof(error)));
    CHECK(g->game.inventory.supplies[3]>=301 && hmbtn_scene_preview_load_area(s,0,error,sizeof(error)));
    for(unsigned t=0;t<110;++t) CHECK(step(s,0,error,sizeof(error)));
    /* Keep tool slots full until the natural population reaches the goal. */
    CHECK(g->game.inventory.equipped.id==56 && !g->game.inventory.bag[0].id && !g->game.inventory.bag[1].id);
    for(unsigned i=0;i<2;++i) { g->game.inventory.bag[i]=g->game.inventory.toolbox[i];g->game.inventory.toolbox[i]=(HmbtnStoredTool){0}; }
    unsigned mornings=0;
    while(g->pond.morning_count<50 && mornings<300) {
        /* The installed winter surface is frozen. Wait through winter without
         * feed rather than granting a thaw or changing the season directly. */
        if(g->game.clock.season==3) { CHECK(sleep_day(s,error,sizeof(error)));continue; }
        CHECK(object(s,10,error,sizeof(error)) && pond_throw(s,&host,NULL,error,sizeof(error)) && sleep_day(s,error,sizeof(error)));
        ++mornings;CHECK(!g->visit.active && !g->village.flags[251+177] && !poles(&g->game.inventory));
    }
    CHECK(g->pond.morning_count>=50 && mornings<300 && g->game.clock.hour==6);
    /* A full tool bag defers the original entry trigger without consuming it. */
    CHECK(travel(s,80,error,sizeof(error)) && travel(s,0,error,sizeof(error)));
    for(unsigned t=0;t<10;++t) CHECK(step(s,0,error,sizeof(error)));
    CHECK(!g->visit.active && !g->village.flags[251+177]);
    CHECK(travel(s,80,error,sizeof(error)));
    g->game.inventory.toolbox[0]=g->game.inventory.bag[0];g->game.inventory.bag[0]=(HmbtnStoredTool){0};
    CHECK(checkpoint(&s,&host,argv[1],argv[3],".ready",error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    char ready_path[1024];CHECK(snprintf(ready_path,sizeof(ready_path),"%s.ready",argv[3])<(int)sizeof(ready_path));
    /* The same naturally achieved checkpoint exercises timeout and leaving
     * before talking. Each branch starts before the original entry trigger. */
    CHECK(travel(s,0,error,sizeof(error)) && wait_greg(s,error,sizeof(error)));
    CHECK(capture(s,&host,shots,"greg-pole-arrival",error,sizeof(error)));hmbtn_session_advance_minutes(g,180);
    for(unsigned t=0;t<2000 && g->visit.active;++t) CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(!g->visit.active && !g->intro.script.boundary && !poles(&g->game.inventory) && !g->village.flags[0x66] && g->village.flags[251+177]);
    CHECK(reload(&s,&host,argv[1],ready_path,error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(travel(s,0,error,sizeof(error)) && wait_greg(s,error,sizeof(error)) && travel(s,80,error,sizeof(error)));
    for(unsigned t=0;t<110;++t) CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(!g->visit.active && !poles(&g->game.inventory) && !g->village.flags[0x66]);
    /* Filled hands choose the script's quiet storage branch, preserving
     * purchased feed withdrawn through the actual farm depot. */
    CHECK(reload(&s,&host,argv[1],ready_path,error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(travel(s,0,error,sizeof(error)) && wait_greg(s,error,sizeof(error)));
    CHECK(object(s,10,error,sizeof(error)) && g->game.inventory.held_item.id==132);
    CHECK(receive(s,&host,shots,true,error,sizeof(error)));
    CHECK(checkpoint(&s,&host,argv[1],argv[3],".quiet",error,sizeof(error)));
    CHECK(reload(&s,&host,argv[1],ready_path,error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(travel(s,0,error,sizeof(error)) && wait_greg(s,error,sizeof(error)) && receive(s,&host,shots,false,error,sizeof(error)));
    CHECK(g->game.inventory.equipped.id==56 && g->game.inventory.bag[0].id==57);
    CHECK(travel(s,80,error,sizeof(error)) && travel(s,0,error,sizeof(error)) && !g->visit.active && poles(&g->game.inventory)==1);
    CHECK(hmbtn_inventory_cycle(&g->game.inventory,true,false) && g->game.inventory.equipped.id==57);
    const HmbtnFarmClock c=g->game.clock;CHECK(hmbtn_scene_preview_load_area(s,56,error,sizeof(error)));
    for(unsigned t=0;t<110;++t) CHECK(step(s,0,error,sizeof(error)));
    /* Use original water geometry as the search centre when Greg is absent. */
    const HmbtnModel *ground=&hmbtn_scene_preview_resources(s).scene->terrain;int32_t shore[3];unsigned face=0;bool shore_found=false;
    for(unsigned ob=0;ob<ground->object_count && !shore_found;++ob) {
        HmbtnModelObject o;CHECK(hmbtn_model_object(ground,ob,&o));size_t at=12+o.primitives;
        for(unsigned n=0;n<o.primitive_count && !shore_found;++n) {
            HmbtnModelPrimitive p;CHECK(hmbtn_model_primitive(ground,at,&p));at+=p.bytes;if(!(p.flags&2u) || (p.flags&0x44u)) continue;
            int32_t centre[3]={0};for(unsigned v=0;v<p.count;++v) { int32_t q[3];hmbtn_model_vertex(ground,&o,p.vertex[v],q);for(unsigned a=0;a<3;++a) centre[a]+=q[a]; }
            for(unsigned a=0;a<3;++a) centre[a]/=p.count;
            shore_found=cast_point_near(s,centre,shore,&face);
        }
    }
    CHECK(shore_found);memcpy(g->intro.player.position,shore,sizeof(shore));g->intro.player.facing=face;
    CHECK(held(s,HMBTN_PAD_SQUARE,HMBTN_PAD_SQUARE,error,sizeof(error)));
    for(unsigned t=0;t<5000 && g->fishing.phase!=HMBTN_FISHING_HOOKED;++t) CHECK(held(s,0,HMBTN_PAD_SQUARE,error,sizeof(error)));
    CHECK(g->fishing.phase==HMBTN_FISHING_HOOKED && held(s,0,0,error,sizeof(error)));
    for(unsigned t=0;t<500 && g->fishing.phase;++t) CHECK(step(s,0,error,sizeof(error)));
    CHECK(!g->fishing.phase && g->game.inventory.equipped.id==57 && (g->game.inventory.held_item.id || g->fishing.catch_flags));
    CHECK(capture(s,&host,shots,"greg-pole-fishing",error,sizeof(error)) && checkpoint(&s,&host,argv[1],argv[3],"",error,sizeof(error)));
    printf("Greg pole: naturally bred %u fish after %u more fed mornings, full bag deferral, timeout/exit, quiet/visible delivery, no duplicate, actual pole catch and resume passed (%u h)\n",g->pond.morning_count,mornings,c.hour);
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

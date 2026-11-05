/* Real installed Barley scripts. Fixtures: date, gate/player poses and the
 * fallback's cultivation threshold; never manufacture a pony/event cursor. */
#include "poultry_helpers.h"
#include "field_helpers.h"
#include "horse_helpers.h"
static bool trigger_checks(HmbtnScenePreview *s,unsigned event,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);HmbtnVillage *v=&g->village;
    const bool done=v->flags[251+event];v->flags[251+event]=false;
    HmbtnFarmClock c=g->game.clock;
    const unsigned previous=event==1 ? 42u : 2u;
    REQUIRE(hmbtn_farm_event_trigger(&g->triggers,v,&c,0,previous,2,0,&g->game.inventory)==event);
    g->horse.owned=true;REQUIRE(hmbtn_farm_event_trigger(&g->triggers,v,&c,0,previous,2,0,&g->game.inventory)!=event);g->horse.owned=false;
    REQUIRE(hmbtn_farm_event_trigger(&g->triggers,v,&c,0,previous,0,0,&g->game.inventory)!=event);
    if(event==1) {
        c.year=1;REQUIRE(hmbtn_farm_event_trigger(&g->triggers,v,&c,0,42,2,0,&g->game.inventory)!=event);c=g->game.clock;
        c.season=1;REQUIRE(hmbtn_farm_event_trigger(&g->triggers,v,&c,0,42,2,0,&g->game.inventory)!=event);c=g->game.clock;
        c.hour=7;REQUIRE(hmbtn_farm_event_trigger(&g->triggers,v,&c,0,42,2,0,&g->game.inventory)!=event);c=g->game.clock;
        REQUIRE(hmbtn_farm_event_trigger(&g->triggers,v,&c,1,42,2,0,&g->game.inventory)!=event);
        REQUIRE(hmbtn_farm_event_trigger(&g->triggers,v,&c,0,29,2,0,&g->game.inventory)!=event);
    } else {
        const HmbtnFarmTile grown=g->field.cells[0];g->field.cells[0]=(HmbtnFarmTile){0};
        REQUIRE(hmbtn_farm_event_trigger(&g->triggers,v,&c,0,2,2,0,&g->game.inventory)!=event);
        const HmbtnFarmTile inside=g->greenhouse.cells[0];g->greenhouse.cells[0]=grown;
        REQUIRE(hmbtn_farm_event_trigger(&g->triggers,v,&c,0,2,2,0,&g->game.inventory)==event);
        g->greenhouse.cells[0]=inside;g->field.cells[0]=grown;
    }
    v->flags[251+event]=done;return true;
}
static void near_pony(HmbtnSession *g)
{ memcpy(g->intro.player.position,g->horse.actor.position,sizeof(g->intro.player.position));g->intro.player.position[2]+=400;g->intro.player.facing=0;g->control.mode=0; }
int main(int argc,char **argv)
{
    if(argc<3 || argc>4) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};const char *shots=argc==4 ? argv[3] : NULL;
    CHECK(hmbtn_host_open(&host));const HmbtnFarmClock date={0,0,4,4,10,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],52,&date,error,sizeof(error));CHECK(s);
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned t=0;t<110;++t) CHECK(step(s,0,error,sizeof(error)));
    CHECK(travel(s,28,error,sizeof(error)));
    CHECK(trigger_checks(s,1,error,sizeof(error)));
    CHECK(encounter(s,&host,shots,1,true,error,sizeof(error)) && !g->horse.owned);
    /* The event remains free at the ranch: ask Barley again before leaving.
     * Its done flag means leaving after refusing does not restart this offer. */
    const HmbtnIntroActor *barley=hmbtn_village_actor(&g->village,15);
    memcpy(g->intro.player.position,barley->position,sizeof(g->intro.player.position));g->intro.player.position[2]+=300;
    g->intro.player.facing=0;g->control.mode=0;
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && encounter(s,&host,NULL,1,false,error,sizeof(error)));
    CHECK(g->horse.name[0]!=0xffff && g->horse.area==92 && g->horse.affection==10);
    CHECK(travel(s,52,error,sizeof(error)));
    for(unsigned t=0;t<20 && g->visit.active;++t) CHECK(step(s,0,error,sizeof(error)));
    CHECK(!g->visit.active);
    CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)) && travel(s,92,error,sizeof(error)));
    near_pony(g);CHECK(step(s,0,error,sizeof(error)));
    CHECK(capture(s,&host,shots,"pony-stable",error,sizeof(error)));
    near_pony(g);CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && g->house.request==600);
    for(unsigned t=0;t<1000 && g->house.mode!=HMBTN_HOUSE_FREE;++t) CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(!g->horse.called_today && g->horse.affection==10);
    /* Shared brush tool; purchasing is independently covered by animal_tools. */
    g->game.inventory.equipped=(HmbtnStoredTool){0x1c,0,1};
    for(unsigned repeat=0;repeat<2;++repeat) {
        near_pony(g);CHECK(step(s,HMBTN_PAD_SQUARE,error,sizeof(error)));
        for(unsigned t=0;t<150 && g->farm_action.phase!=HMBTN_FARM_ACTION_FREE;++t) CHECK(step(s,0,error,sizeof(error)));
        CHECK(g->horse.brushed_today && g->horse.affection==10);
    }
    g->intro.player.position[0]=-500;g->intro.player.position[2]=-500;
    for(unsigned tries=0;tries<20 && !g->horse.called_today;++tries) {
        CHECK(step(s,HMBTN_PAD_L2,error,sizeof(error)) && g->horse.whistling);
        for(unsigned t=0;t<200 && g->horse.whistling;++t) CHECK(step(s,0,error,sizeof(error)));
    }
    CHECK(g->horse.called_today && !g->horse.whistling);
    CHECK(step(s,0,error,sizeof(error)) && hmbtn_scene_preview_resources(s).xa->track==14);
    const int32_t old_x=g->horse.actor.position[0],old_z=g->horse.actor.position[2];
    bool moved=false;
    for(unsigned t=0;t<300;++t) {
        CHECK(step(s,0,error,sizeof(error)));
        moved|=g->horse.actor.position[0]!=old_x || g->horse.actor.position[2]!=old_z;
    }
    CHECK(moved && capture(s,&host,shots,"pony-walking",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)));HmbtnHorse frozen=g->horse;
    for(unsigned t=0;t<30;++t) CHECK(step(s,0,error,sizeof(error)));
    CHECK(!memcmp(&frozen,&g->horse,sizeof(frozen)) && capture(s,&host,shots,"pony-status",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)));
    for(unsigned t=0;t<30;++t) CHECK(step(s,0,error,sizeof(error)));
    char inside[1024];snprintf(inside,sizeof(inside),"%s.inside",argv[2]);
    CHECK(field_reload(&s,&host,argv[1],inside,error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(travel(s,0,error,sizeof(error)));g->game.weather.today=g->game.weather.tomorrow=0;
    CHECK(sleep_day(s,error,sizeof(error)) && g->horse.affection==14 && g->horse.area==92);
    CHECK(field_reload(&s,&host,argv[1],argv[2],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(!g->horse.brushed_today && !g->horse.called_today);
    /* Alternative installed delivery after 200 cultivated cells. */
    hmbtn_scene_preview_destroy(s);s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&date,error,sizeof(error));CHECK(s);g=hmbtn_scene_preview_session(s);
    for(unsigned t=0;t<110;++t) CHECK(step(s,0,error,sizeof(error)));
    CHECK(travel(s,80,error,sizeof(error)));
    for(unsigned i=0;i<200;++i) g->field.cells[i]=(HmbtnFarmTile){.value=0x36,.kind=25};
    CHECK(travel(s,0,error,sizeof(error)) && trigger_checks(s,73,error,sizeof(error)) && encounter(s,&host,shots,73,true,error,sizeof(error)));
    CHECK(!g->horse.owned && g->village.flags[251+73]);
    CHECK(travel(s,80,error,sizeof(error)) && travel(s,0,error,sizeof(error)) && !g->visit.active);
    hmbtn_scene_preview_destroy(s);s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&date,error,sizeof(error));CHECK(s);g=hmbtn_scene_preview_session(s);
    for(unsigned t=0;t<110;++t) CHECK(step(s,0,error,sizeof(error)));
    CHECK(travel(s,80,error,sizeof(error)));
    for(unsigned i=0;i<200;++i) g->field.cells[i]=(HmbtnFarmTile){.value=0x36,.kind=25};
    CHECK(travel(s,0,error,sizeof(error)) && encounter(s,&host,NULL,73,false,error,sizeof(error)));
    CHECK(g->horse.owned && g->horse.area==92 && g->horse.name[0]!=0xffff);
    puts("Pony: original ranch/farm offers and refusal/retry, mandatory naming, stable art, talk/brush/L2, pause/status, real doors/morning and save/reopen passed");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

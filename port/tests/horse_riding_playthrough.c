/* Fixtures: original acquisition date, anniversary eve and affinity at the
 * inspection boundary. Events, growth, ANI, movement and doors run normally. */
#include "poultry_helpers.h"
#include "field_helpers.h"
#include "horse_helpers.h"
static bool inspect(HmbtnScenePreview *s,HmbtnHost *host,const char *shots,unsigned event,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);bool seen=false,inside=false,hidden=false,nod=false;
    const unsigned friendship=g->village.people[15].friendship;
    for(unsigned t=0;t<9000;++t) {
        if(g->visit.active) {
            REQUIRE(g->visit.event==event);seen=true;inside|=g->area==92;
            hidden|=g->horse.in_event;nod|=g->intro.action_active;
            if(g->area==92 && g->intro.message) REQUIRE(capture(s,host,shots,event==2 ? "barley-reclaims" : "barley-keeps",error,size));
        }
        if(seen && !g->visit.active) {
            REQUIRE(inside && hidden && g->control.enabled && !g->horse.in_event);
            REQUIRE(g->horse.owned==(event==3) && (event!=3 || nod));
            REQUIRE(g->village.flags[251+event]);
            REQUIRE(g->village.people[15].friendship==(event==3 ? friendship+30 : friendship>30 ? friendship-30 : 0));
            return true;
        }
        REQUIRE(step(s,g->intro.message ? HMBTN_PAD_CROSS : 0,error,size) && !g->intro.script.boundary);
    }
    fprintf(stderr,"Inspection stuck event=%u pc=%u area=%u action=%d\n",g->visit.event,g->intro.script.pc,g->area,g->intro.action_active);
    REQUIRE(false);return false;
}
static bool acquire(HmbtnScenePreview *s,HmbtnHost *host,char *error,size_t size)
{
    for(unsigned t=0;t<110;++t) REQUIRE(step(s,0,error,size));
    REQUIRE(travel(s,28,error,size) && encounter(s,host,NULL,1,false,error,size));
    REQUIRE(travel(s,52,error,size));
    for(unsigned t=0;t<30 && hmbtn_scene_preview_session(s)->visit.active;++t) REQUIRE(step(s,0,error,size));
    REQUIRE(!hmbtn_scene_preview_session(s)->visit.active && hmbtn_scene_preview_load_area(s,0,error,size));return true;
}
static bool mount(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    memcpy(g->intro.player.position,g->horse.actor.position,sizeof(g->intro.player.position));g->intro.player.position[2]+=400;
    g->intro.player.facing=0;g->control.mode=0;
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->horse.riding && g->horse.ride_phase==1 && g->horse.trained_today);
    REQUIRE(!hmbtn_save_available(g,error,size));error[0]=0;
    for(unsigned t=0;t<200 && g->horse.ride_phase;++t) REQUIRE(step(s,0,error,size));
    REQUIRE(g->horse.riding && !g->horse.ride_phase && g->intro.player.sprite.animation==54);return true;
}
static bool stable_entry(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const uint8_t *zone=hmbtn_scene_preview_resources(s).section+16+4*44;
    REQUIRE(g->area==0 && hmbtn_read_le16(zone+2)==92);at(s,zone);
    REQUIRE(step(s,0,error,size) && (g->door_tick || g->travel_tick));
    for(unsigned t=0;t<150 && (g->area!=92 || g->door_tick || g->travel_tick);++t) REQUIRE(step(s,0,error,size));
    REQUIRE(g->area==92 && g->horse.area==92 && g->horse.riding && !g->control.exit_pending);
    REQUIRE(!memcmp(g->horse.actor.position,g->intro.player.position,sizeof(g->intro.player.position)));return true;
}
int main(int argc,char **argv)
{
    if(argc<3 || argc>4) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};const char *shots=argc==4 ? argv[3] : NULL;
    CHECK(hmbtn_host_open(&host));const HmbtnFarmClock date={0,0,4,4,10,0,0};
    /* The threshold is inclusive on both triggers; event 2 comes first at 180. */
    for(unsigned event=2;event<=3;++event) {
        s=hmbtn_scene_area_create_at(host.renderer,argv[1],52,&date,error,sizeof(error));CHECK(s);
        CHECK(acquire(s,&host,error,sizeof(error)));HmbtnSession *g=hmbtn_scene_preview_session(s);
        CHECK(g->horse.year==0 && g->horse.day==4 && !g->horse.adult);
        g->game.clock=(HmbtnFarmClock){1,0,3,4,18,0,0};g->game.weather.today=g->game.weather.tomorrow=0;
        g->horse.affection=event==2 ? 181 : 182;
        CHECK(sleep_day(s,error,sizeof(error)) && g->horse.adult && g->horse.affection==(event==2 ? 180u : 181u));
        CHECK(inspect(s,&host,shots,event,error,sizeof(error)));
        if(event==2) { hmbtn_scene_preview_destroy(s);s=NULL;continue; }
        CHECK(travel(s,92,error,sizeof(error)) && capture(s,&host,shots,"adult-stable",error,sizeof(error)));
        CHECK(mount(s,error,sizeof(error)) && capture(s,&host,shots,"mounted-stable",error,sizeof(error)));
        /* Real movement uses the previous mode's original 112/73-unit speed. */
        bool open=false;
        for(int z=-800;z<=1000 && !open;z+=200) for(int x=-500;x<=800 && !open;x+=200) {
            int32_t a[3]={x-185,0,z},b[3]={x,0,z};
            if(hmbtn_farm_settle(NULL,&hmbtn_scene_preview_resources(s).scene->terrain,a,0xe080,200)
                && hmbtn_farm_settle(NULL,&hmbtn_scene_preview_resources(s).scene->terrain,b,0xe080,200)) {
                memcpy(g->intro.player.position,b,sizeof(b));open=true;
            }
        }
        CHECK(open);const int32_t start_x=g->intro.player.position[0];g->control.mode=0;
        CHECK(step(s,HMBTN_PAD_LEFT,error,sizeof(error)));
        CHECK(g->intro.player.position[0]==start_x-112 && g->intro.player.sprite.animation==60);
        CHECK(step(s,HMBTN_PAD_LEFT,error,sizeof(error)) && g->intro.player.position[0]==start_x-185);
        CHECK(step(s,HMBTN_PAD_RIGHT|HMBTN_PAD_CIRCLE,error,sizeof(error)) && g->intro.player.position[0]==start_x-112);
        CHECK(step(s,HMBTN_PAD_RIGHT|HMBTN_PAD_CIRCLE,error,sizeof(error)) && g->intro.player.position[0]==start_x);
        CHECK(g->horse.actor.sprite.animation==12 && step(s,0,error,sizeof(error)));
        const unsigned stamina=g->farm_action.stamina,equipped=g->game.inventory.equipped.id;
        CHECK(step(s,HMBTN_PAD_SQUARE|HMBTN_PAD_L1|HMBTN_PAD_L2|HMBTN_PAD_R1,error,sizeof(error)));
        CHECK(!g->dog.whistling && !g->horse.whistling && g->farm_action.phase==HMBTN_FARM_ACTION_FREE
            && g->farm_action.stamina==stamina && g->game.inventory.equipped.id==equipped);
        CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)));HmbtnHorse frozen=g->horse;
        for(unsigned t=0;t<30;++t) CHECK(step(s,0,error,sizeof(error)));
        CHECK(!memcmp(&frozen,&g->horse,sizeof(frozen)) && step(s,HMBTN_PAD_START,error,sizeof(error)));
        for(unsigned t=0;t<30;++t) CHECK(step(s,0,error,sizeof(error)));
        char path[1024];snprintf(path,sizeof(path),"%s.stable",argv[2]);
        CHECK(field_reload(&s,&host,argv[1],path,error,sizeof(error)));g=hmbtn_scene_preview_session(s);
        CHECK(g->horse.riding && travel(s,0,error,sizeof(error)) && g->horse.area==0);
        CHECK(capture(s,&host,shots,"mounted-farm",error,sizeof(error)));
        CHECK(field_reload(&s,&host,argv[1],argv[2],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
        /* Other doors and exits do not transfer a mounted player. */
        bool house=false,gate=false;
        for(unsigned i=0;i<50;++i) {
            const uint8_t *z=hmbtn_scene_preview_resources(s).section+16+i*44;
            const unsigned target=hmbtn_read_le16(z+2);
            unsigned logical,resource;
            if(!hmbtn_world_area(target,g->game.clock.season,&logical,&resource) || target==0 || target==92) continue;
            at(s,z);HmbtnHouseExit exit;
            if(target!=80 && !hmbtn_world_gate_at(hmbtn_scene_preview_resources(s).section,0x2000,g->intro.player.position,&exit)
                    && !hmbtn_house_exit_at(hmbtn_scene_preview_resources(s).section,0x2000,g->intro.player.position,&exit)) continue;
            CHECK(step(s,0,error,sizeof(error)));
            CHECK(g->horse.riding && g->area==0 && !g->travel_tick && !g->door_tick);
            house|=target==80;gate|=target!=80;
        }
        CHECK(house && gate);
        CHECK(g->horse.riding && stable_entry(s,error,sizeof(error)));
        CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && g->horse.ride_phase==2 && g->intro.player.sprite.animation==176);
        int32_t left[3];memcpy(left,g->horse.actor.position,sizeof(left));
        for(unsigned t=0;t<200 && g->horse.ride_phase;++t) CHECK(step(s,0,error,sizeof(error)));
        CHECK(!g->horse.riding && !g->horse.ride_phase && g->intro.player.sprite.animation==0);
        CHECK(!memcmp(left,g->horse.actor.position,sizeof(left)) && g->horse.trained_today);
        CHECK(capture(s,&host,shots,"dismounted-stable",error,sizeof(error)));
        const unsigned training=g->horse.training;
        CHECK(travel(s,0,error,sizeof(error)) && sleep_day(s,error,sizeof(error)) && g->horse.training==training+2 && !g->horse.trained_today);
        hmbtn_session_clock_events(g,&g->game.clock,HMBTN_CLOCK_DAY);
        CHECK(g->horse.training==training+2);
        snprintf(path,sizeof(path),"%s.after",argv[2]);CHECK(field_reload(&s,&host,argv[1],path,error,sizeof(error)));g=hmbtn_scene_preview_session(s);
        CHECK(travel(s,92,error,sizeof(error)) && mount(s,error,sizeof(error)) && travel(s,0,error,sizeof(error)));
        int32_t resting[3];memcpy(resting,g->horse.actor.position,sizeof(resting));
        g->game.clock.hour=5;g->game.clock.minute=59;g->game.clock.frames=14;
        CHECK(step(s,0,error,sizeof(error)) && g->overnight_tick);
        for(unsigned t=0;t<180 && (g->overnight_tick || g->morning_fade);++t) CHECK(step(s,0,error,sizeof(error)));
        CHECK(g->area==80 && !g->horse.riding && g->horse.area==0 && !g->horse.ride_phase);
        CHECK(!memcmp(resting,g->horse.actor.position,sizeof(resting)) && g->horse.training==training+4);
        snprintf(path,sizeof(path),"%s.dawn",argv[2]);CHECK(field_reload(&s,&host,argv[1],path,error,sizeof(error)));
        hmbtn_scene_preview_destroy(s);s=NULL;
    }
    puts("Adult horse: original acquisition/anniversary, Barley 180/181 outcomes, adult ANI, mount/walk/run/dismount, stable doors, training, pause and mounted saves passed");
    hmbtn_host_close(&host);return 0;
fail:hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

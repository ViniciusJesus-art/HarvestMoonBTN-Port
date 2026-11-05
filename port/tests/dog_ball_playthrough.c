/* Actual seasonal growth, Won script and ball flight/fetch. Date/player poses are fixtures. */
#include "poultry_helpers.h"
#include "field_helpers.h"
static bool pet_idle(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<180 && (g->dog.carry_phase || g->quick_ticks);++i) REQUIRE(step(s,0,error,size));
    REQUIRE(!g->dog.carry_phase && !g->quick_ticks);return true;
}
static bool offer(HmbtnScenePreview *s,HmbtnHost *host,const char *shots,bool decline,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);bool seen=false,selected=false,question=false,reply=false,receipt=false;
    for(unsigned i=0;i<2000;++i) {
        if(g->visit.active) { REQUIRE(g->visit.event==286);seen=true; }
        REQUIRE(!g->intro.script.boundary);
        HmbtnMessage *m=&g->dialogue.message;
        if(seen && !g->visit.active && !g->event_item.phase && g->game.inventory.held_item.id==101) return true;
        uint32_t button=g->intro.message ? HMBTN_PAD_CROSS : 0;
        if(g->intro.message==866 && m->phase==HMBTN_MESSAGE_WAIT_END && m->choices) {
            if(!question) { REQUIRE(capture(s,host,shots,"won-ball-question",error,size));question=true; }
            if(decline && !selected) { button=HMBTN_PAD_DOWN;selected=true; }
        }
        if(!reply && g->intro.message==(decline ? 868u : 867u) && m->phase==HMBTN_MESSAGE_WAIT_END) {
            REQUIRE(capture(s,host,shots,decline ? "won-ball-no" : "won-ball-yes",error,size));reply=true;
        }
        if(!receipt && g->event_item.shown==101) { REQUIRE(capture(s,host,shots,"won-ball-receipt",error,size));receipt=true; }
        REQUIRE(step(s,button,error,size));
    }
    fprintf(stderr,"Won stuck event=%u pc=%u message=%u item=%u\n",g->visit.event,g->intro.script.pc,g->intro.message,g->game.inventory.held_item.id);
    REQUIRE(false);return false;
}
static bool take_dog(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);REQUIRE(g->dog.adult && !g->game.inventory.held_item.id);
    memcpy(g->intro.player.position,g->dog.actor.position,sizeof(g->intro.player.position));g->intro.player.position[2]+=300;
    g->intro.player.facing=0;g->control.mode=0;
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->dog.held && pet_idle(s,error,size) && g->game.inventory.held_item.id==HMBTN_ADULT_DOG);return true;
}
static bool yard(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->intro.player.position[0]=-5000;g->intro.player.position[1]=0;g->intro.player.position[2]=-6200;
    g->intro.player.facing=0;g->control.mode=0;
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->dog.carry_phase==2 && pet_idle(s,error,size) && !g->dog.held);return true;
}
static bool ball_hand(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    if(g->game.inventory.held_item.id==101) return true;
    if(g->dog.ball.placed) {
        memcpy(g->intro.player.position,g->dog.ball.position,sizeof(g->intro.player.position));g->intro.player.position[2]+=224;
        g->intro.player.facing=0;g->control.mode=0;
        REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->dog.carry_phase==3 && pet_idle(s,error,size));
    } else for(unsigned i=0;i<8 && g->game.inventory.held_item.id!=101;++i) REQUIRE(step(s,HMBTN_PAD_R2,error,size) && pet_idle(s,error,size));
    REQUIRE(g->game.inventory.held_item.id==101 && !g->dog.ball.placed && !g->dog.held);return true;
}
static bool toss(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);REQUIRE(ball_hand(s,error,size));
    g->intro.player.position[0]=-5000;g->intro.player.position[1]=0;g->intro.player.position[2]=-6850;g->intro.player.facing=2;g->control.mode=0;
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->carry.phase && pet_idle(s,error,size));
    for(unsigned i=0;i<150 && g->carry.phase;++i) REQUIRE(step(s,0,error,size));
    REQUIRE(!g->carry.phase && !g->game.inventory.held_item.id);return true;
}
static bool fetch(HmbtnScenePreview **scene,HmbtnHost *host,const char *root,const char *checkpoint,const char *shots,char *error,size_t size)
{
    HmbtnScenePreview *s=*scene;HmbtnSession *g=hmbtn_scene_preview_session(s);bool mouth=false,paused=false;
    for(unsigned i=0;i<1600;++i) {
        REQUIRE(step(s,0,error,size));
        if(g->dog.fetch==3 && !mouth) { REQUIRE(!g->dog.ball.placed && capture(s,host,shots,"adult-ball-mouth",error,size));mouth=true; }
        if(g->dog.fetch==3 && checkpoint && !paused) {
            REQUIRE(step(s,HMBTN_PAD_START,error,size));HmbtnDog before=g->dog;
            for(unsigned tick=0;tick<30;++tick) REQUIRE(step(s,0,error,size));
            REQUIRE(!memcmp(&before,&g->dog,sizeof(before)) && step(s,HMBTN_PAD_START,error,size));
            for(unsigned tick=0;tick<30;++tick) REQUIRE(step(s,0,error,size));
            REQUIRE(field_reload(scene,host,root,checkpoint,error,size));s=*scene;g=hmbtn_scene_preview_session(s);paused=true;
        }
        if(mouth && !g->dog.fetch && g->dog.ball.placed) {
            REQUIRE(g->dog.trained_today && abs(g->dog.ball.position[0]-g->intro.player.position[0])<700
                && abs(g->dog.ball.position[2]-g->intro.player.position[2])<700);
            REQUIRE(!checkpoint || paused);REQUIRE(capture(s,host,shots,"adult-ball-returned",error,size));return true;
        }
    }
    fprintf(stderr,"Fetch stuck phase=%u fetch=%u time=%u pose=%u pos=%d,%d ball=%d,%d player=%d,%d\n",g->dog.phase,g->dog.fetch,g->dog.fetch_time,g->dog.actor.sprite.animation,g->dog.actor.position[0],g->dog.actor.position[2],g->dog.ball.position[0],g->dog.ball.position[2],g->intro.player.position[0],g->intro.player.position[2]);
    REQUIRE(false);return false;
}
static bool lose_in_water(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);REQUIRE(ball_hand(s,error,size));
    const HmbtnModel *m=&hmbtn_scene_preview_resources(s).scene->terrain;bool found=false;unsigned waters=0,dry=0,landed=0;
    static const int delta[4][2]={{0,-1},{-1,0},{0,1},{1,0}};
    for(unsigned ob=0;ob<m->object_count && !found;++ob) {
        HmbtnModelObject object;REQUIRE(hmbtn_model_object(m,ob,&object));size_t off=12+object.primitives;
        for(unsigned k=0;k<object.primitive_count && !found;++k) {
            HmbtnModelPrimitive p;REQUIRE(hmbtn_model_primitive(m,off,&p));off+=p.bytes;if(!(p.flags&7u)) continue;
            int32_t centre[3]={0};for(unsigned v=0;v<p.count;++v) { int32_t point[3];hmbtn_model_vertex(m,&object,p.vertex[v],point);for(unsigned a=0;a<3;++a) centre[a]+=point[a]; }
            for(unsigned a=0;a<3;++a) centre[a]/=(int32_t)p.count;
            ++waters;
            for(unsigned facing=0;facing<4 && !found;++facing) for(int distance=880;distance<=2288 && !found;distance+=88) {
                int32_t origin[3]={centre[0]-delta[facing][0]*distance,0,centre[2]-delta[facing][1]*distance};
                if(!hmbtn_farm_settle(hmbtn_session_field(g),m,origin,0xE087,64)) continue;
                ++dry;
                HmbtnDog dog=g->dog;HmbtnFarmCarry trial={.area=g->area,.dog=&dog};
                trial.thrown[0]=(HmbtnThrownItem){.item={101,0,1},.position={origin[0],origin[1]-300,origin[2]},.ground=origin[1],.velocity=-28,.facing=facing,.area=g->area};
                for(unsigned tick=0;tick<100 && trial.thrown[0].item.id && !trial.ball_lost;++tick) hmbtn_farm_carry_flight(&trial,NULL,m,NULL,NULL,0);
                if(trial.ball_landed) ++landed;
                if(!trial.ball_lost) continue;
                memcpy(g->intro.player.position,origin,sizeof(origin));g->intro.player.facing=facing;g->control.mode=0;found=true;
            }
        }
    }
    if(!found) fprintf(stderr,"Water search: waters=%u dry=%u landed=%u armed=%d\n",waters,dry,landed,g->dog.ball.armed);
    REQUIRE(found && step(s,HMBTN_PAD_CROSS,error,size));
    for(unsigned i=0;i<200 && (g->dog.ball.armed || g->carry.phase || g->carry.thrown[0].item.id);++i) REQUIRE(step(s,0,error,size));
    REQUIRE(!g->dog.ball.armed && !g->dog.ball.placed && !g->game.inventory.held_item.id && g->village.days.count0[16]==0);return true;
}
int main(int argc,char **argv)
{
    if(argc<3 || argc>4) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};const char *shots=argc==4 ? argv[3] : NULL;
    CHECK(hmbtn_host_open(&host));const HmbtnFarmClock date={0,1,29,0,12,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&date,error,sizeof(error));CHECK(s);
    HmbtnSession *g=hmbtn_scene_preview_session(s);for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(!g->dog.adult && !g->dog.ball.armed && g->game.economy.gold==500);
    g->game.weather.tomorrow=0;CHECK(sleep_day(s,error,sizeof(error)) && g->dog.adult && g->game.clock.season==2 && !g->visit.active);
    CHECK(capture(s,&host,shots,"adult-first-fall",error,sizeof(error)));
    g->game.weather.tomorrow=0;CHECK(sleep_day(s,error,sizeof(error)) && g->village.days.count0[16]==2);
    char before[1024],returned[1024];snprintf(before,sizeof(before),"%s.adult",argv[2]);snprintf(returned,sizeof(returned),"%s.return",argv[2]);
    CHECK(field_reload(&s,&host,argv[1],before,error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    g->game.weather.tomorrow=0;CHECK(sleep_day(s,error,sizeof(error)) && offer(s,&host,shots,true,error,sizeof(error)));
    CHECK(g->game.economy.gold==400 && g->dog.ball.armed && g->game.inventory.held_item.id==101);
    /* Both original choices grant the ball and debit 100; replay baseline for the other branch. */
    hmbtn_scene_preview_destroy(s);s=hmbtn_scene_saved_create(host.renderer,argv[1],before,error,sizeof(error));CHECK(s);g=hmbtn_scene_preview_session(s);
    g->game.weather.tomorrow=0;CHECK(sleep_day(s,error,sizeof(error)) && offer(s,&host,shots,false,error,sizeof(error)) && g->game.economy.gold==400);
    CHECK(step(s,HMBTN_PAD_R2,error,sizeof(error)) && pet_idle(s,error,sizeof(error)) && !g->game.inventory.held_item.id);
    CHECK(take_dog(s,error,sizeof(error)) && travel(s,80,error,sizeof(error)) && capture(s,&host,shots,"adult-carried-house",error,sizeof(error)));
    CHECK(travel(s,0,error,sizeof(error)) && yard(s,error,sizeof(error)) && ball_hand(s,error,sizeof(error)));
    CHECK(toss(s,error,sizeof(error)) && fetch(&s,&host,argv[1],returned,shots,error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(ball_hand(s,error,sizeof(error)) && toss(s,error,sizeof(error)) && fetch(&s,&host,argv[1],NULL,NULL,error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(!g->dog.training);g->game.clock.hour=21;
    CHECK(ball_hand(s,error,sizeof(error)) && toss(s,error,sizeof(error)));
    for(unsigned i=0;i<150;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(!g->dog.fetch && g->dog.ball.placed);g->game.clock.hour=12;
    CHECK(lose_in_water(s,error,sizeof(error)));
    for(unsigned day=0;day<3;++day) {
        g->game.weather.today=g->game.weather.tomorrow=0;CHECK(sleep_day(s,error,sizeof(error)));
        if(day<2) CHECK(!g->visit.active && g->village.days.count0[16]==day+1);
    }
    CHECK(g->dog.training==1 && offer(s,&host,NULL,false,error,sizeof(error)) && g->game.economy.gold==300);
    CHECK(field_reload(&s,&host,argv[1],argv[2],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(g->dog.adult && g->dog.training==1 && g->dog.ball.armed && g->game.inventory.held_item.id==101);
    puts("Adult dog/ball: real fall growth, original Won choices/100G, storage/doors, throw/fetch/mouth/return and pause/save, daily training, night refusal, water loss and three-day paid replacement passed");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

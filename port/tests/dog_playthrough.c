/* The pet comes from new-game initialization; only date/name/player poses are fixtures. */
#include "poultry_helpers.h"
#include "field_helpers.h"
static bool (*forward_effect)(void *,const HmbtnExplorationEffect *,char *,size_t);
static bool whistle_sound,response_sound;
static bool sound_probe(void *ctx,const HmbtnExplorationEffect *fx,char *error,size_t size)
{
    if(fx->kind==HMBTN_EXP_KEY_SOUND) {
        for(unsigned id=18;id<=25;id+=7) {
            const HmbtnFarmSound *s=hmbtn_farm_sound(id);
            if(s && fx->bank==(s->bank==1 ? 2u : 0u) && fx->program==s->program && fx->tone==s->tone
                    && fx->note==s->note && fx->volume==s->volume) {
                if(id==18) whistle_sound=true;else response_sound=true;
            }
        }
    }
    return forward_effect(ctx,fx,error,size);
}
static bool pet_step(HmbtnScenePreview *s,uint32_t pressed,char *error,size_t size)
{
    HmbtnSessionFrame f=hmbtn_scene_preview_frame(s);forward_effect=f.exploration.effect;f.exploration.effect=sound_probe;
    return hmbtn_session_tick(hmbtn_scene_preview_session(s),&f,(HmbtnLogicalInput){pressed,pressed},error,size)
        && hmbtn_title_audio_pump(hmbtn_scene_preview_resources(s).audio,error,size);
}
static bool dog_idle(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnDog *d=&hmbtn_scene_preview_session(s)->dog;
    for(unsigned i=0;i<180 && (d->carry_phase || d->whistling);++i) REQUIRE(pet_step(s,0,error,size));
    REQUIRE(!d->carry_phase && !d->whistling);return true;
}
static bool take(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);HmbtnDog *d=&g->dog;
    REQUIRE(!d->held && d->area==g->area && !g->game.inventory.held_item.id);
    memcpy(g->intro.player.position,d->actor.position,sizeof(d->actor.position));g->intro.player.position[2]+=300;
    g->intro.player.facing=0;g->control.mode=0;
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && d->held && d->carry_phase==1 && dog_idle(s,error,size));
    REQUIRE(g->game.inventory.held_item.id==HMBTN_YOUNG_DOG && g->control.carrying_big && !hmbtn_save_available(g,error,size));return true;
}
static bool drop(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const bool farm=g->area==0;bool found=false;
    for(int z=farm ? -7600 : -1600;z<=(farm ? -4000 : 1200) && !found;z+=400)
        for(int x=farm ? -5200 : -800;x<=(farm ? -4800 : 800);x+=400) {
            int32_t point[3]={x,0,z},player[3]={x,0,z+300};
            if(hmbtn_farm_field_probe(hmbtn_session_field(g),point)
                    || !hmbtn_farm_settle(hmbtn_session_field(g),&hmbtn_scene_preview_resources(s).scene->terrain,point,0xE087,64)
                    || !hmbtn_farm_settle(hmbtn_session_field(g),&hmbtn_scene_preview_resources(s).scene->terrain,player,0xE087,64)) continue;
            memcpy(g->intro.player.position,player,sizeof(player));g->intro.player.facing=0;g->control.mode=0;
            HmbtnHouseExit door;unsigned model;
            if(hmbtn_world_door_at(hmbtn_scene_preview_resources(s).section,0x2000,&g->intro.player,&door,&model)) continue;
            found=true;break;
        }
    REQUIRE(found && step(s,HMBTN_PAD_CROSS,error,size) && g->dog.carry_phase==2 && dog_idle(s,error,size));
    REQUIRE(!g->dog.held && !g->game.inventory.held_item.id && g->dog.area==g->area && g->dog.handled_today);return true;
}
static bool water_refusal(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const HmbtnModel *m=&hmbtn_scene_preview_resources(s).scene->terrain;
    for(unsigned ob=0;ob<m->object_count;++ob) {
        HmbtnModelObject object;REQUIRE(hmbtn_model_object(m,ob,&object));size_t at=12+object.primitives;
        for(unsigned i=0;i<object.primitive_count;++i) {
            HmbtnModelPrimitive p;REQUIRE(hmbtn_model_primitive(m,at,&p));at+=p.bytes;
            if(!(p.flags&7u)) continue;
            int32_t centre[3]={0};
            for(unsigned v=0;v<p.count;++v) { int32_t vertex[3];hmbtn_model_vertex(m,&object,p.vertex[v],vertex);for(unsigned a=0;a<3;++a) centre[a]+=vertex[a]; }
            for(unsigned a=0;a<3;++a) centre[a]/=(int32_t)p.count;
            HmbtnGroundHit hit;if(!hmbtn_ground_find(m,centre,0,200,&hit) || !(hit.flags&7u)) continue;
            memcpy(g->intro.player.position,centre,sizeof(centre));g->intro.player.position[2]+=300;g->intro.player.facing=0;g->control.mode=0;
            REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->dog.held && !g->dog.carry_phase && g->game.inventory.held_item.id==HMBTN_YOUNG_DOG);return true;
        }
    }
    REQUIRE(false);return false;
}
int main(int argc,char **argv)
{
    if(argc<3 || argc>4) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};const char *shots=argc==4 ? argv[3] : NULL;
    CHECK(hmbtn_host_open(&host));const HmbtnFarmClock date={0,0,2,3,12,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&date,error,sizeof(error));CHECK(s);
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    CHECK(g->dog.affection==10 && g->dog.actor.position[0]==-1800 && g->dog.actor.position[2]==3100);
    /* Name remains the existing registration's owner; no second pet-name record. */
    g->game.registration.dog_name[0]=0;g->game.registration.dog_name[1]=1;g->game.registration.dog_name[2]=0xffff;
    memcpy(g->intro.camera,g->dog.actor.position,sizeof(g->intro.camera));CHECK(capture(s,&host,shots,"puppy-initial",error,sizeof(error)));
    const int32_t initial[3]={-1800,0,3100};bool moved=false;
    for(unsigned i=0;i<3600 && !moved;++i) { CHECK(step(s,0,error,sizeof(error)));if(memcmp(initial,g->dog.actor.position,sizeof(initial))) moved=true; }
    if(!moved) fprintf(stderr,"Dog motion: phase=%u step=%u timer=%u interrupt=%u walk=%u pos=%d,%d,%d sprite=%u/%u/%u done=%d rng=%u control=%d\n",g->dog.phase,g->dog.step,g->dog.timer,g->dog.interrupt,g->dog.walk_timer,g->dog.actor.position[0],g->dog.actor.position[1],g->dog.actor.position[2],g->dog.actor.sprite.animation,g->dog.actor.sprite.frame,g->dog.actor.sprite.ticks,g->dog.actor.animation_done,g->dog.random,g->control.enabled);
    CHECK(moved && g->dog.affection==10);
    CHECK(g->dog.phase==18 && g->dog.actor.sprite.animation==1+g->dog.actor.facing);
    CHECK(take(s,error,sizeof(error)) && capture(s,&host,shots,"puppy-held",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_R1|HMBTN_PAD_R2,error,sizeof(error)) && !g->quick_ticks && g->dog.held);
    CHECK(water_refusal(s,error,sizeof(error)));
    CHECK(travel(s,80,error,sizeof(error)) && g->dog.held && g->dog.area==80 && g->control.carrying_big);
    CHECK(drop(s,error,sizeof(error)) && capture(s,&host,shots,"puppy-inside",error,sizeof(error)));
    /* A menu pauses the actual pet state, including the RNG and cue timers. */
    CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)) && g->menu.mode==HMBTN_MENU_STATUS);
    const HmbtnDog paused=g->dog;
    for(unsigned i=0;i<30;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(!memcmp(&paused,&g->dog,sizeof(paused)) && g->menu.dog==&g->dog);
    CHECK(capture(s,&host,shots,"puppy-status",error,sizeof(error)) && step(s,HMBTN_PAD_START,error,sizeof(error)));
    for(unsigned i=0;i<30;++i) CHECK(step(s,0,error,sizeof(error)));
    char inside[1024];snprintf(inside,sizeof(inside),"%s.inside",argv[2]);
    CHECK(field_reload(&s,&host,argv[1],inside,error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(g->dog.area==80 && g->dog.affection==10 && g->dog.handled_today && capture(s,&host,NULL,"puppy-reload",error,sizeof(error)));
    /* Sleeping with the pet in hand releases that same record before the daily care tick. */
    CHECK(take(s,error,sizeof(error)));g->game.weather.today=1;g->game.weather.tomorrow=0;g->house.mode=HMBTN_HOUSE_SLEEP;
    const unsigned day=g->game.clock.day;
    for(unsigned i=0;i<1200 && (g->game.clock.day==day || g->morning_fade);++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(g->dog.area==80 && !g->dog.held && !g->game.inventory.held_item.id && g->dog.affection==12 && !g->dog.handled_today);
    CHECK(take(s,error,sizeof(error)) && travel(s,0,error,sizeof(error)) && g->dog.area==0 && drop(s,error,sizeof(error)));
    CHECK(take(s,error,sizeof(error)) && drop(s,error,sizeof(error)) && g->dog.affection==12);
    for(unsigned attempt=0;attempt<30 && !g->dog.called_today;++attempt) CHECK(pet_step(s,HMBTN_PAD_L1,error,sizeof(error)) && dog_idle(s,error,sizeof(error)));
    CHECK(g->dog.called_today && g->dog.affection==12 && whistle_sound && response_sound);
    g->game.clock.hour=21;
    for(unsigned i=0;i<500;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(g->dog.phase==2 && capture(s,&host,shots,"puppy-night",error,sizeof(error)));
    g->game.clock.hour=12;g->game.weather.today=1;g->game.weather.tomorrow=0;
    CHECK(sleep_day(s,error,sizeof(error)) && g->dog.area==0 && g->dog.affection==11);
    for(unsigned i=0;i<60;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(g->dog.affection==11 && !g->dog.handled_today && !g->dog.called_today);
    CHECK(field_reload(&s,&host,argv[1],argv[2],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(g->dog.area==0 && g->dog.affection==11 && capture(s,&host,shots,"puppy-final",error,sizeof(error)));
    puts("Dog: original new-game puppy/ANI, movement, L1, conserved water refusal, carry/doors/house, menu pause/status, daily care/weather and two checkpoints passed");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

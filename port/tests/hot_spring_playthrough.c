/* Installed spring terrain/action rows; date/status/positions are fixtures. */
#include "poultry_helpers.h"
#include "field_helpers.h"
static bool checkpoint(HmbtnScenePreview **s,HmbtnHost *host,const char *root,const char *base,const char *suffix,char *error,size_t size)
{ char path[1024];if(snprintf(path,sizeof(path),"%s%s",base,suffix)>=(int)sizeof(path)) return false;return field_reload(s,host,root,path,error,size); }
static bool entrance(HmbtnScenePreview *s,char *error,size_t size)
{
    const uint8_t *section=hmbtn_scene_preview_resources(s).section;
    for(unsigned i=0;i<50;++i) {
        const uint8_t *z=section+16+i*44;
        if(hmbtn_read_le16(z)==0x1000 && hmbtn_read_le16(z+2)==0xfffe && hmbtn_read_le16(z+10)==46) {
            at(s,z);memset(hmbtn_scene_preview_session(s)->intro.player.velocity,0,sizeof(hmbtn_scene_preview_session(s)->intro.player.velocity));return true;
        }
    }
    REQUIRE(false);return false;
}
static bool wait_phase(HmbtnScenePreview *s,HmbtnSpringPhase phase,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<600 && g->hot_spring.phase!=phase;++i) REQUIRE(step(s,0,error,size));
    if(g->hot_spring.phase!=phase) fprintf(stderr,"Spring stuck phase=%u ticks=%u position=%d,%d,%d velocity=%d landed=%d\n",g->hot_spring.phase,g->hot_spring.ticks,g->intro.player.position[0],g->intro.player.position[1],g->intro.player.position[2],g->hot_spring.velocity,g->hot_spring.landed);
    REQUIRE(g->hot_spring.phase==phase && !g->door_tick && !g->travel_tick);return true;
}
static bool bath(HmbtnScenePreview *s,HmbtnHost *host,const char *shots,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);REQUIRE(entrance(s,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->hot_spring.phase==HMBTN_SPRING_APPROACH);
    REQUIRE(!hmbtn_save_available(g,error,size) && wait_phase(s,HMBTN_SPRING_ENTER,error,size));
    REQUIRE(capture(s,host,shots,"spring-enter",error,size));
    REQUIRE(wait_phase(s,HMBTN_SPRING_SPLASH,error,size) && capture(s,host,shots,"spring-splash",error,size));
    REQUIRE(wait_phase(s,HMBTN_SPRING_SOAK,error,size));
    int32_t surface[3];memcpy(surface,g->intro.player.position,sizeof(surface));bool target=false;
    for(unsigned i=0;i<50;++i) {
        const uint8_t *z=hmbtn_scene_preview_resources(s).section+16+i*44;
        if(hmbtn_read_le16(z+2)==38) { surface[1]=half(z+16);target=true;break; }
    }
    /* The swimmer is submerged; the first polygon near his feet is the
     * pool's 0x8000 floor, above the 0x8040 water used by thrown eggs. */
    HmbtnGroundHit water;REQUIRE(target && hmbtn_ground_find(&hmbtn_scene_preview_resources(s).scene->terrain,surface,0,4,&water) && (water.flags&0x40u));
    REQUIRE(g->intro.action_active && capture(s,host,shots,"spring-soak",error,size));
    g->farm_action.stamina=30;g->farm_action.fatigue=20;const unsigned minute=g->game.clock.minute;
    for(unsigned i=0;i<45;++i) REQUIRE(step(s,0,error,size));
    REQUIRE(g->farm_action.stamina==36 && g->farm_action.fatigue==17 && g->game.clock.minute==(minute+3)%60);
    REQUIRE(wait_phase(s,HMBTN_SPRING_REST,error,size) && capture(s,host,shots,"spring-rest",error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->hot_spring.phase==HMBTN_SPRING_SOAK);
    REQUIRE(hmbtn_scene_preview_tick(s,(HmbtnLogicalInput){0,HMBTN_PAD_CROSS},error,size) && g->hot_spring.phase==HMBTN_SPRING_SOAK);
    g->farm_action.stamina=g->farm_action.max_stamina-1;g->farm_action.fatigue=0;
    for(unsigned i=0;i<30;++i) REQUIRE(step(s,0,error,size));
    REQUIRE(g->farm_action.stamina==g->farm_action.max_stamina && !g->farm_action.fatigue);
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->hot_spring.phase==HMBTN_SPRING_LEAVE && capture(s,host,shots,"spring-leave",error,size));
    const unsigned stamina=g->farm_action.stamina;
    REQUIRE(wait_phase(s,HMBTN_SPRING_OFF,error,size));
    REQUIRE(!g->intro.action_active && !g->intro.player.pose && g->farm_action.stamina==stamina
        && g->intro.player.facing==0 && g->intro.player.position[2]<4300);
    HmbtnGroundHit dry;REQUIRE(hmbtn_ground_find(&hmbtn_scene_preview_resources(s).scene->terrain,g->intro.player.position,0xE0C7,200,&dry));
    REQUIRE(capture(s,host,shots,"spring-return",error,size));
    for(unsigned i=0;i<HMBTN_SCENE_MODELS;++i) REQUIRE(g->area_doors.models[i].phase==HMBTN_DOOR_CLOSED);
    return true;
}
int main(int argc,char **argv)
{
    if(argc<3 || argc>4) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};const char *shots=argc==4 ? argv[3] : NULL;
    CHECK(hmbtn_host_open(&host));const HmbtnFarmClock date={0,0,4,4,12,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],72,&date,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    /* Pick a real spring bamboo; occupied hands must not throw it or enter. */
    const HmbtnGatherRecord *r=hmbtn_farm_gather_record(146);CHECK(r && r->location==12);
    memcpy(g->intro.player.position,r->position,sizeof(r->position));g->intro.player.position[2]+=224;g->intro.player.facing=0;g->control.mode=0;
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && idle(s,error,sizeof(error)) && g->game.inventory.held_item.id==r->item);
    CHECK(entrance(s,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(!g->hot_spring.phase && !g->door_tick && !g->carry.phase && g->game.inventory.held_item.id==r->item);
    CHECK(step(s,HMBTN_PAD_R2,error,sizeof(error)) && idle(s,error,sizeof(error)) && !g->game.inventory.held_item.id);
    CHECK(checkpoint(&s,&host,argv[1],argv[2],".before",error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(bath(s,&host,shots,error,sizeof(error)));
    CHECK(checkpoint(&s,&host,argv[1],argv[2],".after",error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(g->game.inventory.items[0].id==r->item && !g->hot_spring.phase);
    /* Dawn while soaking follows the normal overnight handoff, with no stale
     * action bank/bath state left in the farmhouse. */
    g->game.weather.today=HMBTN_WEATHER_RAIN;g->game.clock.hour=22;
    CHECK(entrance(s,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && wait_phase(s,HMBTN_SPRING_SOAK,error,sizeof(error)));
    HmbtnWeatherEffects rain=g->weather_effects;
    for(unsigned i=0;i<30;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(g->weather_effects.mode==HMBTN_WEATHER_RAIN && memcmp(&rain,&g->weather_effects,sizeof(rain)));
    CHECK(capture(s,&host,shots,"spring-night-rain",error,sizeof(error)));
    g->game.clock.hour=5;g->game.clock.minute=59;g->game.clock.frames=14;
    CHECK(step(s,0,error,sizeof(error)));
    for(unsigned i=0;i<300 && (g->area!=80 || g->morning_fade || g->overnight_tick);++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(g->area==80 && !g->hot_spring.phase && !g->intro.action_active && !g->morning_fade && g->game.clock.hour==6);
    CHECK(checkpoint(&s,&host,argv[1],argv[2],"",error,sizeof(error)));
    hmbtn_scene_preview_destroy(s);s=NULL;
    const HmbtnFarmClock winter={0,3,4,4,12,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],72,&winter,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    g=hmbtn_scene_preview_session(s);g->game.weather.today=HMBTN_WEATHER_SNOW;
    CHECK(bath(s,&host,NULL,error,sizeof(error)) && g->game.clock.season==3 && g->weather_effects.mode==HMBTN_WEATHER_SNOW);
    puts("Spring route: real forage/occupied hands, original jumps/banks, minute recovery/clamps, two poses/cross release, dry return, rain/night, snow/winter, dawn and three checkpoints passed");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

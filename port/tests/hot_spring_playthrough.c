/* Installed spring terrain/action rows; date/status/positions are fixtures. */
#include "poultry_helpers.h"
#include "field_helpers.h"
static bool (*effect_forward)(void *,const HmbtnExplorationEffect *,char *,size_t);
static unsigned opening_sounds,closing_sounds;static bool twin_moved;
static unsigned expected_sound;static bool sound_seen;
static bool effect_probe(void *ctx,const HmbtnExplorationEffect *fx,char *error,size_t size)
{
    if(fx->kind==HMBTN_EXP_KEY_SOUND) {
        const HmbtnFarmSound *wanted=hmbtn_farm_sound(expected_sound);
        if(wanted && fx->bank==(wanted->bank==1 ? 2u : 0u) && fx->program==wanted->program && fx->tone==wanted->tone
                && fx->note==wanted->note && fx->volume==wanted->volume) sound_seen=true;
        const HmbtnFarmSound *open=hmbtn_farm_sound(0x16),*close=hmbtn_farm_sound(0x4c);
        if(fx->program==open->program && fx->tone==open->tone && fx->note==open->note) ++opening_sounds;
        if(fx->program==close->program && fx->tone==close->tone && fx->note==close->note) ++closing_sounds;
    }
    return effect_forward(ctx,fx,error,size);
}
static bool observed_step(HmbtnScenePreview *s,uint32_t pressed,unsigned sound,char *error,size_t size)
{
    HmbtnSessionFrame f=hmbtn_scene_preview_frame(s);effect_forward=f.exploration.effect;
    expected_sound=sound;sound_seen=false;f.exploration.effect=effect_probe;
    REQUIRE(hmbtn_session_tick(hmbtn_scene_preview_session(s),&f,(HmbtnLogicalInput){pressed,pressed},error,size));
    REQUIRE(!sound || sound_seen);return hmbtn_title_audio_pump(hmbtn_scene_preview_resources(s).audio,error,size);
}
static bool checkpoint(HmbtnScenePreview **s,HmbtnHost *host,const char *root,const char *base,const char *suffix,char *error,size_t size)
{ char path[1024];if(snprintf(path,sizeof(path),"%s%s",base,suffix)>=(int)sizeof(path)) return false;return field_reload(s,host,root,path,error,size); }
static bool entrance(HmbtnScenePreview *s,char *error,size_t size)
{
    const uint8_t *section=hmbtn_scene_preview_resources(s).section;
    for(unsigned i=0;i<50;++i) {
        const uint8_t *z=section+16+i*44;
        if(hmbtn_read_le16(z)==0x1000 && hmbtn_read_le16(z+2)==0xfffe && hmbtn_read_le16(z+10)==46) {
            at(s,z);hmbtn_scene_preview_session(s)->intro.player.position[2]=half(z+18)-600;memset(hmbtn_scene_preview_session(s)->intro.player.velocity,0,sizeof(hmbtn_scene_preview_session(s)->intro.player.velocity));
            for(unsigned tick=0;tick<40;++tick) REQUIRE(step(s,HMBTN_PAD_UP | (hmbtn_scene_preview_session(s)->game.clock.season==0 ? HMBTN_PAD_CIRCLE : 0),error,size));
            fprintf(stderr,"Natural spring approach: z=%d trigger=%d..%d\n",hmbtn_scene_preview_session(s)->intro.player.position[2],half(z+18),half(z+24));
            REQUIRE(hmbtn_scene_preview_session(s)->intro.player.position[2]>half(z+18));
            return true;
        }
    }
    REQUIRE(false);return false;
}
static bool wait_phase(HmbtnScenePreview *s,HmbtnSpringPhase phase,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<600 && g->hot_spring.phase!=phase;++i) {
        if(g->hot_spring.phase==HMBTN_SPRING_ENTER && g->hot_spring.ticks==19) REQUIRE(observed_step(s,0,16,error,size));
        else REQUIRE(step(s,0,error,size));
        const HmbtnSceneData *scene=hmbtn_scene_preview_resources(s).scene;
        if(g->area_doors.models[0].turn && ((scene->angle[0][1]+scene->angle[1][1])&4095u)==0) twin_moved=true;
    }
    if(g->hot_spring.phase!=phase) fprintf(stderr,"Spring stuck phase=%u ticks=%u position=%d,%d,%d velocity=%d landed=%d\n",g->hot_spring.phase,g->hot_spring.ticks,g->intro.player.position[0],g->intro.player.position[1],g->intro.player.position[2],g->hot_spring.velocity,g->hot_spring.landed);
    REQUIRE(g->hot_spring.phase==phase && !g->door_tick && !g->travel_tick);return true;
}
static bool bath(HmbtnScenePreview *s,HmbtnHost *host,const char *shots,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);REQUIRE(entrance(s,error,size));
    opening_sounds=closing_sounds=0;twin_moved=false;
    REQUIRE(observed_step(s,HMBTN_PAD_CROSS,0x16,error,size) && g->hot_spring.phase==HMBTN_SPRING_APPROACH);
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
    REQUIRE(observed_step(s,HMBTN_PAD_CROSS,6,error,size) && g->hot_spring.phase==HMBTN_SPRING_LEAVE && capture(s,host,shots,"spring-leave",error,size));
    const unsigned stamina=g->farm_action.stamina;
    REQUIRE(wait_phase(s,HMBTN_SPRING_OFF,error,size));
    fprintf(stderr,"Spring return: p=%d,%d,%d facing=%u pose=%d action=%d stamina=%u/%u ticks=%u exit=%d\n",g->intro.player.position[0],g->intro.player.position[1],g->intro.player.position[2],g->intro.player.facing,g->intro.player.pose,g->intro.action_active,g->farm_action.stamina,stamina,g->hot_spring.ticks,g->hot_spring.exit_pending);
    REQUIRE(!g->intro.action_active && !g->intro.player.pose && g->farm_action.stamina==stamina
        && g->intro.player.facing==0 && g->intro.player.position[2]<4300);
    HmbtnGroundHit dry;REQUIRE(hmbtn_ground_find(&hmbtn_scene_preview_resources(s).scene->terrain,g->intro.player.position,0xE0C7,200,&dry));
    REQUIRE(capture(s,host,shots,"spring-return",error,size));
    REQUIRE(g->intro.player.position[2]>g->hot_spring.exit_box[2]-400);
    for(unsigned i=0;i<60;++i) REQUIRE(observed_step(s,0,0,error,size));
    REQUIRE(twin_moved && opening_sounds && closing_sounds);
    REQUIRE(g->area_doors.models[0].phase==HMBTN_DOOR_CLOSED && g->area_doors.models[0].twin==1);
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
    CHECK(entrance(s,error,sizeof(error)));
    for(unsigned i=0;i<20;++i) CHECK(step(s,HMBTN_PAD_UP|HMBTN_PAD_CIRCLE,error,sizeof(error)));
    CHECK(!g->hot_spring.phase && g->intro.player.position[2]<g->hot_spring.door_z && !g->door_tick);
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

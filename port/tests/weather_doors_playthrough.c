/* Installed-resource regression for rain audio, carried entry and NPC doors.
 * Only inventory/route starting states are fixtures; geometry, route cells,
 * movement, door timing, audio banks, transitions and save/load are real. */
#include "poultry_helpers.h"
#include "field_helpers.h"
static uint64_t pcm_energy(HmbtnSound *sound,unsigned blocks)
{
    uint64_t energy=0;int16_t pcm[1024];
    for(unsigned i=0;i<blocks;++i) {
        hmbtn_sound_render(sound,pcm,512);
        for(unsigned j=0;j<1024;++j) energy+=(uint64_t)abs(pcm[j]);
    }
    return energy;
}
static bool carried_entry(HmbtnScenePreview *s,unsigned item,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    const HmbtnStoredTool held={(uint16_t)item,7,1};g->game.inventory.held_item=held;
    const unsigned discarded=g->carry.discarded;
    const uint8_t *zone=NULL;
    for(unsigned i=0;i<50;++i) {
        const uint8_t *z=hmbtn_scene_preview_resources(s).section+16+44*i;
        if(hmbtn_read_le16(z)==0x9000 && hmbtn_read_le16(z+2)==80) { zone=z;break; }
    }
    REQUIRE(zone);at(s,zone);
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->door_tick && !g->carry.phase);
    REQUIRE(!memcmp(&held,&g->game.inventory.held_item,sizeof(held)));
    const unsigned facing=g->intro.player.facing,axis=(facing&1u) ? 0 : 2;
    const int32_t start=g->intro.player.position[axis];
    REQUIRE(step(s,0,error,size));
    REQUIRE(g->intro.player.position[axis]==start+(facing<2 ? 30 : -30));
    REQUIRE(g->door_speed==5 && g->door_angle==-5);
    const bool big=(hmbtn_farm_item(item)->flags&0x180u)!=0;
    REQUIRE(g->intro.player.sprite.animation==(facing+2)%4+(big ? 42u : 15u));
    for(unsigned i=0;i<120 && (g->area!=80 || g->travel_tick || g->door_tick);++i) {
        REQUIRE(step(s,0,error,size));
        REQUIRE(!memcmp(&held,&g->game.inventory.held_item,sizeof(held)) && !g->carry.phase);
    }
    REQUIRE(g->area==80 && !g->control.exit_pending && g->carry.discarded==discarded);
    REQUIRE(travel(s,0,error,size) && g->game.inventory.held_item.id==item);
    g->game.inventory.held_item=(HmbtnStoredTool){0};return true;
}
static void resident_fixture(HmbtnSession *g,const uint8_t *section,unsigned row,unsigned column)
{
    /* Keep unrelated residents out of this controlled passage. */
    for(unsigned id=1;id<HMBTN_VILLAGERS;++id) {
        HmbtnVillager *p=&g->village.people[id];p->step=0;
        for(unsigned i=0;i<30;++i) {
            p->schedule[i][0]=1500;p->schedule[i][3]=54;
        }
        p->location=54;p->actor.visible=false;
    }
    HmbtnVillager *p=&g->village.people[7];const uint8_t *previous=section+0x1000+(row*17+column-1)*12;
    p->schedule[0][0]=(uint16_t)(g->game.clock.hour*60+g->game.clock.minute);
    p->schedule[0][1]=(uint16_t)row;p->schedule[0][3]=hmbtn_read_le16(section+2);
    p->location=p->schedule[0][3];p->row=row;p->column=column;p->wait=p->delay=0;
    p->placed=true;p->standing=p->cell_entered=p->blocked_by_player=false;
    for(unsigned a=0;a<3;++a) p->actor.position[a]=half(previous+6+a*2);
    p->actor.facing=hmbtn_read_le16(previous+4)&3u;p->actor.visible=true;
    g->intro.player.position[0]=p->actor.position[0]+800;
    g->intro.player.position[2]=p->actor.position[2]+1000;g->control.mode=0;
}
int main(int argc,char **argv)
{
    if(argc<3 || argc>4) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};
    const char *shots=argc==4 ? argv[3] : NULL;
    CHECK(hmbtn_host_open(&host));const HmbtnFarmClock clock={.day=3,.weekday=3,.hour=10};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&clock,error,sizeof(error));CHECK(s);
    CHECK(hmbtn_scene_preview_set_weather(s,1,0));
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);CHECK(g->runtime.audio_song==22);
    HmbtnSound *sound=hmbtn_scene_preview_resources(s).audio->sound;
    CHECK(pcm_energy(sound,128)>0);
    /* Check sustained decoded ambience and the original SEQ loop. */
    for(unsigned i=0;i<2400 && !hmbtn_sound_loop_count(sound);++i) CHECK(pcm_energy(sound,8)>0);
    CHECK(hmbtn_sound_loop_count(sound)>0);
    CHECK(carried_entry(s,0x44,error,sizeof(error)) && g->runtime.audio_song==22);
    g->game.inventory.held_item=(HmbtnStoredTool){0x44,0,1};
    CHECK(field_reload(&s,&host,argv[1],argv[2],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(g->runtime.audio_song==22 && g->game.inventory.held_item.id==0x44);
    CHECK(pcm_energy(hmbtn_scene_preview_resources(s).audio->sound,128)>0);
    /* Away from doors the same button still throws normally. */
    g->intro.player.position[0]=-5000;g->intro.player.position[2]=-5000;g->intro.player.facing=3;g->control.mode=0;
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && g->carry.phase>=2);
    CHECK(idle(s,error,sizeof(error)) && !g->game.inventory.held_item.id);
    for(unsigned i=0;i<100;++i) CHECK(step(s,0,error,sizeof(error)));
    g->game.clock.hour=22;CHECK(step(s,0,error,sizeof(error)) && g->runtime.audio_song==22);
    CHECK(pcm_energy(hmbtn_scene_preview_resources(s).audio->sound,64)>0);
    CHECK(travel(s,80,error,sizeof(error)) && g->runtime.audio_song==22);
    CHECK(pcm_energy(hmbtn_scene_preview_resources(s).audio->sound,64)>0);
    CHECK(travel(s,0,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_set_weather(s,0,0));CHECK(step(s,0,error,sizeof(error)) && g->runtime.audio_song==28);
    g->game.clock.hour=10;CHECK(step(s,0,error,sizeof(error)) && g->runtime.audio_song==1);
    /* Closed village shop: show its refusal without losing the held crop. */
    g->game.clock.hour=6;CHECK(hmbtn_scene_preview_load_area(s,32,error,sizeof(error)));
    CHECK(step(s,0,error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    const uint8_t *zone=NULL;
    for(unsigned i=0;i<50;++i) {
        const uint8_t *z=hmbtn_scene_preview_resources(s).section+16+44*i;
        if(hmbtn_read_le16(z)==0x9000 && hmbtn_read_le16(z+10)==13) { zone=z;break; }
    }
    CHECK(zone);g->game.inventory.held_item=(HmbtnStoredTool){0x44,3,1};at(s,zone);
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(g->house.mode==HMBTN_HOUSE_BOOK && g->house.loaded && !g->carry.phase && !g->door_tick);
    CHECK(g->game.inventory.held_item.id==0x44 && g->game.inventory.held_item.modifier==3);
    for(unsigned i=0;i<1200 && g->house.mode!=HMBTN_HOUSE_FREE;++i) CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(g->house.mode==HMBTN_HOUSE_FREE);
    g->game.inventory.held_item=(HmbtnStoredTool){0};
    /* Live routes from the real area-40 image: row 0 leaves, row 1 enters.
     * Their original door event is 25, model 0. */
    g->game.clock.hour=10;
    for(unsigned direction=0;direction<2;++direction) {
        CHECK(hmbtn_scene_preview_load_area(s,40,error,sizeof(error)) && step(s,0,error,sizeof(error)));
        const uint8_t *section=hmbtn_scene_preview_resources(s).section;
        const unsigned column=direction ? 3 : 1;
        const uint8_t *event=section+0x1000+(direction*17+column)*12;
        CHECK(hmbtn_read_le16(event)==(direction ? 0x12 : 0x11) && hmbtn_read_le16(event+2)==25);
        resident_fixture(g,section,direction,column);
        HmbtnVillager *p=&g->village.people[7];const int32_t x=p->actor.position[0],z=p->actor.position[2];
        const uint16_t base=hmbtn_scene_preview_resources(s).scene->angle[0][1];
        CHECK(step(s,0,error,sizeof(error)) && g->area_doors.models[0].phase==HMBTN_DOOR_OPENING);
        CHECK(p->column==column && p->actor.position[0]==x && p->actor.position[2]==z);
        /* Save/load halfway through a real door request must not lose/skip the route. */
        for(unsigned t=0;t<5;++t) CHECK(step(s,0,error,sizeof(error)));
        CHECK(hmbtn_scene_preview_resources(s).scene->angle[0][1]!=base);
        CHECK(capture(s,&host,shots,direction ? "npc-enter-opening" : "npc-exit-opening",error,sizeof(error)));
        CHECK(field_reload(&s,&host,argv[1],argv[2],error,sizeof(error)));g=hmbtn_scene_preview_session(s);p=&g->village.people[7];
        CHECK(p->column==column && !g->area_doors.bound);
        CHECK(step(s,0,error,sizeof(error)) && g->area_doors.models[0].phase==HMBTN_DOOR_OPENING);
        CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)));
        HmbtnAreaDoors frozen=g->area_doors;
        for(unsigned t=0;t<20;++t) CHECK(step(s,0,error,sizeof(error)));
        CHECK(!memcmp(&frozen,&g->area_doors,sizeof(frozen)));
        CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)));
        for(unsigned t=0;t<180 && p->column==column;++t) CHECK(step(s,0,error,sizeof(error)));
        CHECK(p->column>column && g->area_doors.models[0].phase==HMBTN_DOOR_HELD);
        CHECK(capture(s,&host,shots,direction ? "npc-enter-open" : "npc-exit-open",error,sizeof(error)));
        const HmbtnAreaDoors unchanged=g->area_doors;
        CHECK(capture(s,&host,NULL,"unchanged",error,sizeof(error)) && !memcmp(&unchanged,&g->area_doors,sizeof(unchanged)));
        for(unsigned t=0;t<60;++t) CHECK(step(s,0,error,sizeof(error)));
        CHECK(g->area_doors.models[0].phase==HMBTN_DOOR_CLOSED && hmbtn_scene_preview_resources(s).scene->angle[0][1]==base);
        CHECK(capture(s,&host,shots,direction ? "npc-enter-closed" : "npc-exit-closed",error,sizeof(error)));
    }
    /* Opening and closing use actual farm-bank effects, also after song swaps. */
    sound=hmbtn_scene_preview_resources(s).audio->sound;hmbtn_sound_stop_music(sound);
    for(unsigned i=0;i<2;++i) {
        const HmbtnFarmSound *fx=hmbtn_farm_sound(i ? 0x4c : 0x16);CHECK(fx);
        CHECK(hmbtn_sound_key_on(sound,2,fx->program,fx->tone,fx->note,fx->volume));
        CHECK(pcm_energy(sound,16)>0);
    }
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);
    puts("Weather/doors: decoded looping rain, day/night/indoor/save audio, held entry/refusal/throw, resident exit/entry/close, pause, restore and effects passed.");return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

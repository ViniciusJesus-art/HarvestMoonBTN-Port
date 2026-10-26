/* Original event and message BINs. Item/date/position fixtures exercise script
 * paths; the final world path obtains its present from the actual gathering code. */
#include "scene_fixture.h"
#include "item_reaction.h"
#include "sdl_host.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Gift integration line %d NPC %u: %s; %s\n",__LINE__,npc,#x,error);goto fail; } } while(0)
static bool step(HmbtnScenePreview *s,uint32_t pressed,uint32_t held,char *error,size_t size)
{
    return hmbtn_scene_preview_tick(s,(HmbtnLogicalInput){pressed,held},error,size);
}
static bool original_message(HmbtnScenePreview *s,char *error,size_t size)
{
    uint8_t bytes[HMBTN_TEXT_RECORD_BYTES];HmbtnMessage expected;
    const HmbtnTextResourceId id={HMBTN_TEXT_RECORD,hmbtn_scene_preview_session(s)->talk.bank,hmbtn_scene_preview_session(s)->talk.request};
    return hmbtn_text_resource_read(hmbtn_scene_preview_resources(s).root,id,bytes,sizeof(bytes),error,size)
        && hmbtn_feed_shop_expand(bytes,sizeof(bytes),hmbtn_game_registration_const(&hmbtn_scene_preview_session(s)->game))
        && hmbtn_message_open(&expected,bytes,sizeof(bytes),error,size)
        && expected.count==hmbtn_scene_preview_session(s)->dialogue.message.count
        && !memcmp(expected.words,hmbtn_scene_preview_session(s)->dialogue.message.words,expected.count*sizeof(*expected.words));
}
static bool gift(HmbtnScenePreview *s,HmbtnVillage *v,unsigned npc,unsigned item,unsigned contents,
    const HmbtnFarmClock *clock,unsigned *messages,bool *birthday,char *error,size_t size)
{
    uint8_t bytes[8192];uint32_t offset;size_t length;
    hmbtn_scene_preview_session(s)->house.inventory->held_item=(HmbtnStoredTool){(uint16_t)item,(uint16_t)contents,1};
    if(!hmbtn_village_talk_range(npc,&offset,&length)
            || !hmbtn_resource_read(hmbtn_scene_preview_resources(s).root,"A_FILE/12_event_s.bin",offset,bytes,length,error,size)
            || !hmbtn_village_talk_open(&hmbtn_scene_preview_session(s)->talk,npc,bytes,length,v,&hmbtn_scene_preview_session(s)->intro.player,clock,hmbtn_game_registration_const(&hmbtn_scene_preview_session(s)->game),&hmbtn_scene_preview_session(s)->house,error,size)) return false;
    unsigned seen=UINT32_MAX;*birthday=false;
    for(unsigned frame=0;frame<5000 && hmbtn_scene_preview_session(s)->talk.active;++frame) {
        if(!hmbtn_scene_preview_tick(s,(HmbtnLogicalInput){HMBTN_PAD_CROSS,HMBTN_PAD_CROSS},error,size)) return false;
        if(hmbtn_scene_preview_session(s)->talk.pending && hmbtn_scene_preview_session(s)->talk.loaded && seen!=hmbtn_scene_preview_session(s)->talk.message_pc) {
            seen=hmbtn_scene_preview_session(s)->talk.message_pc;++*messages;
            if(!original_message(s,error,size)) return false;
            if(hmbtn_scene_preview_session(s)->talk.request==1737) *birthday=true;
        }
    }
    if(hmbtn_scene_preview_session(s)->talk.script.boundary) snprintf(error,size,"NPC %u gift stopped at %u/%u",npc,hmbtn_scene_preview_session(s)->talk.boundary_type,hmbtn_scene_preview_session(s)->talk.script.pc);
    return !hmbtn_scene_preview_session(s)->talk.active && !hmbtn_scene_preview_session(s)->talk.script.boundary;
}
static void fixture(HmbtnVillage *v,unsigned npc)
{
    hmbtn_village_init(v,NULL,0);v->people[npc].location=54;v->people[npc].schedule[0][2]=54;
    v->people[npc].actor.visible=true;v->people[npc].actor.facing=3;
}
int main(int argc,char **argv)
{
    if(argc<2 || argc>3) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};unsigned npc=0,messages=0,cases=0;
    CHECK(hmbtn_host_open(&host));s=hmbtn_scene_area_create(host.renderer,argv[1],128,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,0,error,sizeof(error)));
    const HmbtnFarmClock clock={0,0,10,2,20,0,0};bool birthday;
    /* First/repeated gifts and showing an animal across all regular residents. */
    for(npc=1;npc<36;++npc) {
        if(npc==27 || npc==30 || npc==33 || npc==35) continue;
        const unsigned items[]={161,58,265};
        for(unsigned i=0;i<3;++i) {
            HmbtnVillage v;fixture(&v,npc);
            for(unsigned repeat=0;repeat<2;++repeat) {
                CHECK(gift(s,&v,npc,items[i],0,&clock,&messages,&birthday,error,sizeof(error)));
                CHECK(hmbtn_scene_preview_session(s)->house.inventory->held_item.id==(i==2 ? 265u : 0u));
                CHECK(v.people[npc].actor.facing==3);++cases;
            }
        }
    }
    npc=18;HmbtnVillage v;fixture(&v,npc);
    CHECK(gift(s,&v,npc,161,0,&clock,&messages,&birthday,error,sizeof(error)) && v.people[npc].friendship==59);
    CHECK(gift(s,&v,npc,161,0,&clock,&messages,&birthday,error,sizeof(error)) && v.people[npc].friendship==62);
    fixture(&v,npc);HmbtnFarmClock date=clock;date.day=18;
    CHECK(gift(s,&v,npc,161,0,&date,&messages,&birthday,error,sizeof(error)) && v.people[npc].friendship==95 && birthday);
    fixture(&v,npc);
    /* This installed translation's bytecode subtracts 1 for wrapping; execute
     * its operands literally rather than substituting the C helper's +2. */
    CHECK(gift(s,&v,npc,164,161,&clock,&messages,&birthday,error,sizeof(error)) && v.people[npc].friendship==58);
    fixture(&v,npc);
    CHECK(gift(s,&v,npc,164,161,&date,&messages,&birthday,error,sizeof(error)) && v.people[npc].friendship==90 && birthday);
    fixture(&v,npc);hmbtn_scene_preview_session(s)->house.event_count1=14;
    CHECK(gift(s,&v,npc,58,0,&clock,&messages,&birthday,error,sizeof(error))
        && v.people[npc].friendship==41 && hmbtn_scene_preview_session(s)->house.event_count1==15);
    CHECK(gift(s,&v,npc,58,0,&clock,&messages,&birthday,error,sizeof(error))
        && v.people[npc].friendship==32 && hmbtn_scene_preview_session(s)->house.event_count1==15);
    /* Bachelorette birthday changes when it matches the player's birthday. */
    npc=3;unsigned loved=0;
    for(unsigned i=58;i<247 && !loved;++i) if(hmbtn_item_villager_reaction(i,npc)==0) loved=i;
    CHECK(loved);fixture(&v,npc);
    date=clock;date.season=2;date.day=14;
    /* In this BIN Karen's multiplier is inside the wrapped-item branch.
     * Birthday detection still changes the unwrapped dialogue. */
    CHECK(gift(s,&v,npc,loved,0,&date,&messages,&birthday,error,sizeof(error))
        && v.people[npc].affection==500 && hmbtn_scene_preview_session(s)->talk.slots[12]==1);
    fixture(&v,npc);
    CHECK(gift(s,&v,npc,164,loved,&date,&messages,&birthday,error,sizeof(error))
        && v.people[npc].affection==2500 && hmbtn_scene_preview_session(s)->talk.slots[12]==1);
    hmbtn_scene_preview_session(s)->game.registration.birthday_season=2;hmbtn_scene_preview_session(s)->game.registration.birthday_day=14;fixture(&v,npc);
    CHECK(gift(s,&v,npc,164,loved,&date,&messages,&birthday,error,sizeof(error))
        && v.people[npc].affection==500 && !hmbtn_scene_preview_session(s)->talk.slots[12]);
    date.day=22;fixture(&v,npc);
    CHECK(gift(s,&v,npc,164,loved,&date,&messages,&birthday,error,sizeof(error))
        && v.people[npc].affection==2500 && hmbtn_scene_preview_session(s)->talk.slots[12]==1);
    /* Real gathering -> gift to Lillia while her shop is open. */
    npc=18;hmbtn_scene_preview_session(s)->house.inventory->held_item=(HmbtnStoredTool){0};hmbtn_scene_preview_session(s)->game.clock=clock;hmbtn_scene_preview_session(s)->game.clock.hour=12;
    CHECK(hmbtn_scene_preview_load_area(s,72,error,sizeof(error)));hmbtn_scene_preview_session(s)->intro.area=72;
    unsigned picked=0;
    for(unsigned i=125;i<=172 && !picked;++i) {
        const HmbtnGatherRecord *r=hmbtn_farm_gather_record(i);
        if(!hmbtn_farm_gather_item(&hmbtn_scene_preview_session(s)->gather,i,hmbtn_read_le16(hmbtn_scene_preview_resources(s).section+2),0)) continue;
        memcpy(hmbtn_scene_preview_session(s)->intro.player.position,r->position,sizeof(r->position));
        hmbtn_scene_preview_session(s)->intro.player.position[2]+=128;hmbtn_scene_preview_session(s)->intro.player.facing=0;
        if(hmbtn_farm_gather_take(&hmbtn_scene_preview_session(s)->gather,hmbtn_read_le16(hmbtn_scene_preview_resources(s).section+2),0,&hmbtn_scene_preview_session(s)->house,&hmbtn_scene_preview_session(s)->carry,&hmbtn_scene_preview_session(s)->intro.player)) picked=hmbtn_scene_preview_session(s)->house.inventory->held_item.id;
    }
    CHECK(picked);for(unsigned i=0;i<100 && hmbtn_scene_preview_session(s)->carry.phase;++i) CHECK(step(s,0,0,error,sizeof(error)));
    CHECK(!hmbtn_scene_preview_session(s)->carry.phase && hmbtn_scene_preview_session(s)->house.inventory->held_item.id==picked);
    CHECK(hmbtn_scene_preview_load_area(s,128,error,sizeof(error)));hmbtn_scene_preview_session(s)->intro.area=128;
    CHECK(step(s,0,0,error,sizeof(error)));
    const HmbtnIntroActor *l=&hmbtn_scene_preview_session(s)->village.people[npc].actor;CHECK(l->visible);
    memcpy(hmbtn_scene_preview_session(s)->intro.player.position,l->position,sizeof(l->position));hmbtn_scene_preview_session(s)->intro.player.position[2]+=300;hmbtn_scene_preview_session(s)->intro.player.facing=0;
    memcpy(hmbtn_scene_preview_session(s)->intro.camera,hmbtn_scene_preview_session(s)->intro.player.position,sizeof(hmbtn_scene_preview_session(s)->intro.camera));hmbtn_scene_preview_session(s)->control.mode=0;
    CHECK(step(s,HMBTN_PAD_CROSS,0,error,sizeof(error)) && hmbtn_scene_preview_session(s)->talk.active && hmbtn_scene_preview_session(s)->feed_shop.phase==HMBTN_SHOP_OFF && !hmbtn_scene_preview_session(s)->carry.phase);
    const HmbtnFarmClock before=hmbtn_scene_preview_session(s)->game.clock;const unsigned discarded=hmbtn_scene_preview_session(s)->carry.discarded;
    for(unsigned i=0;i<60;++i) CHECK(step(s,HMBTN_PAD_START,HMBTN_PAD_UP,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->talk.active && hmbtn_scene_preview_session(s)->menu.mode==HMBTN_MENU_NONE && !memcmp(&before,&hmbtn_scene_preview_session(s)->game.clock,sizeof(before)));
    CHECK(hmbtn_scene_preview_draw(s,host.renderer,error,sizeof(error)));
    if(argc==3) {
        SDL_Surface *pixels=SDL_RenderReadPixels(host.renderer,NULL);CHECK(pixels);
        const bool saved=SDL_SaveBMP(pixels,argv[2]);SDL_DestroySurface(pixels);CHECK(saved);
    }
    for(unsigned i=0;i<3000 && hmbtn_scene_preview_session(s)->talk.active;++i) CHECK(step(s,HMBTN_PAD_CROSS,0,error,sizeof(error)));
    CHECK(!hmbtn_scene_preview_session(s)->talk.active && !hmbtn_scene_preview_session(s)->talk.script.boundary && !hmbtn_scene_preview_session(s)->house.inventory->held_item.id && !hmbtn_scene_preview_session(s)->control.carrying
        && !hmbtn_scene_preview_session(s)->control.carrying_big && !hmbtn_scene_preview_session(s)->carry.phase && hmbtn_scene_preview_session(s)->carry.discarded==discarded);
    printf("%u gift cases and %u BIN message requests: first/repeat, birthdays, wrapping, losses, passive reactions and gathered present passed.\n",cases,messages);
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

/* Original event and message BINs. Item/date/position fixtures exercise script
 * paths; the final world path obtains its present from the actual gathering code. */
#include "scene_preview.c"
#include "item_reaction.h"
#include "sdl_host.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Gift integration line %d NPC %u: %s; %s\n",__LINE__,npc,#x,error);goto fail; } } while(0)
static bool step(HmbtnScenePreview *s,uint32_t pressed,uint32_t held,char *error,size_t size)
{
    hmbtn_scene_data_tick(&s->scene);s->pending_pressed=pressed;
    bool ok=opening_tick(s,held,error,size);s->pending_pressed=0;return ok;
}
static bool original_message(HmbtnScenePreview *s,char *error,size_t size)
{
    uint8_t bytes[HMBTN_TEXT_RECORD_BYTES];HmbtnMessage expected;
    const HmbtnTextResourceId id={HMBTN_TEXT_RECORD,s->talk.bank,s->talk.request};
    return hmbtn_text_resource_read(s->data_root,id,bytes,sizeof(bytes),error,size)
        && hmbtn_feed_shop_expand(bytes,sizeof(bytes),&s->dialogue)
        && hmbtn_message_open(&expected,bytes,sizeof(bytes),error,size)
        && expected.count==s->dialogue.message.count
        && !memcmp(expected.words,s->dialogue.message.words,expected.count*sizeof(*expected.words));
}
static bool gift(HmbtnScenePreview *s,HmbtnVillage *v,unsigned npc,unsigned item,unsigned contents,
    const HmbtnFarmClock *clock,unsigned *messages,bool *birthday,char *error,size_t size)
{
    uint8_t bytes[8192];uint32_t offset;size_t length;
    s->house.held_item=(HmbtnStoredTool){(uint16_t)item,(uint16_t)contents,1};
    if(!hmbtn_village_talk_range(npc,&offset,&length)
            || !hmbtn_resource_read(s->data_root,"A_FILE/12_event_s.bin",offset,bytes,length,error,size)
            || !hmbtn_village_talk_open(&s->talk,npc,bytes,length,v,&s->intro.player,clock,&s->dialogue,&s->house,error,size)) return false;
    unsigned seen=UINT32_MAX;*birthday=false;
    for(unsigned frame=0;frame<5000 && s->talk.active;++frame) {
        if(!village_conversation(s,HMBTN_PAD_CROSS,HMBTN_PAD_CROSS,error,size)) return false;
        if(s->talk.pending && s->talk.loaded && seen!=s->talk.message_pc) {
            seen=s->talk.message_pc;++*messages;
            if(!original_message(s,error,size)) return false;
            if(s->talk.request==1737) *birthday=true;
        }
    }
    if(s->talk.script.boundary) snprintf(error,size,"NPC %u gift stopped at %u/%u",npc,s->talk.boundary_type,s->talk.script.pc);
    return !s->talk.active && !s->talk.script.boundary;
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
                CHECK(s->house.held_item.id==(i==2 ? 265u : 0u));
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
    fixture(&v,npc);s->house.event_count1=14;
    CHECK(gift(s,&v,npc,58,0,&clock,&messages,&birthday,error,sizeof(error))
        && v.people[npc].friendship==41 && s->house.event_count1==15);
    CHECK(gift(s,&v,npc,58,0,&clock,&messages,&birthday,error,sizeof(error))
        && v.people[npc].friendship==32 && s->house.event_count1==15);
    /* Bachelorette birthday changes when it matches the player's birthday. */
    npc=3;unsigned loved=0;
    for(unsigned i=58;i<247 && !loved;++i) if(hmbtn_item_villager_reaction(i,npc)==0) loved=i;
    CHECK(loved);fixture(&v,npc);
    date=clock;date.season=2;date.day=14;
    /* In this BIN Karen's multiplier is inside the wrapped-item branch.
     * Birthday detection still changes the unwrapped dialogue. */
    CHECK(gift(s,&v,npc,loved,0,&date,&messages,&birthday,error,sizeof(error))
        && v.people[npc].affection==500 && s->talk.slots[12]==1);
    fixture(&v,npc);
    CHECK(gift(s,&v,npc,164,loved,&date,&messages,&birthday,error,sizeof(error))
        && v.people[npc].affection==2500 && s->talk.slots[12]==1);
    s->dialogue.season=2;s->dialogue.day=14;fixture(&v,npc);
    CHECK(gift(s,&v,npc,164,loved,&date,&messages,&birthday,error,sizeof(error))
        && v.people[npc].affection==500 && !s->talk.slots[12]);
    date.day=22;fixture(&v,npc);
    CHECK(gift(s,&v,npc,164,loved,&date,&messages,&birthday,error,sizeof(error))
        && v.people[npc].affection==2500 && s->talk.slots[12]==1);
    /* Real gathering -> gift to Lillia while her shop is open. */
    npc=18;s->house.held_item=(HmbtnStoredTool){0};s->clock=clock;s->clock.hour=12;sync_clock(s);
    CHECK(load_area(s,s->data_root,72,error,sizeof(error)));s->intro.area=72;
    unsigned picked=0;
    for(unsigned i=125;i<=172 && !picked;++i) {
        const HmbtnGatherRecord *r=hmbtn_farm_gather_record(i);
        if(!hmbtn_farm_gather_item(&s->gather,i,hmbtn_read_le16(s->section+2),0)) continue;
        memcpy(s->intro.player.position,r->position,sizeof(r->position));
        s->intro.player.position[2]+=128;s->intro.player.facing=0;
        if(hmbtn_farm_gather_take(&s->gather,hmbtn_read_le16(s->section+2),0,&s->house,&s->carry,&s->intro.player)) picked=s->house.held_item.id;
    }
    CHECK(picked);for(unsigned i=0;i<100 && s->carry.phase;++i) CHECK(step(s,0,0,error,sizeof(error)));
    CHECK(!s->carry.phase && s->house.held_item.id==picked);
    CHECK(load_area(s,s->data_root,128,error,sizeof(error)));s->intro.area=128;
    CHECK(step(s,0,0,error,sizeof(error)));
    const HmbtnIntroActor *l=&s->village.people[npc].actor;CHECK(l->visible);
    memcpy(s->intro.player.position,l->position,sizeof(l->position));s->intro.player.position[2]+=300;s->intro.player.facing=0;
    memcpy(s->intro.camera,s->intro.player.position,sizeof(s->intro.camera));s->control.mode=0;
    CHECK(step(s,HMBTN_PAD_CROSS,0,error,sizeof(error)) && s->talk.active && s->feed_shop.phase==HMBTN_SHOP_OFF && !s->carry.phase);
    const HmbtnFarmClock before=s->clock;const unsigned discarded=s->carry.discarded;
    for(unsigned i=0;i<60;++i) CHECK(step(s,HMBTN_PAD_START,HMBTN_PAD_UP,error,sizeof(error)));
    CHECK(s->talk.active && s->menu.mode==HMBTN_MENU_NONE && !memcmp(&before,&s->clock,sizeof(before)));
    CHECK(hmbtn_scene_preview_draw(s,host.renderer,0,0,0,error,sizeof(error)));
    if(argc==3) {
        SDL_Surface *pixels=SDL_RenderReadPixels(host.renderer,NULL);CHECK(pixels);
        const bool saved=SDL_SaveBMP(pixels,argv[2]);SDL_DestroySurface(pixels);CHECK(saved);
    }
    for(unsigned i=0;i<3000 && s->talk.active;++i) CHECK(step(s,HMBTN_PAD_CROSS,0,error,sizeof(error)));
    CHECK(!s->talk.active && !s->talk.script.boundary && !s->house.held_item.id && !s->control.carrying
        && !s->control.carrying_big && !s->carry.phase && s->carry.discarded==discarded);
    printf("%u gift cases and %u BIN message requests: first/repeat, birthdays, wrapping, losses, passive reactions and gathered present passed.\n",cases,messages);
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

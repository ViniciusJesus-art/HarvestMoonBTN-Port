/* Imported conversation bytecode and text records; state changes below are
 * fixtures. No copyrighted message or event blocks are stored in this test. */
#include "scene_preview.c"
#include "sdl_host.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Conversation line %d: %s; %s\n",__LINE__,#x,error);goto fail; } } while(0)
static bool step(HmbtnScenePreview *s,uint32_t pressed,uint32_t held,char *error,size_t size)
{
    hmbtn_scene_data_tick(&s->scene);s->pending_pressed=pressed;
    bool ok=opening_tick(s,held,error,size);s->pending_pressed=0;return ok;
}
int main(int argc,char **argv)
{
    if(argc<2 || argc>3) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};unsigned messages=0;
    CHECK(hmbtn_host_open(&host));s=hmbtn_scene_area_create(host.renderer,argv[1],128,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,0,error,sizeof(error)));
    /* Read every common conversation block and run first/repeat/affinity paths
     * outside the shop. Stop honestly on any non-native story action. */
    for(unsigned id=1;id<37;++id) {
        if(id==27 || id==30 || id==33 || id==36) continue; /* Event-only actors. */
        for(unsigned repeat=0;repeat<3;++repeat) {
            HmbtnVillage v;hmbtn_village_init(&v,NULL,0);
            if(id==32) v.flags[0xE7]=true; /* Ordinary talk after the fishing-rod introduction. */
            v.people[id].actor.visible=true;v.people[id].location=54;
            v.people[id].schedule[0][2]=54;v.people[id].actor.facing=3;
            if(repeat) v.people[id].talk_flags=0x610;
            if(repeat==2) { v.people[id].friendship=220;v.people[id].affection=40000; }
            const HmbtnFarmClock clock={0,0,2,2,20,0,0};
            uint8_t block[8192];uint32_t offset;size_t size;
            CHECK(hmbtn_village_talk_range(id,&offset,&size));
            CHECK(hmbtn_resource_read(argv[1],"A_FILE/12_event_s.bin",offset,block,size,error,sizeof(error)));
            CHECK(hmbtn_village_talk_open(&s->talk,id,block,size,&v,&s->intro.player,&clock,&s->dialogue,&s->house,error,sizeof(error)));
            unsigned seen=0;
            for(unsigned frame=0;frame<4000 && s->talk.active;++frame) {
                CHECK(village_conversation(s,HMBTN_PAD_CROSS,HMBTN_PAD_CROSS,error,sizeof(error)));
                if(s->talk.pending && s->talk.loaded && s->talk.message_pc!=seen) {
                    seen=s->talk.message_pc;++messages;
                    /* Preserve original words/control codes; only runtime name
                     * placeholders expand, as in the original text engine. */
                    static const char *archives[]={"A_FILE/02_messege0.bin","A_FILE/03_messege1.bin",
                        "A_FILE/04_messege2.bin","A_FILE/05_messege3.bin","A_FILE/06_messege4.bin"};
                    uint8_t original[8192];HmbtnMessage expected;
                    CHECK(s->talk.bank<5);
                    CHECK(hmbtn_resource_read(argv[1],archives[s->talk.bank],s->talk.request*8192u,
                        original,sizeof(original),error,sizeof(error)));
                    CHECK(hmbtn_feed_shop_expand(original,sizeof(original),&s->dialogue));
                    CHECK(hmbtn_message_open(&expected,original,sizeof(original),error,sizeof(error)));
                    CHECK(expected.count==s->dialogue.message.count && !memcmp(expected.words,
                        s->dialogue.message.words,expected.count*sizeof(*expected.words)));
                    CHECK(hmbtn_scene_preview_draw(s,host.renderer,0,0,0,error,sizeof(error)));
                }
            }
            if(s->talk.script.boundary) fprintf(stderr,"Unsupported NPC %u repeat %u type %u step %u\n",id,repeat,s->talk.boundary_type,s->talk.script.pc);
            if(id==35) { /* Won's shop is an explicit native boundary. */
                CHECK(!s->talk.active && s->talk.script.boundary && s->talk.boundary_type==81);
                CHECK(v.people[id].talk_flags==(repeat ? 0x610 : 0));
            } else CHECK(!s->talk.active && !s->talk.script.boundary && seen);
            CHECK(v.people[id].actor.facing==3);
        }
    }
    /* Actual location: Lillia's closed shop yields an ordinary conversation;
     * movement, menus, schedules and clock are held until the window closes. */
    s->clock=(HmbtnFarmClock){0,0,2,2,16,0,0};sync_clock(s);
    CHECK(step(s,0,0,error,sizeof(error)));
    HmbtnIntroActor *npc=&s->village.people[18].actor;CHECK(npc->visible);
    memcpy(s->intro.player.position,npc->position,sizeof(npc->position));s->intro.player.position[2]+=300;
    s->intro.player.facing=0;s->control.mode=0;
    CHECK(hmbtn_village_target(&s->village,&s->intro.player)==18);
    CHECK(step(s,HMBTN_PAD_CROSS,0,error,sizeof(error)) && s->talk.active);
    const HmbtnFarmClock before=s->clock;int32_t position[3];memcpy(position,s->intro.player.position,sizeof(position));
    for(unsigned i=0;i<300;++i) CHECK(step(s,HMBTN_PAD_START,HMBTN_PAD_UP,error,sizeof(error)));
    CHECK(s->talk.active && s->menu.mode==HMBTN_MENU_NONE && !memcmp(&before,&s->clock,sizeof(before))
        && !memcmp(position,s->intro.player.position,sizeof(position)));
    CHECK(hmbtn_scene_preview_draw(s,host.renderer,0,0,0,error,sizeof(error)));
    if(argc==3) {
        SDL_Surface *pixels=SDL_RenderReadPixels(host.renderer,NULL);CHECK(pixels);
        bool saved=SDL_SaveBMP(pixels,argv[2]);SDL_DestroySurface(pixels);CHECK(saved);
    }
    for(unsigned i=0;i<2000 && s->talk.active;++i) CHECK(step(s,HMBTN_PAD_CROSS,0,error,sizeof(error)));
    CHECK(!s->talk.active && !s->talk.script.boundary && s->house.mode==HMBTN_HOUSE_FREE);
    printf("%u original message requests: first/repeat/affinity dialogue, portraits, return and paused controls passed.\n",messages);
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

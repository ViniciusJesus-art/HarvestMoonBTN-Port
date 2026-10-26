/* Imported conversation bytecode and text records; state changes below are
 * fixtures. No copyrighted message or event blocks are stored in this test. */
#include "scene_fixture.h"
#include "sdl_host.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Conversation line %d: %s; %s\n",__LINE__,#x,error);goto fail; } } while(0)
static bool step(HmbtnScenePreview *s,uint32_t pressed,uint32_t held,char *error,size_t size)
{
    return hmbtn_scene_preview_tick(s,(HmbtnLogicalInput){pressed,held},error,size);
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
            v.weather=&hmbtn_scene_preview_session(s)->game.weather;
            if(id==32) v.flags[0xE7]=true; /* Ordinary talk after the fishing-rod introduction. */
            v.people[id].actor.visible=true;v.people[id].location=54;
            v.people[id].schedule[0][2]=54;v.people[id].actor.facing=3;
            if(repeat) v.people[id].talk_flags=0x610;
            if(repeat==2) { v.people[id].friendship=220;v.people[id].affection=40000; }
            const HmbtnFarmClock clock={0,0,2,2,20,0,0};
            uint8_t block[8192];uint32_t offset;size_t size;
            CHECK(hmbtn_village_talk_range(id,&offset,&size));
            CHECK(hmbtn_resource_read(argv[1],"A_FILE/12_event_s.bin",offset,block,size,error,sizeof(error)));
            CHECK(hmbtn_village_talk_open(&hmbtn_scene_preview_session(s)->talk,id,block,size,&v,&hmbtn_scene_preview_session(s)->intro.player,&clock,hmbtn_game_registration_const(&hmbtn_scene_preview_session(s)->game),&hmbtn_scene_preview_session(s)->house,error,sizeof(error)));
            unsigned seen=0;
            for(unsigned frame=0;frame<4000 && hmbtn_scene_preview_session(s)->talk.active;++frame) {
                CHECK(hmbtn_scene_preview_tick(s,(HmbtnLogicalInput){HMBTN_PAD_CROSS,HMBTN_PAD_CROSS},error,sizeof(error)));
                if(hmbtn_scene_preview_session(s)->talk.pending && hmbtn_scene_preview_session(s)->talk.loaded && hmbtn_scene_preview_session(s)->talk.message_pc!=seen) {
                    seen=hmbtn_scene_preview_session(s)->talk.message_pc;++messages;
                    /* Preserve original words/control codes; only runtime name
                     * placeholders expand, as in the original text engine. */
                    static const char *archives[]={"A_FILE/02_messege0.bin","A_FILE/03_messege1.bin",
                        "A_FILE/04_messege2.bin","A_FILE/05_messege3.bin","A_FILE/06_messege4.bin"};
                    uint8_t original[8192];HmbtnMessage expected;
                    CHECK(hmbtn_scene_preview_session(s)->talk.bank<5);
                    CHECK(hmbtn_resource_read(argv[1],archives[hmbtn_scene_preview_session(s)->talk.bank],hmbtn_scene_preview_session(s)->talk.request*8192u,
                        original,sizeof(original),error,sizeof(error)));
                    CHECK(hmbtn_feed_shop_expand(original,sizeof(original),hmbtn_game_registration_const(&hmbtn_scene_preview_session(s)->game)));
                    CHECK(hmbtn_message_open(&expected,original,sizeof(original),error,sizeof(error)));
                    CHECK(expected.count==hmbtn_scene_preview_session(s)->dialogue.message.count && !memcmp(expected.words,
                        hmbtn_scene_preview_session(s)->dialogue.message.words,expected.count*sizeof(*expected.words)));
                    CHECK(hmbtn_scene_preview_draw(s,host.renderer,error,sizeof(error)));
                }
            }
            if(hmbtn_scene_preview_session(s)->talk.script.boundary) fprintf(stderr,"Unsupported NPC %u repeat %u type %u step %u\n",id,repeat,hmbtn_scene_preview_session(s)->talk.boundary_type,hmbtn_scene_preview_session(s)->talk.script.pc);
            if(id==35) { /* Won's shop is an explicit native boundary. */
                CHECK(!hmbtn_scene_preview_session(s)->talk.active && hmbtn_scene_preview_session(s)->talk.script.boundary && hmbtn_scene_preview_session(s)->talk.boundary_type==81);
                CHECK(v.people[id].talk_flags==(repeat ? 0x610 : 0));
            } else CHECK(!hmbtn_scene_preview_session(s)->talk.active && !hmbtn_scene_preview_session(s)->talk.script.boundary && seen);
            CHECK(v.people[id].actor.facing==3);
        }
    }
    /* Actual location: Lillia's closed shop yields an ordinary conversation;
     * movement, menus, schedules and clock are held until the window closes. */
    hmbtn_scene_preview_session(s)->game.clock=(HmbtnFarmClock){0,0,2,2,16,0,0};
    CHECK(step(s,0,0,error,sizeof(error)));
    HmbtnIntroActor *npc=&hmbtn_scene_preview_session(s)->village.people[18].actor;CHECK(npc->visible);
    memcpy(hmbtn_scene_preview_session(s)->intro.player.position,npc->position,sizeof(npc->position));hmbtn_scene_preview_session(s)->intro.player.position[2]+=300;
    hmbtn_scene_preview_session(s)->intro.player.facing=0;hmbtn_scene_preview_session(s)->control.mode=0;
    CHECK(hmbtn_village_target(&hmbtn_scene_preview_session(s)->village,&hmbtn_scene_preview_session(s)->intro.player)==18);
    CHECK(step(s,HMBTN_PAD_CROSS,0,error,sizeof(error)) && hmbtn_scene_preview_session(s)->talk.active);
    const HmbtnFarmClock before=hmbtn_scene_preview_session(s)->game.clock;int32_t position[3];memcpy(position,hmbtn_scene_preview_session(s)->intro.player.position,sizeof(position));
    for(unsigned i=0;i<300;++i) CHECK(step(s,HMBTN_PAD_START,HMBTN_PAD_UP,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->talk.active && hmbtn_scene_preview_session(s)->menu.mode==HMBTN_MENU_NONE && !memcmp(&before,&hmbtn_scene_preview_session(s)->game.clock,sizeof(before))
        && !memcmp(position,hmbtn_scene_preview_session(s)->intro.player.position,sizeof(position)));
    CHECK(hmbtn_scene_preview_draw(s,host.renderer,error,sizeof(error)));
    if(argc==3) {
        SDL_Surface *pixels=SDL_RenderReadPixels(host.renderer,NULL);CHECK(pixels);
        bool saved=SDL_SaveBMP(pixels,argv[2]);SDL_DestroySurface(pixels);CHECK(saved);
    }
    for(unsigned i=0;i<2000 && hmbtn_scene_preview_session(s)->talk.active;++i) CHECK(step(s,HMBTN_PAD_CROSS,0,error,sizeof(error)));
    CHECK(!hmbtn_scene_preview_session(s)->talk.active && !hmbtn_scene_preview_session(s)->talk.script.boundary && hmbtn_scene_preview_session(s)->house.mode==HMBTN_HOUSE_FREE);
    /* Complete BIN dialogue, repeat the same day, then actually sleep. The
     * original daily flags determine the gain, which is credited only once. */
    CHECK(hmbtn_scene_preview_session(s)->village.people[18].talk_flags&HMBTN_VILLAGER_DAILY_GAIN);
    const unsigned friendship=hmbtn_scene_preview_session(s)->village.people[18].friendship;
    CHECK(step(s,HMBTN_PAD_CROSS,0,error,sizeof(error)) && hmbtn_scene_preview_session(s)->talk.active);
    for(unsigned i=0;i<2000 && hmbtn_scene_preview_session(s)->talk.active;++i) CHECK(step(s,HMBTN_PAD_CROSS,0,error,sizeof(error)));
    CHECK(!hmbtn_scene_preview_session(s)->talk.active && !hmbtn_scene_preview_session(s)->talk.script.boundary && hmbtn_scene_preview_session(s)->village.people[18].friendship==friendship);
    CHECK(hmbtn_scene_preview_load_area(s,80,error,sizeof(error)));hmbtn_scene_preview_session(s)->intro.area=80;
    const unsigned yesterday=hmbtn_scene_preview_session(s)->game.clock.day;
    hmbtn_scene_preview_session(s)->house.mode=HMBTN_HOUSE_SLEEP;
    for(unsigned i=0;i<1500 && (hmbtn_scene_preview_session(s)->game.clock.day==yesterday || hmbtn_scene_preview_session(s)->morning_fade);++i) CHECK(step(s,0,0,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->game.clock.day==yesterday+1 && !hmbtn_scene_preview_session(s)->sleep.active && !hmbtn_scene_preview_session(s)->morning_fade && hmbtn_scene_preview_session(s)->area==80);
    CHECK(hmbtn_scene_preview_session(s)->village.people[18].friendship==friendship+1 && hmbtn_scene_preview_session(s)->village.people[18].talk_flags==0x10);
    printf("%u original message requests: first/repeat/affinity dialogue, portraits, paused controls and conversation-to-sleep friendship passed.\n",messages);
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

/* Installed trigger tables/scripts/textures/ANI and messages. Dates, affinity
 * and player placement are test fixtures; gameplay still chooses the event. */
#include "scene_fixture.h"
#include "sdl_host.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Visit line %d scenario %u: %s; %s (event %u step %u)\n",__LINE__,scenario,#x,error,s ? hmbtn_scene_preview_session(s)->intro.event : 0,s ? hmbtn_scene_preview_session(s)->intro.script.pc : 0);goto fail; } } while(0)
static bool step(HmbtnScenePreview *s,uint32_t pressed,uint32_t held,char *error,size_t size)
{
    return hmbtn_scene_preview_tick(s,(HmbtnLogicalInput){pressed,held},error,size);
}
static bool original_message(HmbtnScenePreview *s,char *error,size_t size)
{
    uint8_t record[8192];HmbtnMessage expected;
    const HmbtnTextResourceId id={HMBTN_TEXT_RECORD,1,hmbtn_scene_preview_session(s)->intro.message};
    return hmbtn_text_resource_read(hmbtn_scene_preview_resources(s).root,id,record,sizeof(record),error,size)
        && hmbtn_feed_shop_expand(record,sizeof(record),hmbtn_game_registration_const(&hmbtn_scene_preview_session(s)->game))
        && hmbtn_message_open(&expected,record,sizeof(record),error,size)
        && expected.count==hmbtn_scene_preview_session(s)->dialogue.message.count
        && !memcmp(expected.words,hmbtn_scene_preview_session(s)->dialogue.message.words,expected.count*sizeof(*expected.words));
}
int main(int argc,char **argv)
{
    if(argc<2 || argc>3) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};unsigned scenario=0;
    CHECK(hmbtn_host_open(&host));
    /* All nine cooking requests, refusal, timeout, actual scene exit, pie yes/no. */
    for(scenario=0;scenario<14;++scenario) {
        const bool pie=scenario>=12,refuse=scenario==9 || scenario==13,timeout=scenario==10,leave=scenario==11;
        HmbtnFarmClock clock={pie ? 0 : scenario<9 ? scenario : 0,pie ? 1 : 0,pie ? 5 : 14,0,6,0,0};
        s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&clock,error,sizeof(error));CHECK(s);
        /* Complete the host handoff before installing friendship fixtures. */
        CHECK(step(s,0,0,error,sizeof(error)));
        const HmbtnInventory inventory_before=*hmbtn_scene_preview_session(s)->house.inventory;
        for(unsigned i=1;i<HMBTN_VILLAGERS;++i) { hmbtn_scene_preview_session(s)->village.people[i].friendship=100;hmbtn_scene_preview_session(s)->village.people[i].affection=5000; }
        for(unsigned i=0;i<7;++i) hmbtn_scene_preview_session(s)->menu.stats.helper_affection[i]=100;
        CHECK(hmbtn_farm_visit_trigger(&hmbtn_scene_preview_session(s)->triggers,&hmbtn_scene_preview_session(s)->village,&hmbtn_scene_preview_session(s)->game.clock,0,hmbtn_scene_preview_session(s)->previous_place,2)==(pie ? 128u : 288u));
        HmbtnFarmClock wrong=hmbtn_scene_preview_session(s)->game.clock;wrong.hour=23;
        CHECK(!hmbtn_farm_visit_trigger(&hmbtn_scene_preview_session(s)->triggers,&hmbtn_scene_preview_session(s)->village,&wrong,0,hmbtn_scene_preview_session(s)->previous_place,2));
        if(pie) {
            hmbtn_scene_preview_session(s)->village.people[17].friendship=99;
            CHECK(!hmbtn_farm_visit_trigger(&hmbtn_scene_preview_session(s)->triggers,&hmbtn_scene_preview_session(s)->village,&hmbtn_scene_preview_session(s)->game.clock,0,hmbtn_scene_preview_session(s)->previous_place,2));
            hmbtn_scene_preview_session(s)->village.people[17].friendship=100;hmbtn_scene_preview_session(s)->village.days.count0[8]=3;
            CHECK(!hmbtn_farm_visit_trigger(&hmbtn_scene_preview_session(s)->triggers,&hmbtn_scene_preview_session(s)->village,&hmbtn_scene_preview_session(s)->game.clock,0,hmbtn_scene_preview_session(s)->previous_place,2));
            hmbtn_scene_preview_session(s)->village.days.count0[8]=255;wrong=hmbtn_scene_preview_session(s)->game.clock;wrong.weekday=1;
            CHECK(!hmbtn_farm_visit_trigger(&hmbtn_scene_preview_session(s)->triggers,&hmbtn_scene_preview_session(s)->village,&wrong,0,hmbtn_scene_preview_session(s)->previous_place,2));
        } else {
            CHECK(!hmbtn_farm_visit_trigger(&hmbtn_scene_preview_session(s)->triggers,&hmbtn_scene_preview_session(s)->village,&hmbtn_scene_preview_session(s)->game.clock,0,1,2));
            CHECK(!hmbtn_farm_visit_trigger(&hmbtn_scene_preview_session(s)->triggers,&hmbtn_scene_preview_session(s)->village,&hmbtn_scene_preview_session(s)->game.clock,0,2,0));
        }
        CHECK(step(s,0,0,error,sizeof(error)) && hmbtn_scene_preview_session(s)->visit.active && hmbtn_scene_preview_session(s)->visit.event==(pie ? 128u : 288u));
        CHECK(hmbtn_scene_preview_session(s)->village.event_actor==&hmbtn_scene_preview_session(s)->intro.mayor && hmbtn_scene_preview_session(s)->village.event_actor_id==17);
        bool messages[2042]={0},spoke=false,selected=false,captured=false,exited=false,walked=false;
        unsigned records=0;unsigned last=UINT32_MAX;
        for(unsigned frame=0;frame<7000 && hmbtn_scene_preview_session(s)->visit.active;++frame) {
            CHECK(!hmbtn_scene_preview_session(s)->intro.script.boundary);
            if(hmbtn_scene_preview_session(s)->intro.message && hmbtn_scene_preview_session(s)->runtime.loaded_message==hmbtn_scene_preview_session(s)->intro.message && last!=hmbtn_scene_preview_session(s)->intro.message) {
                CHECK(hmbtn_scene_preview_session(s)->intro.message<2042 && original_message(s,error,sizeof(error)));
                messages[hmbtn_scene_preview_session(s)->intro.message]=true;last=hmbtn_scene_preview_session(s)->intro.message;++records;
            }
            if(!hmbtn_scene_preview_session(s)->intro.message) last=UINT32_MAX;
            uint32_t pressed=hmbtn_scene_preview_session(s)->intro.message ? HMBTN_PAD_CROSS : 0,held=HMBTN_PAD_CROSS;
            if(hmbtn_scene_preview_session(s)->dialogue.message.choices && hmbtn_scene_preview_session(s)->dialogue.message.phase==HMBTN_MESSAGE_WAIT_END && !selected) {
                CHECK(hmbtn_scene_preview_session(s)->dialogue.step==100);
                selected=true;
                if(refuse) pressed=HMBTN_PAD_DOWN;
                if(argc==3 && !captured && (scenario==0 || scenario==12)) {
                    CHECK(hmbtn_scene_preview_draw(s,host.renderer,error,sizeof(error)));
                    SDL_Surface *pixels=SDL_RenderReadPixels(host.renderer,NULL);CHECK(pixels);
                    char path[1024];snprintf(path,sizeof(path),"%s/%s.bmp",argv[2],pie ? "apple-pie" : "cooking-request");
                    const bool saved=SDL_SaveBMP(pixels,path);SDL_DestroySurface(pixels);CHECK(saved);captured=true;
                }
            }
            if(hmbtn_scene_preview_session(s)->control.enabled && hmbtn_scene_preview_session(s)->visit.free && !hmbtn_scene_preview_session(s)->intro.message && hmbtn_scene_preview_session(s)->intro.mayor.position[2]<=3959 && !spoke) {
                if(timeout) {
                    /* Let the real clock reach the original one-hour deadline. */
                    CHECK(hmbtn_scene_preview_session(s)->game.clock.hour<8);
                } else if(leave && !exited) {
                    CHECK(hmbtn_scene_preview_load_area(s,80,error,sizeof(error)));hmbtn_scene_preview_session(s)->intro.area=hmbtn_scene_preview_session(s)->area;
                    hmbtn_scene_preview_session(s)->intro.player.position[0]=-90;hmbtn_scene_preview_session(s)->intro.player.position[1]=0;hmbtn_scene_preview_session(s)->intro.player.position[2]=541;
                    exited=true;
                } else if(!leave) {
                    memcpy(hmbtn_scene_preview_session(s)->intro.player.position,hmbtn_scene_preview_session(s)->intro.mayor.position,sizeof(hmbtn_scene_preview_session(s)->intro.player.position));
                    hmbtn_scene_preview_session(s)->intro.player.position[2]-=300;hmbtn_scene_preview_session(s)->intro.player.facing=2;hmbtn_scene_preview_session(s)->control.mode=0;
                    CHECK(hmbtn_village_target(&hmbtn_scene_preview_session(s)->village,&hmbtn_scene_preview_session(s)->intro.player)==17);
                    /* The scripted actor participates in the established NPC probe. */
                    int32_t next[3];memcpy(next,hmbtn_scene_preview_session(s)->intro.player.position,sizeof(next));next[2]+=150;
                    CHECK(hmbtn_village_blocked(&hmbtn_scene_preview_session(s)->village,hmbtn_scene_preview_session(s)->intro.player.position,next));
                    HmbtnIntroActor runner=hmbtn_scene_preview_session(s)->intro.player;
                    HmbtnHouseControl probe={.mode=2,.enabled=true,.npc_blocked=hmbtn_village_blocked,.collision_context=&hmbtn_scene_preview_session(s)->village};
                    for(unsigned i=0;i<8;++i) hmbtn_farm_control_tick(&probe,&runner,&(*hmbtn_scene_preview_resources(s).scene).terrain,&hmbtn_scene_preview_session(s)->field,HMBTN_PAD_UP);
                    CHECK(runner.position[2]<hmbtn_scene_preview_session(s)->intro.mayor.position[2]-190);
                    for(unsigned i=0;i<12;++i) hmbtn_farm_control_tick(&probe,&runner,&(*hmbtn_scene_preview_resources(s).scene).terrain,&hmbtn_scene_preview_session(s)->field,HMBTN_PAD_UP|HMBTN_PAD_CIRCLE);
                    CHECK(runner.position[2]>hmbtn_scene_preview_session(s)->intro.mayor.position[2]+200);
                    pressed=HMBTN_PAD_CROSS;spoke=true;
                }
            }
            if(!walked && hmbtn_scene_preview_session(s)->control.enabled && !hmbtn_scene_preview_session(s)->intro.message && hmbtn_scene_preview_session(s)->intro.mayor.position[2]>5000) {
                int32_t before[3];memcpy(before,hmbtn_scene_preview_session(s)->intro.player.position,sizeof(before));
                CHECK(step(s,0,HMBTN_PAD_RIGHT|HMBTN_PAD_CIRCLE,error,sizeof(error)));
                CHECK(step(s,0,HMBTN_PAD_RIGHT|HMBTN_PAD_CIRCLE,error,sizeof(error)));
                const HmbtnAnimationPlayer animation=hmbtn_scene_preview_session(s)->intro.player.sprite;
                CHECK(step(s,0,HMBTN_PAD_RIGHT|HMBTN_PAD_CIRCLE,error,sizeof(error)));
                CHECK(memcmp(&animation,&hmbtn_scene_preview_session(s)->intro.player.sprite,sizeof(animation)));
                walked=memcmp(before,hmbtn_scene_preview_session(s)->intro.player.position,sizeof(before))!=0;
            }
            if(hmbtn_scene_preview_session(s)->intro.message) {
                const HmbtnFarmClock before=hmbtn_scene_preview_session(s)->game.clock;const int32_t position[3]={hmbtn_scene_preview_session(s)->intro.player.position[0],hmbtn_scene_preview_session(s)->intro.player.position[1],hmbtn_scene_preview_session(s)->intro.player.position[2]};
                CHECK(step(s,pressed,held,error,sizeof(error)));
                if(hmbtn_scene_preview_session(s)->intro.message) CHECK(!memcmp(&hmbtn_scene_preview_session(s)->game.clock,&before,sizeof(before))
                    && !memcmp(position,hmbtn_scene_preview_session(s)->intro.player.position,sizeof(position)));
            } else CHECK(step(s,pressed,held,error,sizeof(error)));
        }
        CHECK(!hmbtn_scene_preview_session(s)->visit.active && hmbtn_scene_preview_session(s)->intro.script.finished && !hmbtn_scene_preview_session(s)->intro.script.boundary && hmbtn_scene_preview_session(s)->control.enabled);
        CHECK(!hmbtn_scene_preview_session(s)->village.event_actor && !hmbtn_scene_preview_session(s)->intro.mayor.visible && !hmbtn_scene_preview_session(s)->intro.game_command);
        CHECK(!hmbtn_scene_preview_session(s)->village.flags[3] && !hmbtn_scene_preview_session(s)->village.flags[4] && !hmbtn_scene_preview_session(s)->village.flags[5] && !hmbtn_scene_preview_session(s)->village.flags[0xB0]);
        CHECK(!memcmp(&hmbtn_scene_preview_session(s)->house.inventory->held_item,&inventory_before.held_item,sizeof(hmbtn_scene_preview_session(s)->house.inventory->held_item))
            && !memcmp(&hmbtn_scene_preview_session(s)->house.inventory->equipped,&inventory_before.equipped,sizeof(hmbtn_scene_preview_session(s)->house.inventory->equipped))
            && !memcmp(hmbtn_scene_preview_session(s)->house.inventory->bag,inventory_before.bag,sizeof(hmbtn_scene_preview_session(s)->house.inventory->bag)));
        if(pie) {
            CHECK(messages[385] && messages[386] && messages[refuse ? 389 : 387] && selected && spoke);
            CHECK(messages[388]==!refuse && hmbtn_scene_preview_session(s)->village.flags[203]==!refuse && hmbtn_scene_preview_session(s)->village.flags[379]);
            CHECK(hmbtn_scene_preview_session(s)->village.people[17].friendship==(refuse ? 80u : 120u) && hmbtn_scene_preview_session(s)->village.days.count0[13]==0);
        } else {
            CHECK(messages[2041] && !hmbtn_scene_preview_session(s)->village.flags[539] && hmbtn_scene_preview_session(s)->village.days.count0[26]==0);
            if(timeout || leave) CHECK(!selected && !spoke && records==1 && !hmbtn_scene_preview_session(s)->village.flags[217]);
            else {
                static const unsigned requests[9]={926,872,898,902,906,910,914,918,922};
                CHECK(messages[requests[(hmbtn_scene_preview_session(s)->game.clock.year+1)%9]] && messages[refuse ? 874 : 873] && selected && spoke);
                CHECK(hmbtn_scene_preview_session(s)->village.flags[217]==!refuse && hmbtn_scene_preview_session(s)->village.people[17].friendship==(refuse ? 80u : 100u));
                CHECK(hmbtn_scene_preview_session(s)->village.people[3].affection==(refuse ? 4000u : 5000u)
                    && hmbtn_scene_preview_session(s)->menu.stats.helper_affection[0]==(refuse ? 80u : 100u));
            }
        }
        if(timeout) CHECK(hmbtn_scene_preview_session(s)->game.clock.hour>=7 && records==1);
        if(leave) CHECK(hmbtn_scene_preview_session(s)->area==80 && exited);
        /* Re-entering on the same date never repeats either visit. */
        if(hmbtn_scene_preview_session(s)->area!=0) { CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)));hmbtn_scene_preview_session(s)->intro.area=0; }
        if(!pie) {
            HmbtnFarmClock future=hmbtn_scene_preview_session(s)->game.clock;++future.year;future.hour=6;future.minute=0;
            hmbtn_scene_preview_session(s)->village.days.count0[26]=1;
            CHECK(hmbtn_farm_visit_trigger(&hmbtn_scene_preview_session(s)->triggers,&hmbtn_scene_preview_session(s)->village,&future,0,2,2)==288);
            hmbtn_scene_preview_session(s)->village.days.count0[26]=0;
        }
        hmbtn_scene_preview_session(s)->trigger_entry=true;
        for(unsigned i=0;i<40;++i) CHECK(step(s,0,0,error,sizeof(error)) && !hmbtn_scene_preview_session(s)->visit.active);
        hmbtn_scene_preview_destroy(s);s=NULL;
    }
    hmbtn_host_close(&host);puts("Original farm visits: nine requests, yes/no, timeout, exit, pie request and exact BIN dialogue passed.");return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

/* Installed trigger tables/scripts/textures/ANI and messages. Dates, affinity
 * and player placement are test fixtures; gameplay still chooses the event. */
#include "scene_preview.c"
#include "sdl_host.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Visit line %d scenario %u: %s; %s (event %u step %u)\n",__LINE__,scenario,#x,error,s ? s->intro.event : 0,s ? s->intro.script.pc : 0);goto fail; } } while(0)
static bool step(HmbtnScenePreview *s,uint32_t pressed,uint32_t held,char *error,size_t size)
{
    s->pending_pressed=pressed;hmbtn_scene_data_tick(&s->scene);
    const bool ok=opening_tick(s,held,error,size);s->pending_pressed=0;return ok;
}
static bool original_message(HmbtnScenePreview *s,char *error,size_t size)
{
    uint8_t record[8192];HmbtnMessage expected;
    const HmbtnTextResourceId id={HMBTN_TEXT_RECORD,1,s->intro.message};
    return hmbtn_text_resource_read(s->data_root,id,record,sizeof(record),error,size)
        && hmbtn_feed_shop_expand(record,sizeof(record),hmbtn_game_registration_const(&s->session->game))
        && hmbtn_message_open(&expected,record,sizeof(record),error,size)
        && expected.count==s->dialogue.message.count
        && !memcmp(expected.words,s->dialogue.message.words,expected.count*sizeof(*expected.words));
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
        const HmbtnInventory inventory_before=*s->house.inventory;
        for(unsigned i=1;i<HMBTN_VILLAGERS;++i) { s->session->village.people[i].friendship=100;s->session->village.people[i].affection=5000; }
        for(unsigned i=0;i<7;++i) s->menu.stats.helper_affection[i]=100;
        CHECK(hmbtn_farm_visit_trigger(&s->triggers,&s->session->village,&s->session->game.clock,0,s->previous_place,2)==(pie ? 128u : 288u));
        HmbtnFarmClock wrong=s->session->game.clock;wrong.hour=23;
        CHECK(!hmbtn_farm_visit_trigger(&s->triggers,&s->session->village,&wrong,0,s->previous_place,2));
        if(pie) {
            s->session->village.people[17].friendship=99;
            CHECK(!hmbtn_farm_visit_trigger(&s->triggers,&s->session->village,&s->session->game.clock,0,s->previous_place,2));
            s->session->village.people[17].friendship=100;s->session->village.days.count0[8]=3;
            CHECK(!hmbtn_farm_visit_trigger(&s->triggers,&s->session->village,&s->session->game.clock,0,s->previous_place,2));
            s->session->village.days.count0[8]=255;wrong=s->session->game.clock;wrong.weekday=1;
            CHECK(!hmbtn_farm_visit_trigger(&s->triggers,&s->session->village,&wrong,0,s->previous_place,2));
        } else {
            CHECK(!hmbtn_farm_visit_trigger(&s->triggers,&s->session->village,&s->session->game.clock,0,1,2));
            CHECK(!hmbtn_farm_visit_trigger(&s->triggers,&s->session->village,&s->session->game.clock,0,2,0));
        }
        CHECK(step(s,0,0,error,sizeof(error)) && s->visit.active && s->visit.event==(pie ? 128u : 288u));
        CHECK(s->session->village.event_actor==&s->intro.mayor && s->session->village.event_actor_id==17);
        bool messages[2042]={0},spoke=false,selected=false,captured=false,exited=false,walked=false;
        unsigned records=0;unsigned last=UINT32_MAX;
        for(unsigned frame=0;frame<7000 && s->visit.active;++frame) {
            CHECK(!s->intro.script.boundary);
            if(s->intro.message && s->loaded_message==s->intro.message && last!=s->intro.message) {
                CHECK(s->intro.message<2042 && original_message(s,error,sizeof(error)));
                messages[s->intro.message]=true;last=s->intro.message;++records;
            }
            if(!s->intro.message) last=UINT32_MAX;
            uint32_t pressed=s->intro.message ? HMBTN_PAD_CROSS : 0,held=HMBTN_PAD_CROSS;
            if(s->dialogue.message.choices && s->dialogue.message.phase==HMBTN_MESSAGE_WAIT_END && !selected) {
                CHECK(s->dialogue.step==100);
                selected=true;
                if(refuse) pressed=HMBTN_PAD_DOWN;
                if(argc==3 && !captured && (scenario==0 || scenario==12)) {
                    CHECK(hmbtn_scene_preview_draw(s,host.renderer,0,0,0,error,sizeof(error)));
                    SDL_Surface *pixels=SDL_RenderReadPixels(host.renderer,NULL);CHECK(pixels);
                    char path[1024];snprintf(path,sizeof(path),"%s/%s.bmp",argv[2],pie ? "apple-pie" : "cooking-request");
                    const bool saved=SDL_SaveBMP(pixels,path);SDL_DestroySurface(pixels);CHECK(saved);captured=true;
                }
            }
            if(s->control.enabled && s->visit.free && !s->intro.message && s->intro.mayor.position[2]<=3959 && !spoke) {
                if(timeout) {
                    /* Let the real clock reach the original one-hour deadline. */
                    CHECK(s->session->game.clock.hour<8);
                } else if(leave && !exited) {
                    CHECK(load_area(s,argv[1],80,error,sizeof(error)));s->intro.area=s->area;
                    s->intro.player.position[0]=-90;s->intro.player.position[1]=0;s->intro.player.position[2]=541;
                    exited=true;
                } else if(!leave) {
                    memcpy(s->intro.player.position,s->intro.mayor.position,sizeof(s->intro.player.position));
                    s->intro.player.position[2]-=300;s->intro.player.facing=2;s->control.mode=0;
                    CHECK(hmbtn_village_target(&s->session->village,&s->intro.player)==17);
                    /* The scripted actor participates in the established NPC probe. */
                    int32_t next[3];memcpy(next,s->intro.player.position,sizeof(next));next[2]+=150;
                    CHECK(hmbtn_village_blocked(&s->session->village,s->intro.player.position,next));
                    HmbtnIntroActor runner=s->intro.player;
                    HmbtnHouseControl probe={.mode=2,.enabled=true,.npc_blocked=hmbtn_village_blocked,.collision_context=&s->session->village};
                    for(unsigned i=0;i<8;++i) hmbtn_farm_control_tick(&probe,&runner,&s->scene.terrain,&s->session->field,HMBTN_PAD_UP);
                    CHECK(runner.position[2]<s->intro.mayor.position[2]-190);
                    for(unsigned i=0;i<12;++i) hmbtn_farm_control_tick(&probe,&runner,&s->scene.terrain,&s->session->field,HMBTN_PAD_UP|HMBTN_PAD_CIRCLE);
                    CHECK(runner.position[2]>s->intro.mayor.position[2]+200);
                    pressed=HMBTN_PAD_CROSS;spoke=true;
                }
            }
            if(!walked && s->control.enabled && !s->intro.message && s->intro.mayor.position[2]>5000) {
                int32_t before[3];memcpy(before,s->intro.player.position,sizeof(before));
                CHECK(step(s,0,HMBTN_PAD_RIGHT|HMBTN_PAD_CIRCLE,error,sizeof(error)));
                CHECK(step(s,0,HMBTN_PAD_RIGHT|HMBTN_PAD_CIRCLE,error,sizeof(error)));
                const HmbtnAnimationPlayer animation=s->intro.player.sprite;
                CHECK(step(s,0,HMBTN_PAD_RIGHT|HMBTN_PAD_CIRCLE,error,sizeof(error)));
                CHECK(memcmp(&animation,&s->intro.player.sprite,sizeof(animation)));
                walked=memcmp(before,s->intro.player.position,sizeof(before))!=0;
            }
            if(s->intro.message) {
                const HmbtnFarmClock before=s->session->game.clock;const int32_t position[3]={s->intro.player.position[0],s->intro.player.position[1],s->intro.player.position[2]};
                CHECK(step(s,pressed,held,error,sizeof(error)));
                if(s->intro.message) CHECK(!memcmp(&s->session->game.clock,&before,sizeof(before))
                    && !memcmp(position,s->intro.player.position,sizeof(position)));
            } else CHECK(step(s,pressed,held,error,sizeof(error)));
        }
        CHECK(!s->visit.active && s->intro.script.finished && !s->intro.script.boundary && s->control.enabled);
        CHECK(!s->session->village.event_actor && !s->intro.mayor.visible && !s->intro.game_command);
        CHECK(!s->session->village.flags[3] && !s->session->village.flags[4] && !s->session->village.flags[5] && !s->session->village.flags[0xB0]);
        CHECK(!memcmp(&s->house.inventory->held_item,&inventory_before.held_item,sizeof(s->house.inventory->held_item))
            && !memcmp(&s->house.inventory->equipped,&inventory_before.equipped,sizeof(s->house.inventory->equipped))
            && !memcmp(s->house.inventory->bag,inventory_before.bag,sizeof(s->house.inventory->bag)));
        if(pie) {
            CHECK(messages[385] && messages[386] && messages[refuse ? 389 : 387] && selected && spoke);
            CHECK(messages[388]==!refuse && s->session->village.flags[203]==!refuse && s->session->village.flags[379]);
            CHECK(s->session->village.people[17].friendship==(refuse ? 80u : 120u) && s->session->village.days.count0[13]==0);
        } else {
            CHECK(messages[2041] && !s->session->village.flags[539] && s->session->village.days.count0[26]==0);
            if(timeout || leave) CHECK(!selected && !spoke && records==1 && !s->session->village.flags[217]);
            else {
                static const unsigned requests[9]={926,872,898,902,906,910,914,918,922};
                CHECK(messages[requests[(s->session->game.clock.year+1)%9]] && messages[refuse ? 874 : 873] && selected && spoke);
                CHECK(s->session->village.flags[217]==!refuse && s->session->village.people[17].friendship==(refuse ? 80u : 100u));
                CHECK(s->session->village.people[3].affection==(refuse ? 4000u : 5000u)
                    && s->menu.stats.helper_affection[0]==(refuse ? 80u : 100u));
            }
        }
        if(timeout) CHECK(s->session->game.clock.hour>=7 && records==1);
        if(leave) CHECK(s->area==80 && exited);
        /* Re-entering on the same date never repeats either visit. */
        if(s->area!=0) { CHECK(load_area(s,argv[1],0,error,sizeof(error)));s->intro.area=0; }
        if(!pie) {
            HmbtnFarmClock future=s->session->game.clock;++future.year;future.hour=6;future.minute=0;
            s->session->village.days.count0[26]=1;
            CHECK(hmbtn_farm_visit_trigger(&s->triggers,&s->session->village,&future,0,2,2)==288);
            s->session->village.days.count0[26]=0;
        }
        s->trigger_entry=true;
        for(unsigned i=0;i<40;++i) CHECK(step(s,0,0,error,sizeof(error)) && !s->visit.active);
        hmbtn_scene_preview_destroy(s);s=NULL;
    }
    hmbtn_host_close(&host);puts("Original farm visits: nine requests, yes/no, timeout, exit, pie request and exact BIN dialogue passed.");return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

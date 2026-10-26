/* Event 140: installed trigger, bytecode, Duke art/ANI and exact text resources.
 * Dates/initial friendship are fixtures; the ordinary runtime chooses/runs it. */
#include "poultry_helpers.h"
static bool original_message(HmbtnScenePreview *s,char *error,size_t size)
{
    uint8_t record[8192];HmbtnMessage expected;
    return hmbtn_text_resource_read(hmbtn_scene_preview_resources(s).root,(HmbtnTextResourceId){HMBTN_TEXT_RECORD,1,hmbtn_scene_preview_session(s)->intro.message},
            record,sizeof(record),error,size)
        && hmbtn_feed_shop_expand(record,sizeof(record),hmbtn_game_registration_const(&hmbtn_scene_preview_session(s)->game))
        && hmbtn_message_open(&expected,record,sizeof(record),error,size)
        && expected.count==hmbtn_scene_preview_session(s)->dialogue.message.count
        && !memcmp(expected.words,hmbtn_scene_preview_session(s)->dialogue.message.words,expected.count*sizeof(*expected.words));
}
int main(int argc,char **argv)
{
    if(argc<2 || argc>3) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};
    const char *shots=argc==3 ? argv[2] : NULL;
    CHECK(hmbtn_host_open(&host));
    for(unsigned scenario=0;scenario<3;++scenario) {
        const bool refuse=scenario==1;
        const HmbtnFarmClock clock={0,2,13,0,6,0,0};
        s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&clock,error,sizeof(error));CHECK(s);
        CHECK(step(s,0,error,sizeof(error)));
        HmbtnVillage *v=&hmbtn_scene_preview_session(s)->village;
        v->people[21].friendship=scenario==2 ? 10 : 100;
        const unsigned friendship=v->people[21].friendship;
        const HmbtnInventory inventory=*hmbtn_scene_preview_session(s)->house.inventory;
        const HmbtnEconomy economy=hmbtn_scene_preview_session(s)->game.economy;
        CHECK(hmbtn_farm_visit_trigger(&hmbtn_scene_preview_session(s)->triggers,v,&clock,0,0,2)==140);
        HmbtnFarmClock wrong=clock;wrong.year=1;
        CHECK(hmbtn_farm_visit_trigger(&hmbtn_scene_preview_session(s)->triggers,v,&wrong,0,0,2)!=140);
        wrong=clock;wrong.season=1;CHECK(hmbtn_farm_visit_trigger(&hmbtn_scene_preview_session(s)->triggers,v,&wrong,0,0,2)!=140);
        wrong=clock;wrong.day=12;CHECK(hmbtn_farm_visit_trigger(&hmbtn_scene_preview_session(s)->triggers,v,&wrong,0,0,2)!=140);
        wrong=clock;wrong.hour=12;CHECK(hmbtn_farm_visit_trigger(&hmbtn_scene_preview_session(s)->triggers,v,&wrong,0,0,2)!=140);
        wrong=clock;wrong.hour=11;CHECK(hmbtn_farm_visit_trigger(&hmbtn_scene_preview_session(s)->triggers,v,&wrong,4,0,2)==140);
        CHECK(step(s,0,error,sizeof(error)) && hmbtn_scene_preview_session(s)->visit.active && hmbtn_scene_preview_session(s)->visit.event==140);
        CHECK(hmbtn_village_event_actor(v,21)==&hmbtn_scene_preview_session(s)->visit.actors[21]
            && hmbtn_farm_intro_actor(&hmbtn_scene_preview_session(s)->intro,21)==hmbtn_village_actor(v,21));
        bool selected=false,messages[4]={0},walked=false,captured=false;
        unsigned frames=0;
        for(;frames<6000 && hmbtn_scene_preview_session(s)->visit.active;++frames) {
            CHECK(!hmbtn_scene_preview_session(s)->intro.script.boundary);
            const unsigned id=hmbtn_scene_preview_session(s)->intro.message;
            if(id && hmbtn_scene_preview_session(s)->runtime.loaded_message==id) {
                CHECK(id>=418 && id<=421 && original_message(s,error,sizeof(error)));
                messages[id-418]=true;
            }
            const HmbtnIntroActor before=*hmbtn_village_actor(v,21);
            uint32_t pressed=id ? HMBTN_PAD_CROSS : 0;
            if(hmbtn_scene_preview_session(s)->dialogue.message.choices && hmbtn_scene_preview_session(s)->dialogue.message.phase==HMBTN_MESSAGE_WAIT_END && !selected) {
                selected=true;if(refuse) pressed=HMBTN_PAD_DOWN;
                CHECK(capture(s,&host,shots,refuse ? "vineyard-refuse" : "vineyard-invitation",error,sizeof(error)));
                captured=true;
            }
            const HmbtnFarmClock date=hmbtn_scene_preview_session(s)->game.clock;
            CHECK(step(s,pressed,error,sizeof(error)));
            if(id) CHECK(!memcmp(&date,&hmbtn_scene_preview_session(s)->game.clock,sizeof(date)));
            if(hmbtn_scene_preview_session(s)->visit.active && hmbtn_village_actor(v,21)->position[2]!=before.position[2]) walked=true;
        }
        CHECK(frames<6000 && selected && captured && walked && hmbtn_scene_preview_session(s)->control.enabled
            && hmbtn_scene_preview_session(s)->intro.script.finished && hmbtn_scene_preview_session(s)->intro.finish_requested);
        CHECK(messages[0] && (refuse ? messages[3] && !messages[1] && !messages[2]
            : messages[1] && messages[2] && !messages[3]));
        CHECK(v->flags[205]==!refuse && v->flags[251+140]);
        CHECK(v->people[21].friendship==(refuse ? friendship : friendship<40 ? 0 : friendship-40));
        CHECK(!hmbtn_village_event_actor(v,21) && hmbtn_village_actor(v,21)==&v->people[21].actor);
        CHECK(!memcmp(&inventory,hmbtn_scene_preview_session(s)->house.inventory,sizeof(inventory))
            && !memcmp(&economy,&hmbtn_scene_preview_session(s)->game.economy,sizeof(economy)));
        const unsigned bond=v->people[21].friendship;
        for(unsigned i=0;i<60;++i) CHECK(step(s,0,error,sizeof(error)));
        CHECK(!hmbtn_scene_preview_session(s)->visit.active && v->people[21].friendship==bond);
        CHECK(hmbtn_farm_visit_trigger(&hmbtn_scene_preview_session(s)->triggers,v,&clock,0,0,2)!=140);
        CHECK(hmbtn_scene_preview_load_area(s,80,error,sizeof(error)));hmbtn_scene_preview_session(s)->intro.area=80;
        CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)));hmbtn_scene_preview_session(s)->intro.area=0;
        CHECK(step(s,0,error,sizeof(error)) && !hmbtn_scene_preview_session(s)->visit.active && v->flags[205]==!refuse
            && v->people[21].friendship==bond);
        hmbtn_scene_preview_destroy(s);s=NULL;
    }
    hmbtn_host_close(&host);
    puts("Original vineyard invitation: trigger/date, exact yes/no text, shared Duke actor, actual walking, flags/clamps, no repeat or unrelated transaction passed.");
    return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

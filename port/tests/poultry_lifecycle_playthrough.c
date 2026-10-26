/* Original event resources. Fixtures exercise age/illness without years of waiting;
 * incubation uses the actual zone, held egg, animation and daily observer. */
#include "poultry_helpers.h"
static bool event_run(HmbtnScenePreview *s,HmbtnHost *host,const char *shots,unsigned event,char *error,size_t size)
{
    bool started=false,church=false,coop=false,named=false,cancel_tested=false;
    for(unsigned i=0;i<16000;++i) {
        uint32_t input=HMBTN_PAD_CROSS;
        if(hmbtn_scene_preview_session(s)->animal_name_active) {
            HmbtnNewGame *e=&hmbtn_scene_preview_session(s)->animal_name;input=0;
            if(e->message.phase==HMBTN_MESSAGE_WAIT_END && !e->queued) {
                if(!e->length && !cancel_tested) { input=HMBTN_PAD_CIRCLE;cancel_tested=true; }
                else if(!e->length) input=HMBTN_PAD_CROSS;
                else input=e->confirming ? HMBTN_PAD_CROSS : HMBTN_PAD_START;
            }
            named=true;
        }
        REQUIRE(step(s,input,error,size));
        if(input==HMBTN_PAD_CIRCLE) REQUIRE(hmbtn_scene_preview_session(s)->animal_name_active && !hmbtn_scene_preview_session(s)->animal_name.finished);
        REQUIRE(!hmbtn_scene_preview_session(s)->intro.script.boundary);
        started|=hmbtn_scene_preview_session(s)->animal_event==event;church|=hmbtn_scene_preview_session(s)->area==44;coop|=hmbtn_scene_preview_session(s)->area==112;
        if(hmbtn_scene_preview_session(s)->intro.message || hmbtn_scene_preview_session(s)->animal_name_active || i%100==0) {
            REQUIRE(capture(s,host,NULL,"event",error,size));
            if(shots && hmbtn_scene_preview_session(s)->dialogue.message.phase==HMBTN_MESSAGE_WAIT_END) REQUIRE(capture(s,host,shots,event==172 ? "funeral" : "hatch",error,size));
            if(shots && hmbtn_scene_preview_session(s)->dialogue.message.phase==HMBTN_MESSAGE_WAIT_END && (hmbtn_scene_preview_session(s)->intro.message==502 || hmbtn_scene_preview_session(s)->intro.message==504 || hmbtn_scene_preview_session(s)->intro.message==1832))
                REQUIRE(capture(s,host,shots,event==172 ? "funeral-scene" : "hatch-scene",error,size));
        }
        if(started && !hmbtn_scene_preview_session(s)->animal_event && hmbtn_scene_preview_session(s)->control.enabled) {
            for(unsigned tick=0;tick<60 && hmbtn_scene_preview_session(s)->intro.fade_level;++tick) REQUIRE(step(s,0,error,size));
            REQUIRE(!hmbtn_scene_preview_session(s)->intro.fade_level);
            REQUIRE(hmbtn_scene_preview_session(s)->area==80 && hmbtn_scene_preview_session(s)->intro.script.finished);
            REQUIRE(event==172 ? church : coop && named);return true;
        }
    }
    snprintf(error,size,"Animal event %u stuck at step %u",event,hmbtn_scene_preview_session(s)->intro.script.pc);return false;
}
int main(int argc,char **argv)
{
    if(argc<2 || argc>3) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};const char *shots=argc==3 ? argv[2] : NULL;
    CHECK(hmbtn_host_open(&host));s=hmbtn_scene_house_create(host.renderer,argv[1],error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<120;++i) CHECK(step(s,0,error,sizeof(error)));
    hmbtn_scene_preview_session(s)->first_visit_started=true;
    const uint16_t name[9]={1,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF};
    /* Both funeral causes use the original scene and release only flagged records. */
    for(unsigned cause=0;cause<2;++cause) {
        CHECK(hmbtn_poultry_buy(&hmbtn_scene_preview_session(s)->poultry,name,&hmbtn_scene_preview_session(s)->game.clock));
        HmbtnHen *hen=&hmbtn_scene_preview_session(s)->poultry.hens[0];hen->dead=true;hen->died_of_age=cause==0;hen->actor.visible=false;
        for(unsigned id=1;id<HMBTN_VILLAGERS;++id) { hmbtn_scene_preview_session(s)->village.people[id].friendship=100;hmbtn_scene_preview_session(s)->village.people[id].affection=2000; }
        hmbtn_scene_preview_session(s)->menu.stats.helper_affection[0]=100;
        const unsigned minutes=hmbtn_scene_preview_session(s)->game.clock.hour*60+hmbtn_scene_preview_session(s)->game.clock.minute;
        CHECK(event_run(s,&host,shots,172,error,sizeof(error)));
        CHECK(!hmbtn_poultry_count(&hmbtn_scene_preview_session(s)->poultry) && hmbtn_poultry_room(&hmbtn_scene_preview_session(s)->poultry) && hmbtn_scene_preview_session(s)->carry.discarded==cause+1);
        CHECK(hmbtn_scene_preview_session(s)->game.clock.hour*60+hmbtn_scene_preview_session(s)->game.clock.minute==minutes+(cause ? 120 : 60));
        CHECK(hmbtn_scene_preview_session(s)->village.people[3].affection==(cause ? 1000u : 2000u));
        CHECK(hmbtn_scene_preview_session(s)->village.people[6].friendship==(cause ? 80u : 100u));
        CHECK(hmbtn_scene_preview_session(s)->menu.stats.helper_affection[0]==(cause ? 80u : 100u));
    }
    CHECK(travel(s,0,error,sizeof(error)) && travel(s,112,error,sizeof(error)));
    /* A pre-existing egg fixture; no egg is created by the incubator interaction. */
    hmbtn_scene_preview_session(s)->house.inventory->held_item=(HmbtnStoredTool){0x122,0,1};
    const uint8_t *zone=NULL;
    for(unsigned i=0;i<50;++i) if(hmbtn_read_le16(hmbtn_scene_preview_resources(s).section+16+44*i+2)==34) { zone=hmbtn_scene_preview_resources(s).section+16+44*i;break; }
    CHECK(zone);at(s,zone);hmbtn_scene_preview_session(s)->intro.player.position[0]+=224;
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && hmbtn_scene_preview_session(s)->feed.phase==2);
    CHECK(hmbtn_scene_preview_session(s)->poultry.hens[0].occupied && hmbtn_scene_preview_session(s)->poultry.hens[0].stage==HMBTN_HEN_EGG);
    for(unsigned i=0;i<100 && hmbtn_scene_preview_session(s)->feed.phase;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(!hmbtn_scene_preview_session(s)->house.inventory->held_item.id && !hmbtn_poultry_count(&hmbtn_scene_preview_session(s)->poultry));
    CHECK(capture(s,&host,shots,"incubation",error,sizeof(error)));
    for(unsigned day=1;day<=3;++day) {
        CHECK(travel(s,0,error,sizeof(error)) && travel(s,80,error,sizeof(error)));
        const unsigned before=hmbtn_scene_preview_session(s)->game.clock.day;hmbtn_scene_preview_session(s)->house.mode=HMBTN_HOUSE_SLEEP;
        for(unsigned i=0;i<1200 && (hmbtn_scene_preview_session(s)->game.clock.day==before || hmbtn_scene_preview_session(s)->morning_fade);++i) CHECK(step(s,0,error,sizeof(error)));
        CHECK(hmbtn_scene_preview_session(s)->game.clock.day!=before && hmbtn_scene_preview_session(s)->area==80);
        if(day<3) CHECK(hmbtn_scene_preview_session(s)->poultry.hens[0].growth==day && travel(s,0,error,sizeof(error)) && travel(s,112,error,sizeof(error)));
    }
    CHECK(hmbtn_scene_preview_session(s)->poultry.hens[0].stage==HMBTN_HEN_CHICK && hmbtn_scene_preview_session(s)->poultry.hens[0].naming_pending);
    CHECK(event_run(s,&host,shots,299,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->poultry.hens[0].name[0]==0 && !hmbtn_scene_preview_session(s)->poultry.hens[0].naming_pending && !hmbtn_scene_preview_session(s)->poultry.hens[0].incubator);
    CHECK(hmbtn_scene_preview_session(s)->poultry.hens[0].growth==3 && hmbtn_scene_preview_session(s)->poultry.hens[0].affection==10);
    CHECK(travel(s,0,error,sizeof(error)) && travel(s,112,error,sizeof(error)));
    CHECK(capture(s,&host,shots,"named-chick",error,sizeof(error)));
    for(unsigned day=1;day<=7;++day) {
        CHECK(travel(s,0,error,sizeof(error)) && sleep_day(s,error,sizeof(error)));
        CHECK(hmbtn_scene_preview_session(s)->poultry.hens[0].growth==3+day && !hmbtn_scene_preview_session(s)->poultry.eggs[0].item);
        CHECK(travel(s,112,error,sizeof(error)));
        CHECK(capture(s,&host,NULL,"growth",error,sizeof(error)));
    }
    CHECK(hmbtn_scene_preview_session(s)->poultry.hens[0].stage==HMBTN_HEN_ADULT && !hmbtn_scene_preview_session(s)->poultry.hens[0].fed_yesterday);
    puts("Original funeral scenes/causes, removal/penalties, incubator zone, three-day hatch, naming and seven-day growth passed.");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

/* Installed Karen scripts 18/19 -> Won -> the shared crop/inventory/gift path.
 * Dates, initial bond and player positions are fixtures; no forced event/spawn. */
#include "poultry_helpers.h"
#include "field_helpers.h"
static bool visit(HmbtnScenePreview *s,HmbtnHost *host,const char *shots,unsigned event,bool talk,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);bool message=false,prop=false,walk=false,interacted=false;
    for(unsigned i=0;i<6000 && g->visit.active;++i) {
        REQUIRE(g->visit.event==event && !g->intro.script.boundary);
        if(g->intro.script.pc>=7) REQUIRE(g->runtime.audio_song==5);
        const HmbtnIntroActor *k=hmbtn_village_actor(&g->village,3);
        const int32_t before=k->position[2];uint32_t pressed=g->intro.message ? HMBTN_PAD_CROSS : 0;
        if(g->intro.message && g->runtime.loaded_message==g->intro.message) {
            uint8_t record[8192];HmbtnMessage expected;
            REQUIRE(g->intro.message==86 || g->intro.message==(event==18 ? 87u : 88u));
            REQUIRE(hmbtn_text_resource_read(hmbtn_scene_preview_resources(s).root,
                (HmbtnTextResourceId){HMBTN_TEXT_RECORD,1,g->intro.message},record,sizeof(record),error,size));
            REQUIRE(hmbtn_feed_shop_expand(record,sizeof(record),&g->game.registration));
            REQUIRE(hmbtn_message_open(&expected,record,sizeof(record),error,size)
                && expected.count==g->dialogue.message.count && !memcmp(expected.words,g->dialogue.message.words,expected.count*sizeof(*expected.words)));
            message=true;
        }
        if(g->visit.prop.visible && !prop) {
            REQUIRE(capture(s,host,shots,"karen-seed-packet",error,size));prop=true;
        }
        if(g->visit.free && !g->intro.message && !interacted) {
            if(talk) {
                memcpy(g->intro.player.position,k->position,sizeof(k->position));
                g->intro.player.position[0]+=280;g->intro.player.facing=1;g->control.mode=0;
                pressed=HMBTN_PAD_CROSS;interacted=true;
                REQUIRE(hmbtn_village_target(&g->village,&g->intro.player)==3);
            } else {
                /* Natural deadline branch: advance the authoritative clock,
                 * without editing script slots/cursor or completing a message. */
                hmbtn_session_advance_minutes(g,61);interacted=true;
            }
        }
        REQUIRE(step(s,pressed,error,size));
        if(g->visit.active && hmbtn_village_actor(&g->village,3)->position[2]!=before) walk=true;
    }
    REQUIRE(!g->visit.active && g->intro.script.finished && !g->intro.script.boundary && g->control.enabled);
    REQUIRE(message && walk && interacted && (event!=18 || !talk || prop));
    REQUIRE(!hmbtn_village_event_actor(&g->village,3));
    return true;
}
int main(int argc,char **argv)
{
    if(argc<4 || argc>5) return 2;
    const char *shots=argc==5 ? argv[4] : NULL;HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};
    CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock clock={0,0,2,2,6,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&clock,error,sizeof(error));CHECK(s && step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);HmbtnVillage *v=&g->village;
    CHECK(!g->game.progress.moondrop_seeds && g->previous_place==2 && g->triggers.field==&g->field);
    v->people[3].affection=14999;
    CHECK(hmbtn_farm_event_trigger(&g->triggers,v,&clock,0,2,2,0,&g->game.inventory)!=18);
    v->people[3].affection=15000;
    CHECK(hmbtn_farm_event_trigger(&g->triggers,v,&clock,0,2,2,0,&g->game.inventory)==18);
    HmbtnFarmClock wrong=clock;wrong.day=20;
    CHECK(hmbtn_farm_event_trigger(&g->triggers,v,&wrong,0,2,2,0,&g->game.inventory)!=18);
    wrong=clock;wrong.season=1;
    CHECK(hmbtn_farm_event_trigger(&g->triggers,v,&wrong,0,2,2,0,&g->game.inventory)!=18);
    wrong=clock;wrong.hour=12;
    CHECK(hmbtn_farm_event_trigger(&g->triggers,v,&wrong,0,2,2,0,&g->game.inventory)!=18);
    CHECK(hmbtn_farm_event_trigger(&g->triggers,v,&clock,0,1,2,0,&g->game.inventory)!=18);
    const unsigned gold=g->game.economy.gold;
    g->trigger_entry=true;CHECK(step(s,0,error,sizeof(error)) && g->visit.event==18);
    CHECK(visit(s,&host,shots,18,true,error,sizeof(error)));
    CHECK(v->flags[269] && v->people[3].affection==17000 && !g->game.progress.moondrop_seeds
        && g->game.economy.gold==gold && hmbtn_inventory_count(&g->game.inventory,0x31,0,2)==1);
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);v=&g->village;
    CHECK(g->feed_shop.progress==&g->game.progress && !g->game.progress.moondrop_seeds);
    g->field.cells[460]=(HmbtnFarmTile){.value=0x167,.ground=0x167};
    cell(s,10,10);CHECK(tool(s,6,error,sizeof(error)));
    cell(s,11,10);CHECK(tool(s,0x31,error,sizeof(error)) && g->field.cells[460].value==0x31);
    CHECK(field_refill(s,error,sizeof(error)));
    const unsigned stages[6]={0,1,1,2,2,3};
    for(unsigned day=0;day<6;++day) {
        cell(s,10,10);CHECK(tool(s,21,error,sizeof(error)));
        g->game.weather.tomorrow=HMBTN_WEATHER_SUNNY;
        CHECK(sleep_day(s,error,sizeof(error)) && g->field.cells[460].kind==stages[day]);
        if(day<5) CHECK(!g->game.progress.moondrop_seeds);
        if(day==2) {CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);v=&g->village;}
    }
    unsigned sprite,animation;CHECK(hmbtn_estate_crop_look(0x31,3,0,&sprite,&animation) && sprite==12 && !animation);
    for(unsigned i=0;i<20 && !g->visit.active;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(g->visit.event==19 && g->game.progress.moondrop_seeds); /* Script unlocks on arrival, before talk. */
    const unsigned affection_before_unlock=v->people[3].affection;
    CHECK(visit(s,&host,shots,19,true,error,sizeof(error)));
    CHECK(v->flags[270] && v->people[3].affection==affection_before_unlock+2000 && g->game.progress.moondrop_seeds);
    cell(s,10,10);CHECK(step(s,0,error,sizeof(error)) && capture(s,&host,shots,"moondrop-grown",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && idle(s,error,sizeof(error)) && g->game.inventory.held_item.id==0x51);
    CHECK(step(s,HMBTN_PAD_R2,error,sizeof(error)) && idle(s,error,sizeof(error)) && g->game.inventory.items[0].id==0x51);
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);v=&g->village;
    CHECK(g->game.progress.moondrop_seeds && hmbtn_farm_event_trigger(&g->triggers,v,&g->game.clock,0,2,2,0,&g->game.inventory)!=19);
    hmbtn_session_advance_minutes(g,360);CHECK(hmbtn_scene_preview_load_area(s,204,error,sizeof(error)));
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    HmbtnIntroActor *won=&v->people[35].actor;CHECK(won->visible);
    memcpy(g->intro.player.position,won->position,sizeof(won->position));g->intro.player.position[2]+=280;g->intro.player.facing=0;g->control.mode=0;
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(hmbtn_seed_shop_count(&g->feed_shop)==7 && g->feed_shop.progress==&g->game.progress);
    for(unsigned row=0;row<7;++row) CHECK(hmbtn_seed_shop_choice(&g->feed_shop,row)==hmbtn_won_seed_entry(row));
    for(unsigned i=0;i<3;++i) CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)));
    CHECK(g->feed_shop.item==0x31 && capture(s,&host,shots,"won-moondrop-unlocked",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_TRIANGLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_INFO,error,sizeof(error)) && g->feed_shop.request==0x168);
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_COUNT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && g->game.economy.gold==gold-300);
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_OFF,error,sizeof(error)));
    for(unsigned i=0;i<800 && g->talk.active;++i) CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(!g->talk.active && !g->talk.script.boundary && hmbtn_inventory_count(&g->game.inventory,0x31,0,2)==1);
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(g->game.progress.moondrop_seeds && g->game.inventory.items[0].id==0x51);
    CHECK(step(s,HMBTN_PAD_R2,error,sizeof(error)) && idle(s,error,sizeof(error)) && g->game.inventory.held_item.id==0x51);
    CHECK(hmbtn_scene_preview_load_area(s,128,error,sizeof(error)));
    for(unsigned day=0;day<7;++day) {
        CHECK(step(s,0,error,sizeof(error)));
        if(g->village.people[18].actor.visible) break;
        hmbtn_session_advance_minutes(g,1440);
    }
    HmbtnVillager *lillia=&g->village.people[18];CHECK(lillia->actor.visible);
    const unsigned friendship=lillia->friendship;
    memcpy(g->intro.player.position,lillia->actor.position,sizeof(lillia->actor.position));
    g->intro.player.position[2]+=280;g->intro.player.facing=0;g->control.mode=0;
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && g->talk.active && g->talk.target==18);
    for(unsigned i=0;i<3000 && g->talk.active;++i) CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(!g->talk.active && !g->talk.script.boundary && !g->game.inventory.held_item.id && lillia->friendship>friendship);
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));
    hmbtn_scene_preview_destroy(s);s=NULL;
    /* Leaving Karen waiting gives no packet/bond, and never unlocks the shop. */
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&clock,error,sizeof(error));CHECK(s && step(s,0,error,sizeof(error)));
    g=hmbtn_scene_preview_session(s);g->village.people[3].affection=15000;g->trigger_entry=true;
    CHECK(step(s,0,error,sizeof(error)) && g->visit.event==18 && visit(s,&host,NULL,18,false,error,sizeof(error)));
    CHECK(!g->game.progress.moondrop_seeds && g->village.people[3].affection==15000
        && !hmbtn_inventory_count(&g->game.inventory,0x31,0,2));
    hmbtn_scene_preview_destroy(s);s=NULL;
    /* The unlock itself is at entry: ignoring the second visit keeps it,
     * without granting the conversation's affection. Explicit mature-bed fixture. */
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&clock,error,sizeof(error));CHECK(s && step(s,0,error,sizeof(error)));
    g=hmbtn_scene_preview_session(s);const unsigned bond=g->village.people[3].affection;
    g->intro.music_mode=5; /* Stale prior-event mode must not skip loading song 5. */
    g->field.cells[460]=(HmbtnFarmTile){.value=0x31,.ground=0x168,.kind=3};g->trigger_entry=true;
    CHECK(step(s,0,error,sizeof(error)) && g->visit.event==19 && g->game.progress.moondrop_seeds);
    CHECK(visit(s,&host,NULL,19,false,error,sizeof(error)));
    CHECK(g->game.progress.moondrop_seeds && g->village.flags[270] && g->village.people[3].affection==bond);
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);
    puts("Moondrop: real Karen gift/unlock, prerequisites, exact words/art, no repeat, shared Won purchase, growth/harvest and persistent progress passed.");return 0;
fail:hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

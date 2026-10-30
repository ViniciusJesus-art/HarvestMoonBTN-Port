/* Original event 172: real overnight causes, all six branches, names/pages,
 * penalties once, live records/reservations and subsequent birth/hatch intact.
 * Fixtures set ages/illness and completed buildings rather than waiting years. */
#include "poultry_helpers.h"
#include "field_helpers.h"
static const uint16_t name[9]={1,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff};
static bool contains(const HmbtnMessage *m,const uint16_t *name)
{
    unsigned length=0;while(length<8 && name[length]!=0xffff) ++length;
    if(!length) return false;
    for(unsigned i=0;i+length<=m->count;++i) if(!memcmp(m->words+i,name,length*sizeof(*name))) return true;
    return false;
}
static bool names(HmbtnScenePreview *s,unsigned selector,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const bool age=selector<3;const unsigned species=selector%3;
    const HmbtnMessage *m=&g->dialogue.message;unsigned count=0;
    for(unsigned i=0;i<(species==2 ? HMBTN_HENS : HMBTN_LIVESTOCK);++i) {
        const uint16_t *n;
        if(species==2) {
            const HmbtnHen *h=&g->poultry.hens[i];if(!h->occupied || !h->dead || h->died_of_age!=age) continue;n=h->name;
        } else {
            const HmbtnFarmAnimal *a=&g->livestock.animals[i];
            if(!a->occupied || !a->dead || a->sheep!=(species==1) || a->died_of_age!=age) continue;n=a->name;
        }
        REQUIRE(contains(m,n));++count;
    }
    REQUIRE(count);return true;
}
static bool funeral(HmbtnScenePreview *s,HmbtnHost *host,const char *shots,unsigned expected,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);bool started=false,church=false,barley=false;unsigned seen=0,captured=0;
    const unsigned ids[6]={498,500,504,494,496,502};
    for(unsigned t=0;t<18000;++t) {
        REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && !g->intro.script.boundary);
        started|=g->animal_event==172;church|=g->area==44;barley|=g->intro.barley.visible;
        if(g->animal_event==172 && g->runtime.loaded_message==g->intro.message) for(unsigned selector=0;selector<6;++selector) {
            if(g->intro.message!=ids[selector]) continue;
            REQUIRE(names(s,selector,error,size));seen|=1u<<selector;
            if(!(captured&(1u<<selector)) && g->dialogue.message.phase==HMBTN_MESSAGE_WAIT_END && !g->intro.fade_level) {
                char label[48];snprintf(label,sizeof(label),"funeral-message-%u",ids[selector]);
                REQUIRE(capture(s,host,shots,label,error,size));captured|=1u<<selector;
            }
        }
        if(t%100==0 || g->intro.message) REQUIRE(capture(s,host,NULL,"funeral",error,size));
        if(started && !g->animal_event && g->control.enabled) {
            REQUIRE(g->area==80 && church && barley && seen==expected && !g->animal_event);return true;
        }
    }
    snprintf(error,size,"Funeral stuck at event step %u",g->intro.script.pc);return false;
}
static bool offspring(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);bool started=false;unsigned names=0;bool naming=false;
    for(unsigned t=0;t<16000;++t) {
        uint32_t pad=HMBTN_PAD_CROSS;
        if(g->animal_name_active) {
            HmbtnNewGame *e=&g->animal_name;pad=0;
            if(!naming) { ++names;naming=true; }
            if(e->message.phase==HMBTN_MESSAGE_WAIT_END && !e->queued)
                pad=!e->length || e->confirming ? HMBTN_PAD_CROSS : HMBTN_PAD_START;
        } else naming=false;
        REQUIRE(step(s,pad,error,size) && !g->intro.script.boundary);started|=g->animal_event==299;
        if(started && !g->animal_event && g->control.enabled) {
            for(unsigned i=0;i<60 && g->intro.fade_level;++i) REQUIRE(step(s,0,error,size));
            REQUIRE(names==2 && g->livestock.animals[4].occupied && !g->livestock.animals[4].reserved
                && g->livestock.animals[4].name[0]==0 && !g->poultry.hens[1].naming_pending && g->poultry.hens[1].name[0]==0);return true;
        }
    }
    snprintf(error,size,"Post-funeral birth stuck at event step %u",g->intro.script.pc);return false;
}
static bool fixture(HmbtnScenePreview *s,unsigned scenario,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->first_visit_started=true;
    g->poultry.upgraded=true;g->construction.house_level=1;g->construction.barn_level=1;
    g->village.flags[251+288]=g->village.flags[251+140]=true;
    const unsigned total=scenario==2 ? 15 : 3;
    for(unsigned i=0;i<total;++i) {
        const bool sheep=scenario==2 ? (i>=6 && i<=11) || i==13 : i==1;
        REQUIRE(hmbtn_livestock_buy(&g->livestock,sheep ? 0x165 : 0x161,name,&g->game.clock));
        HmbtnFarmAnimal *a=&g->livestock.animals[i];a->growth=28;a->affection=100;
        for(unsigned j=0;j<8;++j) a->name[j]=(uint16_t)i;
        a->name[8]=0xffff;
        if(scenario==2) {
            if(i<12) a->days_sick=31;
            else if(i<14) a->age=sheep ? 5 : 10;
            else a->growth=0; /* Living calf must keep its slot. */
        } else if(i<2) {
            if(scenario==0) a->age=sheep ? 5 : 10;
            else a->days_sick=31;
        }
    }
    if(scenario==1) {
        g->livestock.animals[0].days_sick=0;
        REQUIRE(hmbtn_livestock_breed(&g->livestock,0,false)); /* mode 0 -> child 3 */
        g->livestock.animals[0].days_sick=31;
        REQUIRE(hmbtn_livestock_breed(&g->livestock,2,false)); /* mode 1 -> child 4 */
        g->livestock.animals[2].pregnancy=20;
    }
    const unsigned hens=scenario==2 ? 3 : 1;
    for(unsigned i=0;i<hens;++i) {
        REQUIRE(hmbtn_poultry_buy(&g->poultry,name,&g->game.clock));
        HmbtnHen *h=&g->poultry.hens[i];h->affection=100;for(unsigned j=0;j<8;++j) h->name[j]=(uint16_t)(0x18+i);h->name[8]=0xffff;
        if(scenario==2 && i<2) { if(i) h->age=5;else h->days_sick=31; }
    }
    if(scenario==1) {
        REQUIRE(!hmbtn_poultry_incubate(&g->poultry,0,0x122,&g->game.clock));g->poultry.hens[1].growth=2;
    }
    for(unsigned i=1;i<HMBTN_VILLAGERS;++i) { g->village.people[i].friendship=100;g->village.people[i].affection=2000; }
    for(unsigned i=0;i<7;++i) g->menu.stats.helper_affection[i]=100;
    for(unsigned i=0;i<22;++i) g->feed.barn[i]=true;for(unsigned i=0;i<10;++i) g->feed.coop[i]=true;
    g->carry.discarded=scenario==2 ? 90 : 0;g->house.mode=HMBTN_HOUSE_SLEEP;
    const unsigned before=g->game.clock.day;
    for(unsigned t=0;t<1200 && (g->game.clock.day==before || g->morning_fade);++t) REQUIRE(step(s,0,error,size));
    REQUIRE(g->game.clock.day!=before && !g->morning_fade && !g->animal_event && g->area==80);
    return true;
}
int main(int argc,char **argv)
{
    if(argc<3 || argc>4) return 2;const char *shots=argc==4 ? argv[3] : NULL;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};CHECK(hmbtn_host_open(&host));
    for(unsigned scenario=0;scenario<3;++scenario) {
        const HmbtnFarmClock clock={.day=3,.weekday=3,.hour=6};
        s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&clock,error,sizeof(error));CHECK(s);
        for(unsigned t=0;t<110;++t) CHECK(step(s,0,error,sizeof(error)));
        HmbtnSession *g=hmbtn_scene_preview_session(s);g->first_visit_started=true;
        g->poultry.upgraded=true;g->construction.house_level=1;g->construction.barn_level=1;
        CHECK(travel(s,80,error,sizeof(error)) && fixture(s,scenario,error,sizeof(error)));
        if(scenario==1) {
            CHECK(g->livestock.animals[0].dead && g->livestock.births[1].ready && hmbtn_poultry_hatch_ready(&g->poultry,0));
        }
        char pending[2048];CHECK(snprintf(pending,sizeof(pending),"%s.pending",argv[2])<(int)sizeof(pending));
        CHECK(field_reload(&s,&host,argv[1],pending,error,sizeof(error)));g=hmbtn_scene_preview_session(s);
        const unsigned living=scenario==2 ? 14 : 2;
        const HmbtnFarmAnimal survivor=g->livestock.animals[living];
        const unsigned minutes=g->game.clock.hour*60+g->game.clock.minute,gold=g->game.economy.gold;
        CHECK(funeral(s,&host,shots,scenario==0 ? 3u : scenario==1 ? 24u : 63u,error,sizeof(error)));
        if(scenario!=1) for(unsigned t=0;t<60 && g->intro.fade_level;++t) CHECK(step(s,0,error,sizeof(error)));
        CHECK(!memcmp(&survivor,&g->livestock.animals[living],sizeof(survivor)) && g->game.economy.gold==gold && g->game.economy.shipping.honey_bonus);
        CHECK(g->game.clock.hour*60+g->game.clock.minute==minutes+(scenario==0 ? 60u : scenario==1 ? 180u : 240u));
        CHECK(g->carry.discarded==(scenario==2 ? 99u : 2u));
        CHECK(g->village.people[3].affection==(scenario ? 1000u : 2000u)
            && g->village.people[6].friendship==(scenario ? 80u : 100u) && g->menu.stats.helper_affection[0]==(scenario ? 80u : 100u));
        CHECK(hmbtn_livestock_count(&g->livestock,false)==1 && !hmbtn_livestock_count(&g->livestock,true) && hmbtn_livestock_valid_births(&g->livestock));
        if(scenario==1) {
            CHECK(!g->livestock.births[0].active && !g->livestock.animals[3].reserved && !g->livestock.animals[3].occupied
                && g->livestock.births[1].ready && g->livestock.animals[4].reserved);
            CHECK(offspring(s,error,sizeof(error)));
        }
        CHECK(field_reload(&s,&host,argv[1],argv[2],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
        const unsigned discarded=g->carry.discarded,affection=g->village.people[3].affection;
        for(unsigned t=0;t<120;++t) CHECK(step(s,0,error,sizeof(error)));
        CHECK(!g->animal_event && g->carry.discarded==discarded && g->village.people[3].affection==affection);
        CHECK(hmbtn_livestock_buy(&g->livestock,0x165,name,&g->game.clock) && g->livestock.animals[0].occupied);
        CHECK(field_reload(&s,&host,argv[1],argv[2],error,sizeof(error)));
        hmbtn_scene_preview_destroy(s);s=NULL;
    }
    hmbtn_host_close(&host);puts("Funerals: real overnight deaths/all six event-172 branches, complete names, time/penalties once, reservations/live animals, subsequent birth/hatch and reload passed.");return 0;
fail:hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

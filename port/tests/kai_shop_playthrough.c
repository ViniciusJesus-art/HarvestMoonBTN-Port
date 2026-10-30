/* Installed Kai conversation and schedules. Money/initial status are fixtures;
 * every dish comes through the original dispatch, with no item grant. */
#include "poultry_helpers.h"
#include "field_helpers.h"
static bool finish_talk(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<3000 && (g->talk.active || g->feed_shop.phase || g->event_item.phase);++i)
        REQUIRE(step(s,g->event_item.phase ? 0 : HMBTN_PAD_CROSS,error,size));
    REQUIRE(!g->talk.active && !g->talk.script.boundary && !g->feed_shop.phase && !g->event_item.phase);return true;
}
static bool kai(HmbtnScenePreview *s,bool shop,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);HmbtnVillager *npc=&g->village.people[25];
    REQUIRE(g->area==56 && npc->location==44 && npc->actor.visible);
    memcpy(g->intro.player.position,npc->actor.position,sizeof(npc->actor.position));g->intro.player.position[2]+=300;
    g->intro.player.facing=0;g->control.mode=0;memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));
    memcpy(g->intro.camera,g->intro.player.position,sizeof(g->intro.camera));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->talk.active && g->talk.target==25);
    if(shop) REQUIRE(shop_wait(s,HMBTN_SHOP_SELECT,error,size) && g->feed_shop.kai && !g->talk.script.boundary);
    else {
        for(unsigned t=0;t<3000 && g->talk.active;++t) { REQUIRE(!g->feed_shop.phase);REQUIRE(step(s,HMBTN_PAD_CROSS,error,size)); }
        REQUIRE(!g->talk.active && !g->talk.script.boundary && !g->feed_shop.phase);
    }
    return true;
}
static bool serving(HmbtnScenePreview *s,HmbtnHost *host,const char *shots,unsigned row,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const unsigned face=g->village.people[25].actor.facing;
    REQUIRE(kai(s,true,error,size));for(unsigned i=0;i<row;++i) REQUIRE(step(s,HMBTN_PAD_DOWN,error,size));
    const HmbtnSeedShopEntry *entry=hmbtn_meal_shop_entry(&g->feed_shop,row);REQUIRE(entry);
    const HmbtnFarmItem *item=hmbtn_farm_item(entry->item);const bool drink=(item->flags&0x400u)!=0;
    const HmbtnInventory inventory=g->game.inventory;const HmbtnFarmClock clock=g->game.clock;
    const unsigned stamina=g->farm_action.stamina,fatigue=g->farm_action.fatigue,happiness=g->farm_action.happiness,
        gold=g->game.economy.gold,spent=g->game.economy.ledger[0][1];
    REQUIRE(capture(s,host,shots,"kai-menu",error,size));
    REQUIRE(step(s,HMBTN_PAD_TRIANGLE,error,size) && shop_wait(s,HMBTN_SHOP_INFO,error,size) && g->feed_shop.request==entry->description);
    REQUIRE(capture(s,host,shots,"kai-info",error,size) && shop_wait(s,HMBTN_SHOP_SELECT,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->game.economy.gold==gold-(unsigned)item->buy_price);
    REQUIRE(shop_wait(s,HMBTN_SHOP_RESULT,error,size) && g->feed_shop.request==0x130);
    REQUIRE(!memcmp(&inventory,&g->game.inventory,sizeof(inventory)) && g->farm_action.stamina==stamina && g->farm_action.fatigue==fatigue);
    /* Check the actual BIN message before consuming; water has its own reply. */
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
    REQUIRE(shop_wait(s,HMBTN_SHOP_RESULT,error,size) && g->feed_shop.request==(drink ? 0x200u : 0x131u));
    REQUIRE(capture(s,host,shots,drink ? "kai-water-reply" : "kai-food-reply",error,size));
    for(unsigned t=0;t<3000 && !g->event_item.phase;++t) REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
    REQUIRE(g->feed_shop.phase==HMBTN_SHOP_SERVE && g->feed_shop.cue_started && g->talk.active);
    REQUIRE(g->event_item.phase==(drink ? HMBTN_EVENT_ITEM_SERVE_DRINK : HMBTN_EVENT_ITEM_SERVE_FOOD) && g->event_item.shown==entry->item);
    REQUIRE(g->intro.action_active==drink && g->intro.player.sprite.animation==(drink ? 0 : 5));
    REQUIRE(!hmbtn_save_available(g,error,size));error[0]=0;
    char name[64];snprintf(name,sizeof(name),"kai-serving-%u",entry->item);REQUIRE(capture(s,host,shots,name,error,size));
    REQUIRE(step(s,HMBTN_PAD_START|HMBTN_PAD_R2|HMBTN_PAD_TRIANGLE|HMBTN_PAD_SQUARE,error,size) && g->menu.mode==HMBTN_MENU_NONE);
    bool hidden=drink;
    for(unsigned t=0;t<800 && g->event_item.phase;++t) {
        REQUIRE(g->farm_action.stamina==stamina && g->farm_action.fatigue==fatigue && g->game.inventory.held_item.id==entry->item);
        if(!drink && !hidden && g->intro.player.sprite.frame>=2) { REQUIRE(capture(s,host,shots,"kai-food-hidden",error,size));hidden=true; }
        REQUIRE(step(s,0,error,size));
    }
    REQUIRE(hidden && !g->event_item.phase && !g->intro.action_active && !memcmp(&inventory,&g->game.inventory,sizeof(inventory)));
    REQUIRE(!memcmp(&clock,&g->game.clock,sizeof(clock)) && finish_talk(s,error,size));
    const unsigned sum=stamina+(unsigned)item->stamina,expected=sum>g->farm_action.max_stamina ? g->farm_action.max_stamina : sum;
    const unsigned tired=fatigue<(unsigned)-item->fatigue ? 0 : fatigue-(unsigned)-item->fatigue;
    REQUIRE(g->farm_action.stamina==expected && g->farm_action.fatigue==tired && g->farm_action.happiness==happiness);
    REQUIRE(g->game.economy.gold==gold-(unsigned)item->buy_price && g->game.economy.ledger[0][1]==spent+(unsigned)item->buy_price);
    REQUIRE(g->village.people[25].actor.facing==face && !g->game.inventory.held_item.id);return true;
}
static bool schedules(HmbtnScenePreview *s,char *error,size_t size)
{
    const HmbtnVillage *live=&hmbtn_scene_preview_session(s)->village;HmbtnVillage v;HmbtnWeather weather={0};
    /* Independent original route selections, including seasonal absence,
     * Sunday, dated-event days and rain. No production schedule is changed. */
    const unsigned times[]={539,540,659,660,794,795,1019,1020};
    const bool working[]={false,true,true,false,false,true,true,false};
    for(unsigned i=0;i<8;++i) {
        hmbtn_village_init(&v,live->schedule_data,live->schedule_size);v.weather=&weather;
        HmbtnFarmClock c={0,1,3,2,times[i]/60,times[i]%60,0};REQUIRE(hmbtn_village_sync(&v,&c,error,size));
        const HmbtnVillager *p=&v.people[25];REQUIRE((p->schedule[p->step][1]>>8==6)==working[i]);
    }
    for(unsigned variant=0;variant<4;++variant) {
        hmbtn_village_init(&v,live->schedule_data,live->schedule_size);v.weather=&weather;
        HmbtnFarmClock c={0,1,3,0,14,0,0};
        if(variant==1) weather.today=HMBTN_WEATHER_RAIN;
        if(variant==2) { weather.today=HMBTN_WEATHER_SUNNY;c.weekday=2;c.day=6; }
        if(variant==3) { c.season=2;c.day=3;c.weekday=2; }
        REQUIRE(hmbtn_village_sync(&v,&c,error,size));const HmbtnVillager *p=&v.people[25];
        REQUIRE(p->schedule[p->step][1]>>8!=6);if(variant==3) REQUIRE(p->location==54);
    }
    return true;
}
static bool checkpoint(HmbtnScenePreview **s,HmbtnHost *host,const char *root,const char *base,const char *suffix,char *error,size_t size)
{ char path[1024];if(snprintf(path,sizeof(path),"%s%s",base,suffix)>=(int)sizeof(path)) return false;return field_reload(s,host,root,path,error,size); }
int main(int argc,char **argv)
{
    if(argc<3 || argc>4) return 2;const char *shots=argc==4 ? argv[3] : NULL;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock clock={0,1,3,2,10,0,0};s=hmbtn_scene_area_create_at(host.renderer,argv[1],56,&clock,error,sizeof(error));CHECK(s);
    for(unsigned t=0;t<110;++t) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->game.economy.gold=2000;
    g->farm_action.max_stamina=200;g->farm_action.stamina=0;g->farm_action.fatigue=100;g->farm_action.happiness=12;
    g->village.flags[251+288]=g->village.flags[251+140]=true;
    CHECK(schedules(s,error,sizeof(error)) && kai(s,true,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && finish_talk(s,error,sizeof(error)) && g->game.economy.gold==2000);
    CHECK(kai(s,true,error,sizeof(error)));g->game.economy.gold=299;
    const unsigned spent=g->game.economy.ledger[0][1];
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && g->feed_shop.request==0x4f);
    CHECK(capture(s,&host,shots,"kai-poor",error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_INFO,error,sizeof(error)) && g->feed_shop.request==0x12f && shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(g->game.economy.gold==299 && g->game.economy.ledger[0][1]==spent && !g->game.inventory.held_item.id);
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && finish_talk(s,error,sizeof(error)));g->game.economy.gold=2000;
    CHECK(serving(s,&host,shots,4,error,sizeof(error)) && checkpoint(&s,&host,argv[1],argv[2],".water",error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    for(unsigned row=0;row<4;++row) CHECK(serving(s,&host,shots,row,error,sizeof(error)));
    CHECK(g->game.economy.gold==950 && g->farm_action.stamina==46 && g->farm_action.fatigue==87 && g->farm_action.happiness==12);
    CHECK(checkpoint(&s,&host,argv[1],argv[2],".food",error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(serving(s,&host,shots,4,error,sizeof(error)) && g->game.economy.gold==950 && g->farm_action.stamina==47);
    /* Actual route leaves the beach at lunch and returns before afternoon sales. */
    unsigned now=g->game.clock.hour*60+g->game.clock.minute;CHECK(now<675);hmbtn_session_advance_minutes(g,675-now);
    for(unsigned t=0;t<110;++t) CHECK(step(s,0,error,sizeof(error)));
    CHECK(!g->village.people[25].actor.visible && g->village.people[25].location==46);
    now=g->game.clock.hour*60+g->game.clock.minute;hmbtn_session_advance_minutes(g,795-now);
    for(unsigned t=0;t<110;++t) CHECK(step(s,0,error,sizeof(error)));
    CHECK(kai(s,true,error,sizeof(error)) && step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && finish_talk(s,error,sizeof(error)));
    CHECK(checkpoint(&s,&host,argv[1],argv[2],"",error,sizeof(error)));
    hmbtn_scene_preview_destroy(s);s=NULL;
    /* On Sunday Kai is on the beach as a resident, without opening his shop. */
    const HmbtnFarmClock sunday={0,1,3,0,14,0,0};s=hmbtn_scene_area_create_at(host.renderer,argv[1],56,&sunday,error,sizeof(error));CHECK(s);
    for(unsigned t=0;t<110;++t) CHECK(step(s,0,error,sizeof(error)));
    g=hmbtn_scene_preview_session(s);g->village.flags[251+288]=g->village.flags[251+140]=true;
    CHECK(kai(s,false,error,sizeof(error)));
    puts("Kai: original summer routes/closed periods, actual conversation, poor/cancel, five dishes/water replies, food/drink ANI, unique effects/payment and fresh resumes passed");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

/* Actual forage held through ten paid/free servings at Doug's installed counter.
 * Purse and initial attributes are fixtures; no served meal is granted. */
#include "poultry_helpers.h"
#include "field_helpers.h"
static bool forage(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const unsigned areas[]={64,68,72};
    for(unsigned a=0;a<3;++a) {
        REQUIRE(hmbtn_scene_preview_load_area(s,areas[a],error,size));
        for(unsigned t=0;t<110;++t) REQUIRE(step(s,0,error,size));
        const unsigned place=hmbtn_read_le16(hmbtn_scene_preview_resources(s).section+2);
        for(unsigned i=125;i<=172;++i) {
            if(hmbtn_farm_gather_item(&g->gather,i,place,g->game.clock.season)!=93) continue;
            const HmbtnGatherRecord *r=hmbtn_farm_gather_record(i);memcpy(g->intro.player.position,r->position,sizeof(r->position));
            g->intro.player.position[2]+=224;g->intro.player.facing=0;g->control.mode=0;
            memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));
            REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && idle(s,error,size) && g->game.inventory.held_item.id==93);return true;
        }
    }
    return false;
}
static bool counter(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);REQUIRE(g->area==204);
    const uint8_t *zone=NULL;
    for(unsigned i=0;i<50;++i) {
        const uint8_t *z=hmbtn_scene_preview_resources(s).section+16+i*44;
        if(hmbtn_read_le16(z)==0x4000 && hmbtn_read_le16(z+2)==36 && hmbtn_read_le16(z+4)==6) { zone=z;break; }
    }
    REQUIRE(zone && g->village.people[6].actor.visible);at(s,zone);const unsigned face=hmbtn_read_le16(zone+8);REQUIRE(face<4);
    g->intro.player.position[face&1 ? 0 : 2]+=face<2 ? 224 : -224;
    memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));memcpy(g->intro.camera,g->intro.player.position,sizeof(g->intro.camera));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && !g->talk.active);
    REQUIRE(shop_wait(s,HMBTN_SHOP_SELECT,error,size) && g->feed_shop.inn && g->feed_shop.merchant==6);return true;
}
static bool cancel(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const HmbtnInventory inventory=g->game.inventory;const HmbtnEconomy economy=g->game.economy;
    REQUIRE(step(s,HMBTN_PAD_CIRCLE,error,size) && shop_wait(s,HMBTN_SHOP_OFF,error,size));
    REQUIRE(!memcmp(&inventory,&g->game.inventory,sizeof(inventory)) && !memcmp(&economy,&g->game.economy,sizeof(economy)));return true;
}
static bool serving(HmbtnScenePreview *s,HmbtnHost *host,const char *shots,unsigned row,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);REQUIRE(counter(s,error,size));
    for(unsigned i=0;i<row;++i) REQUIRE(step(s,HMBTN_PAD_DOWN,error,size));
    const HmbtnSeedShopEntry *entry=hmbtn_meal_shop_entry(&g->feed_shop,row);REQUIRE(entry);
    const HmbtnFarmItem *item=hmbtn_farm_item(entry->item);const bool drink=(item->flags&0x400u)!=0;
    const HmbtnInventory inventory=g->game.inventory;const HmbtnFarmClock clock=g->game.clock;
    const unsigned stamina=g->farm_action.stamina,fatigue=g->farm_action.fatigue,happiness=g->farm_action.happiness,
        gold=g->game.economy.gold,spent=g->game.economy.ledger[0][1],face=g->village.people[6].actor.facing;
    REQUIRE(capture(s,host,shots,g->feed_shop.inn_drinks ? "inn-drink-menu" : "inn-food-menu",error,size));
    REQUIRE(step(s,HMBTN_PAD_TRIANGLE,error,size) && shop_wait(s,HMBTN_SHOP_INFO,error,size) && g->feed_shop.request==entry->description);
    REQUIRE(capture(s,host,shots,"inn-info",error,size) && shop_wait(s,HMBTN_SHOP_SELECT,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->game.economy.gold==gold-(unsigned)item->buy_price);
    REQUIRE(shop_wait(s,HMBTN_SHOP_RESULT,error,size) && g->feed_shop.request==0x118);
    REQUIRE(!memcmp(&inventory,&g->game.inventory,sizeof(inventory)) && g->farm_action.stamina==stamina && g->farm_action.fatigue==fatigue);
    for(unsigned t=0;t<3000 && !g->event_item.phase;++t) REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
    REQUIRE(g->feed_shop.phase==HMBTN_SHOP_SERVE && g->feed_shop.cue_started);
    REQUIRE(g->event_item.phase==(drink ? HMBTN_EVENT_ITEM_SERVE_DRINK : HMBTN_EVENT_ITEM_SERVE_FOOD) && g->event_item.shown==entry->item);
    REQUIRE(g->intro.action_active==drink && g->intro.player.sprite.animation==(drink ? 0 : 5));
    REQUIRE(g->game.inventory.held_item.id==entry->item && g->game.inventory.held_item.count==1 && !g->game.inventory.held_item.modifier);
    REQUIRE(!hmbtn_save_available(g,error,size));error[0]=0;
    char name[64];snprintf(name,sizeof(name),"inn-serving-%u",entry->item);REQUIRE(capture(s,host,shots,name,error,size));
    REQUIRE(step(s,HMBTN_PAD_START|HMBTN_PAD_R2|HMBTN_PAD_TRIANGLE|HMBTN_PAD_SQUARE,error,size) && g->menu.mode==HMBTN_MENU_NONE);
    bool hidden=drink;
    for(unsigned t=0;t<800 && g->event_item.phase;++t) {
        REQUIRE(g->farm_action.stamina==stamina && g->farm_action.fatigue==fatigue && g->game.inventory.held_item.id==entry->item);
        if(!drink && !hidden && g->intro.player.sprite.frame>=2) { REQUIRE(capture(s,host,shots,"inn-food-hidden",error,size));hidden=true; }
        REQUIRE(step(s,0,error,size));
    }
    const unsigned energy=stamina+(unsigned)item->stamina,expected=energy>g->farm_action.max_stamina ? g->farm_action.max_stamina : energy;
    const unsigned tired=fatigue<(unsigned)-item->fatigue ? 0 : fatigue-(unsigned)-item->fatigue;
    REQUIRE(hidden && !g->event_item.phase && !g->intro.action_active && !memcmp(&inventory,&g->game.inventory,sizeof(inventory)));
    REQUIRE(g->farm_action.stamina==expected && g->farm_action.fatigue==tired && g->farm_action.happiness==happiness);
    for(unsigned t=0;t<10;++t) REQUIRE(step(s,0,error,size));
    REQUIRE(g->feed_shop.phase==HMBTN_SHOP_OFF && g->game.economy.gold==gold-(unsigned)item->buy_price && g->game.economy.ledger[0][1]==spent+(unsigned)item->buy_price);
    REQUIRE(!memcmp(&clock,&g->game.clock,sizeof(clock)) && !g->talk.active && !g->visit.active && g->village.people[6].actor.facing==face);
    REQUIRE(!g->travel_tick && !g->sleep.active && g->farm_action.stamina==expected && g->farm_action.fatigue==tired);return true;
}
static bool checkpoint(HmbtnScenePreview **s,HmbtnHost *host,const char *root,const char *base,const char *suffix,char *error,size_t size)
{ char path[1024];if(snprintf(path,sizeof(path),"%s%s",base,suffix)>=(int)sizeof(path)) return false;return field_reload(s,host,root,path,error,size); }
int main(int argc,char **argv)
{
    if(argc<3 || argc>4) return 2;const char *shots=argc==4 ? argv[3] : NULL;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock clock={0,0,3,2,12,0,0};s=hmbtn_scene_area_create_at(host.renderer,argv[1],64,&clock,error,sizeof(error));CHECK(s);
    for(unsigned t=0;t<110;++t) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->game.economy.gold=3000;
    g->farm_action.max_stamina=200;g->farm_action.stamina=0;g->farm_action.fatigue=100;g->farm_action.happiness=12;
    g->village.flags[251+288]=g->village.flags[251+140]=true;
    CHECK(forage(s,error,sizeof(error)) && hmbtn_scene_preview_load_area(s,48,error,sizeof(error)));
    for(unsigned t=0;t<110;++t) CHECK(step(s,0,error,sizeof(error)));
    CHECK(travel(s,204,error,sizeof(error)) && counter(s,error,sizeof(error)) && hmbtn_meal_shop_rows(&g->feed_shop)==6 && !g->feed_shop.inn_drinks);
    CHECK(cancel(s,error,sizeof(error)) && counter(s,error,sizeof(error)));g->game.economy.gold=499;
    const unsigned spent=g->game.economy.ledger[0][1];
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && g->feed_shop.request==0x4f);
    CHECK(capture(s,&host,shots,"inn-poor",error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_INFO,error,sizeof(error)) && g->feed_shop.request==0x117 && shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(g->game.economy.gold==499 && g->game.economy.ledger[0][1]==spent && g->game.inventory.held_item.id==93 && cancel(s,error,sizeof(error)));g->game.economy.gold=3000;
    for(unsigned row=0;row<6;++row) CHECK(serving(s,&host,shots,row,error,sizeof(error)));
    CHECK(g->game.economy.gold==1450 && g->farm_action.stamina==121 && g->farm_action.fatigue==67);
    CHECK(checkpoint(&s,&host,argv[1],argv[2],".day",error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    const unsigned minute=g->game.clock.hour*60+g->game.clock.minute;CHECK(minute<1079);hmbtn_session_advance_minutes(g,1079-minute);
    for(unsigned t=0;t<110;++t) CHECK(step(s,0,error,sizeof(error)));
    CHECK(counter(s,error,sizeof(error)) && !g->feed_shop.inn_drinks && cancel(s,error,sizeof(error)));
    CHECK(g->game.clock.hour==17 && g->game.clock.minute==59);hmbtn_session_advance_minutes(g,1);
    for(unsigned t=0;t<110;++t) CHECK(step(s,0,error,sizeof(error)));
    CHECK(counter(s,error,sizeof(error)) && g->feed_shop.inn_drinks && hmbtn_meal_shop_rows(&g->feed_shop)==4 && cancel(s,error,sizeof(error)));
    for(unsigned row=0;row<4;++row) CHECK(serving(s,&host,shots,row,error,sizeof(error)));
    CHECK(g->game.economy.gold==450 && g->farm_action.stamina==122 && g->farm_action.fatigue==22 && g->farm_action.happiness==12 && g->game.inventory.held_item.id==93);
    CHECK(checkpoint(&s,&host,argv[1],argv[2],".night",error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(travel(s,48,error,sizeof(error)) && g->game.inventory.held_item.id==93);
    CHECK(checkpoint(&s,&host,argv[1],argv[2],"",error,sizeof(error)));
    puts("Inn: actual forage/door/Doug counter, refusal, six day/four night entries, 18h menu switch, row-17 serving, restored hand/status/payment and resume passed");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

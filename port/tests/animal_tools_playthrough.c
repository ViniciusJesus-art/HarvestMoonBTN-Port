/* Installed Saibara conversation -> original tool menus -> barn care/products.
 * Fixtures: money, named nonpregnant animals, affinity and one maturity edge. */
#include "poultry_helpers.h"
#include "field_helpers.h"
static void animal(HmbtnScenePreview *s,unsigned slot)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);HmbtnFarmAnimal *a=&g->livestock.animals[slot];
    memcpy(g->intro.player.position,a->actor.position,sizeof(a->actor.position));
    g->intro.player.position[0]-=200;g->intro.player.facing=3;g->control.mode=0;
}
static bool action(HmbtnScenePreview *s,unsigned slot,unsigned id,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<12 && g->game.inventory.equipped.id!=id;++i) {
        REQUIRE(step(s,HMBTN_PAD_R1,error,size) && idle(s,error,size));
    }
    REQUIRE(g->game.inventory.equipped.id==id);animal(s,slot);
    REQUIRE(step(s,HMBTN_PAD_SQUARE,error,size) && g->farm_action.phase==HMBTN_FARM_ANIMAL_TOOL);
    for(unsigned i=0;i<500 && (g->farm_action.phase!=HMBTN_FARM_ACTION_FREE || g->event_item.phase!=HMBTN_EVENT_ITEM_IDLE);++i)
        REQUIRE(step(s,0,error,size));
    REQUIRE(g->farm_action.phase==HMBTN_FARM_ACTION_FREE && g->event_item.phase==HMBTN_EVENT_ITEM_IDLE);return true;
}
static bool shot(HmbtnScenePreview *s,HmbtnHost *host,const char *dir,const char *name,char *error,size_t size)
{
    uint8_t *a=NULL,*b=NULL;size_t na=0,nb=0;
    REQUIRE(hmbtn_save_encode(hmbtn_scene_preview_session(s),&a,&na,error,size));
    bool ok=capture(s,host,dir,name,error,size)
        && hmbtn_save_encode(hmbtn_scene_preview_session(s),&b,&nb,error,size) && na==nb && !memcmp(a,b,na);
    free(a);free(b);return ok;
}
static bool night(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    REQUIRE(feed(s,9,0,error,size) && feed(s,41,0,error,size));
    REQUIRE(feed(s,9,0,error,size) && feed(s,41,1,error,size));
    g->game.weather.tomorrow=HMBTN_WEATHER_SUNNY;
    REQUIRE(travel(s,0,error,size) && sleep_day(s,error,size) && travel(s,96,error,size));return true;
}
int main(int argc,char **argv)
{
    if(argc<4 || argc>5) return 2;
    const char *shots=argc==5 ? argv[4] : NULL;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock clock={0,0,3,3,12,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],156,&clock,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->game.economy.gold=10000;g->game.inventory.supplies[0]=40;
    g->village.flags[251+288]=g->village.flags[251+140]=true;
    HmbtnVillager *smith=&g->village.people[4];
    if(!smith->actor.visible) {
        for(unsigned area=128;area<216;area+=4) {
            unsigned location;CHECK(hmbtn_scene_preview_area_location(s,area,&location,error,sizeof(error)));
            if(location==smith->location) { CHECK(hmbtn_scene_preview_load_area(s,area,error,sizeof(error)) && step(s,0,error,sizeof(error)));break; }
        }
    }
    CHECK(smith->actor.visible);memcpy(g->intro.player.position,smith->actor.position,sizeof(smith->actor.position));
    g->intro.player.position[2]+=300;g->intro.player.facing=0;g->control.mode=0;
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && g->talk.active);
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)) && g->feed_shop.blacksmith);
    CHECK(capture(s,&host,shots,"saibara-main",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && step(s,HMBTN_PAD_DOWN,error,sizeof(error))
        && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_TOOL_LIST,error,sizeof(error)) && hmbtn_blacksmith_tool_count(&g->feed_shop)==3);
    CHECK(capture(s,&host,shots,"saibara-tools",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_TRIANGLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_INFO,error,sizeof(error)));
    CHECK(g->feed_shop.request==0xaa && capture(s,&host,shots,"saibara-brush-description",error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_TOOL_LIST,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(g->game.economy.gold==10000 && hmbtn_blacksmith_tool_count(&g->feed_shop)==3);
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_TOOL_LIST,error,sizeof(error)));
    g->game.economy.gold=799;
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)));
    CHECK(g->feed_shop.request==0x4f && g->game.economy.gold==799);
    CHECK(shop_wait(s,HMBTN_SHOP_TOOL_LIST,error,sizeof(error)));g->game.economy.gold=10000;
    const unsigned ids[]={0x1c,0x1a,0x1b},prices[]={800,2000,1800};
    for(unsigned i=0;i<3;++i) {
        CHECK(g->feed_shop.item==ids[i]);const unsigned gold=g->game.economy.gold;
        CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)));
        CHECK(g->feed_shop.request==0xae && g->game.economy.gold==gold);
        CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)) && g->game.economy.gold==gold-prices[i]);
        CHECK(hmbtn_inventory_count(&g->game.inventory,(int)ids[i],0,2)==1);
        if(i<2) CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_TOOL_LIST,error,sizeof(error)));
    }
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && g->feed_shop.request==0xad);
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)) && step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_OFF,error,sizeof(error)));
    for(unsigned i=0;i<1500 && g->talk.active;++i) CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(!g->talk.active && !g->talk.script.boundary && g->game.economy.gold==5400);
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    const uint16_t name[9]={0x100,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff};
    CHECK(hmbtn_livestock_buy(&g->livestock,0x161,name,&g->game.clock) && hmbtn_livestock_buy(&g->livestock,0x165,name,&g->game.clock));
    g->livestock.animals[0].growth=27;g->livestock.animals[0].affection=120;g->livestock.animals[1].affection=210;
    CHECK(hmbtn_scene_preview_load_area(s,96,error,sizeof(error)));
    CHECK(action(s,0,0x1a,error,sizeof(error)) && !g->game.inventory.held_item.id && !g->livestock.animals[0].milked_today);
    CHECK(night(s,error,sizeof(error)) && hmbtn_livestock_kind(&g->livestock.animals[0])==0x162);
    const unsigned love=g->livestock.animals[0].affection;
    CHECK(action(s,0,0x1c,error,sizeof(error)) && g->livestock.animals[0].brushed_today && g->livestock.animals[0].affection==love);
    CHECK(action(s,0,0x1c,error,sizeof(error)) && g->livestock.animals[0].affection==love);
    CHECK(action(s,0,0x1a,error,sizeof(error)) && g->game.inventory.held_item.id==0x8c && g->livestock.animals[0].milked_today);
    CHECK(shot(s,&host,shots,"cow-milk-held",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_R2,error,sizeof(error)) && idle(s,error,sizeof(error)) && g->game.inventory.items[0].id==0x8c);
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(g->livestock.animals[0].milked_today && g->livestock.animals[0].brushed_today);
    CHECK(action(s,0,0x1a,error,sizeof(error)) && !g->game.inventory.held_item.id);
    CHECK(action(s,1,0x1b,error,sizeof(error)) && g->game.inventory.held_item.id==0x93
        && !g->livestock.animals[1].days_since_shearing && hmbtn_livestock_pack(&g->livestock.animals[1])==8);
    CHECK(shot(s,&host,shots,"sheep-shorn-wool",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_R2,error,sizeof(error)) && idle(s,error,sizeof(error)));
    CHECK(action(s,1,0x1b,error,sizeof(error)) && !g->game.inventory.held_item.id);
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    unsigned shipped=0;
    for(unsigned i=0;i<2;++i) {
        CHECK(step(s,HMBTN_PAD_R2,error,sizeof(error)) && idle(s,error,sizeof(error)));
        const HmbtnFarmItem *item=hmbtn_farm_item(g->game.inventory.held_item.id);CHECK(item && item->sell_price>0);shipped+=(unsigned)item->sell_price;
        CHECK(travel(s,0,error,sizeof(error)) && ship(s,error,sizeof(error)) && travel(s,96,error,sizeof(error)));
    }
    const unsigned gold=g->game.economy.gold;
    for(unsigned day=0;day<7;++day) {
        CHECK(night(s,error,sizeof(error)) && g->livestock.animals[1].days_since_shearing==day+1
            && g->game.economy.gold==gold+shipped && !g->livestock.animals[0].milked_today && !g->livestock.animals[0].brushed_today);
        CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    }
    animal(s,1);CHECK(step(s,0,error,sizeof(error)));
    CHECK(hmbtn_livestock_pack(&g->livestock.animals[1])==5 && shot(s,&host,shots,"sheep-regrown",error,sizeof(error)));
    CHECK(action(s,1,0x1b,error,sizeof(error)) && g->game.inventory.held_item.id==0x93);
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);
    puts("Saibara original conversation/tools/funds/cancel/ownership, cow maturity/brush/milk, sheep shear/regrowth/art, bags/doors/shipping, eight real nights and current saves passed.");return 0;
fail:hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

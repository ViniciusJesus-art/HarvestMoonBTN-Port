/* Actual Barley purchase/editor -> named animals/menus -> real feed/night/save.
 * Fixtures: purse, fodder, prior house/coop; barn order calls the same domain
 * transaction already exercised through Gotz by barn_upgrade_playthrough. */
#include "poultry_helpers.h"
#include "field_helpers.h"
static bool barley(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const HmbtnIntroActor *a=&g->village.people[15].actor;
    REQUIRE(a->visible && g->village.people[15].location==9);
    memcpy(g->intro.player.position,a->position,sizeof(a->position));g->intro.player.position[2]+=224;
    g->intro.player.facing=0;g->control.mode=0;
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && shop_wait(s,HMBTN_SHOP_SELECT,error,size));
    REQUIRE(g->feed_shop.request==0x6b && g->feed_shop.livestock==&g->livestock);return true;
}
static bool name_animal(HmbtnScenePreview *s,bool sheep,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && shop_wait(s,HMBTN_SHOP_NAME,error,size) && editor_wait(s,error,size));
    if(sheep) REQUIRE(step(s,HMBTN_PAD_RIGHT,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && step(s,HMBTN_PAD_START,error,size) && editor_wait(s,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && shop_wait(s,HMBTN_SHOP_SELECT,error,size));
    REQUIRE(hmbtn_livestock_count(&g->livestock,sheep)==1);return true;
}
static bool capture_stable(HmbtnScenePreview *s,HmbtnHost *host,const char *shots,const char *name,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const HmbtnLivestock herd=g->livestock;
    const HmbtnGameState game=g->game;
    REQUIRE(capture(s,host,shots,name,error,size));
    REQUIRE(!memcmp(&herd,&g->livestock,sizeof(herd)) && !memcmp(&game,&g->game,sizeof(game)));return true;
}
int main(int argc,char **argv)
{
    if(argc<4 || argc>5) return 2;const char *shots=argc==5 ? argv[4] : NULL;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock clock={0,0,3,3,12,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],136,&clock,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->game.economy.gold=10000;g->game.inventory.supplies[0]=20;
    g->poultry.upgraded=true;g->construction.house_level=1;
    g->village.flags[251+288]=g->village.flags[251+140]=true;
    CHECK(barley(s,error,sizeof(error)) && step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && g->feed_shop.item==0x161);
    CHECK(capture_stable(s,&host,shots,"barley-cow-menu",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_TRIANGLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && g->feed_shop.request==0x67);
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_NAME,error,sizeof(error)) && editor_wait(s,error,sizeof(error)));
    CHECK(capture_stable(s,&host,shots,"barley-name",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(g->game.economy.gold==10000 && !hmbtn_livestock_count(&g->livestock,false));
    CHECK(name_animal(s,false,error,sizeof(error)) && g->game.economy.gold==4000);
    CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && g->feed_shop.item==0x165 && name_animal(s,true,error,sizeof(error)));
    CHECK(!g->game.economy.gold && g->livestock.animals[0].name[0]==0 && g->livestock.animals[1].name[0]==1);
    CHECK(step(s,HMBTN_PAD_UP,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && g->feed_shop.request==0x4f);
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)) && step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_OFF,error,sizeof(error)));
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)) && travel(s,96,error,sizeof(error)));
    CHECK(g->livestock.animals[0].area==96 && g->livestock.animals[0].growth==14 && g->livestock.animals[1].growth==14);
    g->intro.player.position[0]=1900;g->intro.player.position[2]=-200;
    for(unsigned i=0;i<10;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(capture_stable(s,&host,shots,"barn-animals",error,sizeof(error)));
    /* Original talk text with the actual stored name; repeated talk gains once. */
    HmbtnFarmAnimal *cow=&g->livestock.animals[0];cow->timer=300;cow->state=1;
    memcpy(g->intro.player.position,cow->actor.position,sizeof(cow->actor.position));g->intro.player.position[0]-=224;g->intro.player.facing=3;
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && g->house.request==0x6bd && cow->talked_today);
    for(unsigned i=0;i<1500 && g->house.message.phase!=HMBTN_MESSAGE_WAIT_END;++i)
        CHECK(step(s,g->house.message.phase==HMBTN_MESSAGE_WAIT_PAGE ? HMBTN_PAD_CROSS : 0,error,sizeof(error)));
    CHECK(g->house.message.phase==HMBTN_MESSAGE_WAIT_END);
    CHECK(capture_stable(s,&host,shots,"cow-talk",error,sizeof(error)));
    for(unsigned i=0;i<1200 && g->house.mode!=HMBTN_HOUSE_FREE;++i) CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(g->house.mode==HMBTN_HOUSE_FREE);
    /* Derived cow/sheep pages, no copied count in the presentation. */
    CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)));
    for(unsigned i=0;i<30 && g->menu.phase!=3;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_RIGHT,error,sizeof(error)) && step(s,HMBTN_PAD_RIGHT,error,sizeof(error)) && g->menu.page==3);
    CHECK(capture_stable(s,&host,shots,"cow-status",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_RIGHT,error,sizeof(error)) && g->menu.page==4 && capture_stable(s,&host,shots,"sheep-status",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)));
    for(unsigned i=0;i<30 && g->menu.mode!=HMBTN_MENU_NONE;++i) CHECK(step(s,0,error,sizeof(error)));
    /* Upgrade with purchased animals present. This is a normal domain order;
     * the separate Gotz traversal validates acquisition through his dialogue. */
    g->game.economy.gold=6800;g->game.inventory.supplies[1]=500;
    CHECK(hmbtn_construction_order(&g->construction,&g->poultry,&g->game.inventory,&g->game.economy)==HMBTN_BUILD_OK);
    for(unsigned day=0;day<4;++day) {
        CHECK(feed(s,9,0,error,sizeof(error)) && feed(s,41,0,error,sizeof(error)));
        CHECK(feed(s,9,0,error,sizeof(error)) && feed(s,41,1,error,sizeof(error)));
        CHECK(travel(s,0,error,sizeof(error)) && sleep_day(s,error,sizeof(error)));
        CHECK(g->livestock.animals[0].fed_yesterday && g->livestock.animals[1].fed_yesterday
            && !g->feed.barn[0] && !g->feed.barn[1] && g->livestock.animals[0].growth==15+day);
        CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
        CHECK(travel(s,96,error,sizeof(error)));
    }
    CHECK(g->construction.barn_level==1 && hmbtn_livestock_capacity(&g->livestock)==20
        && hmbtn_read_le16(hmbtn_scene_preview_resources(s).section+4)==100);
    /* Controlled age fixture reaches the original 28-day cow stage next night. */
    g->livestock.animals[0].growth=27;
    CHECK(feed(s,9,0,error,sizeof(error)) && feed(s,41,18,error,sizeof(error)));
    CHECK(feed(s,9,0,error,sizeof(error)) && feed(s,41,19,error,sizeof(error)));
    CHECK(step(s,0,error,sizeof(error)) && capture_stable(s,&host,shots,"animals-expanded-barn",error,sizeof(error)));
    CHECK(travel(s,0,error,sizeof(error)) && sleep_day(s,error,sizeof(error)) && travel(s,96,error,sizeof(error)));
    CHECK(hmbtn_livestock_kind(&g->livestock.animals[0])==0x162 && g->livestock.animals[0].fed_yesterday
        && g->livestock.animals[1].fed_yesterday && !g->feed.barn[18] && !g->feed.barn[19] && g->game.inventory.supplies[0]==10);
    memcpy(g->intro.player.position,g->livestock.animals[0].actor.position,sizeof(g->intro.player.position));
    g->intro.player.position[0]-=400;g->intro.player.position[2]+=400;CHECK(step(s,0,error,sizeof(error)));
    CHECK(capture_stable(s,&host,shots,"adult-cow",error,sizeof(error)));
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(hmbtn_farm_menu_animal_count(&g->menu,1)==1 && hmbtn_farm_menu_animal_count(&g->menu,2)==1);
    CHECK(travel(s,0,error,sizeof(error)) && travel(s,80,error,sizeof(error)) && field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);
    puts("Livestock: real Barley menu/naming/cancel/funds, original animal art/talk/status, food and real nights, upgrade with animals, maturity and save/app resume passed.");return 0;
fail:hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

/* Real Barley conversation, original sale texts/UI and save in a new app.
 * Fixtures provide a mixed herd, ages/affection/prize and a reserved death. */
#include "poultry_helpers.h"
#include "field_helpers.h"
#include "shop_ui_helpers.h"
static bool barley(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const HmbtnIntroActor *a=&g->village.people[15].actor;
    REQUIRE(a->visible && g->village.people[15].location==9);
    memcpy(g->intro.player.position,a->position,sizeof(a->position));g->intro.player.position[2]+=224;
    g->intro.player.facing=0;g->control.mode=0;
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && shop_wait(s,HMBTN_SHOP_SELECT,error,size));
    REQUIRE(g->feed_shop.request==0x46 && g->feed_shop.bank==2
        && !memcmp(&g->feed_shop.message,&g->dialogue.message,sizeof(g->dialogue.message)));
    REQUIRE(shop_cursor_pixels(s,23,20,error,size));return true;
}
static bool choose(HmbtnScenePreview *s,unsigned selection,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<6 && g->feed_shop.selection!=selection;++i) REQUIRE(step(s,HMBTN_PAD_DOWN,error,size));
    REQUIRE(g->feed_shop.selection==selection);return true;
}
int main(int argc,char **argv)
{
    if(argc<3 || argc>4) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};const char *shots=argc==4 ? argv[3] : NULL;
    CHECK(hmbtn_host_open(&host));const HmbtnFarmClock clock={.day=3,.weekday=3,.hour=12};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],136,&clock,error,sizeof(error));CHECK(s);
    for(unsigned t=0;t<110;++t) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->game.economy.gold=100;
    g->village.flags[251+288]=g->village.flags[251+140]=true;
    CHECK(barley(s,error,sizeof(error)) && choose(s,6,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && g->feed_shop.request==0x76);
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)) && choose(s,7,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && g->feed_shop.request==0x78);
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    const uint16_t name[9]={0,1,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff};
    for(unsigned i=0;i<9;++i) {
        CHECK(hmbtn_livestock_buy(&g->livestock,i==6 ? 0x165 : 0x161,name,&g->game.clock));
        HmbtnFarmAnimal *a=&g->livestock.animals[i];a->growth=i==7 ? 14 : 28;a->affection=120;
        a->name[0]=(uint16_t)i;a->days_sick=i==5 ? 2 : 0;
    }
    g->livestock.animals[6].prize_winner=true;g->livestock.animals[6].days_since_shearing=0;
    g->livestock.animals[8].dead=true;g->feed.barn[3]=true;
    CHECK(choose(s,6,error,sizeof(error)) && capture(s,&host,shots,"barley-sale-menu",error,sizeof(error)));
    CHECK(g->feed_shop.request==0x46 && shop_cursor_pixels(s,23,62,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_TRIANGLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && g->feed_shop.request==0x69);
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_SELL_LIST,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)) && g->game.economy.gold==100);
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_SELL_LIST,error,sizeof(error)));
    for(unsigned i=0;i<5;++i) CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)));
    CHECK(g->feed_shop.sale_cursor==5 && g->feed_shop.sale_top==2);
    CHECK(capture(s,&host,shots,"cow-sale-second-page",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_SELL_CONFIRM,error,sizeof(error)) && g->feed_shop.request==0x7f);
    CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)) && g->game.economy.gold==100 && g->livestock.animals[5].occupied);
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_SELL_LIST,error,sizeof(error)));
    for(unsigned i=0;i<5;++i) CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_SELL_CONFIRM,error,sizeof(error)));
    const HmbtnFarmAnimal neighbor=g->livestock.animals[4],dead=g->livestock.animals[8];
    CHECK(capture(s,&host,shots,"cow-sale-confirm",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(g->game.economy.gold==4600 && !g->livestock.animals[5].occupied && g->feed.barn[3]);
    CHECK(!memcmp(&neighbor,&g->livestock.animals[4],sizeof(neighbor)) && !memcmp(&dead,&g->livestock.animals[8],sizeof(dead)));
    CHECK(choose(s,7,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_SELL_LIST,error,sizeof(error)) && capture(s,&host,shots,"sheep-sale-shorn",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_SELL_CONFIRM,error,sizeof(error)) && g->feed_shop.request==0x81);
    CHECK(capture(s,&host,shots,"sheep-sale-confirm",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(g->game.economy.gold==10600 && !g->livestock.animals[6].occupied && g->livestock.animals[8].dead);
    /* A normal purchase/editor reuses the first vacated slot. */
    g->game.economy.gold=10600;
    CHECK(choose(s,4,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_NAME,error,sizeof(error)) && editor_wait(s,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && step(s,HMBTN_PAD_START,error,sizeof(error)) && editor_wait(s,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(g->game.economy.gold==4600 && g->livestock.animals[5].occupied && g->livestock.animals[5].growth==14 && !g->livestock.animals[6].occupied);
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_LEAVE,error,sizeof(error)) && g->feed_shop.request==0x6b);
    CHECK(shop_wait(s,HMBTN_SHOP_OFF,error,sizeof(error)));
    CHECK(field_reload(&s,&host,argv[1],argv[2],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(g->game.economy.gold==4600 && g->livestock.animals[5].occupied && !g->livestock.animals[6].occupied
        && g->livestock.animals[8].dead && g->feed.barn[3]);
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);
    puts("Barley sales: empty lists, original menus/texts, pages, cancel, sick cow/prize shorn sheep, payment, real repurchase and save passed.");return 0;
fail:hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

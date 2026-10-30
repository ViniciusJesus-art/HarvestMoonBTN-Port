/* Original shop, ANI, area doors, real flight/box/payout and fresh resumes.
 * Purse and ordinary test goods are fixtures; the basket is purchased once. */
#include "poultry_helpers.h"
#include "field_helpers.h"
static bool settle(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<1000 && (g->basket_action.phase || g->event_item.phase || g->carry.phase);++i) REQUIRE(step(s,0,error,size));
    REQUIRE(!g->basket_action.phase && !g->event_item.phase && !g->carry.phase);return true;
}
static bool close_shop(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<3000 && (g->feed_shop.phase!=HMBTN_SHOP_OFF || g->event_item.phase);++i)
        REQUIRE(step(s,g->event_item.phase ? 0 : HMBTN_PAD_CROSS,error,size));
    REQUIRE(g->feed_shop.phase==HMBTN_SHOP_OFF && !g->event_item.phase);return true;
}
static void aim(HmbtnScenePreview *s,const int32_t point[3])
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);memcpy(g->intro.player.position,point,3*sizeof(*point));
    g->intro.player.position[2]+=400;g->intro.player.facing=0;g->control.mode=0;
    memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));memcpy(g->intro.camera,g->intro.player.position,sizeof(g->intro.camera));
}
static bool counter(HmbtnScenePreview *s,HmbtnHost *host,const char *shots,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const HmbtnGatherRecord *r=hmbtn_farm_gather_record(117);
    REQUIRE(r && r->item==100 && g->village.people[1].actor.visible);
    memcpy(g->intro.player.position,r->position,sizeof(r->position));g->intro.player.position[0]-=400;g->intro.player.position[1]=0;
    g->intro.player.facing=3;g->control.mode=0;memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));
    memcpy(g->intro.camera,g->intro.player.position,sizeof(g->intro.camera));
    REQUIRE(capture(s,host,shots,"basket-stock",error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->feed_shop.basket && g->event_item.shown==100);
    REQUIRE(shop_wait(s,HMBTN_SHOP_OFFER,error,size) && g->feed_shop.request==0xd5);
    REQUIRE(capture(s,host,shots,"basket-offer",error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->feed_shop.phase==HMBTN_SHOP_MAKER_CONFIRM);return true;
}
static bool pickup(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);REQUIRE(g->game.basket.placed);aim(s,g->game.basket.position);
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->basket_action.phase==HMBTN_BASKET_PICKUP);
    REQUIRE(!hmbtn_save_available(g,error,size));error[0]=0;
    REQUIRE(settle(s,error,size) && g->game.inventory.held_item.id==100 && !g->game.basket.placed);return true;
}
static bool drop(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);REQUIRE(g->game.inventory.held_item.id==100);
    bool found=false;
    for(int z=3200;z<=4000 && !found;z+=400) for(int x=-5000;x<=5000 && !found;x+=400) {
        int32_t p[3]={x,0,z},target[3]={x,0,z-200};
        if(hmbtn_farm_field_probe(hmbtn_session_field(g),target) || !hmbtn_farm_settle(NULL,&hmbtn_scene_preview_resources(s).scene->terrain,target,0xc087,200)) continue;
        memcpy(g->intro.player.position,p,sizeof(p));g->intro.player.facing=0;g->control.mode=0;
        REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));found=g->basket_action.phase==HMBTN_BASKET_DROP;
    }
    REQUIRE(found && settle(s,error,size) && g->game.basket.placed && !g->game.inventory.held_item.id);return true;
}
static bool add(HmbtnScenePreview *s,unsigned item,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);aim(s,g->game.basket.position);
    g->game.inventory.held_item=(HmbtnStoredTool){(uint16_t)item,0,1};
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->basket_action.phase==HMBTN_BASKET_ADD);
    REQUIRE(settle(s,error,size) && !g->game.inventory.held_item.id);return true;
}
int main(int argc,char **argv)
{
    if(argc<3 || argc>4) return 2;const char *shots=argc==4 ? argv[3] : NULL;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock clock={0,0,2,3,12,0,0};s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&clock,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->game.economy.gold=10000;
    g->village.flags[251+288]=g->village.flags[251+140]=true;
    CHECK(hmbtn_scene_preview_load_area(s,180,error,sizeof(error)));
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(counter(s,&host,shots,error,sizeof(error)) && step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && close_shop(s,error,sizeof(error)));
    CHECK(!g->game.basket.owned && g->game.economy.gold==10000);
    g->game.economy.gold=4999;
    CHECK(counter(s,&host,shots,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && g->feed_shop.request==0x4f && close_shop(s,error,sizeof(error)));
    CHECK(!g->game.basket.owned);g->game.economy.gold=10000;
    CHECK(counter(s,&host,shots,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && g->feed_shop.request==0xbb && g->game.economy.gold==10000);
    CHECK(close_shop(s,error,sizeof(error)) && g->game.basket.owned && g->game.basket.placed && g->game.economy.gold==5000);
    CHECK(!hmbtn_basket_shop_open(&g->feed_shop,&g->village,&g->intro.player,&g->game.clock,g->area,hmbtn_scene_preview_resources(s).section,0x2000,&g->game.inventory));
    CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)) && pickup(s,error,sizeof(error)));
    CHECK(capture(s,&host,shots,"basket-held",error,sizeof(error)));
    CHECK(!hmbtn_inventory_cycle(&g->game.inventory,false,false) && !hmbtn_inventory_swap(&g->game.inventory,9,0));
    CHECK(travel(s,80,error,sizeof(error)) && g->game.inventory.held_item.id==100);
    CHECK(travel(s,0,error,sizeof(error)) && drop(s,error,sizeof(error)));
    const int32_t placed[3]={g->game.basket.position[0],g->game.basket.position[1],g->game.basket.position[2]};
    CHECK(hmbtn_scene_preview_load_area(s,68,error,sizeof(error)) && hmbtn_scene_preview_load_area(s,0,error,sizeof(error)));
    CHECK(g->game.basket.placed && !memcmp(placed,g->game.basket.position,sizeof(placed)));
    CHECK(add(s,0x41,error,sizeof(error)) && g->game.basket.count==1);
    unsigned value=hmbtn_farm_shipping_price(&g->game.economy,0x41);
    /* A throw from beyond the direct reach lands in the same physical object. */
    int y=-300,v=0;unsigned flight=0;do { y+=v;v+=9;if(v>45) v=45;++flight; } while(y<g->game.basket.position[1] && flight<100);
    memcpy(g->intro.player.position,g->game.basket.position,sizeof(g->intro.player.position));g->intro.player.position[0]-=(int)flight*44+190;
    g->intro.player.facing=3;g->control.mode=0;g->game.inventory.held_item=(HmbtnStoredTool){0x42,0,1};
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && g->carry.phase>=2 && settle(s,error,sizeof(error)));
    for(unsigned i=0;i<150 && g->carry.thrown[0].item.id;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(g->game.basket.count==2 && !g->carry.thrown[0].item.id);value+=hmbtn_farm_shipping_price(&g->game.economy,0x42);
    const unsigned goods[]={0x41,0xf7,0xa1,0x5c};
    for(unsigned i=2;i<30;++i) { CHECK(add(s,goods[i%4],error,sizeof(error)));value+=hmbtn_farm_shipping_price(&g->game.economy,goods[i%4]); }
    CHECK(g->game.basket.count==30 && g->game.basket.value==value && !g->game.economy.shipping.pending_value);
    CHECK(capture(s,&host,shots,"basket-full",error,sizeof(error)));
    CHECK(add(s,0x41,error,sizeof(error)) && g->house.mode==HMBTN_HOUSE_BOOK && g->house.request==0x383);
    for(unsigned i=0;i<500 && g->house.message.phase!=HMBTN_MESSAGE_WAIT_END;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(capture(s,&host,shots,"basket-full-warning",error,sizeof(error)));
    for(unsigned i=0;i<1000 && g->house.mode!=HMBTN_HOUSE_FREE;++i) CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(g->house.mode==HMBTN_HOUSE_FREE && g->game.basket.count==30 && g->game.basket.value==value && !g->game.inventory.held_item.id);
    char snapshot[1024];CHECK(snprintf(snapshot,sizeof(snapshot),"%s.full",argv[2])<(int)sizeof(snapshot));
    CHECK(field_reload(&s,&host,argv[1],snapshot,error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(pickup(s,error,sizeof(error)));CHECK(snprintf(snapshot,sizeof(snapshot),"%s.held",argv[2])<(int)sizeof(snapshot));
    CHECK(field_reload(&s,&host,argv[1],snapshot,error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(step(s,HMBTN_PAD_SQUARE,error,sizeof(error)) && settle(s,error,sizeof(error)) && g->game.basket.count==30);
    int32_t box[3];CHECK(box_position(&hmbtn_scene_preview_resources(s).scene->terrain,box));
    memcpy(g->intro.player.position,box,sizeof(box));g->intro.player.position[0]-=224;g->intro.player.position[1]=0;
    g->intro.player.facing=3;g->control.mode=0;
    CHECK(step(s,HMBTN_PAD_SQUARE,error,sizeof(error)) && g->basket_action.phase==HMBTN_BASKET_SHIP);
    CHECK(capture(s,&host,shots,"basket-empty-at-box",error,sizeof(error)) && settle(s,error,sizeof(error)));
    CHECK(!g->game.basket.count && !g->game.basket.value && g->game.economy.shipping.pending_value==value && g->game.economy.gold==5000);
    CHECK(step(s,HMBTN_PAD_SQUARE,error,sizeof(error)) && settle(s,error,sizeof(error)) && g->game.economy.shipping.pending_value==value);
    CHECK(drop(s,error,sizeof(error)) && g->game.basket.placed);
    const unsigned minute=g->game.clock.hour*60+g->game.clock.minute;CHECK(minute<1078);
    hmbtn_session_advance_minutes(g,1078-minute);CHECK(g->game.economy.gold==5000+value && !g->game.economy.shipping.pending_value);
    hmbtn_session_clock_events(g,&g->game.clock,HMBTN_CLOCK_MINUTE);CHECK(g->game.economy.gold==5000+value);
    CHECK(field_reload(&s,&host,argv[1],argv[2],error,sizeof(error)));
    puts("Original basket purchase -> pickup/doors/drop/30 goods/full loss/flight/box/payout/resume passed");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

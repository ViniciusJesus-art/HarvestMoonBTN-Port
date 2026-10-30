/* Purse, first house and pot are fixtures. Every ingredient is bought at its
 * installed counter, carried with original ANI and stored through the icebox UI. */
#include "poultry_helpers.h"
#include "field_helpers.h"
#include "cooking.h"
static bool close_shop(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<3000 && (g->feed_shop.phase!=HMBTN_SHOP_OFF || g->event_item.phase);++i)
        REQUIRE(step(s,g->event_item.phase ? 0 : HMBTN_PAD_CROSS,error,size));
    REQUIRE(g->feed_shop.phase==HMBTN_SHOP_OFF && !g->event_item.phase);return true;
}
static bool house_wait(HmbtnScenePreview *s,HmbtnHouseMode mode,char *error,size_t size)
{
    HmbtnHouseInteraction *h=&hmbtn_scene_preview_session(s)->house;
    for(unsigned i=0;i<1600;++i) {
        if(h->mode==mode && (mode==HMBTN_HOUSE_FREE || (!h->closing && h->message.phase==HMBTN_MESSAGE_WAIT_END))) return true;
        REQUIRE(step(s,h->message.phase==HMBTN_MESSAGE_WAIT_PAGE ? HMBTN_PAD_CROSS : 0,error,size));
    }
    return false;
}
static bool ready(HmbtnScenePreview *s,HmbtnKitchenPhase phase,char *error,size_t size)
{
    REQUIRE(house_wait(s,HMBTN_HOUSE_COOKING,error,size));
    REQUIRE(hmbtn_scene_preview_session(s)->house.kitchen.phase==phase);return true;
}
static bool object(HmbtnScenePreview *s,unsigned id,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<50;++i) {
        const uint8_t *z=hmbtn_scene_preview_resources(s).section+16+i*44;
        if(hmbtn_read_le16(z)!=0x4000 || hmbtn_read_le16(z+2)!=id) continue;
        at(s,z);const unsigned face=hmbtn_read_le16(z+8);REQUIRE(face<4);
        g->intro.player.position[face&1 ? 0 : 2]+=face<2 ? 224 : -224;
        memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));
        memcpy(g->intro.camera,g->intro.player.position,sizeof(g->intro.camera));
        return step(s,HMBTN_PAD_CROSS,error,size);
    }
    return false;
}
static bool counter(HmbtnScenePreview *s,unsigned index,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const HmbtnGroceryStock *stock=hmbtn_grocery_stock(index);
    const HmbtnGatherRecord *r=NULL;
    for(unsigned i=0;i<HMBTN_GATHER_RECORDS;++i) { const HmbtnGatherRecord *p=hmbtn_farm_gather_record(i);if(p->location==30 && p->item==stock->item) { r=p;break; } }
    REQUIRE(r && g->village.people[1].actor.visible);
    memcpy(g->intro.player.position,r->position,sizeof(r->position));g->intro.player.position[0]+=400;
    g->intro.player.position[1]=0;g->intro.player.facing=1;g->control.mode=0;
    memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));memcpy(g->intro.camera,g->intro.player.position,sizeof(g->intro.camera));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->feed_shop.groceries && g->feed_shop.item==stock->item && g->event_item.phase==HMBTN_EVENT_ITEM_SHOW);
    REQUIRE(g->intro.player.sprite.animation==12);
    REQUIRE(shop_wait(s,HMBTN_SHOP_OFFER,error,size) && g->feed_shop.request==stock->description && g->game.inventory.held_item.id==stock->item);
    return step(s,HMBTN_PAD_CROSS,error,size) && g->feed_shop.phase==HMBTN_SHOP_MAKER_CONFIRM;
}
static bool deposit(HmbtnScenePreview *s,unsigned cell,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const unsigned item=g->game.inventory.held_item.id;
    REQUIRE(object(s,4,error,size) && house_wait(s,HMBTN_HOUSE_TOOLBOX,error,size));
    REQUIRE(g->house.storage==2 && g->house.cursor==0);
    if(g->house.detail) REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && house_wait(s,HMBTN_HOUSE_TOOLBOX,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && step(s,HMBTN_PAD_DOWN,error,size));
    if(cell>=4) REQUIRE(step(s,HMBTN_PAD_DOWN,error,size));
    for(unsigned i=0;i<cell%4;++i) REQUIRE(step(s,HMBTN_PAD_RIGHT,error,size));
    REQUIRE(g->house.cursor==9+cell && step(s,HMBTN_PAD_CROSS,error,size));
    REQUIRE(g->game.inventory.icebox[cell].id==item && !g->game.inventory.held_item.id);
    REQUIRE(step(s,HMBTN_PAD_CIRCLE,error,size) && house_wait(s,HMBTN_HOUSE_FREE,error,size));return true;
}
int main(int argc,char **argv)
{
    if(argc<3 || argc>4) return 2;const char *shots=argc==4 ? argv[3] : NULL;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock clock={0,0,2,3,12,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&clock,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->game.economy.gold=1000;
    g->poultry.upgraded=true;g->construction.house_level=1;g->game.cooking.utensils=4;
    g->village.flags[251+288]=g->village.flags[251+140]=true;
    const unsigned ids[]={153,154,165,166,167};
    for(unsigned i=0;i<6;++i) {
        CHECK(hmbtn_scene_preview_load_area(s,180,error,sizeof(error)));
        for(unsigned t=0;t<110;++t) CHECK(step(s,0,error,sizeof(error)));
        const unsigned index=i==5 ? 1 : i,price=index<2 ? 100 : 50;
        const unsigned gold=g->game.economy.gold,spent=g->game.economy.ledger[0][1],face=g->village.people[1].actor.facing;
        CHECK(counter(s,index,error,sizeof(error)) && capture(s,&host,shots,"groceries-confirm",error,sizeof(error)));
        CHECK(!hmbtn_save_available(g,error,sizeof(error)));error[0]=0;
        if(!i) {
            CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && g->feed_shop.request==0xd8);
            CHECK(close_shop(s,error,sizeof(error)) && !g->game.inventory.held_item.id && g->game.economy.gold==gold);
            g->game.economy.gold=99;
            CHECK(counter(s,index,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
            CHECK(shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && g->feed_shop.request==0x4f && close_shop(s,error,sizeof(error)));
            CHECK(!g->game.inventory.held_item.id && g->game.economy.gold==99);g->game.economy.gold=gold;
            CHECK(counter(s,index,error,sizeof(error)));
        }
        CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && g->feed_shop.request==0xbb);
        CHECK(g->game.economy.gold==gold && g->game.economy.ledger[0][1]==spent);
        CHECK(close_shop(s,error,sizeof(error)) && g->game.inventory.held_item.id==ids[index]);
        CHECK(g->game.economy.gold==gold-price && g->game.economy.ledger[0][1]==spent+price && g->village.people[1].actor.facing==face);
        CHECK(capture(s,&host,shots,"groceries-bought-hand",error,sizeof(error)));
        if(i==4) {
            char save[1024];CHECK(snprintf(save,sizeof(save),"%s.held",argv[2])<(int)sizeof(save));
            CHECK(field_reload(&s,&host,argv[1],save,error,sizeof(error)));g=hmbtn_scene_preview_session(s);
        }
        CHECK(step(s,HMBTN_PAD_R2,error,sizeof(error)) && idle(s,error,sizeof(error)) && !g->game.inventory.held_item.id && g->game.inventory.items[0].id==ids[index]);
        CHECK(step(s,HMBTN_PAD_R2,error,sizeof(error)) && idle(s,error,sizeof(error)) && g->game.inventory.held_item.id==ids[index]);
        CHECK(hmbtn_scene_preview_load_area(s,80,error,sizeof(error)));
        for(unsigned t=0;t<110;++t) CHECK(step(s,0,error,sizeof(error)));
        CHECK(deposit(s,index,error,sizeof(error)) && capture(s,&host,shots,"groceries-icebox",error,sizeof(error)));
    }
    CHECK(g->game.economy.gold==550 && g->game.inventory.icebox[1].count==2);
    char save[1024];CHECK(snprintf(save,sizeof(save),"%s.stock",argv[2])<(int)sizeof(save));
    CHECK(field_reload(&s,&host,argv[1],save,error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    /* Real kitchen selection: pot, bought rice and bought curry from icebox. */
    CHECK(object(s,5,error,sizeof(error)) && ready(s,HMBTN_KITCHEN_OPEN,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && ready(s,HMBTN_KITCHEN_FIRST,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && ready(s,HMBTN_KITCHEN_SECOND,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && ready(s,HMBTN_KITCHEN_TOOLS,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_RIGHT,error,sizeof(error)) && step(s,HMBTN_PAD_RIGHT,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && g->house.kitchen.options==4);
    CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)) && ready(s,HMBTN_KITCHEN_ITEMS,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_R1,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)) && ready(s,HMBTN_KITCHEN_CONFIRM,error,sizeof(error)));
    CHECK(capture(s,&host,shots,"groceries-curry-ingredients",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    for(unsigned t=0;t<2000 && g->house.kitchen.phase!=HMBTN_KITCHEN_RESULT;++t) CHECK(step(s,0,error,sizeof(error)));
    CHECK(ready(s,HMBTN_KITCHEN_RESULT,error,sizeof(error)) && g->game.inventory.held_item.id==178);
    CHECK(!g->game.inventory.icebox[0].id && !g->game.inventory.icebox[4].id && g->game.inventory.icebox[1].count==2 && g->game.inventory.icebox[2].id==165 && g->game.inventory.icebox[3].id==166);
    CHECK(capture(s,&host,shots,"groceries-cooked-curry",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && house_wait(s,HMBTN_HOUSE_FREE,error,sizeof(error)));
    CHECK(hmbtn_cooking_record_count(&g->game.cooking)==1 && g->game.economy.gold==550);
    CHECK(field_reload(&s,&host,argv[1],argv[2],error,sizeof(error)));
    puts("Groceries: original five counters, refusal/funds, six purchases, hand/bag/icebox UI, purchased curry preparation and three resumes passed");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

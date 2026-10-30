/* Purse and first-house prerequisite are fixtures. Buy bread, pay to wrap it,
 * use the real bag/storage UI and give that exact present to scheduled Gotz. */
#include "poultry_helpers.h"
#include "field_helpers.h"
#include "item_reaction.h"
static bool close_shop(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<3000 && (g->feed_shop.phase!=HMBTN_SHOP_OFF || g->event_item.phase);++i)
        REQUIRE(step(s,g->event_item.phase ? 0 : HMBTN_PAD_CROSS,error,size));
    REQUIRE(g->feed_shop.phase==HMBTN_SHOP_OFF && !g->event_item.phase);return true;
}
static void aim(HmbtnScenePreview *s,unsigned record,unsigned facing)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const HmbtnGatherRecord *r=hmbtn_farm_gather_record(record);
    memcpy(g->intro.player.position,r->position,sizeof(r->position));g->intro.player.position[0]+=facing==1 ? 400 : -400;
    g->intro.player.position[1]=0;g->intro.player.facing=facing;g->control.mode=0;
    memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));memcpy(g->intro.camera,g->intro.player.position,sizeof(g->intro.camera));
}
static bool wrapping(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);aim(s,118,3);
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->feed_shop.wrapping);
    return shop_wait(s,HMBTN_SHOP_MAKER_CONFIRM,error,size);
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
static bool reject_storage(HmbtnScenePreview *s,unsigned id,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);bool found=false;
    for(unsigned i=0;i<50 && !found;++i) {
        const uint8_t *z=hmbtn_scene_preview_resources(s).section+16+i*44;
        if(hmbtn_read_le16(z)!=0x4000 || hmbtn_read_le16(z+2)!=id) continue;
        at(s,z);const unsigned face=hmbtn_read_le16(z+8);REQUIRE(face<4);
        g->intro.player.position[face&1 ? 0 : 2]+=face<2 ? 224 : -224;
        memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));found=true;
    }
    REQUIRE(found && step(s,HMBTN_PAD_CROSS,error,size) && house_wait(s,HMBTN_HOUSE_TOOLBOX,error,size));
    if(g->house.detail) REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && house_wait(s,HMBTN_HOUSE_TOOLBOX,error,size));
    REQUIRE(step(s,HMBTN_PAD_TRIANGLE,error,size) && g->house.request==1154 && house_wait(s,HMBTN_HOUSE_BOOK,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && house_wait(s,HMBTN_HOUSE_TOOLBOX,error,size));
    const HmbtnInventory before=g->game.inventory;
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && step(s,HMBTN_PAD_DOWN,error,size) && step(s,HMBTN_PAD_CROSS,error,size));
    REQUIRE(g->house.sound==2 && !memcmp(&before,&g->game.inventory,sizeof(before)));
    REQUIRE(step(s,HMBTN_PAD_CIRCLE,error,size) && step(s,HMBTN_PAD_CIRCLE,error,size) && house_wait(s,HMBTN_HOUSE_FREE,error,size));return true;
}
static bool checkpoint(HmbtnScenePreview **s,HmbtnHost *host,const char *root,const char *base,const char *suffix,char *error,size_t size)
{
    char path[1024];if(snprintf(path,sizeof(path),"%s%s",base,suffix)>=(int)sizeof(path)) return false;
    return field_reload(s,host,root,path,error,size);
}
int main(int argc,char **argv)
{
    if(argc<3 || argc>4) return 2;const char *shots=argc==4 ? argv[3] : NULL;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock clock={0,0,2,3,12,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&clock,error,sizeof(error));CHECK(s);
    for(unsigned t=0;t<110;++t) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->game.economy.gold=1000;
    g->poultry.upgraded=true;g->construction.house_level=1;
    g->village.flags[251+288]=g->village.flags[251+140]=true;
    CHECK(hmbtn_scene_preview_load_area(s,180,error,sizeof(error)));
    for(unsigned t=0;t<110;++t) CHECK(step(s,0,error,sizeof(error)));
    aim(s,118,3);CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && g->feed_shop.wrapping && g->feed_shop.request==0xb8);
    CHECK(close_shop(s,error,sizeof(error)) && !g->game.inventory.held_item.id && g->game.economy.gold==1000);
    aim(s,124,1);CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && g->feed_shop.groceries);
    CHECK(shop_wait(s,HMBTN_SHOP_OFFER,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && close_shop(s,error,sizeof(error)));
    CHECK(g->game.inventory.held_item.id==154 && g->game.economy.gold==900);
    const HmbtnStoredTool bread=g->game.inventory.held_item;const unsigned spent=g->game.economy.ledger[0][1],face=g->village.people[1].actor.facing;
    CHECK(wrapping(s,error,sizeof(error)) && g->feed_shop.request==0xb6);
    CHECK(capture(s,&host,shots,"wrapping-confirm",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && g->feed_shop.request==0xb7);
    CHECK(close_shop(s,error,sizeof(error)) && !memcmp(&bread,&g->game.inventory.held_item,sizeof(bread)) && g->game.economy.gold==900);
    g->game.economy.gold=99;
    CHECK(wrapping(s,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && g->feed_shop.request==0x4f && close_shop(s,error,sizeof(error)));
    CHECK(!memcmp(&bread,&g->game.inventory.held_item,sizeof(bread)) && g->game.economy.gold==99);g->game.economy.gold=900;
    CHECK(wrapping(s,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && g->feed_shop.request==0xbb && g->game.economy.gold==900 && g->game.economy.ledger[0][1]==spent);
    for(unsigned t=0;t<2000 && g->event_item.phase!=HMBTN_EVENT_ITEM_SHOW;++t) CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(g->feed_shop.phase==HMBTN_SHOP_RUCKSACK_CUE && g->event_item.shown==164 && !g->game.inventory.held_item.id && g->intro.player.sprite.animation==14);
    CHECK(g->game.economy.gold==800 && g->game.economy.ledger[0][1]==spent+100);
    CHECK(!hmbtn_save_available(g,error,sizeof(error)));error[0]=0;
    CHECK(capture(s,&host,shots,"wrapping-carry-ani",error,sizeof(error)) && close_shop(s,error,sizeof(error)));
    CHECK(g->game.inventory.held_item.id==164 && g->game.inventory.held_item.modifier==154 && g->game.inventory.held_item.count==1 && g->village.people[1].actor.facing==face);
    CHECK(capture(s,&host,shots,"wrapping-held",error,sizeof(error)));
    CHECK(checkpoint(&s,&host,argv[1],argv[2],".held",error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    aim(s,118,3);CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && g->feed_shop.request==0xbc && close_shop(s,error,sizeof(error)));
    CHECK(g->game.inventory.held_item.id==164 && g->game.inventory.held_item.modifier==154 && g->game.economy.gold==800);
    CHECK(step(s,HMBTN_PAD_R2,error,sizeof(error)) && idle(s,error,sizeof(error)) && !g->game.inventory.held_item.id && g->game.inventory.items[0].id==164 && g->game.inventory.items[0].modifier==154);
    CHECK(step(s,HMBTN_PAD_TRIANGLE,error,sizeof(error)));
    for(unsigned t=0;t<1000 && g->menu.message.phase!=HMBTN_MESSAGE_WAIT_END;++t) CHECK(step(s,0,error,sizeof(error)));
    CHECK(capture(s,&host,shots,"wrapping-bag",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && step(s,HMBTN_PAD_RIGHT,error,sizeof(error)) && g->menu.cursor==10);
    CHECK(step(s,HMBTN_PAD_TRIANGLE,error,sizeof(error)) && g->menu.detail && g->menu.request==1154);
    for(unsigned t=0;t<1000 && g->menu.message.phase!=HMBTN_MESSAGE_WAIT_END;++t) CHECK(step(s,g->menu.message.phase==HMBTN_MESSAGE_WAIT_PAGE ? HMBTN_PAD_CROSS : 0,error,sizeof(error)));
    CHECK(capture(s,&host,shots,"wrapping-bread-description",error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    for(unsigned t=0;t<1000 && (g->menu.detail || g->menu.message.phase!=HMBTN_MESSAGE_WAIT_END);++t) CHECK(step(s,g->menu.detail ? HMBTN_PAD_CROSS : 0,error,sizeof(error)));
    CHECK(!g->menu.detail && step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)));
    for(unsigned t=0;t<100 && g->menu.mode!=HMBTN_MENU_NONE;++t) CHECK(step(s,0,error,sizeof(error)));
    CHECK(checkpoint(&s,&host,argv[1],argv[2],".bag",error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(step(s,HMBTN_PAD_R2,error,sizeof(error)) && idle(s,error,sizeof(error)) && g->game.inventory.held_item.id==164 && g->game.inventory.held_item.modifier==154);
    CHECK(hmbtn_scene_preview_load_area(s,80,error,sizeof(error)));
    for(unsigned t=0;t<110;++t) CHECK(step(s,0,error,sizeof(error)));
    CHECK(reject_storage(s,4,error,sizeof(error)) && reject_storage(s,23,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_load_area(s,212,error,sizeof(error)));
    for(unsigned t=0;t<110;++t) CHECK(step(s,0,error,sizeof(error)));
    HmbtnVillager *gotz=&g->village.people[24];CHECK(gotz->actor.visible && hmbtn_item_villager_reaction(154,24)==1);
    const unsigned friendship=gotz->friendship;CHECK(friendship==50);
    memcpy(g->intro.player.position,gotz->actor.position,sizeof(g->intro.player.position));g->intro.player.position[2]+=300;g->intro.player.facing=0;g->control.mode=0;
    memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));memcpy(g->intro.camera,g->intro.player.position,sizeof(g->intro.camera));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && g->talk.active && g->talk.target==24 && !g->carry.phase);
    CHECK(capture(s,&host,shots,"wrapping-gift-gotz",error,sizeof(error)));
    for(unsigned t=0;t<3000 && g->talk.active;++t) CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    /* Installed event script: first liked gift +3, wrapping -1. */
    CHECK(!g->talk.active && !g->talk.script.boundary && !g->game.inventory.held_item.id && gotz->friendship==friendship+2);
    CHECK(!g->game.inventory.items[0].id && g->game.economy.gold==800 && g->game.economy.ledger[0][1]==spent+100 && !g->carry.discarded);
    for(unsigned t=0;t<60;++t) CHECK(step(s,0,error,sizeof(error)));
    CHECK(gotz->friendship==friendship+2 && !g->game.inventory.held_item.id);
    CHECK(checkpoint(&s,&host,argv[1],argv[2],"",error,sizeof(error)));
    puts("Wrapping: bought bread, original counter/refusals/100G, record ANI, hand/bag, storage refusals, Gotz gift/affinity and resumes passed");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

/* Installed clinic access, original conversation/exam, four purchased medicines,
 * herb shipment thresholds, icebox controls, drink ANI and stable checkpoints.
 * Purse/house/attributes and the first 50 historic herb shipments are fixtures. */
#include "poultry_helpers.h"
#include "field_helpers.h"
#include "farm_shipping.h"
static bool finish_talk(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<3000 && (g->talk.active || g->feed_shop.phase || g->event_item.phase);++i)
        REQUIRE(step(s,g->event_item.phase ? 0 : HMBTN_PAD_CROSS,error,size));
    REQUIRE(!g->talk.active && !g->talk.script.boundary && !g->feed_shop.phase && !g->event_item.phase);return true;
}
static bool doctor(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    if(g->area!=188) {
        REQUIRE(hmbtn_scene_preview_load_area(s,40,error,size));
        for(unsigned i=0;i<110;++i) REQUIRE(step(s,0,error,size));
        REQUIRE(travel(s,188,error,size));
    }
    HmbtnVillager *npc=&g->village.people[8];REQUIRE(npc->actor.visible && npc->location==32);
    memcpy(g->intro.player.position,npc->actor.position,sizeof(npc->actor.position));
    g->intro.player.position[2]+=300;g->intro.player.facing=0;g->control.mode=0;
    memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));memcpy(g->intro.camera,g->intro.player.position,sizeof(g->intro.camera));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->talk.active);
    REQUIRE(shop_wait(s,HMBTN_SHOP_SELECT,error,size));
    REQUIRE(g->feed_shop.clinic && g->talk.target==8 && !g->talk.script.boundary);return true;
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
static bool icebox(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);REQUIRE(hmbtn_scene_preview_load_area(s,80,error,size));
    for(unsigned i=0;i<110;++i) REQUIRE(step(s,0,error,size));
    for(unsigned i=0;i<50;++i) {
        const uint8_t *z=hmbtn_scene_preview_resources(s).section+16+i*44;
        if(hmbtn_read_le16(z)!=0x4000 || hmbtn_read_le16(z+2)!=4) continue;
        at(s,z);const unsigned face=hmbtn_read_le16(z+8);REQUIRE(face<4);
        g->intro.player.position[face&1 ? 0 : 2]+=face<2 ? 224 : -224;
        memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));memcpy(g->intro.camera,g->intro.player.position,sizeof(g->intro.camera));
        REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && house_wait(s,HMBTN_HOUSE_TOOLBOX,error,size) && g->house.storage==2);
        for(unsigned t=0;t<1600 && g->house.detail;++t) REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
        REQUIRE(!g->house.detail && house_wait(s,HMBTN_HOUSE_TOOLBOX,error,size));return true;
    }
    return false;
}
static bool store(HmbtnScenePreview *s,unsigned cell,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const unsigned item=g->game.inventory.held_item.id;
    REQUIRE(icebox(s,error,size) && g->house.cursor==0);
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && step(s,HMBTN_PAD_DOWN,error,size));
    for(unsigned i=0;i<cell;++i) REQUIRE(step(s,HMBTN_PAD_RIGHT,error,size));
    REQUIRE(g->house.cursor==9+cell && step(s,HMBTN_PAD_CROSS,error,size));
    REQUIRE(g->game.inventory.icebox[cell].id==item && !g->game.inventory.held_item.id);
    REQUIRE(step(s,HMBTN_PAD_CIRCLE,error,size) && house_wait(s,HMBTN_HOUSE_FREE,error,size));return true;
}
static bool herb_shipment(HmbtnScenePreview *s,unsigned id,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const int bin=hmbtn_farm_shipping_bin(id);REQUIRE(bin>=0);
    bool found=false;
    const unsigned areas[]={64,68,72};
    for(unsigned a=0;a<3 && !found;++a) {
        REQUIRE(hmbtn_scene_preview_load_area(s,areas[a],error,size));
        for(unsigned t=0;t<110;++t) REQUIRE(step(s,0,error,size));
        const unsigned place=hmbtn_read_le16(hmbtn_scene_preview_resources(s).section+2);
        for(unsigned i=125;i<=172;++i) {
            if(hmbtn_farm_gather_item(&g->gather,i,place,g->game.clock.season)!=id) continue;
            const HmbtnGatherRecord *r=hmbtn_farm_gather_record(i);memcpy(g->intro.player.position,r->position,sizeof(r->position));
            g->intro.player.position[2]+=224;g->intro.player.facing=0;g->control.mode=0;
            memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));
            REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && idle(s,error,size) && g->game.inventory.held_item.id==id);found=true;break;
        }
    }
    REQUIRE(found && g->game.economy.shipments[bin]==50);
    REQUIRE(hmbtn_scene_preview_load_area(s,0,error,size));
    for(unsigned t=0;t<110;++t) REQUIRE(step(s,0,error,size));
    REQUIRE(ship(s,error,size) && g->game.economy.shipments[bin]==51);return true;
}
static bool buy(HmbtnScenePreview *s,HmbtnHost *host,const char *shots,unsigned row,unsigned id,unsigned cost,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);REQUIRE(doctor(s,error,size));
    REQUIRE(step(s,HMBTN_PAD_DOWN,error,size) && step(s,HMBTN_PAD_CROSS,error,size) && shop_wait(s,HMBTN_SHOP_TOOL_LIST,error,size));
    for(unsigned i=0;i<row;++i) REQUIRE(step(s,HMBTN_PAD_DOWN,error,size));
    REQUIRE(hmbtn_clinic_entry(&g->feed_shop,row)->item==id);
    REQUIRE(capture(s,host,shots,"clinic-medicines",error,size));
    const unsigned description=hmbtn_clinic_entry(&g->feed_shop,row)->description;
    REQUIRE(step(s,HMBTN_PAD_TRIANGLE,error,size) && shop_wait(s,HMBTN_SHOP_INFO,error,size) && g->feed_shop.request==description);
    REQUIRE(shop_wait(s,HMBTN_SHOP_TOOL_LIST,error,size));
    const unsigned gold=g->game.economy.gold,spent=g->game.economy.ledger[0][1];const HmbtnFarmAction status=g->farm_action;
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->game.economy.gold==gold-cost && g->game.economy.ledger[0][1]==spent+cost);
    REQUIRE(shop_wait(s,HMBTN_SHOP_RESULT,error,size) && g->feed_shop.request==0xfb && !g->game.inventory.held_item.id);
    REQUIRE(!hmbtn_save_available(g,error,size));error[0]=0;
    for(unsigned t=0;t<3000 && !g->event_item.phase;++t) REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
    REQUIRE(g->event_item.phase==HMBTN_EVENT_ITEM_SHOW && g->event_item.shown==id);
    char name[64];snprintf(name,sizeof(name),"clinic-receipt-%u",id);REQUIRE(capture(s,host,shots,name,error,size));
    REQUIRE(finish_talk(s,error,size));
    REQUIRE(g->game.inventory.held_item.id==id && g->game.inventory.held_item.count==1 && !g->game.inventory.held_item.modifier);
    REQUIRE(g->game.economy.gold==gold-cost && g->game.economy.ledger[0][1]==spent+cost && !memcmp(&g->farm_action,&status,sizeof(status)));return true;
}
static bool checkpoint(HmbtnScenePreview **s,HmbtnHost *host,const char *root,const char *base,const char *suffix,char *error,size_t size)
{ char path[1024];if(snprintf(path,sizeof(path),"%s%s",base,suffix)>=(int)sizeof(path)) return false;return field_reload(s,host,root,path,error,size); }
int main(int argc,char **argv)
{
    if(argc<3 || argc>4) return 2;const char *shots=argc==4 ? argv[3] : NULL;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock clock={0,0,3,2,12,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],40,&clock,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->game.economy.gold=6000;g->construction.house_level=1;
    g->poultry.upgraded=true;g->farm_action.stamina=6;g->farm_action.fatigue=70;
    g->village.flags[251+288]=g->village.flags[251+140]=true;
    CHECK(doctor(s,error,sizeof(error)) && hmbtn_clinic_rows(&g->feed_shop)==2);
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_MAKER_CONFIRM,error,sizeof(error)));
    CHECK(capture(s,&host,shots,"clinic-exam-question",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)) && g->game.economy.gold==6000);
    const HmbtnFarmClock exam_clock=g->game.clock;const HmbtnInventory inventory=g->game.inventory;
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_MAKER_CONFIRM,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(g->game.economy.gold==5990 && shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && g->feed_shop.request==0xe7);
    for(unsigned t=0;t<3000 && g->feed_shop.phase!=HMBTN_SHOP_CLINIC_FADE_OUT;++t) CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(g->feed_shop.phase==HMBTN_SHOP_CLINIC_FADE_OUT);
    for(unsigned t=0;t<10;++t) CHECK(step(s,0,error,sizeof(error)));
    CHECK(g->feed_shop.clinic_shade>0 && g->feed_shop.clinic_shade<255 && capture(s,&host,shots,"clinic-exam-fade",error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_INFO,error,sizeof(error)) && g->feed_shop.request==0xe9 && g->feed_shop.clinic_shade==255);
    CHECK(capture(s,&host,shots,"clinic-exam-dark-message",error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_LEAVE,error,sizeof(error)) && g->feed_shop.request==0x1db && !g->feed_shop.clinic_shade);
    CHECK(capture(s,&host,shots,"clinic-diagnosis",error,sizeof(error)) && finish_talk(s,error,sizeof(error)));
    CHECK(g->game.economy.gold==5990 && g->farm_action.stamina==6 && g->farm_action.fatigue==70 && !memcmp(&inventory,&g->game.inventory,sizeof(inventory)) && !memcmp(&exam_clock,&g->game.clock,sizeof(exam_clock)));
    CHECK(buy(s,&host,shots,0,155,500,error,sizeof(error)) && store(s,0,error,sizeof(error)));
    CHECK(buy(s,&host,shots,1,157,1000,error,sizeof(error)) && store(s,1,error,sizeof(error)));
    /* Last threshold crossing is a real collected/shipped herb, not a shop flag. */
    g->game.economy.shipments[hmbtn_farm_shipping_bin(93)]=50;
    CHECK(herb_shipment(s,93,error,sizeof(error)) && buy(s,&host,shots,2,156,1000,error,sizeof(error)) && store(s,2,error,sizeof(error)));
    hmbtn_session_advance_minutes(g,60u*1440u);if(g->game.clock.weekday==3) hmbtn_session_advance_minutes(g,1440);
    g->game.economy.shipments[hmbtn_farm_shipping_bin(95)]=50;
    CHECK(herb_shipment(s,95,error,sizeof(error)) && buy(s,&host,shots,3,158,2000,error,sizeof(error)) && store(s,3,error,sizeof(error)));
    CHECK(checkpoint(&s,&host,argv[1],argv[2],".bought",error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    const unsigned ids[]={155,157,156,158},gains[]={50,0,100,0},loss[]={1,20,1,50};
    for(unsigned i=0;i<4;++i) {
        CHECK(icebox(s,error,sizeof(error)));
        CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)));
        for(unsigned c=0;c<i;++c) CHECK(step(s,HMBTN_PAD_RIGHT,error,sizeof(error)));
        CHECK(g->house.cursor==9+i && step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
        CHECK(g->game.inventory.held_item.id==ids[i] && !g->game.inventory.icebox[i].id && step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && house_wait(s,HMBTN_HOUSE_FREE,error,sizeof(error)));
        if(!i) { CHECK(checkpoint(&s,&host,argv[1],argv[2],".held",error,sizeof(error)));g=hmbtn_scene_preview_session(s); }
        const unsigned stamina=g->farm_action.stamina,fatigue=g->farm_action.fatigue,gold=g->game.economy.gold;
        CHECK(step(s,HMBTN_PAD_SQUARE,error,sizeof(error)) && g->event_item.phase==HMBTN_EVENT_ITEM_USE_DRINK && g->intro.action_active);
        CHECK(capture(s,&host,shots,"clinic-drink",error,sizeof(error)));
        for(unsigned t=0;t<800 && g->event_item.phase;++t) {
            CHECK(g->farm_action.stamina==stamina && g->farm_action.fatigue==fatigue && g->game.inventory.held_item.id==ids[i]);CHECK(step(s,0,error,sizeof(error)));
        }
        const unsigned expected=stamina+gains[i]>g->farm_action.max_stamina ? g->farm_action.max_stamina : stamina+gains[i];
        CHECK(!g->event_item.phase && !g->intro.action_active && !g->game.inventory.held_item.id && g->farm_action.stamina==expected && g->farm_action.fatigue==(fatigue>loss[i] ? fatigue-loss[i] : 0) && g->game.economy.gold==gold);
    }
    CHECK(g->farm_action.stamina==100 && !g->farm_action.fatigue);
    CHECK(checkpoint(&s,&host,argv[1],argv[2],"",error,sizeof(error)));
    puts("Clinic: original door/conversation, exam refusal/fade/diagnosis, four purchases, 51st herb shipments, icebox, drink ANI and resume passed");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

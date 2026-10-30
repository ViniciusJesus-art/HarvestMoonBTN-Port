/* Starts from the groceries producer: all rice/bread/curry ingredients bought,
 * curry cooked. Mixer/status values are explicit fixtures; no food is granted. */
#include "poultry_helpers.h"
#include "field_helpers.h"
#include "cooking.h"
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
{ REQUIRE(house_wait(s,HMBTN_HOUSE_COOKING,error,size));REQUIRE(hmbtn_scene_preview_session(s)->house.kitchen.phase==phase);return true; }
static bool object(HmbtnScenePreview *s,unsigned id,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<50;++i) {
        const uint8_t *z=hmbtn_scene_preview_resources(s).section+16+i*44;
        if(hmbtn_read_le16(z)!=0x4000 || hmbtn_read_le16(z+2)!=id) continue;
        at(s,z);const unsigned face=hmbtn_read_le16(z+8);REQUIRE(face<4);
        g->intro.player.position[face&1 ? 0 : 2]+=face<2 ? 224 : -224;
        memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));memcpy(g->intro.camera,g->intro.player.position,sizeof(g->intro.camera));
        return step(s,HMBTN_PAD_CROSS,error,size);
    }
    return false;
}
static bool close_shop(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<3000 && (g->feed_shop.phase!=HMBTN_SHOP_OFF || g->event_item.phase);++i)
        REQUIRE(step(s,g->event_item.phase ? 0 : HMBTN_PAD_CROSS,error,size));
    REQUIRE(g->feed_shop.phase==HMBTN_SHOP_OFF && !g->event_item.phase);return true;
}
static bool consume(HmbtnScenePreview *s,HmbtnHost *host,const char *shots,int stamina,int fatigue,bool drink,bool clock_runs,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const HmbtnInventory inventory=g->game.inventory;
    const unsigned before=g->farm_action.stamina,tired=g->farm_action.fatigue,gold=g->game.economy.gold;
    const HmbtnFarmClock clock=g->game.clock;int32_t position[3];memcpy(position,g->intro.player.position,sizeof(position));
    REQUIRE(step(s,HMBTN_PAD_SQUARE,error,size) && g->event_item.phase==(drink ? HMBTN_EVENT_ITEM_USE_DRINK : HMBTN_EVENT_ITEM_USE_FOOD));
    REQUIRE(g->event_item.sound==(drink ? 75 : 74) && g->intro.player.sprite.animation==(drink ? 0 : 5) && g->intro.action_active==drink);
    REQUIRE(!memcmp(&inventory,&g->game.inventory,sizeof(inventory)) && g->farm_action.stamina==before && g->farm_action.fatigue==tired);
    REQUIRE(!hmbtn_save_available(g,error,size));error[0]=0;
    REQUIRE(capture(s,host,shots,drink ? "food-use-drink-ani" : "food-use-eat-ani",error,size));
    REQUIRE(step(s,HMBTN_PAD_R2|HMBTN_PAD_TRIANGLE|HMBTN_PAD_START,error,size) && g->event_item.sound==-1 && g->menu.mode==HMBTN_MENU_NONE);
    bool concealed=drink;
    for(unsigned i=0;i<800 && hmbtn_event_item_consuming(&g->event_item);++i) {
        REQUIRE(g->farm_action.stamina==before && g->farm_action.fatigue==tired && g->game.inventory.held_item.id==inventory.held_item.id);
        if(!drink && g->intro.player.sprite.frame>=2 && !concealed) {
            REQUIRE(capture(s,host,shots,"food-use-eat-hidden",error,size));concealed=true;
        }
        REQUIRE(step(s,0,error,size));
    }
    const int sum=(int)before+stamina,loss=(int)tired+fatigue;
    const unsigned expected=sum<0 ? 0 : sum>(int)g->farm_action.max_stamina ? g->farm_action.max_stamina : (unsigned)sum;
    const unsigned expected_fatigue=loss<0 ? 0 : loss>100 ? 100 : (unsigned)loss;
    REQUIRE(concealed && g->event_item.phase==HMBTN_EVENT_ITEM_IDLE && !g->game.inventory.held_item.id && !g->intro.action_active);
    REQUIRE(g->farm_action.stamina==expected && g->farm_action.fatigue==expected_fatigue && g->game.economy.gold==gold);
    REQUIRE(!memcmp(g->game.inventory.items,inventory.items,sizeof(inventory.items)) && !memcmp(g->game.inventory.icebox,inventory.icebox,sizeof(inventory.icebox)));
    REQUIRE(!memcmp(position,g->intro.player.position,sizeof(position)));
    REQUIRE(clock_runs ? memcmp(&clock,&g->game.clock,sizeof(clock))!=0 : !memcmp(&clock,&g->game.clock,sizeof(clock)));
    for(unsigned i=0;i<5;++i) REQUIRE(step(s,0,error,size));
    REQUIRE(g->farm_action.stamina==expected && g->farm_action.fatigue==expected_fatigue && !g->game.inventory.held_item.id);return true;
}
static bool gather(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const unsigned place=hmbtn_read_le16(hmbtn_scene_preview_resources(s).section+2);
    for(unsigned i=125;i<=172;++i) {
        if(hmbtn_farm_gather_item(&g->gather,i,place,g->game.clock.season)!=89) continue;
        const HmbtnGatherRecord *r=hmbtn_farm_gather_record(i);memcpy(g->intro.player.position,r->position,sizeof(r->position));
        g->intro.player.position[2]+=224;g->intro.player.facing=0;g->control.mode=0;
        memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));memcpy(g->intro.camera,g->intro.player.position,sizeof(g->intro.camera));
        REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->game.inventory.held_item.id==89 && idle(s,error,size));return true;
    }
    return false;
}
static bool checkpoint(HmbtnScenePreview **s,HmbtnHost *host,const char *root,const char *base,const char *suffix,char *error,size_t size)
{ char path[1024];if(snprintf(path,sizeof(path),"%s%s",base,suffix)>=(int)sizeof(path)) return false;return field_reload(s,host,root,path,error,size); }
int main(int argc,char **argv)
{
    if(argc<4 || argc>5) return 2;const char *shots=argc==5 ? argv[4] : NULL;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};CHECK(hmbtn_host_open(&host));
    s=hmbtn_scene_saved_create(host.renderer,argv[1],argv[2],error,sizeof(error));CHECK(s);
    HmbtnSession *g=hmbtn_scene_preview_session(s);CHECK(g->game.inventory.held_item.id==178 && g->game.inventory.held_item.modifier==30 && g->game.economy.gold==550);
    g->farm_action.stamina=50;g->farm_action.fatigue=20;
    CHECK(consume(s,&host,shots,30,-3,false,false,error,sizeof(error)));
    CHECK(object(s,4,error,sizeof(error)) && house_wait(s,HMBTN_HOUSE_TOOLBOX,error,sizeof(error)));
    if(g->house.detail) CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && house_wait(s,HMBTN_HOUSE_TOOLBOX,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && step(s,HMBTN_PAD_RIGHT,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(g->game.inventory.held_item.id==154 && g->game.inventory.held_item.count==1 && g->game.inventory.icebox[1].count==1);
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && house_wait(s,HMBTN_HOUSE_FREE,error,sizeof(error)));
    CHECK(consume(s,&host,shots,2,-1,false,false,error,sizeof(error)));
    CHECK(checkpoint(&s,&host,argv[1],argv[3],".eaten",error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(hmbtn_scene_preview_load_area(s,180,error,sizeof(error)));
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    const HmbtnGatherRecord *rice=hmbtn_farm_gather_record(123);memcpy(g->intro.player.position,rice->position,sizeof(rice->position));
    g->intro.player.position[0]+=400;g->intro.player.position[1]=0;g->intro.player.facing=1;g->control.mode=0;
    memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_OFFER,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && close_shop(s,error,sizeof(error)));
    CHECK(g->game.inventory.held_item.id==153 && g->game.economy.gold==450 && consume(s,&host,shots,2,-1,false,false,error,sizeof(error)));
    /* Advance the shared calendar to autumn, when the original mountain
     * records publish wild grapes. No ingredient is placed by the fixture. */
    hmbtn_session_advance_minutes(g,60u*1440u);
    CHECK(g->game.clock.season==2 && hmbtn_scene_preview_load_area(s,68,error,sizeof(error)));
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(gather(s,error,sizeof(error)) && consume(s,&host,shots,10,-2,false,true,error,sizeof(error)));
    CHECK(gather(s,error,sizeof(error)) && step(s,HMBTN_PAD_R2,error,sizeof(error)) && idle(s,error,sizeof(error)));
    CHECK(g->game.inventory.items[0].id==89 && !g->game.inventory.held_item.id);
    CHECK(hmbtn_scene_preview_load_area(s,80,error,sizeof(error)));
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    /* Original fruit-juice table includes wild grape 89; mixer ownership is a fixture,
     * while the ingredient was collected with the real forward probe. */
    g->game.cooking.utensils|=8;
    CHECK(object(s,5,error,sizeof(error)) && ready(s,HMBTN_KITCHEN_OPEN,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && ready(s,HMBTN_KITCHEN_FIRST,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && ready(s,HMBTN_KITCHEN_SECOND,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && ready(s,HMBTN_KITCHEN_TOOLS,error,sizeof(error)));
    for(unsigned i=0;i<3;++i) CHECK(step(s,HMBTN_PAD_RIGHT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && g->house.kitchen.options==8 && step(s,HMBTN_PAD_START,error,sizeof(error)) && ready(s,HMBTN_KITCHEN_ITEMS,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && step(s,HMBTN_PAD_START,error,sizeof(error)) && ready(s,HMBTN_KITCHEN_CONFIRM,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    for(unsigned i=0;i<2000 && g->house.kitchen.phase!=HMBTN_KITCHEN_RESULT;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(ready(s,HMBTN_KITCHEN_RESULT,error,sizeof(error)) && g->game.inventory.held_item.id==180 && g->game.inventory.held_item.modifier==20);
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && house_wait(s,HMBTN_HOUSE_FREE,error,sizeof(error)));
    CHECK(checkpoint(&s,&host,argv[1],argv[3],".drink",error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(consume(s,&host,shots,20,-4,true,false,error,sizeof(error)));
    CHECK(g->farm_action.stamina==100 && g->farm_action.fatigue==9 && g->game.economy.gold==450 && g->game.inventory.icebox[1].count==1);
    CHECK(checkpoint(&s,&host,argv[1],argv[3],"",error,sizeof(error)));
    puts("Food use: bought curry/bread/rice, gathered food, real juice recipe/drink bank, deferred unique effects, controls/clock/hidden items and resume passed");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

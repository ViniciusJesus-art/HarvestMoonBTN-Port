/* Original door/conversation -> wine/grape juice purchases -> bag/icebox,
 * direct drinking and an actual juice recipe with the purchased ingredient.
 * Purse, first house, mixer and initial attributes are explicit fixtures. */
#include "poultry_helpers.h"
#include "field_helpers.h"
#include "cooking.h"
static bool finish_talk(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<3000 && (g->talk.active || g->feed_shop.phase || g->event_item.phase);++i)
        REQUIRE(step(s,g->event_item.phase ? 0 : HMBTN_PAD_CROSS,error,size));
    REQUIRE(!g->talk.active && !g->talk.script.boundary && !g->feed_shop.phase && !g->event_item.phase);return true;
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
static bool manna(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    if(g->area!=144) {
        REQUIRE(hmbtn_scene_preview_load_area(s,32,error,size));
        for(unsigned i=0;i<110;++i) REQUIRE(step(s,0,error,size));
        REQUIRE(travel(s,144,error,size));
    }
    HmbtnVillager *npc=&g->village.people[22];
    if(!npc->actor.visible || npc->location!=36) { snprintf(error,size,"Manna location %u visible %u",npc->location,npc->actor.visible);return false; }
    memcpy(g->intro.player.position,npc->actor.position,sizeof(npc->actor.position));g->intro.player.position[2]+=300;
    g->intro.player.facing=0;g->control.mode=0;memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));
    memcpy(g->intro.camera,g->intro.player.position,sizeof(g->intro.camera));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->talk.active);
    REQUIRE(shop_wait(s,HMBTN_SHOP_SELECT,error,size) && g->feed_shop.wine && g->talk.target==22 && !g->talk.script.boundary);return true;
}
static bool buy(HmbtnScenePreview *s,HmbtnHost *host,const char *shots,unsigned row,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);REQUIRE(manna(s,error,size));
    if(row) REQUIRE(step(s,HMBTN_PAD_DOWN,error,size));
    const HmbtnSeedShopEntry *entry=hmbtn_wine_shop_entry(row);const unsigned id=entry->item,cost=(unsigned)hmbtn_farm_item(id)->buy_price;
    REQUIRE(g->feed_shop.item==id && capture(s,host,shots,"winery-menu",error,size));
    REQUIRE(step(s,HMBTN_PAD_TRIANGLE,error,size) && shop_wait(s,HMBTN_SHOP_INFO,error,size) && g->feed_shop.request==entry->description);
    REQUIRE(capture(s,host,shots,"winery-info",error,size) && shop_wait(s,HMBTN_SHOP_SELECT,error,size));
    const unsigned gold=g->game.economy.gold,spent=g->game.economy.ledger[0][1];const HmbtnFarmAction status=g->farm_action;
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->game.economy.gold==gold-cost && g->game.economy.ledger[0][1]==spent+cost);
    REQUIRE(shop_wait(s,HMBTN_SHOP_RESULT,error,size) && g->feed_shop.request==0xb4 && !g->game.inventory.held_item.id);
    REQUIRE(!hmbtn_save_available(g,error,size));error[0]=0;
    for(unsigned t=0;t<3000 && !g->event_item.phase;++t) REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
    REQUIRE(g->event_item.phase==HMBTN_EVENT_ITEM_SHOW && g->event_item.shown==id);
    for(unsigned t=0;t<5;++t) REQUIRE(step(s,0,error,size));
    char name[64];snprintf(name,sizeof(name),"winery-receipt-%u",id);REQUIRE(capture(s,host,shots,name,error,size) && finish_talk(s,error,size));
    REQUIRE(g->game.inventory.held_item.id==id && g->game.inventory.held_item.count==1 && !g->game.inventory.held_item.modifier);
    REQUIRE(g->game.economy.gold==gold-cost && g->game.economy.ledger[0][1]==spent+cost && !memcmp(&status,&g->farm_action,sizeof(status)));return true;
}
int main(int argc,char **argv)
{
    if(argc<3 || argc>4) return 2;const char *shots=argc==4 ? argv[3] : NULL;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock clock={0,0,3,2,10,0,0};s=hmbtn_scene_area_create_at(host.renderer,argv[1],32,&clock,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->game.economy.gold=1000;g->construction.house_level=1;g->poultry.upgraded=true;
    g->game.cooking.utensils=8;g->farm_action.stamina=50;g->farm_action.fatigue=20;g->farm_action.happiness=10;
    g->village.flags[251+288]=g->village.flags[251+140]=true;
    CHECK(manna(s,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && finish_talk(s,error,sizeof(error)) && g->game.economy.gold==1000 && !g->game.inventory.held_item.id);
    CHECK(manna(s,error,sizeof(error)));g->game.economy.gold=299;
    const unsigned spent=g->game.economy.ledger[0][1];
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && g->feed_shop.request==0x4f);
    CHECK(capture(s,&host,shots,"winery-poor",error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_GREETING,error,sizeof(error)) && g->feed_shop.request==0xb5 && shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(g->game.economy.gold==299 && g->game.economy.ledger[0][1]==spent && !g->game.inventory.held_item.id);
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && finish_talk(s,error,sizeof(error)));g->game.economy.gold=1000;
    CHECK(buy(s,&host,shots,0,error,sizeof(error)) && store(s,0,error,sizeof(error)));
    CHECK(buy(s,&host,shots,1,error,sizeof(error)) && store(s,1,error,sizeof(error)));
    CHECK(checkpoint(&s,&host,argv[1],argv[2],".bought",error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(g->game.inventory.icebox[0].id==159 && g->game.inventory.icebox[1].id==160 && g->game.economy.gold==500);
    /* Original stock repeats; this third bottle is bought, not granted. */
    CHECK(buy(s,&host,shots,1,error,sizeof(error)) && g->game.economy.gold==300);
    CHECK(consume(s,&host,shots,5,-2,true,false,error,sizeof(error)) && g->farm_action.happiness==10);
    CHECK(icebox(s,error,sizeof(error)) && step(s,HMBTN_PAD_DOWN,error,sizeof(error)));
    CHECK(g->house.cursor==9 && step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(g->game.inventory.held_item.id==159 && step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && house_wait(s,HMBTN_HOUSE_FREE,error,sizeof(error)));
    CHECK(checkpoint(&s,&host,argv[1],argv[2],".held",error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(consume(s,&host,shots,0,-5,true,false,error,sizeof(error)) && g->farm_action.happiness==11);
    /* Collected autumn grapes and purchased grape juice contribute separately:
     * the original recipe is fruit juice 180, base 20 + grape-juice bonus 3. */
    hmbtn_session_advance_minutes(g,60u*1440u);CHECK(g->game.clock.season==2 && hmbtn_scene_preview_load_area(s,68,error,sizeof(error)));
    for(unsigned t=0;t<110;++t) CHECK(step(s,0,error,sizeof(error)));
    CHECK(gather(s,error,sizeof(error)) && step(s,HMBTN_PAD_R2,error,sizeof(error)) && idle(s,error,sizeof(error)) && g->game.inventory.items[0].id==89);
    CHECK(hmbtn_scene_preview_load_area(s,80,error,sizeof(error)));
    for(unsigned t=0;t<110;++t) CHECK(step(s,0,error,sizeof(error)));
    /* Locate the existing kitchen zone rather than invoking the recipe directly. */
    const uint8_t *kitchen=NULL;
    for(unsigned i=0;i<50;++i) {
        const uint8_t *z=hmbtn_scene_preview_resources(s).section+16+i*44;
        if(hmbtn_read_le16(z)==0x4000 && hmbtn_read_le16(z+2)==5) { kitchen=z;break; }
    }
    CHECK(kitchen);at(s,kitchen);unsigned face=hmbtn_read_le16(kitchen+8);CHECK(face<4);
    g->intro.player.position[face&1 ? 0 : 2]+=face<2 ? 224 : -224;memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));
    memcpy(g->intro.camera,g->intro.player.position,sizeof(g->intro.camera));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && house_wait(s,HMBTN_HOUSE_COOKING,error,sizeof(error)));
    CHECK(g->house.kitchen.phase==HMBTN_KITCHEN_OPEN);
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && house_wait(s,HMBTN_HOUSE_COOKING,error,sizeof(error)) && g->house.kitchen.phase==HMBTN_KITCHEN_FIRST);
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && house_wait(s,HMBTN_HOUSE_COOKING,error,sizeof(error)) && g->house.kitchen.phase==HMBTN_KITCHEN_SECOND);
    CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && house_wait(s,HMBTN_HOUSE_COOKING,error,sizeof(error)) && g->house.kitchen.phase==HMBTN_KITCHEN_TOOLS);
    for(unsigned t=0;t<3;++t) CHECK(step(s,HMBTN_PAD_RIGHT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && g->house.kitchen.options==8 && step(s,HMBTN_PAD_START,error,sizeof(error)) && house_wait(s,HMBTN_HOUSE_COOKING,error,sizeof(error)));
    CHECK(g->house.kitchen.phase==HMBTN_KITCHEN_ITEMS && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_R1,error,sizeof(error)) && step(s,HMBTN_PAD_RIGHT,error,sizeof(error)) && g->house.kitchen.page==1 && g->house.kitchen.cursor==1);
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && g->house.kitchen.values[0]==89 && g->house.kitchen.values[1]==160);
    CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)) && house_wait(s,HMBTN_HOUSE_COOKING,error,sizeof(error)) && g->house.kitchen.phase==HMBTN_KITCHEN_CONFIRM && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    for(unsigned t=0;t<2000 && g->house.kitchen.phase!=HMBTN_KITCHEN_RESULT;++t) CHECK(step(s,0,error,sizeof(error)));
    CHECK(house_wait(s,HMBTN_HOUSE_COOKING,error,sizeof(error)) && g->house.kitchen.phase==HMBTN_KITCHEN_RESULT);
    CHECK(g->game.inventory.held_item.id==180 && g->game.inventory.held_item.modifier==23 && !g->game.inventory.icebox[1].id && !g->game.inventory.items[0].id);
    CHECK(capture(s,&host,shots,"winery-juice-recipe",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && house_wait(s,HMBTN_HOUSE_FREE,error,sizeof(error)));
    CHECK(consume(s,&host,shots,23,-4,true,false,error,sizeof(error)));
    CHECK(g->game.economy.gold==300 && g->farm_action.stamina==78 && g->farm_action.fatigue==9 && g->farm_action.happiness==11);
    CHECK(checkpoint(&s,&host,argv[1],argv[2],"",error,sizeof(error)));
    puts("Winery: original door/Manna script, cancel/poor/retry, three purchases, icebox and wine/juice drinking, acquired recipe bonus and resume passed");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

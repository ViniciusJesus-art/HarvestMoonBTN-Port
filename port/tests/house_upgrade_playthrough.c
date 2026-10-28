/* Real Gotz dialogue/order, four nights, upgraded map, furniture/storage,
 * cooking/recipes and current-version saves. Mode 2 covers the second expansion;
 * explicit prior works, ingredients and existing animals are fixture setup. */
#include "poultry_helpers.h"
#include "field_helpers.h"
static bool house_wait(HmbtnScenePreview *s,HmbtnHouseMode mode,unsigned message,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<1500;++i) {
        HmbtnHouseInteraction *h=&g->house;
        if(h->mode==mode && (mode==HMBTN_HOUSE_FREE || !h->closing) && (mode==HMBTN_HOUSE_FREE
                || (h->message.phase==HMBTN_MESSAGE_WAIT_END && (!message || h->request==message)))) return true;
        REQUIRE(step(s,h->message.phase==HMBTN_MESSAGE_WAIT_PAGE ? HMBTN_PAD_CROSS : 0,error,size));
    }
    fprintf(stderr,"House wait mode=%u phase=%u req=%x expected=%u/%x\n",g->house.mode,g->house.message.phase,g->house.request,mode,message);return false;
}
static bool object(HmbtnScenePreview *s,unsigned id,char *error,size_t size)
{
    const uint8_t *z=NULL;for(unsigned i=0;i<50;++i) {
        const uint8_t *zone=hmbtn_scene_preview_resources(s).section+16+i*44;
        if(hmbtn_read_le16(zone)==0x4000 && hmbtn_read_le16(zone+2)==id) { z=zone;break; }
    }
    REQUIRE(z);at(s,z);HmbtnSession *g=hmbtn_scene_preview_session(s);
    const unsigned face=hmbtn_read_le16(z+8);REQUIRE(face<4);g->intro.player.facing=face;
    g->intro.player.position[face&1 ? 0 : 2]+=face<2 ? 224 : -224;
    return step(s,HMBTN_PAD_CROSS,error,size);
}
static bool kitchen_wait(HmbtnScenePreview *s,HmbtnKitchenPhase phase,char *error,size_t size)
{
    REQUIRE(house_wait(s,HMBTN_HOUSE_COOKING,0,error,size));
    REQUIRE(hmbtn_scene_preview_session(s)->house.kitchen.phase==phase);return true;
}
static bool kitchen_entry(HmbtnScenePreview *s,char *error,size_t size)
{
    REQUIRE(object(s,5,error,size));
    REQUIRE(kitchen_wait(s,HMBTN_KITCHEN_OPEN,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
    for(unsigned i=0;i<20 && hmbtn_scene_preview_session(s)->house.kitchen.phase==HMBTN_KITCHEN_OPEN;++i) REQUIRE(step(s,0,error,size));
    REQUIRE(kitchen_wait(s,HMBTN_KITCHEN_FIRST,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && kitchen_wait(s,HMBTN_KITCHEN_SECOND,error,size));
    REQUIRE(hmbtn_scene_preview_session(s)->house.message.choices==3);
    return true;
}
static bool kitchen_new(HmbtnScenePreview *s,char *error,size_t size)
{
    REQUIRE(kitchen_entry(s,error,size));
    REQUIRE(step(s,HMBTN_PAD_DOWN,error,size) && step(s,HMBTN_PAD_CROSS,error,size));
    return kitchen_wait(s,HMBTN_KITCHEN_TOOLS,error,size);
}
static bool deposit(HmbtnScenePreview *s,unsigned column,char *error,size_t size)
{
    REQUIRE(column<4 && object(s,4,error,size) && house_wait(s,HMBTN_HOUSE_TOOLBOX,0x3ae,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && step(s,HMBTN_PAD_DOWN,error,size));
    for(unsigned i=0;i<column;++i) REQUIRE(step(s,HMBTN_PAD_RIGHT,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && !hmbtn_scene_preview_session(s)->game.inventory.held_item.id);
    REQUIRE(step(s,HMBTN_PAD_CIRCLE,error,size) && house_wait(s,HMBTN_HOUSE_FREE,0,error,size));return true;
}
static bool cooked(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    REQUIRE(kitchen_wait(s,HMBTN_KITCHEN_RESULT,error,size));
    for(unsigned i=0;i<200 && g->house.mode!=HMBTN_HOUSE_FREE;++i) REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
    REQUIRE(g->house.mode==HMBTN_HOUSE_FREE && !g->carry.phase);return true;
}
static bool capture_stable(HmbtnScenePreview *s,HmbtnHost *host,const char *shots,const char *name,char *error,size_t size)
{
    const HmbtnGameState game=hmbtn_scene_preview_session(s)->game;
    const HmbtnConstruction work=hmbtn_scene_preview_session(s)->construction;
    REQUIRE(capture(s,host,shots,name,error,size));
    REQUIRE(!memcmp(&game,&hmbtn_scene_preview_session(s)->game,sizeof(game))
        && !memcmp(&work,&hmbtn_scene_preview_session(s)->construction,sizeof(work)));return true;
}
int main(int argc,char **argv)
{
    if(argc<4 || argc>5) return 2;const char *shots=argc==5 ? argv[4] : NULL;
    const unsigned level=!strcmp(argv[2],"2") ? 2u : 1u,stage=level==2 ? 3u : 1u;
    const unsigned farm=level==2 ? 16u : 8u,interior=80+level*4;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock clock={0,0,3,3,12,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],212,&clock,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    g->poultry.upgraded=true;g->game.economy.gold=6000;g->game.inventory.supplies[1]=400;
    if(level==2) {
        g->construction.house_level=g->construction.barn_level=1;
        g->game.economy.gold=11300;g->game.inventory.supplies[1]=780;
        const uint16_t name[9]={0x100,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff};
        CHECK(hmbtn_livestock_buy(&g->livestock,0x161,name,&g->game.clock)
            && hmbtn_livestock_buy(&g->livestock,0x165,name,&g->game.clock));
        g->livestock.animals[0].affection=g->livestock.animals[1].affection=200;
        g->game.inventory.icebox[63]=(HmbtnStoredTool){0x43,7,23};
    }
    g->game.inventory.toolbox[63]=(HmbtnStoredTool){0x24,0,7};
    g->game.inventory.items[0]=(HmbtnStoredTool){0x9a,0,1};g->game.inventory.items[1]=(HmbtnStoredTool){0x59,0,1};
    g->field.cells[460]=(HmbtnFarmTile){.value=0x24,.ground=0x168,.kind=1};
    g->village.flags[251+288]=g->village.flags[251+140]=true;
    HmbtnVillager *p=&g->village.people[24];CHECK(p->actor.visible && p->schedule[p->step][1]>>8==6);
    memcpy(g->intro.player.position,p->actor.position,sizeof(g->intro.player.position));g->intro.player.position[2]+=300;g->intro.player.facing=0;
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && capture_stable(s,&host,shots,"gotz-house-menu",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_TRIANGLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_INFO,error,sizeof(error)) && g->feed_shop.request==0x134+stage);
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_INFO,error,sizeof(error)) && g->feed_shop.request==0x13c+stage);
    CHECK(capture_stable(s,&host,shots,"gotz-house-offer",error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_BUILD_CONFIRM,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_OFF,error,sizeof(error)));
    for(unsigned i=0;i<1000 && g->talk.active;++i) CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(!g->talk.active && !g->talk.script.boundary && g->construction.pending==(level==2 ? HMBTN_BUILD_HOUSE2 : HMBTN_BUILD_HOUSE)
        && g->construction.mornings_left==4 && g->game.economy.gold==1300 && g->game.inventory.supplies[1]==30);
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)));
    for(unsigned day=0;day<4;++day) {
        if(level==2) g->feed.barn[0]=g->feed.barn[1]=true;
        g->game.weather.tomorrow=HMBTN_WEATHER_SUNNY;CHECK(sleep_day(s,error,sizeof(error)));
        CHECK(g->construction.mornings_left==3-day && g->construction.house_level==(day==3 ? level : level-1));
        if(day==1) { CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s); }
    }
    CHECK(hmbtn_read_le16(hmbtn_scene_preview_resources(s).section+4)==farm && hmbtn_farm_menu_building_score(&g->menu)==(level==2 ? 80u : 40u));
    CHECK(capture_stable(s,&host,shots,"house-expanded-farm",error,sizeof(error)));
    CHECK(travel(s,80,error,sizeof(error)) && hmbtn_read_le16(hmbtn_scene_preview_resources(s).section+4)==interior);
    CHECK(g->field.cells[460].value==0x24 && g->field.cells[460].kind==1 && g->game.inventory.toolbox[63].count==7);
    CHECK(capture_stable(s,&host,shots,"house-expanded-interior",error,sizeof(error)));
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(hmbtn_read_le16(hmbtn_scene_preview_resources(s).section+4)==interior);
    /* New furniture uses the same modal, inventory and original zone priorities. */
    CHECK(object(s,3,error,sizeof(error)) && house_wait(s,HMBTN_HOUSE_TV_SELECT,0,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_UP,error,sizeof(error)) && house_wait(s,HMBTN_HOUSE_TV,0,error,sizeof(error)));
    CHECK(capture_stable(s,&host,shots,"house-expanded-tv",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && house_wait(s,HMBTN_HOUSE_FREE,0,error,sizeof(error)));
    CHECK(object(s,8,error,sizeof(error)) && house_wait(s,HMBTN_HOUSE_CALENDAR,0,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && house_wait(s,HMBTN_HOUSE_FREE,0,error,sizeof(error)));
    CHECK(object(s,22,error,sizeof(error)) && house_wait(s,HMBTN_HOUSE_TOOLBOX,0x3ad,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && house_wait(s,HMBTN_HOUSE_FREE,0,error,sizeof(error)));
    g->game.inventory.held_item=(HmbtnStoredTool){0x7b,0,1};const unsigned discarded=g->carry.discarded;
    CHECK(object(s,23,error,sizeof(error)) && house_wait(s,HMBTN_HOUSE_TOOLBOX,0x3af,error,sizeof(error)));
    CHECK(g->house.storage==1 && !g->carry.phase && g->carry.discarded==discarded);
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(g->game.inventory.cabinet[0].id==0x7b && !g->game.inventory.held_item.id);
    CHECK(capture_stable(s,&host,shots,"house-cabinet",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && house_wait(s,HMBTN_HOUSE_FREE,0,error,sizeof(error)));
    CHECK(object(s,4,error,sizeof(error)) && house_wait(s,HMBTN_HOUSE_TOOLBOX,0x3ae,error,sizeof(error)));
    CHECK(g->house.storage==2);
    CHECK(step(s,HMBTN_PAD_RIGHT,error,sizeof(error)) && step(s,HMBTN_PAD_RIGHT,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && step(s,HMBTN_PAD_LEFT,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(g->game.inventory.icebox[0].id==0x59 && !g->game.inventory.items[1].id && g->game.inventory.items[0].id==0x9a);
    CHECK(capture_stable(s,&host,shots,"house-icebox",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && house_wait(s,HMBTN_HOUSE_FREE,0,error,sizeof(error)));
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(kitchen_new(s,error,sizeof(error)) && capture_stable(s,&host,shots,"house-kitchen-tools",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && !g->house.kitchen.options && g->house.sound==2);
    CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)) && kitchen_wait(s,HMBTN_KITCHEN_ITEMS,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && step(s,HMBTN_PAD_R1,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(capture_stable(s,&host,shots,"house-kitchen-ingredients",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)) && kitchen_wait(s,HMBTN_KITCHEN_CONFIRM,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && kitchen_wait(s,HMBTN_KITCHEN_ITEMS,error,sizeof(error)));
    CHECK(g->game.inventory.items[0].id==0x9a && g->game.inventory.icebox[0].id==0x59);
    CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)) && kitchen_wait(s,HMBTN_KITCHEN_CONFIRM,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(g->game.inventory.held_item.id==0xd4 && g->game.inventory.held_item.modifier==30 && !g->game.inventory.items[0].id && !g->game.inventory.icebox[0].id);
    CHECK(kitchen_wait(s,HMBTN_KITCHEN_RESULT,error,sizeof(error)) && capture_stable(s,&host,shots,"house-cooked-dish",error,sizeof(error)));
    CHECK(cooked(s,error,sizeof(error)) && hmbtn_cooking_record_count(&g->game.cooking)==1);
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(g->game.inventory.held_item.id==0xd4 && g->game.cooking.records[0].id==0xd4 && g->game.inventory.cabinet[0].id==0x7b);
    /* Deposit the actual cooked dish, then use the shifted bed and diary/save. */
    CHECK(object(s,4,error,sizeof(error)) && house_wait(s,HMBTN_HOUSE_TOOLBOX,0x3ae,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(g->game.inventory.icebox[0].id==0xd4 && !g->game.inventory.held_item.id);
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && house_wait(s,HMBTN_HOUSE_FREE,0,error,sizeof(error)));
    /* Stored recipe view/check/remake, with the original 4-choice prompt. */
    CHECK(kitchen_entry(s,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && kitchen_wait(s,HMBTN_KITCHEN_RECORDS,error,sizeof(error)));
    CHECK(capture_stable(s,&host,shots,"house-recipe-list",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_TRIANGLE,error,sizeof(error)) && kitchen_wait(s,HMBTN_KITCHEN_RECORD_VIEW,error,sizeof(error)));
    CHECK(capture_stable(s,&host,shots,"house-recipe-view",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && kitchen_wait(s,HMBTN_KITCHEN_RECORDS,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && kitchen_wait(s,HMBTN_KITCHEN_DESCRIPTION,error,sizeof(error)) && g->house.request==0x3ba);
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && kitchen_wait(s,HMBTN_KITCHEN_RECORDS,error,sizeof(error)));
    g->game.inventory.items[0]=(HmbtnStoredTool){0x9a,0,1};g->game.inventory.items[1]=(HmbtnStoredTool){0x59,0,1};
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && kitchen_wait(s,HMBTN_KITCHEN_RECIPE_CONFIRM,error,sizeof(error)) && g->house.message.choices==4);
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && cooked(s,error,sizeof(error)) && g->game.inventory.held_item.id==0xd4);
    CHECK(hmbtn_cooking_record_count(&g->game.cooking)==1 && deposit(s,0,error,sizeof(error)) && g->game.inventory.icebox[0].count==2);
    /* Fixture ownership of seasonings: their TV purchase is a later delivery.
     * This verifies selection, exact original salt recipe and failed cooking. */
    g->game.cooking.utensils=0x80;g->game.inventory.items[0]=(HmbtnStoredTool){0x43,0,1};
    CHECK(kitchen_new(s,error,sizeof(error)) && step(s,HMBTN_PAD_LEFT,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(kitchen_wait(s,HMBTN_KITCHEN_SEASONING,error,sizeof(error)) && step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(g->house.kitchen.options==0x100 && capture_stable(s,&host,shots,"house-seasonings",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && kitchen_wait(s,HMBTN_KITCHEN_TOOLS,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)) && kitchen_wait(s,HMBTN_KITCHEN_ITEMS,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)) && kitchen_wait(s,HMBTN_KITCHEN_CONFIRM,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(cooked(s,error,sizeof(error)) && g->game.inventory.held_item.id==0xba && hmbtn_cooking_record_count(&g->game.cooking)==2);
    CHECK(deposit(s,1,error,sizeof(error)) && g->game.inventory.icebox[1].id==0xba);
    g->game.inventory.items[0]=(HmbtnStoredTool){0x43,0,1};
    CHECK(kitchen_new(s,error,sizeof(error)) && step(s,HMBTN_PAD_START,error,sizeof(error)) && kitchen_wait(s,HMBTN_KITCHEN_ITEMS,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && step(s,HMBTN_PAD_START,error,sizeof(error)) && kitchen_wait(s,HMBTN_KITCHEN_CONFIRM,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && kitchen_wait(s,HMBTN_KITCHEN_RESULT,error,sizeof(error)) && g->house.request==0x3c2);
    CHECK(capture_stable(s,&host,shots,"house-cooking-failed",error,sizeof(error)) && cooked(s,error,sizeof(error)));
    CHECK(g->game.inventory.held_item.id==0xe2 && g->game.inventory.held_item.modifier==1 && hmbtn_cooking_record_count(&g->game.cooking)==2);
    CHECK(deposit(s,2,error,sizeof(error)) && g->game.inventory.icebox[2].id==0xe2);
    CHECK(object(s,2,error,sizeof(error)) && house_wait(s,HMBTN_HOUSE_BED,0,error,sizeof(error)));
    const unsigned day=g->game.clock.day;
    CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    bool shifted=false,sleep_captured=false;
    for(unsigned i=0;i<1500 && (g->game.clock.day==day || g->morning_fade);++i) {
        CHECK(step(s,0,error,sizeof(error)));
        if(g->sleep.active) { CHECK(g->sleep.target[0]==-2950 && g->sleep.target[2]==(level==2 ? -2560 : -960));shifted=true; }
        if(g->sleep.active && g->sleep.phase==3 && g->sleep.link_step==12 && !sleep_captured) {
            HmbtnSessionFrame f=hmbtn_scene_preview_frame(s);
            const unsigned raised=level==2 ? 1u : 2u,settled=level==2 ? 5u : 3u;
            CHECK((f.scene->flags[raised]&0x1000u) && !(f.scene->flags[settled]&0x1000u)
                && f.scene->models[settled].morph_steps==12 && f.scene->models[settled].morph_step==12);
            if(level==2) CHECK(!(f.scene->flags[2]&0x1000u)); /* The other bed stays visible. */
            CHECK(capture_stable(s,&host,shots,"house-sleeping",error,sizeof(error)));sleep_captured=true;
        }
    }
    CHECK(shifted && sleep_captured && g->game.clock.day!=day && g->area==80 && !g->morning_fade && hmbtn_read_le16(hmbtn_scene_preview_resources(s).section+4)==interior);
    CHECK(g->intro.player.position[0]==(level==2 ? 902 : 791) && g->intro.player.position[2]==(level==2 ? 1804 : 258) && g->morning_save_requested);
    g->morning_save_requested=false; /* Direct scene driver, file I/O is done below between ticks. */
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(g->game.inventory.icebox[0].id==0xd4 && g->game.inventory.cabinet[0].id==0x7b && g->game.inventory.toolbox[63].count==7);
    /* Seasonal fixtures validate all four original section variants. */
    for(unsigned season=0;season<4;++season) {
        g->game.clock.season=season;g->field.season=season;g->daily_date=hmbtn_session_date(&g->game.clock);
        CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)) && hmbtn_read_le16(hmbtn_scene_preview_resources(s).section+4)==farm+season);
        CHECK(travel(s,80,error,sizeof(error)) && hmbtn_read_le16(hmbtn_scene_preview_resources(s).section+4)==interior+season);
        CHECK(capture_stable(s,&host,shots,season==0 ? "house-spring" : season==1 ? "house-summer" : season==2 ? "house-fall" : "house-winter",error,sizeof(error)));
    }
    if(level==2) {
        CHECK(g->game.inventory.icebox[63].count==23 && hmbtn_livestock_count(&g->livestock,false)==1
            && hmbtn_livestock_count(&g->livestock,true)==1 && g->livestock.animals[0].name[0]==0x100);
        const unsigned discarded=g->carry.discarded;g->farm_action.fatigue=12;
        CHECK(travel(s,0,error,sizeof(error)) && object(s,13,error,sizeof(error)));
        for(unsigned i=0;i<100 && g->house_lumber.phase;++i) CHECK(step(s,0,error,sizeof(error)));
        CHECK(g->game.inventory.held_item.id==0x3f && g->game.inventory.supplies[1]==29 && !g->house_lumber.phase);
        CHECK(travel(s,80,error,sizeof(error)) && g->game.inventory.held_item.id==0x3f);
        CHECK(capture_stable(s,&host,shots,"house-fireplace-unlit",error,sizeof(error)));
        CHECK(object(s,18,error,sizeof(error)) && g->house_lumber.phase==3);
        for(unsigned i=0;i<100 && g->house_lumber.phase;++i) CHECK(step(s,0,error,sizeof(error)));
        CHECK(!g->house_lumber.phase && !g->game.inventory.held_item.id && g->village.flags[0x45]
            && g->house_lumber.deliveries==1 && g->farm_action.fatigue==7 && g->carry.discarded==discarded);
        g->intro.player.position[2]-=450; /* Move the observer out of the fireplace opening. */
        CHECK(step(s,0,error,sizeof(error)) && capture_stable(s,&host,shots,"house-fireplace-lit",error,sizeof(error)));
        for(unsigned i=0;i<12;++i) CHECK(step(s,0,error,sizeof(error)));
        CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
        CHECK(g->village.flags[0x45] && g->house_lumber.deliveries==1);
        CHECK(travel(s,0,error,sizeof(error)) && travel(s,80,error,sizeof(error)) && !g->village.flags[0x45]);
        CHECK(g->house_lumber.deliveries==1 && capture_stable(s,&host,shots,"house-fireplace-reentry",error,sizeof(error)));
    }
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);
    printf("House%u: real Gotz order, four nights, saves, original maps/furniture, shared storage/cooking/recipes, bed/diary and four seasons passed.\n",level);return 0;
fail:hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

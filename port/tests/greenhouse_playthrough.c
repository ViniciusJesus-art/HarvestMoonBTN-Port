/* Gotz's real fifth offer, construction, installed greenhouse, shared field
 * actions, winter crop/shipping, current saves and storm/reconstruction. */
#include "poultry_helpers.h"
#include "field_helpers.h"
#include "cooking.h"
static bool offer(HmbtnScenePreview *s,char *error,size_t size)
{
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
    REQUIRE(shop_wait(s,HMBTN_SHOP_INFO,error,size) && hmbtn_scene_preview_session(s)->feed_shop.request==0x140);
    return shop_wait(s,HMBTN_SHOP_BUILD_CONFIRM,error,size);
}
static void target(HmbtnScenePreview *s,unsigned c,unsigned r)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    hmbtn_farm_field_position(&g->greenhouse,c-1,r,g->intro.player.position);
    g->intro.player.facing=3;g->control.mode=0;
}
static bool soil_uv(HmbtnScenePreview *s,unsigned clut)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    const uint8_t *quad=hmbtn_scene_preview_resources(s).scene->terrain.data+g->greenhouse.quad[24];
    return hmbtn_read_le16(quad+6)==clut && hmbtn_read_le16(quad+10)==9;
}
static bool shot(HmbtnScenePreview *s,HmbtnHost *host,const char *dir,const char *name,char *error,size_t size)
{
    uint8_t *before=NULL,*after=NULL;size_t a=0,b=0;
    REQUIRE(hmbtn_save_encode(hmbtn_scene_preview_session(s),&before,&a,error,size));
    bool ok=capture(s,host,dir,name,error,size)
        && hmbtn_save_encode(hmbtn_scene_preview_session(s),&after,&b,error,size) && a==b && !memcmp(before,after,a);
    free(before);free(after);return ok;
}
static bool night(HmbtnScenePreview *s,unsigned tomorrow,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->feed.barn[0]=g->feed.barn[1]=true;
    g->game.weather.tomorrow=tomorrow;return sleep_day(s,error,size);
}
int main(int argc,char **argv)
{
    if(argc<4 || argc>5) return 2;const char *shots=argc==5 ? argv[4] : NULL;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock clock={0,0,3,3,12,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],212,&clock,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    g->poultry.upgraded=true;g->construction.house_level=2;g->construction.barn_level=1;
    g->game.economy.gold=31300;g->game.inventory.supplies[1]=610;
    g->game.inventory.icebox[63]=(HmbtnStoredTool){0x43,7,23};
    g->game.inventory.cabinet[63]=(HmbtnStoredTool){0x7b,0,9};
    const uint16_t ingredients[8]={0x58,0x99};HmbtnRecipeRecord recipe;
    CHECK(hmbtn_cooking_evaluate(0,ingredients,&recipe) && hmbtn_cooking_remember(&g->game.cooking,&recipe));
    const uint16_t name[9]={0x100,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff};
    CHECK(hmbtn_livestock_buy(&g->livestock,0x161,name,&g->game.clock) && hmbtn_livestock_buy(&g->livestock,0x165,name,&g->game.clock));
    g->livestock.animals[0].affection=g->livestock.animals[1].affection=200;
    g->village.flags[251+288]=g->village.flags[251+140]=true;
    HmbtnVillager *gotz=&g->village.people[24];CHECK(gotz->actor.visible && gotz->schedule[gotz->step][1]>>8==6);
    memcpy(g->intro.player.position,gotz->actor.position,sizeof(g->intro.player.position));g->intro.player.position[2]+=300;g->intro.player.facing=0;
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && step(s,HMBTN_PAD_TRIANGLE,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_INFO,error,sizeof(error)) && g->feed_shop.request==0x138);
    CHECK(capture(s,&host,shots,"gotz-greenhouse-description",error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(offer(s,error,sizeof(error)) && capture(s,&host,shots,"gotz-greenhouse-order",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(!g->construction.mornings_left && g->game.economy.gold==31300 && g->game.inventory.supplies[1]==610);
    g->game.economy.gold=29999;CHECK(offer(s,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && g->feed_shop.request==0x4f);
    CHECK(g->game.inventory.supplies[1]==610 && shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    g->game.economy.gold=31300;g->game.inventory.supplies[1]=579;
    CHECK(offer(s,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && g->feed_shop.request==0x145);
    CHECK(g->game.economy.gold==31300 && shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    g->game.inventory.supplies[1]=610;CHECK(offer(s,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_OFF,error,sizeof(error)));
    for(unsigned i=0;i<1000 && g->talk.active;++i) CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(!g->talk.active && !g->talk.script.boundary && g->construction.pending==HMBTN_BUILD_GREENHOUSE
        && g->construction.mornings_left==4 && g->game.economy.gold==1300 && g->game.inventory.supplies[1]==30);
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)));
    for(unsigned day=0;day<4;++day) {
        CHECK(night(s,HMBTN_WEATHER_SUNNY,error,sizeof(error)) && g->construction.mornings_left==3-day && g->construction.greenhouse==(day==3));
        if(day==1) { CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s); }
    }
    CHECK(hmbtn_read_le16(hmbtn_scene_preview_resources(s).section+4)==20 && hmbtn_farm_menu_building_score(&g->menu)==100);
    CHECK(shot(s,&host,shots,"greenhouse-farm",error,sizeof(error)) && field_refill(s,error,sizeof(error)));
    /* The fixture changes season; all subsequent till/sow/water/harvest actions
     * and eight overnight construction/crop updates use production input. */
    g->game.clock.season=3;g->game.clock.day=3;g->daily_date=hmbtn_session_date(&g->game.clock);
    CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)) && travel(s,124,error,sizeof(error)));
    CHECK(hmbtn_read_le16(hmbtn_scene_preview_resources(s).section+4)==127 && hmbtn_session_field(g)==&g->greenhouse);
    unsigned active=0;for(unsigned i=0;i<49;++i) active+=g->greenhouse.active[i];CHECK(active==49);
    CHECK(shot(s,&host,shots,"greenhouse-winter-entry",error,sizeof(error)));
    target(s,3,3);CHECK(tool(s,6,error,sizeof(error)) && g->greenhouse.cells[24].value==0x168 && soil_uv(s,0x3c64));
    CHECK(hmbtn_inventory_add_seed_packet(&g->game.inventory,0x22,NULL));
    CHECK(hmbtn_inventory_cycle(&g->game.inventory,true,false) && g->game.inventory.equipped.id==0x22);
    CHECK(hmbtn_farm_field_position(&g->greenhouse,3,3,g->intro.player.position));g->intro.player.facing=3;
    CHECK(tool(s,0x22,error,sizeof(error)) && g->greenhouse.cells[24].value==0x22 && !g->game.inventory.equipped.id && soil_uv(s,0x3e25));
    target(s,3,3);CHECK(tool(s,21,error,sizeof(error)) && g->greenhouse.cells[24].ground==0x16b && soil_uv(s,0x3de5));
    const HmbtnFarmTile outside=g->field.cells[24];
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(!memcmp(&outside,&g->field.cells[24],sizeof(outside)) && g->greenhouse.cells[24].value==0x22);
    CHECK(travel(s,0,error,sizeof(error)));
    for(unsigned day=0;day<4;++day) {
        CHECK(night(s,HMBTN_WEATHER_SUNNY,error,sizeof(error)) && travel(s,124,error,sizeof(error)));
        CHECK(g->greenhouse.cells[24].kind==(day<1 ? 0u : day<3 ? 1u : 2u));
        target(s,3,3);CHECK(step(s,0,error,sizeof(error)));
        CHECK(shot(s,&host,shots,day==0 ? "greenhouse-seed" : day==1 ? "greenhouse-sprout" : day==2 ? "greenhouse-growth" : "greenhouse-ripe",error,sizeof(error)));
        if(day<3) { target(s,3,3);CHECK(tool(s,21,error,sizeof(error)) && travel(s,0,error,sizeof(error))); }
    }
    target(s,3,3);CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && idle(s,error,sizeof(error)));
    CHECK(g->game.inventory.held_item.id==0x41 && g->greenhouse.cells[24].value==0x168);
    CHECK(travel(s,0,error,sizeof(error)) && g->game.inventory.held_item.id==0x41 && ship(s,error,sizeof(error)));
    const unsigned gold=g->game.economy.gold;
    CHECK(night(s,HMBTN_WEATHER_SUNNY,error,sizeof(error)) && g->game.economy.gold==gold+60);
    CHECK(travel(s,124,error,sizeof(error)) && field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    /* Explicit art-only fixtures exercise every already-supported crop in
     * the original greenhouse resource pack, including mature flower ranges. */
    unsigned at=0;
    for(unsigned seed=0x22;seed<=0x34;++seed) if(seed!=0x33) {
        g->greenhouse.cells[at++]=(HmbtnFarmTile){.value=(uint16_t)seed,.ground=0x168,.kind=(uint8_t)hmbtn_farm_crop(seed)->grown};
    }
    g->greenhouse.cells[at++]=(HmbtnFarmTile){.value=0x178,.ground=0x168,.kind=4};
    g->greenhouse.cells[at++]=(HmbtnFarmTile){.value=0x179,.ground=0x168,.kind=4};
    g->greenhouse.cells[48]=(HmbtnFarmTile){.value=0x36,.ground=0x168,.kind=3};
    CHECK(hmbtn_scene_preview_load_area(s,124,error,sizeof(error)));
    for(unsigned season=0;season<4;++season) {
        g->game.clock.season=season;g->daily_date=hmbtn_session_date(&g->game.clock);
        CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)) && hmbtn_read_le16(hmbtn_scene_preview_resources(s).section+4)==20+season);
        CHECK(travel(s,124,error,sizeof(error)) && hmbtn_read_le16(hmbtn_scene_preview_resources(s).section+4)==124+season);
        CHECK(shot(s,&host,shots,season==0 ? "greenhouse-spring-catalog" : season==1 ? "greenhouse-summer-catalog" : season==2 ? "greenhouse-fall-catalog" : "greenhouse-winter-catalog",error,sizeof(error)));
    }
    CHECK(hmbtn_scene_preview_set_weather(s,HMBTN_WEATHER_RAIN,HMBTN_WEATHER_RAIN));
    const HmbtnFarmClock stopped=g->game.clock;const HmbtnFarmTile dry=g->greenhouse.cells[24];
    for(unsigned i=0;i<40;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(!memcmp(&stopped,&g->game.clock,sizeof(stopped)) && !memcmp(&dry,&g->greenhouse.cells[24],sizeof(dry)));
    CHECK(!hmbtn_world_outdoors(hmbtn_scene_preview_resources(s).section,8192) && shot(s,&host,shots,"greenhouse-rain-protected",error,sizeof(error)));
    CHECK(travel(s,0,error,sizeof(error)));
    g->game.clock=(HmbtnFarmClock){0,1,8,2,12,0,0};g->daily_date=hmbtn_session_date(&g->game.clock);g->construction.damage_random=0;
    CHECK(night(s,HMBTN_WEATHER_STORM,error,sizeof(error)) && !g->construction.greenhouse);
    CHECK(hmbtn_read_le16(hmbtn_scene_preview_resources(s).section+4)==17 && g->greenhouse.cells[48].value==0x167);
    unsigned location;CHECK(!hmbtn_scene_preview_area_location(s,124,&location,error,sizeof(error)));
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    g->game.economy.gold=30000;g->game.inventory.supplies[1]=580;
    CHECK(hmbtn_construction_order(&g->construction,&g->poultry,&g->game.inventory,&g->game.economy)==HMBTN_BUILD_OK);
    for(unsigned day=0;day<4;++day) CHECK(night(s,HMBTN_WEATHER_SUNNY,error,sizeof(error)));
    CHECK(g->construction.greenhouse && travel(s,124,error,sizeof(error)) && g->greenhouse.cells[48].value==0x167);
    CHECK(g->game.inventory.icebox[63].count==23 && g->game.inventory.cabinet[63].count==9
        && g->game.cooking.records[0].quality==30 && hmbtn_livestock_count(&g->livestock,false)==1 && hmbtn_livestock_count(&g->livestock,true)==1);
    CHECK(shot(s,&host,shots,"greenhouse-rebuilt",error,sizeof(error)) && field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);
    puts("Greenhouse: real Gotz order/cancel/failures, four nights/reloads, 49 original quads, winter till/sow/water/harvest/shipping, seasonal art, rain shelter, storm loss/reconstruction and current save passed.");return 0;
fail:hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

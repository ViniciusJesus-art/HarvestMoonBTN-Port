/* Real Won conversation/purchase and greenhouse actions on installed resources.
 * Fixtures declare money, prior construction and date; no fixture seed packet. */
#include "poultry_helpers.h"
#include "field_helpers.h"
static void target(HmbtnScenePreview *s)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    hmbtn_farm_field_position(&g->greenhouse,2,3,g->intro.player.position);
    g->intro.player.facing=3;g->control.mode=0;
}
static bool night(HmbtnScenePreview *s,unsigned weather,char *error,size_t size)
{
    hmbtn_scene_preview_session(s)->game.weather.tomorrow=weather;
    return sleep_day(s,error,size);
}
static bool shot(HmbtnScenePreview *s,HmbtnHost *host,const char *dir,const char *name,char *error,size_t size)
{
    uint8_t *before=NULL,*after=NULL;size_t a=0,b=0;
    REQUIRE(hmbtn_save_encode(hmbtn_scene_preview_session(s),&before,&a,error,size));
    bool ok=capture(s,host,dir,name,error,size)
        && hmbtn_save_encode(hmbtn_scene_preview_session(s),&after,&b,error,size) && a==b && !memcmp(before,after,a);
    free(before);free(after);return ok;
}
int main(int argc,char **argv)
{
    if(argc<4 || argc>5) return 2;
    const char *shots=argc==5 ? argv[4] : NULL;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock clock={1,3,27,3,12,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],204,&clock,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->game.economy.gold=5000;
    g->village.flags[251+288]=g->village.flags[251+140]=true;
    HmbtnIntroActor *won=&g->village.people[35].actor;CHECK(won->visible && !g->construction.greenhouse);
    memcpy(g->intro.player.position,won->position,sizeof(won->position));g->intro.player.position[2]+=300;
    g->intro.player.facing=0;g->control.mode=0;
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && g->talk.active);
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)) && g->feed_shop.merchant==35);
    CHECK(hmbtn_seed_shop_count(&g->feed_shop)==7);
    for(unsigned i=0;i<6;++i) CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)));
    CHECK(g->feed_shop.item==0x35 && capture(s,&host,shots,"orangecup-menu",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_TRIANGLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_INFO,error,sizeof(error)));
    CHECK(g->feed_shop.request==0x16c && capture(s,&host,shots,"orangecup-description",error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_COUNT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(g->game.economy.gold==5000 && !hmbtn_inventory_count(&g->game.inventory,0x35,0,2));
    g->game.economy.gold=999;
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_COUNT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)));
    CHECK(g->feed_shop.request==0x4f && g->game.economy.gold==999 && !hmbtn_inventory_count(&g->game.inventory,0x35,0,2));
    CHECK(shop_wait(s,HMBTN_SHOP_COUNT,error,sizeof(error)));g->game.economy.gold=5000;
    CHECK(capture(s,&host,shots,"orangecup-quantity",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && g->game.economy.gold==4000);
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_OFF,error,sizeof(error)));
    for(unsigned i=0;i<1000 && g->talk.active;++i) CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(!g->talk.active && !g->talk.script.boundary && hmbtn_inventory_count(&g->game.inventory,0x35,0,2)==1);
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    /* Prior works are fixture prerequisites. Actual construction is already
     * covered by greenhouse_playthrough; purchasing requires none of them. */
    g->poultry.upgraded=true;g->construction.house_level=2;g->construction.barn_level=1;g->construction.greenhouse=true;
    CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)) && field_refill(s,error,sizeof(error)));
    CHECK(travel(s,124,error,sizeof(error)));
    target(s);CHECK(tool(s,6,error,sizeof(error)) && g->greenhouse.cells[24].value==0x168);
    CHECK(hmbtn_farm_field_position(&g->greenhouse,3,3,g->intro.player.position));g->intro.player.facing=3;
    CHECK(tool(s,0x35,error,sizeof(error)) && g->greenhouse.cells[24].value==0x35);
    CHECK(!hmbtn_inventory_count(&g->game.inventory,0x35,0,2));
    const HmbtnFarmTile outside=g->field.cells[24];const uint32_t random=g->greenhouse.plant_random;
    CHECK(travel(s,0,error,sizeof(error)) && night(s,HMBTN_WEATHER_SUNNY,error,sizeof(error)));
    CHECK(!g->greenhouse.cells[24].kind && !g->greenhouse.cells[24].watered_days); /* Unwatered night. */
    CHECK(travel(s,124,error,sizeof(error)));
    for(unsigned day=0;day<8;++day) {
        target(s);CHECK(tool(s,21,error,sizeof(error)) && travel(s,0,error,sizeof(error)));
        CHECK(night(s,HMBTN_WEATHER_SUNNY,error,sizeof(error)) && travel(s,124,error,sizeof(error)));
        CHECK(g->greenhouse.cells[24].value==0x35 && g->greenhouse.cells[24].kind==(day+1)/2
            && g->greenhouse.cells[24].watered_days==(day+1)%2 && g->greenhouse.plant_random==random);
        if(day==3) {
            CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
        }
        if(day%2) {
            unsigned sprite,animation;CHECK(hmbtn_estate_greenhouse_crop_look(0x35,g->greenhouse.cells[24].kind,&sprite,&animation));
            CHECK(sprite==24 && animation==g->greenhouse.cells[24].kind+2);
            const HmbtnAnimation *ani=hmbtn_estate_art_sprite(hmbtn_scene_preview_resources(s).estate_art,sprite);
            CHECK(ani && animation<ani->count[3]);
            target(s);CHECK(step(s,0,error,sizeof(error)));
            char name[32];snprintf(name,sizeof(name),"orangecup-stage-%u",g->greenhouse.cells[24].kind);
            CHECK(shot(s,&host,shots,name,error,sizeof(error)));
        }
    }
    CHECK(g->game.clock.year==2 && g->game.clock.season==0 && !memcmp(&outside,&g->field.cells[24],sizeof(outside)));
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    if(shots) {
        char ripe[1024];CHECK(snprintf(ripe,sizeof(ripe),"%s.ripe.hms",argv[3])<(int)sizeof(ripe));
        CHECK(hmbtn_scene_preview_save(s,ripe,error,sizeof(error)));
    }
    target(s);CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && idle(s,error,sizeof(error)));
    CHECK(g->game.inventory.held_item.id==0x85 && g->greenhouse.cells[24].value==0x168);
    CHECK(shot(s,&host,shots,"orangecup-harvest",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_R2,error,sizeof(error)) && idle(s,error,sizeof(error)));
    CHECK(g->game.inventory.items[0].id==0x85 && !g->game.inventory.held_item.id);
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(step(s,HMBTN_PAD_R2,error,sizeof(error)) && idle(s,error,sizeof(error)) && g->game.inventory.held_item.id==0x85);
    CHECK(travel(s,0,error,sizeof(error)) && g->game.inventory.held_item.id==0x85);
    unsigned shipments[25];memcpy(shipments,g->game.economy.shipments,sizeof(shipments));
    CHECK(ship(s,error,sizeof(error)) && !g->game.inventory.held_item.id);
    const unsigned gold=g->game.economy.gold;
    CHECK(night(s,HMBTN_WEATHER_SUNNY,error,sizeof(error)) && g->game.economy.gold==gold+60
        && !memcmp(shipments,g->game.economy.shipments,sizeof(shipments)));
    CHECK(night(s,HMBTN_WEATHER_SUNNY,error,sizeof(error)) && g->game.economy.gold==gold+60);
    /* Explicit source-art vectors; no claim of spring outdoor visibility. */
    unsigned sprite,animation;
    CHECK(!hmbtn_estate_crop_look(0x35,4,0,&sprite,&animation));
    CHECK(hmbtn_estate_crop_look(0x35,1,1,&sprite,&animation) && sprite==18 && animation==3);
    CHECK(hmbtn_estate_crop_look(0x35,1,2,&sprite,&animation) && sprite==38 && !animation);
    /* Loss after a growing Orangecup, then reconstruction, cannot resurrect it. */
    g->greenhouse.cells[48]=(HmbtnFarmTile){.value=0x35,.ground=0x168,.kind=3};
    g->game.clock=(HmbtnFarmClock){2,1,8,2,12,0,0};g->daily_date=hmbtn_session_date(&g->game.clock);
    g->construction.damage_random=0;
    CHECK(night(s,HMBTN_WEATHER_STORM,error,sizeof(error)) && !g->construction.greenhouse
        && g->greenhouse.cells[48].value==0x167);
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    g->game.economy.gold=30000;g->game.inventory.supplies[1]=580;
    CHECK(hmbtn_construction_order(&g->construction,&g->poultry,&g->game.inventory,&g->game.economy)==HMBTN_BUILD_OK);
    for(unsigned i=0;i<4;++i) CHECK(night(s,HMBTN_WEATHER_SUNNY,error,sizeof(error)));
    CHECK(g->construction.greenhouse && travel(s,124,error,sizeof(error)) && g->greenhouse.cells[48].value==0x167);
    CHECK(shot(s,&host,shots,"orangecup-rebuilt",error,sizeof(error)) && field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);
    puts("Orangecup: real Won description/cancel/funds/1000 G purchase, winter planting/dry night, 8 watered nights across New Year, pack-4 art, harvest/bag/doors/60 G payment, saves and storm/rebuild passed.");return 0;
fail:hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

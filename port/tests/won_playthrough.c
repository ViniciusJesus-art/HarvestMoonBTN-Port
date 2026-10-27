/* Won's imported conversation opens the existing seed shop. No fixture shop,
 * hardcoded merchant spawn or alternative purchase/growth implementation. */
#include "poultry_helpers.h"
#include "field_helpers.h"
typedef struct WonCrop {
    unsigned seed,product,season,price,sale,grown,regrow;int repeat;
    unsigned days;uint8_t stages[20],progress[20];
    unsigned sprite,source_row;
} WonCrop;
static const WonCrop crops[]={
    {0x26,0x45,0,500,250,3,0,-1,14,{0,0,0,1,1,1,1,1,2,2,2,2,2,3},{1,2,3,0,1,2,3,4,0,1,2,3,4,0},11,0},
    {0x2b,0x4a,1,1000,500,4,5,3,20,{0,0,0,0,1,1,1,1,1,2,2,2,2,2,3,3,3,3,3,4},{1,2,3,4,0,1,2,3,4,0,1,2,3,4,0,1,2,3,4,0},16,1},
    {0x30,0x4f,2,150,40,4,2,3,7,{0,1,2,2,3,3,4},{1,0,0,1,0,1,0},16,2},
    {0x32,0x52,1,200,0,3,0,-1,6,{0,1,1,2,2,3},{1,0,1,0,1,0},17,4},
    {0x34,0x55,0,500,0,4,0,-1,12,{0,0,1,1,1,2,2,2,3,3,3,4},{1,2,0,1,2,0,1,2,0,1,2,0},13,6},
    {0x33,0x53,2,600,0,4,0,-1,10,{0,0,1,1,1,2,2,3,3,4},{1,2,0,1,2,0,1,0,1,0},17,5},
    {0x33,0x54,2,600,200,4,0,-1,10,{0,0,1,1,1,2,2,3,3,4},{1,2,0,1,2,0,1,0,1,0},25,5},
};
static unsigned planted(const WonCrop *c)
{ return c->seed==0x33 ? (c->product==0x54 ? 0x179u : 0x178u) : c->seed; }
static bool mature_zero(const WonCrop *c)
{ return c->seed==0x26 || c->seed==0x32 || c->seed==0x34 || c->seed==0x33; }
static bool sleep_sunny(HmbtnScenePreview *s,char *error,size_t size)
{
    hmbtn_scene_preview_session(s)->game.weather.tomorrow=HMBTN_WEATHER_SUNNY;
    return sleep_day(s,error,size);
}
static bool flower_finish(HmbtnScenePreview **scene,HmbtnHost *host,const char *root,const char *save,
    const char *shots,const WonCrop *c,char *error,size_t size)
{
    HmbtnScenePreview *s=*scene;HmbtnSession *g=hmbtn_scene_preview_session(s);
    REQUIRE(g->field.cells[460].value!=c->seed && !g->field.cells[460].kind);
    REQUIRE(capture(s,host,shots,"won-flower-held",error,size));
    REQUIRE(step(s,HMBTN_PAD_R2,error,size) && idle(s,error,size));
    REQUIRE(!g->game.inventory.held_item.id && g->game.inventory.items[0].id==c->product);
    if(!field_reload(scene,host,root,save,error,size)) return false;
    s=*scene;g=hmbtn_scene_preview_session(s);
    REQUIRE(g->game.inventory.items[0].id==c->product);
    REQUIRE(step(s,HMBTN_PAD_R2,error,size) && idle(s,error,size) && g->game.inventory.held_item.id==c->product);
    if(!field_reload(scene,host,root,save,error,size)) return false;
    s=*scene;g=hmbtn_scene_preview_session(s);
    REQUIRE(g->game.inventory.held_item.id==c->product);
    /* Gift the actual harvested flower to Lillia, using her real schedule and
     * imported conversation. Advance to midday; wait for her next working day
     * if absent, instead of creating a resident or skipping a script. */
    hmbtn_session_advance_minutes(g,360);
    REQUIRE(hmbtn_scene_preview_load_area(s,128,error,size));
    for(unsigned day=0;day<7;++day) {
        REQUIRE(step(s,0,error,size));
        if(g->village.people[18].actor.visible) break;
        hmbtn_session_advance_minutes(g,1440);
    }
    HmbtnVillager *lillia=&g->village.people[18];REQUIRE(lillia->actor.visible && lillia->location==49);
    memcpy(g->intro.player.position,lillia->actor.position,sizeof(g->intro.player.position));
    g->intro.player.position[2]+=300;g->intro.player.facing=0;g->control.mode=0;
    const unsigned friendship=lillia->friendship,discarded=g->carry.discarded,gold=g->game.economy.gold;
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->talk.active && g->talk.target==18);
    for(unsigned i=0;i<3000 && g->talk.active;++i) REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
    REQUIRE(!g->talk.active && !g->talk.script.boundary && !g->game.inventory.held_item.id
        && lillia->friendship>friendship && !g->carry.phase && g->carry.discarded==discarded && g->game.economy.gold==gold);
    REQUIRE(capture(s,host,shots,"won-flower-gift",error,size));
    /* Explicit season-boundary fixture: an established second flower bed.
     * Normal sleep/resource reload must resolve the original withered art. */
    g->game.clock.day=29;g->daily_date=hmbtn_session_date(&g->game.clock);
    g->field.cells[460]=(HmbtnFarmTile){.value=(uint16_t)planted(c),.ground=0x168,.kind=(uint8_t)c->grown};
    REQUIRE(hmbtn_scene_preview_load_area(s,0,error,size) && sleep_sunny(s,error,size));
    REQUIRE(g->game.clock.season==c->season+1);
    unsigned sprite,animation;
    if(c->season==2) {
        REQUIRE(g->field.cells[460].value==0x168 && !g->field.cells[460].kind);
        REQUIRE(!hmbtn_estate_crop_look(planted(c),1,3,&sprite,&animation));
    } else {
        REQUIRE(g->field.cells[460].value==planted(c) && g->field.cells[460].kind==1);
        REQUIRE(hmbtn_estate_crop_look(planted(c),1,g->game.clock.season,&sprite,&animation));
        REQUIRE(sprite==(c->seed==0x32 ? 37u : 41u) && !animation);
        const HmbtnAnimation *ani=hmbtn_estate_art_sprite(hmbtn_scene_preview_resources(s).estate_art,sprite);
        REQUIRE(ani && ani->count[3]);
    }
    cell(s,10,10);REQUIRE(step(s,0,error,size));
    REQUIRE(capture(s,host,shots,"won-flower-withered",error,size));
    return true;
}
int main(int argc,char **argv)
{
    if(argc<4 || argc>5) return 2;
    const unsigned selection=(unsigned)strtoul(argv[2],NULL,10);if(selection>=sizeof(crops)/sizeof(*crops)) return 2;
    const WonCrop *c=&crops[selection];const char *shots=argc==5 ? argv[4] : NULL;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};
    CHECK(hmbtn_host_open(&host));
    /* Midday at the actual inn, avoiding the Spring/Summer 1 festivals. */
    const HmbtnFarmClock clock={0,c->season,c->season==0 ? 2u : c->season==1 ? 1u : 0u,3,12,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],204,&clock,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->game.economy.gold=5000;
    g->village.flags[251+288]=true;g->village.flags[251+140]=true;
    HmbtnIntroActor *won=&g->village.people[35].actor;
    if(!won->visible) fprintf(stderr,"Won actual schedule: location %u row %u area %u\n",g->village.people[35].location,g->village.people[35].row,g->area);
    CHECK(won->visible);
    memcpy(g->intro.player.position,won->position,sizeof(won->position));g->intro.player.position[2]+=300;
    g->intro.player.facing=0;g->control.mode=0;
    CHECK(hmbtn_village_target(&g->village,&g->intro.player)==35);
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && g->talk.active);
    CHECK(shop_wait(s,HMBTN_SHOP_GREETING,error,sizeof(error)));
    CHECK(g->feed_shop.merchant==35 && g->feed_shop.request==0x164 && g->talk.active);
    CHECK(capture(s,&host,shots,"won-greeting",error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(hmbtn_seed_shop_count(&g->feed_shop)==6);
    unsigned menu_row=0;
    while(menu_row<hmbtn_seed_shop_count(&g->feed_shop) && hmbtn_seed_shop_choice(&g->feed_shop,menu_row)->item!=c->seed) ++menu_row;
    CHECK(menu_row<hmbtn_seed_shop_count(&g->feed_shop));
    for(unsigned i=0;i<menu_row;++i) CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)));
    CHECK(g->feed_shop.item==c->seed);
    CHECK(capture(s,&host,shots,"won-menu",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_TRIANGLE,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_INFO,error,sizeof(error)) && g->feed_shop.request==0x165+c->source_row);
    CHECK(capture(s,&host,shots,"won-description",error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_OFFER,error,sizeof(error)));
    CHECK(g->feed_shop.request==0x16e && shop_wait(s,HMBTN_SHOP_COUNT,error,sizeof(error)));
    CHECK(capture(s,&host,shots,"won-quantity",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && g->game.economy.gold==5000-c->price);
    CHECK(shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && g->feed_shop.request==0x170);
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_OFF,error,sizeof(error)));
    for(unsigned i=0;i<800 && g->talk.active;++i) CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(!g->talk.active && !g->talk.script.boundary && g->game.economy.gold==5000-c->price);
    CHECK(hmbtn_inventory_count(&g->game.inventory,c->seed,0,2)==1);
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(g->game.economy.gold==5000-c->price);
    /* The crop engine/action bank already used by seasonal seeds handles Won's seeds. */
    CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)));g->intro.area=0;
    g->field.cells[460]=(HmbtnFarmTile){.value=0x167,.ground=0x167};
    cell(s,10,10);CHECK(tool(s,6,error,sizeof(error)));
    /* Controlled native stream exercises each original variant through the real
     * action bank. Its one accepted cell rolls exactly once, at completion. */
    if(c->seed==0x33) g->field.plant_random=c->product==0x54 ? 0 : 1;
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    const uint32_t plant_before=g->field.plant_random;
    cell(s,11,10);CHECK(tool(s,c->seed,error,sizeof(error)) && g->field.cells[460].value==planted(c));
    CHECK(g->field.plant_random==(c->seed==0x33 ? plant_before*UINT32_C(0x41c64e6d)+12345u : plant_before));
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(field_refill(s,error,sizeof(error)));
    for(unsigned day=0;day<c->days;++day) {
        cell(s,10,10);CHECK(tool(s,21,error,sizeof(error)) && sleep_sunny(s,error,sizeof(error)));
        CHECK(g->field.cells[460].kind==c->stages[day] && g->field.cells[460].watered_days==c->progress[day]);
        if((!c->sale || c->seed==0x33) && day==1) {
            const HmbtnFarmTile before=g->field.cells[460];
            CHECK(sleep_sunny(s,error,sizeof(error)));
            CHECK(g->field.cells[460].kind==before.kind && g->field.cells[460].watered_days==before.watered_days);
        }
        if(c->stages[day] && (!day || c->stages[day]!=c->stages[day-1])) {
            unsigned sprite,animation;
            CHECK(hmbtn_estate_crop_look(planted(c),c->stages[day],c->season,&sprite,&animation));
            const HmbtnAnimation *ani=hmbtn_estate_art_sprite(hmbtn_scene_preview_resources(s).estate_art,sprite);
            CHECK(ani && animation<ani->count[3]);
            char name[32];snprintf(name,sizeof(name),"won-stage-%u",c->stages[day]);
            cell(s,10,10);CHECK(step(s,0,error,sizeof(error)) && capture(s,&host,shots,name,error,sizeof(error)));
        }
        if(day==4) {
            CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
        }
    }
    CHECK(g->field.plant_random==(c->seed==0x33 ? plant_before*UINT32_C(0x41c64e6d)+12345u : plant_before));
    unsigned sprite,animation;
    CHECK(hmbtn_estate_crop_look(planted(c),c->grown,c->season,&sprite,&animation));
    CHECK(sprite==c->sprite && animation==(mature_zero(c) ? 0u : c->grown+2));
    if(mature_zero(c)) {
        /* The original unchecked grown+2 lookup reads the first frame's zero
         * dx as its offset. The host resolves the resulting head as range 0. */
        const HmbtnAnimation *ani=hmbtn_estate_art_sprite(hmbtn_scene_preview_resources(s).estate_art,sprite);
        CHECK(ani && ani->count[3]==c->grown+2 && !hmbtn_read_le16(ani->data+ani->section[4]+2));
    }
    cell(s,10,10);CHECK(step(s,0,error,sizeof(error)) && capture(s,&host,shots,"won-grown",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && idle(s,error,sizeof(error)));
    CHECK(g->game.inventory.held_item.id==c->product);
    if(!c->sale) {
        CHECK(flower_finish(&s,&host,argv[1],argv[3],shots,c,error,sizeof(error)));
        g=hmbtn_scene_preview_session(s);
    } else {
        unsigned shipments[25];memcpy(shipments,g->game.economy.shipments,sizeof(shipments));
        CHECK(ship(s,error,sizeof(error)));
        const unsigned gold=g->game.economy.gold;CHECK(sleep_sunny(s,error,sizeof(error)));
        CHECK(g->game.economy.gold==gold+c->sale);
        if(c->seed==0x33) CHECK(!memcmp(shipments,g->game.economy.shipments,sizeof(shipments)));
        else CHECK(g->game.economy.shipments[c->product-0x41]==1);
        if(c->repeat>=0) {
            CHECK(g->field.cells[460].kind==(unsigned)c->repeat && (g->field.cells[460].growth_flags&0x8000));
            for(unsigned day=0;day<=c->regrow;++day) {
                cell(s,10,10);CHECK(tool(s,21,error,sizeof(error)) && sleep_sunny(s,error,sizeof(error)));
                CHECK(g->field.cells[460].kind==(day<c->regrow ? (unsigned)c->repeat : c->grown));
            }
            CHECK(hmbtn_farm_field_harvest_item(&g->field,460)==c->product);
            cell(s,10,10);CHECK(step(s,0,error,sizeof(error)) && capture(s,&host,shots,"won-regrown",error,sizeof(error)));
        }
    }
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);
    puts("Won: original conversation/shop/words, quantity/payment, crop/sprites/harvest, shipping/regrowth or flower/gift/season change, and persistent load passed.");return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

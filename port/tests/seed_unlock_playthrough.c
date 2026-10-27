/* Last shipment -> conditional table row -> bought seed -> crop -> shipment.
 * Historical shipment counts/purse and clear soil are declared fixtures;
 * the last shipment, shop, daily updates, ANI, harvest and saves are real. */
#include "poultry_helpers.h"
#include "field_helpers.h"
typedef struct UnlockCrop {
    unsigned seed,product,price,sale,days,grown,sprite;
    unsigned group[4];uint8_t stages[14],progress[14];
} UnlockCrop;
static const UnlockCrop crops[]={
    {0x25,0x44,150,30,8,3,10,{0x41,0x42,0x43,0x45},{0,0,1,1,1,2,2,3},{1,2,0,1,2,0,1,0}},
    {0x2a,0x49,500,250,14,3,15,{0x46,0x47,0x48,0x4a},{0,0,0,1,1,1,1,1,2,2,2,2,2,3},{1,2,3,0,1,2,3,4,0,1,2,3,4,0}},
    {0x2f,0x4e,200,80,5,2,15,{0x4b,0x4c,0x4d,0x4f},{0,1,1,1,2},{1,0,1,2,0}},
};
static bool dry_sleep(HmbtnScenePreview *s,char *error,size_t size)
{ hmbtn_scene_preview_session(s)->game.weather.tomorrow=0;return sleep_day(s,error,size); }
static void at_table(HmbtnScenePreview *s,unsigned season)
{
    const HmbtnGatherRecord *r=hmbtn_farm_gather_record(112+season);HmbtnSession *g=hmbtn_scene_preview_session(s);
    memcpy(g->intro.player.position,r->position,sizeof(r->position));
    g->intro.player.position[1]=0; /* Player stands on the shop floor, below the table records. */
    g->intro.player.position[0]+=season==1 ? -400 : season==2 ? 400 : 0;
    if(!season) g->intro.player.position[2]-=400;
    g->intro.player.facing=season==0 ? 2u : season==1 ? 3u : 1u;
    g->control.mode=0;memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));
}
int main(int argc,char **argv)
{
    if(argc<4 || argc>5) return 2;
    const unsigned season=(unsigned)strtoul(argv[2],NULL,10);if(season>=3) return 2;
    const UnlockCrop *c=&crops[season];const char *shots=argc==5 ? argv[4] : NULL;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock clock={0,season,season==2 ? 0u : 2u,season==2 ? 4u : 3u,12,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&clock,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->game.economy.gold=5000;
    g->village.flags[251+288]=g->village.flags[251+140]=true;
    for(unsigned i=0;i<4;++i) g->game.economy.shipments[c->group[i]-0x41]=i==3 ? 100 : 101;
    CHECK(hmbtn_scene_preview_load_area(s,180,error,sizeof(error)) && step(s,0,error,sizeof(error)));
    at_table(s,season);CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(hmbtn_seed_shop_count(&g->feed_shop)==4 && !hmbtn_seed_shop_choice(&g->feed_shop,4));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_OFF,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)));
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    g->game.inventory.held_item=(HmbtnStoredTool){(uint16_t)c->group[3],0,1};
    CHECK(ship(s,error,sizeof(error)) && g->game.economy.shipments[c->group[3]-0x41]==101);
    const unsigned initial_pending=g->game.economy.shipping.pending_value;
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(hmbtn_scene_preview_load_area(s,180,error,sizeof(error)) && step(s,0,error,sizeof(error)));
    at_table(s,season);CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(hmbtn_seed_shop_count(&g->feed_shop)==5 && g->feed_shop.economy==&g->game.economy);
    CHECK(step(s,HMBTN_PAD_UP,error,sizeof(error)) && g->feed_shop.selection==4 && g->feed_shop.item==c->seed);
    CHECK(capture(s,&host,shots,"unlocked-menu",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_TRIANGLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_INFO,error,sizeof(error)));
    CHECK(g->feed_shop.request==(season==0 ? 0xc6u : season==1 ? 0xcau : 0xceu));
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_COUNT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(g->game.economy.gold==5000);
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_COUNT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(g->game.economy.gold==5000-c->price && hmbtn_inventory_count(&g->game.inventory,c->seed,0,2)==1);
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_OFF,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)));
    g->field.cells[460]=(HmbtnFarmTile){.value=0x167,.ground=0x167};
    cell(s,10,10);CHECK(tool(s,6,error,sizeof(error)));cell(s,11,10);CHECK(tool(s,c->seed,error,sizeof(error)));
    CHECK(g->field.cells[460].value==c->seed && field_refill(s,error,sizeof(error)));
    for(unsigned day=0;day<c->days;++day) {
        cell(s,10,10);CHECK(tool(s,21,error,sizeof(error)) && dry_sleep(s,error,sizeof(error)));
        CHECK(g->field.cells[460].kind==c->stages[day] && g->field.cells[460].watered_days==c->progress[day]);
        if(day==1) { /* Dry night cannot advance growth. */
            const HmbtnFarmTile before=g->field.cells[460];CHECK(dry_sleep(s,error,sizeof(error)));
            CHECK(g->field.cells[460].kind==before.kind && g->field.cells[460].watered_days==before.watered_days);
        }
        if(day==3) { CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s); }
        if(c->stages[day] && (!day || c->stages[day]!=c->stages[day-1])) {
            unsigned sprite,animation;CHECK(hmbtn_estate_crop_look(c->seed,c->stages[day],season,&sprite,&animation) && sprite==c->sprite);
            const HmbtnAnimation *ani=hmbtn_estate_art_sprite(hmbtn_scene_preview_resources(s).estate_art,sprite);
            CHECK(ani && animation<ani->count[3]);
            cell(s,10,10);CHECK(step(s,0,error,sizeof(error)));
            char name[32];snprintf(name,sizeof(name),"unlocked-stage-%u",c->stages[day]);
            CHECK(capture(s,&host,shots,name,error,sizeof(error)));
        }
    }
    CHECK(g->game.economy.gold==5000-c->price+initial_pending);
    cell(s,10,10);CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && idle(s,error,sizeof(error)));
    CHECK(g->game.inventory.held_item.id==c->product && ship(s,error,sizeof(error)));
    const unsigned gold=g->game.economy.gold;CHECK(dry_sleep(s,error,sizeof(error)));
    if(g->game.economy.shipping.pending_value) CHECK(dry_sleep(s,error,sizeof(error)));
    CHECK(g->game.economy.gold==gold+c->sale && g->game.economy.shipments[c->product-0x41]==1);
    if(season==0) {
        /* Source crop_table: repeat stage 1, three + two watered updates,
         * plus FIELD_TILE_SKIP_DAY. Both stages use the existing daily engine. */
        const uint8_t stages[]={1,1,1,2,2,3},progress[]={0,1,2,0,1,0};
        for(unsigned i=0;i<6;++i) {
            cell(s,10,10);CHECK(tool(s,21,error,sizeof(error)) && dry_sleep(s,error,sizeof(error)));
            CHECK(g->field.cells[460].kind==stages[i] && g->field.cells[460].watered_days==progress[i]);
        }
        CHECK(hmbtn_farm_field_harvest_item(&g->field,460)==c->product);
        CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
        CHECK(hmbtn_farm_field_harvest_item(&g->field,460)==c->product);
    } else CHECK(g->field.cells[460].value!=c->seed);
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);
    puts("Unlock: final real shipment, persisted threshold/table row, purchase/cancel, crop art/daily/harvest/regrowth and shipment passed.");return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

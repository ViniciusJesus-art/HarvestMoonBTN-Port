/* Installed-data traversal through current input/resource adapters. Daily rules
 * themselves are also tested through public session APIs without SDL. */
#include "poultry_helpers.h"
#include "save_file.h"
#include "field_helpers.h"
typedef struct CropCase {
    const char *option,*name;
    unsigned season,seed,product,price,sale,selection,grow_days,first_stage,save_day;
    uint8_t stages[14];
    unsigned regrow_stage,regrow_days;
} CropCase;
static const CropCase crops[]={
    {"", "Turnip", 0,0x22,0x41,120,60,1,4,2,0,{0,1,1,2}},
    {"--potato", "Potato", 0,0x23,0x42,150,80,2,7,3,4,{0,0,1,1,1,1,2}},
    {"--tomato", "Tomato", 1,0x27,0x46,200,60,1,9,2,5,{0,1,1,2,2,3,3,3,4},3,3},
    {"--corn", "Corn", 1,0x28,0x47,300,100,2,14,3,8,{0,0,1,1,1,1,2,2,2,2,3,3,3,4},3,3},
    {"--onion", "Onion", 1,0x29,0x48,150,80,3,8,3,4,{0,0,1,1,1,1,2,3}},
    {"--eggplant", "Eggplant", 2,0x2c,0x4b,120,80,1,9,3,4,{0,0,1,1,1,2,2,2,3},2,3},
    {"--carrot", "Carrot", 2,0x2d,0x4c,300,120,2,7,3,4,{0,0,1,1,1,1,2}},
    {"--sweet-potato", "Sweet potato", 2,0x2e,0x4d,300,120,3,5,3,4,{0,0,1,1,2},1,2},
};
/* Keep the manual watering / unwatered-night traversal deterministic. The
 * weather playthrough exercises real forecasts and rainy mornings separately. */
static bool sleep_dry_day(HmbtnScenePreview *s,char *error,size_t size)
{
    hmbtn_scene_preview_session(s)->game.weather.tomorrow=HMBTN_WEATHER_SUNNY;
    return sleep_day(s,error,size);
}
int main(int argc,char **argv)
{
    unsigned which=0;
    if(argc>=3) for(unsigned i=1;i<sizeof(crops)/sizeof(*crops);++i)
        if(!strcmp(argv[2],crops[i].option)) which=i;
    if((!which && (argc<2 || argc>3)) || (which && (argc<4 || argc>5))) return 2;
    const CropCase *test=&crops[which];
    const unsigned season=test->season,seed=test->seed,product=test->product;
    const unsigned price=test->price,sale=test->sale,grow_days=test->grow_days;
    const unsigned first_stage=test->first_stage,selection=test->selection;
    const char *save_path=which ? argv[3] : NULL;
    const bool regrows=test->regrow_days!=0;
    const unsigned grown=test->stages[grow_days-1];
    const int approach_x=season==0 ? 0 : season==1 ? -400 : 400;
    const int approach_z=season==0 ? -400 : 0;
    const unsigned table_facing=season==0 ? 2u : season==1 ? 3u : 1u;
    /* Fall 3 is a dated event and Jeff is away. Use Fall 1 (Thursday),
     * preserving actual business/merchant gates instead of forcing presence. */
    const unsigned business_weekday=season==2 ? 4u : 3u;
    const unsigned start_day=season==2 ? 0u : 2u;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};
    const char *shots=which ? (argc==5 ? argv[4] : NULL) : (argc==3 ? argv[2] : NULL);
    CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock start={0,season,start_day,business_weekday,12,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&start,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    /* Controlled purse funds the quantity-three corn purchase. Keep the spring
     * baseline unchanged and isolate long crop runs from Thomas's own test. */
    if(season) {
        hmbtn_scene_preview_session(s)->game.economy.gold=1000;
        hmbtn_scene_preview_session(s)->village.flags[251+288]=true;
        /* Duke invitation has its own traversal; crop fixtures start after it. */
        if(season==2) hmbtn_scene_preview_session(s)->village.flags[251+140]=true;
    }
    /* Original delivery counter must not sell seeds; table bundles resolve
     * distinct estate keys through the same long-facing probe as the source. */
    CHECK(hmbtn_scene_preview_load_area(s,180,error,sizeof(error)));hmbtn_scene_preview_session(s)->intro.area=180;
    CHECK(step(s,0,error,sizeof(error)));
    const uint8_t *delivery=hmbtn_scene_preview_resources(s).section+16+2*44;
    CHECK(hmbtn_read_le16(delivery+2)==36);
    at(s,delivery);hmbtn_scene_preview_session(s)->intro.player.position[2]-=224;
    CHECK(!hmbtn_seed_shop_open(&hmbtn_scene_preview_session(s)->feed_shop,&hmbtn_scene_preview_session(s)->village,&hmbtn_scene_preview_session(s)->intro.player,
        &hmbtn_scene_preview_session(s)->game.clock,180,hmbtn_scene_preview_resources(s).section,0x2000,0));
    const unsigned record_ids[3]={112,113,114},keys[3]={0x128,0x129,0x12a};
    for(unsigned i=0;i<3;++i) {
        if(i==season) continue;
        const HmbtnGatherRecord *bundle=hmbtn_farm_gather_record(record_ids[i]);
        CHECK(bundle && bundle->item==keys[i] && bundle->location==30);
        hmbtn_scene_preview_session(s)->intro.player.position[0]=bundle->position[0]+(i==0 ? 0 : i==1 ? -400 : 400);
        hmbtn_scene_preview_session(s)->intro.player.position[1]=0;hmbtn_scene_preview_session(s)->intro.player.position[2]=bundle->position[2]-(i==0 ? 400 : 0);
        hmbtn_scene_preview_session(s)->intro.player.facing=i==0 ? 2 : i==1 ? 3 : 1;
        CHECK(!hmbtn_seed_shop_open(&hmbtn_scene_preview_session(s)->feed_shop,&hmbtn_scene_preview_session(s)->village,&hmbtn_scene_preview_session(s)->intro.player,
            &hmbtn_scene_preview_session(s)->game.clock,180,hmbtn_scene_preview_resources(s).section,0x2000,0));
    }
    const HmbtnGatherRecord *bundle=hmbtn_farm_gather_record(record_ids[season]);
    CHECK(bundle && bundle->item==keys[season] && bundle->location==30);
    hmbtn_scene_preview_session(s)->intro.player.position[0]=bundle->position[0]+approach_x;hmbtn_scene_preview_session(s)->intro.player.position[1]=0;
    hmbtn_scene_preview_session(s)->intro.player.position[2]=bundle->position[2]+approach_z;hmbtn_scene_preview_session(s)->intro.player.facing=table_facing;
    memset(hmbtn_scene_preview_session(s)->intro.player.velocity,0,sizeof(hmbtn_scene_preview_session(s)->intro.player.velocity));
    const unsigned jeff_facing=hmbtn_scene_preview_session(s)->village.people[1].actor.facing;
    memcpy(hmbtn_scene_preview_session(s)->intro.camera,hmbtn_scene_preview_session(s)->intro.player.position,sizeof(hmbtn_scene_preview_session(s)->intro.camera));
    CHECK((*hmbtn_scene_preview_resources(s).estate_art).row==8 && hmbtn_estate_art_sprite(&(*hmbtn_scene_preview_resources(s).estate_art),15+season));
    CHECK(capture(s,&host,shots,"seed-table-display",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->feed_shop.seeds && hmbtn_scene_preview_session(s)->feed_shop.request==(season==0 ? 0xBEu : season==1 ? 0x273u : 0x274u));
    CHECK(shop_wait(s,HMBTN_SHOP_GREETING,error,sizeof(error)));
    CHECK(capture(s,&host,shots,"seed-greeting",error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->village.people[1].actor.facing==jeff_facing);
    const unsigned money=hmbtn_scene_preview_session(s)->game.economy.gold;
    CHECK(capture(s,&host,shots,"seed-table",error,sizeof(error)));
    CHECK(hmbtn_seed_shop_entries(season)==4 && hmbtn_scene_preview_session(s)->feed_shop.item==54);
    for(unsigned row=0;row<selection;++row) CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->feed_shop.item==seed);
    CHECK(step(s,HMBTN_PAD_TRIANGLE,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_INFO,error,sizeof(error)) && hmbtn_scene_preview_session(s)->feed_shop.request==hmbtn_seed_shop_entry(season,selection)->description);
    CHECK(capture(s,&host,shots,"seed-info",error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_OFFER,error,sizeof(error)) && hmbtn_scene_preview_session(s)->feed_shop.request==0xC0);
    CHECK(capture(s,&host,shots,"seed-chosen",error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_COUNT,error,sizeof(error)) && hmbtn_scene_preview_session(s)->feed_shop.request==0x4D);
    CHECK(step(s,HMBTN_PAD_LEFT,error,sizeof(error)) && hmbtn_scene_preview_session(s)->feed_shop.quantity==99);
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && hmbtn_scene_preview_session(s)->feed_shop.request==0x4F);
    CHECK(hmbtn_scene_preview_session(s)->game.economy.gold==money && !hmbtn_scene_preview_session(s)->game.inventory.equipped.id);
    CHECK(shop_wait(s,HMBTN_SHOP_COUNT,error,sizeof(error)) && hmbtn_scene_preview_session(s)->feed_shop.quantity==99);
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_CANCEL_COUNT,error,sizeof(error)) && hmbtn_scene_preview_session(s)->feed_shop.request==0xBA);
    CHECK(shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && hmbtn_scene_preview_session(s)->feed_shop.request==0xC2);
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)) && hmbtn_scene_preview_session(s)->feed_shop.selection==selection);
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_COUNT,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->feed_shop.quantity==1 && hmbtn_scene_preview_session(s)->game.economy.gold==money);
    CHECK(step(s,HMBTN_PAD_RIGHT,error,sizeof(error)) && step(s,HMBTN_PAD_RIGHT,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->feed_shop.quantity==3 && hmbtn_feed_shop_cost(&hmbtn_scene_preview_session(s)->feed_shop)==price*3);
    CHECK(capture(s,&host,shots,"seed-quantity",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->game.inventory.equipped.id==seed && hmbtn_scene_preview_session(s)->game.inventory.equipped.count==3
        && hmbtn_scene_preview_session(s)->game.economy.gold==money-price*3 && hmbtn_scene_preview_session(s)->game.economy.ledger[0][1]==price*3);
    CHECK(shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && hmbtn_scene_preview_session(s)->feed_shop.request==0xBB);
    CHECK(capture(s,&host,shots,"seed-purchased",error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_OFF,error,sizeof(error)));
    /* Use installed terrain, action ANI, and normal input for all field effects. */
    CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)));hmbtn_scene_preview_session(s)->intro.area=0;
    const unsigned index=10*45+10;
    /* A clear prepared fixture removes random layout debris, not a planted crop. */
    for(unsigned r=9;r<=11;++r) for(unsigned c=9;c<=11;++c)
        hmbtn_scene_preview_session(s)->field.cells[r*45+c]=(HmbtnFarmTile){.value=0x167,.ground=0x167};
    cell(s,10,10);CHECK(tool(s,6,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->field.cells[index].value==0x168);
    /* Restore the purchased packet from the box used while equipping the hoe. */
    cell(s,11,10);CHECK(tool(s,seed,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->field.cells[index].value==seed && hmbtn_scene_preview_session(s)->field.cells[index].ground==0x16a);
    CHECK(hmbtn_scene_preview_session(s)->game.inventory.equipped.id==seed && hmbtn_scene_preview_session(s)->game.inventory.equipped.count==2
        && !hmbtn_scene_preview_session(s)->farm_action.bank_pending);
    CHECK(capture(s,&host,shots,"sown",error,sizeof(error)));
    /* Refill the starting can at an actual water primitive, then water every
     * day. The animation driver, never the test, determines completion. */
    CHECK(field_refill(s,error,sizeof(error)));
    for(unsigned day=0;day<grow_days;++day) {
        cell(s,10,10);CHECK(tool(s,21,error,sizeof(error)));
        CHECK(hmbtn_scene_preview_session(s)->field.cells[index].ground==0x16b || hmbtn_scene_preview_session(s)->field.cells[index].ground==0x169);
        CHECK(sleep_dry_day(s,error,sizeof(error)));
        CHECK(hmbtn_scene_preview_session(s)->field.cells[index].kind==test->stages[day]);
        if(which && day==1) {
            /* Actual unwatered night between the second and third watered days. */
            const HmbtnFarmTile before=hmbtn_scene_preview_session(s)->field.cells[index];
            CHECK(sleep_dry_day(s,error,sizeof(error)));
            CHECK(hmbtn_scene_preview_session(s)->field.cells[index].kind==before.kind
                && hmbtn_scene_preview_session(s)->field.cells[index].watered_days==before.watered_days);
        }
        if(save_path && day+1==test->save_day) {
            CHECK(hmbtn_scene_preview_session(s)->field.cells[index].watered_days==1);
            CHECK(field_reload(&s,&host,argv[1],save_path,error,sizeof(error)));
            CHECK(hmbtn_scene_preview_session(s)->field.cells[index].watered_days==1);
        }
        const unsigned stage=hmbtn_scene_preview_session(s)->field.cells[index].kind;
        if(stage) {
            unsigned sprite,animation;
            CHECK(hmbtn_estate_crop_look(seed,stage,season,&sprite,&animation)
                && sprite==(season==0 ? 7u+seed-0x22u : season==1 ? 12u+seed-0x27u : 12u+seed-0x2cu) && animation==stage+2);
        }
        cell(s,10,10);memcpy(hmbtn_scene_preview_session(s)->intro.camera,hmbtn_scene_preview_session(s)->intro.player.position,sizeof(hmbtn_scene_preview_session(s)->intro.camera));
        CHECK(capture(s,&host,shots,day+1==first_stage ? "sprout" : day+1==grow_days ? "ripe" : "growing",error,sizeof(error)));
    }
    cell(s,10,10);CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && idle(s,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->game.inventory.held_item.id==product && (regrows
        ? hmbtn_scene_preview_session(s)->field.cells[index].value==seed
            && hmbtn_scene_preview_session(s)->field.cells[index].kind==test->regrow_stage
            && (hmbtn_scene_preview_session(s)->field.cells[index].growth_flags&0x8000)
        : hmbtn_scene_preview_session(s)->field.cells[index].value!=seed));
    CHECK(ship(s,error,sizeof(error)) && hmbtn_scene_preview_session(s)->game.economy.shipping.pending_value==sale);
    /* Payment can occur during sleep; check full native observer and uniqueness. */
    const unsigned before=hmbtn_scene_preview_session(s)->game.economy.gold;
    CHECK(sleep_dry_day(s,error,sizeof(error)));
    if(seed==0x29 || seed==0x2d) {
        /* Onion ships on Summer 12; carrot on Fall 9. Both are original
         * shipping_special_dates, so value must remain pending for a day. */
        CHECK(hmbtn_scene_preview_session(s)->game.clock.day==(seed==0x29 ? 12u : 9u)
            && hmbtn_scene_preview_session(s)->game.economy.gold==before
            && !hmbtn_scene_preview_session(s)->game.economy.ledger[0][0]
            && hmbtn_scene_preview_session(s)->game.economy.shipping.pending_value==sale);
        CHECK(sleep_dry_day(s,error,sizeof(error)));
    }
    CHECK(hmbtn_scene_preview_session(s)->game.economy.gold==before+sale && hmbtn_scene_preview_session(s)->game.economy.ledger[0][0]==sale
        && hmbtn_scene_preview_session(s)->game.economy.shipments[product-0x41]==1 && !hmbtn_scene_preview_session(s)->game.economy.shipping.pending_value);
    CHECK(capture(s,&host,shots,"sold",error,sizeof(error)));
    bool packets_preserved=false;
    for(unsigned i=0;i<64;++i) if(hmbtn_scene_preview_session(s)->game.inventory.toolbox[i].id==seed
            && hmbtn_scene_preview_session(s)->game.inventory.toolbox[i].count==2) packets_preserved=true;
    CHECK(packets_preserved && hmbtn_inventory_count(&hmbtn_scene_preview_session(s)->game.inventory,(int)seed,0,2)==1
        && hmbtn_scene_preview_session(s)->game.economy.gold==money-price*3+sale && hmbtn_scene_preview_session(s)->game.economy.ledger[0][1]==price*3);
    if(regrows) {
        /* The unwatered payment night must not consume skip-day. Save/load this
         * second state, then clear skip and complete the expected regrowth days. */
        CHECK(hmbtn_scene_preview_session(s)->field.cells[index].kind==test->regrow_stage
            && (hmbtn_scene_preview_session(s)->field.cells[index].growth_flags&0x8000));
        CHECK(field_reload(&s,&host,argv[1],save_path,error,sizeof(error)));
        for(unsigned day=0;day<=test->regrow_days;++day) {
            cell(s,10,10);CHECK(tool(s,21,error,sizeof(error)) && sleep_dry_day(s,error,sizeof(error)));
            CHECK(hmbtn_scene_preview_session(s)->field.cells[index].kind==(day<test->regrow_days ? test->regrow_stage : grown));
            CHECK(hmbtn_scene_preview_session(s)->field.cells[index].watered_days==(day<test->regrow_days ? day : 0u));
            if(day==1) CHECK(field_reload(&s,&host,argv[1],save_path,error,sizeof(error)));
        }
        cell(s,10,10);CHECK(step(s,0,error,sizeof(error)) && capture(s,&host,shots,"regrown",error,sizeof(error)));
        CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && idle(s,error,sizeof(error)));
        CHECK(hmbtn_scene_preview_session(s)->game.inventory.held_item.id==product);
        CHECK(ship(s,error,sizeof(error)) && sleep_dry_day(s,error,sizeof(error)));
        CHECK(hmbtn_scene_preview_session(s)->game.economy.gold==money-price*3+sale*2
            && hmbtn_scene_preview_session(s)->game.economy.ledger[0][0]==sale*2
            && hmbtn_scene_preview_session(s)->game.economy.shipments[product-0x41]==2
            && !hmbtn_scene_preview_session(s)->game.economy.shipping.pending_value);
    }
    /* Independent full-inventory fixture after the complete crop traversal:
     * verify actual overflow/error records, especially bank 3's storage notice. */
    hmbtn_scene_preview_session(s)->game.clock=(HmbtnFarmClock){0,season,start_day,business_weekday,12,0,0};
    CHECK(hmbtn_scene_preview_load_area(s,180,error,sizeof(error)));hmbtn_scene_preview_session(s)->intro.area=180;
    CHECK(step(s,0,error,sizeof(error)));
    hmbtn_scene_preview_session(s)->intro.player.position[0]=bundle->position[0]+approach_x;hmbtn_scene_preview_session(s)->intro.player.position[1]=0;
    hmbtn_scene_preview_session(s)->intro.player.position[2]=bundle->position[2]+approach_z;hmbtn_scene_preview_session(s)->intro.player.facing=table_facing;
    memset(hmbtn_scene_preview_session(s)->intro.player.velocity,0,sizeof(hmbtn_scene_preview_session(s)->intro.player.velocity));
    HmbtnInventory *inv=&hmbtn_scene_preview_session(s)->game.inventory;
    inv->equipped=(HmbtnStoredTool){(uint16_t)seed,0,97};
    for(unsigned i=0;i<hmbtn_inventory_capacity(inv);++i) inv->bag[i]=(HmbtnStoredTool){6,0,1};
    for(unsigned i=0;i<64;++i) inv->toolbox[i]=(HmbtnStoredTool){6,0,1};
    hmbtn_scene_preview_session(s)->game.economy.gold=1000;
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    for(unsigned row=0;row<selection;++row) CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_COUNT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_RIGHT,error,sizeof(error)) && step(s,HMBTN_PAD_RIGHT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && hmbtn_scene_preview_session(s)->feed_shop.request==0xC1);
    CHECK(hmbtn_scene_preview_session(s)->game.economy.gold==1000 && inv->equipped.count==97);
    CHECK(shop_wait(s,HMBTN_SHOP_COUNT,error,sizeof(error)));
    inv->toolbox[17]=(HmbtnStoredTool){0};
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && hmbtn_scene_preview_session(s)->feed_shop.bank==3
        && hmbtn_scene_preview_session(s)->feed_shop.request==0x207 && inv->equipped.count==97 && inv->toolbox[17].count==3
        && hmbtn_scene_preview_session(s)->game.economy.gold==1000-price*3);
    CHECK(capture(s,&host,shots,"seed-stored",error,sizeof(error)));
    /* Wait explicitly for BB; shop_wait(RESULT) would also match the notice. */
    for(unsigned i=0;i<3000 && (hmbtn_scene_preview_session(s)->feed_shop.bank!=2 || hmbtn_scene_preview_session(s)->feed_shop.request!=0xBB);++i)
        CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->feed_shop.bank==2 && hmbtn_scene_preview_session(s)->feed_shop.request==0xBB);
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    inv->equipped.count=99;inv->toolbox[17]=(HmbtnStoredTool){6,0,1};
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && hmbtn_scene_preview_session(s)->feed_shop.request==0x54);
    CHECK(shop_wait(s,HMBTN_SHOP_OFF,error,sizeof(error)) && hmbtn_scene_preview_session(s)->game.economy.gold==1000-price*3);
    if(season) {
        HmbtnSession *g=hmbtn_scene_preview_session(s);
        g->game.clock=(HmbtnFarmClock){0,2,season==2 ? 29u : 2u,3,12,0,0};
        g->field.cells[index]=(HmbtnFarmTile){.value=(uint16_t)seed,.ground=0x168,.kind=(uint8_t)grown};
        if(season==1) hmbtn_farm_field_new_day(&g->field,2);
        else { g->field.season=2;g->daily_date=hmbtn_session_date(&g->game.clock); }
        CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)));g->intro.area=0;
        if(season==2) {
            CHECK(sleep_dry_day(s,error,sizeof(error)));
            CHECK(g->game.clock.season==3 && !g->game.clock.day && g->daily_date==hmbtn_session_date(&g->game.clock));
        }
        cell(s,10,10);CHECK(step(s,0,error,sizeof(error)));
        unsigned sprite,animation;
        CHECK(!hmbtn_farm_field_harvest_item(&g->field,index));
        if(season==1) {
            CHECK(g->field.cells[index].kind==1 && hmbtn_estate_crop_look(seed,1,2,&sprite,&animation)
                && sprite==7u+seed-0x27u && !animation);
            CHECK(capture(s,&host,shots,"withered",error,sizeof(error)));
        } else {
            CHECK(g->field.cells[index].value==0x168 && !g->field.cells[index].kind
                && !g->field.cells[index].watered_days && !g->field.cells[index].growth_flags);
            CHECK(!hmbtn_farm_field_plant(&g->field,10,10,seed));
            CHECK(capture(s,&host,shots,"winter",error,sizeof(error)));
            char winter_path[2048];
            CHECK(snprintf(winter_path,sizeof(winter_path),"%s.winter.hms",save_path)<(int)sizeof(winter_path));
            CHECK(field_reload(&s,&host,argv[1],winter_path,error,sizeof(error)));
            CHECK(hmbtn_scene_preview_session(s)->field.cells[index].value==0x168
                && !hmbtn_scene_preview_session(s)->field.cells[index].kind);
        }
        CHECK(!hmbtn_estate_crop_look(seed,0,season,&sprite,&animation)
            && !hmbtn_estate_crop_look(seed,1,0,&sprite,&animation)
            && !hmbtn_estate_crop_look(seed,1,3,&sprite,&animation));
    }
    printf("%s: ",test->name);
    puts("Original table/key selection, seed names, original dialogue, quantity-three purchase, original seed ANI/grid, watering/refill, day updates, crop sprites, harvest, shipping and unique payment passed.");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

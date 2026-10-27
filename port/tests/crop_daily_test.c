#include "session.h"
#include "inventory.h"
#include "psx_data.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Crop daily line %d: %s\n",__LINE__,#x);hmbtn_session_destroy(s);return 1; } } while(0)
/* The original crop vectors deliberately measure dry mornings. Weather/rain
 * has independent integration coverage; do not let random rain water these. */
static unsigned advance_dry(HmbtnSession *s,unsigned minutes)
{
    unsigned changes=0;
    while(minutes) {
        const unsigned step=minutes>1440 ? 1440 : minutes;
        s->game.weather.tomorrow=HMBTN_WEATHER_SUNNY;
        changes|=hmbtn_session_advance_minutes(s,step);minutes-=step;
    }
    return changes;
}
int main(void)
{
    HmbtnSession *s=hmbtn_session_create(NULL,0);CHECK(s);
    HmbtnFarmField *f=&s->field;HmbtnFarmTile *t=&f->cells[460];
    /* PF-06: CROP_POTATO is already extracted from farm_tables.c. Exercise
     * the shared stage/day algorithm rather than introducing another grower. */
    const HmbtnFarmCrop *potato=hmbtn_farm_crop(0x23);
    CHECK(potato && potato->season==0 && potato->grown==2 && potato->regrow==-1
        && potato->days[0]==3 && potato->days[1]==4 && potato->harvest==0x42);
    CHECK(hmbtn_farm_item(0x23)->buy_price==150 && hmbtn_farm_item(0x42)->sell_price==80);
    CHECK(hmbtn_farm_crop_supported(0x23));
    CHECK(!hmbtn_farm_field_plant(f,-1,10,0x23) && !hmbtn_farm_field_plant(f,45,10,0x23));
    f->active[460]=false;*t=(HmbtnFarmTile){.value=0x168,.ground=0x168};
    CHECK(!hmbtn_farm_field_plant(f,10,10,0x23));f->active[460]=true;
    CHECK(hmbtn_farm_field_plant(f,10,10,0x23) && !hmbtn_farm_field_harvest_item(f,460));
    advance_dry(s,1440);CHECK(!t->kind && !t->watered_days);
    for(unsigned watered=1;watered<=7;++watered) {
        CHECK(hmbtn_farm_field_tool(f,10,10,4,0,1,NULL));
        advance_dry(s,1440);
        const unsigned stage=watered<3 ? 0 : watered<7 ? 1 : 2;
        const unsigned days=watered<3 ? watered : watered<7 ? watered-3 : 0;
        CHECK(t->kind==stage && t->watered_days==days && t->ground!=0x16b && t->ground!=0x169);
        CHECK(hmbtn_farm_field_harvest_item(f,460)==(watered==7 ? 0x42u : 0u));
        if(watered==2) {
            advance_dry(s,1440);CHECK(!t->kind && t->watered_days==2);
        }
        hmbtn_session_clock_events(s,&s->game.clock,HMBTN_CLOCK_DAY);
        CHECK(t->kind==stage && t->watered_days==days);
    }
    advance_dry(s,2880);CHECK(t->kind==2 && hmbtn_farm_field_harvest_item(f,460)==0x42);
    CHECK(hmbtn_farm_field_harvest(f,460)==0x42 && !hmbtn_farm_field_harvest(f,460)
        && t->value==t->ground && !t->kind && !t->watered_days && !(t->growth_flags&0x8000));
    *t=(HmbtnFarmTile){.value=0x23,.ground=0x16b,.kind=2,.watered_days=3};
    hmbtn_farm_field_new_day(f,1);CHECK(t->value==0x23 && t->kind==1 && !t->watered_days && !hmbtn_farm_field_harvest_item(f,460));
    *t=(HmbtnFarmTile){.value=0x168,.ground=0x168};CHECK(hmbtn_farm_field_plant(f,10,10,0x23));
    hmbtn_farm_field_new_day(f,1);CHECK(t->value==0x168 && !t->kind);
    hmbtn_farm_field_new_day(f,3);CHECK(!hmbtn_farm_field_plant(f,10,10,0x23));
    /* Independent fixture leaves the existing turnip/cucumber vectors intact. */
    hmbtn_session_destroy(s);s=hmbtn_session_create(NULL,0);CHECK(s);f=&s->field;t=&f->cells[460];
    const HmbtnFarmCrop *turnip=hmbtn_farm_crop(34),*cucumber=hmbtn_farm_crop(36);
    CHECK(turnip && turnip->days[0]==2 && turnip->days[1]==2 && turnip->grown==2
        && turnip->harvest==65 && turnip->regrow==-1 && cucumber && cucumber->regrow==1);
    CHECK(!hmbtn_farm_crop(6) && !hmbtn_farm_crop(65535));
    CHECK(!hmbtn_farm_field_plant(f,10,10,34));
    CHECK(hmbtn_farm_field_tool(f,10,10,1,0,1,NULL));
    CHECK(hmbtn_farm_field_plant(f,10,10,34) && t->ground==0x16a && !t->kind);
    CHECK(!hmbtn_farm_field_plant(f,10,10,34) && !hmbtn_farm_field_harvest(f,460));
    /* Dry nights preserve the stage's accumulated watering days. */
    advance_dry(s,1440);CHECK(!t->kind && !t->watered_days);
    for(unsigned day=0;day<4;++day) {
        CHECK(hmbtn_farm_field_tool(f,10,10,4,0,1,NULL));
        advance_dry(s,1440);
        CHECK(t->kind==(day+1)/2 && t->watered_days==(day+1)%2);
        CHECK(t->ground!=0x169 && t->ground!=0x16b);
        const HmbtnFarmTile after=*t;
        hmbtn_session_clock_events(s,&s->game.clock,HMBTN_CLOCK_MINUTE|HMBTN_CLOCK_HOUR|HMBTN_CLOCK_DAY);
        CHECK(t->kind==after.kind && t->watered_days==after.watered_days);
    }
    CHECK(hmbtn_farm_field_harvest(f,460)==65 && !hmbtn_farm_field_harvest(f,460));
    CHECK(t->value==t->ground && !t->kind && !t->watered_days);

    /* Original regrowth stage and skip bit; ripe crops remain until picked. */
    *t=(HmbtnFarmTile){.value=36,.ground=0x168,.kind=3};
    CHECK(hmbtn_farm_field_harvest(f,460)==67 && t->kind==1 && (t->growth_flags&0x8000));
    CHECK(hmbtn_farm_field_tool(f,10,10,4,0,1,NULL));
    advance_dry(s,1440);CHECK(t->kind==1 && !t->watered_days && !(t->growth_flags&0x8000));
    for(unsigned i=0;i<5;++i) {
        CHECK(hmbtn_farm_field_tool(f,10,10,4,0,1,NULL));advance_dry(s,1440);
    }
    CHECK(t->kind==3 && hmbtn_farm_field_harvest_item(f,460)==67);
    advance_dry(s,2880);CHECK(t->kind==3);

    /* Season rollover follows the original withering rules; winter clears. */
    s->game.clock=(HmbtnFarmClock){0,0,29,1,5,59,0};
    *t=(HmbtnFarmTile){.value=34,.ground=0x16b,.kind=1,.watered_days=1};
    advance_dry(s,1);CHECK(s->game.clock.season==1 && s->game.clock.day==0);
    CHECK(t->value==34 && t->kind==1 && !t->watered_days && !hmbtn_farm_field_harvest_item(f,460));
    *t=(HmbtnFarmTile){.value=0x168,.ground=0x168};
    CHECK(hmbtn_farm_field_plant(f,10,10,34));advance_dry(s,1440);
    CHECK(t->value==0x168 && !t->kind);
    s->game.clock=(HmbtnFarmClock){0,3,29,6,5,59,0};
    *t=(HmbtnFarmTile){.value=34,.ground=0x16b,.kind=1};
    hmbtn_session_clock_events(s,&s->game.clock,HMBTN_CLOCK_DAY);
    CHECK(t->value==0x168 && !hmbtn_farm_field_plant(f,10,10,34));
    advance_dry(s,1);CHECK(s->game.clock.year==1 && !s->game.clock.season);
    /* Time/partial frames remain frozen under a modal pause. */
    s->game.clock.frames=7;
    CHECK(!hmbtn_farm_clock_tick(&s->game.clock,true) && s->game.clock.frames==7);

    /* Production consumes yesterday's feed before clearing the bins; morning
     * preparation is idempotent and does not apply another daily crop update. */
    s->poultry.hens[0]=(HmbtnHen){.occupied=true,.stage=HMBTN_HEN_ADULT,.area=112};
    s->feed.coop[0]=true;
    s->game.clock=(HmbtnFarmClock){1,0,2,2,23,0,0};
    *t=(HmbtnFarmTile){.value=34,.ground=0x16b};
    CHECK(hmbtn_session_morning(s,true));
    CHECK(t->watered_days==1 && !t->kind && s->poultry.hens[0].fed_yesterday && !s->feed.coop[0]);
    const unsigned age=s->poultry.hens[0].growth,day=s->game.clock.day;
    CHECK(!hmbtn_session_morning(s,true) && s->game.clock.day==day && s->poultry.hens[0].growth==age);
    hmbtn_session_clock_events(s,&s->game.clock,HMBTN_CLOCK_DAY);
    CHECK(s->poultry.hens[0].fed_yesterday && s->poultry.hens[0].growth==age && t->watered_days==1);
    hmbtn_session_morning_ready(s);
    CHECK(hmbtn_session_morning(s,true) && t->watered_days==1 && !s->poultry.hens[0].fed_yesterday
        && s->poultry.eggs[0].item==0x122);
    /* Fast-forward processes every crossed day, not just the last morning. */
    hmbtn_session_morning_ready(s);*t=(HmbtnFarmTile){.value=34,.ground=0x16b};
    advance_dry(s,4320);CHECK(!t->kind && t->watered_days==1);
    int x,z;CHECK(hmbtn_farm_seed_cell(34,2,3,&x,&z) && x==1 && z==1);
    CHECK(hmbtn_farm_seed_cell(34,2,1,&x,&z) && x==-1 && z==1);
    CHECK(hmbtn_farm_seed_cell(35,0,0,&x,&z));
    CHECK(hmbtn_farm_seed_cell(37,0,0,&x,&z)); /* Strawberry now has its unlock/crop path. */
    CHECK(!hmbtn_farm_seed_cell(0x31,0,0,&x,&z)); /* Moondrop progression is still pending. */
    /* A complete packet reaches all nine original targets once, at animation
     * completion. Repeating the finished action cannot plant/consume again. */
    for(unsigned seed=34;seed<=35;++seed) {
        for(unsigned r=5;r<=7;++r) for(unsigned c=5;c<=7;++c)
            f->cells[r*45+c]=(HmbtnFarmTile){.value=0x168,.ground=0x168};
        s->game.inventory.equipped=(HmbtnStoredTool){(uint16_t)seed,0,2};
        s->intro.player=(HmbtnIntroActor){.position={-1800,0,-1000},.facing=3};
        hmbtn_farm_action_init(&s->farm_action);
        CHECK(hmbtn_farm_action_tick(&s->farm_action,&s->intro.player,&s->house,f,NULL,HMBTN_PAD_SQUARE,0));
        CHECK(s->farm_action.phase==HMBTN_FARM_PLANT_SEED && s->game.inventory.equipped.count==2);
        CHECK(hmbtn_farm_action_tick(&s->farm_action,&s->intro.player,&s->house,f,NULL,0,0) && s->game.inventory.equipped.count==2);
        s->intro.player.animation_done=true;
        CHECK(hmbtn_farm_action_tick(&s->farm_action,&s->intro.player,&s->house,f,NULL,0,0));
        for(unsigned r=5;r<=7;++r) for(unsigned c=5;c<=7;++c) CHECK(f->cells[r*45+c].value==seed);
        CHECK(s->game.inventory.equipped.count==1 && s->farm_action.stamina==99);
        CHECK(!hmbtn_farm_action_tick(&s->farm_action,&s->intro.player,&s->house,f,NULL,0,0));
        CHECK(hmbtn_farm_action_tick(&s->farm_action,&s->intro.player,&s->house,f,NULL,HMBTN_PAD_SQUARE,0));
        s->intro.player.animation_done=true;
        CHECK(hmbtn_farm_action_tick(&s->farm_action,&s->intro.player,&s->house,f,NULL,0,0)
            && !s->game.inventory.equipped.id && !s->farm_action.changed && s->farm_action.stamina==98);
    }
    /* Synthetic original-format table zone; no installed data or host device.
     * Closed days/hours, wrong floor and occupied hand cannot start a purchase. */
    uint8_t section[0x2000]={0};uint8_t *zone=section+16;
    hmbtn_write_le16(section+2,30);
    hmbtn_write_le16(zone,0x4000);hmbtn_write_le16(zone+2,37);hmbtn_write_le16(zone+8,999);
    hmbtn_write_le16(zone+14,(uint16_t)-600);hmbtn_write_le16(zone+18,(uint16_t)-1400);
    hmbtn_write_le16(zone+20,600);hmbtn_write_le16(zone+24,(uint16_t)-800);
    s->game.clock=(HmbtnFarmClock){1,0,2,3,12,0,0};
    s->village.people[1].actor.visible=true;s->village.people[1].location=30;
    s->intro.player=(HmbtnIntroActor){.position={0,0,-600},.facing=0};
    HmbtnFeedShop *shop=&s->feed_shop;
    CHECK(!hmbtn_seed_shop_open(shop,&s->village,&s->intro.player,&s->game.clock,180,section,sizeof(section),65));
    s->game.clock.weekday=2;CHECK(!hmbtn_seed_shop_open(shop,&s->village,&s->intro.player,&s->game.clock,180,section,sizeof(section),0));
    s->game.clock.weekday=0;CHECK(!hmbtn_seed_shop_open(shop,&s->village,&s->intro.player,&s->game.clock,180,section,sizeof(section),0));
    s->game.clock.weekday=3;s->game.clock.hour=17;
    CHECK(!hmbtn_seed_shop_open(shop,&s->village,&s->intro.player,&s->game.clock,180,section,sizeof(section),0));
    s->game.clock.hour=12;s->intro.player.position[1]=1000;
    CHECK(!hmbtn_seed_shop_open(shop,&s->village,&s->intro.player,&s->game.clock,180,section,sizeof(section),0));
    s->intro.player.position[1]=0;
    hmbtn_write_le16(zone+2,36); /* Old wrong delivery-zone implementation. */
    CHECK(!hmbtn_seed_shop_open(shop,&s->village,&s->intro.player,&s->game.clock,180,section,sizeof(section),0));
    hmbtn_write_le16(zone+2,37);
    s->intro.player.position[0]=150; /* Strict estate search-box boundary. */
    CHECK(!hmbtn_seed_shop_open(shop,&s->village,&s->intro.player,&s->game.clock,180,section,sizeof(section),0));
    s->intro.player.position[0]=0;
    /* Zone probe is 224, record probe 400. Velocity affects only zone lookup. */
    s->intro.player.velocity[2]=1000;
    CHECK(!hmbtn_seed_shop_open(shop,&s->village,&s->intro.player,&s->game.clock,180,section,sizeof(section),0));
    s->intro.player.velocity[2]=0;
    CHECK(hmbtn_seed_shop_open(shop,&s->village,&s->intro.player,&s->game.clock,180,section,sizeof(section),0));
    CHECK(shop->phase==HMBTN_SHOP_GREETING && shop->bank==2 && shop->request==0xBE);
    /* Detailed original menu/quantity/transaction tests: seed_shop_test.c. */
    shop->phase=HMBTN_SHOP_OFF;
    s->game.inventory.equipped=(HmbtnStoredTool){6,0,1};
    s->game.inventory.bag[0]=(HmbtnStoredTool){11,0,1};
    s->game.inventory.bag[1]=(HmbtnStoredTool){0};
    for(unsigned i=0;i<64;++i) s->game.inventory.toolbox[i]=(HmbtnStoredTool){11,0,1};
    /* Same-seed stacks fill before opening a new record; spill follows the
     * original spare toolbox path without another storage implementation. */
    s->game.inventory.equipped=(HmbtnStoredTool){34,0,98};
    CHECK(hmbtn_inventory_add_seed_packet(&s->game.inventory,34,NULL) && s->game.inventory.equipped.count==99);
    CHECK(hmbtn_inventory_add_seed_packet(&s->game.inventory,34,NULL) && s->game.inventory.bag[1].id==34);
    s->game.inventory.bag[1]=(HmbtnStoredTool){34,0,99};s->game.inventory.toolbox[17]=(HmbtnStoredTool){0};
    HmbtnInventoryPlacement placement;
    CHECK(hmbtn_inventory_add_seed_packet(&s->game.inventory,34,&placement)
        && placement.store==HMBTN_STORE_TOOLBOX && placement.index==17 && s->game.inventory.toolbox[17].count==1);
    hmbtn_session_destroy(s);puts("Original crop stages, dry days, regrowth, seasons, ordered daily production and idempotent mornings passed.");return 0;
}

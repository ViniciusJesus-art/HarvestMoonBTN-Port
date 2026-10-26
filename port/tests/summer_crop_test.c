/* Exact daily vectors from farm_tables.c + field_daily_tick (d3c0f38), not
 * durations inferred from a guide. In particular onion's zero-length stage
 * still occupies one daily update in the original single-step algorithm. */
#include "session.h"
#include "inventory.h"
#include "psx_data.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Seasonal rules line %d: %s\n",__LINE__,#x);hmbtn_session_destroy(s);return 1; } } while(0)
typedef struct CropVector {
    unsigned seed,product,price,sale,grown,days;int regrow;
    unsigned durations[4];uint8_t stages[14],progress[14];
} CropVector;
static const CropVector summer_vectors[]={
    {0x27,0x46,200,60,4,9,3,{2,2,2,3},
        {0,1,1,2,2,3,3,3,4},{1,0,1,0,1,0,1,2,0}},
    {0x28,0x47,300,100,4,14,3,{3,4,4,3},
        {0,0,1,1,1,1,2,2,2,2,3,3,3,4},{1,2,0,1,2,3,0,1,2,3,0,1,2,0}},
    {0x29,0x48,150,80,3,8,-1,{3,4,0,0},
        {0,0,1,1,1,1,2,3},{1,2,0,1,2,3,0,0}},
};
static const CropVector fall_vectors[]={
    {0x2c,0x4b,120,80,3,9,2,{3,3,3,0},
        {0,0,1,1,1,2,2,2,3},{1,2,0,1,2,0,1,2,0}},
    {0x2d,0x4c,300,120,2,7,-1,{3,4,0,0},
        {0,0,1,1,1,1,2},{1,2,0,1,2,3,0}},
    {0x2e,0x4d,300,120,2,5,1,{3,2,0,0},
        {0,0,1,1,2},{1,2,0,1,0}},
};
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
int main(int argc,char **argv)
{
    const bool fall=argc==2 && !strcmp(argv[1],"--fall");
    if((!fall && argc!=1) || argc>2) return 2;
    const unsigned season=fall ? 2u : 1u;
    const CropVector *vectors=fall ? fall_vectors : summer_vectors;
    const unsigned count=fall ? sizeof(fall_vectors)/sizeof(*fall_vectors) : sizeof(summer_vectors)/sizeof(*summer_vectors);
    HmbtnSession *s=NULL;
    for(unsigned n=0;n<count;++n) {
        const CropVector *v=&vectors[n];s=hmbtn_session_create(NULL,0);CHECK(s);
        s->game.clock=(HmbtnFarmClock){0,season,2,3,6,0,0};s->daily_date=hmbtn_session_date(&s->game.clock);
        HmbtnFarmField *f=&s->field;f->season=season;HmbtnFarmTile *t=&f->cells[460];
        const HmbtnFarmCrop *crop=hmbtn_farm_crop(v->seed);
        CHECK(hmbtn_farm_crop_supported(v->seed) && crop && crop->season==season
            && crop->grown==v->grown && crop->regrow==v->regrow && crop->harvest==(int)v->product
            && !memcmp(crop->days,v->durations,sizeof(v->durations)));
        CHECK(hmbtn_farm_item(v->seed)->buy_price==(int)v->price && hmbtn_farm_item(v->product)->sell_price==(int)v->sale);
        CHECK(!hmbtn_farm_field_plant(f,-1,10,v->seed) && !hmbtn_farm_field_plant(f,45,10,v->seed));
        *t=(HmbtnFarmTile){.value=0x168,.ground=0x168};f->active[460]=false;
        CHECK(!hmbtn_farm_field_plant(f,10,10,v->seed));f->active[460]=true;
        CHECK(hmbtn_farm_field_plant(f,10,10,v->seed) && !hmbtn_farm_field_plant(f,10,10,v->seed));
        advance_dry(s,1440);CHECK(!t->kind && !t->watered_days);
        for(unsigned day=0;day<v->days;++day) {
            CHECK(!hmbtn_farm_field_harvest(f,460));
            CHECK(hmbtn_farm_field_tool(f,10,10,4,0,1,NULL));
            advance_dry(s,1440);
            CHECK(t->kind==v->stages[day] && t->watered_days==v->progress[day]
                && t->ground!=0x169 && t->ground!=0x16b);
            CHECK(hmbtn_farm_field_harvest_item(f,460)==(day+1==v->days ? v->product : 0));
            hmbtn_session_clock_events(s,&s->game.clock,HMBTN_CLOCK_DAY);
            CHECK(t->kind==v->stages[day] && t->watered_days==v->progress[day]);
            if(day==1) {
                advance_dry(s,1440);
                CHECK(t->kind==v->stages[day] && t->watered_days==v->progress[day]);
            }
        }
        advance_dry(s,1440);CHECK(t->kind==v->grown);
        CHECK(hmbtn_farm_field_harvest(f,460)==v->product && !hmbtn_farm_field_harvest(f,460));
        CHECK(!t->watered_days && !s->game.inventory.held_item.id && !s->game.inventory.supplies[0]);
        if(v->regrow>=0) {
            CHECK(t->value==v->seed && t->kind==(unsigned)v->regrow && (t->growth_flags&0x8000));
            advance_dry(s,1440);CHECK(t->growth_flags&0x8000);
            const unsigned length=v->durations[v->regrow];
            for(unsigned day=0;day<=length;++day) {
                CHECK(hmbtn_farm_field_tool(f,10,10,4,0,1,NULL));advance_dry(s,1440);
                CHECK(t->kind==(day<length ? (unsigned)v->regrow : v->grown) && t->watered_days==(day<length ? day : 0u)
                    && !(t->growth_flags&0x8000));
                CHECK(hmbtn_farm_field_harvest_item(f,460)==(day==length ? v->product : 0));
            }
            CHECK(hmbtn_farm_field_harvest(f,460)==v->product);
        } else CHECK(t->value==t->ground && !t->kind && !t->growth_flags);
        /* Season rollover uses the same daily observer and preserves grass. */
        s->game.clock=(HmbtnFarmClock){0,season,29,1,5,59,0};
        *t=(HmbtnFarmTile){.value=(uint16_t)v->seed,.ground=0x16b,.kind=(uint8_t)v->grown};
        f->cells[461]=(HmbtnFarmTile){.value=0x36,.ground=0x168,.kind=3};
        advance_dry(s,1);
        CHECK(s->game.clock.season==season+1 && !s->game.clock.day && !t->watered_days
            && !hmbtn_farm_field_harvest(f,460));
        if(fall) CHECK(t->value==0x168 && !t->kind && !t->growth_flags
            && f->cells[461].value==0x36 && f->cells[461].kind==1 && f->cells[461].watered_days==1);
        else CHECK(t->value==v->seed && t->kind==1 && f->cells[461].kind==3);
        hmbtn_farm_field_new_day(f,3);CHECK(t->value==0x168 && !t->kind && f->cells[461].value==0x36);
        CHECK(!hmbtn_farm_field_plant(f,10,10,v->seed));
        f->season=0;CHECK(hmbtn_farm_field_plant(f,10,10,v->seed));
        hmbtn_farm_field_new_day(f,0);CHECK(t->value==0x168 && !t->kind);
        f->season=season;
        for(unsigned facing=0;facing<4;++facing) {
            for(unsigned r=5;r<=7;++r) for(unsigned c=5;c<=7;++c)
                f->cells[r*45+c]=(HmbtnFarmTile){.value=0x168,.ground=0x168};
            s->game.inventory.equipped=(HmbtnStoredTool){(uint16_t)v->seed,0,2};
            s->intro.player=(HmbtnIntroActor){.position={-1800,0,-1000},.facing=facing};
            hmbtn_farm_action_init(&s->farm_action);
            CHECK(hmbtn_farm_action_tick(&s->farm_action,&s->intro.player,&s->house,f,NULL,HMBTN_PAD_SQUARE,0));
            CHECK(s->game.inventory.equipped.count==2);
            s->intro.player.animation_done=true;
            CHECK(hmbtn_farm_action_tick(&s->farm_action,&s->intro.player,&s->house,f,NULL,0,0));
            for(unsigned r=5;r<=7;++r) for(unsigned c=5;c<=7;++c) CHECK(f->cells[r*45+c].value==v->seed);
            CHECK(s->game.inventory.equipped.count==1);
            CHECK(!hmbtn_farm_action_tick(&s->farm_action,&s->intro.player,&s->house,f,NULL,0,0)
                && s->game.inventory.equipped.count==1);
        }
        hmbtn_session_destroy(s);s=NULL;
    }
    /* Correct seasonal record key, never the delivery counter/another bundle. */
    s=hmbtn_session_create(NULL,0);CHECK(s);
    uint8_t section[0x2000]={0},*zone=section+16;
    hmbtn_write_le16(section+2,30);hmbtn_write_le16(zone,0x4000);hmbtn_write_le16(zone+2,37);
    hmbtn_write_le16(zone+14,(uint16_t)-1000);hmbtn_write_le16(zone+18,(uint16_t)-1600);
    hmbtn_write_le16(zone+20,1000);hmbtn_write_le16(zone+24,(uint16_t)-100);
    s->game.clock=(HmbtnFarmClock){0,season,2,3,12,0,0};
    s->village.people[1].actor.visible=true;s->village.people[1].location=30;
    s->intro.player=(HmbtnIntroActor){.position={0,0,-600},.facing=0};
    CHECK(!hmbtn_seed_shop_open(&s->feed_shop,&s->village,&s->intro.player,&s->game.clock,180,section,sizeof(section),0));
    s->intro.player=(HmbtnIntroActor){.position={fall ? 656 : -656,0,-580},.facing=fall ? 1u : 3u};
    CHECK(hmbtn_seed_shop_open(&s->feed_shop,&s->village,&s->intro.player,&s->game.clock,180,section,sizeof(section),0));
    CHECK(s->feed_shop.request==(fall ? 0x274u : 0x273u) && s->feed_shop.bank==2 && s->feed_shop.clock==&s->game.clock);
    s->feed_shop.phase=HMBTN_SHOP_OFF;s->game.clock.season=0;
    CHECK(!hmbtn_seed_shop_open(&s->feed_shop,&s->village,&s->intro.player,&s->game.clock,180,section,sizeof(section),0));
    s->game.clock.season=season;hmbtn_write_le16(zone+2,36);
    CHECK(!hmbtn_seed_shop_open(&s->feed_shop,&s->village,&s->intro.player,&s->game.clock,180,section,sizeof(section),0));
    CHECK(hmbtn_seed_shop_entries(2)==4 && !hmbtn_seed_shop_entries(3) && !hmbtn_seed_shop_entry(1,4));
    hmbtn_session_destroy(s);
    printf("%s: original crop vectors, dry nights, regrowth, seasonal rollover, nine-cell actions and table key passed.\n",fall ? "Fall" : "Summer");return 0;
}

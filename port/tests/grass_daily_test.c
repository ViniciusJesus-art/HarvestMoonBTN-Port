/* Grass rules adapted from field_daily_tick / field_tile_harvest, d3c0f38.
 * Test the shared field, daily coordinator and native save without SDL/assets. */
#include "save_fixture.h"
#include "psx_data.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Grass line %d: %s; %s\n",__LINE__,#x,error);goto fail; } } while(0)
int main(void)
{
    char error[512]={0};uint8_t *bytes=NULL;size_t size=0;
    HmbtnSession *s=save_fixture(0),*loaded=NULL;
    HmbtnFarmField *f=&s->field;hmbtn_farm_field_init(f,NULL,0);
    s->game.clock=(HmbtnFarmClock){0,0,2,2,6,0,0};s->daily_date=hmbtn_session_date(&s->game.clock);
    unsigned *stock=s->game.inventory.supplies;stock[0]=0;
    const HmbtnFarmCrop *crop=hmbtn_farm_crop(0x36);
    CHECK(crop && crop->season==4 && crop->grown==3 && crop->regrow==1 && crop->harvest==-1
        && crop->days[0]==3 && crop->days[1]==4 && crop->days[2]==4);
    CHECK(hmbtn_farm_item(0x36)->buy_price==500 && hmbtn_farm_crop_supported(0x36));
    const unsigned index=460;HmbtnFarmTile *t=&f->cells[index];
    CHECK(!hmbtn_farm_field_plant(f,10,10,0x36));
    CHECK(hmbtn_farm_field_tool(f,10,10,1,0,1,stock) && hmbtn_farm_field_plant(f,10,10,0x36));
    CHECK(!hmbtn_farm_field_plant(f,10,10,0x36));
    for(unsigned day=1;day<=11;++day) {
        CHECK(!hmbtn_farm_field_tool(f,10,10,0,0,1,stock) && !stock[0]);
        hmbtn_session_advance_minutes(s,1440);
        const unsigned stage=day<3 ? 0 : day<7 ? 1 : day<11 ? 2 : 3;
        const unsigned progress=day<3 ? day : day<7 ? day-3 : day<11 ? day-7 : 0;
        CHECK(t->value==0x36 && t->kind==stage && t->watered_days==progress);
        if(stage) CHECK(!hmbtn_farm_field_tool(f,10,10,4,0,1,stock));
        CHECK(!hmbtn_farm_field_harvest_item(f,index) && !hmbtn_farm_field_harvest(f,index));
        hmbtn_session_clock_events(s,&s->game.clock,HMBTN_CLOCK_DAY);
        CHECK(t->kind==stage && t->watered_days==progress && !stock[0]);
    }
    CHECK(!hmbtn_farm_field_tool(f,10,10,0,0,1,NULL) && t->kind==3);
    CHECK(hmbtn_farm_field_tool(f,10,10,0,0,1,stock));
    CHECK(stock[0]==1 && t->value==0x36 && t->kind==1 && !t->watered_days && (t->growth_flags&0x8000));
    CHECK(!hmbtn_farm_field_tool(f,10,10,0,0,1,stock) && stock[0]==1);
    CHECK(hmbtn_save_encode(s,&bytes,&size,error,sizeof(error)));
    loaded=hmbtn_save_decode(bytes,size,error,sizeof(error));CHECK(loaded);free(bytes);bytes=NULL;
    CHECK(loaded->game.inventory.supplies[0]==1 && loaded->field.cells[index].kind==1
        && loaded->field.cells[index].growth_flags==t->growth_flags);
    for(unsigned day=0;day<9;++day) {
        hmbtn_session_advance_minutes(s,1440);hmbtn_session_advance_minutes(loaded,1440);
        CHECK(t->kind==loaded->field.cells[index].kind && t->watered_days==loaded->field.cells[index].watered_days
            && t->growth_flags==loaded->field.cells[index].growth_flags);
        if(day==0) CHECK(t->kind==1 && !t->watered_days && !(t->growth_flags&0x8000));
        if(day<8) CHECK(!hmbtn_farm_field_tool(f,10,10,0,0,1,stock));
    }
    CHECK(t->kind==3 && hmbtn_farm_field_tool(f,10,10,0,0,1,stock) && stock[0]==2);
    t->kind=3;stock[0]=998;CHECK(hmbtn_farm_field_tool(f,10,10,0,0,1,stock) && stock[0]==999);
    t->kind=3;CHECK(hmbtn_farm_field_tool(f,10,10,0,0,1,stock) && stock[0]==999 && t->kind==1);
    /* ANY-season grass survives summer/fall, unlike spring crops. Winter
     * resets established grass to stage 1/day 1 every day, then spring resumes. */
    t->kind=3;t->growth_flags=0x8007;t->watered_days=0;
    hmbtn_farm_field_new_day(f,1);CHECK(t->value==0x36 && t->kind==3);
    hmbtn_farm_field_new_day(f,2);CHECK(t->kind==3);
    for(unsigned day=0;day<30;++day) {
        hmbtn_farm_field_new_day(f,3);
        CHECK(t->value==0x36 && t->kind==1 && t->watered_days==1 && !t->growth_flags);
        CHECK(!hmbtn_farm_field_tool(f,10,10,0,0,1,stock));
    }
    CHECK(!hmbtn_farm_field_plant(f,11,10,0x36));
    for(unsigned day=0;day<7;++day) hmbtn_farm_field_new_day(f,0);
    CHECK(t->kind==3);
    /* Original tile-rule table: stage 0/1 can be hoed, stage 2 cannot be cut
     * or hoed, stage 3 can be cut. All grass stages are walkable. */
    const int32_t position[3]={-200,0,-2600};
    for(unsigned stage=0;stage<=3;++stage) {
        *t=(HmbtnFarmTile){.value=0x36,.ground=0x168,.kind=(uint8_t)stage};
        CHECK(!hmbtn_farm_field_probe(f,position));
        CHECK(hmbtn_farm_field_tool(f,10,10,1,0,1,stock)==(stage<2));
    }
    /* Derive terrain shape/UV from current neighbours, including map edges;
     * draw/retexture must not change gameplay state. Synthetic one-quad TMD. */
    uint8_t terrain[16]={0};hmbtn_write_le16(terrain+2,0x2d01);f->quad[index]=0;
    *t=(HmbtnFarmTile){.value=0x36,.ground=0x168,.kind=3};
    const HmbtnFarmTile before=*t;
    CHECK(hmbtn_farm_field_retexture(f,terrain,sizeof(terrain)));
    CHECK(hmbtn_read_le16(terrain+10)==29 && hmbtn_read_le16(terrain+6)==0x67f4 && terrain[4]==0 && terrain[5]==255);
    for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx)
        f->cells[(10+dy)*45+10+dx]=(HmbtnFarmTile){.value=0x36,.ground=0x168,.kind=3};
    CHECK(hmbtn_farm_field_retexture(f,terrain,sizeof(terrain)) && terrain[4]==192 && terrain[5]==255);
    CHECK(t->value==before.value && t->kind==before.kind && t->growth_flags==before.growth_flags && !t->watered_days);
    f->season=2;CHECK(hmbtn_farm_field_retexture(f,terrain,sizeof(terrain)) && hmbtn_read_le16(terrain+6)==0x67f5);
    f->season=3;CHECK(hmbtn_farm_field_retexture(f,terrain,sizeof(terrain)) && hmbtn_read_le16(terrain+10)==7);
    f->quad[index]=SIZE_MAX;f->quad[0]=0;f->season=0;
    f->cells[0]=(HmbtnFarmTile){.value=0x36,.ground=0x168,.kind=1};
    CHECK(hmbtn_farm_field_retexture(f,terrain,sizeof(terrain)) && hmbtn_read_le16(terrain+10)==29);
    HmbtnFieldEffect effects[HMBTN_FIELD_EFFECTS]={0};HmbtnAnimation art[7]={0},cut={0};
    uint8_t animation[16]={0};hmbtn_write_le16(animation+4,3);
    cut.data=animation;cut.count[3]=2;
    CHECK(hmbtn_field_effect_submit(effects,0x36,10,10) && effects[0].art==HMBTN_FIELD_EFFECT_GRASS);
    for(unsigned i=0;i<8;++i) { hmbtn_field_effect_tick(effects,art,&cut);CHECK(effects[0].active); }
    hmbtn_field_effect_tick(effects,art,&cut);CHECK(!effects[0].active);
    CHECK(hmbtn_field_effect_submit(effects,0x36,10,10));
    hmbtn_field_effect_tick(effects,art,NULL);CHECK(!effects[0].active);
    puts("Grass: unwatered growth, single cut/stock, regrowth, save continuation, cap, seasons, tools, collision and derived terrain passed.");
    hmbtn_session_destroy(loaded);hmbtn_session_destroy(s);return 0;
fail:
    free(bytes);hmbtn_session_destroy(loaded);hmbtn_session_destroy(s);return 1;
}

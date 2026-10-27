/* field_event_dispatch / field_daily_tick, farm_tables.c and rand_range.
 * Expected draws use fixed source vectors, not the production RNG helper. */
#include "save_fixture.h"
#include "farm_shipping.h"
#include "item_reaction.h"
#include <stdlib.h>
#include <stdio.h>
static HmbtnSession *autumn(void)
{
    HmbtnSession *s=save_fixture(0);
    s->game.clock.season=s->field.season=2;s->daily_date=hmbtn_session_date(&s->game.clock);
    s->game.inventory.held_item=(HmbtnStoredTool){0};
    return s;
}
static HmbtnSession *roundtrip(HmbtnSession *s)
{
    uint8_t *data=NULL;size_t bytes=0;char error[256]={0};
    assert(hmbtn_save_encode(s,&data,&bytes,error,sizeof(error)));
    HmbtnSession *loaded=hmbtn_save_decode(data,bytes,error,sizeof(error));free(data);assert(loaded);
    static const uint8_t schedules[1]={0};
    assert(hmbtn_session_bind_schedules(loaded,schedules,sizeof(schedules)));
    assert(loaded->field.plant_random==s->field.plant_random
        && !memcmp(loaded->field.cells,s->field.cells,sizeof(s->field.cells)));
    return loaded;
}
int main(void)
{
    /* The actual source generator maps initial 0 to red, 1 to blue. Other
     * gameplay RNGs must not be consumed by planting, dry days or loading. */
    const unsigned initial[]={0,1},variants[]={0x179,0x178},products[]={0x54,0x53};
    const uint32_t next[]={12345,0x41c67ea6};
    const unsigned stage[]={0,0,1,1,1,2,2,3,3,4},progress[]={1,2,0,1,2,0,1,0,1,0};
    for(unsigned n=0;n<2;++n) {
        HmbtnSession *s=autumn();assert(s->field.plant_random==1);
        HmbtnFarmField *f=&s->field;f->plant_random=initial[n];
        f->cells[460]=(HmbtnFarmTile){.value=0x168,.ground=0x168};
        HmbtnSession *before=roundtrip(s);
        assert(hmbtn_farm_field_plant(f,10,10,0x33));
        assert(hmbtn_farm_field_plant(&before->field,10,10,0x33));
        assert(f->cells[460].value==variants[n] && f->plant_random==next[n]);
        assert(before->field.cells[460].value==variants[n] && before->field.plant_random==next[n]);
        hmbtn_session_destroy(before);
        const HmbtnFarmCrop *crop=hmbtn_farm_crop(variants[n]);
        assert(crop && crop->season==2 && crop->grown==4 && crop->regrow==-1 && crop->harvest==(int)products[n]);
        assert(hmbtn_farm_item(0x33)->buy_price==600 && hmbtn_farm_item(products[n])->sell_price==(n ? 0 : 200));
        /* picture_codes.c picture 125, second octal word 01020110122. */
        assert(hmbtn_item_villager_reaction(products[n],18)==0);
        for(unsigned d=0;d<10;++d) {
            const HmbtnFarmTile tile=f->cells[460];hmbtn_farm_field_new_day(f,2);
            assert(f->cells[460].kind==tile.kind && f->cells[460].watered_days==tile.watered_days);
            assert(hmbtn_farm_field_water(f,460));hmbtn_farm_field_new_day(f,2);
            assert(f->cells[460].value==variants[n] && f->cells[460].kind==stage[d] && f->cells[460].watered_days==progress[d]);
            assert(f->plant_random==next[n]);
            if(d==4) { HmbtnSession *loaded=roundtrip(s);hmbtn_session_destroy(s);s=loaded;f=&s->field; }
        }
        assert(hmbtn_farm_field_harvest_item(f,460)==products[n]);
        assert(hmbtn_farm_field_harvest(f,460)==products[n] && !hmbtn_farm_field_harvest(f,460));
        assert(f->cells[460].value==0x168 && !f->cells[460].kind);
        const HmbtnEconomy old=s->game.economy;
        assert(hmbtn_farm_shipping_add(&s->game.economy,products[n])==(n==0));
        assert(s->game.economy.gold==old.gold && !memcmp(old.shipments,s->game.economy.shipments,sizeof(old.shipments)));
        assert(s->game.economy.shipping.pending_value==old.shipping.pending_value+(n ? 0 : 200));
        if(n) assert(!memcmp(&old,&s->game.economy,sizeof(old)));
        /* Continuing a loaded stream gives the same next planted variant. */
        HmbtnSession *loaded=roundtrip(s);
        assert(hmbtn_farm_field_plant(f,10,10,0x33) && hmbtn_farm_field_plant(&loaded->field,10,10,0x33));
        assert(f->plant_random==loaded->field.plant_random && f->cells[460].value==loaded->field.cells[460].value);
        assert(s->village.random==0x10203040 && s->poultry.random==0xfedcba98 && s->fishing.random==0x77665544);
        f->cells[460].kind=4;hmbtn_farm_field_new_day(f,3);
        assert(f->cells[460].value==0x168 && !f->cells[460].kind);
        hmbtn_session_destroy(loaded);hmbtn_session_destroy(s);
    }
    HmbtnSession *s=autumn();HmbtnFarmField *f=&s->field;
    f->cells[460]=(HmbtnFarmTile){.value=0x168,.ground=0x168};
    const uint32_t rng=f->plant_random;
    assert(!hmbtn_farm_field_plant(f,-1,10,0x33) && !hmbtn_farm_field_plant(f,45,10,0x33));
    f->active[460]=false;assert(!hmbtn_farm_field_plant(f,10,10,0x33));f->active[460]=true;
    f->season=3;assert(!hmbtn_farm_field_plant(f,10,10,0x33));f->season=2;
    assert(!hmbtn_farm_field_plant(f,10,10,0x31) && !hmbtn_farm_field_plant(f,10,10,0x35));
    assert(f->plant_random==rng);
    assert(hmbtn_farm_field_plant(f,10,10,0x32) && f->plant_random==rng);
    assert(!hmbtn_farm_field_plant(f,10,10,0x33) && f->plant_random==rng);
    /* Real action: wait for ANI, then apply the extracted nine-cell grid once.
     * Two unavailable cells skip draws; no-eligible-target action draws none. */
    const int32_t position[3]={-200,0,-2600};memcpy(s->intro.player.position,position,sizeof(position));
    s->intro.player.facing=3;s->farm_action.fatigue=0;s->farm_action.phase=HMBTN_FARM_ACTION_FREE;
    hmbtn_farm_field_init(f,NULL,0);f->season=2;
    for(int y=9;y<=11;++y) for(int x=9;x<=11;++x) f->cells[y*45+x]=(HmbtnFarmTile){.value=0x168,.ground=0x168};
    f->cells[9*45+9]=(HmbtnFarmTile){.value=0x167,.ground=0x167};f->active[11*45+11]=false;
    s->game.inventory.equipped=(HmbtnStoredTool){0x33,0,2};
    assert(hmbtn_farm_action_tick(&s->farm_action,&s->intro.player,&s->house,f,NULL,HMBTN_PAD_SQUARE,HMBTN_PAD_SQUARE));
    assert(f->plant_random==1 && s->farm_action.phase==HMBTN_FARM_PLANT_SEED);
    assert(hmbtn_farm_action_tick(&s->farm_action,&s->intro.player,&s->house,f,NULL,0,0) && f->plant_random==1);
    s->intro.player.animation_done=true;
    assert(hmbtn_farm_action_tick(&s->farm_action,&s->intro.player,&s->house,f,NULL,0,0));
    uint32_t expected=1;unsigned accepted=0;
    for(unsigned part=0;part<9;++part) {
        int dx,dz;assert(hmbtn_farm_seed_cell(0x33,part,3,&dx,&dz));const unsigned at=(10+dz)*45+10+dx;
        if(at==9*45+9 || at==11*45+11) continue;
        expected=expected*UINT32_C(0x41c64e6d)+12345u;++accepted;
        assert(f->cells[at].value==((((expected>>16)*25u)>>16)==0 ? 0x179 : 0x178));
    }
    assert(accepted==7 && f->plant_random==expected && s->game.inventory.equipped.count==1);
    assert(hmbtn_farm_action_tick(&s->farm_action,&s->intro.player,&s->house,f,NULL,HMBTN_PAD_SQUARE,HMBTN_PAD_SQUARE));
    s->intro.player.animation_done=true;
    assert(hmbtn_farm_action_tick(&s->farm_action,&s->intro.player,&s->house,f,NULL,0,0));
    assert(f->plant_random==expected && !s->game.inventory.equipped.id);
    hmbtn_session_destroy(s);
    puts("Magic Red: original roll/cell, variants, action grid, rejected cells, growth, shipping, winter and persistent isolated stream passed.");
    return 0;
}

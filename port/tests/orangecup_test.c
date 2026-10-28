/* Original vectors: farm_tables.c:CROP_ORANGE_CUP/ITEM_ORANGE_CUP_*,
 * field.c:field_event_dispatch/field_daily_tick/field_tile_item_stage.
 * Reference 6fa97ab. No separate Orangecup growing/purchase algorithm. */
#include "save_fixture.h"
#include "farm_shipping.h"
#include "inventory.h"
#include <stdio.h>
#include <stdlib.h>

static bool tile_equal(const HmbtnFarmTile *a,
                       const HmbtnFarmTile *b)
{
    return a->value == b->value
        && a->ground == b->ground
        && a->kind == b->kind
        && a->growth_flags == b->growth_flags
        && a->watered_days == b->watered_days;
}


static HmbtnSession *reload(HmbtnSession *s)
{
    char error[256]={0};uint8_t *data=NULL;size_t bytes=0;
    bool ok=hmbtn_save_encode(s,&data,&bytes,error,sizeof(error));
    if(!ok) fprintf(stderr,"%s\n",error);
    assert(ok);
    HmbtnSession *next=hmbtn_save_decode(data,bytes,error,sizeof(error));
    if(!next) fprintf(stderr,"%s\n",error);
    assert(next);free(data);hmbtn_session_destroy(s);
    static const uint8_t schedules[1]={0};assert(hmbtn_session_bind_schedules(next,schedules,1));
    return next;
}




int main(void)
{
    const HmbtnFarmCrop *crop=hmbtn_farm_crop(0x35);
    assert(crop && crop->season==0 && crop->grown==4 && crop->regrow==-1 && crop->harvest==0x85);
    for(unsigned i=0;i<4;++i) assert(crop->days[i]==2);
    assert(hmbtn_farm_item(0x35)->buy_price==1000 && hmbtn_farm_item(0x35)->plant_animation==7);
    assert(hmbtn_farm_item(0x85)->sell_price==60 && !hmbtn_farm_item(0x56)->sell_price);
    for(unsigned season=0;season<4;++season) {
        HmbtnSession *s=save_fixture(0);s->poultry.upgraded=true;
        s->construction.house_level=2;s->construction.barn_level=1;s->construction.greenhouse=true;
        s->area=s->intro.area=124;s->greenhouse.season=season;
        s->game.clock.season=season;s->daily_date=hmbtn_session_date(&s->game.clock);
        const HmbtnFarmTile outside=s->field.cells[2];
        const uint32_t rng=s->greenhouse.plant_random;
        assert(hmbtn_farm_field_tool(&s->greenhouse,6,6,1,0,1,NULL));
        assert(hmbtn_farm_field_plant(&s->greenhouse,6,6,0x35));
        assert(!hmbtn_farm_field_plant(&s->greenhouse,7,6,0x35));
        for(unsigned day=0;day<8;++day) {
            HmbtnFarmTile before=s->greenhouse.cells[48];
            hmbtn_farm_field_rain(&s->greenhouse);
            hmbtn_farm_field_new_day(&s->greenhouse,(season+day/3)%4);
            assert(tile_equal(&before, &s->greenhouse.cells[48])); /* Dry/rain shelter. */
            assert(hmbtn_farm_field_water(&s->greenhouse,48));
            hmbtn_farm_field_new_day(&s->greenhouse,(season+day/3)%4);
            assert(s->greenhouse.cells[48].kind==(day+1)/2 && s->greenhouse.cells[48].watered_days==(day+1)%2);
            assert(hmbtn_farm_field_harvest_item(&s->greenhouse,48)==(day==7 ? 0x85u : 0u));
            s=reload(s);
            assert(hmbtn_session_field(s)==&s->greenhouse && s->greenhouse.plant_random==rng);
            assert(tile_equal(&outside, &s->field.cells[2]));
        }
        s->game.inventory.held_item=(HmbtnStoredTool){0};
        memset(s->game.inventory.items,0,sizeof(s->game.inventory.items));
        assert(hmbtn_inventory_hold(&s->game.inventory,(HmbtnStoredTool){(uint16_t)hmbtn_farm_field_harvest(&s->greenhouse,48),0,1}));
        assert(!hmbtn_farm_field_harvest(&s->greenhouse,48) && s->greenhouse.cells[48].value==0x168);
        assert(hmbtn_inventory_cycle(&s->game.inventory,false,false));s=reload(s);
        assert(s->game.inventory.items[0].id==0x85 && hmbtn_inventory_cycle(&s->game.inventory,false,false));
        assert(s->game.inventory.held_item.id==0x85);
        const unsigned value=s->game.economy.shipping.pending_value;unsigned shipments[25];
        memcpy(shipments,s->game.economy.shipments,sizeof(shipments));
        assert(hmbtn_farm_shipping_add(&s->game.economy,0x85) && s->game.economy.shipping.pending_value==value+60);
        assert(!memcmp(shipments,s->game.economy.shipments,sizeof(shipments)));
        assert(!hmbtn_farm_shipping_add(&s->game.economy,0x56));
        hmbtn_session_destroy(s);
    }
    /* No original crop-specific outdoor refusal: normal planting/season rules
     * apply. Spring grows; summer withers; winter rejects planting. */
    HmbtnFarmField *f=malloc(sizeof(*f));assert(f);hmbtn_farm_field_init(f,NULL,0);
    for(unsigned season=0;season<4;++season) {
        hmbtn_farm_field_clear(f);f->season=season;
        f->cells[0]=(HmbtnFarmTile){.value=0x168,.ground=0x168};
        assert(hmbtn_farm_field_plant(f,0,0,0x35)==(season!=3));
        if(!season) {
            for(unsigned day=0;day<8;++day) { assert(hmbtn_farm_field_water(f,0));hmbtn_farm_field_new_day(f,season); }
            assert(hmbtn_farm_field_harvest_item(f,0)==0x85);
            hmbtn_farm_field_new_day(f,1);assert(f->cells[0].kind==1 && !hmbtn_farm_field_harvest_item(f,0));
        } else if(season!=3) { hmbtn_farm_field_new_day(f,season);assert(f->cells[0].value==0x168); }
    }
    free(f);puts("Orangecup: original 8 watered days, year-round shelter, boundaries, independent fields/RNG, current saves, inventory, 60 G product and outdoor source rules passed.");return 0;
}

/* Vectors from farm_tables.c and field_daily_tick / field_tile_item_stage.
 * Flower products have price zero and use the existing inventory/gift path. */
#include "save_fixture.h"
#include "farm_shipping.h"
#include "inventory.h"
#include "item_reaction.h"
#include <stdlib.h>
#include <stdio.h>
int main(void)
{
    const unsigned seeds[]={0x32,0x34,0x31},products[]={0x52,0x55,0x51},seasons[]={1,0,0},prices[]={200,500,300},days[]={6,12,6};
    const uint8_t stages[3][12]={{0,1,1,2,2,3},{0,0,1,1,1,2,2,2,3,3,3,4},{0,1,1,2,2,3}};
    const uint8_t progress[3][12]={{1,0,1,0,1,0},{1,2,0,1,2,0,1,2,0,1,2,0},{1,0,1,0,1,0}};
    for(unsigned n=0;n<3;++n) {
        HmbtnSession *s=save_fixture(0);char error[512]={0};
        s->game.clock.season=seasons[n];s->daily_date=hmbtn_session_date(&s->game.clock);
        s->field.season=seasons[n];s->field.cells[460]=(HmbtnFarmTile){.value=0x168,.ground=0x168};
        const HmbtnFarmCrop *crop=hmbtn_farm_crop(seeds[n]);
        assert(hmbtn_farm_crop_supported(seeds[n]) && crop && crop->season==seasons[n] && crop->regrow==-1);
        assert(hmbtn_farm_item(seeds[n])->buy_price==(int)prices[n] && crop->harvest==(int)products[n]);
        assert(!hmbtn_farm_item(products[n])->sell_price && hmbtn_item_villager_reaction(products[n],18)==1);
        assert(hmbtn_farm_field_plant(&s->field,10,10,seeds[n]));
        for(unsigned d=0;d<days[n];++d) {
            const HmbtnFarmTile before=s->field.cells[460];
            hmbtn_farm_field_new_day(&s->field,seasons[n]); /* Dry morning. */
            assert(s->field.cells[460].kind==before.kind && s->field.cells[460].watered_days==before.watered_days);
            assert(hmbtn_farm_field_water(&s->field,460));hmbtn_farm_field_new_day(&s->field,seasons[n]);
            assert(s->field.cells[460].kind==stages[n][d] && s->field.cells[460].watered_days==progress[n][d]);
        }
        assert(hmbtn_farm_field_harvest_item(&s->field,460)==products[n]);
        s->game.inventory.held_item=(HmbtnStoredTool){0};
        assert(hmbtn_inventory_hold(&s->game.inventory,(HmbtnStoredTool){(uint16_t)hmbtn_farm_field_harvest(&s->field,460),0,1}));
        assert(s->field.cells[460].value==0x168 && !s->field.cells[460].kind && !hmbtn_farm_field_harvest(&s->field,460));
        /* Empty the save fixture's goods to test a full flower bag roundtrip. */
        memset(s->game.inventory.items,0,sizeof(s->game.inventory.items));
        s->game.inventory.held_item=(HmbtnStoredTool){(uint16_t)products[n],0,1};
        assert(hmbtn_inventory_cycle(&s->game.inventory,false,false) && s->game.inventory.items[0].id==products[n]);
        assert(hmbtn_inventory_cycle(&s->game.inventory,false,false) && s->game.inventory.held_item.id==products[n]);
        HmbtnEconomy before=s->game.economy;
        assert(!hmbtn_farm_shipping_add(&s->game.economy,products[n]) && !memcmp(&before,&s->game.economy,sizeof(before)));
        s->field.cells[460]=(HmbtnFarmTile){.value=(uint16_t)seeds[n],.ground=0x168,.kind=(uint8_t)crop->grown};
        hmbtn_farm_field_new_day(&s->field,seasons[n]+1);
        assert(s->field.cells[460].value==seeds[n] && s->field.cells[460].kind==1 && !hmbtn_farm_field_harvest_item(&s->field,460));
        s->game.clock.season=seasons[n]+1;s->daily_date=hmbtn_session_date(&s->game.clock);
        /* Restored flower in hand and withered bed do not need a format bump. */
        uint8_t *bytes=NULL;size_t count=0;assert(hmbtn_save_encode(s,&bytes,&count,error,sizeof(error)));
        HmbtnSession *loaded=hmbtn_save_decode(bytes,count,error,sizeof(error));free(bytes);assert(loaded);
        assert(loaded->house.inventory==&loaded->game.inventory && loaded->game.inventory.held_item.id==products[n]
            && loaded->game.inventory.held_item.count==1 && loaded->field.cells[460].value==seeds[n] && loaded->field.cells[460].kind==1);
        hmbtn_session_destroy(loaded);
        hmbtn_farm_field_new_day(&s->field,3);assert(s->field.cells[460].value==0x168 && !s->field.cells[460].kind);
        hmbtn_session_destroy(s);
    }
    assert(hmbtn_farm_crop_supported(0x31) && hmbtn_farm_crop_supported(0x33) && !hmbtn_farm_crop_supported(0x35));
    puts("Flowers: original prices/daily vectors, dry mornings, one harvest, inventory, gifts, no shipping income, seasons/winter and native save passed.");return 0;
}

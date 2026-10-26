/* estate.c: shipment bins/value, purse and earnings; world/clock.c callbacks. */
#include "farm_shipping.h"
#include "economy.h"
#include "farm_field.h"
#include <string.h>
#include "shipping_tables.inc"

bool hmbtn_farm_shipping_add(HmbtnEconomy *economy,unsigned id)
{
    const HmbtnFarmItem *item=hmbtn_farm_item(id);
    if(!economy || !item || item->sell_price<=0) return false;
    HmbtnFarmShipping *s=&economy->shipping;
    s->pending_value+=(uint32_t)item->sell_price+(id==SHIPPING_HONEY && s->honey_bonus ? 10u : 0u);
    const int bin=shipping_bins[id];
    if(bin>=0 && economy->shipments[bin]<999) ++economy->shipments[bin];
    if(id>=SHIPPING_FISH_FIRST && id<SHIPPING_FISH_FIRST+3) ++s->fish_shipped;
    s->item_shipped=true;
    return true;
}
void hmbtn_farm_shipping_clock(HmbtnEconomy *economy,
    const HmbtnFarmClock *c,unsigned changes)
{
    if(!(changes&HMBTN_CLOCK_MINUTE)) return;
    HmbtnFarmShipping *s=&economy->shipping;
    if(c->hour==5 && c->minute==59 && c->day==29) {
        for(unsigned i=3;i>0;--i) memcpy(economy->ledger[i],economy->ledger[i-1],sizeof(economy->ledger[i]));
        memset(economy->ledger[0],0,sizeof(economy->ledger[0]));
    }
    if(changes&HMBTN_CLOCK_DAY) s->settled=false;
    if(c->hour!=17 || c->minute!=58 || s->settled) return;
    s->settled=true;
    for(unsigned i=0;i<14;++i)
        if(c->season==shipping_special_dates[i][0] && c->day==shipping_special_dates[i][1]) return;
    hmbtn_economy_gold_change(economy,(int64_t)s->pending_value,false);
    s->pending_value=0;
}

/* estate.c: shipment bins/value, purse and earnings; world/clock.c callbacks. */
#include "farm_shipping.h"
#include <string.h>
#include "shipping_tables.inc"

bool hmbtn_farm_shipping_add(HmbtnFarmShipping *s,HmbtnFarmStatistics *stats,unsigned id)
{
    const HmbtnFarmItem *item=hmbtn_farm_item(id);
    if(!s || !stats || !item || item->sell_price<=0) return false;
    s->pending_value+=(uint32_t)item->sell_price+(id==SHIPPING_HONEY && s->honey_bonus ? 10u : 0u);
    const int bin=shipping_bins[id];
    if(bin>=0 && stats->shipments[bin]<999) ++stats->shipments[bin];
    if(id>=SHIPPING_FISH_FIRST && id<SHIPPING_FISH_FIRST+3) ++s->fish_shipped;
    s->item_shipped=true;
    return true;
}
void hmbtn_farm_shipping_clock(HmbtnFarmShipping *s,HmbtnFarmStatistics *stats,
    const HmbtnFarmClock *c,unsigned changes)
{
    if(!(changes&HMBTN_CLOCK_MINUTE)) return;
    if(c->hour==5 && c->minute==59 && c->day==29) {
        for(unsigned i=3;i>0;--i) memcpy(stats->ledger[i],stats->ledger[i-1],sizeof(stats->ledger[i]));
        memset(stats->ledger[0],0,sizeof(stats->ledger[0]));
    }
    if(changes&HMBTN_CLOCK_DAY) s->settled=false;
    if(c->hour!=17 || c->minute!=58 || s->settled) return;
    s->settled=true;
    for(unsigned i=0;i<14;++i)
        if(c->season==shipping_special_dates[i][0] && c->day==shipping_special_dates[i][1]) return;
    const uint64_t gold=(uint64_t)stats->gold+s->pending_value;
    stats->gold=gold>9999999 ? 9999999u : (unsigned)gold;
    stats->ledger[0][0]+=s->pending_value;
    s->pending_value=0;
}

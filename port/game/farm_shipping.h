#ifndef HMBTN_FARM_SHIPPING_H
#define HMBTN_FARM_SHIPPING_H
#include "farm_menu.h"
#include "farm_clock.h"
typedef struct HmbtnFarmShipping {
    uint32_t pending_value,fish_shipped;
    bool settled,item_shipped,honey_bonus;
} HmbtnFarmShipping;
/* Positive-price goods only. The caller must establish a real box landing. */
bool hmbtn_farm_shipping_add(HmbtnFarmShipping *shipping,HmbtnFarmStatistics *stats,unsigned item);
/* estate_shipment_settle, the 05:59 ledger roll, and morning settlement reset. */
void hmbtn_farm_shipping_clock(HmbtnFarmShipping *shipping,HmbtnFarmStatistics *stats,
    const HmbtnFarmClock *clock,unsigned changes);
#endif

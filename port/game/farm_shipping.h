#ifndef HMBTN_FARM_SHIPPING_H
#define HMBTN_FARM_SHIPPING_H
#include "game_state.h"
#include "farm_clock.h"
/* Positive-price goods only. The caller must establish a real box landing. */
bool hmbtn_farm_shipping_add(HmbtnEconomy *economy,unsigned item);
unsigned hmbtn_farm_shipping_price(const HmbtnEconomy *,unsigned item);
int hmbtn_farm_shipping_bin(unsigned item);
/* estate_basket_merge: counts/value are already accumulated, no second sale. */
void hmbtn_farm_shipping_merge(HmbtnEconomy *,const unsigned counts[25],unsigned value);
/* milestone_check_group_a/b/c: all four original products >=101. Derived
 * from the same saved shipment counters; no second progress flags or stores. */
bool hmbtn_farm_shipping_seed_unlocked(const HmbtnEconomy *,unsigned season);
/* estate_shipment_settle, the 05:59 ledger roll, and morning settlement reset. */
void hmbtn_farm_shipping_clock(HmbtnEconomy *economy,
    const HmbtnFarmClock *clock,unsigned changes);
#endif

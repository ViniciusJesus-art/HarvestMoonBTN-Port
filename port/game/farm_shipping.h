#ifndef HMBTN_FARM_SHIPPING_H
#define HMBTN_FARM_SHIPPING_H
#include "game_state.h"
#include "farm_clock.h"
/* Positive-price goods only. The caller must establish a real box landing. */
bool hmbtn_farm_shipping_add(HmbtnEconomy *economy,unsigned item);
/* estate_shipment_settle, the 05:59 ledger roll, and morning settlement reset. */
void hmbtn_farm_shipping_clock(HmbtnEconomy *economy,
    const HmbtnFarmClock *clock,unsigned changes);
#endif

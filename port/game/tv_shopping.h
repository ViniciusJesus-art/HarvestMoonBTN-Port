#ifndef HMBTN_TV_SHOPPING_H
#define HMBTN_TV_SHOPPING_H
#include "game_state.h"
enum { HMBTN_TV_FREE=0,HMBTN_TV_OFFERED=1,HMBTN_TV_WATCHED=2,HMBTN_TV_PAID=3,HMBTN_TV_CLOSED=255 };
/* message_refresh_clock/message_pick_ending: call when opening the TV, before
 * channel selection. Returns 9 if no shopping program is broadcast. */
unsigned hmbtn_tv_shopping_program(HmbtnTvShopping *,const HmbtnFarmClock *,unsigned weather,unsigned house_level);
void hmbtn_tv_shopping_watched(HmbtnTvShopping *,unsigned offer);
unsigned hmbtn_tv_shopping_accepted(const HmbtnTvShopping *);
unsigned hmbtn_tv_shopping_item(unsigned offer);
/* Phone fee is separate and retained after cancellation/insufficient funds.
 * On success the caller sets the original delivery event flag 0x67+offer. */
bool hmbtn_tv_shopping_buy(HmbtnTvShopping *,HmbtnEconomy *,unsigned offer);
bool hmbtn_tv_shopping_valid(const HmbtnTvShopping *);
#endif

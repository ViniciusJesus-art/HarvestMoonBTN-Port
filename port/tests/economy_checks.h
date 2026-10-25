#ifndef HMBTN_TEST_ECONOMY_CHECKS_H
#define HMBTN_TEST_ECONOMY_CHECKS_H
#include "game_state.h"
/* Compare values, never struct padding or addresses. Snapshots are expectations,
 * not financial state used by the game. */
static inline bool economy_equal(const HmbtnEconomy *a,const HmbtnEconomy *b)
{
    if(a->gold!=b->gold) return false;
    for(unsigned i=0;i<25;++i) if(a->shipments[i]!=b->shipments[i]) return false;
    for(unsigned i=0;i<4;++i) for(unsigned j=0;j<2;++j)
        if(a->ledger[i][j]!=b->ledger[i][j]) return false;
    return a->shipping.pending_value==b->shipping.pending_value
        && a->shipping.fish_shipped==b->shipping.fish_shipped
        && a->shipping.settled==b->shipping.settled
        && a->shipping.item_shipped==b->shipping.item_shipped
        && a->shipping.honey_bonus==b->shipping.honey_bonus;
}
#endif

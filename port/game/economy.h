#ifndef HMBTN_ECONOMY_H
#define HMBTN_ECONOMY_H
#include "game_state.h"

enum { HMBTN_GOLD_DEFAULT_LIMIT=9999999 };
typedef enum HmbtnGoldResult {
    HMBTN_GOLD_OK, HMBTN_GOLD_LIMIT, HMBTN_GOLD_INSUFFICIENT, HMBTN_GOLD_INVALID
} HmbtnGoldResult;

/* estate_gold_change: zero is a no-op; refused debit changes neither purse nor
 * ledger. reset permits a partial debit to zero. Income records the full amount
 * even when the purse is capped; a partial debit records only money removed.
 * gold_limit==0 selects the base-game limit, otherwise the session's limit.
 * Ledger arithmetic is unsigned modulo 2^32, as in the original records.
 * No graphics, script operands, I/O or host services. */
HmbtnGoldResult hmbtn_economy_gold_change(HmbtnEconomy *economy,int64_t amount,bool reset);
unsigned hmbtn_economy_gold_limit(const HmbtnEconomy *economy);
#endif

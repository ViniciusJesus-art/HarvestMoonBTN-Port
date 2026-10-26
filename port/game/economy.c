/* Adapted from src/overlay/11_GF_FARM/estate.c: estate_gold_change,
 * reference d3c0f38 (src unchanged). Replace game_state globals with an explicit
 * economy, FARM_MONEY_MAX with a configurable base-game default, and signed PS1
 * addition with magnitude arithmetic defined even for INT64_MIN/MAX. Return
 * codes 0/1/2, reset policy and full-income ledger semantics are retained. */
#include "economy.h"

unsigned hmbtn_economy_gold_limit(const HmbtnEconomy *economy)
{
    return economy && economy->gold_limit ? economy->gold_limit : HMBTN_GOLD_DEFAULT_LIMIT;
}
HmbtnGoldResult hmbtn_economy_gold_change(HmbtnEconomy *economy,int64_t amount,bool reset)
{
    if(!economy) return HMBTN_GOLD_INVALID;
    if(!amount) return HMBTN_GOLD_OK;
    const unsigned limit=hmbtn_economy_gold_limit(economy);
    if(economy->gold>limit) return HMBTN_GOLD_INVALID;
    if(amount<0) {
        uint64_t debit=(uint64_t)(-(amount+1))+1u;
        const bool insufficient=debit>economy->gold;
        if(insufficient && !reset) return HMBTN_GOLD_INSUFFICIENT;
        if(insufficient) debit=economy->gold;
        economy->gold-=(unsigned)debit;
        economy->ledger[0][1]=(uint32_t)((uint64_t)economy->ledger[0][1]+debit);
        return insufficient ? HMBTN_GOLD_INSUFFICIENT : HMBTN_GOLD_OK;
    }
    const uint64_t credit=(uint64_t)amount;
    const bool capped=credit>=limit-economy->gold;
    economy->gold=capped ? limit : economy->gold+(unsigned)credit;
    economy->ledger[0][0]=(uint32_t)((uint64_t)economy->ledger[0][0]+credit);
    return capped ? HMBTN_GOLD_LIMIT : HMBTN_GOLD_OK;
}

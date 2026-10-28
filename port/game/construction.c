/* Adapted from shop_lumber_deal_step and milestones.c: milestone_tick.
 * Four overnight ticks: original stampTimer=3 completes when --timer < 0.
 * All five original building offers share one order and daily countdown. */
#include "construction.h"
#include "poultry.h"
#include "economy.h"
#include "psx_random.h"
#include "construction_tables.inc"
const HmbtnConstructionOffer *hmbtn_construction_offer(unsigned stage) { return stage<5 ? &construction_offers[stage] : NULL; }
unsigned hmbtn_construction_next(const HmbtnConstruction *c,const HmbtnPoultry *p)
{ return !p->upgraded ? 0u : !c->house_level ? 1u : !c->barn_level ? 2u : c->house_level<2 ? 3u : !c->greenhouse ? 4u : 5u; }
unsigned hmbtn_construction_mornings(void) { return CONSTRUCTION_MORNINGS; }
unsigned hmbtn_carpenter_lumber_limit(void) { return LUMBER_MAX; }
unsigned hmbtn_carpenter_lumber_item(void) { return LUMBER_ITEM; }
HmbtnConstructionResult hmbtn_construction_order(HmbtnConstruction *c,
    const HmbtnPoultry *p,HmbtnInventory *inventory,HmbtnEconomy *economy)
{
    const unsigned stage=hmbtn_construction_next(c,p);
    const HmbtnConstructionOffer *offer=hmbtn_construction_offer(stage);
    if(c->mornings_left) return HMBTN_BUILD_BUSY;
    if(!offer) return HMBTN_BUILD_COMPLETE;
    if(economy->gold<offer->gold) return HMBTN_BUILD_GOLD;
    if(inventory->supplies[1]<offer->lumber) return HMBTN_BUILD_LUMBER;
    inventory->supplies[1]-=offer->lumber;
    hmbtn_economy_gold_change(economy,-(int)offer->gold,true);
    c->pending=stage==0 ? HMBTN_BUILD_COOP : stage==1 ? HMBTN_BUILD_HOUSE : stage==2 ? HMBTN_BUILD_BARN : stage==3 ? HMBTN_BUILD_HOUSE2 : HMBTN_BUILD_GREENHOUSE;
    c->mornings_left=CONSTRUCTION_MORNINGS;
    return HMBTN_BUILD_OK;
}
void hmbtn_construction_new_day(HmbtnConstruction *c,HmbtnPoultry *p)
{
    if(c->mornings_left && !--c->mornings_left) {
        if(c->pending==HMBTN_BUILD_COOP) p->upgraded=true;
        else if(c->pending==HMBTN_BUILD_HOUSE) c->house_level=1;
        else if(c->pending==HMBTN_BUILD_BARN) c->barn_level=1;
        else if(c->pending==HMBTN_BUILD_HOUSE2) c->house_level=2;
        else if(c->pending==HMBTN_BUILD_GREENHOUSE) c->greenhouse=true;
        c->pending=HMBTN_BUILD_NONE;
    }
}
bool hmbtn_construction_storm(HmbtnConstruction *c)
{
    if(!c || !c->greenhouse || hmbtn_psx_random_range(&c->damage_random,100)>=31) return false;
    c->greenhouse=false;return true;
}

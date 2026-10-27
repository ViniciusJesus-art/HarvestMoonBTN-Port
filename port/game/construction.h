#ifndef HMBTN_CONSTRUCTION_H
#define HMBTN_CONSTRUCTION_H
#include "game_state.h"
struct HmbtnPoultry;
/* Session owns pending work and house level. Poultry.upgraded remains the one coop level;
 * inventory.supplies[1] and game.economy remain the wood and purse authorities. */
typedef enum HmbtnConstructionWork { HMBTN_BUILD_NONE,HMBTN_BUILD_COOP,HMBTN_BUILD_HOUSE } HmbtnConstructionWork;
typedef struct HmbtnConstruction {
    unsigned mornings_left,house_level;
    HmbtnConstructionWork pending;
} HmbtnConstruction;
typedef struct HmbtnConstructionOffer { unsigned gold,lumber; } HmbtnConstructionOffer;
typedef enum HmbtnConstructionResult {
    HMBTN_BUILD_OK,HMBTN_BUILD_BUSY,HMBTN_BUILD_COMPLETE,HMBTN_BUILD_GOLD,HMBTN_BUILD_LUMBER
} HmbtnConstructionResult;
const HmbtnConstructionOffer *hmbtn_construction_offer(unsigned stage);
unsigned hmbtn_construction_next(const HmbtnConstruction *,const struct HmbtnPoultry *);
unsigned hmbtn_construction_mornings(void);
unsigned hmbtn_carpenter_lumber_limit(void);
unsigned hmbtn_carpenter_lumber_item(void);
HmbtnConstructionResult hmbtn_construction_order(HmbtnConstruction *,
    const struct HmbtnPoultry *,HmbtnInventory *,HmbtnEconomy *);
void hmbtn_construction_new_day(HmbtnConstruction *,struct HmbtnPoultry *);
#endif

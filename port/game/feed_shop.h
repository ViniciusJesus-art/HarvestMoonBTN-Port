#ifndef HMBTN_FEED_SHOP_H
#define HMBTN_FEED_SHOP_H
#include "farm_menu.h"
#include "village.h"
#include "poultry.h"
typedef enum HmbtnFeedShopPhase {
    HMBTN_SHOP_OFF,HMBTN_SHOP_GREETING,HMBTN_SHOP_SELECT,HMBTN_SHOP_INFO,
    HMBTN_SHOP_OFFER,HMBTN_SHOP_COUNT,HMBTN_SHOP_RESULT,HMBTN_SHOP_LEAVE,HMBTN_SHOP_NAME,
    HMBTN_SHOP_SELL_LIST,HMBTN_SHOP_SELL_CONFIRM
} HmbtnFeedShopPhase;
typedef struct HmbtnFeedShop {
    HmbtnFeedShopPhase phase,next,after_result;
    HmbtnMessage message;
    unsigned merchant,item,quantity,request,loaded,next_request,repeat;
    unsigned saved_facing;
    uint32_t previous_arrows;
    bool closing;
    bool medicine_available;
    int sound;
    HmbtnPoultry *poultry;
    HmbtnNewGame editor;
    const HmbtnFarmClock *clock; /* Live session clock; modal shop pauses it. */
    unsigned selection;
    unsigned sale_cursor,sale_top,sale_slot,answer;
} HmbtnFeedShop;
/* Original scripts 365/368: empty hands, business hours, weekday and merchant present. */
bool hmbtn_feed_shop_open(HmbtnFeedShop *shop,HmbtnVillage *village,
    const HmbtnIntroActor *player,const HmbtnFarmClock *clock,unsigned area,unsigned held_item);
/* Borrow the authoritative economy. stats contains only the remaining
 * nonfinancial counters (animal count); no purse or accounts live there. */
void hmbtn_feed_shop_tick(HmbtnFeedShop *shop,HmbtnHouseInteraction *inventory,
    HmbtnEconomy *economy,HmbtnFarmStatistics *stats,uint32_t pressed,uint32_t held);
unsigned hmbtn_feed_shop_cost(const HmbtnFeedShop *shop);
bool hmbtn_feed_shop_expand(uint8_t *record,size_t size,const HmbtnRegistration *registration);
#endif

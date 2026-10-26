#ifndef HMBTN_FEED_SHOP_H
#define HMBTN_FEED_SHOP_H
#include "farm_menu.h"
#include "village.h"
#include "poultry.h"
typedef enum HmbtnFeedShopPhase {
    HMBTN_SHOP_OFF,HMBTN_SHOP_GREETING,HMBTN_SHOP_SELECT,HMBTN_SHOP_INFO,
    HMBTN_SHOP_OFFER,HMBTN_SHOP_COUNT,HMBTN_SHOP_RESULT,HMBTN_SHOP_LEAVE,HMBTN_SHOP_NAME,
    HMBTN_SHOP_SELL_LIST,HMBTN_SHOP_SELL_CONFIRM,HMBTN_SHOP_CANCEL_COUNT
} HmbtnFeedShopPhase;
typedef struct HmbtnFeedShop {
    HmbtnFeedShopPhase phase,next,after_result;
    HmbtnMessage message;
    unsigned merchant,item,quantity,request,loaded,next_request,repeat;
    unsigned bank,next_bank; /* Text bank, not an address in the original executable. */
    unsigned saved_facing;
    uint32_t previous_arrows;
    bool closing;
    bool medicine_available;
    bool seeds; /* Original seasonal seed-table menu, same shop state. */
    int sound;
    HmbtnPoultry *poultry;
    HmbtnNewGame editor;
    const HmbtnFarmClock *clock; /* Live session clock; modal shop pauses it. */
    unsigned selection;
    unsigned sale_cursor,sale_top,sale_slot,answer;
} HmbtnFeedShop;
typedef struct HmbtnSeedShopEntry {
    unsigned item,description;
    const uint16_t *name;
    unsigned name_words;
} HmbtnSeedShopEntry;
/* Base spring/summer/fall stock from shop_seed_menu_setup_a/b/c. Unsupported seasons
 * return no entries. Conditional milestone seeds remain pending. */
unsigned hmbtn_seed_shop_entries(unsigned season);
const HmbtnSeedShopEntry *hmbtn_seed_shop_entry(unsigned season,unsigned index);
/* Original scripts 365/368: empty hands, business hours, weekday and merchant present. */
bool hmbtn_feed_shop_open(HmbtnFeedShop *shop,HmbtnVillage *village,
    const HmbtnIntroActor *player,const HmbtnFarmClock *clock,unsigned area,unsigned held_item);
/* Borrow the authoritative economy. stats contains only the remaining
 * nonfinancial counters (animal count); no purse or accounts live there. */
void hmbtn_feed_shop_tick(HmbtnFeedShop *shop,HmbtnHouseInteraction *inventory,
    HmbtnEconomy *economy,HmbtnFarmStatistics *stats,uint32_t pressed,uint32_t held);
bool hmbtn_seed_shop_open(HmbtnFeedShop *shop,HmbtnVillage *village,
    const HmbtnIntroActor *player,const HmbtnFarmClock *clock,unsigned area,
    const uint8_t *section,size_t section_size,unsigned held_item);
/* Original group D has eight rows; current native sale supports its first
 * three crops. Flowers/greenhouse entries remain outside this recut. */
const HmbtnSeedShopEntry *hmbtn_won_seed_entry(unsigned index);
unsigned hmbtn_seed_shop_count(const HmbtnFeedShop *shop);
const HmbtnSeedShopEntry *hmbtn_seed_shop_choice(const HmbtnFeedShop *shop,unsigned index);
/* Called by Won's original event_cmd_dispatch(8,10,0,0); the event script
 * selects whether to open the shop. No separate merchant interaction engine. */
bool hmbtn_won_seed_shop_open(HmbtnFeedShop *shop,const HmbtnFarmClock *clock);
unsigned hmbtn_feed_shop_cost(const HmbtnFeedShop *shop);
bool hmbtn_feed_shop_expand(uint8_t *record,size_t size,const HmbtnRegistration *registration);
#endif

#ifndef HMBTN_APIARY_H
#define HMBTN_APIARY_H
#include "farm_feed.h"
typedef struct HmbtnApiary { bool owned,collected; } HmbtnApiary;
/* estate_flag_take_once / player_act_zone_front(ZONE_HONEY). The existing
 * feed pickup phase presents the committed item; there is no second stock. */
bool hmbtn_apiary_tick(HmbtnApiary *,HmbtnFarmFeed *,HmbtnInventory *,
    HmbtnIntroActor *,const uint8_t *,size_t,uint32_t pressed);
#endif

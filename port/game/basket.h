#ifndef HMBTN_BASKET_H
#define HMBTN_BASKET_H
#include "game_state.h"
#include "farm_field.h"
#include "ground.h"
#include "farm_intro.h"
/* Persistent basket lives in game.basket; action is only an ANI/modal cursor. */
typedef enum HmbtnBasketPhase { HMBTN_BASKET_FREE,HMBTN_BASKET_PICKUP,HMBTN_BASKET_DROP,HMBTN_BASKET_ADD,HMBTN_BASKET_SHIP } HmbtnBasketPhase;
typedef struct HmbtnBasketAction { HmbtnBasketPhase phase;unsigned request,bounce;int sound; } HmbtnBasketAction;
bool hmbtn_basket_buy(HmbtnBasket *,HmbtnInventory *,HmbtnEconomy *);
bool hmbtn_basket_add(HmbtnBasket *,const HmbtnEconomy *,unsigned item);
bool hmbtn_basket_merge(HmbtnBasket *,HmbtnEconomy *);
bool hmbtn_basket_near(const HmbtnBasket *,unsigned area,const int32_t point[3]);
bool hmbtn_basket_blocked(const HmbtnBasket *,unsigned area,const int32_t from[3],const int32_t to[3]);
bool hmbtn_basket_valid(const HmbtnBasket *,const HmbtnInventory *);
bool hmbtn_basket_tick(HmbtnBasket *,HmbtnBasketAction *,HmbtnInventory *,HmbtnEconomy *,
    HmbtnIntroActor *,unsigned area,const HmbtnModel *,const HmbtnFarmField *,uint32_t pressed,
    bool (*blocked)(const void *,const int32_t[3],const int32_t[3]),const void *context);
#endif

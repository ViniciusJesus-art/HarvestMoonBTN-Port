#ifndef HMBTN_FARM_CARRY_H
#define HMBTN_FARM_CARRY_H
#include "house_interaction.h"
#include "farm_shipping.h"
#include "ground.h"
#include "pond.h"
typedef struct HmbtnThrownItem {
    HmbtnStoredTool item;
    int32_t position[3], ground;
    int velocity;
    unsigned facing,area;
    unsigned phase,timer; /* player.c: FLY -> HOLD (1 tick) -> BLINK (4 ticks). */
    HmbtnAnimationPlayer splash; /* Original stage ANI 23 on water landing. */
} HmbtnThrownItem;
typedef struct HmbtnFarmCarry {
    unsigned phase;
    int sound;
    bool changed;
    HmbtnThrownItem thrown[3];
    unsigned discarded;
    unsigned area; /* Current scene; flights retain the scene in which they began. */
    bool small_fish_placed; /* Original beach throw prerequisite for squid. */
    HmbtnBasket *basket; /* Borrowed game.basket, never another container. */
    HmbtnPond *pond;
    unsigned request;
} HmbtnFarmCarry;
/* Loose field items and mature crops share the same forward world probe. */
bool hmbtn_farm_carry_tick(HmbtnFarmCarry *state,HmbtnHouseInteraction *inventory,
    HmbtnFarmField *field,HmbtnIntroActor *player,uint32_t pressed);
void hmbtn_farm_carry_flight(HmbtnFarmCarry *state,HmbtnFarmField *field,
    const HmbtnModel *ground,HmbtnEconomy *economy);
bool hmbtn_farm_carry_splashes(HmbtnFarmCarry *,const HmbtnAnimation *,char *,size_t);
void hmbtn_farm_carried_offset(unsigned facing,bool running,int *x,int *y);
#endif

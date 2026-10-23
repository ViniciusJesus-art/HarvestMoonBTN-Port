#ifndef HMBTN_FARM_CARRY_H
#define HMBTN_FARM_CARRY_H
#include "house_interaction.h"
#include "farm_shipping.h"
#include "ground.h"
typedef struct HmbtnThrownItem {
    HmbtnStoredTool item;
    int32_t position[3], ground;
    int velocity;
    unsigned facing;
} HmbtnThrownItem;
typedef struct HmbtnFarmCarry {
    unsigned phase;
    int sound;
    bool changed;
    HmbtnThrownItem thrown[3];
    unsigned discarded;
} HmbtnFarmCarry;
/* The field path currently accepts loose pickups, not crops or estate objects. */
bool hmbtn_farm_carry_tick(HmbtnFarmCarry *state,HmbtnHouseInteraction *inventory,
    HmbtnFarmField *field,HmbtnIntroActor *player,uint32_t pressed);
void hmbtn_farm_carry_flight(HmbtnFarmCarry *state,HmbtnFarmField *field,
    const HmbtnModel *ground,HmbtnFarmShipping *shipping,HmbtnFarmStatistics *stats);
void hmbtn_farm_carried_offset(unsigned facing,bool running,int *x,int *y);
#endif

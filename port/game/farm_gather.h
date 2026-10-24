#ifndef HMBTN_FARM_GATHER_H
#define HMBTN_FARM_GATHER_H
#include "farm_carry.h"
enum { HMBTN_GATHER_RECORDS=197 };
typedef struct HmbtnGatherRecord { unsigned location,item,flags;int32_t position[3]; } HmbtnGatherRecord;
typedef struct HmbtnFarmGather { bool taken[HMBTN_GATHER_RECORDS]; } HmbtnFarmGather;
const HmbtnGatherRecord *hmbtn_farm_gather_record(unsigned index);
unsigned hmbtn_farm_gather_item(const HmbtnFarmGather *state,unsigned index,unsigned location,unsigned season);
void hmbtn_farm_gather_new_day(HmbtnFarmGather *state);
bool hmbtn_farm_gather_take(HmbtnFarmGather *state,unsigned location,unsigned season,
    HmbtnHouseInteraction *inventory,HmbtnFarmCarry *carry,HmbtnIntroActor *player);
int hmbtn_estate_item_art(unsigned item);
#endif

#ifndef HMBTN_FARM_ACTION_H
#define HMBTN_FARM_ACTION_H
#include "house_interaction.h"
typedef enum HmbtnFarmActionPhase {
    HMBTN_FARM_ACTION_FREE,HMBTN_FARM_TOOL_CHARGE,HMBTN_FARM_TOOL_SWING,
    HMBTN_FARM_REFILL,HMBTN_FARM_OVERCHARGE,HMBTN_FARM_MEDICINE,HMBTN_FARM_PLANT_SEED,HMBTN_FARM_ANIMAL_TOOL
} HmbtnFarmActionPhase;
typedef struct HmbtnFarmAction {
    HmbtnFarmActionPhase phase;
    unsigned kind, charge_frames, overcharge_frames, strikes, target;
    unsigned stamina, max_stamina, fatigue, experience[5];
    int sound, bank;
    bool bank_pending, finished_bank, changed;
    bool release_sound;
    uint16_t broken_item;
    int effect_column,effect_row;
} HmbtnFarmAction;
void hmbtn_farm_action_init(HmbtnFarmAction *action);
void hmbtn_farm_action_cost(HmbtnFarmAction *,const HmbtnHouseInteraction *,unsigned amount);
/* Returns true while an action consumes movement input. */
bool hmbtn_farm_action_tick(HmbtnFarmAction *action,HmbtnIntroActor *player,
    HmbtnHouseInteraction *house,HmbtnFarmField *field,const HmbtnModel *ground,
    uint32_t pressed,uint32_t held);
#endif

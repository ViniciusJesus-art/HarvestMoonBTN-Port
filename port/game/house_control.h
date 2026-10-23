#ifndef HMBTN_HOUSE_CONTROL_H
#define HMBTN_HOUSE_CONTROL_H
#include "farm_intro.h"
#include "ground.h"
#include "farm_field.h"
typedef struct HmbtnHouseControl {
    unsigned mode, blocked_frames;
    bool enabled, exit_pending, carrying;
    unsigned exit_zone;
    int jump_velocity;
    bool jumping, jump_started, landed;
} HmbtnHouseControl;
typedef struct HmbtnHouseExit {
    unsigned zone, target;
    int32_t entry[3];
} HmbtnHouseExit;
bool hmbtn_house_exit_at(const uint8_t *section, size_t size,
    const int32_t position[3], HmbtnHouseExit *exit);
bool hmbtn_farm_house_door_at(const uint8_t *section,size_t size,
    const HmbtnIntroActor *player,HmbtnHouseExit *exit,unsigned *model);
bool hmbtn_world_door_at(const uint8_t *section,size_t size,
    const HmbtnIntroActor *player,HmbtnHouseExit *exit,unsigned *model);
bool hmbtn_world_gate_at(const uint8_t *section,size_t size,const int32_t position[3],HmbtnHouseExit *exit);
unsigned hmbtn_farm_music_volume(const uint8_t *section,size_t size);
/* House uses model ground; farm variant also queries the original field rules.
 * Livestock, villagers and carrying states remain separate systems. */
void hmbtn_house_control_tick(HmbtnHouseControl *control, HmbtnIntroActor *player,
    const HmbtnModel *ground, uint32_t held);
void hmbtn_farm_control_tick(HmbtnHouseControl *control, HmbtnIntroActor *player,
    const HmbtnModel *ground, const HmbtnFarmField *field, uint32_t held);
#endif

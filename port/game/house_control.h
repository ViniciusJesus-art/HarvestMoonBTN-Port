#ifndef HMBTN_HOUSE_CONTROL_H
#define HMBTN_HOUSE_CONTROL_H
#include "farm_intro.h"
#include "ground.h"
typedef struct HmbtnHouseControl {
    unsigned mode, blocked_frames;
    bool enabled, exit_pending;
    unsigned exit_zone;
} HmbtnHouseControl;
typedef struct HmbtnHouseExit {
    unsigned zone, target;
    int32_t entry[3];
} HmbtnHouseExit;
bool hmbtn_house_exit_at(const uint8_t *section, size_t size,
    const int32_t position[3], HmbtnHouseExit *exit);
/* Initial house only: no field occupants, animals, held items or tool modes. */
void hmbtn_house_control_tick(HmbtnHouseControl *control, HmbtnIntroActor *player,
    const HmbtnModel *ground, uint32_t held);
#endif

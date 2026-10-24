#ifndef HMBTN_FISHING_H
#define HMBTN_FISHING_H
#include "farm_action.h"
#include "farm_clock.h"
typedef enum HmbtnFishingPhase {
    HMBTN_FISHING_FREE,HMBTN_FISHING_CAST,HMBTN_FISHING_WAIT,HMBTN_FISHING_HOOKED,
    HMBTN_FISHING_REEL,HMBTN_FISHING_CATCH,HMBTN_FISHING_RECEIVE,HMBTN_FISHING_EAT,
    HMBTN_FISHING_RELEASE,HMBTN_FISHING_RELEASE_FLIGHT,HMBTN_FISHING_PRINT,
    HMBTN_FISHING_PRINT_HOLD,HMBTN_FISHING_PRINT_STORE
} HmbtnFishingPhase;
typedef struct HmbtnFishing {
    HmbtnFishingPhase phase;
    unsigned kind,wait,pull,streak,catch_flags,shown,caught,hold,bob_phase,bob_frame,direction;
    uint32_t random;
    int32_t marker[3],ground;
    int velocity,dx,dz,sound;
    bool marker_visible,water,bank_pending,using_action,finished_bank,berry_eaten,in_flight;
} HmbtnFishing;
void hmbtn_fishing_cancel(HmbtnFishing *f);
unsigned hmbtn_fishing_wait(HmbtnFishing *f,unsigned minute);
unsigned hmbtn_fishing_roll(HmbtnFishing *f,const HmbtnFarmClock *clock,bool flags[512]);
/* True consumes player controls. State/clock remain native; ground is decoded BIN data. */
bool hmbtn_fishing_tick(HmbtnFishing *f,HmbtnIntroActor *player,HmbtnHouseInteraction *inventory,
    HmbtnFarmAction *status,const HmbtnModel *ground,const HmbtnFarmClock *clock,
    unsigned location,bool flags[512],uint32_t pressed,uint32_t held);
#endif

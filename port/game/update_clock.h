#ifndef HMBTN_UPDATE_CLOCK_H
#define HMBTN_UPDATE_CLOCK_H
#include "services.h"
/* The original ~29.97 Hz is a gameplay cadence, not a rendering cap. */
#define HMBTN_UPDATE_NS (UINT64_C(1000000000)*1001/30000)
typedef struct HmbtnUpdateClock { uint64_t accumulated;uint32_t pressed,held; } HmbtnUpdateClock;
void hmbtn_update_clock_add(HmbtnUpdateClock *clock,uint64_t elapsed,HmbtnLogicalInput input);
bool hmbtn_update_clock_take(HmbtnUpdateClock *clock,HmbtnLogicalInput *input);
void hmbtn_update_clock_discard_input(HmbtnUpdateClock *clock);
#endif

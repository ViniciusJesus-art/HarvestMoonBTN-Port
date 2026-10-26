#include "update_clock.h"
void hmbtn_update_clock_add(HmbtnUpdateClock *c,uint64_t elapsed,HmbtnLogicalInput input)
{
    const uint64_t cap=HMBTN_UPDATE_NS*8;
    c->accumulated+=elapsed>cap ? cap : elapsed;
    c->pressed|=input.pressed;c->held=input.held;
}
bool hmbtn_update_clock_take(HmbtnUpdateClock *c,HmbtnLogicalInput *input)
{
    if(c->accumulated<HMBTN_UPDATE_NS) return false;
    c->accumulated-=HMBTN_UPDATE_NS;
    *input=(HmbtnLogicalInput){c->pressed,c->held};c->pressed=0;return true;
}
void hmbtn_update_clock_discard_input(HmbtnUpdateClock *c) { c->pressed=c->held=0; }

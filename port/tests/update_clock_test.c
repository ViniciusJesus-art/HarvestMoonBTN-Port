#include "update_clock.h"
#include "pad_buttons.h"
#include <assert.h>
int main(void)
{
    HmbtnUpdateClock c={0};HmbtnLogicalInput in={0};
    hmbtn_update_clock_add(&c,HMBTN_UPDATE_NS-1,(HmbtnLogicalInput){HMBTN_PAD_CROSS,HMBTN_PAD_CROSS});
    assert(!hmbtn_update_clock_take(&c,&in));
    hmbtn_update_clock_add(&c,1,(HmbtnLogicalInput){0});
    assert(hmbtn_update_clock_take(&c,&in) && in.pressed==HMBTN_PAD_CROSS && !in.held);
    hmbtn_update_clock_add(&c,HMBTN_UPDATE_NS*3,(HmbtnLogicalInput){HMBTN_PAD_START,HMBTN_PAD_RIGHT});
    for(unsigned i=0;i<3;++i) {
        assert(hmbtn_update_clock_take(&c,&in));
        assert(in.pressed==(i ? 0u : HMBTN_PAD_START) && in.held==HMBTN_PAD_RIGHT);
    }
    assert(!hmbtn_update_clock_take(&c,&in));
    hmbtn_update_clock_add(&c,UINT64_MAX,(HmbtnLogicalInput){HMBTN_PAD_CIRCLE,HMBTN_PAD_CIRCLE});
    hmbtn_update_clock_discard_input(&c);
    for(unsigned i=0;i<8;++i) assert(hmbtn_update_clock_take(&c,&in) && !in.pressed && !in.held);
    assert(!hmbtn_update_clock_take(&c,&in));
    const unsigned batches[]={30,60,144};
    for(unsigned k=0;k<3;++k) {
        c=(HmbtnUpdateClock){0};unsigned ticks=0;
        const uint64_t total=HMBTN_UPDATE_NS*120;
        for(unsigned frame=0;frame<batches[k];++frame) {
            hmbtn_update_clock_add(&c,total*(frame+1)/batches[k]-total*frame/batches[k],(HmbtnLogicalInput){0});
            while(hmbtn_update_clock_take(&c,&in)) ++ticks;
        }
        assert(ticks==120 && !c.accumulated);
    }
    return 0;
}

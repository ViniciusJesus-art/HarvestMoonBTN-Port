/* event_request.c: scene_item_queue_reset and scene_message_timers_tick. */
#include "event_days.h"
#include <string.h>
#include "event_day_defaults.inc"
void hmbtn_event_days_init(HmbtnEventDays *days)
{
    memset(days,0,sizeof(*days));
    memcpy(days->count0,event_day_count0,sizeof(days->count0));
    memset(days->count1,255,sizeof(days->count1));
}
static void tick(uint8_t *timer)
{
    if(*timer==255) return;
    *timer=(uint8_t)(*timer>=120 ? 120 : *timer+1);
}
void hmbtn_event_days_advance(HmbtnEventDays *days,const HmbtnFarmClock *clock,
    unsigned weather,bool wedding_offer_pending)
{
    for(unsigned i=0;i<128;++i) tick(&days->count0[i]);
    if(hmbtn_farm_clock_date_index(clock->season,clock->day)
            || wedding_offer_pending || weather==3 || weather==4) return;
    for(unsigned i=0;i<8;++i) tick(&days->count1[i]);
}

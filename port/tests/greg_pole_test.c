#include "save_fixture.h"
#include "psx_data.h"
#include <stdlib.h>
#include <stdio.h>
int main(void)
{
    HmbtnSession *s=save_fixture(1);HmbtnInventory *h=&s->game.inventory;
    memset(h,0,sizeof(*h));s->pond=(HmbtnPond){.random=1};
    uint8_t bytes[38]={0};hmbtn_write_le16(bytes,177|(2u<<10));bytes[2]=255;
    hmbtn_write_le32(bytes+8,0x3ffffcc6);bytes[12]=0xf7;bytes[13]=255;
    hmbtn_write_le32(bytes+16,0x3fffffff);hmbtn_write_le32(bytes+20,0x3fffffff);
    hmbtn_write_le32(bytes+24,0xfffff);hmbtn_write_le32(bytes+28,0x7ffe3800);
    bytes[32]=255;bytes[33]=61;bytes[34]=2;hmbtn_write_le16(bytes+36,65535);
    char error[256];HmbtnFarmTriggers t;assert(hmbtn_farm_triggers_open(&t,bytes,sizeof(bytes),error,sizeof(error)));t.pond=&s->pond;
    HmbtnFarmClock c={0,0,2,2,6,0,0};
    for(unsigned i=0;i<49;++i) assert(hmbtn_pond_add(&s->pond,163));
    hmbtn_pond_new_day(&s->pond);assert(s->pond.morning_count==49);
    assert(!hmbtn_farm_event_trigger(&t,&s->village,&c,0,2,2,0,h));
    assert(hmbtn_pond_add(&s->pond,163) && hmbtn_pond_count(&s->pond)==50 && s->pond.morning_count==49);
    assert(!hmbtn_farm_event_trigger(&t,&s->village,&c,0,2,2,0,h));
    /* Persist the difference: loading must not make today's 50th deposit an
     * eligible morning before the original daily callback has run. */
    uint8_t *save=NULL;size_t size=0;assert(hmbtn_save_encode(s,&save,&size,error,sizeof(error)));
    HmbtnSession *loaded=hmbtn_save_decode(save,size,error,sizeof(error));assert(loaded && loaded->pond.morning_count==49 && hmbtn_pond_count(&loaded->pond)==50);
    free(save);hmbtn_session_destroy(loaded);
    hmbtn_pond_new_day(&s->pond);assert(s->pond.morning_count==50);
    assert(hmbtn_farm_event_trigger(&t,&s->village,&c,0,2,2,0,h)==177);
    assert(!hmbtn_farm_event_trigger(&t,&s->village,&c,0,1,2,0,h) && !hmbtn_farm_event_trigger(&t,&s->village,&c,0,2,0,0,h));
    c.hour=5;assert(!hmbtn_farm_event_trigger(&t,&s->village,&c,0,2,2,0,h));
    c.hour=11;c.minute=59;assert(hmbtn_farm_event_trigger(&t,&s->village,&c,0,2,2,0,h)==177);
    c.hour=12;assert(!hmbtn_farm_event_trigger(&t,&s->village,&c,0,2,2,0,h));c.hour=6;c.minute=0;
    for(unsigned season=0;season<4;++season) for(unsigned day=0;day<7;++day) for(unsigned weather=0;weather<5;++weather) {
        c.season=season;c.weekday=day;assert(hmbtn_farm_event_trigger(&t,&s->village,&c,weather,2,2,0,h)==177);
    }
    h->equipped=(HmbtnStoredTool){56,0,1};h->bag[0]=(HmbtnStoredTool){1,0,1};h->bag[1]=(HmbtnStoredTool){6,0,1};
    assert(!hmbtn_farm_event_trigger(&t,&s->village,&c,0,2,2,0,h));
    h->bag[1]=(HmbtnStoredTool){0};h->held_item=(HmbtnStoredTool){58,0,1};
    assert(hmbtn_farm_event_trigger(&t,&s->village,&c,0,2,2,0,h)==177);
    HmbtnFarmVisit visit;assert(hmbtn_farm_visit_begin(&visit,&t,177,&s->village) && s->village.flags[251+177]);
    assert(!hmbtn_farm_event_trigger(&t,&s->village,&c,0,2,2,0,h));
    assert(hmbtn_pond_take(&s->pond)==163 && hmbtn_pond_count(&s->pond)==49 && s->pond.morning_count==50);
    /* The common presentation changes no item stores or attributes. */
    h->held_item=(HmbtnStoredTool){0};const HmbtnInventory before=*h;
    assert(hmbtn_event_item_tool(&s->event_item,&s->intro.player,57) && s->intro.player.sprite.animation==71);
    assert(!hmbtn_event_item_tool(&s->event_item,&s->intro.player,57));
    for(unsigned i=0;i<10;++i) { hmbtn_event_item_tick(&s->event_item,&s->intro.player,h,&s->farm_action);assert(s->event_item.phase==HMBTN_EVENT_ITEM_TOOL); }
    hmbtn_event_item_tick(&s->event_item,&s->intro.player,h,&s->farm_action);
    assert(!s->event_item.phase && !s->event_item.shown && !memcmp(h,&before,sizeof(*h)));
    hmbtn_session_destroy(s);puts("Greg pole: original entry/hours/weather/space, 49/50 morning snapshot and persistence, one-time trigger, occupied hands and 11-tick tool presentation passed");return 0;
}

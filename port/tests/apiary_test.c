#include "save_fixture.h"
#include "psx_data.h"
#include <stdio.h>
#include <stdlib.h>
static void trigger(uint8_t *p,unsigned event,unsigned condition)
{
    memset(p,0,36);hmbtn_write_le16(p,(uint16_t)(event|(2u<<10)));p[2]=255;
    hmbtn_write_le32(p+8,0x3ffffcc6);p[12]=0xf7;p[13]=255;
    hmbtn_write_le32(p+16,0x3fffffff);hmbtn_write_le32(p+20,0x3fffffff);
    hmbtn_write_le32(p+24,0xfffff);hmbtn_write_le32(p+28,0x7ffc3800);
    p[32]=255;p[33]=(uint8_t)condition;p[34]=2;
}
int main(void)
{
    HmbtnSession *s=save_fixture(1);char error[256]={0};HmbtnFarmClock c={0,0,2,2,6,0,0};
    uint8_t bytes[74];trigger(bytes,171,255);trigger(bytes+36,277,76);
    bytes[2]=40;bytes[3]=19;hmbtn_write_le16(bytes+4,1);hmbtn_write_le16(bytes+6,127);
    hmbtn_write_le16(bytes+72,65535);HmbtnFarmTriggers t;
    assert(hmbtn_farm_triggers_open(&t,bytes,sizeof(bytes),error,sizeof(error)));t.field=&s->field;
    memset(&s->game.inventory,0,sizeof(s->game.inventory));hmbtn_farm_field_clear(&s->field);
    s->village.days.count0[19]=255;
    assert(!hmbtn_farm_event_trigger(&t,&s->village,&c,0,2,2,0,&s->game.inventory));
    const unsigned flowers[]={0x31,0x32,0x178,0x179,0x34};
    for(unsigned i=0;i<5;++i) {
        const HmbtnFarmCrop *crop=hmbtn_farm_crop(flowers[i]);assert(crop);
        s->field.cells[0]=(HmbtnFarmTile){.value=(uint16_t)flowers[i],.ground=0x168,.kind=(uint8_t)(crop->grown-1)};
        assert(!hmbtn_farm_event_trigger(&t,&s->village,&c,0,2,2,0,&s->game.inventory));
        ++s->field.cells[0].kind;
        assert(hmbtn_farm_event_trigger(&t,&s->village,&c,0,2,2,0,&s->game.inventory)==277);
    }
    t.field=&s->greenhouse;s->greenhouse.cells[0]=s->field.cells[0];
    assert(!hmbtn_farm_event_trigger(&t,&s->village,&c,0,2,2,0,&s->game.inventory));t.field=&s->field;
    hmbtn_farm_field_clear(&s->greenhouse);
    c.hour=5;assert(!hmbtn_farm_event_trigger(&t,&s->village,&c,0,2,2,0,&s->game.inventory));
    c.hour=12;assert(!hmbtn_farm_event_trigger(&t,&s->village,&c,0,2,2,0,&s->game.inventory));
    c.hour=11;c.minute=59;assert(hmbtn_farm_event_trigger(&t,&s->village,&c,0,2,2,0,&s->game.inventory)==277);
    assert(!hmbtn_farm_event_trigger(&t,&s->village,&c,0,1,2,0,&s->game.inventory));
    assert(!hmbtn_farm_event_trigger(&t,&s->village,&c,0,2,0,0,&s->game.inventory));
    for(unsigned season=0;season<4;++season) for(unsigned weather=0;weather<5;++weather) {
        c.season=season;assert(hmbtn_farm_event_trigger(&t,&s->village,&c,weather,2,2,0,&s->game.inventory)==277);
    }
    HmbtnFarmVisit visit;assert(hmbtn_farm_visit_begin(&visit,&t,277,&s->village));
    assert(s->village.flags[528] && !hmbtn_farm_event_trigger(&t,&s->village,&c,0,2,2,0,&s->game.inventory));
    s->village.days.count0[19]=0;assert(!hmbtn_farm_event_trigger(&t,&s->village,&c,0,2,2,0,&s->game.inventory));
    s->village.days.count0[19]=1;assert(hmbtn_farm_event_trigger(&t,&s->village,&c,0,2,2,0,&s->game.inventory)==171);
    s->village.days.count0[19]=127;assert(hmbtn_farm_event_trigger(&t,&s->village,&c,0,2,2,0,&s->game.inventory)==171);
    s->village.days.count0[19]=128;assert(!hmbtn_farm_event_trigger(&t,&s->village,&c,0,2,2,0,&s->game.inventory));
    uint8_t section[0x2000]={0},*q=section+16;hmbtn_write_le16(q,0x4000);hmbtn_write_le16(q+2,47);
    for(unsigned axis=0;axis<3;++axis) { hmbtn_write_le16(q+14+axis*2,(uint16_t)-100);hmbtn_write_le16(q+20+axis*2,100); }
    HmbtnIntroActor p={.position={224,0,0},.facing=1};HmbtnFarmFeed f={0};
    HmbtnInventory *h=&s->game.inventory;
    assert(!hmbtn_apiary_tick(&s->apiary,&f,h,&p,section,sizeof(section),0));
    assert(hmbtn_apiary_tick(&s->apiary,&f,h,&p,section,sizeof(section),HMBTN_PAD_CROSS) && !h->held_item.id);
    s->apiary.owned=true;h->held_item=(HmbtnStoredTool){89,3,1};
    assert(hmbtn_apiary_tick(&s->apiary,&f,h,&p,section,sizeof(section),HMBTN_PAD_CROSS));
    assert(!s->apiary.collected && h->held_item.id==89 && !f.phase);
    h->held_item=(HmbtnStoredTool){0};p.facing=3;p.position[0]=-224;
    assert(hmbtn_apiary_tick(&s->apiary,&f,h,&p,section,sizeof(section),HMBTN_PAD_CROSS) && !s->apiary.collected);
    p.facing=1;p.position[0]=224;
    assert(hmbtn_apiary_tick(&s->apiary,&f,h,&p,section,sizeof(section),HMBTN_PAD_CROSS));
    assert(s->apiary.collected && h->held_item.id==151 && h->held_item.count==1 && !h->held_item.modifier && f.phase==1 && f.sound==7);
    p.animation_done=true;hmbtn_farm_feed_tick(&f,&s->house,&p,section,sizeof(section),0);
    h->held_item=(HmbtnStoredTool){0};assert(!f.phase);
    assert(hmbtn_apiary_tick(&s->apiary,&f,h,&p,section,sizeof(section),HMBTN_PAD_CROSS) && !h->held_item.id);
    uint8_t *saved=NULL;size_t size=0;assert(hmbtn_save_encode(s,&saved,&size,error,sizeof(error)));
    HmbtnSession *loaded=hmbtn_save_decode(saved,size,error,sizeof(error));assert(loaded && loaded->apiary.owned && loaded->apiary.collected);
    free(saved);hmbtn_session_destroy(loaded);
    s->apiary.owned=false;assert(!hmbtn_save_encode(s,&saved,&size,error,sizeof(error)));s->apiary.owned=true;
    hmbtn_session_clock_events(s,&s->game.clock,HMBTN_CLOCK_DAY);assert(s->apiary.collected);
    hmbtn_session_advance_minutes(s,1440);assert(s->apiary.owned && !s->apiary.collected);
    s->apiary.collected=true;hmbtn_session_clock_events(s,&s->game.clock,HMBTN_CLOCK_DAY);assert(s->apiary.collected);
    s->apiary.collected=false;p.position[0]=0;p.position[2]=-224;p.facing=2;
    assert(hmbtn_apiary_tick(&s->apiary,&f,h,&p,section,sizeof(section),HMBTN_PAD_CROSS) && h->held_item.id==151);
    assert(hmbtn_farm_shipping_price(&s->game.economy,151)==60); /* fixture bonus */
    s->game.economy.shipping.honey_bonus=false;assert(hmbtn_farm_shipping_price(&s->game.economy,151)==50);
    hmbtn_session_destroy(s);puts("Apiary: five mature flowers, outdoor-only trigger, entry/hours/weather/one-time, Louis timer, honey hands/facing/daily limit, persistence and prices passed");return 0;
}

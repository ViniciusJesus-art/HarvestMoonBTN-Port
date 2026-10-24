#include "farm_visit.h"
#include "event_resources.h"
#include "psx_data.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void entry(uint8_t *p,unsigned id,uint32_t flags)
{
    memset(p,0,38);hmbtn_write_le16(p,(uint16_t)(id|(63u<<10)));
    p[2]=255;p[12]=247;p[13]=255;p[33]=255;p[34]=1;
    hmbtn_write_le32(p+8,0x3fffffff);hmbtn_write_le32(p+16,0x3fffffff);
    hmbtn_write_le32(p+20,0x3fffffff);hmbtn_write_le32(p+24,0xfffff);
    hmbtn_write_le32(p+28,flags);hmbtn_write_le16(p+36,65535);
}
int main(void)
{
    uint8_t bytes[38];char error[128]={0};HmbtnFarmTriggers t;
    HmbtnVillage v;hmbtn_village_init(&v,NULL,0);
    HmbtnFarmClock clock={0,0,14,0,6,0,0};
    const uint32_t all=0x7ffc3800;
    entry(bytes,288,all|0x8000);
    assert(hmbtn_farm_triggers_open(&t,bytes,sizeof(bytes),error,sizeof(error)) && t.count==1);
    assert(!hmbtn_farm_triggers_open(&t,bytes,37,error,sizeof(error)));
    assert(!hmbtn_farm_triggers_open(&t,bytes,35,error,sizeof(error)));
    uint8_t full[3600]={0};assert(hmbtn_farm_triggers_open(&t,full,sizeof(full),error,sizeof(error)) && t.count==100);
    assert(hmbtn_farm_triggers_open(&t,bytes,sizeof(bytes),error,sizeof(error)));
    assert(hmbtn_farm_visit_trigger(&t,&v,&clock,0,2,0)==288);
    bytes[34]=2;assert(!hmbtn_farm_visit_trigger(&t,&v,&clock,0,2,0));
    assert(hmbtn_farm_visit_trigger(&t,&v,&clock,0,2,2)==288);bytes[34]=1;
    bytes[33]=84;assert(!hmbtn_farm_visit_trigger(&t,&v,&clock,0,2,0));bytes[33]=255;
    bytes[2]=40;bytes[3]=26;hmbtn_write_le16(bytes+4,0);hmbtn_write_le16(bytes+6,0);
    assert(!hmbtn_farm_visit_trigger(&t,&v,&clock,0,2,0));
    hmbtn_write_le32(bytes+28,all|0x8400);
    assert(hmbtn_farm_visit_trigger(&t,&v,&clock,0,2,0)==288);
    HmbtnFarmVisit visit={0};assert(hmbtn_farm_visit_begin(&visit,&t,288,&v) && !v.flags[539]);
    HmbtnFarmIntro intro={0};HmbtnEventCommand c={.type=19,.command=40,.operand=26};
    assert(hmbtn_farm_visit_command(&visit,&intro,&v,&clock,&c,error,sizeof(error))==HMBTN_EVENT_DONE);
    assert(!hmbtn_farm_visit_trigger(&t,&v,&clock,0,2,0));
    hmbtn_village_clock(&v,&clock,HMBTN_CLOCK_DAY,0,false);
    assert(hmbtn_farm_visit_trigger(&t,&v,&clock,0,2,0)==288);
    /* An inclusive day window, a six-hour span, year range, weather and calendar masks. */
    hmbtn_write_le32(bytes+8,(1u<<25)|(2u<<20)|(15u<<15)|(0u<<10)|(6u<<5)|6u);
    assert(hmbtn_farm_visit_trigger(&t,&v,&clock,0,2,0)==288);
    clock.hour=11;clock.minute=59;assert(hmbtn_farm_visit_trigger(&t,&v,&clock,0,2,0)==288);
    clock.hour=12;assert(!hmbtn_farm_visit_trigger(&t,&v,&clock,0,2,0));clock.hour=6;
    clock.day=15;assert(!hmbtn_farm_visit_trigger(&t,&v,&clock,0,2,0));clock.day=14;
    clock.year=2;assert(!hmbtn_farm_visit_trigger(&t,&v,&clock,0,2,0));clock.year=0;
    hmbtn_write_le32(bytes+28,(all&~0xC3800u)|0x80000|0x8400);
    assert(!hmbtn_farm_visit_trigger(&t,&v,&clock,1,2,0));
    /* Required/clear event flags and story flags use different original bases. */
    hmbtn_write_le32(bytes+16,0x3ffffc05);assert(!hmbtn_farm_visit_trigger(&t,&v,&clock,0,2,0));
    v.flags[256]=true;assert(hmbtn_farm_visit_trigger(&t,&v,&clock,0,2,0)==288);
    hmbtn_write_le32(bytes+24,0xffc03);assert(!hmbtn_farm_visit_trigger(&t,&v,&clock,0,2,0));
    v.flags[203]=true;assert(hmbtn_farm_visit_trigger(&t,&v,&clock,0,2,0)==288);
    entry(bytes,128,all);assert(hmbtn_farm_visit_begin(&visit,&t,128,&v) && v.flags[379]);
    assert(!hmbtn_farm_visit_trigger(&t,&v,&clock,0,2,0));
    /* Real state changes, saturation and a one-hour wait across midnight. */
    v.people[17].friendship=250;c=(HmbtnEventCommand){.type=19,.command=32,.operand=17,.x=20,.y=1};
    assert(hmbtn_farm_visit_command(&visit,&intro,&v,&clock,&c,error,sizeof(error))==HMBTN_EVENT_DONE && v.people[17].friendship==255);
    c.x=-300;assert(hmbtn_farm_visit_command(&visit,&intro,&v,&clock,&c,error,sizeof(error))==HMBTN_EVENT_DONE && !v.people[17].friendship);
    intro.slots[10]=24;intro.slots[11]=30;clock.hour=0;clock.minute=29;
    c=(HmbtnEventCommand){.type=36,.command=10,.operand=11,.x=3};
    assert(hmbtn_farm_visit_command(&visit,&intro,&v,&clock,&c,error,sizeof(error))==HMBTN_EVENT_WAIT);
    clock.minute=30;assert(hmbtn_farm_visit_command(&visit,&intro,&v,&clock,&c,error,sizeof(error))==HMBTN_EVENT_DONE);
    c=(HmbtnEventCommand){.type=11,.command=2,.operand=1,.x=1};visit.talk=true;
    assert(hmbtn_farm_visit_command(&visit,&intro,&v,&clock,&c,error,sizeof(error))==HMBTN_EVENT_DONE
        && intro.slots[2]==17 && !visit.free && !visit.talk && (v.people[17].talk_flags&HMBTN_VILLAGER_DAILY_GAIN));
    HmbtnEventResourceRange range;
    assert(hmbtn_event_resource_locate(HMBTN_EVENT_RESOURCE_SCRIPT,12,&range) && range.bytes==0xb800);
    assert(hmbtn_event_resource_locate(HMBTN_EVENT_RESOURCE_SCRIPT,128,&range) && range.bytes>44*16);
    assert(hmbtn_event_resource_locate(HMBTN_EVENT_RESOURCE_TRIGGERS,0,&range) && range.bytes<=4096);
    assert(!hmbtn_event_resource_locate(HMBTN_EVENT_RESOURCE_TRIGGERS,255,&range));
    assert(!hmbtn_event_resource_locate(HMBTN_EVENT_RESOURCE_SCRIPT,UINT32_MAX,&range));
    assert(!hmbtn_event_resource_locate((HmbtnEventResourceKind)2,0,&range));
    /* Delivery mode/target and held identity: a pie in the bag is not offered. */
    entry(bytes,130,all);bytes[33]=39;bytes[34]=3;bytes[35]=23;
    HmbtnHouseInteraction inventory={0};inventory.held_item=(HmbtnStoredTool){204,0,1};
    assert(hmbtn_farm_event_trigger(&t,&v,&clock,0,2,3,23,&inventory)==130);
    assert(!hmbtn_farm_event_trigger(&t,&v,&clock,0,2,3,9,&inventory));
    assert(!hmbtn_farm_event_trigger(&t,&v,&clock,0,2,2,23,&inventory));
    inventory.items[0]=inventory.held_item;inventory.held_item=(HmbtnStoredTool){0};
    assert(!hmbtn_farm_event_trigger(&t,&v,&clock,0,2,3,23,&inventory));
    entry(bytes,129,all|0x10000);bytes[34]=10;
    assert(hmbtn_farm_event_trigger(&t,&v,&clock,0,2,10,6,&inventory)==129);
    inventory.held_item=(HmbtnStoredTool){204,0,1};
    assert(!hmbtn_farm_event_trigger(&t,&v,&clock,0,2,10,6,&inventory));
    puts("Farm visit trigger gates, state effects, repeat policy and logical resource identities passed.");return 0;
}

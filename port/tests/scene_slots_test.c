#include "scene_slots.h"
#include "psx_data.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static void frame(uint8_t *s,unsigned i,unsigned id,unsigned duration,unsigned next,unsigned x,unsigned y,unsigned sx,unsigned sy,unsigned w,unsigned h)
{
    unsigned v[]={id,duration,next,x,y,sx,sy,w,h};for(unsigned j=0;j<9;++j) hmbtn_write_le16(s+0x8a8+i*18+j*2,(uint16_t)v[j]);
}
static void empty(uint8_t *s)
{ memset(s,0,0x2000);for(unsigned i=0;i<8;++i) frame(s,i,65535,0,0,0,0,0,0,0,0); }
int main(void)
{
    HmbtnImageAtlas *a=calloc(1,sizeof(*a));assert(a);HmbtnSceneSlots slots;uint8_t s[0x2000];char e[256];empty(s);
    frame(s,0,0,1,0,20,20,0,0,2,5);
    for(unsigned y=0;y<5;++y) for(unsigned x=0;x<2;++x) { a->words[y*1024+x]=(uint16_t)(y*2+x+1);a->present[y*1024+x]=1; }
    assert(hmbtn_scene_slots_open(&slots,s,sizeof(s),0,e,sizeof(e)));
    hmbtn_scene_slots_tick(&slots,a,false,false);assert(slots.slots[0].timer==2 && a->words[20*1024+20]==1);
    hmbtn_scene_slots_tick(&slots,a,true,false);assert(slots.slots[0].timer==2);
    hmbtn_scene_slots_tick(&slots,a,false,false);assert(a->words[20*1024+20]==7 && a->words[22*1024+20]==1 && a->present[20*1024+20]);
    for(unsigned i=0;i<1;++i) hmbtn_scene_slots_tick(&slots,a,false,false);
    assert(slots.slots[0].timer==0); /* Odd source heights wrap without overread. */
    empty(s);hmbtn_write_le16(s+2,44);frame(s,0,0x8001,2,1,30,30,0,0,2,2);frame(s,1,0x8001,1,0,30,30,0,2,2,2);
    assert(hmbtn_scene_slots_open(&slots,s,sizeof(s),0,e,sizeof(e)) && slots.slots[0].active && !slots.slots[1].active);
    hmbtn_scene_slots_tick(&slots,a,false,false);assert(a->words[30*1024+30]==1 && a->words[30*1024+38]==1 && a->words[30*1024+46]==1);
    hmbtn_scene_slots_tick(&slots,a,false,false);hmbtn_scene_slots_tick(&slots,a,false,false);assert(a->words[30*1024+30]==5);
    /* Original beach clear color is required by the translucent ocean. */
    assert(slots.background[0]==0 && slots.background[1]==165 && slots.background[2]==213);
    assert(hmbtn_scene_slots_open(&slots,s,sizeof(s),0,e,sizeof(e)));
    hmbtn_scene_slots_tick(&slots,a,false,false);
    assert(slots.background_reversed && slots.background_step==511 && slots.background[1]==165);
    for(unsigned i=0;i<512;++i) hmbtn_scene_slots_tick(&slots,a,false,false);
    assert(!slots.background_reversed && !slots.background_step && slots.background[1]==99 && slots.background[2]==127);
    hmbtn_scene_slots_tick(&slots,a,true,false);
    assert(!slots.background_step && slots.background[1]==99);
    for(unsigned i=0;i<512;++i) hmbtn_scene_slots_tick(&slots,a,false,false);
    assert(slots.background_step==511 && slots.background[1]==165 && slots.background[2]==213);
    hmbtn_write_le32(s+4,91);assert(hmbtn_scene_slots_open(&slots,s,sizeof(s),0,e,sizeof(e)));
    hmbtn_scene_slots_tick(&slots,a,false,false);assert(!slots.slots[0].timer);
    hmbtn_scene_slots_tick(&slots,a,false,true);assert(slots.slots[0].timer==1);
    hmbtn_write_le16(s+2,15);assert(hmbtn_scene_slots_open(&slots,s,sizeof(s),1,e,sizeof(e)) && !slots.slots[0].active);
    assert(!slots.background[0] && !slots.background[1] && !slots.background[2]);
    assert(!hmbtn_scene_slots_open(&slots,s,0x937,0,e,sizeof(e)));
    frame(s,0,0x8000,1,8,0,0,0,0,2,2);assert(!hmbtn_scene_slots_open(&slots,s,sizeof(s),0,e,sizeof(e)));
    frame(s,0,0,1,0,1023,0,0,0,2,2);assert(!hmbtn_scene_slots_open(&slots,s,sizeof(s),0,e,sizeof(e)));
    free(a);puts("Scene slots: scroll/wrap/presence, image cadence/dedup/beach copies, pause/weather/fireplace and bounds passed");return 0;
}

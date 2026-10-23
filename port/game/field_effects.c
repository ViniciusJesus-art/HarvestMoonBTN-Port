/* field_item_requests.c: 25 slots, range 1, one frame per three ticks. */
#include "field_effects.h"
#include "psx_data.h"
bool hmbtn_field_effect_submit(HmbtnFieldEffect slots[HMBTN_FIELD_EFFECTS],unsigned item,int col,int row)
{
    unsigned art;
    switch(item) {
    case 0x3a: art=0;break;
    case 0x3b: case 0x16c: case 0x16d: case 0x16e: art=1;break;
    case 0x3c: case 0x16f: case 0x170: case 0x171: art=2;break;
    case 0x3d: case 0x172: case 0x173: case 0x174: art=3;break;
    case 0x3e: art=4;break;
    case 0x3f: case 0x175: art=5;break;
    case 0x40: art=6;break;
    default:return false;
    }
    for(unsigned i=0;i<HMBTN_FIELD_EFFECTS;++i) if(!slots[i].active) {
        slots[i]=(HmbtnFieldEffect){art,0,{col*400-4200,0,1400-row*400},true};return true;
    }
    return false;
}
void hmbtn_field_effect_tick(HmbtnFieldEffect slots[HMBTN_FIELD_EFFECTS],const HmbtnAnimation art[7])
{
    for(unsigned i=0;i<HMBTN_FIELD_EFFECTS;++i) if(slots[i].active) {
        HmbtnFieldEffect *e=&slots[i];const HmbtnAnimation *a=&art[e->art];
        if(!a->data || a->count[3]<2 || ++e->tick/3>=hmbtn_read_le16(a->data+a->section[3]+4)) e->active=false;
    }
}

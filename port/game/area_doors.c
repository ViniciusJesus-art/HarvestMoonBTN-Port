#include "area_doors.h"
#include "psx_data.h"
#include <string.h>

void hmbtn_area_doors_bind(HmbtnAreaDoors *d,const HmbtnSceneData *scene)
{
    if(d->bound || !scene) return;
    memset(d,0,sizeof(*d));d->bound=true;
    for(unsigned i=0;i<HMBTN_SCENE_MODELS;++i) { d->base[i]=scene->angle[i][1];d->models[i].twin=-1; }
}
HmbtnDoorPassage hmbtn_area_doors_request(HmbtnAreaDoors *d,const uint8_t *section,size_t size,unsigned event,unsigned facing)
{
    if(!d || !d->bound || !section || size<0x2000 || facing>3 || event==0xffff) return HMBTN_DOOR_UNHANDLED;
    for(unsigned i=0;i<50;++i) {
        const uint8_t *zone=section+16+i*44;
        const unsigned flags=hmbtn_read_le16(zone),model=hmbtn_read_le16(zone+12);
        if(hmbtn_read_le16(zone+10)!=event || hmbtn_read_le16(zone+8)>3
                || model>=HMBTN_SCENE_MODELS || (flags!=0x9000 && flags!=0x1000)) continue;
        /* ZONE id -2 is a swinging door within this map, not an area exit.
         * Both sides share the same event/model (inn rooms, clinic, etc.). */
        if(flags==0x1000 && hmbtn_read_le16(zone+2)!=0xfffe) return HMBTN_DOOR_READY;
        HmbtnAreaDoor *door=&d->models[model];
        /* Two route events can address the opposite leaves of one door. */
        for(unsigned j=0;j<HMBTN_SCENE_MODELS;++j)
            if(d->models[j].twin==(int)model) { door=&d->models[j];break; }
        const uint8_t *record=section+0x948+model*44;
        const unsigned twin=hmbtn_read_le16(record+8);
        /* SCENE_ENTRY_TWIN is serialized in the model's first halfword. */
        if(hmbtn_read_le16(record)&0x400u) {
            if(twin>=HMBTN_SCENE_MODELS || twin==model) return HMBTN_DOOR_UNHANDLED;
            if(door==&d->models[model]) door->twin=(int)twin;
        }
        if(door->phase==HMBTN_DOOR_HELD) { door->hold=11;return HMBTN_DOOR_READY; }
        if(door->phase!=HMBTN_DOOR_OPENING) {
            door->opening_sound=door->phase==HMBTN_DOOR_CLOSED;
            /* Keep the swing direction if someone catches a closing door. */
            if(door->phase==HMBTN_DOOR_CLOSED) door->facing=facing;
            door->speed=0;door->phase=HMBTN_DOOR_OPENING;
        }
        return HMBTN_DOOR_WAIT;
    }
    return HMBTN_DOOR_UNHANDLED;
}
void hmbtn_area_doors_tick(HmbtnAreaDoors *d,bool paused)
{
    if(paused || !d->bound) return;
    for(unsigned i=0;i<HMBTN_SCENE_MODELS;++i) {
        HmbtnAreaDoor *p=&d->models[i];
        if(p->phase==HMBTN_DOOR_HELD) { if(p->hold && !--p->hold) p->phase=HMBTN_DOOR_CLOSING;continue; }
        if(p->phase!=HMBTN_DOOR_OPENING && p->phase!=HMBTN_DOOR_CLOSING) continue;
        p->speed+=p->phase==HMBTN_DOOR_OPENING ? 8 : -8;
        p->turn+=(p->facing==1 || p->facing==2) ? -p->speed : p->speed;
        const int direction=(p->facing==1 || p->facing==2) ? -1 : 1;
        if(p->phase==HMBTN_DOOR_OPENING && p->turn*direction>=0x420) {
            p->phase=HMBTN_DOOR_HELD;p->hold=11;p->speed=0;p->turn=direction*0x420;
        } else if(p->phase==HMBTN_DOOR_CLOSING && p->turn*direction<=0) {
            p->phase=HMBTN_DOOR_CLOSED;p->turn=p->speed=0;p->closing_sound=true;
        }
    }
}
void hmbtn_area_doors_apply(const HmbtnAreaDoors *d,HmbtnSceneData *scene)
{
    if(!d->bound || !scene) return;
    for(unsigned i=0;i<HMBTN_SCENE_MODELS;++i) {
        const HmbtnAreaDoor *p=&d->models[i];
        /* Untouched models may be driven by scene animation or scripts. */
        if(p->phase==HMBTN_DOOR_CLOSED && !p->closing_sound) continue;
        scene->angle[i][1]=(uint16_t)(d->base[i]+p->turn);
        if(p->twin>=0) scene->angle[p->twin][1]=(uint16_t)(d->base[p->twin]-p->turn);
    }
}
void hmbtn_area_doors_claim(HmbtnAreaDoors *d,HmbtnSceneData *scene,unsigned model)
{
    if(!d->bound || !scene || model>=HMBTN_SCENE_MODELS) return;
    for(unsigned i=0;i<HMBTN_SCENE_MODELS;++i) {
        HmbtnAreaDoor *p=&d->models[i];
        if(i!=model && p->twin!=(int)model) continue;
        scene->angle[i][1]=d->base[i];
        if(p->twin>=0) scene->angle[p->twin][1]=d->base[p->twin];
        *p=(HmbtnAreaDoor){.twin=-1};
    }
}

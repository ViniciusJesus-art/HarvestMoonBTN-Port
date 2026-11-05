/* world/scene_view.c: scene_mode_queue_run, slot_image_step/slot_rect_step.
 * MoveImage works in 16-bit VRAM words, including CLUT/presence data. */
#include "scene_slots.h"
#include "psx_data.h"
#include <stdio.h>
#include <string.h>
static bool valid(HmbtnSceneFrame f,unsigned location)
{
    if(f.id==0xffff) return true;
    const unsigned extra=location==44 && (f.id&0x8001u)==0x8001u ? 16 : 0;
    return f.w && f.h && f.x+f.w+extra<=1024 && f.y+f.h<=512
        && f.sx+f.w<=1024 && f.sy+f.h<=512 && (!(f.id&0x8000u) || (f.duration<32768 && f.next<8));
}
bool hmbtn_scene_slots_open(HmbtnSceneSlots *s,const uint8_t *section,size_t size,unsigned weather,char *error,size_t bytes)
{
    if(!s || !section || size<0x938) { snprintf(error,bytes,"Missing scene image queue");return false; }
    memset(s,0,sizeof(*s));s->location=hmbtn_read_le16(section+2);s->scene=hmbtn_read_le32(section+4);
    /* scene.c: scene_interp_start; beach water blends over this clear color. */
    if(s->location==44) { s->background_step=511;s->background[1]=165;s->background[2]=213; }
    for(unsigned i=0;i<8;++i) {
        HmbtnSceneFrame *f=&s->frames[i];const uint8_t *p=section+0x8a8+i*18;
        uint16_t values[9];for(unsigned j=0;j<9;++j) values[j]=hmbtn_read_le16(p+j*2);
        *f=(HmbtnSceneFrame){values[0],values[1],values[2],values[3],values[4],values[5],values[6],values[7],values[8]};
        if(!valid(*f,s->location)) { snprintf(error,bytes,"Scene image rectangle outside VRAM or invalid next frame");return false; }
    }
    if(s->location==15 && (weather==1 || weather==2)) return true;
    unsigned count=0;
    for(unsigned i=0;i<8;++i) {
        HmbtnSceneFrame f=s->frames[i];if(f.id==0xffff) continue;
        bool found=false;for(unsigned j=0;j<count;++j) if(s->slots[j].frame.id==f.id) found=true;
        if(!found) s->slots[count++]=(HmbtnSceneSlot){.frame=f,.active=true};
    }
    return true;
}
static void copy(HmbtnImageAtlas *a,unsigned sx,unsigned sy,unsigned w,unsigned h,unsigned x,unsigned y)
{
    /* Row ordering and memmove preserve overlapping source rectangles. */
    for(unsigned i=0;i<h;++i) {
        unsigned row=y>sy ? h-1-i : i;
        size_t from=(sy+row)*1024+sx,to=(y+row)*1024+x;
        memmove(a->words+to,a->words+from,w*sizeof(*a->words));
        memmove(a->present+to,a->present+from,w);
    }
}
void hmbtn_scene_slots_tick(HmbtnSceneSlots *s,HmbtnImageAtlas *a,bool paused,bool fireplace)
{
    if(!s || !a || paused) return;
    /* scene_interp_step/math_lerp_vec3, including signed division rounding
     * and one repeated tick at each end of the 512-step cycle. */
    if(s->location==44) {
        int remaining=511-(int)s->background_step;
        s->background[1]=(uint8_t)(165-66*remaining/511);
        s->background[2]=(uint8_t)(213-86*remaining/511);
        if(!s->background_reversed) {
            if(++s->background_step==512) { s->background_step=511;s->background_reversed=true; }
        } else if(!s->background_step) s->background_reversed=false;
        else --s->background_step;
    }
    for(unsigned i=0;i<8;++i) {
        HmbtnSceneSlot *slot=&s->slots[i];HmbtnSceneFrame f=slot->frame;if(!slot->active) continue;
        if(f.id&0x8000u) {
            if(s->scene==91 && !fireplace) continue;
            copy(a,f.sx,f.sy,f.w,f.h,f.x,f.y);
            if(s->location==44 && (f.id&1u)) {
                copy(a,f.sx,f.sy,f.w,f.h,f.x+8,f.y);copy(a,f.sx,f.sy,f.w,f.h,f.x+16,f.y);
            }
            if(++slot->timer>=f.duration) {
                HmbtnSceneFrame next=s->frames[f.next];
                if(next.id==0xffff) { slot->active=false;continue; }
                next.id=f.id;slot->frame=next;slot->timer=0;
            }
        } else {
            copy(a,f.sx,f.sy,f.w,f.h-slot->timer,f.x,f.y+slot->timer);
            if(slot->timer) copy(a,f.sx,f.sy+f.h-slot->timer,f.w,slot->timer,f.x,f.y);
            if(slot->count==f.next) { slot->timer+=2;slot->count=0;if(slot->timer>=f.h) slot->timer=0; }
            else ++slot->count;
        }
    }
}

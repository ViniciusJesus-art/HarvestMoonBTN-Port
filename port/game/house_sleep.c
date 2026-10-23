#include "house_sleep.h"
#include <string.h>
#include <stdlib.h>
void hmbtn_house_sleep_start(HmbtnHouseSleep *s,HmbtnIntroActor *p)
{
    memset(s,0,sizeof(*s)); s->active=true;
    memcpy(s->from,p->position,sizeof(s->from)); memcpy(s->camera,p->position,sizeof(s->camera));
    p->facing=0; p->pose=true; p->animation_done=false;
    p->sprite=(HmbtnAnimationPlayer){0};
}
static void next(HmbtnHouseSleep *s,HmbtnIntroActor *p)
{
    ++s->phase; s->ticks=0;
    p->sprite=(HmbtnAnimationPlayer){(uint16_t)s->phase,0,0}; p->animation_done=false;
    if(s->phase==2) s->sleeping=true;
}
void hmbtn_house_sleep_tick(HmbtnHouseSleep *s,HmbtnIntroActor *p)
{
    if(!s->active || s->boundary) return;
    if(s->phase==0 && p->animation_done) next(s,p);
    else if(s->phase==1) {
        const int32_t target[3]={-1350,s->from[1],-960};
        const int frame=(int)s->travel;
        for(unsigned i=0;i<3;++i) s->camera[i]=(s->from[i]-target[i])*(17-frame)/17+target[i];
        if(s->camera[1]<s->from[1]-40) s->camera[1]=s->from[1]-40;
        p->position[0]=s->camera[0]; p->position[2]=s->camera[2];
        const int offset=abs(frame-8)*111/8;
        p->position[1]+=frame<9 ? -offset : offset;
        if(p->position[1]>=s->from[1]-40 || frame==17) p->position[1]=s->from[1]-40;
        if(frame==17 && p->animation_done) next(s,p);
        else if(frame<17) ++s->travel;
    } else if(s->phase==2 && ++s->ticks==16) next(s,p);
    else if(s->phase==3 && p->animation_done) { s->phase=4; s->ticks=0; }
    else if(s->phase==4) { if(s->fade<30) ++s->fade; if(s->fade==30) s->boundary=true; }
    if(s->link_state==0) { s->link_state=1; }
    else if(s->link_state==1) {
        if(s->link_step<8) ++s->link_step;
        else {
            if(!s->second_bed) s->second_bed=true;
            if(s->sleeping) { s->link_state=2; s->link_step=0; }
        }
    } else if(s->link_state==2 && s->link_step<12) ++s->link_step;
}

/* farm_main.c zone walks; player.c jump/bath modes. No door model mutation. */
#include "hot_spring.h"
#include "pad_buttons.h"
#include "psx_data.h"
#include <string.h>
static int half(const uint8_t *p)
{ unsigned n=hmbtn_read_le16(p);return n<32768 ? (int)n : (int)n-65536; }
static void pose(HmbtnIntroActor *p,unsigned animation,unsigned frame)
{
    p->sprite=(HmbtnAnimationPlayer){(uint16_t)animation,(uint16_t)frame,0};
    p->animation_done=false;p->pose=true;
}
void hmbtn_hot_spring_bind(HmbtnHotSpring *s,const uint8_t *section,size_t size)
{
    s->door_known=false;
    if(!section || size<0x2000 || hmbtn_read_le16(section+2)!=12) return;
    for(unsigned i=0;i<50;++i) {
        const uint8_t *z=section+16+i*44;unsigned model=hmbtn_read_le16(z+12);
        if(hmbtn_read_le16(z)!=0x1000 || hmbtn_read_le16(z+2)!=0xfffe || hmbtn_read_le16(z+10)!=46 || model>=15) continue;
        s->door_known=true;s->door_model=model;s->door_xmin=half(z+14);s->door_xmax=half(z+20);
        s->door_z=half(section+0x948+model*44+24);s->door_y=half(z+16);s->door_front=half(z+18);return;
    }
}
bool hmbtn_hot_spring_blocked(const HmbtnHotSpring *s,const HmbtnAreaDoors *doors,const int32_t from[3],const int32_t to[3])
{
    if(!s->door_known || (doors->bound && doors->models[s->door_model].phase==HMBTN_DOOR_HELD)
            || to[0]<=s->door_xmin-48 || to[0]>=s->door_xmax+48
            || to[1]<s->door_y-200 || to[1]>s->door_y+200) return false;
    /* house_control already probes 48 units ahead of the player. Adding
     * another radius here stops the feet before the original entry zone. */
    return (from[2]<=s->door_z && to[2]>from[2] && to[2]>=s->door_z)
        || (from[2]>=s->door_z && to[2]<from[2] && to[2]<=s->door_z);
}
bool hmbtn_hot_spring_begin(HmbtnHotSpring *s,HmbtnIntroActor *p,bool occupied,
    const uint8_t *section,size_t size,uint32_t pressed)
{
    if(!s || !p || s->phase || !(pressed&HMBTN_PAD_CROSS) || !section || size<16+50*44
            || hmbtn_read_le16(section+2)!=12) return false;
    for(unsigned i=0;i<50;++i) {
        const uint8_t *z=section+16+i*44;
        if(hmbtn_read_le16(z)!=0x1000 || hmbtn_read_le16(z+2)!=0xfffe || hmbtn_read_le16(z+10)!=46
                || p->facing!=hmbtn_read_le16(z+8) || p->position[0]<=half(z+14) || p->position[0]>=half(z+20)
                || p->position[2]<=half(z+18) || p->position[2]>=half(z+24)) continue;
        if(occupied) return true;
        *s=(HmbtnHotSpring){.phase=HMBTN_SPRING_APPROACH,.bank=-1,.sound=-1};
        hmbtn_hot_spring_bind(s,section,size);
        for(unsigned j=0;j<50;++j) {
            const uint8_t *out=section+16+j*44;
            if(hmbtn_read_le16(out)!=0x1000 || hmbtn_read_le16(out+2)!=0xfffe
                    || hmbtn_read_le16(out+10)!=47 || hmbtn_read_le16(out+8)!=0) continue;
            s->exit_known=true;s->exit_box[0]=half(out+14);s->exit_box[1]=half(out+20);
            s->exit_box[2]=half(out+18);s->exit_box[3]=half(out+24);break;
        }
        memset(p->velocity,0,sizeof(p->velocity));pose(p,1+p->facing,0);return true;
    }
    return false;
}
static void walk(HmbtnIntroActor *p,const HmbtnModel *ground)
{
    int32_t next[3]={p->position[0],p->position[1],p->position[2]+(p->facing==2 ? 41 : -41)};
    if(hmbtn_farm_settle(NULL,ground,next,0xE080,200)) memcpy(p->position,next,sizeof(next));
}
static void fly(HmbtnHotSpring *s,HmbtnIntroActor *p,const HmbtnModel *ground,int speed)
{
    p->position[2]+=p->facing==2 ? speed : -speed;
    if(s->landed) {
        HmbtnGroundHit hit;
        if(hmbtn_ground_find(ground,p->position,0xC000,200,&hit)) p->position[1]=hit.height;
        return;
    }
    const int32_t old=p->position[1];p->position[1]+=s->velocity;
    HmbtnGroundHit hit;
    if(s->velocity>=0 && hmbtn_ground_find(ground,p->position,0xC000,128,&hit)
            && old<=hit.height && p->position[1]>=hit.height-50) {
        p->position[1]=hit.height;s->velocity=0;s->landed=true;
    } else { s->velocity+=16;if(s->velocity>128) s->velocity=128; }
}
void hmbtn_hot_spring_tick(HmbtnHotSpring *s,HmbtnIntroActor *p,const HmbtnModel *ground,uint32_t pressed)
{
    s->bank=s->sound=-1;s->finished=false;
    switch(s->phase) {
    case HMBTN_SPRING_OFF:return;
    case HMBTN_SPRING_APPROACH:
        walk(p,ground);
        if(++s->ticks==37) {
            s->phase=HMBTN_SPRING_ENTER;s->ticks=0;s->landed=false;s->velocity=-140;
            p->position[0]=-8838;p->facing=2;pose(p,4,2);s->bank=0;s->sound=6;
        }
        break;
    case HMBTN_SPRING_ENTER:
        fly(s,p,ground,73);++s->ticks;
        /* The original timer freezes the swimmer under the water surface;
         * most of this pool is flagged 0x8040 and excluded by jump mask C000.
         * Waiting for an ordinary ground landing would fall through it. */
        if(s->ticks==20) {
            s->velocity=0;
            s->phase=HMBTN_SPRING_SPLASH;s->ticks=0;p->facing=0;pose(p,0,0);s->sound=16;
        }
        break;
    case HMBTN_SPRING_SPLASH:
        if(p->animation_done) {
            s->phase=HMBTN_SPRING_SOAK;s->ticks=0;pose(p,0,0);s->bank=1;
        }
        break;
    case HMBTN_SPRING_SOAK:
        if(pressed&HMBTN_PAD_CROSS) {
            s->phase=HMBTN_SPRING_LEAVE;s->ticks=0;s->landed=false;s->velocity=-250;
            p->facing=0;pose(p,4,0);s->bank=16;s->sound=6;
        } else if(++s->ticks==300) { s->phase=HMBTN_SPRING_REST;s->ticks=0;pose(p,1,0); }
        break;
    case HMBTN_SPRING_REST:
        if((pressed&HMBTN_PAD_CROSS) || ++s->ticks==300) { s->phase=HMBTN_SPRING_SOAK;s->ticks=0;pose(p,0,0); }
        break;
    case HMBTN_SPRING_LEAVE:
        fly(s,p,ground,51);++s->ticks;
        if(s->exit_known && p->position[0]>s->exit_box[0] && p->position[0]<s->exit_box[1]
                && p->position[2]>s->exit_box[2] && p->position[2]<s->exit_box[3]) s->exit_pending=true;
        /* Zone 47 is automatic, even while airborne. Its passage replaces
         * the launch action once the ground permits the forced walk. */
        if(s->landed && (s->exit_pending || s->ticks>=45)) {
            s->phase=HMBTN_SPRING_WALK;s->ticks=0;p->facing=s->exit_pending ? 0 : 2;
            s->walk_ticks=s->exit_pending ? 37 : 20;pose(p,1+p->facing,0);s->bank=-2;
        }
        break;
    case HMBTN_SPRING_WALK:
        walk(p,ground);
        /* Summer/autumn/winter's exit box starts inside the changing room.
         * Clear the entrance as well, then release control immediately. */
        if(++s->ticks==s->walk_ticks || (s->exit_known && p->facing==0
                && p->position[2]<=(s->door_known && s->door_front<s->exit_box[2] ? s->door_front : s->exit_box[2])-48)) {
            s->phase=HMBTN_SPRING_OFF;s->finished=true;pose(p,0,p->facing);p->pose=false;
        }
        break;
    }
}
void hmbtn_hot_spring_minute(const HmbtnHotSpring *s,HmbtnFarmAction *status)
{
    if(s->phase!=HMBTN_SPRING_SOAK && s->phase!=HMBTN_SPRING_REST) return;
    status->stamina=status->max_stamina-status->stamina<2 ? status->max_stamina : status->stamina+2;
    if(status->fatigue) --status->fatigue;
}

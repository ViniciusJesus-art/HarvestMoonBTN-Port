/* Adapted from player_dpad_facing/player_update_mode/player_slide_on_bump and
 * player_update_position. House location 2 has no FarmChoice shortcut boxes. */
#include "house_control.h"
#include "pad_buttons.h"
#include "psx_data.h"
#include <string.h>

static int32_t half(const uint8_t *p)
{
    const uint16_t v=hmbtn_read_le16(p);
    return v<32768 ? v : (int32_t)v-65536;
}
bool hmbtn_house_exit_at(const uint8_t *section, size_t size,
    const int32_t position[3], HmbtnHouseExit *exit)
{
    if (!section || size<16+50*44 || !position || !exit) return false;
    for(unsigned i=0;i<50;++i) {
        const uint8_t *p=section+16+i*44;
        const unsigned flags=hmbtn_read_le16(p), id=hmbtn_read_le16(p+2);
        /* farm_zone_find: dead, active interaction, marker and trigger zones
         * do not initiate the simple walk-through exit supported here. */
        if(id>=0xFFFE || (flags&0x7388u) || hmbtn_read_le16(p+10)!=99) continue;
        if(half(p+14)<position[0] && position[0]<half(p+20)
                && half(p+18)<position[2] && position[2]<half(p+24)) {
            exit->zone=i; exit->target=id;
            for(unsigned a=0;a<3;++a) exit->entry[a]=half(p+38+a*2);
            return true;
        }
    }
    return false;
}

void hmbtn_house_control_tick(HmbtnHouseControl *s, HmbtnIntroActor *p,
    const HmbtnModel *ground, uint32_t held)
{
    if (!s->enabled || s->exit_pending) return;
    const unsigned old_facing=p->facing, old_mode=s->mode;
    const uint32_t arrows=held&0xF000u;
    switch (arrows) {
    case HMBTN_PAD_DOWN: case HMBTN_PAD_DOWN|HMBTN_PAD_LEFT: p->facing=0; break;
    case HMBTN_PAD_LEFT: case HMBTN_PAD_UP|HMBTN_PAD_LEFT: p->facing=1; break;
    case HMBTN_PAD_UP: case HMBTN_PAD_UP|HMBTN_PAD_RIGHT: p->facing=2; break;
    case HMBTN_PAD_RIGHT: case HMBTN_PAD_DOWN|HMBTN_PAD_RIGHT: p->facing=3; break;
    default: break;
    }
    const bool run=(held&HMBTN_PAD_CIRCLE)!=0;
    /* Original chooses velocity from the previous mode, before changing it. */
    const int32_t speed=old_mode==4 ? 73 : old_mode==2 ? 41 : 0;
    s->mode=run ? 4u : arrows ? 2u : 0u;
    const unsigned animation=s->mode==4 ? p->facing+6 : s->mode==2 ? p->facing+1 : 0;
    if (old_facing!=p->facing || old_mode!=s->mode || p->sprite.animation!=animation) {
        p->sprite=(HmbtnAnimationPlayer){(uint16_t)animation,0,0}; p->animation_done=false;
    }
    if (!s->mode) p->sprite=(HmbtnAnimationPlayer){0,(uint16_t)p->facing,0};
    int32_t dx=0,dz=0;
    if (s->mode) {
        if (p->facing&1u) dx=p->facing==1 ? -speed : speed;
        else dz=p->facing==0 ? -speed : speed;
    }
    int32_t ahead[3]={p->position[0]+dx,p->position[1],p->position[2]+dz};
    if (p->facing&1u) ahead[0]+=p->facing==1 ? -48 : 48;
    else ahead[2]+=p->facing==0 ? -48 : 48;
    HmbtnGroundHit hit;
    if (s->mode && !hmbtn_ground_find(ground,ahead,0xE080,200,&hit)) {
        dx=dz=0;
        if (s->blocked_frames < 5) ++s->blocked_frames;
        else if (run) {
            static const int16_t sides[2][2][2]={{{-60,-10},{60,-10}},{{-10,60},{-10,-60}}};
            static const int16_t slides[2][2][2]={{{-10,4},{10,4}},{{4,10},{4,-10}}};
            unsigned open=0;
            for(unsigned i=0;i<2;++i) {
                int32_t side[3]={p->position[0]+sides[p->facing&1u][i][0],p->position[1],
                    p->position[2]+sides[p->facing&1u][i][1]};
                if(hmbtn_ground_find(ground,side,0xE080,200,&hit)) open|=1u<<i;
            }
            if(open) { const unsigned i=open==1 ? 0u : 1u;
                dx=slides[p->facing&1u][i][0]; dz=slides[p->facing&1u][i][1]; }
        }
    } else s->blocked_frames=0;
    int32_t next[3]={p->position[0]+dx,p->position[1],p->position[2]+dz};
    if(hmbtn_ground_find(ground,next,0xC080,200,&hit)) {
        next[1]=hit.height; memcpy(p->position,next,sizeof(next));
    }
    p->pose=false;
}

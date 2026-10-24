/* Adapted from player_dpad_facing/player_update_mode/player_slide_on_bump and
 * player_update_position. Farm queries add the field and FarmChoice shortcuts. */
#include "house_control.h"
#include "pad_buttons.h"
#include "psx_data.h"
#include "world_map.h"
#include <string.h>

static int32_t half(const uint8_t *p)
{
    const uint16_t v=hmbtn_read_le16(p);
    return v<32768 ? v : (int32_t)v-65536;
}
unsigned hmbtn_farm_music_volume(const uint8_t *section,size_t size)
{
    return section && size>=16 && (hmbtn_read_le16(section+8)&0x1000u) ? 80u : 100u;
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

bool hmbtn_farm_house_door_at(const uint8_t *section,size_t size,
    const HmbtnIntroActor *player,HmbtnHouseExit *exit,unsigned *model)
{
    return hmbtn_world_door_at(section,size,player,exit,model) && exit->target==80;
}
bool hmbtn_world_door_at(const uint8_t *section,size_t size,
    const HmbtnIntroActor *player,HmbtnHouseExit *exit,unsigned *model)
{
    if(!section || size<16+50*44 || !player || !exit || !model || player->facing>3) return false;
    for(unsigned i=0;i<50;++i) {
        const uint8_t *p=section+16+i*44;
        unsigned logical,resource;
        const unsigned flags=hmbtn_read_le16(p);
        if((flags!=0x9000 && flags!=0x1000) || !hmbtn_world_area(hmbtn_read_le16(p+2),0,&logical,&resource)
                || hmbtn_read_le16(p+8)!=player->facing
                || hmbtn_read_le16(p+12)>=15) continue;
        if(player->position[0]<=half(p+14) || player->position[0]>=half(p+20)
                || player->position[2]<=half(p+18) || player->position[2]>=half(p+24)) continue;
        if(player->position[1]<half(p+16)-200 || player->position[1]>half(p+22)+200) continue;
        exit->zone=i;exit->target=hmbtn_read_le16(p+2);*model=hmbtn_read_le16(p+12);
        for(unsigned a=0;a<3;++a) exit->entry[a]=half(p+38+a*2);
        return true;
    }
    return false;
}

void hmbtn_house_control_tick(HmbtnHouseControl *s, HmbtnIntroActor *p,
    const HmbtnModel *ground, uint32_t held)
{
    hmbtn_farm_control_tick(s,p,ground,NULL,held);
}
bool hmbtn_world_gate_at(const uint8_t *section,size_t size,const int32_t position[3],HmbtnHouseExit *exit)
{
    if(!section || size<16+50*44 || !position || !exit) return false;
    for(unsigned i=0;i<50;++i) {
        const uint8_t *p=section+16+i*44;
        const unsigned flags=hmbtn_read_le16(p);unsigned logical,resource;
        if(!(flags&0x200u) || (flags&0x6188u) || !hmbtn_world_area(hmbtn_read_le16(p+2),0,&logical,&resource)) continue;
        if(position[0]<=half(p+14) || position[0]>=half(p+20) || position[2]<=half(p+18) || position[2]>=half(p+24)) continue;
        exit->zone=i;exit->target=hmbtn_read_le16(p+2);
        for(unsigned a=0;a<3;++a) exit->entry[a]=half(p+38+a*2);
        return true;
    }
    return false;
}
static void jump_tick(HmbtnHouseControl *s,HmbtnIntroActor *p,const HmbtnModel *ground)
{
    static const int step[4][2]={{0,-51},{-51,0},{0,51},{51,0}};
    int32_t pos[3]={p->position[0]+step[p->facing][0],p->position[1]+s->jump_velocity,
        p->position[2]+step[p->facing][1]};
    if(hmbtn_farm_settle(NULL,ground,pos,0xc000,50)) {
        s->jump_velocity=0;s->jumping=false;s->landed=true;s->mode=0;
        p->sprite=(HmbtnAnimationPlayer){0,(uint16_t)p->facing,0};
    } else {
        s->jump_velocity+=16;
        if(s->jump_velocity>128) s->jump_velocity=128;
    }
    memcpy(p->position,pos,sizeof(pos));
}
void hmbtn_farm_control_tick(HmbtnHouseControl *s,HmbtnIntroActor *p,
    const HmbtnModel *ground,const HmbtnFarmField *field,uint32_t held)
{
    s->jump_started=s->landed=false;
    if (!s->enabled || s->exit_pending) return;
    if(s->jumping) { jump_tick(s,p,ground);return; }
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
    const unsigned animation=s->carrying_big ? (s->mode==4 ? p->facing+19 : s->mode==2 ? p->facing+42 : 41)
        : s->carrying ? (s->mode==4 ? p->facing+19 : s->mode==2 ? p->facing+15 : 27)
        : s->mode==4 ? p->facing+6 : s->mode==2 ? p->facing+1 : 0;
    if (old_facing!=p->facing || old_mode!=s->mode || p->sprite.animation!=animation) {
        p->sprite=(HmbtnAnimationPlayer){(uint16_t)animation,0,0}; p->animation_done=false;
    }
    if (!s->mode) p->sprite=(HmbtnAnimationPlayer){(uint16_t)(s->carrying_big ? 41 : s->carrying ? 27 : 0),(uint16_t)p->facing,0};
    int32_t dx=0,dz=0;
    if (s->mode) {
        if (p->facing&1u) dx=p->facing==1 ? -speed : speed;
        else dz=p->facing==0 ? -speed : speed;
    }
    int32_t ahead[3]={p->position[0]+dx,p->position[1],p->position[2]+dz};
    if (p->facing&1u) ahead[0]+=p->facing==1 ? -48 : 48;
    else ahead[2]+=p->facing==0 ? -48 : 48;
    const unsigned occupant=s->mode ? hmbtn_farm_field_probe(field,ahead) : 0;
    if(occupant && !s->carrying_big && run && speed && hmbtn_farm_field_can_hop(field,p->position,p->facing)) {
        s->jumping=s->jump_started=true;s->jump_velocity=-80;
        p->sprite=(HmbtnAnimationPlayer){10,(uint16_t)p->facing,0};p->animation_done=false;
        jump_tick(s,p,ground);return;
    }
    if (s->mode && (!hmbtn_farm_settle(field,ground,ahead,0xE080,200)
            || (s->mode!=4 && s->npc_blocked && s->npc_blocked(s->collision_context,p->position,ahead)))) {
        dx=dz=0;
        if (s->blocked_frames < 5) ++s->blocked_frames;
        else if (run) {
            static const int16_t sides[2][2][2]={{{-60,-10},{60,-10}},{{-10,60},{-10,-60}}};
            static const int16_t slides[2][2][2]={{{-10,4},{10,4}},{{4,10},{4,-10}}};
            unsigned open=0;
            for(unsigned i=0;i<2;++i) {
                int32_t side[3]={p->position[0]+sides[p->facing&1u][i][0],p->position[1],
                    p->position[2]+sides[p->facing&1u][i][1]};
                if(hmbtn_farm_settle(field,ground,side,0xE080,200)
                        && !hmbtn_farm_field_probe(field,side)
                        && (s->mode==4 || !s->npc_blocked || !s->npc_blocked(s->collision_context,p->position,side))) open|=1u<<i;
            }
            if(open) { const unsigned i=open==1 ? 0u : 1u;
                dx=slides[p->facing&1u][i][0]; dz=slides[p->facing&1u][i][1]; }
        }
    } else { s->blocked_frames=0;if(occupant) dx=dz=0; }
    int32_t next[3]={p->position[0]+dx,p->position[1],p->position[2]+dz};
    if(hmbtn_farm_settle(field,ground,next,0xC080,200)
            && (s->mode==4 || !s->npc_blocked || !s->npc_blocked(s->collision_context,p->position,next))) {
        memcpy(p->position,next,sizeof(next));
    }
    p->pose=false;
}

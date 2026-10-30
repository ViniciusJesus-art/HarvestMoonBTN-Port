/* Tools, using player_command_validate/player_anim_position_step and
 * player_run_action. Extra action banks are requested from the host loader. */
#include "farm_action.h"
#include "mine.h"
#include "tool_upgrade.h"
#include <string.h>
void hmbtn_farm_action_init(HmbtnFarmAction *a)
{
    memset(a,0,sizeof(*a));a->stamina=a->max_stamina=100;a->sound=a->bank=-1;a->target=HMBTN_FIELD_CELLS;
}
static void idle(HmbtnFarmAction *a,HmbtnIntroActor *p)
{
    a->finished_bank=a->bank>=0;a->bank=-1;a->phase=HMBTN_FARM_ACTION_FREE;
    p->sprite=(HmbtnAnimationPlayer){0,(uint16_t)p->facing,0};p->animation_done=false;
}
void hmbtn_farm_action_cost(HmbtnFarmAction *a,const HmbtnHouseInteraction *h,unsigned amount)
{
    unsigned fatigue=(h->weather->today!=0)+(h->clock->hour>=22 ? 1u : h->clock->hour<6 ? 2u : 0u)+(a->stamina==0 ? 2u : 0u);
    a->stamina=a->stamina>amount ? a->stamina-amount : 0;
    a->fatigue=a->fatigue+fatigue>100 ? 100 : a->fatigue+fatigue;
}
static bool apply_cell(HmbtnFarmAction *a,HmbtnIntroActor *p,HmbtnHouseInteraction *h,HmbtnFarmField *field,unsigned part)
{
    int dx,dr;
    const unsigned charge=a->charge_frames/20;
    if(!field || !hmbtn_tool_grid(a->kind,charge,part,p->facing,&dx,&dr)) return false;
    const int c=a->origin_column+dx,r=a->origin_row+dr;
    const int cols=(int)hmbtn_farm_field_columns(field),rows=(int)hmbtn_farm_field_rows(field);
    if(c<0 || c>=cols || r<0 || r>=rows) return false;
    if(a->kind==1 && !field->indoor && field->season==3) {
        a->result_step=a->result_count;return false; /* field_request_dispatch. */
    }
    const unsigned target=(unsigned)(r*cols+c);
    if(target!=a->target) { a->target=target;a->strikes=0; }
    /* Hammer/axe accumulate 1<<charge per swing, with owned rank as the
     * material's minimum power guard, as player_tool_clear[_alt]. */
    if(a->kind==2 || a->kind==3) a->strikes+=1u<<charge;
    const uint16_t item=field->cells[target].value;
    const bool changed=hmbtn_farm_field_tool(field,c,r,a->kind,h->inventory->equipped.modifier,a->strikes,h->inventory->supplies);
    if(a->kind==1 && !changed && field->active[target]) a->result_step=a->result_count;
    if(changed && !field->indoor && a->kind==1 && h->weather->today==HMBTN_WEATHER_RAIN)
        hmbtn_farm_field_water(field,target);
    if(changed && (a->kind==0 || a->kind==2 || a->kind==3)) {
        a->broken_item=item;a->effect_column=c;a->effect_row=r;
        if(item>=0x3b && item<=0x3d) { ++a->effect_column;++a->effect_row; }
        else if(item>=0x16c && item<=0x174) {
            const unsigned corner=(item-0x16c)%3+1;
            if(corner==1) ++a->effect_row;
            if(corner==3) ++a->effect_column;
        }
    }
    return changed;
}
static void finish_tool(HmbtnFarmAction *a,HmbtnIntroActor *p,HmbtnHouseInteraction *h,bool changed)
{
    const unsigned exp=changed ? 150u : 40u;
    a->experience[a->kind]=a->experience[a->kind]+exp>65535 ? 65535 : a->experience[a->kind]+exp;
    a->release_sound=a->kind==4;
    hmbtn_farm_action_cost(a,h,hmbtn_tool_stamina(a->kind,a->charge_frames/20));idle(a,p);
}
bool hmbtn_farm_action_mine_tick(HmbtnFarmAction *a,HmbtnIntroActor *p,HmbtnHouseInteraction *h,
    HmbtnFarmField *field,const HmbtnModel *ground,uint32_t pressed,uint32_t held,HmbtnMine *mine,bool *flags)
{
    a->sound=-1;a->finished_bank=a->changed=false;
    a->release_sound=false;a->broken_item=0;
    HmbtnStoredTool *tool=&h->inventory->equipped;
    /* player.c: player_start_command_item / PLAYER_MODE_PLANT_SEED. Bank
     * 0x53-0x3b=24; completion applies picture-grid targets once, then consumes
     * one packet even if no tilled cell accepts it, as stored_item_release. */
    if(a->phase==HMBTN_FARM_PLANT_SEED) {
        if(!p->animation_done) return true;
        int c,r;
        if(field && hmbtn_farm_field_world_cell(field,p->position,&c,&r)) for(unsigned part=0;part<9;++part) {
            int dx,dz;
            if(hmbtn_farm_seed_cell(tool->id,part,p->facing,&dx,&dz))
                a->changed|=hmbtn_farm_field_plant(field,c+dx,r+dz,tool->id);
        }
        if(tool->count>1) --tool->count;else *tool=(HmbtnStoredTool){0};
        a->sound=14;hmbtn_farm_action_cost(a,h,1);idle(a,p);return true;
    }
    if(a->phase==HMBTN_FARM_ACTION_FREE && (pressed&HMBTN_PAD_SQUARE)
            && hmbtn_farm_crop_supported(tool->id)) {
        const HmbtnFarmItem *seed=hmbtn_farm_item(tool->id);
        if(!field || !tool->count || !seed || a->fatigue>=100 || p->facing>3) return false;
        a->phase=HMBTN_FARM_PLANT_SEED;a->bank=24;a->bank_pending=true;
        p->sprite=(HmbtnAnimationPlayer){(uint16_t)seed->plant_animation,0,0};
        p->animation_done=false;return true;
    }
    if(a->phase==HMBTN_FARM_TOOL_CHARGE) {
        if(held&HMBTN_PAD_SQUARE) {
            if(a->charge_frames<tool->modifier*20u) ++a->charge_frames;
            else if(++a->overcharge_frames>=61) {
                a->phase=HMBTN_FARM_OVERCHARGE;a->bank=3;a->bank_pending=true;
                p->sprite=(HmbtnAnimationPlayer){0};p->animation_done=false;return true;
            }
            p->sprite.frame=(uint16_t)(a->charge_frames/20);p->sprite.ticks=0;
        } else {
            a->phase=HMBTN_FARM_TOOL_SWING;
            const unsigned charge=a->charge_frames/20;
            p->sprite=(HmbtnAnimationPlayer){(uint16_t)(72+a->kind*20+p->facing*5+charge),(uint16_t)(charge+1),0};
            p->animation_done=false;a->sound=(int)(29+a->kind*5+a->charge_frames/20);
        }
        return true;
    }
    if(a->phase==HMBTN_FARM_REFILL || a->phase==HMBTN_FARM_OVERCHARGE) {
        if(p->animation_done) {
            if(a->phase==HMBTN_FARM_REFILL) { tool->count=100;a->sound=21;hmbtn_farm_action_cost(a,h,1); }
            idle(a,p);
        }
        return true;
    }
    if(a->phase==HMBTN_FARM_TOOL_RESULT) {
        if(a->result_step<a->result_count) {
            const unsigned part=a->result_step++;
            a->changed=apply_cell(a,p,h,field,part);a->result_changed|=a->changed;
        } else finish_tool(a,p,h,a->result_changed);
        return true;
    }
    if(a->phase==HMBTN_FARM_TOOL_SWING) {
        if(!p->animation_done) return true;
        if(mine && mine->kind && a->kind==1) {
            mine->found=hmbtn_mine_dig(mine,p,flags,&a->changed);
            finish_tool(a,p,h,mine->found!=0);return true; /* Mines always one cell. */
        }
        a->origin_column=a->origin_row=-100;
        hmbtn_farm_field_world_cell(field,p->position,&a->origin_column,&a->origin_row);
        a->result_step=0;a->result_count=field ? hmbtn_tool_targets(a->kind,a->charge_frames/20) : 0;a->result_changed=false;
        if(a->kind==4 && field) {
            /* Deduct the requested count, even on dry/ineligible targets;
             * insufficient stock bounds the inclusive source result loop. */
            if(h->inventory->equipped.count<a->result_count) a->result_count=h->inventory->equipped.count;
            const unsigned requested=hmbtn_tool_targets(4,a->charge_frames/20);
            h->inventory->equipped.count=h->inventory->equipped.count>requested ? h->inventory->equipped.count-requested : 0;
        }
        if(a->result_count>1) { a->phase=HMBTN_FARM_TOOL_RESULT;return true; }
        a->changed=a->result_count && apply_cell(a,p,h,field,0);
        finish_tool(a,p,h,a->changed);return true;
    }
    if(!(pressed&HMBTN_PAD_SQUARE) || !tool->id || tool->id>25 || a->fatigue>=100) return false;
    if(tool->modifier>4 || tool->modifier!=(tool->id-1)%5) { a->sound=2;return true; }
    if(a->kind!=(unsigned)(tool->id-1)/5) { a->target=HMBTN_FIELD_CELLS;a->strikes=0; }
    a->kind=(tool->id-1)/5;a->overcharge_frames=0;a->charge_frames=1;
    if(a->kind==4) {
        static const int reach[4][2]={{0,-224},{-224,0},{0,224},{224,0}};
        const int32_t at[3]={p->position[0]+reach[p->facing][0],p->position[1],p->position[2]+reach[p->facing][1]};
        HmbtnGroundHit hit;
        if(hmbtn_ground_find(ground,at,0,200,&hit) && (hit.flags&3)) {
            a->phase=HMBTN_FARM_REFILL;a->bank=23;a->bank_pending=true;
            p->sprite=(HmbtnAnimationPlayer){(uint16_t)(p->facing*5+tool->modifier),0,0};
            p->animation_done=false;return true;
        }
        if(!tool->count) return false;
    }
    a->phase=HMBTN_FARM_TOOL_CHARGE;
    p->sprite=(HmbtnAnimationPlayer){(uint16_t)(72+a->kind*20+p->facing*5+(tool->id-1)%5),0,0};
    p->animation_done=false;return true;
}

bool hmbtn_farm_action_tick(HmbtnFarmAction *a,HmbtnIntroActor *p,HmbtnHouseInteraction *h,
    HmbtnFarmField *field,const HmbtnModel *ground,uint32_t pressed,uint32_t held)
{ return hmbtn_farm_action_mine_tick(a,p,h,field,ground,pressed,held,NULL,NULL); }

/* Initial tools, using player_command_validate/player_anim_position_step and
 * player_run_action. Extra action banks are requested from the host loader. */
#include "farm_action.h"
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
static void cost(HmbtnFarmAction *a,const HmbtnHouseInteraction *h,unsigned amount)
{
    unsigned fatigue=(h->weather!=0)+(h->clock->hour>=22 ? 1u : h->clock->hour<6 ? 2u : 0u)+(a->stamina==0 ? 2u : 0u);
    a->stamina=a->stamina>amount ? a->stamina-amount : 0;
    a->fatigue=a->fatigue+fatigue>100 ? 100 : a->fatigue+fatigue;
}
bool hmbtn_farm_action_tick(HmbtnFarmAction *a,HmbtnIntroActor *p,HmbtnHouseInteraction *h,
    HmbtnFarmField *field,const HmbtnModel *ground,uint32_t pressed,uint32_t held)
{
    a->sound=-1;a->finished_bank=a->changed=false;
    a->release_sound=false;a->broken_item=0;
    HmbtnStoredTool *tool=&h->inventory->equipped;
    if(a->phase==HMBTN_FARM_TOOL_CHARGE) {
        if(held&HMBTN_PAD_SQUARE) {
            if(a->charge_frames<tool->modifier*20) ++a->charge_frames;
            else if(++a->overcharge_frames>=61) {
                a->phase=HMBTN_FARM_OVERCHARGE;a->bank=3;a->bank_pending=true;
                p->sprite=(HmbtnAnimationPlayer){0};p->animation_done=false;return true;
            }
            p->sprite.frame=(uint16_t)(a->charge_frames/20);p->sprite.ticks=0;
        } else {
            a->phase=HMBTN_FARM_TOOL_SWING;
            p->sprite.frame=(uint16_t)(a->charge_frames/20+1);p->sprite.ticks=0;
            p->animation_done=false;a->sound=(int)(29+a->kind*5+a->charge_frames/20);
        }
        return true;
    }
    if(a->phase==HMBTN_FARM_REFILL || a->phase==HMBTN_FARM_OVERCHARGE) {
        if(p->animation_done) {
            if(a->phase==HMBTN_FARM_REFILL) { tool->count=100;a->sound=21;cost(a,h,1); }
            idle(a,p);
        }
        return true;
    }
    if(a->phase==HMBTN_FARM_TOOL_SWING) {
        if(!p->animation_done) return true;
        int c=-100,r=-100; bool changed=false;
        hmbtn_farm_world_cell(p->position,&c,&r);
        if(c>=-1 && c<=45 && r>=-1 && r<=25) {
            static const int step[4][2]={{0,1},{-1,0},{0,-1},{1,0}};
            c+=step[p->facing][0];r+=step[p->facing][1];
            const unsigned target=c>=0 && c<45 && r>=0 && r<25 ? (unsigned)(r*45+c) : HMBTN_FIELD_CELLS;
            if(target!=a->target) { a->target=target;a->strikes=0; }
            ++a->strikes;
            const uint16_t item=target<HMBTN_FIELD_CELLS ? field->cells[target].value : 0;
            /* Initial tools affect one square. Upgraded area grids are a
             * separate step and are not accepted by the input path below. */
            changed=hmbtn_farm_field_tool(field,c,r,a->kind,tool->modifier,a->strikes,h->inventory->supplies);
            if(changed && (a->kind==0 || a->kind==2 || a->kind==3)) {
                a->broken_item=item;a->effect_column=c;a->effect_row=r;
                if(item>=0x3b && item<=0x3d) { ++a->effect_column;++a->effect_row; }
                else if(item>=0x16c && item<=0x174) {
                    const unsigned corner=(item-0x16c)%3+1;
                    if(corner==1) ++a->effect_row;
                    if(corner==3) ++a->effect_column;
                }
            }
            if(a->kind==4 && tool->count) --tool->count;
        }
        const unsigned exp=changed ? 150u : 40u;
        a->experience[a->kind]=a->experience[a->kind]+exp>65535 ? 65535 : a->experience[a->kind]+exp;
        a->release_sound=a->kind==4;
        a->changed=changed;cost(a,h,1+a->charge_frames/20);idle(a,p);return true;
    }
    if(!(pressed&HMBTN_PAD_SQUARE) || !tool->id || tool->id>25 || a->fatigue>=100) return false;
    if(tool->modifier || (tool->id-1)%5) { a->sound=2;return true; }
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

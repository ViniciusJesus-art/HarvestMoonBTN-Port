/* event_commands.c: comparisons deliberately return 1 when the rule FAILS. */
#include "village_talk.h"
#include "item_reaction.h"
#include "event_item.h"
#include "inventory.h"
#include "feed_shop.h"
#include "event_variables.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "village_talk_tables.inc"
static bool slot(int i) { return i>=0 && i<32; }
static bool person(int i) { return i>0 && i<HMBTN_VILLAGERS; }
static void birthday(const HmbtnVillageTalk *t,unsigned id,int *season,int *day)
{
    *season=*day=0;
    for(unsigned i=0;i<28;++i) if((unsigned)birthdays[i][0]==id) {
        *season=birthdays[i][1];*day=birthdays[i][2];return;
    }
    unsigned s,d;
    if(hmbtn_bride_birthday(t->registration,id,&s,&d)) { *season=(int)s;*day=(int)d; }
}
static HmbtnEventResult command(void *context,const HmbtnEventCommand *c,char *error,size_t size)
{
    HmbtnVillageTalk *t=context;int season,day;
    const int v=c->command,e=c->operand,x=c->x,y=c->y;
    bool done=true;
    switch(c->type) {
    case 2:
        if(!t->pending) {
            if(v<0 || v>4 || (y && !slot(e))) goto invalid;
            t->bank=(unsigned)v;t->request=(unsigned)(y ? t->slots[e] : e);
            t->message_pc=t->script.pc;t->pending=true;t->loaded=t->message_done=false;
        }
        if(!t->message_done) return HMBTN_EVENT_PENDING;
        if(x!=-1) { if(!slot(x)) goto invalid;t->slots[x]=(int32_t)t->result; }
        t->pending=false;break;
    case 12: {
        if(v!=(int)t->target || e || x || y) goto boundary;
        HmbtnIntroActor *npc=&t->village->people[v].actor;
        int dx=t->player->position[0]-npc->position[0],dz=t->player->position[2]-npc->position[2];
        npc->facing=abs(dx)>abs(dz) ? (dx<0 ? 1u : 3u) : (dz<0 ? 0u : 2u);
        npc->sprite=(HmbtnAnimationPlayer){0,(uint16_t)npc->facing,0};break;
    }
    case 13:case 14:case 15:case 16:case 17:case 18:case 19:case 20:case 21:case 22: {
        HmbtnEventVariables variables={t->village,t->clock,t->registration,t->player,t->target,
            t->slots,sizeof(t->slots)/sizeof(*t->slots)};
        const HmbtnEventResult result=hmbtn_event_variables_command(&variables,c,error,size);
        if(result==HMBTN_EVENT_BOUNDARY) goto boundary;
        return result;
    }
    case 29:case 30: {
        if(!t->inventory) goto invalid;
        int item=c->type==29 ? 0 : v,where=c->type==29 ? v : e,which=c->type==29 ? e : x;
        if(c->type==30 && (y&2)) {
            if(!slot(item)) goto invalid;
            item=(int)((uint32_t)t->slots[item]&0xFFFFu);
            if(item>=32768) item-=65536;
        }
        const int found=hmbtn_event_items_take(t->inventory,item,where,which,c->type==30 && (y&1) ? 1u : 0u);
        if(found<0) goto boundary;
        done=found!=0;
        break;
    }
    case 32: { /* event_item_store: equipped, rucksack, then optional toolbox. */
        if(!hmbtn_inventory_capacity(t->inventory) || (e && !slot(v))) goto invalid;
        int item=e ? (int)((uint32_t)t->slots[v]&0xFFFFu) : v;
        if(item>=32768) item-=65536; /* script_vars[item].half */
        if(item<=0 || item>=380) goto boundary;
        HmbtnInventoryPlacement placement;
        const HmbtnStoredTool record={(uint16_t)item,(uint16_t)(item<=25 ? (item-1)%5 : 0),1};
        done=hmbtn_inventory_store_tool(t->inventory,record,!x,&placement);
        if(done && y && placement.store!=HMBTN_STORE_TOOLBOX) {
            t->show_tool=(unsigned)item;t->show_frames=y==1 ? 11 : 61;
        }
        break;
    }
    case 35: /* Bind the event actor; the world route is retained for handback. */
        if(v!=(int)t->target) goto boundary;
        break;
    case 37:
        if(v || e || x || y) goto boundary;
        t->script.finished=true;break;
    case 57: {
        if(!t->inventory->held_item.id) { done=false;break; }
        if(!slot(e) || (x && !slot(v))) goto invalid;
        const int id=x ? t->slots[v] : v;
        if(!person(id)) goto invalid;
        const HmbtnStoredTool *gift=&t->inventory->held_item;
        const unsigned item=gift->id==164 && gift->modifier ? gift->modifier : gift->id;
        const int reaction=hmbtn_item_villager_reaction(item,(unsigned)id);
        t->slots[e]=reaction>=0 && reaction<7 ? reaction+1 : 0;
        break;
    }
    case 58: {
        if(e!=0 && e!=1) goto invalid;
        const HmbtnStoredTool *entry=e ? &t->inventory->held_item : &t->inventory->equipped;
        const unsigned item=x && entry->id==164 ? entry->modifier : entry->id;
        done=item!=0;
        if(done && v!=-1) { if(!slot(v)) goto invalid;t->slots[v]=(int32_t)item; }
        break;
    }
    case 66:
        if(x<0 || x>=HMBTN_VILLAGE_FLAGS) goto invalid;
        if(v==0) { if(!hmbtn_village_flag_set(t->village,(unsigned)x,e!=0)) goto boundary; }
        else done=hmbtn_village_flag(t->village,(unsigned)x);
        break;
    case 71:
        if(v!=(int)t->target || e) goto boundary;
        t->released=true;break;
    case 79:if(!slot(v)) goto invalid;t->slots[v]=(int32_t)(c->next_other==255 ? t->script.pc+1 : c->next_other);break;
    case 80:
        if(!slot(v) || t->slots[v]<0 || (unsigned)t->slots[v]>=t->script.count) goto invalid;
        t->script.pc=(unsigned)t->slots[v];return HMBTN_EVENT_JUMP;
    case 81:
        if(v==27) return hmbtn_event_progress_command(t->house->progress,c,error,size);
        /* event_cmd_dispatch: start Won/Gotz shops, then wait for their close.
         * The imported conversation decides when this command is reached. */
        if(v==8 && (t->target==35 || t->target==24) && t->shop) {
            if(t->target==24 && e==9 && x==1 && !y) {
                done=t->shop->construction && t->shop->construction->mornings_left;break;
            }
            if(x) goto boundary;
            if(!y && ((t->target==35 && e==10) || (t->target==24 && e==9))) {
                if(!(t->target==35 ? hmbtn_won_seed_shop_open(t->shop,t->clock)
                    : hmbtn_carpenter_shop_open(t->shop,t->clock))) goto boundary;
                break;
            }
            if(y==1 && !e) {
                if(t->shop->phase!=HMBTN_SHOP_OFF) return HMBTN_EVENT_WAIT;
                /* Purchases are committed, not rollback candidates if a later
                 * unsupported story command occurs in this conversation. */
                t->saved_inventory=*t->inventory;break;
            }
        }
        if(v==22) {
            if(t->house->event_count1<15) ++t->house->event_count1;
            break;
        }
        /* Only queries of initial-state systems; shops/story mutations are boundaries. */
        if(v==2 && e==1) { done=false;break; } /* No weddings in this session. */
        if(v==18 && x==0) { done=false;break; } /* No livestock spawn gate armed. */
        if(v==20 && e==0) { done=false;break; } /* No construction order. */
        if(v==29 && slot(e)) { t->slots[e]=0;done=hmbtn_event_comparison(0,x,y);break; }
        goto boundary;
    case 94:
        if(e || !person(v)) goto boundary;
        birthday(t,(unsigned)v,&season,&day);
        if(x!=-1) { if(!slot(x)) goto invalid;t->slots[x]=season; }
        if(y!=-1) { if(!slot(y)) goto invalid;t->slots[y]=day; }
        done=t->clock->season==(unsigned)season && t->clock->day==(unsigned)day;break;
    case 95: {
        /* Original house,barn,coop,greenhouse slots, all derived from owners. */
        const int args[4]={v,e,x,y};
        for(unsigned i=0;i<4;++i) if(args[i]!=-1) {
            if(!slot(args[i])) goto invalid;
            if(i==2 && !t->village->poultry) goto boundary;
            t->slots[args[i]]=i==0 && t->village->construction ? (int32_t)t->village->construction->house_level
                : i==1 && t->village->construction ? (int32_t)t->village->construction->barn_level
                : i==2 && t->village->poultry->upgraded ? 1
                : i==3 && t->village->construction && t->village->construction->greenhouse ? 1 : 0;
        }
        break;
    }
    default:goto boundary;
    }
    return done ? HMBTN_EVENT_DONE : HMBTN_EVENT_WAIT;
boundary:
    t->boundary_type=c->type;return HMBTN_EVENT_BOUNDARY;
invalid:
    snprintf(error,size,"Invalid conversation operand at NPC %u step %u",t->target,t->script.pc);
    return HMBTN_EVENT_ERROR;
}
bool hmbtn_village_talk_range(unsigned id,uint32_t *offset,size_t *size)
{
    if(!id || id>=HMBTN_VILLAGERS || !offset || !size) return false;
    *offset=talk_ranges[id][0];*size=talk_ranges[id][1]-*offset;return *size>0 && *size<=8192;
}
bool hmbtn_village_talk_open(HmbtnVillageTalk *t,unsigned id,const uint8_t *data,size_t size,
    HmbtnVillage *v,const HmbtnIntroActor *player,const HmbtnFarmClock *clock,
    const HmbtnRegistration *registration,HmbtnHouseInteraction *house,char *error,size_t error_size)
{
    HmbtnInventory *inventory=house ? house->inventory : NULL;
    HmbtnEventBlock block;const uint8_t *code;size_t length;
    if(!t || !person((int)id) || !v || !player || !clock || !registration || !inventory || inventory->held_item.id>=380
            || !hmbtn_event_block_open(&block,data,size,error,error_size)
            || !hmbtn_event_block_entry(&block,block.script,&code,&length,NULL)) return false;
    if(length>sizeof(t->code)) { snprintf(error,error_size,"NPC conversation exceeds native script bound");return false; }
    memset(t,0,sizeof(*t));memcpy(t->code,code,length);
    if(!hmbtn_event_script_open(&t->script,t->code,length,error,error_size)) return false;
    memcpy(t->saved_flags,v->flags,sizeof(t->saved_flags));t->saved_days=v->days;
    t->saved_inventory=*inventory;
    t->saved_event_count1=house->event_count1;
    if(house->progress) t->saved_progress=*house->progress;
    for(unsigned i=0;i<HMBTN_VILLAGERS;++i) {
        t->saved_people[i].flags=v->people[i].talk_flags;t->saved_people[i].affection=v->people[i].affection;
        t->saved_people[i].friendship=v->people[i].friendship;
    }
    t->target=id;t->saved_facing=v->people[id].actor.facing;t->village=v;t->player=player;
    t->clock=clock;t->registration=registration;t->inventory=inventory;t->house=house;t->active=true;return true;
}
bool hmbtn_village_talk_tick(HmbtnVillageTalk *t,char *error,size_t size)
{
    if(!t || !t->active) return true;
    if(!hmbtn_event_tick(&t->script,command,t,error,size)) return false;
    if(t->script.finished || t->script.boundary) {
        if(t->script.boundary) {
            /* An unported action must not claim story/item progress. */
            memcpy(t->village->flags,t->saved_flags,sizeof(t->saved_flags));t->village->days=t->saved_days;
            for(unsigned i=0;i<HMBTN_VILLAGERS;++i) {
                t->village->people[i].talk_flags=t->saved_people[i].flags;
                t->village->people[i].affection=t->saved_people[i].affection;
                t->village->people[i].friendship=t->saved_people[i].friendship;
            }
            /* Preserve the existing rollback scope: item stores only. Capacity
             * and supplies are not mutated by these conversation commands. */
            HmbtnInventory *live=t->inventory;
            live->equipped=t->saved_inventory.equipped;
            memcpy(live->bag,t->saved_inventory.bag,sizeof(live->bag));
            memcpy(live->toolbox,t->saved_inventory.toolbox,sizeof(live->toolbox));
            live->held_item=t->saved_inventory.held_item;
            memcpy(live->items,t->saved_inventory.items,sizeof(live->items));
            t->house->event_count1=t->saved_event_count1;
            if(t->house->progress) *t->house->progress=t->saved_progress;
            t->show_tool=t->show_frames=0;
        }
        HmbtnIntroActor *a=&t->village->people[t->target].actor;
        a->facing=t->saved_facing;a->sprite=(HmbtnAnimationPlayer){0,(uint16_t)a->facing,0};
        t->active=false;
    }
    return true;
}

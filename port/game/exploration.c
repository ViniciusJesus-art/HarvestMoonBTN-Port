#include "exploration.h"
#include "economy.h"
#include "psx_data.h"
#include "pad_buttons.h"
#include "world_map.h"
#include <stdio.h>
#include <string.h>

static bool valid(const HmbtnSession *s,const HmbtnExplorationFrame *f,char *error,size_t size)
{
    if(s && f && f->section && f->section_size>=0x2000 && f->ground) return true;
    snprintf(error,size,"Exploration requires session and bounded decoded world data");return false;
}
static bool emit(const HmbtnExplorationFrame *f,HmbtnExplorationEffect effect,char *error,size_t size)
{
    return !f->effect || f->effect(f->context,&effect,error,size);
}
static bool key(const HmbtnExplorationFrame *f,unsigned bank,unsigned program,unsigned tone,
    unsigned note,unsigned volume,bool loop,char *error,size_t size)
{
    return emit(f,(HmbtnExplorationEffect){.kind=HMBTN_EXP_KEY_SOUND,.bank=bank,.program=program,
        .tone=tone,.note=note,.volume=volume,.water_loop=loop},error,size);
}
static bool action_bank(const HmbtnExplorationFrame *f,unsigned row,char *error,size_t size)
{
    if(f->action_bank) return f->action_bank(f->context,row,error,size);
    snprintf(error,size,"Action bank %u unavailable to exploration",row);return false;
}
static bool text(const HmbtnExplorationFrame *f,unsigned bank,unsigned record,uint8_t *data,char *error,size_t size)
{
    if(f->text) return f->text(f->context,bank,record,data,8192,error,size);
    snprintf(error,size,"Text %u/%u unavailable to exploration",bank,record);return false;
}
static bool message(const HmbtnExplorationFrame *f,const uint8_t *record,HmbtnMessage *out,char *error,size_t size)
{
    return hmbtn_message_open(out,record,8192,error,size)
        && emit(f,(HmbtnExplorationEffect){.kind=HMBTN_EXP_MESSAGE_ART,.record=record,.bytes=8192},error,size);
}
static bool boundary(HmbtnSession *s,const HmbtnExplorationFrame *f,HmbtnExplorationBoundary kind,
    unsigned value,HmbtnLogicalInput input,HmbtnExplorationBoundaryResult *result,char *error,size_t size)
{
    HmbtnExplorationBoundaryResult ignored={0};
    if(f->boundary) return f->boundary(f->boundary_context ? f->boundary_context : f->context,s,kind,value,input,result ? result : &ignored,error,size);
    snprintf(error,size,"Exploration reached MA-07 boundary %u",kind);return false;
}
static bool animate(const HmbtnExplorationFrame *f,HmbtnExplorationAnimation kind,unsigned id,
    HmbtnAnimationPlayer *sprite,char *error,size_t size)
{
    const HmbtnAnimation *ani=f->animation ? f->animation(f->context,kind,id) : NULL;
    if(!ani || !ani->data) { snprintf(error,size,"Decoded animation %u/%u unavailable",kind,id);return false; }
    return hmbtn_animation_tick(ani,sprite,error,size);
}
bool hmbtn_actor_animation_tick(const HmbtnAnimation *a, HmbtnIntroActor *actor,
    char *error, size_t error_size)
{
    /* Preserve the original ENDED/JUMPED signal for event_cmd_set_sprite waits. */
    if(!a || !a->data || !actor) { snprintf(error,error_size,"Missing decoded actor animation");return false; }
    const HmbtnAnimationPlayer *p = &actor->sprite;
    if (p->animation >= a->count[3]) { snprintf(error,error_size,"Opening animation outside range"); return false; }
    const uint8_t *range = a->data+a->section[3]+p->animation*4;
    const unsigned count = hmbtn_read_le16(range);
    if (p->frame >= count) { snprintf(error,error_size,"Opening animation frame outside range"); return false; }
    const uint8_t *frame = a->data+a->section[4]+hmbtn_read_le16(range+2)+p->frame*10;
    actor->animation_done = p->ticks+1u >= ((unsigned)frame[6]+1)/2
        && (frame[7] != 0 || p->frame+1u >= count);
    return hmbtn_animation_tick(a,&actor->sprite,error,error_size);
}

static bool feed_shop_message(HmbtnSession *s,const HmbtnExplorationFrame *f,char *error,size_t size)
{
    HmbtnFeedShop *shop=&s->feed_shop;
    if(shop->phase==HMBTN_SHOP_RUCKSACK_CUE || shop->phase==HMBTN_SHOP_RUCKSACK_UPGRADE) {
        s->dialogue.message=(HmbtnMessage){0};
        if(!shop->cue_started) {
            const bool ok=shop->phase==HMBTN_SHOP_RUCKSACK_CUE
                ? hmbtn_event_item_show_record(&s->event_item,&s->intro.player,s->house.inventory,
                    (HmbtnStoredTool){(uint16_t)shop->item,(uint16_t)(shop->wrapping ? shop->gift : 0),1})
                : hmbtn_event_item_rucksack(&s->event_item,&s->intro.player,s->house.inventory);
            if(!ok) { snprintf(error,size,"Cannot present original shop item");return false; }
            shop->cue_started=true;
        }
        s->control.mode=0;return true;
    }
    if(shop->phase==HMBTN_SHOP_SERVE) {
        s->dialogue.message=(HmbtnMessage){0};
        if(!shop->cue_started) {
            if(!hmbtn_event_item_receive(&s->event_item,&s->intro.player,s->house.inventory,&s->game.clock,shop->item)) {
                snprintf(error,size,"Cannot receive original shop meal");return false;
            }
            if(hmbtn_event_item_drinking(&s->event_item)) {
                if(!action_bank(f,17,error,size)) return false;
                s->intro.action_active=true;
            }
            shop->cue_started=true;
        }
        s->control.mode=0;return true;
    }
    if(shop->phase==HMBTN_SHOP_MAKER_CUE) {
        s->dialogue.message=(HmbtnMessage){0};
        if(!shop->cue_started) {
            if(!hmbtn_event_item_show(&s->event_item,&s->intro.player,s->house.inventory,hmbtn_saibara_material(shop->item))) {
                snprintf(error,size,"Cannot present Saibara order material");return false;
            }
            shop->cue_started=true;
        }
        s->control.mode=0;return true;
    }
    if(shop->clinic && (shop->phase==HMBTN_SHOP_CLINIC_FADE_OUT || shop->phase==HMBTN_SHOP_CLINIC_FADE_IN)) {
        s->dialogue.message=(HmbtnMessage){0};s->control.mode=0;return true;
    }
    if(shop->phase==HMBTN_SHOP_NAME) {
        if(shop->editor.request && shop->editor.loaded!=shop->editor.request) {
            uint8_t record[8192];
            if(!text(f,3,shop->editor.request,record,error,size) || !message(f,record,&shop->editor.message,error,size)) return false;
            shop->editor.loaded=shop->editor.request;
        }
        if(shop->sound>=0) (void)emit(f,(HmbtnExplorationEffect){.kind=HMBTN_EXP_UI_SOUND,.id=(unsigned)shop->sound},error,size);
        s->dialogue.message=(HmbtnMessage){0};return true;
    }
    if(shop->phase!=HMBTN_SHOP_OFF && shop->request!=shop->loaded) {
        uint8_t record[8192];
        if(!text(f,shop->bank ? shop->bank : 2,shop->request,record,error,size)) return false;
        if(!hmbtn_feed_shop_expand(record,sizeof(record),hmbtn_game_registration_const(&s->game))) {
            snprintf(error,size,"Cannot expand feed shop names");return false;
        }
        if(!message(f,record,&shop->message,error,size)) return false;
        shop->message.suppress_cursor=shop->phase==HMBTN_SHOP_SELECT || shop->phase==HMBTN_SHOP_COUNT
            || shop->phase==HMBTN_SHOP_TOOL_LIST || shop->phase==HMBTN_SHOP_SELL_LIST || shop->phase==HMBTN_SHOP_SELL_CONFIRM
            || shop->phase==HMBTN_SHOP_MAKER_CONFIRM || shop->phase==HMBTN_SHOP_MAKER_AGAIN;
        shop->loaded=shop->request;
    }
    if(shop->sound>=0) (void)emit(f,(HmbtnExplorationEffect){.kind=HMBTN_EXP_UI_SOUND,.id=(unsigned)shop->sound},error,size);
    s->dialogue.message=shop->message;
    s->intro.player.sprite=(HmbtnAnimationPlayer){0,(uint16_t)s->intro.player.facing,0};
    s->control.mode=0;return true;
}
static bool world_blocked(const void *context,const int32_t from[3],const int32_t to[3])
{
    const HmbtnSession *s=context;
    return hmbtn_village_blocked(&s->village,from,to) || hmbtn_livestock_blocked(&s->livestock,s->area,from,to)
        || hmbtn_basket_blocked(&s->game.basket,s->area,from,to);
}
static bool basket_blocked(const void *context,const int32_t from[3],const int32_t to[3])
{
    const HmbtnSession *s=context;return hmbtn_basket_blocked(&s->game.basket,s->area,from,to);
}
static bool fishing_frame(HmbtnSession *s,const HmbtnExplorationFrame *f,HmbtnLogicalInput input,char *error,size_t size)
{
    const bool active=hmbtn_fishing_tick(&s->fishing,&s->intro.player,&s->house,&s->farm_action,
        f->ground,&s->game.clock,hmbtn_read_le16(f->section+2),s->village.flags,input.pressed,input.held);
    if(!active) return true;
    if(s->fishing.bank_pending) {
        if(!action_bank(f,10,error,size)) return false;
        s->fishing.bank_pending=false;
    }
    s->intro.action_active=s->fishing.using_action;
    if(s->fishing.berry_eaten) ++s->menu.stats.power_berries;
    if(s->fishing.sound>=0) {
        const HmbtnFarmSound *fx=hmbtn_farm_sound((unsigned)s->fishing.sound);
        if(!fx || !key(f,fx->bank==1 ? 2u : 0u,fx->program,fx->tone,fx->note,fx->volume,false,error,size)) {
            snprintf(error,size,"Original fishing sound unavailable");return false;
        }
    }
    s->control.mode=0;s->control.carrying=s->house.inventory->held_item.id!=0;s->control.carrying_big=false;
    return true;
}
/* Mine modals reuse the message renderer, base player ANI and normal area
 * transition. No second inventory, clock or travel loop. */
static bool mine_reward(HmbtnSession *s,const HmbtnExplorationFrame *f,char *error,size_t size)
{
    HmbtnMine *m=&s->mine;if(!m->reward || s->event_item.phase!=HMBTN_EVENT_ITEM_IDLE) return true;
    HmbtnIntroActor *p=&s->intro.player;
    if(m->reward_phase==1) {
        s->house.inventory->held_item=(HmbtnStoredTool){0};m->reward_phase=2;
        p->sprite=(HmbtnAnimationPlayer){(uint16_t)(m->reward==0x67 ? 5u : 29u+p->facing),0,0};p->animation_done=false;
        (void)emit(f,(HmbtnExplorationEffect){.kind=HMBTN_EXP_UI_SOUND,.id=m->reward==0x67 ? 74u : 8u},error,size);
    } else if(p->animation_done) {
        if(m->reward==0x66) hmbtn_economy_gold_change(&s->game.economy,hmbtn_farm_item(0x66)->sell_price,false);
        else {
            s->farm_action.max_stamina=s->farm_action.max_stamina+10>255 ? 255 : s->farm_action.max_stamina+10;
            s->farm_action.stamina=s->farm_action.max_stamina;if(s->farm_action.fatigue) --s->farm_action.fatigue;
            ++s->menu.stats.power_berries;
        }
        m->reward=m->reward_phase=0;p->pose=false;p->animation_done=false;
        p->sprite=(HmbtnAnimationPlayer){0,(uint16_t)p->facing,0};
    }
    return true;
}
static bool mine_modal(HmbtnSession *s,const HmbtnExplorationFrame *f,HmbtnLogicalInput input,char *error,size_t size)
{
    HmbtnMine *m=&s->mine;HmbtnIntroActor *p=&s->intro.player;s->control.mode=0;
    if(m->back_ticks) {
        const unsigned axis=p->facing&1u ? 0u : 2u;
        int32_t pos[3];memcpy(pos,p->position,sizeof(pos));pos[axis]+=p->facing<2 ? 41 : -41;
        if(hmbtn_farm_settle(NULL,f->ground,pos,0xC080,64)) memcpy(p->position,pos,sizeof(pos));
        if(!--m->back_ticks) p->sprite=(HmbtnAnimationPlayer){0,(uint16_t)p->facing,0};
        return true;
    }
    const bool ready=m->message.phase==HMBTN_MESSAGE_WAIT_END;
    if(ready && (input.pressed&(HMBTN_PAD_UP|HMBTN_PAD_DOWN))) {
        m->answer^=1;(void)emit(f,(HmbtnExplorationEffect){.kind=HMBTN_EXP_UI_SOUND,.id=5},error,size);
    }
    if(ready && (input.pressed&(HMBTN_PAD_CROSS|HMBTN_PAD_CIRCLE))) {
        const bool yes=!(input.pressed&HMBTN_PAD_CIRCLE) && !m->answer;
        if(yes) {
            s->travel_exit=(HmbtnHouseExit){0};s->travel_exit.target=hmbtn_mine_transition(m,s->travel_exit.entry);
            s->travel_tick=1;s->control.exit_pending=true;
        } else { m->prompt=0;m->back_ticks=10;p->sprite=(HmbtnAnimationPlayer){(uint16_t)(1+p->facing),0,0}; }
        m->message=(HmbtnMessage){0};s->dialogue.message=(HmbtnMessage){0};return true;
    }
    const int sound=hmbtn_message_tick(&m->message,input.pressed,input.held);
    if(sound>=0) (void)emit(f,(HmbtnExplorationEffect){.kind=HMBTN_EXP_UI_SOUND,.id=(unsigned)sound},error,size);
    s->dialogue.message=m->message;return true;
}
bool hmbtn_session_exploration_controls(HmbtnSession *s,const HmbtnExplorationFrame *f,
    HmbtnLogicalInput input,char *error,size_t error_size)
{
    if(!valid(s,f,error,error_size)) return false;
    const uint32_t held=input.held;
    if(s->control.enabled) {
        bool opened=false;
        if(s->house_lumber.phase) {
            int sound;
            hmbtn_house_lumber_tick(&s->house_lumber,s->house.inventory,&s->intro.player,&s->farm_action,
                &s->village.flags[0x45],f->section,f->section_size,0,&sound);
            s->control.mode=0;goto house_done;
        }
        if(s->mine.reward) { if(!mine_reward(s,f,error,error_size)) return false;goto house_done; }
        if(s->mine.prompt || s->mine.back_ticks) { if(!mine_modal(s,f,input,error,error_size)) return false;goto house_done; }
        if(s->event_item.phase!=HMBTN_EVENT_ITEM_IDLE) goto house_done;
        if(s->fishing.phase!=HMBTN_FISHING_FREE) {
            if(!fishing_frame(s,f,input,error,error_size)) return false;
            goto house_done;
        }
        if(s->feed_shop.departure && !s->talk.active && s->feed_shop.phase==HMBTN_SHOP_OFF) {
            if(!hmbtn_village_reset_schedule(&s->village,4,error,error_size)) return false;
            s->feed_shop.departure=false;s->travel_exit=(HmbtnHouseExit){.target=32,.entry={1000,0,-2800}};
            s->intro.player.facing=3;s->travel_tick=1;s->control.exit_pending=true;
        }
        if(s->talk.active && s->feed_shop.phase==HMBTN_SHOP_OFF) {
            if(!boundary(s,f,HMBTN_EXP_TALK,0,input,NULL,error,error_size)) return false;
            goto house_done;
        }
        if(s->feed_shop.phase!=HMBTN_SHOP_OFF) {
            hmbtn_feed_shop_tick(&s->feed_shop,&s->house,hmbtn_game_economy(&s->game),&s->menu.stats,input.pressed,held);
            if(s->feed_shop.phase==HMBTN_SHOP_OFF) {
                if(!s->feed_shop.seeds && !s->feed_shop.rucksack && !s->feed_shop.basket && !s->feed_shop.groceries && !s->feed_shop.wrapping && !s->feed_shop.carpenter && !s->feed_shop.blacksmith && !s->feed_shop.clinic && !s->feed_shop.wine && !s->feed_shop.inn && !s->feed_shop.kai && !s->feed_shop.fish_food) {
                    HmbtnIntroActor *merchant=&s->village.people[s->feed_shop.merchant].actor;
                    merchant->facing=s->feed_shop.saved_facing;
                    merchant->sprite=(HmbtnAnimationPlayer){0,(uint16_t)merchant->facing,0};
                }
            }
            if(!feed_shop_message(s,f,error,error_size)) return false;
            goto house_done;
        }
        if(!s->travel_tick && !s->door_tick && !s->quick_ticks && s->house.mode==HMBTN_HOUSE_FREE) {
            const bool menu_opened=!s->basket_action.phase && !s->carry.phase && !s->feed.phase && !s->poultry.carry_phase && s->farm_action.phase==HMBTN_FARM_ACTION_FREE
                && hmbtn_farm_menu_open(&s->menu,input.pressed);
            if(s->menu.mode!=HMBTN_MENU_NONE) {
                if(menu_opened) { (void)emit(f,(HmbtnExplorationEffect){.kind=HMBTN_EXP_RELEASE_WATER},error,error_size); }
                else hmbtn_farm_menu_tick(&s->menu,s->house.inventory,input.pressed,held);
                if(s->menu.sound>=0) (void)emit(f,(HmbtnExplorationEffect){.kind=HMBTN_EXP_UI_SOUND,.id=(unsigned)s->menu.sound},error,error_size);
                if(s->menu.request && s->menu.loaded!=s->menu.request) {
                    uint8_t record[8192];
                    if(!text(f,3,s->menu.request,record,error,error_size)) return false;
                    const HmbtnStoredTool *selected=hmbtn_inventory_slot_const(s->house.inventory,s->menu.cursor);
                    if(!selected) selected=&s->house.inventory->equipped;
                    if(!hmbtn_house_expand_item_reading(&s->house,selected,record,sizeof(record))
                            || !message(f,record,&s->menu.message,error,error_size)) return false;
                    s->menu.message.suppress_cursor=!s->menu.detail;s->menu.loaded=s->menu.request;
                }
                s->dialogue.message=s->menu.message;
                goto house_done;
            }
        }
        if(!s->travel_tick && !s->door_tick && !s->sleep.active && !s->morning_fade) {
            hmbtn_farm_carry_flight(&s->carry,hmbtn_session_field(s),f->ground,hmbtn_game_economy(&s->game));
            if(!hmbtn_farm_carry_splashes(&s->carry,f->animation ? f->animation(f->context,HMBTN_EXP_STAGE,0) : NULL,error,error_size)) return false;
            if(s->carry.request) {
                s->house.mode=HMBTN_HOUSE_BOOK;s->house.message_bank=3;s->house.request=s->carry.request;
                s->house.loaded=0;s->house.closing=s->house.detail=false;s->house.sound=3;opened=true;
                s->carry.request=0;goto house_interaction;
            }
            if(s->carry.small_fish_placed) s->village.flags[0x5E]=true;
            if(s->carry.sound>=0) {
                const HmbtnFarmSound *fx=hmbtn_farm_sound((unsigned)s->carry.sound);
                if(!key(f,2,fx->program,fx->tone,fx->note,fx->volume,false,error,error_size)) return false;
            }
            if(s->carry.changed && !emit(f,(HmbtnExplorationEffect){.kind=HMBTN_EXP_RETEXTURE},error,error_size)) return false;
        }
        s->menu.stats.discarded=s->carry.discarded;
        if(s->basket_action.bounce) --s->basket_action.bounce;
        if(s->basket_action.phase) goto basket_interaction;
        /* Door input precedes pickup, gifts and throwing, including refusals.
         * Inventory remains owned by the session across the transition. */
        if(!s->carry.phase && !s->feed.phase && !s->poultry.carry_phase && !s->quick_ticks
                && !s->travel_tick && !s->door_tick && !s->control.jumping && !s->control.exit_pending
                && s->house.mode==HMBTN_HOUSE_FREE && s->farm_action.phase==HMBTN_FARM_ACTION_FREE
                && (input.pressed&HMBTN_PAD_CROSS)
                && hmbtn_world_door_at(f->section,f->section_size,&s->intro.player,&s->travel_exit,&s->door_model)) {
            const HmbtnFarmItem *item=hmbtn_farm_item(s->house.inventory->held_item.id);
            s->control.carrying=s->house.inventory->held_item.id!=0;
            s->control.carrying_big=s->control.carrying && item && (item->flags&0x180u);
            HmbtnExplorationBoundaryResult result={0};
            if(!boundary(s,f,HMBTN_EXP_DOOR,0,input,&result,error,error_size)) return false;
            opened=result.opened;
            if(result.consumed) goto house_done;
            if(opened) goto house_interaction;
        }
        /* A targeted resident handles the held gift before pickup/throw.
         * Merchants retain the original empty-handed shop priority. */
        if(!s->carry.phase && !s->feed.phase && !s->poultry.carry_phase && !s->quick_ticks
                && !s->travel_tick && !s->door_tick && !s->control.jumping && !s->control.exit_pending
                && s->house.mode==HMBTN_HOUSE_FREE && s->farm_action.phase==HMBTN_FARM_ACTION_FREE
                && (input.pressed&HMBTN_PAD_CROSS)) {
            if(hmbtn_inn_shop_open(&s->feed_shop,&s->village,&s->intro.player,&s->game.clock,s->area,
                    f->section,f->section_size,s->house.inventory)
                    || hmbtn_fish_food_shop_open(&s->feed_shop,&s->village,&s->intro.player,&s->game.clock,s->area,
                    f->section,f->section_size,s->house.inventory)
                    || hmbtn_basket_shop_open(&s->feed_shop,&s->village,&s->intro.player,&s->game.clock,s->area,
                    f->section,f->section_size,s->house.inventory)
                    || hmbtn_rucksack_shop_open(&s->feed_shop,&s->village,&s->intro.player,&s->game.clock,s->area,
                    f->section,f->section_size,s->house.inventory)
                    || hmbtn_wrapping_shop_open(&s->feed_shop,&s->village,&s->intro.player,&s->game.clock,s->area,
                    f->section,f->section_size,s->house.inventory)
                    || hmbtn_grocery_shop_open(&s->feed_shop,&s->village,&s->intro.player,&s->game.clock,s->area,
                    f->section,f->section_size,s->house.inventory)
                    || hmbtn_seed_shop_open(&s->feed_shop,&s->village,&s->intro.player,&s->game.clock,s->area,
                    f->section,f->section_size,s->house.inventory->held_item.id)
                    || hmbtn_feed_shop_open(&s->feed_shop,&s->village,&s->intro.player,&s->game.clock,s->area,s->house.inventory->held_item.id)) {
                if(!s->feed_shop.seeds && !s->feed_shop.rucksack && !s->feed_shop.basket && !s->feed_shop.groceries && !s->feed_shop.wrapping && !s->feed_shop.carpenter && !s->feed_shop.inn && !s->feed_shop.fish_food) {
                    HmbtnIntroActor *merchant=&s->village.people[s->feed_shop.merchant].actor;
                    merchant->facing=(s->intro.player.facing+2)&3u;
                    merchant->sprite=(HmbtnAnimationPlayer){0,(uint16_t)merchant->facing,0};
                }
                if(!feed_shop_message(s,f,error,error_size)) return false;
                goto house_done;
            }
            unsigned id=hmbtn_village_zone_target(&s->village,f->section,f->section_size,&s->intro.player);
            if(!id) id=hmbtn_village_target(&s->village,&s->intro.player);
            if(id) {
                if(s->visit.active && (id==17 || s->visit.event==130 || s->visit.event==289
                            || ((s->visit.event==18 || s->visit.event==19) && id==3))) {
                    s->visit.talk=true;s->visit.talk_target=id;input.pressed&=~HMBTN_PAD_CROSS;
                    (void)emit(f,(HmbtnExplorationEffect){.kind=HMBTN_EXP_UI_SOUND,.id=3},error,error_size);goto house_done;
                }
                if(!s->visit.active) {
                    const unsigned mode=id==6 && hmbtn_read_le16(f->section+2)==27 ? 10u : 3u;
                    const unsigned event=hmbtn_farm_event_trigger(&s->triggers,&s->village,&s->game.clock,s->game.weather.today,
                        s->previous_place,mode,id,s->house.inventory);
                    if(event) { if(!boundary(s,f,HMBTN_EXP_START_VISIT,event,input,NULL,error,error_size)) return false;goto house_done; }
                }
                if(!boundary(s,f,HMBTN_EXP_START_TALK,id,(HmbtnLogicalInput){0},NULL,error,error_size)) return false;
                (void)emit(f,(HmbtnExplorationEffect){.kind=HMBTN_EXP_UI_SOUND,.id=3},error,error_size);goto house_done;
            }
        }
        if(!s->carry.phase && !s->feed.phase && !s->house.inventory->held_item.id && !s->quick_ticks
                && !s->travel_tick && !s->door_tick && !s->control.jumping && !s->control.exit_pending
                && s->house.mode==HMBTN_HOUSE_FREE && s->farm_action.phase==HMBTN_FARM_ACTION_FREE
                && (input.pressed&HMBTN_PAD_CROSS)) {
            const unsigned target=hmbtn_livestock_target(&s->livestock,s->area,&s->intro.player,224);
            if(target<HMBTN_LIVESTOCK) {
                s->house.mode=HMBTN_HOUSE_BOOK;s->house.message_bank=0;
                s->house.request=hmbtn_livestock_talk(&s->livestock,target);s->house.loaded=0;
                s->house.closing=s->house.detail=false;s->house.sound=3;opened=true;goto house_interaction;
            }
        }
        if(!s->carry.phase && !s->house.inventory->held_item.id && !s->quick_ticks && !s->travel_tick && !s->door_tick
                && !s->control.jumping && !s->control.exit_pending && s->house.mode==HMBTN_HOUSE_FREE
                && s->farm_action.phase==HMBTN_FARM_ACTION_FREE
                && (input.pressed&HMBTN_PAD_CROSS)) {
            bool taken=(s->area==112 || s->area==0) ? hmbtn_poultry_take_egg(&s->poultry,s->area,&s->house,&s->carry,&s->intro.player)
                : hmbtn_farm_gather_take(&s->gather,hmbtn_read_le16(f->section+2),s->game.clock.season,&s->house,&s->carry,&s->intro.player);
            if(taken) (void)emit(f,(HmbtnExplorationEffect){.kind=HMBTN_EXP_UI_SOUND,.id=7},error,error_size);
        }
        if(!s->carry.phase && !s->feed.phase && !s->poultry.carry_phase && !s->quick_ticks && !s->travel_tick && !s->door_tick
                && !s->control.jumping && !s->control.exit_pending && s->house.mode==HMBTN_HOUSE_FREE
                && s->farm_action.phase==HMBTN_FARM_ACTION_FREE) {
            int sound;
            if(hmbtn_house_lumber_tick(&s->house_lumber,s->house.inventory,&s->intro.player,&s->farm_action,
                    &s->village.flags[0x45],f->section,f->section_size,input.pressed,&sound)) {
                if(sound>=0) (void)emit(f,(HmbtnExplorationEffect){.kind=HMBTN_EXP_UI_SOUND,.id=(unsigned)sound},error,error_size);
                s->control.mode=0;goto house_done;
            }
        }
basket_interaction:
        if(s->basket_action.phase || (!s->carry.phase && !s->feed.phase && !s->poultry.carry_phase && !s->quick_ticks
                && !s->travel_tick && !s->door_tick && !s->control.jumping && !s->control.exit_pending
                && s->house.mode==HMBTN_HOUSE_FREE && s->farm_action.phase==HMBTN_FARM_ACTION_FREE)) {
            if(hmbtn_basket_tick(&s->game.basket,&s->basket_action,s->house.inventory,&s->game.economy,&s->intro.player,
                    s->area,f->ground,hmbtn_session_field(s),input.pressed,world_blocked,s)) {
                if(s->basket_action.sound>=0) (void)emit(f,(HmbtnExplorationEffect){.kind=HMBTN_EXP_UI_SOUND,.id=(unsigned)s->basket_action.sound},error,error_size);
                if(s->basket_action.request) {
                    s->house.mode=HMBTN_HOUSE_BOOK;s->house.message_bank=3;s->house.request=s->basket_action.request;
                    s->house.loaded=0;s->house.closing=s->house.detail=false;s->house.sound=3;opened=true;
                    goto house_interaction;
                }
                s->control.mode=0;goto house_done;
            }
        }
        if(s->poultry.carry_phase || (!s->carry.phase && !s->feed.phase && !s->quick_ticks && !s->travel_tick && !s->door_tick
                && !s->control.jumping && !s->control.exit_pending && s->house.mode==HMBTN_HOUSE_FREE
                && s->farm_action.phase==HMBTN_FARM_ACTION_FREE && (input.pressed&HMBTN_PAD_CROSS))) {
            HmbtnHouseExit door;unsigned model;
            const bool at_door=!s->poultry.carry_phase && hmbtn_world_door_at(f->section,0x2000,&s->intro.player,&door,&model);
            if(!at_door && hmbtn_poultry_carry_tick(&s->poultry,&s->house,&s->intro.player,s->area,
                    f->ground,hmbtn_session_field(s),input.pressed)) {
                if(s->poultry.sound>=0) (void)emit(f,(HmbtnExplorationEffect){.kind=HMBTN_EXP_UI_SOUND,.id=(unsigned)s->poultry.sound},error,error_size);
                s->control.carrying=s->house.inventory->held_item.id!=0;
                s->control.carrying_big=hmbtn_poultry_held_slot(&s->poultry)<HMBTN_HENS;
                s->control.mode=0;memcpy(s->intro.camera,s->intro.player.position,sizeof(s->intro.camera));
                goto house_done;
            }
        }
        if(s->carry.phase || ((hmbtn_session_field(s) || (s->area==56 && s->house.inventory->held_item.id)) && !s->quick_ticks && !s->travel_tick && !s->door_tick
                && !s->control.jumping && s->house.mode==HMBTN_HOUSE_FREE
                && s->farm_action.phase==HMBTN_FARM_ACTION_FREE && (input.pressed&HMBTN_PAD_CROSS))) {
            if(hmbtn_farm_carry_tick(&s->carry,&s->house,hmbtn_session_field(s),
                    &s->intro.player,input.pressed)) {
                if(s->carry.sound>=0) {
                    const HmbtnFarmSound *fx=hmbtn_farm_sound((unsigned)s->carry.sound);
                    if(!key(f,2,fx->program,fx->tone,fx->note,fx->volume,false,error,error_size)) return false;
                }
                if(s->carry.changed && !emit(f,(HmbtnExplorationEffect){.kind=HMBTN_EXP_RETEXTURE},error,error_size)) return false;
                s->control.mode=0;
                memcpy(s->intro.camera,s->intro.player.position,sizeof(s->intro.camera));
                goto house_done;
            }
        }
        if(s->quick_ticks) {
            if(!--s->quick_ticks) {
                s->quick_id=0;s->intro.player.sprite=(HmbtnAnimationPlayer){0,(uint16_t)s->intro.player.facing,0};
            }
            goto house_done;
        }
        if(hmbtn_poultry_held_slot(&s->poultry)==HMBTN_HENS && !s->control.jumping && !s->control.exit_pending && s->house.mode==HMBTN_HOUSE_FREE
                && s->farm_action.phase==HMBTN_FARM_ACTION_FREE) {
            const bool tool=(input.pressed&HMBTN_PAD_R1)!=0 || ((held&HMBTN_PAD_R1) && (input.pressed&(HMBTN_PAD_LEFT|HMBTN_PAD_RIGHT))),
                item=(input.pressed&HMBTN_PAD_R2)!=0 || ((held&HMBTN_PAD_R2) && (input.pressed&(HMBTN_PAD_LEFT|HMBTN_PAD_RIGHT)));
            const bool storing=item && !tool && s->house.inventory->held_item.id!=0;
            if((tool || item) && hmbtn_inventory_cycle(s->house.inventory,tool,(input.pressed&HMBTN_PAD_LEFT)!=0)) {
                s->quick_tool=tool;s->quick_id=tool ? s->house.inventory->equipped.id : s->house.inventory->held_item.id;s->quick_ticks=10;
                s->control.mode=0;
                s->intro.player.sprite=(HmbtnAnimationPlayer){(uint16_t)(tool ? (s->quick_id ? 71u : 0u) : (storing ? 29u : 11u)+s->intro.player.facing),0,0};s->intro.player.animation_done=false;
                if(tool && !s->quick_id) { s->quick_ticks=1;s->intro.player.sprite.frame=(uint16_t)s->intro.player.facing; }
                const HmbtnFarmSound *fx=hmbtn_farm_sound(7);
                if(!key(f,2,fx->program,fx->tone,fx->note,fx->volume,false,error,error_size)) return false;
                goto house_done;
            }
        }
        if(hmbtn_session_field(s) && f->field_art) hmbtn_field_effect_tick(s->field_effects,f->field_art,f->grass_cut_art);
        if(s->door_tick || s->travel_tick) {
            if(!boundary(s,f,HMBTN_EXP_TRANSITION,0,input,NULL,error,error_size)) return false;
            goto house_done;
        }
        if(!s->feed.phase && !s->carry.phase && !s->quick_ticks && !s->travel_tick && !s->door_tick
                && !s->control.exit_pending && !s->control.jumping && s->house.mode==HMBTN_HOUSE_FREE
                && s->farm_action.phase==HMBTN_FARM_ACTION_FREE && (input.pressed&HMBTN_PAD_SQUARE)
                && hmbtn_event_item_use(&s->event_item,&s->intro.player,s->house.inventory,&s->game.clock)) {
            if(s->event_item.phase==HMBTN_EVENT_ITEM_USE_DRINK) {
                /* player_load_tool_anim: mode 0x49 - base 0x3b = row 14. */
                if(!action_bank(f,14,error,error_size)) return false;
                s->intro.action_active=true;
            }
            s->control.mode=0;goto house_done;
        }
        if(!s->feed.phase && !s->carry.phase && !s->quick_ticks && !s->travel_tick && !s->door_tick
                && !s->house.inventory->held_item.id && !s->control.exit_pending && !s->control.jumping
                && s->house.mode==HMBTN_HOUSE_FREE && s->farm_action.phase==HMBTN_FARM_ACTION_FREE
                && (s->house.inventory->equipped.id==0x38 || s->house.inventory->equipped.id==0x39) && (input.pressed&HMBTN_PAD_SQUARE)) {
            if(!fishing_frame(s,f,input,error,error_size)) return false;
            if(s->fishing.phase!=HMBTN_FISHING_FREE) goto house_done;
        }
        const unsigned equipped=s->house.inventory->equipped.id;
        const bool animal_tool=(equipped>=0x1a && equipped<=0x1d) || s->farm_action.phase==HMBTN_FARM_ANIMAL_TOOL;
        if((hmbtn_session_field(s) || hmbtn_mine_kind(s->area) || (equipped>=0x1e && equipped<=0x20) || s->farm_action.phase==HMBTN_FARM_MEDICINE || animal_tool)
                && !s->feed.phase && !s->house.inventory->held_item.id && !s->control.exit_pending && !s->control.jumping && s->house.mode==HMBTN_HOUSE_FREE
                && (!(input.pressed&HMBTN_PAD_CROSS) || s->farm_action.phase!=HMBTN_FARM_ACTION_FREE)) {
            const bool medicine=s->farm_action.phase==HMBTN_FARM_MEDICINE || (equipped>=0x1e && equipped<=0x20);
            const bool acting=animal_tool ? hmbtn_livestock_tool_tick(&s->livestock,s->area,&s->farm_action,
                &s->intro.player,&s->house,&s->event_item,input.pressed)
                : medicine ? hmbtn_animal_medicine_tick(&s->poultry,&s->livestock,s->area,
                &s->farm_action,&s->intro.player,&s->house,input.pressed)
                : hmbtn_farm_action_mine_tick(&s->farm_action,&s->intro.player,&s->house,
                    hmbtn_session_field(s),f->ground,input.pressed,held,hmbtn_mine_kind(s->area) ? &s->mine : NULL,s->village.flags);
            if(s->farm_action.bank_pending) {
                if(!action_bank(f,(unsigned)s->farm_action.bank,error,error_size)) return false;
                s->intro.action_active=true;s->farm_action.bank_pending=false;
            }
            if(s->farm_action.finished_bank) s->intro.action_active=false;
            if(s->farm_action.sound>=0) {
                const HmbtnFarmSound *fx=hmbtn_farm_sound((unsigned)s->farm_action.sound);
                if(!fx || !key(f,fx->bank==1 ? 2u : 0u,fx->program,fx->tone,fx->note,fx->volume,
                        s->farm_action.kind==4 && s->farm_action.phase==HMBTN_FARM_TOOL_SWING,error,error_size)) {
                    snprintf(error,error_size,"Original farm action tone unavailable");return false;
                }
            }
            if(s->farm_action.release_sound) {
                (void)emit(f,(HmbtnExplorationEffect){.kind=HMBTN_EXP_RELEASE_WATER},error,error_size);
            }
            if(s->farm_action.broken_item) hmbtn_field_effect_submit_in(s->field_effects,hmbtn_session_field(s),
                s->farm_action.broken_item,s->farm_action.effect_column,s->farm_action.effect_row);
            if((hmbtn_session_field(s) || hmbtn_mine_kind(s->area)) && s->farm_action.changed && !emit(f,(HmbtnExplorationEffect){.kind=HMBTN_EXP_RETEXTURE},error,error_size)) {
                snprintf(error,error_size,"Cannot update field soil textures");return false;
            }
            if(s->mine.found) {
                const unsigned item=s->mine.found;
                if(!hmbtn_event_item_show(&s->event_item,&s->intro.player,s->house.inventory,item)) {
                    snprintf(error,error_size,"Cannot present original mined item %u",item);return false;
                }
                if(item==0x66 || item==0x67) { s->mine.reward=item;s->mine.reward_phase=1; }
                s->mine.found=0;
            }
            if(acting) {
                s->control.mode=0;
                memcpy(s->intro.camera,s->intro.player.position,sizeof(s->intro.camera));
                goto house_done;
            }
        }
        if(s->area==112 && !s->control.exit_pending && !s->control.jumping && s->house.mode==HMBTN_HOUSE_FREE
                && !s->quick_ticks && hmbtn_poultry_incubator_tick(&s->poultry,&s->feed,&s->house,
                    &s->intro.player,&s->game.clock,f->section,f->section_size,input.pressed)) {
            if(s->feed.sound>=0) (void)emit(f,(HmbtnExplorationEffect){.kind=HMBTN_EXP_UI_SOUND,.id=(unsigned)s->feed.sound},error,error_size);
            if(s->feed.request) {
                s->house.mode=HMBTN_HOUSE_BOOK;s->house.message_bank=2;s->house.request=s->feed.request;
                s->house.loaded=0;s->house.closing=s->house.detail=false;s->house.sound=3;opened=true;
            } else { s->control.mode=0;goto house_done; }
        }
        if(s->feed.phase || ((s->area==0 || s->area==96 || s->area==112) && !s->control.exit_pending
                && !s->control.jumping && s->house.mode==HMBTN_HOUSE_FREE && !s->quick_ticks)) {
            if((s->area==0 && hmbtn_farm_feed_pond_tick(&s->feed,&s->pond,&s->house,&s->intro.player,
                        s->game.clock.season,f->section,f->section_size,input.pressed))
                    || hmbtn_farm_feed_makers_tick(&s->feed,&s->makers,&s->house,&s->intro.player,
                        s->area,f->section,f->section_size,input.pressed)
                    || hmbtn_farm_feed_tick(&s->feed,&s->house,&s->intro.player,f->section,
                    f->section_size,input.pressed)) {
                if(s->feed.sound>=0) (void)emit(f,(HmbtnExplorationEffect){.kind=HMBTN_EXP_UI_SOUND,.id=(unsigned)s->feed.sound},error,error_size);
                if(s->feed.request) {
                    s->house.mode=HMBTN_HOUSE_BOOK;s->house.message_bank=2;s->house.request=s->feed.request;
                    s->house.loaded=0;s->house.closing=s->house.detail=false;s->house.sound=3;opened=true;
                } else { s->control.mode=0;goto house_done; }
            }
        }
        /* player_start_request checks furniture/the inn phone before throwing:
         * using a cabinet or calling with an item in hand must not discard it. */
        if((s->area==80 || s->area==204) && !opened && !s->control.exit_pending && !s->control.jumping
                && !s->quick_ticks && s->house.mode==HMBTN_HOUSE_FREE && (input.pressed&HMBTN_PAD_CROSS)
                && hmbtn_house_interact(&s->house,f->section,f->section_size,&s->intro.player)) {
            opened=true;goto house_interaction;
        }
        if(s->area!=0 && s->house.inventory->held_item.id && !s->control.exit_pending && !s->control.jumping
                && s->house.mode==HMBTN_HOUSE_FREE && !s->quick_ticks
                && hmbtn_farm_carry_tick(&s->carry,&s->house,NULL,&s->intro.player,input.pressed)) {
            s->control.mode=0;goto house_done;
        }
house_interaction:
        if(!opened && !s->control.exit_pending && !s->control.jumping
                && s->house.mode==HMBTN_HOUSE_FREE && (input.pressed&HMBTN_PAD_CROSS))
            opened=hmbtn_house_interact(&s->house,f->section,f->section_size,&s->intro.player);
        if(s->house.mode!=HMBTN_HOUSE_FREE) {
            if(!opened) hmbtn_house_interaction_tick(&s->house,input.pressed,held);
            if(s->house.sound>=0) (void)emit(f,(HmbtnExplorationEffect){.kind=HMBTN_EXP_UI_SOUND,.id=(unsigned)s->house.sound},error,error_size);
            if(s->house.request && s->house.loaded!=s->house.request) {
                uint8_t record[8192];
                if(!text(f,s->house.message_bank,s->house.request,record,error,error_size)) return false;
                if((s->house.message_bank==2 || s->house.request==21)
                        && !hmbtn_house_expand_reading(&s->house,record,sizeof(record))) {
                    snprintf(error,error_size,"Cannot expand original house message");return false;
                }
                if(s->house.message_bank==0 && s->house.request>=0x6bd && s->house.request<=0x6c1
                        && !hmbtn_livestock_expand(&s->livestock,record,sizeof(record))) {
                    snprintf(error,error_size,"Cannot expand original livestock message");return false;
                }
                if(!message(f,record,&s->house.message,error,error_size)) return false;
                s->house.loaded=s->house.request;
                s->house.message.suppress_cursor=s->house.mode==HMBTN_HOUSE_TV_SELECT
                    || s->house.mode==HMBTN_HOUSE_TOOLBOX
                    || s->house.mode==HMBTN_HOUSE_COOKING
                    || s->house.mode==HMBTN_HOUSE_BED
                    || (s->house.mode==HMBTN_HOUSE_PHONE && (s->house.phone_phase==0 || s->house.phone_phase==2))
                    || (s->house.mode==HMBTN_HOUSE_OPTIONS && !s->house.detail)
                    || (s->house.mode==HMBTN_HOUSE_CALENDAR && !s->house.detail);
                if(!emit(f,(HmbtnExplorationEffect){.kind=HMBTN_EXP_MESSAGE_LOADED,.id=s->house.request,.bank=s->house.mode},error,error_size)) return false;
            }
            if(s->house.mode!=HMBTN_HOUSE_SLEEP)
                s->intro.player.sprite=(HmbtnAnimationPlayer){0,(uint16_t)s->intro.player.facing,0};
        } else if(!opened) {
            s->control.npc_blocked=world_blocked;s->control.object_blocked=basket_blocked;s->control.collision_context=s;
            s->control.carrying=s->house.inventory->held_item.id!=0;
            s->control.carrying_big=hmbtn_poultry_held_slot(&s->poultry)<HMBTN_HENS;
            hmbtn_farm_control_tick(&s->control,&s->intro.player,f->ground,hmbtn_session_field(s),
                hmbtn_house_world_buttons(&s->house.options,held));
            if(s->control.jump_started && !key(f,2,0,0,60,127,false,error,error_size)) {
                snprintf(error,error_size,"Original jump tone unavailable"); return false;
            }
            hmbtn_house_options_idle(&s->house.options,s->control.mode!=0,s->intro.player.facing);
            if(s->control.mode) { s->farm_action.strikes=0;s->farm_action.target=HMBTN_FIELD_CELLS; }
            if(s->control.mode && !s->control.exit_pending && !s->control.jumping && !s->control.landed) {
                if(s->house.steps>(s->control.mode==4 ? 8u : 12u)) {
                    if(!key(f,2,8,0,68,20,false,error,error_size)) {
                        snprintf(error,error_size,"Original footstep tone is unavailable"); return false;
                    }
                    s->house.steps=0;
                } else ++s->house.steps;
            }
        }
        if(!emit(f,(HmbtnExplorationEffect){.kind=HMBTN_EXP_STEREO,.id=s->house.options.stereo},error,error_size)) return false;
        if(s->house.boundary) {
            if(s->house.boundary==2) s->load_requested=true;
            else if(!emit(f,(HmbtnExplorationEffect){.kind=HMBTN_EXP_UNSUPPORTED_HOUSE,.id=s->house.boundary},error,error_size)) return false;
            s->house.boundary=0;
        }
        /* Waking reloads the house and retires this frame's section loan.
         * Keep presentation updates, but leave exit probes for a fresh frame. */
        const bool sleeping=s->house.mode==HMBTN_HOUSE_SLEEP;
        if(sleeping && !boundary(s,f,HMBTN_EXP_SLEEP,0,input,NULL,error,error_size)) return false;
        if(!emit(f,(HmbtnExplorationEffect){.kind=HMBTN_EXP_HOUSE_ART},error,error_size)) return false;
        s->dialogue.message=s->house.message;
        if(s->house.mode==HMBTN_HOUSE_COOKING) s->dialogue.choice=s->house.kitchen.choice;
        if(hmbtn_mine_kind(s->area) && !sleeping && !s->control.exit_pending && !s->control.jumping
                && !s->quick_ticks && !s->carry.phase && s->farm_action.phase==HMBTN_FARM_ACTION_FREE
                && s->house.mode==HMBTN_HOUSE_FREE && s->event_item.phase==HMBTN_EVENT_ITEM_IDLE) {
            if(s->mine.kind==1) s->village.flags[0x63]=true;
            if(hmbtn_read_le16(f->section+2)==0x48) s->village.flags[0x64]=true;
            const unsigned prompt=hmbtn_mine_check(&s->mine,&s->intro.player);
            if(prompt) {
                if(!hmbtn_mine_prompt(&s->mine,prompt,error,error_size)) return false;
                s->control.mode=0;s->intro.player.sprite=(HmbtnAnimationPlayer){0,(uint16_t)s->intro.player.facing,0};
                s->dialogue.message=s->mine.message;goto house_done;
            }
        }
        HmbtnHouseExit exit;
        if(!sleeping && s->house.mode==HMBTN_HOUSE_FREE && !s->control.exit_pending && s->village_ready) {
            if(hmbtn_world_gate_at(f->section,f->section_size,s->intro.player.position,&exit)) {
                const unsigned event=hmbtn_read_le16(f->section+16+exit.zone*44+10);
                const int refusal=hmbtn_village_door(&s->village,event,&s->game.clock);
                if(refusal<0) {
                    s->travel_exit=exit;s->travel_tick=1;s->control.exit_pending=true;
                } else if(s->refused_zone!=(int)exit.zone) {
                    s->refused_zone=(int)exit.zone;
                    s->house.mode=HMBTN_HOUSE_BOOK;s->house.message_bank=2;s->house.request=(unsigned)refusal;
                    s->house.loaded=0;s->house.closing=s->house.detail=false;
                    /* The common loader opens this on the next tick. */
                    memset(&s->house.message,0,sizeof(s->house.message));s->house.message.phase=HMBTN_MESSAGE_OPENING;
                }
            } else s->refused_zone=-1;
        }
        if(!sleeping && s->house.mode==HMBTN_HOUSE_FREE && !s->control.exit_pending && hmbtn_house_exit_at(f->section,
                f->section_size,s->intro.player.position,&exit)) {
            unsigned logical,resource;
            if(hmbtn_world_area(exit.target,s->game.clock.season,&logical,&resource)) {
                s->control.exit_pending=true;s->control.exit_zone=exit.zone;
                s->intro.player.sprite=(HmbtnAnimationPlayer){0,(uint16_t)s->intro.player.facing,0};
                s->travel_exit=exit;s->travel_tick=1;
            }
        }
        memcpy(s->intro.camera,s->sleep.active ? s->sleep.camera : s->intro.player.position,sizeof(s->intro.camera));
    }
house_done:
    return true;
}
bool hmbtn_session_exploration_world(HmbtnSession *s,const HmbtnExplorationFrame *f,
    unsigned *out_changes,bool *out_held,char *error,size_t error_size)
{
    if(!valid(s,f,error,error_size)) return false;
    if(out_changes) *out_changes=0;
    if(out_held) *out_held=true;
    if(s->completion_reported) {
        s->village.player_running=s->control.mode==4;
        const bool held_clock=!s->control.enabled || s->travel_tick || s->door_tick || s->control.exit_pending
            || s->sleep.active || s->morning_fade || s->intro.message || s->first_visit_active
            || s->menu.mode!=HMBTN_MENU_NONE || s->house.mode!=HMBTN_HOUSE_FREE || s->feed_shop.phase!=HMBTN_SHOP_OFF
            || s->talk.active || (s->event_item.phase!=HMBTN_EVENT_ITEM_IDLE && !hmbtn_event_item_consuming(&s->event_item)) || s->mine.prompt || s->mine.back_ticks || s->mine.reward;
        const unsigned changes=hmbtn_farm_clock_tick(&s->game.clock,held_clock
            || hmbtn_farm_clock_place_held(f->section,16));
        hmbtn_session_clock_events(s,&s->game.clock,changes);
        s->menu.stats.animals[0]=hmbtn_poultry_count(&s->poultry);
        if(!held_clock) {
            hmbtn_livestock_push(&s->livestock,s->area,&s->intro.player,
                s->control.mode==2 && !s->control.carrying && !s->control.jumping
                && s->farm_action.phase==HMBTN_FARM_ACTION_FREE);
            hmbtn_livestock_tick(&s->livestock,s->area,f->ground,hmbtn_session_field(s),
                f->section,f->section_size,&s->area_doors,&s->intro.player,s->game.clock.hour);
            for(unsigned i=0;i<HMBTN_LIVESTOCK;++i) {
                HmbtnFarmAnimal *a=&s->livestock.animals[i];
                if(a->occupied && !a->dead && a->area==s->area
                        && !animate(f,HMBTN_EXP_LIVESTOCK,hmbtn_livestock_pack(a),&a->actor.sprite,error,error_size)) return false;
            }
            hmbtn_poultry_tick(&s->poultry,s->area,f->ground,hmbtn_session_field(s),&s->intro.player,s->game.clock.hour);
            for(unsigned i=0;i<HMBTN_HENS;++i) if(s->poultry.hens[i].occupied && !s->poultry.hens[i].dead
                    && !s->poultry.hens[i].held && s->poultry.hens[i].area==s->area
                    && s->poultry.hens[i].stage!=HMBTN_HEN_EGG
                    && !animate(f,s->poultry.hens[i].stage==HMBTN_HEN_CHICK ? HMBTN_EXP_CHICK : HMBTN_EXP_HEN,0,
                        &s->poultry.hens[i].actor.sprite,error,error_size)) return false;
        }
        if(s->village_ready) {
            const bool internal_walk=s->door_tick && s->travel_exit.target==0xfffe;
            hmbtn_area_doors_bind(&s->area_doors,f->scene);
            hmbtn_area_doors_tick(&s->area_doors,held_clock && !internal_walk);
            if(!hmbtn_village_sync(&s->village,&s->game.clock,error,error_size)) return false;
            if(f->village_assets && !f->village_assets(f->context,hmbtn_read_le16(f->section+12),&s->game.clock,error,error_size)) return false;
            hmbtn_village_tick(&s->village,f->section,f->section_size,f->ground,held_clock);
            /* Player transitions own their door pose until the area changes. */
            if((!s->door_tick || internal_walk) && !s->travel_tick) {
                hmbtn_area_doors_apply(&s->area_doors,f->scene);
                for(unsigned i=0;i<HMBTN_SCENE_MODELS;++i) {
                    HmbtnAreaDoor *door=&s->area_doors.models[i];
                    const unsigned sound=door->opening_sound ? 0x16u : door->closing_sound ? 0x4cu : 0;
                    if(sound) {
                        const HmbtnFarmSound *fx=hmbtn_farm_sound(sound);
                        if(!fx || !key(f,fx->bank==1 ? 2u : 0u,fx->program,fx->tone,fx->note,fx->volume,false,error,error_size)) return false;
                    }
                    door->opening_sound=door->closing_sound=false;
                }
            }
            if(s->talk.active && s->talk.released) s->village.people[s->talk.target].actor.visible=false;
            /* villager_set_action: Ellen remains seated; Gotz works on farm row 4. */
            for(unsigned id=1;id<37;++id) if(!hmbtn_village_event_actor(&s->village,id) && s->village.people[id].actor.visible) {
                HmbtnVillager *p=&s->village.people[id];
                const unsigned special=id==23 ? 1 : id==24 && s->area==0 && p->row==4 ? p->actor.facing+5 : 0;
                if(special && p->actor.sprite.animation!=special)
                    p->actor.sprite=(HmbtnAnimationPlayer){(uint16_t)special,0,0};
            }
            if(!held_clock) for(unsigned id=1;id<37;++id) if(!hmbtn_village_event_actor(&s->village,id) && s->village.people[id].actor.visible) {
                const HmbtnAnimation *ani=f->animation ? f->animation(f->context,HMBTN_EXP_VILLAGER,id) : NULL;
                if(ani && !hmbtn_animation_tick(ani,&s->village.people[id].actor.sprite,error,error_size)) return false;
            }
        }

        if(changes&HMBTN_CLOCK_DAY) s->overnight_tick=1;
        if(changes&HMBTN_CLOCK_HOUR) {
            if(s->area==0 && !emit(f,(HmbtnExplorationEffect){.kind=HMBTN_EXP_RETEXTURE},error,error_size)) return false;
            s->farm_action.fatigue+=hmbtn_farm_clock_fatigue(s->game.clock.hour);
            if(s->farm_action.fatigue>100) s->farm_action.fatigue=100;
            if(s->game.clock.hour==6 && s->game.clock.minute==0 && s->farm_action.fatigue==100) s->farm_action.fatigue=99;
        }
        if(out_changes) *out_changes=changes;
        if(out_held) *out_held=held_clock;
        /* weather_fx_frame is independent of the clock/control lock: doors
         * and character dialogue keep particles and the sky tint running. */
        const bool weather_held=s->menu.mode!=HMBTN_MENU_NONE || !hmbtn_world_outdoors(f->section,f->section_size);
        hmbtn_weather_effects_tick(&s->weather_effects,s->game.weather.today,weather_held);
        hmbtn_daylight_weather_tick(&s->daylight,&s->game.clock,s->game.weather.today,weather_held);
    }
    return true;
}
bool hmbtn_session_exploration_animate(HmbtnSession *s,const HmbtnExplorationFrame *f,char *error,size_t size)
{
    if(!valid(s,f,error,size)) return false;
    if(!s->intro.message && s->menu.mode==HMBTN_MENU_NONE && s->farm_action.phase!=HMBTN_FARM_TOOL_CHARGE
            && (!s->sleep.active || s->sleep.phase<4)) {
        const HmbtnExplorationAnimation kind=s->intro.action_active ? HMBTN_EXP_ACTION
            : s->first_visit_active ? HMBTN_EXP_EVENT_PLAYER : HMBTN_EXP_PLAYER;
        const HmbtnAnimation *ani=f->animation ? f->animation(f->context,kind,0) : NULL;
        if(!hmbtn_actor_animation_tick(ani,&s->intro.player,error,size)) return false;
    }
    return true;
}
bool hmbtn_session_exploration_tick(HmbtnSession *s,const HmbtnExplorationFrame *f,
    HmbtnLogicalInput input,char *error,size_t size)
{
    if(!valid(s,f,error,size)) return false;
    if(!s->completion_reported || s->first_visit_active || s->animal_event || s->visit.active
            || s->talk.active || s->event_item.phase!=HMBTN_EVENT_ITEM_IDLE || !s->intro.script.finished
            || s->travel_tick || s->door_tick || s->sleep.active || s->house.mode==HMBTN_HOUSE_SLEEP
            || s->morning_fade || s->overnight_tick) {
        snprintf(error,size,"Full exploration tick requires free-world handoff; scripted tick is MA-07");return false;
    }
    if(!hmbtn_session_exploration_controls(s,f,input,error,size)
            || !hmbtn_session_exploration_world(s,f,NULL,NULL,error,size)
            || !hmbtn_session_exploration_animate(s,f,error,size)) return false;
    if(s->control.enabled && !s->sleep.active)
        hmbtn_world_camera(f->section,f->section_size,s->intro.camera);
    return true;
}

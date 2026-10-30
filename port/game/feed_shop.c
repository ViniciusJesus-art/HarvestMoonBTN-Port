/* shop_dialogues.c: feed, chicken purchase/sale, quantity panel and reopen.
 * Business predicates are the initial branches of original talk scripts 365/368. */
#include "feed_shop.h"
#include "psx_random.h"
#include "saibara_gifts.inc"
#include "inventory.h"
#include "basket.h"
#include "tool_upgrade.h"
#include "farm_gather.h"
#include "economy.h"
#include "farm_shipping.h"
#include "psx_data.h"
#include <stdlib.h>
#include <string.h>
#include "seed_menu.inc"
#include "animal_tool_menu.inc"
unsigned hmbtn_seed_shop_entries(unsigned season)
{
    return season==0 ? (unsigned)(sizeof(spring_seeds)/sizeof(*spring_seeds))
        : season==1 ? (unsigned)(sizeof(summer_seeds)/sizeof(*summer_seeds))
        : season==2 ? (unsigned)(sizeof(fall_seeds)/sizeof(*fall_seeds)) : 0;
}
const HmbtnSeedShopEntry *hmbtn_seed_shop_entry(unsigned season,unsigned index)
{
    if(index>=hmbtn_seed_shop_entries(season)) return NULL;
    return season==0 ? &spring_seeds[index] : season==1 ? &summer_seeds[index] : &fall_seeds[index];
}
const HmbtnSeedShopEntry *hmbtn_won_seed_entry(unsigned index)
{ return index<sizeof(won_seeds)/sizeof(*won_seeds) ? &won_seeds[index] : NULL; }
static bool won_available(const HmbtnFeedShop *s,const HmbtnSeedShopEntry *entry)
{
    return hmbtn_farm_crop_supported(entry->item)
        && (entry->item!=0x31 || (s->progress && s->progress->moondrop_seeds));
}
unsigned hmbtn_seed_shop_count(const HmbtnFeedShop *s)
{
    if(!s || !s->seeds) return 0;
    if(s->merchant==35) {
        unsigned count=0;
        for(unsigned i=0;i<sizeof(won_seeds)/sizeof(*won_seeds);++i)
            if(won_available(s,&won_seeds[i])) ++count;
        return count;
    }
    return s->clock ? hmbtn_seed_shop_entries(s->clock->season)
        +(hmbtn_farm_shipping_seed_unlocked(s->economy,s->clock->season) ? 1u : 0u) : 0;
}
const HmbtnSeedShopEntry *hmbtn_seed_shop_choice(const HmbtnFeedShop *s,unsigned index)
{
    if(index>=hmbtn_seed_shop_count(s)) return NULL;
    if(s->merchant==35) {
        /* shop_seed_menu_setup_d order, filtered to implemented crops. A
         * visible row is not a source row (Pink Cat=4, Toy Flower=6). Keep
         * each source entry's own description/name rather than index math. */
        for(unsigned i=0;i<sizeof(won_seeds)/sizeof(*won_seeds);++i) {
            if(!won_available(s,&won_seeds[i])) continue;
            if(!index) return &won_seeds[i];
            --index;
        }
        return NULL;
    }
    return index==hmbtn_seed_shop_entries(s->clock->season) ? &conditional_seeds[s->clock->season]
        : hmbtn_seed_shop_entry(s->clock->season,index);
}
bool hmbtn_feed_shop_expand(uint8_t *record,size_t size,const HmbtnRegistration *registration)
{
    if(!record || size!=8192 || !registration) return false;
    uint16_t words[HMBTN_MESSAGE_WORDS];unsigned count=0;
    for(unsigned i=0;i<HMBTN_MESSAGE_WORDS;++i) {
        const uint16_t code=hmbtn_read_le16(record+0x1800+i*2);
        const uint16_t *name=code==0x400A ? registration->player_name
            : code==0x400D ? registration->farm_name : NULL;
        if(name) {
            for(unsigned j=0;j<HMBTN_NAME_CHARS && name[j]!=0xFFFF;++j) {
                if(count>=HMBTN_MESSAGE_WORDS) return false;
                words[count++]=name[j];
            }
        } else {
            if(count>=HMBTN_MESSAGE_WORDS) return false;
            words[count++]=code;
        }
        if(code==0xFFFF) {
            for(unsigned j=0;j<count;++j) hmbtn_write_le16(record+0x1800+j*2,words[j]);
            return true;
        }
    }
    return false;
}
unsigned hmbtn_feed_shop_cost(const HmbtnFeedShop *s)
{
    const HmbtnFarmItem *item=hmbtn_farm_item(s->item);
    return item && item->buy_price>0 ? (unsigned)item->buy_price*s->quantity : 0;
}
static void start_message(HmbtnFeedShop *s,HmbtnFeedShopPhase phase,unsigned message)
{
    s->phase=phase;s->request=message;s->loaded=0;s->closing=false;s->bank=2;
    s->message=(HmbtnMessage){0};s->repeat=0;s->previous_arrows=0;
}
static void next(HmbtnFeedShop *s,HmbtnFeedShopPhase phase,unsigned message)
{
    s->next=phase;s->next_request=message;s->closing=true;s->next_bank=2;
    s->message.phase=HMBTN_MESSAGE_CLOSING;s->message.box_frame=0;
}
static bool has_medicine(const HmbtnHouseInteraction *h)
{
    /* milestone_probe_animal_medicine: same represented stores as scripts. */
    return hmbtn_inventory_count(h->inventory,0x1E,0,2)>0;
}
static bool buy_medicine(HmbtnHouseInteraction *h)
{
    return !has_medicine(h) && hmbtn_inventory_store_tool(h->inventory,
        (HmbtnStoredTool){0x1E,0,1},false,NULL);
}
static void reset_shop(HmbtnFeedShop *s)
{
    HmbtnBasket *basket=s->basket_state;
    uint32_t *random=s->random;
    const HmbtnFarmAction *action=s->action;
    HmbtnLivestock *livestock=s->livestock;
    HmbtnPoultry *poultry=s->poultry;const HmbtnEconomy *economy=s->economy;
    const HmbtnGameProgress *progress=s->progress;
    HmbtnConstruction *construction=s->construction;HmbtnMakers *makers=s->makers;
    const HmbtnInventory *inventory=s->inventory;
    memset(s,0,sizeof(*s));s->poultry=poultry;s->economy=economy;
    s->basket_state=basket;s->random=random;s->action=action;s->makers=makers;s->progress=progress;s->construction=construction;s->livestock=livestock;s->inventory=inventory;
}
bool hmbtn_feed_shop_open(HmbtnFeedShop *s,HmbtnVillage *v,const HmbtnIntroActor *p,
    const HmbtnFarmClock *clock,unsigned area,unsigned held_item)
{
    if(s->phase!=HMBTN_SHOP_OFF || held_item || p->facing>3) return false;
    const bool barn=area==136;
    if(!barn && area!=128) return false;
    const unsigned id=barn ? 15 : 18;
    if(clock->weekday==(barn ? 1u : 0u) || clock->hour<(barn ? 9u : 11u)
            || clock->hour>=(barn ? 15u : 16u)) return false;
    const HmbtnIntroActor *merchant=&v->people[id].actor;
    if(!merchant->visible || v->people[id].location!=(barn ? 9u : 49u)) return false;
    /* farm_villager_find_box: 200 X/Z and 20 Y about the forward probe. */
    static const int step[4][2]={{0,-224},{-224,0},{0,224},{224,0}};
    if(abs(merchant->position[0]-p->position[0]-step[p->facing][0])>=200
            || abs(merchant->position[2]-p->position[2]-step[p->facing][1])>=200
            || abs(merchant->position[1]-p->position[1])>=20) return false;
    reset_shop(s);s->clock=clock;
    s->merchant=id;s->item=barn ? 0x68 : 0x69;s->quantity=1;
    s->saved_facing=merchant->facing;s->sound=3;
    start_message(s,HMBTN_SHOP_GREETING,barn ? 0x62 : 0x44);return true;
}
/* Source d3c0f38: player.c:player_act_zone_front/ZONE_SHOP_COUNTER (37),
 * farm_probe.c:farm_zone_at_probe/farm_record_find_key and shop_dialogues.c:
 * shop_key_value_lookup/shop_seed_menu_setup_a/b/c. Zone 36 is delivery, not a shop.
 * Probe the zone at facing_reach=224 (+velocity), then the estate record at
 * facing_steps=400 with strict +/-150 X/Z bounds. The three table bundles have
 * distinct record keys; select the matching supported season, as
 * shop_result_request / ready / alt and estate record gating do. */
#include "shop_keys.inc"
static unsigned shop_request_at(unsigned location,int64_t x,int64_t z)
{
    for(unsigned i=0;i<HMBTN_GATHER_RECORDS;++i) {
        const HmbtnGatherRecord *record=hmbtn_farm_gather_record(i);
        if(record->location!=location || record->position[0]<=x-150 || record->position[0]>=x+150
                || record->position[2]<=z-150 || record->position[2]>=z+150) continue;
        /* estate_items_restore replaces this placeholder with either 0x6d or
         * 0x6e; both resolve to key 9. Availability is checked by its caller. */
        const unsigned key=record->flags&0x8000u ? 0x6d : record->item;
        for(unsigned k=0;k<sizeof(shop_keys)/sizeof(*shop_keys);++k)
            if(shop_keys[k][0]==key) return shop_keys[k][1];
        return 0; /* The first matching record is the original lookup's result. */
    }
    return 0;
}
static int zone_half(const uint8_t *p)
{
    const unsigned n=hmbtn_read_le16(p);return n<32768 ? (int)n : (int)n-65536;
}
static unsigned supermarket_request(HmbtnFeedShop *s,HmbtnVillage *v,const HmbtnIntroActor *p,
    const HmbtnFarmClock *clock,unsigned area,const uint8_t *section,size_t bytes,unsigned held_item)
{
    if(!s || !v || !p || !clock || !section || bytes<0x2000 || s->phase!=HMBTN_SHOP_OFF
            || area!=180 || held_item || p->facing>3 || clock->hour<9 || clock->hour>=17
            || clock->weekday==0 || clock->weekday==2 || !v->people[1].actor.visible || v->people[1].location!=30) return false;
    static const int direction[4][2]={{0,-1},{-1,0},{0,1},{1,0}};
    const int64_t x=(int64_t)p->position[0]+p->velocity[0]+direction[p->facing][0]*224,
        z=(int64_t)p->position[2]+p->velocity[2]+direction[p->facing][1]*224;
    for(unsigned i=0;i<50;++i) {
        const uint8_t *zone=section+16+i*44;
        if(!(hmbtn_read_le16(zone)&0x4000u)) continue;
        if(x<=zone_half(zone+14) || x>=zone_half(zone+20) || z<=zone_half(zone+18) || z>=zone_half(zone+24)
                || p->position[1]<=zone_half(zone+16)-128 || p->position[1]>=zone_half(zone+22)+120) continue;
        if(hmbtn_read_le16(zone+2)!=37) return false;
        /* As in the original case, zone.facing/param do not select the shop.
         * The long facing probe selects the record, then the key's dialogue. */
        const unsigned request=shop_request_at(hmbtn_read_le16(section+2),
            (int64_t)p->position[0]+direction[p->facing][0]*400,
            (int64_t)p->position[2]+direction[p->facing][1]*400);
        return request;
    }
    return 0;
}
bool hmbtn_seed_shop_open(HmbtnFeedShop *s,HmbtnVillage *v,const HmbtnIntroActor *p,
    const HmbtnFarmClock *clock,unsigned area,const uint8_t *section,size_t bytes,unsigned held_item)
{
    if(!clock || !hmbtn_seed_shop_entries(clock->season)
            || supermarket_request(s,v,p,clock,area,section,bytes,held_item)!=11+clock->season) return false;
    reset_shop(s);
    s->seeds=true;s->merchant=1;s->clock=clock;s->item=hmbtn_seed_shop_entry(clock->season,0)->item;s->quantity=1;
    s->saved_facing=v->people[1].actor.facing;s->sound=3;
    start_message(s,HMBTN_SHOP_GREETING,clock->season==0 ? 0xBE : clock->season==1 ? 0x273 : 0x274);return true;
}
bool hmbtn_rucksack_shop_open(HmbtnFeedShop *s,HmbtnVillage *v,const HmbtnIntroActor *p,
    const HmbtnFarmClock *clock,unsigned area,const uint8_t *section,size_t bytes,const HmbtnInventory *h)
{
    const unsigned offer=hmbtn_inventory_rucksack_offer(h);
    if(!offer || supermarket_request(s,v,p,clock,area,section,bytes,h->held_item.id)!=9) return false;
    reset_shop(s);s->rucksack=true;s->merchant=1;s->clock=clock;s->inventory=h;s->item=offer;s->quantity=1;
    start_message(s,HMBTN_SHOP_RUCKSACK_CUE,0);return true;
}
bool hmbtn_basket_shop_open(HmbtnFeedShop *s,HmbtnVillage *v,const HmbtnIntroActor *p,
    const HmbtnFarmClock *clock,unsigned area,const uint8_t *section,size_t bytes,const HmbtnInventory *h)
{
    if(!s || !s->basket_state || s->basket_state->owned || !h
            || supermarket_request(s,v,p,clock,area,section,bytes,h->held_item.id)!=10) return false;
    reset_shop(s);s->basket=true;s->merchant=1;s->clock=clock;s->inventory=h;s->item=100;s->quantity=1;
    start_message(s,HMBTN_SHOP_RUCKSACK_CUE,0);return true;
}
static unsigned stock_description(const HmbtnFeedShop *s) { return s->basket ? 0xd5 : s->item==0x6d ? 0xd4 : 0x248; }
/* shop_item_grant mode=RUCKSACK: show the stock before asking, pay only after
 * the acceptance text closes, then use the base equip ANI. Ten daily ticks
 * gate the next offer; capacity is owned exclusively by the inventory. */
static void stock_tick(HmbtnFeedShop *s,HmbtnHouseInteraction *h,HmbtnEconomy *e,uint32_t pressed,uint32_t held)
{
    if(s->loaded!=s->request) return;
    if(s->phase==HMBTN_SHOP_RUCKSACK_CUE) {
        if(s->cue_started && h->inventory->held_item.id==s->item)
            start_message(s,HMBTN_SHOP_OFFER,stock_description(s));
        return;
    }
    if(s->phase==HMBTN_SHOP_RUCKSACK_UPGRADE) {
        if(s->cue_started && !h->inventory->held_item.id) { s->phase=HMBTN_SHOP_OFF;s->request=0; }
        return;
    }
    const bool confirm=s->phase==HMBTN_SHOP_MAKER_CONFIRM;
    const bool ready=s->message.phase==HMBTN_MESSAGE_WAIT_END;
    s->sound=hmbtn_message_tick(&s->message,(confirm || (s->phase==HMBTN_SHOP_OFFER && ready)) && !s->closing ? 0 : pressed,held);
    if(s->closing) {
        if(s->message.phase==HMBTN_MESSAGE_CLOSED) start_message(s,s->next,s->next_request);
        return;
    }
    if(confirm && ready) {
        if(pressed&(HMBTN_PAD_UP|HMBTN_PAD_DOWN)) { s->answer^=1;s->sound=5; }
        if((pressed&HMBTN_PAD_CIRCLE) || ((pressed&HMBTN_PAD_CROSS) && s->answer)) next(s,HMBTN_SHOP_RESULT,0xba);
        else if(pressed&HMBTN_PAD_CROSS) next(s,HMBTN_SHOP_RESULT,e->gold<hmbtn_feed_shop_cost(s) ? 0x4f : 0xbb);
        return;
    }
    /* The original accepts Cross or Circle after the description, before
     * opening Yes/No. Neither key at this step is itself a purchase. */
    if(s->phase==HMBTN_SHOP_OFFER && ready && (pressed&(HMBTN_PAD_CROSS|HMBTN_PAD_CIRCLE))) {
        s->answer=0;s->phase=HMBTN_SHOP_MAKER_CONFIRM;s->message.suppress_cursor=true;return;
    }
    if(s->phase!=HMBTN_SHOP_RESULT || s->message.phase!=HMBTN_MESSAGE_CLOSED) return;
    if(s->basket && s->request==0xbb && hmbtn_basket_buy(s->basket_state,h->inventory,e)) {
        s->phase=HMBTN_SHOP_OFF;s->request=0;
    } else if(!s->basket && s->request==0xbb && hmbtn_inventory_buy_rucksack(h->inventory,e,s->item)) {
        s->cue_started=false;start_message(s,HMBTN_SHOP_RUCKSACK_UPGRADE,0);
    } else {
        /* A delayed message must not authorize stale stock or a changed purse. */
        if(s->request==0xbb && e->gold<hmbtn_feed_shop_cost(s)) { start_message(s,HMBTN_SHOP_RESULT,0x4f);return; }
        if(h->inventory->held_item.id==s->item) h->inventory->held_item=(HmbtnStoredTool){0};
        s->phase=HMBTN_SHOP_OFF;s->request=0;
    }
}
bool hmbtn_won_seed_shop_open(HmbtnFeedShop *s,const HmbtnFarmClock *clock)
{
    if(!s || !clock || s->phase!=HMBTN_SHOP_OFF) return false;
    reset_shop(s);
    s->seeds=true;s->merchant=35;s->clock=clock;s->quantity=1;
    s->item=won_seeds[0].item;s->sound=3;start_message(s,HMBTN_SHOP_GREETING,0x164);return true;
}
/* shop_tool_upgrade_menu_setup / shop_stock_step, reference fcef049.
 * Reuse the shared modal for instructions, random gifts, makers and tools.
 * Held ore enters the original upgrade branch. */
unsigned hmbtn_blacksmith_tool_count(const HmbtnFeedShop *s)
{
    unsigned count=0;
    if(s && s->inventory) for(unsigned i=0;i<sizeof(animal_tools)/sizeof(*animal_tools);++i)
        count+=hmbtn_inventory_count(s->inventory,(int)animal_tools[i].item,0,2)==0;
    return count;
}
const HmbtnSeedShopEntry *hmbtn_blacksmith_tool_entry(const HmbtnFeedShop *s,unsigned row)
{
    if(s && s->inventory) for(unsigned i=0;i<sizeof(animal_tools)/sizeof(*animal_tools);++i)
        if(hmbtn_inventory_count(s->inventory,(int)animal_tools[i].item,0,2)==0) {
            if(!row) return &animal_tools[i];
            --row;
        }
    return NULL;
}
/* Stable native selections: 0 improve, 1 buy tools, 2..4 makers, 5 gift.
 * Visible order follows shop_tool_upgrade_menu_setup. */
unsigned hmbtn_blacksmith_rows(const HmbtnFeedShop *s)
{ return 3u+(s && hmbtn_makers_offer(s->makers,s->construction,s->poultry)!=0); }
unsigned hmbtn_blacksmith_row(const HmbtnFeedShop *s,unsigned row)
{
    const unsigned offer=s ? hmbtn_makers_offer(s->makers,s->construction,s->poultry) : 0;
    if(!row) return 0;
    if(row==1) return 5;
    if(offer && row==2) return offer-0x13f+2;
    return 1;
}
static bool maker_ready(const HmbtnFeedShop *s,const HmbtnInventory *h,const HmbtnEconomy *e)
{
    if(hmbtn_saibara_accessory(s->item)) return hmbtn_saibara_accessory_ready(s->makers,h,e,s->item);
    const HmbtnFarmItem *item=hmbtn_farm_item(s->item);
    return s->item && s->item==hmbtn_makers_offer(s->makers,s->construction,s->poultry)
        && item && item->buy_price>0 && e->gold>=(unsigned)item->buy_price
        && !h->held_item.id && hmbtn_inventory_count(h,0x62,1,1)>0;
}
bool hmbtn_blacksmith_shop_open(HmbtnFeedShop *s,const HmbtnFarmClock *clock,const HmbtnInventory *inventory)
{
    if(!s || !clock || !inventory || s->phase!=HMBTN_SHOP_OFF || !s->random || (s->makers && s->makers->pending)) return false;
    reset_shop(s);s->blacksmith=true;s->merchant=4;s->clock=clock;s->inventory=inventory;
    s->gift=saibara_gifts[hmbtn_psx_random_range(s->random,sizeof(saibara_gifts)/sizeof(*saibara_gifts))];
    s->sound=3;s->quantity=1;start_message(s,HMBTN_SHOP_GREETING,0x85);return true;
}
static unsigned upgrade_message(unsigned reason)
{
    static const unsigned messages[]={0,0x96,0x8d,0x97,0x98,0x99};
    return reason<6 ? messages[reason] : 0x96;
}
bool hmbtn_blacksmith_upgrade_open(HmbtnFeedShop *s,const HmbtnFarmClock *clock,const HmbtnInventory *h)
{
    if(!s || !clock || !h || !s->action || !s->makers || s->makers->pending || s->phase!=HMBTN_SHOP_OFF) return false;
    reset_shop(s);s->blacksmith=s->upgrade=true;s->merchant=4;s->clock=clock;s->inventory=h;s->sound=3;
    const unsigned reason=hmbtn_tool_upgrade_reason(h,s->action->experience);
    if(reason || !hmbtn_tool_ore_level(h->held_item.id)) start_message(s,HMBTN_SHOP_RESULT,upgrade_message(reason ? reason : 1));
    else { s->answer=0;start_message(s,HMBTN_SHOP_MAKER_CONFIRM,0x99+hmbtn_tool_ore_level(h->held_item.id)); }
    return true;
}
static void blacksmith_tick(HmbtnFeedShop *s,HmbtnHouseInteraction *h,HmbtnEconomy *economy,uint32_t pressed,uint32_t held)
{
    if(s->loaded!=s->request) return;
    if(s->phase==HMBTN_SHOP_MAKER_CUE) {
        if(s->cue_started && h->inventory->held_item.id==hmbtn_saibara_material(s->item))
            start_message(s,HMBTN_SHOP_RESULT,0x9f);
        return;
    }
    const bool confirm=s->phase==HMBTN_SHOP_MAKER_CONFIRM || s->phase==HMBTN_SHOP_MAKER_AGAIN;
    const bool input=s->phase==HMBTN_SHOP_SELECT || s->phase==HMBTN_SHOP_TOOL_LIST || confirm;
    const bool ready=s->message.phase==HMBTN_MESSAGE_WAIT_END;
    s->sound=hmbtn_message_tick(&s->message,input && !s->closing ? 0 : pressed,held);
    if(s->closing) {
        if(s->message.phase==HMBTN_MESSAGE_CLOSED) start_message(s,s->next,s->next_request);
        return;
    }
    if(!input) {
        if(s->message.phase!=HMBTN_MESSAGE_CLOSED) return;
        if(s->phase==HMBTN_SHOP_LEAVE) { s->phase=HMBTN_SHOP_OFF;s->request=0;return; }
        if(s->phase==HMBTN_SHOP_GREETING) { start_message(s,HMBTN_SHOP_SELECT,0x86);return; }
        if(s->phase==HMBTN_SHOP_RESULT && s->request==0x9f) {
            h->inventory->held_item=(HmbtnStoredTool){0};s->departure=true;
            s->phase=HMBTN_SHOP_OFF;s->request=0;return;
        }
        if(s->upgrade) { start_message(s,HMBTN_SHOP_LEAVE,0x87);return; }
        if(s->phase==HMBTN_SHOP_RESULT && s->request==0xae) {
            /* Pay/store after the acceptance text, just once. No hidden spare
             * slot fallback: original shop_item_room(item,1)/add(mode=1). */
            if(!hmbtn_inventory_can_store_tool(h->inventory,false)) { s->after_result=HMBTN_SHOP_TOOL_LIST;start_message(s,HMBTN_SHOP_RESULT,0x54);return; }
            const unsigned price=hmbtn_feed_shop_cost(s);
            if(economy->gold<price) { s->after_result=HMBTN_SHOP_TOOL_LIST;start_message(s,HMBTN_SHOP_RESULT,0x4f);return; }
            if(hmbtn_inventory_count(h->inventory,(int)s->item,0,2)==0
                    && hmbtn_inventory_store_tool(h->inventory,(HmbtnStoredTool){(uint16_t)s->item,0,1},false,NULL))
                hmbtn_economy_gold_change(economy,-(int64_t)price,true);
            s->gift=saibara_gifts[hmbtn_psx_random_range(s->random,sizeof(saibara_gifts)/sizeof(*saibara_gifts))];
            s->selection=1;s->after_result=HMBTN_SHOP_SELECT;
        }
        start_message(s,s->after_result,s->after_result==HMBTN_SHOP_TOOL_LIST ? 0x91 : 0xb5);return;
    }
    if(!ready) return;
    if(confirm) {
        if(pressed&(HMBTN_PAD_UP|HMBTN_PAD_DOWN)) { s->answer^=1u;s->sound=5; }
        if((pressed&HMBTN_PAD_CIRCLE) || ((pressed&HMBTN_PAD_CROSS) && s->answer)) {
            s->after_result=HMBTN_SHOP_SELECT;next(s,HMBTN_SHOP_RESULT,0x93);
        } else if(pressed&HMBTN_PAD_CROSS) {
            if(s->upgrade) {
                const unsigned reason=hmbtn_tool_upgrade_reason(h->inventory,s->action->experience);
                const unsigned level=hmbtn_tool_ore_level(h->inventory->held_item.id);
                if(reason || !level) next(s,HMBTN_SHOP_RESULT,upgrade_message(reason ? reason : 1));
                else if(economy->gold<hmbtn_tool_upgrade_price(level)) next(s,HMBTN_SHOP_RESULT,0x4f);
                else if(s->phase==HMBTN_SHOP_MAKER_CONFIRM) { s->answer=0;next(s,HMBTN_SHOP_MAKER_AGAIN,0x9e); }
                else if(hmbtn_tool_upgrade_order(s->makers,h->inventory,economy,s->action->experience))
                    next(s,HMBTN_SHOP_RESULT,0x9f);
                return;
            }
            if(!maker_ready(s,h->inventory,economy)) {
                s->after_result=HMBTN_SHOP_SELECT;next(s,HMBTN_SHOP_RESULT,hmbtn_saibara_accessory(s->item) ? 0xa3 : 0xa4);
            } else if(s->phase==HMBTN_SHOP_MAKER_CONFIRM) {
                s->answer=0;next(s,HMBTN_SHOP_MAKER_AGAIN,hmbtn_saibara_accessory(s->item) ? 0x9e : 0xa5);
            } else if(hmbtn_saibara_accessory(s->item)
                    ? hmbtn_saibara_accessory_order(s->makers,h->inventory,economy,s->item)
                    : hmbtn_makers_order(s->makers,s->construction,s->poultry,h->inventory,economy,s->item)) {
                s->cue_started=false;next(s,HMBTN_SHOP_MAKER_CUE,0);
            }
        }
        return;
    }
    s->after_result=s->phase;
    if(s->phase==HMBTN_SHOP_SELECT) {
        if(pressed&(HMBTN_PAD_UP|HMBTN_PAD_DOWN)) {
            const unsigned rows=hmbtn_blacksmith_rows(s);unsigned row=0;
            while(row<rows && hmbtn_blacksmith_row(s,row)!=s->selection) ++row;
            row=(row+(pressed&HMBTN_PAD_UP ? rows-1 : 1))%rows;
            s->selection=hmbtn_blacksmith_row(s,row);s->sound=2;
        }
        if(pressed&HMBTN_PAD_CIRCLE) { next(s,HMBTN_SHOP_LEAVE,0x91);return; }
        if(pressed&HMBTN_PAD_TRIANGLE) { next(s,HMBTN_SHOP_INFO,s->selection==5 ? 0x88 : s->selection>=2 ? 0x89+s->selection-2 : s->selection ? 0x8c : 0x87);return; }
        if(pressed&HMBTN_PAD_CROSS) {
            if(!s->selection) { next(s,HMBTN_SHOP_INFO,0x87);return; }
            if(s->selection>=2) {
                s->item=s->selection==5 ? s->gift : 0x13f+s->selection-2;
                if(!maker_ready(s,h->inventory,economy)) next(s,HMBTN_SHOP_RESULT,s->selection==5 ? 0xa3 : 0xa4);
                else { s->answer=0;next(s,HMBTN_SHOP_MAKER_CONFIRM,s->selection==5 ? 0x1e1 : 0x1e2); }
                return;
            }
            if(!hmbtn_blacksmith_tool_count(s)) { next(s,HMBTN_SHOP_RESULT,0xad);return; }
            s->sale_cursor=0;s->item=hmbtn_blacksmith_tool_entry(s,0)->item;
            next(s,HMBTN_SHOP_TOOL_LIST,0x91);
        }
        return;
    }
    const unsigned count=hmbtn_blacksmith_tool_count(s);
    if(!count) { s->after_result=HMBTN_SHOP_SELECT;next(s,HMBTN_SHOP_RESULT,0xad);return; }
    if(pressed&(HMBTN_PAD_UP|HMBTN_PAD_DOWN)) {
        s->sale_cursor=(s->sale_cursor+(pressed&HMBTN_PAD_UP ? count-1 : 1))%count;s->sound=2;
    }
    const HmbtnSeedShopEntry *entry=hmbtn_blacksmith_tool_entry(s,s->sale_cursor);
    if(!entry) { s->sale_cursor=0;entry=hmbtn_blacksmith_tool_entry(s,0); }
    s->item=entry->item;
    if(pressed&HMBTN_PAD_CIRCLE) { s->after_result=HMBTN_SHOP_SELECT;next(s,HMBTN_SHOP_INFO,0x91); }
    else if(pressed&HMBTN_PAD_TRIANGLE) next(s,HMBTN_SHOP_INFO,entry->description);
    else if(pressed&HMBTN_PAD_CROSS) {
        if(!hmbtn_inventory_can_store_tool(h->inventory,false)) next(s,HMBTN_SHOP_RESULT,0x54);
        else if(economy->gold<hmbtn_feed_shop_cost(s)) next(s,HMBTN_SHOP_RESULT,0x4f);
        else { s->after_result=HMBTN_SHOP_SELECT;next(s,HMBTN_SHOP_RESULT,0xae); }
    }
}
/* shop_result_request_std vs shop_result_request[_ready/_alt]. */
enum SeedMessage { SEED_OFFER,SEED_STORED,SEED_LEAVE,SEED_CANCEL,SEED_CANCELLED,SEED_BOX };
static unsigned seed_message(const HmbtnFeedShop *s,unsigned message)
{
    static const unsigned ids[2][6]={{0xC0,0xBB,0xBF,0xBA,0xC2,0x207},
        {0x16E,0x170,0x16D,0x16F,0x171,0x261}};
    return ids[s->merchant==35][message];
}
/* Adapted shop_menu_open_choose/shop_menu_choose_next/shop_buy_stored_item.
 * Retain original message bank/ids and transitions, without global PS1 menu state.
 * The ordinary feed/animal shops below keep their existing behavior. */
static void seed_tick(HmbtnFeedShop *s,HmbtnHouseInteraction *h,HmbtnEconomy *economy,
    uint32_t pressed,uint32_t held)
{
    if(s->phase==HMBTN_SHOP_OFF || s->loaded!=s->request || !s->clock) return;
    const unsigned rows=hmbtn_seed_shop_count(s);
    const HmbtnSeedShopEntry *entry=hmbtn_seed_shop_choice(s,s->selection);
    if(!rows || !entry) return;
    const bool input=s->phase==HMBTN_SHOP_SELECT || s->phase==HMBTN_SHOP_COUNT;
    const bool ready=s->message.phase==HMBTN_MESSAGE_WAIT_END;
    s->sound=hmbtn_message_tick(&s->message,input && !s->closing ? 0 : pressed,held);
    if(s->closing) {
        if(s->message.phase==HMBTN_MESSAGE_CLOSED) {
            const unsigned bank=s->next_bank;
            start_message(s,s->next,s->next_request);s->bank=bank;
        }
        return;
    }
    if(!input) {
        if(s->message.phase!=HMBTN_MESSAGE_CLOSED) return;
        switch(s->phase) {
        case HMBTN_SHOP_GREETING:start_message(s,HMBTN_SHOP_SELECT,0x46);break;
        case HMBTN_SHOP_INFO:
            start_message(s,s->after_result,s->after_result==HMBTN_SHOP_SELECT ? 0x46 : 0x4D);break;
        case HMBTN_SHOP_OFFER:start_message(s,HMBTN_SHOP_COUNT,0x4D);break;
        case HMBTN_SHOP_CANCEL_COUNT:
            s->after_result=HMBTN_SHOP_SELECT;start_message(s,HMBTN_SHOP_RESULT,seed_message(s,SEED_CANCELLED));break;
        case HMBTN_SHOP_RESULT:
            if(s->bank==3 && s->request==seed_message(s,SEED_BOX)) start_message(s,HMBTN_SHOP_RESULT,seed_message(s,SEED_STORED));
            else if(s->after_result==HMBTN_SHOP_OFF) { s->phase=HMBTN_SHOP_OFF;s->request=0; }
            else start_message(s,s->after_result,s->after_result==HMBTN_SHOP_SELECT ? 0x46 : 0x4D);
            break;
        case HMBTN_SHOP_LEAVE:s->phase=HMBTN_SHOP_OFF;s->request=0;break;
        default:break;
        }
        return;
    }
    if(!ready) return; /* A key revealing text must never also select/buy. */
    const uint32_t arrows=held&(HMBTN_PAD_UP|HMBTN_PAD_DOWN|HMBTN_PAD_LEFT|HMBTN_PAD_RIGHT);
    uint32_t action=pressed;
    if(arrows!=s->previous_arrows) { s->repeat=0;action|=arrows;s->previous_arrows=arrows; }
    else if(arrows && ++s->repeat>=12 && (s->repeat-12)%3==0) action|=arrows;
    if(s->phase==HMBTN_SHOP_SELECT) {
        if(action&HMBTN_PAD_UP) { s->selection=(s->selection+rows-1)%rows;s->sound=5; }
        else if(action&HMBTN_PAD_DOWN) { s->selection=(s->selection+1)%rows;s->sound=5; }
        else if(pressed&HMBTN_PAD_CROSS) {
            s->sound=0;
            if(!hmbtn_inventory_seed_room(h->inventory,s->item,false)) {
                s->after_result=HMBTN_SHOP_OFF;next(s,HMBTN_SHOP_RESULT,0x54);
            } else { s->quantity=1;next(s,HMBTN_SHOP_OFFER,seed_message(s,SEED_OFFER)); }
        } else if(pressed&HMBTN_PAD_TRIANGLE) {
            s->after_result=HMBTN_SHOP_SELECT;s->sound=0;
            next(s,HMBTN_SHOP_INFO,entry->description);
        } else if(pressed&HMBTN_PAD_CIRCLE) { s->sound=1;next(s,HMBTN_SHOP_LEAVE,seed_message(s,SEED_LEAVE)); }
        s->item=hmbtn_seed_shop_choice(s,s->selection)->item;
        return;
    }
    int quantity=(int)s->quantity;
    if(action&HMBTN_PAD_RIGHT) ++quantity;
    else if(action&HMBTN_PAD_LEFT) --quantity;
    else if(action&HMBTN_PAD_UP) quantity+=10;
    else if(action&HMBTN_PAD_DOWN) quantity-=10;
    else if(pressed&HMBTN_PAD_TRIANGLE) {
        s->after_result=HMBTN_SHOP_COUNT;s->sound=0;
        next(s,HMBTN_SHOP_INFO,entry->description);return;
    } else if(pressed&HMBTN_PAD_CIRCLE) {
        s->sound=1;next(s,HMBTN_SHOP_CANCEL_COUNT,seed_message(s,SEED_CANCEL));return;
    } else if(pressed&HMBTN_PAD_CROSS) {
        const unsigned cost=hmbtn_feed_shop_cost(s);
        s->after_result=HMBTN_SHOP_COUNT;s->sound=0;
        if(economy->gold<cost) next(s,HMBTN_SHOP_RESULT,0x4F);
        else if(hmbtn_inventory_seed_room(h->inventory,s->item,false)<s->quantity)
            next(s,HMBTN_SHOP_RESULT,0xC1);
        else {
            const bool stored=hmbtn_inventory_seed_room(h->inventory,s->item,true)<s->quantity;
            /* Both validations precede commitment. No effects run during text
             * loading/waiting, and all consumers see the authoritative inventory. */
            if(!hmbtn_inventory_add_seeds(h->inventory,s->item,s->quantity,NULL)) {
                next(s,HMBTN_SHOP_RESULT,0xC1);return;
            }
            hmbtn_economy_gold_change(economy,-(int64_t)cost,false);
            s->after_result=HMBTN_SHOP_SELECT;
            next(s,HMBTN_SHOP_RESULT,seed_message(s,stored ? SEED_BOX : SEED_STORED));
            if(stored) s->next_bank=3;
        }
        return;
    }
    if(quantity!=(int)s->quantity) { s->quantity=quantity>99 ? 1 : quantity<=0 ? 99 : (unsigned)quantity;s->sound=5; }
}
bool hmbtn_carpenter_shop_open(HmbtnFeedShop *s,const HmbtnFarmClock *clock)
{
    if(!s || !clock || !s->construction || !s->poultry || s->phase!=HMBTN_SHOP_OFF) return false;
    reset_shop(s);s->carpenter=true;s->merchant=24;s->clock=clock;
    s->item=hmbtn_carpenter_lumber_item();s->quantity=1;s->sound=3;
    start_message(s,HMBTN_SHOP_GREETING,0x132);return true;
}
unsigned hmbtn_carpenter_shop_rows(const HmbtnFeedShop *s)
{
    return s && s->poultry && s->construction ? (hmbtn_construction_next(s->construction,s->poultry)<5 ? 2u : 1u) : 0;
}
/* shop_lumber_deal_step/shop_menu_choose, using the existing modal shop.
 * No money, materials or building levels live in the presentation state. */
static void carpenter_tick(HmbtnFeedShop *s,HmbtnHouseInteraction *h,HmbtnEconomy *economy,
    uint32_t pressed,uint32_t held)
{
    if(s->phase==HMBTN_SHOP_OFF || s->loaded!=s->request) return;
    const bool input=s->phase==HMBTN_SHOP_SELECT || s->phase==HMBTN_SHOP_COUNT
        || s->phase==HMBTN_SHOP_BUILD_CONFIRM;
    const bool ready=s->message.phase==HMBTN_MESSAGE_WAIT_END;
    s->sound=hmbtn_message_tick(&s->message,input && !s->closing ? 0 : pressed,held);
    if(s->closing) {
        if(s->message.phase==HMBTN_MESSAGE_CLOSED) start_message(s,s->next,s->next_request);
        return;
    }
    if(!input) {
        if(s->message.phase!=HMBTN_MESSAGE_CLOSED) return;
        switch(s->phase) {
        case HMBTN_SHOP_GREETING:start_message(s,HMBTN_SHOP_SELECT,0x139);break;
        case HMBTN_SHOP_OFFER:start_message(s,HMBTN_SHOP_COUNT,0x4D);break;
        case HMBTN_SHOP_INFO:case HMBTN_SHOP_RESULT:
            if(s->after_result==HMBTN_SHOP_OFF) { s->phase=HMBTN_SHOP_OFF;s->request=0; }
            else start_message(s,s->after_result,s->after_result==HMBTN_SHOP_SELECT ? 0x139
                : s->after_result==HMBTN_SHOP_BUILD_CONFIRM ? 0x5F : 0x4D);
            break;
        case HMBTN_SHOP_LEAVE:s->phase=HMBTN_SHOP_OFF;s->request=0;break;
        default:break;
        }
        return;
    }
    if(!ready) return;
    const uint32_t arrows=held&(HMBTN_PAD_UP|HMBTN_PAD_DOWN|HMBTN_PAD_LEFT|HMBTN_PAD_RIGHT);
    uint32_t action=pressed;
    if(arrows!=s->previous_arrows) { s->repeat=0;action|=arrows;s->previous_arrows=arrows; }
    else if(arrows && ++s->repeat>=12 && (s->repeat-12)%3==0) action|=arrows;
    if(s->phase==HMBTN_SHOP_SELECT) {
        const unsigned rows=hmbtn_carpenter_shop_rows(s);
        if(action&(HMBTN_PAD_UP|HMBTN_PAD_DOWN)) { s->selection=(s->selection+1)%rows;s->sound=5; }
        else if(pressed&HMBTN_PAD_TRIANGLE) {
            s->after_result=HMBTN_SHOP_SELECT;s->sound=0;next(s,HMBTN_SHOP_INFO,s->selection ? 0x134+hmbtn_construction_next(s->construction,s->poultry) : 0x133);
        } else if(pressed&HMBTN_PAD_CIRCLE) { s->sound=1;next(s,HMBTN_SHOP_LEAVE,0x141); }
        else if(pressed&HMBTN_PAD_CROSS) {
            s->sound=0;s->after_result=HMBTN_SHOP_SELECT;
            if(!s->selection) {
                if(h->inventory->supplies[1]>=hmbtn_carpenter_lumber_limit()) next(s,HMBTN_SHOP_RESULT,0x13A);
                else { s->quantity=1;next(s,HMBTN_SHOP_OFFER,0x13B); }
            } else if(s->construction->mornings_left) next(s,HMBTN_SHOP_RESULT,0x144);
            else {
                s->answer=0;s->after_result=HMBTN_SHOP_BUILD_CONFIRM;next(s,HMBTN_SHOP_INFO,0x13C+hmbtn_construction_next(s->construction,s->poultry));
            }
        }
        return;
    }
    if(s->phase==HMBTN_SHOP_BUILD_CONFIRM) {
        if(action&(HMBTN_PAD_UP|HMBTN_PAD_DOWN)) { s->answer^=1;s->sound=5; }
        else if(pressed&(HMBTN_PAD_CROSS|HMBTN_PAD_CIRCLE)) {
            s->sound=0;s->after_result=HMBTN_SHOP_SELECT;
            if(s->answer || (pressed&HMBTN_PAD_CIRCLE)) next(s,HMBTN_SHOP_RESULT,0x141);
            else {
                const HmbtnConstructionResult result=hmbtn_construction_order(s->construction,s->poultry,h->inventory,economy);
                if(result==HMBTN_BUILD_OK) s->after_result=HMBTN_SHOP_OFF;
                next(s,HMBTN_SHOP_RESULT,result==HMBTN_BUILD_GOLD ? 0x4F
                    : result==HMBTN_BUILD_LUMBER ? 0x145 : result==HMBTN_BUILD_COMPLETE ? 0x146 : 0x144);
            }
        }
        return;
    }
    int quantity=(int)s->quantity;
    if(action&HMBTN_PAD_RIGHT) ++quantity;
    else if(action&HMBTN_PAD_LEFT) --quantity;
    else if(action&HMBTN_PAD_UP) quantity+=10;
    else if(action&HMBTN_PAD_DOWN) quantity-=10;
    else if(pressed&HMBTN_PAD_TRIANGLE) {
        s->sound=0;s->after_result=HMBTN_SHOP_COUNT;next(s,HMBTN_SHOP_INFO,0x133);return;
    } else if(pressed&HMBTN_PAD_CIRCLE) {
        s->sound=1;s->after_result=HMBTN_SHOP_SELECT;next(s,HMBTN_SHOP_RESULT,0x141);return;
    } else if(pressed&HMBTN_PAD_CROSS) {
        s->sound=0;s->after_result=HMBTN_SHOP_COUNT;
        const unsigned cost=hmbtn_feed_shop_cost(s),wood=h->inventory->supplies[1],limit=hmbtn_carpenter_lumber_limit();
        if(economy->gold<cost) next(s,HMBTN_SHOP_RESULT,0x4F);
        else if(wood>limit || s->quantity>limit-wood) next(s,HMBTN_SHOP_RESULT,0x142);
        else {
            h->inventory->supplies[1]+=s->quantity;
            hmbtn_economy_gold_change(economy,-(int64_t)cost,true);
            s->after_result=HMBTN_SHOP_OFF;next(s,HMBTN_SHOP_RESULT,0x143);
        }
        return;
    }
    if(quantity!=(int)s->quantity) { s->quantity=quantity>99 ? 1 : quantity<=0 ? 99 : (unsigned)quantity;s->sound=5; }
}
void hmbtn_feed_shop_tick(HmbtnFeedShop *s,HmbtnHouseInteraction *h,HmbtnEconomy *economy,HmbtnFarmStatistics *stats,
    uint32_t pressed,uint32_t held)
{
    s->sound=-1;
    if(s->rucksack || s->basket) { stock_tick(s,h,economy,pressed,held);return; }
    if(s->blacksmith) { blacksmith_tick(s,h,economy,pressed,held);return; }
    if(s->carpenter) { carpenter_tick(s,h,economy,pressed,held);return; }
    if(s->seeds) { seed_tick(s,h,economy,pressed,held);return; }
    const bool barn=s->merchant==15;
    s->medicine_available=!has_medicine(h);
    s->bell_available=barn && s->livestock && hmbtn_inventory_count(h->inventory,0x1d,0,2)==0;
    if(barn && s->selection==10 && !s->bell_available) { s->selection=0;s->item=0x68;s->quantity=1; }
    for(unsigned i=0;i<2;++i) s->potion_available[i]=barn && hmbtn_inventory_count(h->inventory,0x1f+(int)i,0,2)==0;
    if(barn && (s->selection==8 || s->selection==9) && !s->potion_available[s->selection-8]) {
        s->selection=0;s->item=0x68;s->quantity=1;
    }
    if(!s->medicine_available && s->selection==3) {
        s->selection=0;s->item=s->merchant==15 ? 0x68 : 0x69;s->quantity=1;
    }
    if(s->phase==HMBTN_SHOP_NAME) {
        hmbtn_new_game_tick(&s->editor,pressed,held);s->sound=s->editor.sound;
        if(!s->editor.finished || s->editor.message.phase!=HMBTN_MESSAGE_CLOSED) return;
        s->after_result=HMBTN_SHOP_SELECT;
        if(barn) {
            if(s->editor.choice) start_message(s,HMBTN_SHOP_RESULT,0x6E);
            else if(hmbtn_livestock_free(s->livestock)==HMBTN_LIVESTOCK) start_message(s,HMBTN_SHOP_RESULT,0x72);
            else if(economy->gold<hmbtn_feed_shop_cost(s)) start_message(s,HMBTN_SHOP_RESULT,0x4F);
            else if(hmbtn_livestock_buy(s->livestock,s->item,s->editor.name,s->clock)) {
                hmbtn_economy_gold_change(economy,-(int64_t)hmbtn_feed_shop_cost(s),false);
                start_message(s,HMBTN_SHOP_RESULT,0x74);
            }
            return;
        }
        if(s->editor.choice) start_message(s,HMBTN_SHOP_RESULT,0x4E);
        else if(!hmbtn_poultry_room(s->poultry)) start_message(s,HMBTN_SHOP_RESULT,0x55);
        else if(economy->gold<hmbtn_feed_shop_cost(s)) start_message(s,HMBTN_SHOP_RESULT,0x4F);
        else if(hmbtn_poultry_buy(s->poultry,s->editor.name,s->clock)) {
            const unsigned cost=hmbtn_feed_shop_cost(s);
            hmbtn_economy_gold_change(economy,-(int64_t)cost,false);
            stats->animals[0]=hmbtn_poultry_count(s->poultry);
            start_message(s,HMBTN_SHOP_RESULT,0x57);
        }
        return;
    }
    if(s->phase==HMBTN_SHOP_OFF || s->loaded!=s->request) return;
    const bool input=s->phase==HMBTN_SHOP_SELECT || s->phase==HMBTN_SHOP_COUNT
        || s->phase==HMBTN_SHOP_SELL_LIST || s->phase==HMBTN_SHOP_SELL_CONFIRM;
    const bool ready=s->message.phase==HMBTN_MESSAGE_WAIT_END;
    s->sound=hmbtn_message_tick(&s->message,input ? 0 : pressed,held);
    if(s->closing) {
        if(s->message.phase==HMBTN_MESSAGE_CLOSED) start_message(s,s->next,s->next_request);
        return;
    }
    if(!input) {
        if(s->message.phase!=HMBTN_MESSAGE_CLOSED) return;
        switch(s->phase) {
        /* shop_menu_open_choose always prompts with bank 2 / 0x46.
         * request.second (Barley's 0x6B) is the cancellation message. */
        case HMBTN_SHOP_GREETING:case HMBTN_SHOP_INFO:start_message(s,HMBTN_SHOP_SELECT,0x46);break;
        case HMBTN_SHOP_OFFER:
            if(s->selection==1 || (barn && (s->selection==4 || s->selection==5))) { s->phase=HMBTN_SHOP_NAME;hmbtn_name_editor_begin(&s->editor); }
            else if(barn && (s->selection==6 || s->selection==7)) start_message(s,HMBTN_SHOP_SELL_LIST,0x5A);
            else if(s->selection==2) start_message(s,HMBTN_SHOP_SELL_LIST,0x5A);
            else start_message(s,HMBTN_SHOP_COUNT,0x4D);
            break;
        case HMBTN_SHOP_RESULT:start_message(s,s->after_result,s->after_result==HMBTN_SHOP_SELECT ? 0x46 : 0x4D);break;
        case HMBTN_SHOP_LEAVE:s->phase=HMBTN_SHOP_OFF;s->request=0;break;
        default:break;
        }
        return;
    }
    if(!ready) return;
    if(s->phase==HMBTN_SHOP_SELL_LIST) {
        unsigned count=0;
        if(barn) while(hmbtn_livestock_sale_slot(s->livestock,s->selection==7,count)<HMBTN_LIVESTOCK) ++count;
        else while(hmbtn_poultry_sale_slot(s->poultry,count)<HMBTN_HENS) ++count;
        s->after_result=HMBTN_SHOP_SELECT;
        if(!count) { next(s,HMBTN_SHOP_RESULT,barn ? (s->selection==7 ? 0x78 : 0x76) : 0x59);return; }
        if(s->sale_cursor>=count) s->sale_cursor=0;
        if(pressed&HMBTN_PAD_UP) { s->sale_cursor=(s->sale_cursor+count-1)%count;s->sound=5; }
        else if(pressed&HMBTN_PAD_DOWN) { s->sale_cursor=(s->sale_cursor+1)%count;s->sound=5; }
        else if(pressed&HMBTN_PAD_CIRCLE) { s->sound=1;next(s,HMBTN_SHOP_RESULT,barn ? 0x79 : 0x5B); }
        else if(pressed&HMBTN_PAD_CROSS) {
            if(barn) {
                s->sale_slot=hmbtn_livestock_sale_slot(s->livestock,s->selection==7,s->sale_cursor);s->answer=0;s->sound=0;
                const HmbtnFarmAnimal *a=&s->livestock->animals[s->sale_slot];
                const unsigned message=a->sheep ? (a->prize_winner ? 0x81 : a->affection<101 ? 0x7c : a->affection<201 ? 0x7d : 0x7e)
                    : a->prize_winner ? 0x82 : a->affection<101 ? 0x7d : a->affection<201 ? 0x7f : 0x80;
                next(s,HMBTN_SHOP_SELL_CONFIRM,message);
            } else {
            s->sale_slot=hmbtn_poultry_sale_slot(s->poultry,s->sale_cursor);s->answer=0;s->sound=0;
            next(s,HMBTN_SHOP_SELL_CONFIRM,s->poultry->hens[s->sale_slot].prize ? 0x61 : 0x60);
            }
        }
        if(s->sale_cursor<s->sale_top) s->sale_top=s->sale_cursor;
        if(s->sale_cursor>=s->sale_top+4) s->sale_top=s->sale_cursor-3;
        return;
    }
    if(s->phase==HMBTN_SHOP_SELL_CONFIRM) {
        if(pressed&(HMBTN_PAD_UP|HMBTN_PAD_DOWN)) { s->answer^=1;s->sound=5; }
        else if(pressed&(HMBTN_PAD_CROSS|HMBTN_PAD_CIRCLE)) {
            const bool yes=!(pressed&HMBTN_PAD_CIRCLE) && !s->answer;
            const unsigned price=!yes ? 0 : barn ? hmbtn_livestock_sell(s->livestock,s->sale_slot) : hmbtn_poultry_sell(s->poultry,s->sale_slot);
            s->sound=yes ? 0 : 1;s->after_result=HMBTN_SHOP_SELECT;
            if(price) {
                hmbtn_economy_gold_change(economy,(int64_t)price,false);
                if(!barn) stats->animals[0]=hmbtn_poultry_count(s->poultry);
            }
            next(s,HMBTN_SHOP_RESULT,barn ? (price ? 0x83 : 0x79) : price ? 0x5E : 0x5B);
        }
        return;
    }
    if(pressed&HMBTN_PAD_CIRCLE) {
        s->sound=1;
        next(s,HMBTN_SHOP_LEAVE,barn ? (s->phase==HMBTN_SHOP_SELECT ? 0x6B : 0x6E) : 0x4E);return;
    }
    if(pressed&HMBTN_PAD_TRIANGLE) {
        s->sound=0;s->after_result=s->phase;
        next(s,HMBTN_SHOP_RESULT,barn && s->selection==10 ? 0x281 : barn && s->selection>=8 ? 0x65+s->selection-8 : barn && s->selection>=4 ? 0x67+s->selection-4 : s->selection==3 ? (barn ? 0x64 : 0x48)
            : barn ? 0x63 : s->selection==2 ? 0x49 : s->selection ? 0x4A : 0x47);return;
    }
    if(s->phase==HMBTN_SHOP_SELECT) {
        if(pressed&(HMBTN_PAD_UP|HMBTN_PAD_DOWN)) {
            if(barn) {
                const unsigned count=hmbtn_barn_shop_rows(s);unsigned row=0;
                while(row<count && hmbtn_barn_shop_row(s,row)!=s->selection) ++row;
                row=(row+((pressed&HMBTN_PAD_UP) ? count-1 : 1))%count;
                s->selection=hmbtn_barn_shop_row(s,row);
            } else {
                const unsigned base=s->poultry ? 3 : 1,count=base+s->medicine_available;
                unsigned row=s->selection==3 ? base : s->selection;
                row=(row+((pressed&HMBTN_PAD_UP) ? count-1 : 1))%count;
                s->selection=row==base ? 3 : row;
            }
            s->item=barn && s->selection==10 ? 0x1d : barn && s->selection>=8 ? 0x1f+s->selection-8 : s->selection==3 ? 0x1E : s->selection==4 ? 0x161 : s->selection==5 ? 0x165 : s->selection==1 ? 0x109 : s->selection==0 ? (barn ? 0x68 : 0x69) : 0;
            s->quantity=1;s->sound=5;
        } else if(pressed&HMBTN_PAD_CROSS) {
            s->sound=0;s->after_result=HMBTN_SHOP_SELECT;
            if(barn && (s->selection==6 || s->selection==7)) {
                s->sale_cursor=s->sale_top=0;
                const bool sheep=s->selection==7,empty=hmbtn_livestock_sale_slot(s->livestock,sheep,0)==HMBTN_LIVESTOCK;
                next(s,empty ? HMBTN_SHOP_RESULT : HMBTN_SHOP_OFFER,empty ? (sheep ? 0x78 : 0x76) : sheep ? 0x77 : 0x75);
            }
            else if(barn && (s->selection==4 || s->selection==5)) {
                if(hmbtn_livestock_free(s->livestock)==HMBTN_LIVESTOCK) next(s,HMBTN_SHOP_RESULT,0x72);
                else if(economy->gold<hmbtn_feed_shop_cost(s)) next(s,HMBTN_SHOP_RESULT,0x4F);
                else next(s,HMBTN_SHOP_OFFER,0x73);
            }
            else if(s->selection==3 || (barn && s->selection>=8)) {
                const unsigned cost=hmbtn_feed_shop_cost(s);
                if(economy->gold<cost) next(s,HMBTN_SHOP_RESULT,0x4F);
                else if(s->selection==3 ? !buy_medicine(h)
                    : hmbtn_inventory_count(h->inventory,(int)s->item,0,2)>0
                        || !hmbtn_inventory_store_tool(h->inventory,(HmbtnStoredTool){(uint16_t)s->item,0,1},false,NULL))
                    next(s,HMBTN_SHOP_RESULT,0x54);
                else {
                    hmbtn_economy_gold_change(economy,-(int64_t)cost,false);
                    if(s->selection==3) s->medicine_available=false;
                    else if(s->selection==10) s->bell_available=false;
                    else s->potion_available[s->selection-8]=false;
                    s->selection=0;s->item=barn ? 0x68 : 0x69;
                    next(s,HMBTN_SHOP_RESULT,barn ? 0x84 : 0x51);
                }
            }
            else if(s->selection==2) {
                s->sale_cursor=s->sale_top=0;
                next(s,hmbtn_poultry_sale_slot(s->poultry,0)==HMBTN_HENS ? HMBTN_SHOP_RESULT : HMBTN_SHOP_OFFER,
                    hmbtn_poultry_sale_slot(s->poultry,0)==HMBTN_HENS ? 0x59 : 0x58);
            }
            else if(s->selection==1 && !hmbtn_poultry_room(s->poultry)) next(s,HMBTN_SHOP_RESULT,0x55);
            else if(s->selection==1 && economy->gold<hmbtn_feed_shop_cost(s)) next(s,HMBTN_SHOP_RESULT,0x4F);
            else next(s,HMBTN_SHOP_OFFER,barn ? 0x6C : s->selection ? 0x56 : 0x4B);
        }
        return;
    }
    const uint32_t arrows=held&(HMBTN_PAD_UP|HMBTN_PAD_DOWN|HMBTN_PAD_LEFT|HMBTN_PAD_RIGHT);
    uint32_t action=pressed;
    if(arrows!=s->previous_arrows) { s->repeat=0;action|=arrows;s->previous_arrows=arrows; }
    else if(arrows && ++s->repeat>=12 && (s->repeat-12)%3==0) action|=arrows;
    int quantity=(int)s->quantity;
    if(action&HMBTN_PAD_RIGHT) ++quantity;
    else if(action&HMBTN_PAD_LEFT) --quantity;
    else if(action&HMBTN_PAD_UP) quantity+=10;
    else if(action&HMBTN_PAD_DOWN) quantity-=10;
    else if(pressed&HMBTN_PAD_CROSS) {
        const unsigned cost=hmbtn_feed_shop_cost(s),stock=barn ? 0 : 2;
        s->after_result=HMBTN_SHOP_COUNT;s->sound=0;
        if(economy->gold<cost) next(s,HMBTN_SHOP_RESULT,0x4F);
        else if(h->inventory->supplies[stock]>=1000 || s->quantity>=1000-h->inventory->supplies[stock])
            next(s,HMBTN_SHOP_RESULT,barn ? 0x6D : 0x4C);
        else {
            h->inventory->supplies[stock]+=s->quantity;
            hmbtn_economy_gold_change(economy,-(int64_t)cost,false);
            s->after_result=HMBTN_SHOP_SELECT;next(s,HMBTN_SHOP_RESULT,barn ? 0x70 : 0x52);
        }
        return;
    }
    if(quantity!=(int)s->quantity) { s->quantity=quantity>99 ? 1 : quantity<=0 ? 99 : (unsigned)quantity;s->sound=5; }
}

unsigned hmbtn_barn_shop_rows(const HmbtnFeedShop *s)
{ return s ? 1u+(s->livestock ? 4u+s->potion_available[0]+s->potion_available[1]+s->bell_available : 0u)+s->medicine_available : 0; }
unsigned hmbtn_barn_shop_row(const HmbtnFeedShop *s,unsigned row)
{
    if(!s || !row) return 0;
    --row;
    if(s->livestock) {
        for(unsigned i=0;i<2;++i) if(s->potion_available[i]) { if(!row) return 8+i;--row; }
        if(row<4) return row+4;
        row-=4;
    }
    if(s->medicine_available) { if(!row) return 3;--row; }
    return s->bell_available && !row ? 10u : 0u;
}

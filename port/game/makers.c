/* milestones.c: milestone_set_coop_barn_bits/set_timer/status_query/tick;
 * shop_dialogues.c: shop_item_menu_step; player.c: MAKE_* product switches. */
#include "makers.h"
#include "poultry.h"
#include "inventory.h"
#include "economy.h"
bool hmbtn_saibara_accessory(unsigned item) { return item>=0xf7 && item<=0xf9; }
unsigned hmbtn_saibara_material(unsigned item) { return hmbtn_saibara_accessory(item) ? 0x61 : 0x62; }
bool hmbtn_saibara_accessory_ready(const HmbtnMakers *m,const HmbtnInventory *h,const HmbtnEconomy *e,unsigned item)
{
    const HmbtnFarmItem *info=hmbtn_farm_item(item);
    return m && h && e && !m->pending && hmbtn_saibara_accessory(item) && !h->held_item.id
        && info && info->buy_price>0 && e->gold>=(unsigned)info->buy_price
        && hmbtn_inventory_count(h,0x61,1,1)>0;
}
bool hmbtn_saibara_accessory_order(HmbtnMakers *m,HmbtnInventory *h,HmbtnEconomy *e,unsigned item)
{
    if(!hmbtn_saibara_accessory_ready(m,h,e,item)) return false;
    hmbtn_inventory_take(h,0x61,1,1,1);
    hmbtn_economy_gold_change(e,-(int64_t)hmbtn_farm_item(item)->buy_price,true);
    m->pending=item;m->mornings_left=3;return true;
}
unsigned hmbtn_makers_offer(const HmbtnMakers *m,const HmbtnConstruction *c,const HmbtnPoultry *p)
{
    if(!m || !c || !p || m->pending) return 0;
    if(p->upgraded && !(m->owned&1u)) return 0x13f;
    if(!c->barn_level || !(m->owned&1u)) return 0;
    if(!(m->owned&2u)) return 0x140;
    return m->owned&4u ? 0 : 0x141;
}
bool hmbtn_makers_order(HmbtnMakers *m,const HmbtnConstruction *c,const HmbtnPoultry *p,
    HmbtnInventory *h,HmbtnEconomy *e,unsigned item)
{
    const HmbtnFarmItem *info=hmbtn_farm_item(item);
    if(!h || !e || !item || item!=hmbtn_makers_offer(m,c,p) || h->held_item.id
            || !info || info->buy_price<=0 || e->gold<(unsigned)info->buy_price
            || hmbtn_inventory_count(h,0x62,1,1)<=0) return false;
    /* Both checks precede effects. The original modal requires empty hands;
     * where=1/which=1 searches hand + accessible rucksack, never storage. */
    hmbtn_inventory_take(h,0x62,1,1,1);
    hmbtn_economy_gold_change(e,-(int64_t)info->buy_price,true);
    m->pending=item;m->mornings_left=5;return true;
}
void hmbtn_makers_new_day(HmbtnMakers *m)
{ if(m && m->pending && m->mornings_left) --m->mornings_left; }
bool hmbtn_makers_deliver(HmbtnMakers *m)
{
    if(m && ((m->pending>=1 && m->pending<=25) || hmbtn_saibara_accessory(m->pending)) && !m->mornings_left) {
        m->pending=0;return true; /* Item granted by script 354 before APPLY. */
    }
    if(!m || m->pending<0x13f || m->pending>0x141 || m->mornings_left) return false;
    m->owned|=1u<<(m->pending-0x13f);m->pending=0;return true;
}
bool hmbtn_makers_valid(const HmbtnMakers *m,const HmbtnConstruction *c,const HmbtnPoultry *p)
{
    if(!m || !c || !p || (m->owned!=0 && m->owned!=1 && m->owned!=3 && m->owned!=7)
            || (m->owned && !p->upgraded) || ((m->owned&6u) && !c->barn_level)
            || m->mornings_left>5 || (!m->pending && m->mornings_left)) return false;
    if(m->pending>=1 && m->pending<=25) return (m->pending-1)%5!=0 && m->mornings_left<=3;
    if(hmbtn_saibara_accessory(m->pending)) return m->mornings_left<=3;
    HmbtnMakers available=*m;available.pending=0;
    return !m->pending || m->pending==hmbtn_makers_offer(&available,c,p);
}
unsigned hmbtn_makers_scene(const HmbtnMakers *m,unsigned logical,unsigned resource)
{
    if(m && logical==112 && (m->owned&1u)) return 120+(resource&3u);
    if(m && logical==96 && (m->owned&2u)) return ((m->owned&4u) ? 108u : 104u)+(resource&3u);
    return resource;
}
unsigned hmbtn_makers_product(unsigned zone,unsigned input)
{
    if(zone==14) {
        if(input>=0x122 && input<=0x124) return input-0x122+0x88;
        if(input==0x125) return 0x8a;
        return input==0xe8 ? 0xe5 : 0;
    }
    if(zone==33) {
        if(input>=0x8b && input<=0x8d) return input+3;
        return input==0xe3 ? 0xe4 : 0;
    }
    if(zone==15) {
        if(input>=0x91 && input<=0x93) return input+3;
        return input==0xe7 ? 0xe6 : 0;
    }
    return 0;
}

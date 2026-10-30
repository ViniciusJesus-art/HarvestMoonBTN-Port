/* shop_tool_upgrade_state, greenhouse helpers and player_result_step.
 * The original carried ore + equipped tool drive the offer, including skips. */
#include "tool_upgrade.h"
#include "economy.h"
#include "tool_rules.inc"
unsigned hmbtn_tool_ore_level(unsigned ore)
{ return ore==0x7c ? 1u : ore==0x7b ? 2u : ore==0x7a ? 3u : ore==0x60 ? 4u : 0u; }
unsigned hmbtn_tool_upgrade_price(unsigned level)
{ return level<5 ? greenhouse_level_alt_values[level] : 0; }
unsigned hmbtn_tool_upgrade_experience(unsigned level)
{ return level<5 ? greenhouse_level_values[level] : 65535; }
unsigned hmbtn_tool_upgrade_reason(const HmbtnInventory *h,const unsigned exp[5])
{
    if(!h || !exp || !h->equipped.id || h->equipped.id>25) return 1;
    const unsigned kind=(h->equipped.id-1)/5,rank=(h->equipped.id-1)%5,level=hmbtn_tool_ore_level(h->held_item.id);
    if(rank==4) return 2;
    if(level<rank) return 3;
    if(level==rank) return 4;
    return exp[kind]<hmbtn_tool_upgrade_experience(level) ? 5 : 0;
}
bool hmbtn_tool_upgrade_order(HmbtnMakers *m,HmbtnInventory *h,HmbtnEconomy *e,const unsigned exp[5])
{
    if(!m || !h || !e || m->pending || !hmbtn_tool_ore_level(h->held_item.id)
            || hmbtn_tool_upgrade_reason(h,exp)) return false;
    const unsigned level=hmbtn_tool_ore_level(h->held_item.id),price=hmbtn_tool_upgrade_price(level);
    if(e->gold<price) return false;
    m->pending=1+(h->equipped.id-1)/5*5+level;m->mornings_left=3;
    /* Source takes the held records, then event_item_store creates the new id
     * with modifier=rank/count=1. Other stores and shared XP remain untouched. */
    h->equipped=h->held_item=(HmbtnStoredTool){0};
    hmbtn_economy_gold_change(e,-(int64_t)price,true);return true;
}
unsigned hmbtn_tool_targets(unsigned kind,unsigned charge)
{
    if(kind>=5 || charge>=5) return 0;
    return kind==1 ? tool_charge_steps_a[charge] : kind==4 ? tool_charge_steps_b[charge]
        : kind==0 ? tool_charge_steps_c[charge] : 1;
}
unsigned hmbtn_tool_stamina(unsigned kind,unsigned charge)
{ return kind<5 && charge<5 ? (unsigned)-tool_stamina_costs[kind][charge] : 0; }
bool hmbtn_tool_grid(unsigned kind,unsigned charge,unsigned part,unsigned facing,int *dx,int *dr)
{
    if(kind>=5 || charge>=5 || facing>3 || !dx || !dr) return false;
    const unsigned grid=kind*5+charge;if(part>=tool_grid_counts[grid]) return false;
    int x=tool_grids[grid][part][0],r=tool_grids[grid][part][1];
    if(facing==0) { const int swap=x;x=r;r=swap; }
    else if(facing==1) x=-x;
    else if(facing==2) { const int swap=x;x=r;r=-swap; }
    *dx=x;*dr=r;return true;
}

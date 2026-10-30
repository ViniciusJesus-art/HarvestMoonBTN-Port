/* Adapted from menu/house.c:cooking_find_record/claim_recipe_tools,
 * claim_entry_slots/sum_wanted_cues/clear_pattern_value/farm_cue_add_delta,
 * timer_decrement/hold_result/record_store. Tables are the original C definitions.
 * Bounds, explicit native owners and immutable trial buffers replace PS1 globals. */
#include "cooking.h"
#include "farm_field.h"
#include <string.h>
typedef struct HmbtnRecipeBonus { unsigned id;int points; } HmbtnRecipeBonus;
typedef struct HmbtnRecipe {
    const uint16_t *name;unsigned id,quality,tools;uint16_t ingredients[8];
    const HmbtnRecipeBonus *bonuses;
} HmbtnRecipe;
#include "cooking_tables.inc"
const HmbtnStoredTool *hmbtn_cooking_source(const HmbtnInventory *h,unsigned page,unsigned cell)
{
    if(!h || page>8 || cell>=8 || (!page && cell>=hmbtn_inventory_capacity(h))) return NULL;
    return page ? &h->icebox[(page-1)*8+cell] : &h->items[cell];
}
unsigned hmbtn_cooking_option_item(unsigned bit) { return bit<12 ? cooking_option_items[bit] : 0; }
bool hmbtn_cooking_item_stored(const HmbtnInventory *h,unsigned id)
{
    if(!id) return true;
    for(unsigned page=0;page<9;++page) for(unsigned cell=0;cell<8;++cell) {
        const HmbtnStoredTool *item=hmbtn_cooking_source(h,page,cell);
        if(item && item->id==id && item->count) return true;
    }
    return false;
}
unsigned hmbtn_cooking_allowed_options(const HmbtnCooking *c)
{ return c ? (c->utensils&0x7f)|((c->utensils&0x80) ? 0xf80u : 0) : 0; }
const uint16_t *hmbtn_cooking_dish_name(unsigned id)
{
    for(unsigned i=0;i<sizeof(recipes)/sizeof(*recipes);++i) if(recipes[i].id==id) return recipes[i].name;
    return NULL;
}
unsigned hmbtn_cooking_record_count(const HmbtnCooking *c)
{
    unsigned n=0;if(c) for(unsigned i=0;i<HMBTN_RECIPE_RECORDS;++i) n+=c->records[i].id!=0;
    return n;
}
unsigned hmbtn_cooking_record_slot(const HmbtnCooking *c,unsigned id)
{
    if(c) for(unsigned i=0;i<HMBTN_RECIPE_RECORDS;++i) if(c->records[i].id==id) return i;
    return HMBTN_RECIPE_RECORDS;
}
static unsigned group(unsigned id)
{
    for(unsigned g=0;g<4;++g) {
        if(id==0x181+g) return g;
        for(unsigned i=0;i<4;++i) if(cooking_groups[g][i]==id) return g;
    }
    return 4;
}
static bool bonus(const HmbtnRecipe *r,unsigned id,int *total)
{
    for(const HmbtnRecipeBonus *b=r->bonuses;b->id;++b) if(b->id==id) { *total+=b->points;return true; }
    return false;
}
bool hmbtn_cooking_evaluate(unsigned options,const uint16_t values[8],HmbtnRecipeRecord *result)
{
    if(!values || !result || options&~0xfffu) return false;
    *result=(HmbtnRecipeRecord){.id=COOKING_FIASCO,.quality=1,.options=(uint16_t)options};
    memcpy(result->values,values,sizeof(result->values));
    for(unsigned n=0;n<sizeof(recipes)/sizeof(*recipes) && recipes[n].id!=COOKING_FIASCO;++n) {
        const HmbtnRecipe *r=&recipes[n];uint16_t rest[8];memcpy(rest,values,sizeof(rest));
        if((options&r->tools)!=r->tools) continue;
        bool matches=true;
        for(unsigned i=0;i<8 && matches;++i) if(r->ingredients[i]) {
            const unsigned wanted=r->ingredients[i];unsigned j=0;
            for(;j<8;++j) if(rest[j] && (wanted>=0x181 && wanted<=0x184
                    ? group(rest[j])==wanted-0x181 : rest[j]==wanted)) break;
            if(j==8) matches=false;else rest[j]=0;
        }
        int total=0;
        for(unsigned i=0;i<12 && matches;++i) if((options&~r->tools)&(1u<<i)) matches=bonus(r,cooking_option_items[i],&total);
        for(unsigned i=0;i<8 && matches;++i) if(rest[i]) {
            const unsigned g=group(rest[i]);matches=bonus(r,g<4 ? 0x181+g : rest[i],&total);
        }
        if(matches) { result->id=(uint16_t)r->id;result->quality=(uint16_t)((r->quality+total)&0xffffu);break; }
    }
    return true;
}
static bool select_ingredients(const HmbtnCooking *c,const HmbtnInventory *h,unsigned options,const uint16_t values[8],const HmbtnStoredTool *picked[8])
{
    if(!c || !h || !values || h->held_item.id || (options&~hmbtn_cooking_allowed_options(c))) return false;
    unsigned count=0;
    for(unsigned i=0;i<8;++i) if(values[i]) {
        for(unsigned j=0;j<i;++j) if(values[i]==values[j]) return false;
        for(unsigned page=0;page<9 && !picked[i];++page) for(unsigned slot=0;slot<8;++slot) {
            const HmbtnStoredTool *a=hmbtn_cooking_source(h,page,slot);
            if(a && a->id==values[i] && a->count) { picked[i]=a;break; }
        }
        if(!picked[i]) return false;
        const HmbtnFarmItem *item=hmbtn_farm_item(values[i]);
        if(!item || item->category==1) return false;
        ++count;
    }
    return count>0;
}
bool hmbtn_cooking_available(const HmbtnCooking *c,const HmbtnInventory *h,unsigned options,const uint16_t values[8])
{
    const HmbtnStoredTool *picked[8]={0};return select_ingredients(c,h,options,values,picked);
}
bool hmbtn_cooking_make(const HmbtnCooking *c,HmbtnInventory *h,unsigned options,const uint16_t values[8],HmbtnRecipeRecord *result)
{
    const HmbtnStoredTool *picked[8]={0};
    if(!result || !select_ingredients(c,h,options,values,picked) || !hmbtn_cooking_evaluate(options,values,result)) return false;
    for(unsigned i=0;i<8;++i) if(picked[i]) {
        HmbtnStoredTool *item=(HmbtnStoredTool *)picked[i];
        if(item->count>1) --item->count;else *item=(HmbtnStoredTool){0};
    }
    h->held_item=(HmbtnStoredTool){result->id,result->quality,1};return true;
}
bool hmbtn_cooking_remember(HmbtnCooking *c,const HmbtnRecipeRecord *r)
{
    if(!c || !r || !r->id || r->id==COOKING_FIASCO || !hmbtn_cooking_dish_name(r->id)) return false;
    unsigned slot=hmbtn_cooking_record_slot(c,r->id);
    if(slot==HMBTN_RECIPE_RECORDS) slot=hmbtn_cooking_record_slot(c,0);
    if(slot==HMBTN_RECIPE_RECORDS) return false;
    c->records[slot]=*r;return true;
}

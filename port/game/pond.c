/* field/pond.c: first free slot, largest-first withdrawal, fed aging and spawn.
 * Forward traversal intentionally ages newborns allocated later in this pass. */
#include "pond.h"
#include "psx_random.h"
unsigned hmbtn_pond_count(const HmbtnPond *p)
{
    unsigned count=0;if(p) for(unsigned i=0;i<HMBTN_POND_FISH;++i) count+=p->fish[i].item!=0;
    return count;
}
bool hmbtn_pond_add(HmbtnPond *p,unsigned item)
{
    if(!p || item<161 || item>163) return false;
    for(unsigned i=0;i<HMBTN_POND_FISH;++i) if(!p->fish[i].item) {
        p->fish[i]=(HmbtnPondFish){(uint16_t)item,(uint16_t)(item==161 ? HMBTN_POND_LARGE_DAYS : item==162 ? HMBTN_POND_MEDIUM_DAYS : 0)};
        return true;
    }
    return false;
}
unsigned hmbtn_pond_take(HmbtnPond *p)
{
    if(p) for(unsigned item=161;item<=163;++item) for(unsigned i=0;i<HMBTN_POND_FISH;++i) if(p->fish[i].item==item) {
        p->fish[i].item=0;return item;
    }
    return 0;
}
void hmbtn_pond_new_day(HmbtnPond *p)
{
    if(!p) return;
    if(p->fed) for(unsigned i=0;i<HMBTN_POND_FISH;++i) {
        HmbtnPondFish *f=&p->fish[i];if(!f->item) continue;
        ++f->days;f->item=f->days<HMBTN_POND_MEDIUM_DAYS ? 163 : f->days<HMBTN_POND_LARGE_DAYS ? 162 : 161;
        if(f->days>=HMBTN_POND_SPAWN_DAYS) {
            const int count=((int)hmbtn_psx_random_range(&p->random,100)-(int)hmbtn_pond_count(p))/25;
            f->days=HMBTN_POND_LARGE_DAYS;
            for(int n=0;n<count;++n) (void)hmbtn_pond_add(p,163);
        }
    }
    p->fed=false;
}
bool hmbtn_pond_valid(const HmbtnPond *p)
{
    if(!p) return false;
    for(unsigned i=0;i<HMBTN_POND_FISH;++i) {
        const HmbtnPondFish *f=&p->fish[i];
        if(f->item && (f->days>=HMBTN_POND_SPAWN_DAYS || f->item!=(f->days<HMBTN_POND_MEDIUM_DAYS ? 163u : f->days<HMBTN_POND_LARGE_DAYS ? 162u : 161u))) return false;
    }
    return true;
}

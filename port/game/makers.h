#ifndef HMBTN_MAKERS_H
#define HMBTN_MAKERS_H
#include "construction.h"
struct HmbtnPoultry;
/* milestones.c stores these in coop/barn levels and estate bits. Keep the
 * physical native buildings/capacities unchanged; derive scene levels below.
 * pending is the shared Saibara order (tool, accessory or maker item id). */
typedef struct HmbtnMakers { unsigned owned,pending,mornings_left; } HmbtnMakers;
unsigned hmbtn_makers_offer(const HmbtnMakers *,const HmbtnConstruction *,const struct HmbtnPoultry *);
bool hmbtn_makers_order(HmbtnMakers *,const HmbtnConstruction *,const struct HmbtnPoultry *,
    HmbtnInventory *,HmbtnEconomy *,unsigned item);
bool hmbtn_saibara_accessory(unsigned item);
unsigned hmbtn_saibara_material(unsigned item);
bool hmbtn_saibara_accessory_ready(const HmbtnMakers *,const HmbtnInventory *,const HmbtnEconomy *,unsigned item);
bool hmbtn_saibara_accessory_order(HmbtnMakers *,HmbtnInventory *,HmbtnEconomy *,unsigned item);
void hmbtn_makers_new_day(HmbtnMakers *);
bool hmbtn_makers_deliver(HmbtnMakers *);
bool hmbtn_makers_valid(const HmbtnMakers *,const HmbtnConstruction *,const struct HmbtnPoultry *);
/* scene_resolve_selection: seasonal resource id, logical area stays 96/112. */
unsigned hmbtn_makers_scene(const HmbtnMakers *,unsigned logical,unsigned resource);
/* player.c input -> output; zero means wrong ingredient. */
unsigned hmbtn_makers_product(unsigned zone,unsigned input);
#endif

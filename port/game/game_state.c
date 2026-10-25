#include "game_state.h"
#include <stddef.h>
#include <string.h>

void hmbtn_game_state_init(HmbtnGameState *state)
{
    memset(state,0,sizeof(*state));
    hmbtn_farm_clock_init(&state->clock);
    hmbtn_registration_init(&state->registration);
    /* Current host new-game purse, formerly initialized by farm_menu_init. */
    state->economy.gold=500;
    /* Initial stores moved verbatim from hmbtn_house_interaction_init. Source:
     * title.c/title_copy_new_game_toolbox; retain the current host defaults. */
    const uint16_t tools[5]={1,11,6,16,21};
    for(unsigned i=0;i<5;++i) state->inventory.toolbox[i].id=tools[i];
}
void hmbtn_registration_init(HmbtnRegistration *registration)
{
    memset(registration,0,sizeof(*registration));
    /* Defaults formerly installed by new_game_init; not a new birthday rule. */
    for(unsigned i=0;i<=HMBTN_NAME_CHARS;++i)
        registration->player_name[i]=registration->farm_name[i]=registration->dog_name[i]=0xffff;
}
HmbtnInventory *hmbtn_game_inventory(HmbtnGameState *state)
{
    return state ? &state->inventory : NULL;
}
const HmbtnInventory *hmbtn_game_inventory_const(const HmbtnGameState *state)
{
    return state ? &state->inventory : NULL;
}
HmbtnEconomy *hmbtn_game_economy(HmbtnGameState *state)
{
    return state ? &state->economy : NULL;
}
const HmbtnEconomy *hmbtn_game_economy_const(const HmbtnGameState *state)
{
    return state ? &state->economy : NULL;
}
HmbtnFarmClock *hmbtn_game_clock(HmbtnGameState *state)
{
    return state ? &state->clock : NULL;
}
const HmbtnFarmClock *hmbtn_game_clock_const(const HmbtnGameState *state)
{
    return state ? &state->clock : NULL;
}
HmbtnRegistration *hmbtn_game_registration(HmbtnGameState *state)
{
    return state ? &state->registration : NULL;
}
const HmbtnRegistration *hmbtn_game_registration_const(const HmbtnGameState *state)
{
    return state ? &state->registration : NULL;
}
const HmbtnStoredTool *hmbtn_inventory_slot_const(const HmbtnInventory *inventory,unsigned i)
{
    /* Slot selection moved from farm_menu.c, without changing its rules. */
    if(!inventory || i>=18 || inventory->rucksack_level>2) return NULL;
    if(i==0) return &inventory->equipped;
    if(i==9) return &inventory->held_item;
    const unsigned capacity=2u<<inventory->rucksack_level;
    if(i<9) return i<=capacity ? &inventory->bag[i-1] : NULL;
    return i-10<capacity ? &inventory->items[i-10] : NULL;
}
HmbtnStoredTool *hmbtn_inventory_slot(HmbtnInventory *inventory,unsigned index)
{
    /* The input is mutable; share the read-only resolver rather than duplicate
     * capacity/slot rules or require const casts in presentation code. */
    return (HmbtnStoredTool *)hmbtn_inventory_slot_const(inventory,index);
}

#ifndef HMBTN_TOOL_UPGRADE_H
#define HMBTN_TOOL_UPGRADE_H
#include "makers.h"
unsigned hmbtn_tool_ore_level(unsigned ore);
unsigned hmbtn_tool_upgrade_price(unsigned level);
unsigned hmbtn_tool_upgrade_experience(unsigned level);
/* shop_tool_upgrade_state reasons 0..5, original order of guards. */
unsigned hmbtn_tool_upgrade_reason(const HmbtnInventory *,const unsigned experience[5]);
bool hmbtn_tool_upgrade_order(HmbtnMakers *,HmbtnInventory *,HmbtnEconomy *,const unsigned experience[5]);
/* Original charge grid/count/cost, shared by field actions and validation. */
unsigned hmbtn_tool_targets(unsigned kind,unsigned charge);
unsigned hmbtn_tool_stamina(unsigned kind,unsigned charge);
bool hmbtn_tool_grid(unsigned kind,unsigned charge,unsigned part,unsigned facing,int *dx,int *dr);
#endif

#include "item_reaction.h"
#include "farm_field.h"
#include "item_reactions.inc"
int hmbtn_item_villager_reaction(unsigned item,unsigned villager)
{
    const HmbtnFarmItem *info=hmbtn_farm_item(item);
    if(!info || info->picture<0 || info->picture>=174 || villager>=40) return -1;
    if(info->flags&0x4000u) return -2;
    unsigned code=(picture_codes[info->picture][villager/10]>>((villager%10)*3))&7u;
    if(info->flags&0x180u) {
        if(code<=2) code=5;
        else if(code<=4) code=6;
    }
    return (int)code;
}

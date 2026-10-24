#include "event_item.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    HmbtnHouseInteraction inventory={0};HmbtnIntroActor player={.facing=2};HmbtnEventItem item={0};
    HmbtnFarmAction status;hmbtn_farm_action_init(&status);status.fatigue=20;
    inventory.held_item=(HmbtnStoredTool){57,2,3};
    assert(!hmbtn_event_item_begin(&item,&player,&inventory,204,false));
    assert(!hmbtn_event_item_begin(&item,&player,&inventory,204,true));
    assert(hmbtn_event_item_begin(&item,&player,&inventory,225,true));
    assert(player.sprite.animation==5 && player.pose && inventory.held_item.id==225 && item.saved.id==57);
    assert(!hmbtn_event_item_begin(&item,&player,&inventory,225,true));
    hmbtn_event_item_tick(&item,&player,&inventory,&status);assert(item.phase==HMBTN_EVENT_ITEM_EAT && status.fatigue==20);
    player.animation_done=true;hmbtn_event_item_tick(&item,&player,&inventory,&status);
    assert(item.phase==HMBTN_EVENT_ITEM_IDLE && inventory.held_item.id==57
        && inventory.held_item.modifier==2 && inventory.held_item.count==3 && status.fatigue==16);
    status.fatigue=2;assert(hmbtn_event_item_begin(&item,&player,&inventory,225,true));
    player.animation_done=true;hmbtn_event_item_tick(&item,&player,&inventory,&status);assert(!status.fatigue);
    inventory.held_item=(HmbtnStoredTool){0};assert(hmbtn_event_item_begin(&item,&player,&inventory,204,false));
    assert(player.sprite.animation==13 && !inventory.held_item.id && item.shown==204);
    player.animation_done=true;hmbtn_event_item_tick(&item,&player,&inventory,&status);
    assert(inventory.held_item.id==204 && inventory.held_item.count==1 && !player.pose && !item.shown);
    puts("Script item presentation, cake consumption and carried-item restoration passed.");return 0;
}

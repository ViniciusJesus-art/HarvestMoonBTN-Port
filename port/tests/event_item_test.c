#include "event_item.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    HmbtnInventory inventory={0};HmbtnIntroActor player={.facing=2};HmbtnEventItem item={0};
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
    /* Each command removes at most one record, but counts all matches. The
     * record's count field is not the number of copies used by this command. */
    inventory=(HmbtnInventory){.rucksack_level=1,.held_item={290,3,9}};
    for(unsigned i=0;i<8;++i) inventory.items[i]=(HmbtnStoredTool){290,2,7};
    HmbtnInventory before=inventory;
    assert(hmbtn_event_items_take(&inventory,290,1,1,0)==5 && !memcmp(&before,&inventory,sizeof(before)));
    assert(hmbtn_event_items_take(&inventory,290,1,1,1)==5 && !inventory.held_item.id && inventory.items[0].id==290);
    assert(hmbtn_event_items_take(&inventory,290,1,1,1)==4 && !inventory.items[0].id && inventory.items[1].id==290);
    assert(hmbtn_event_items_take(&inventory,290,1,1,2)==3 && !inventory.items[1].id && !inventory.items[2].id && inventory.items[3].id==290);
    inventory=before;
    assert(hmbtn_event_items_take(&inventory,290,1,-1,1)==3 && inventory.held_item.id==290
        && inventory.items[0].id==290 && !inventory.items[1].id && inventory.items[2].id==290);
    before=inventory;assert(hmbtn_event_items_take(&inventory,290,1,2,1)==-1 && !memcmp(&before,&inventory,sizeof(before)));
    inventory.rucksack_level=3;assert(hmbtn_event_items_take(&inventory,290,1,1,1)==-1);
    puts("Script item presentation, cake consumption, restoration and bounded per-record removal passed.");return 0;
}

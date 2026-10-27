#include "inventory.h"
#include "event_item.h"
#include "feed_shop.h"
#include "inventory_checks.h"
#include <stdio.h>

#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Inventory rules line %d: %s\n",__LINE__,#x);return 1; } } while(0)
int main(void)
{
    /* event_item_store order, quiet/spill policy and record preservation. The
     * public rule resolves records; event_tool_store tests script operands,
     * original tool modifiers, notifications and transactional rollback. */
    const HmbtnStoredTool tool={56,7,9};
    for(unsigned level=0;level<3;++level) {
        HmbtnInventory inventory={.rucksack_level=level};
        const unsigned capacity=2u<<level;
        CHECK(hmbtn_inventory_capacity(&inventory)==capacity);
        HmbtnInventoryPlacement placement;
        CHECK(hmbtn_inventory_store_tool(&inventory,tool,false,&placement));
        CHECK(placement.store==HMBTN_STORE_EQUIPPED && inventory.equipped.modifier==7
            && inventory.equipped.count==9);
        for(unsigned i=0;i<capacity;++i) {
            CHECK(hmbtn_inventory_store_tool(&inventory,tool,false,&placement));
            CHECK(placement.store==HMBTN_STORE_BAG && placement.index==i);
        }
        HmbtnInventory before=inventory;
        placement=(HmbtnInventoryPlacement){HMBTN_STORE_BAG,99};
        CHECK(!hmbtn_inventory_store_tool(&inventory,tool,false,&placement)
            && inventory_equal(&inventory,&before) && placement.index==99);
        for(unsigned i=0;i<hmbtn_inventory_toolbox_capacity(&inventory);++i) {
            CHECK(hmbtn_inventory_store_tool(&inventory,tool,true,&placement));
            CHECK(placement.store==HMBTN_STORE_TOOLBOX && placement.index==i);
        }
        before=inventory;
        CHECK(!hmbtn_inventory_store_tool(&inventory,tool,true,NULL) && inventory_equal(&inventory,&before));
        CHECK(hmbtn_event_items_count(&inventory,56,0,2)==(int)(1+capacity+64));
        CHECK(hmbtn_inventory_take(&inventory,56,0,2,2)==(int)(1+capacity+64));
        CHECK(!inventory.equipped.id && !inventory.bag[0].id && inventory.bag[1].id==56);
        CHECK(hmbtn_inventory_count(&inventory,56,0,2)==(int)(capacity+63));
    }
    HmbtnInventory inventory={.rucksack_level=0,.held_item={65,0,1}};
    inventory.items[0]=(HmbtnStoredTool){65,4,99};inventory.items[1]=(HmbtnStoredTool){65,3,7};
    CHECK(hmbtn_inventory_take(&inventory,65,1,-1,1)==1 && !inventory.items[1].id
        && inventory.items[0].count==99 && inventory.held_item.id==65);
    HmbtnInventory before=inventory;
    CHECK(hmbtn_inventory_take(&inventory,65,1,4,1)==-1 && inventory_equal(&inventory,&before));
    CHECK(!hmbtn_inventory_hold(&inventory,(HmbtnStoredTool){64,0,1}) && inventory_equal(&inventory,&before));
    CHECK(hmbtn_event_items_take(&inventory,65,1,0,1)==1);
    CHECK(hmbtn_inventory_hold(&inventory,(HmbtnStoredTool){64,2,3}) && inventory.held_item.modifier==2);
    inventory.rucksack_level=3;before=inventory;
    CHECK(!hmbtn_inventory_store_tool(&inventory,tool,true,NULL) && inventory_equal(&inventory,&before));
    CHECK(!hmbtn_inventory_cycle(&inventory,true,false) && inventory_equal(&inventory,&before));
    CHECK(hmbtn_inventory_take(&inventory,56,0,0,1)==-1);
    CHECK(!hmbtn_inventory_capacity(NULL) && !hmbtn_inventory_toolbox_capacity(NULL));
    CHECK(!hmbtn_inventory_store_tool(NULL,tool,true,NULL) && !hmbtn_inventory_hold(NULL,tool));

    /* Cycling must not pull a record out of an inaccessible bag cell. This is
     * a deliberate correction of the prior host loop over all eight cells. */
    inventory=(HmbtnInventory){.equipped={1,0,1},.bag={{6,0,1},{11,0,1}}};
    inventory.bag[7]=(HmbtnStoredTool){16,4,9};
    CHECK(hmbtn_inventory_cycle(&inventory,true,false));
    CHECK(inventory.equipped.id==6 && inventory.bag[0].id==11 && inventory.bag[1].id==1
        && inventory.bag[7].id==16 && inventory.bag[7].count==9);
    CHECK(hmbtn_inventory_cycle(&inventory,true,true));
    CHECK(inventory.equipped.id==1 && inventory.bag[0].id==6 && inventory.bag[1].id==11
        && inventory.bag[7].modifier==4);
    /* Pure placement has no original catalog ceiling; UI/import expansion is
     * separate. No second inventory is needed for a future content ID. */
    inventory=(HmbtnInventory){0};
    CHECK(hmbtn_inventory_store_tool(&inventory,(HmbtnStoredTool){1000,3,1},false,NULL));
    CHECK(hmbtn_inventory_count(&inventory,1000,0,0)==1);

    /* Shop medicine and generic placement observe the same accessible stores. */
    HmbtnGameState game;hmbtn_game_state_init(&game);game.economy.gold=5000;
    HmbtnHouseInteraction house;hmbtn_house_interaction_init(&house,&game);
    HmbtnFarmMenu menu;hmbtn_farm_menu_init(&menu,&game.economy);
    HmbtnFeedShop shop={.merchant=15,.selection=3,.item=0x1E,.quantity=1,.phase=HMBTN_SHOP_SELECT};
    shop.message.words[0]=0xffff;shop.message.count=1;shop.message.phase=HMBTN_MESSAGE_WAIT_END;
    hmbtn_feed_shop_tick(&shop,&house,&game.economy,&menu.stats,HMBTN_PAD_CROSS,0);
    CHECK(hmbtn_inventory_count(&game.inventory,0x1E,0,2)==1 && game.economy.ledger[0][1]>0);
    const unsigned gold=game.economy.gold;
    CHECK(hmbtn_inventory_swap(&game.inventory,0,1));
    CHECK(hmbtn_event_items_count(house.inventory,0x1E,0,2)==1 && !game.inventory.equipped.id);
    for(unsigned i=0;i<5;++i) hmbtn_feed_shop_tick(&shop,&house,&game.economy,&menu.stats,HMBTN_PAD_CROSS,0);
    CHECK(game.economy.gold==gold && !shop.medicine_available);
    CHECK(hmbtn_event_items_take(&game.inventory,0x1E,0,2,1)==1);
    hmbtn_feed_shop_tick(&shop,&house,&game.economy,&menu.stats,0,0);
    CHECK(shop.medicine_available);
    puts("Original placement/traversal, capacity boundaries, inactive cells and shared medicine/menu/script rules passed.");
    return 0;
}

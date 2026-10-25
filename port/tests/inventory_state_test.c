#include "inventory_checks.h"
#include "house_interaction.h"
#include "farm_menu.h"
#include "farm_feed.h"
#include "event_item.h"
#include "village_talk.h"
#include "psx_data.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Inventory line %d: %s\n",__LINE__,#x);return 1; } } while(0)

static void grant_script(uint8_t bytes[108],bool unsupported)
{
    memset(bytes,0,108);
    hmbtn_write_le16(bytes+2,1);hmbtn_write_le16(bytes+4,0x4000);
    hmbtn_write_le32(bytes+56,48);
    uint8_t *p=bytes+60;
    p[0]=32;p[1]=p[2]=255;p[3]=1; /* One yield after the existing tool grant. */
    hmbtn_write_le16(p+4,56);hmbtn_write_le32(p+8,1);
    p+=16;p[0]=unsupported ? 81 : 37;p[1]=p[2]=255;
    if(unsupported) hmbtn_write_le16(p+4,8);
    p[16]=255;
}
static void feed_zone(uint8_t section[0x2000])
{
    memset(section,0,0x2000);uint8_t *p=section+16;
    hmbtn_write_le16(p,0x4000);hmbtn_write_le16(p+2,26);
    hmbtn_write_le16(p+14,(uint16_t)-300);hmbtn_write_le16(p+18,(uint16_t)-300);
    hmbtn_write_le16(p+20,300);hmbtn_write_le16(p+24,300);
}
int main(void)
{
    HmbtnGameState game,other;
    hmbtn_game_state_init(&game);hmbtn_game_state_init(&other);
    HmbtnInventory *inventory=hmbtn_game_inventory(&game);
    const HmbtnInventory *independent=hmbtn_game_inventory_const(&other);
    CHECK(inventory==&game.inventory && independent==&other.inventory && inventory!=independent);
    CHECK(!hmbtn_game_inventory(NULL) && !hmbtn_game_inventory_const(NULL));
    CHECK(inventory_equal(inventory,independent));
    const unsigned tools[5]={1,11,6,16,21};
    for(unsigned i=0;i<64;++i) {
        CHECK(inventory->toolbox[i].id==(i<5 ? tools[i] : 0));
        CHECK(!inventory->toolbox[i].modifier && !inventory->toolbox[i].count);
    }

    HmbtnHouseInteraction house,second_ui;
    hmbtn_house_interaction_init(&house,&game);
    hmbtn_house_interaction_init(&second_ui,&game);
    CHECK(house.inventory==second_ui.inventory && house.inventory==inventory);
    /* Move a tool through the real toolbox UI, then through the rucksack UI. */
    house.mode=HMBTN_HOUSE_TOOLBOX;house.message.phase=HMBTN_MESSAGE_WAIT_END;house.cursor=9;
    hmbtn_house_interaction_tick(&house,HMBTN_PAD_CROSS,0);
    house.cursor=0;hmbtn_house_interaction_tick(&house,HMBTN_PAD_CROSS,0);
    CHECK(inventory->equipped.id==1 && !inventory->toolbox[0].id && second_ui.inventory->equipped.id==1);
    HmbtnFarmMenu menu;hmbtn_farm_menu_init(&menu,hmbtn_game_economy_const(&game));
    CHECK(hmbtn_farm_menu_open(&menu,HMBTN_PAD_TRIANGLE));menu.message.phase=HMBTN_MESSAGE_WAIT_END;
    hmbtn_farm_menu_tick(&menu,inventory,HMBTN_PAD_CROSS,0);
    menu.cursor=2;hmbtn_farm_menu_tick(&menu,inventory,HMBTN_PAD_CROSS,0);
    CHECK(!inventory->equipped.id && inventory->bag[1].id==1 && house.inventory->bag[1].id==1);
    CHECK(hmbtn_event_items_count(inventory,1,0,1)==1);
    CHECK(hmbtn_event_items_take(second_ui.inventory,1,0,1,1)==1 && !inventory->bag[1].id);

    for(unsigned level=0;level<3;++level) {
        inventory->rucksack_level=level;const unsigned capacity=2u<<level;
        CHECK(hmbtn_inventory_slot(inventory,0)==&inventory->equipped);
        CHECK(hmbtn_inventory_slot_const(inventory,9)==&inventory->held_item);
        CHECK(hmbtn_inventory_slot(inventory,capacity)==&inventory->bag[capacity-1]);
        CHECK(hmbtn_inventory_slot_const(inventory,9+capacity)==&inventory->items[capacity-1]);
        if(capacity<8) CHECK(!hmbtn_inventory_slot(inventory,capacity+1));
    }
    inventory->rucksack_level=3;CHECK(!hmbtn_inventory_slot(inventory,0));
    CHECK(!hmbtn_inventory_slot(inventory,18) && !hmbtn_inventory_slot_const(NULL,0));
    inventory->rucksack_level=1;

    /* Script grants/rollback are observed immediately by both UI references. */
    uint8_t bytes[108];char error[256]={0};HmbtnVillageTalk talk;HmbtnVillage village;
    hmbtn_village_init(&village,NULL,0);
    HmbtnIntroActor player={0};HmbtnFarmClock clock={0,0,2,2,12,0,0};HmbtnRegistration registration;
    hmbtn_registration_init(&registration);grant_script(bytes,false);
    CHECK(hmbtn_village_talk_open(&talk,32,bytes,sizeof(bytes),&village,&player,&clock,&registration,&house,error,sizeof(error)));
    CHECK(hmbtn_village_talk_tick(&talk,error,sizeof(error)) && talk.active && inventory->equipped.id==56);
    CHECK(second_ui.inventory->equipped.id==56 && !hmbtn_event_items_count(independent,56,0,1));
    CHECK(hmbtn_village_talk_tick(&talk,error,sizeof(error)) && !talk.active && !talk.script.boundary);
    CHECK(hmbtn_event_items_take(inventory,56,0,1,1)==1 && !house.inventory->equipped.id);
    const HmbtnInventory before_rollback=*inventory;
    grant_script(bytes,true);
    CHECK(hmbtn_village_talk_open(&talk,32,bytes,sizeof(bytes),&village,&player,&clock,&registration,&second_ui,error,sizeof(error)));
    CHECK(hmbtn_village_talk_tick(&talk,error,sizeof(error)) && inventory->equipped.id==56);
    inventory->bag[7]=(HmbtnStoredTool){6,2,9};inventory->items[7]=(HmbtnStoredTool){65,1,1};
    inventory->toolbox[63]=(HmbtnStoredTool){16,2,3};inventory->held_item=(HmbtnStoredTool){64,1,1};
    /* Legacy conversation rollback covers item records, not external changes
     * to capacity or supplies. Keep that behavior while changing ownership. */
    inventory->rucksack_level=2;inventory->supplies[0]=17;
    HmbtnInventory after_rollback=before_rollback;after_rollback.rucksack_level=2;after_rollback.supplies[0]=17;
    CHECK(hmbtn_village_talk_tick(&talk,error,sizeof(error)) && talk.script.boundary);
    CHECK(inventory_equal(inventory,&after_rollback) && !house.inventory->equipped.id);

    /* Supplies/hand participate in the same live state across depot actions. */
    uint8_t section[0x2000];feed_zone(section);HmbtnFarmFeed feed={0};
    inventory->supplies[2]=2;player=(HmbtnIntroActor){0};
    CHECK(hmbtn_farm_feed_tick(&feed,&second_ui,&player,section,sizeof(section),HMBTN_PAD_CROSS));
    CHECK(inventory->supplies[2]==1 && house.inventory->held_item.id==0x69);
    CHECK(hmbtn_event_items_count(house.inventory,0x69,1,0)==1);
    player.animation_done=true;CHECK(hmbtn_farm_feed_tick(&feed,&house,&player,section,sizeof(section),0));
    CHECK(hmbtn_farm_feed_tick(&feed,&house,&player,section,sizeof(section),HMBTN_PAD_CROSS));
    player.animation_done=true;CHECK(hmbtn_farm_feed_tick(&feed,&second_ui,&player,section,sizeof(section),0));
    CHECK(!inventory->held_item.id && inventory->supplies[2]==2);

    /* A pending tool animation updates the equipped record exactly on completion. */
    HmbtnFarmAction action;hmbtn_farm_action_init(&action);action.phase=HMBTN_FARM_REFILL;
    inventory->equipped=(HmbtnStoredTool){21,0,7};player.animation_done=false;
    CHECK(hmbtn_farm_action_tick(&action,&player,&house,NULL,NULL,0,0) && inventory->equipped.count==7);
    player.animation_done=true;
    CHECK(hmbtn_farm_action_tick(&action,&player,&second_ui,NULL,NULL,0,0) && house.inventory->equipped.count==100);
    CHECK(!hmbtn_farm_action_tick(&action,&player,&house,NULL,NULL,0,0) && inventory->equipped.count==100);

    /* Item help must not equip the described item, even temporarily. */
    uint8_t record[8192]={0};
    inventory->bag[0]=(HmbtnStoredTool){6,2,3};inventory->supplies[0]=9;
    hmbtn_write_le16(record+0x1800,0x4010);hmbtn_write_le16(record+0x1802,0x4012);
    hmbtn_write_le16(record+0x1804,0xFFFF);
    const HmbtnInventory before_help=*inventory;
    CHECK(hmbtn_house_expand_item_reading(&house,hmbtn_inventory_slot_const(inventory,1),record,sizeof(record)));
    CHECK(hmbtn_read_le16(record+0x1800)==0x43 && hmbtn_read_le16(record+0x1802)==0x49);
    CHECK(inventory_equal(inventory,&before_help));

    /* Every store, including inactive slots, survives UI recreation/rebinding. */
    inventory->held_item=(HmbtnStoredTool){64,1,1};inventory->items[7]=(HmbtnStoredTool){65,3,2};
    inventory->toolbox[63]=(HmbtnStoredTool){16,0,11};inventory->supplies[1]=31;inventory->supplies[3]=47;
    const HmbtnInventory before_ui=*inventory;
    hmbtn_house_interaction_init(&house,&game);hmbtn_farm_menu_init(&menu,hmbtn_game_economy_const(&game));
    CHECK(house.inventory==inventory && inventory_equal(inventory,&before_ui));
    hmbtn_house_interaction_init(&house,&other);
    CHECK(house.inventory==independent && inventory_equal(inventory,&before_ui));
    CHECK(!independent->equipped.id && !independent->held_item.id && independent->toolbox[0].id==1);
    CHECK(!independent->rucksack_level);
    for(unsigned i=0;i<4;++i) CHECK(!independent->supplies[i]);
    puts("Authoritative inventory: shared UI/actions/scripts, rollback, read-only help, initialization and independent state passed.");
    return 0;
}

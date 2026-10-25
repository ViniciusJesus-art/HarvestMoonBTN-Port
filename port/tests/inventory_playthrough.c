/* Existing scene integration API is private until MA-08. Exercise real area
 * loading and morning/save boundaries with installed resources, not fake files. */
#include "scene_preview.c"
#include "sdl_host.h"
#include "inventory_checks.h"

#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Inventory route line %d: %s; %s\n",__LINE__,#x,error);goto fail; } } while(0)

int main(int argc,char **argv)
{
    if(argc!=2) return 2;
    HmbtnHost host={0};HmbtnScenePreview *scene=NULL,*other=NULL;char error[512]={0};
    CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock clock={0,0,2,2,12,0,0};
    scene=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&clock,error,sizeof(error));CHECK(scene);
    HmbtnInventory *inventory=hmbtn_game_inventory(&scene->session->game);
    CHECK(scene->house.inventory==inventory);
    inventory->equipped=(HmbtnStoredTool){21,0,73};
    inventory->held_item=(HmbtnStoredTool){64,0,1};inventory->rucksack_level=1;
    for(unsigned i=0;i<8;++i) {
        inventory->bag[i]=(HmbtnStoredTool){6,(uint16_t)(i%5),(uint16_t)(i+1)};
        inventory->items[i]=(HmbtnStoredTool){65,0,(uint16_t)(i+1)};
    }
    inventory->toolbox[63]=(HmbtnStoredTool){16,0,1};
    for(unsigned i=0;i<4;++i) inventory->supplies[i]=10+i;
    const HmbtnInventory expected=*inventory;

    /* Actual loader used by transitions; includes farmhouse, coop and village. */
    const unsigned areas[]={80,112,128,0,80};
    for(unsigned i=0;i<sizeof(areas)/sizeof(*areas);++i) {
        CHECK(load_area(scene,argv[1],areas[i],error,sizeof(error)));
        scene->intro.area=scene->area;
        CHECK(scene->house.inventory==inventory && inventory_equal(inventory,&expected));
    }
    /* Menu/interface recreation does not reinstall tools or reset supplies. */
    hmbtn_house_interaction_init(&scene->house,&scene->session->game);
    hmbtn_farm_menu_init(&scene->menu,hmbtn_game_economy_const(&scene->session->game));scene->menu.poultry=&scene->session->poultry;
    CHECK(inventory_equal(inventory,&expected));
    CHECK(hmbtn_farm_menu_open(&scene->menu,HMBTN_PAD_TRIANGLE));
    scene->menu.message.phase=HMBTN_MESSAGE_WAIT_END;
    hmbtn_farm_menu_tick(&scene->menu,inventory,HMBTN_PAD_START,0);
    for(unsigned i=0;i<12;++i) hmbtn_farm_menu_tick(&scene->menu,inventory,0,0);
    CHECK(scene->menu.mode==HMBTN_MENU_NONE && inventory_equal(inventory,&expected));

    /* Existing diary/load boundary remains explicit and does not reset stores. */
    scene->house.mode=HMBTN_HOUSE_BED;scene->house.bed_choice=2;
    scene->house.message.phase=HMBTN_MESSAGE_WAIT_END;
    hmbtn_house_interaction_tick(&scene->house,HMBTN_PAD_CROSS,0);
    CHECK(scene->house.boundary==2 && inventory_equal(inventory,&expected));
    CHECK(opening_tick(scene,0,error,sizeof(error)) && !scene->house.boundary);
    CHECK(inventory_equal(inventory,&expected));

    /* Sleep/save request follows the same morning code; save writing is absent. */
    scene->house.mode=HMBTN_HOUSE_BED;scene->house.bed_choice=1;
    scene->house.message.phase=HMBTN_MESSAGE_WAIT_END;
    hmbtn_house_interaction_tick(&scene->house,HMBTN_PAD_CROSS,0);
    CHECK(scene->house.save_requested && scene->house.mode==HMBTN_HOUSE_SLEEP);
    const unsigned day=scene->session->game.clock.day;
    CHECK(morning(scene,true,error,sizeof(error)));
    CHECK(scene->session->game.clock.day==day+1 && scene->session->game.clock.hour==6 && scene->area==80);
    CHECK(!scene->house.save_requested && scene->house.inventory==inventory);
    CHECK(inventory_equal(inventory,&expected));

    /* A separate runtime starts with default stores, independent of the first. */
    other=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&clock,error,sizeof(error));CHECK(other);
    HmbtnGameState defaults;hmbtn_game_state_init(&defaults);
    CHECK(other->house.inventory!=inventory && inventory_equal(other->house.inventory,&defaults.inventory));
    other->house.inventory->equipped=(HmbtnStoredTool){1,0,1};
    CHECK(inventory_equal(inventory,&expected));
    hmbtn_scene_preview_destroy(other);hmbtn_scene_preview_destroy(scene);hmbtn_host_close(&host);
    puts("Inventory persistence across installed areas, recreated interfaces, morning, save/load boundaries and independent scenes passed.");
    return 0;
fail:
    hmbtn_scene_preview_destroy(other);hmbtn_scene_preview_destroy(scene);hmbtn_host_close(&host);return 1;
}

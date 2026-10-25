#ifndef HMBTN_FARM_MENU_UI_H
#define HMBTN_FARM_MENU_UI_H
#include "farm_menu.h"
#include "house_ui.h"
typedef struct HmbtnFarmMenuUI HmbtnFarmMenuUI;
HmbtnFarmMenuUI *hmbtn_farm_menu_ui_create(void);
void hmbtn_farm_menu_ui_destroy(HmbtnFarmMenuUI *ui);
bool hmbtn_farm_menu_ui_draw(HmbtnFarmMenuUI *ui,HmbtnHouseUI *items,HmbtnNewGameUI *shared,
    const HmbtnFarmMenu *menu,const HmbtnHouseInteraction *inventory,const HmbtnFarmAction *action,
    const HmbtnRegistration *registration,const char *root,unsigned tick,uint8_t *pixels,char *error,size_t error_size);
#endif

#ifndef HMBTN_HOUSE_UI_H
#define HMBTN_HOUSE_UI_H
#include "house_interaction.h"
#include "new_game_ui.h"
typedef struct HmbtnHouseUI HmbtnHouseUI;
HmbtnHouseUI *hmbtn_house_ui_create(void);
void hmbtn_house_ui_destroy(HmbtnHouseUI *ui);
bool hmbtn_house_ui_draw(HmbtnHouseUI *ui,HmbtnNewGameUI *shared,
    const HmbtnHouseInteraction *house,const char *root,unsigned tick,uint8_t *pixels,
    char *error,size_t error_size);
#endif

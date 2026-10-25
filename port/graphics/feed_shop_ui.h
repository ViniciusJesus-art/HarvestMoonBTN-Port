#ifndef HMBTN_FEED_SHOP_UI_H
#define HMBTN_FEED_SHOP_UI_H
#include "feed_shop.h"
#include "new_game_ui.h"
bool hmbtn_feed_shop_ui_draw(const HmbtnFeedShop *shop,const HmbtnEconomy *economy,const HmbtnRegistration *registration,
    HmbtnNewGameUI *ui,uint8_t *pixels,unsigned tick,char *error,size_t size);
#endif

#ifndef HMBTN_NEW_GAME_UI_H
#define HMBTN_NEW_GAME_UI_H
#include "new_game.h"
#include "asset_decode.h"

typedef struct HmbtnNewGameUI HmbtnNewGameUI;
HmbtnNewGameUI *hmbtn_new_game_ui_create(const char *root, char *error, size_t error_size);
void hmbtn_new_game_ui_destroy(HmbtnNewGameUI *ui);
bool hmbtn_new_game_ui_load(HmbtnNewGameUI *ui, const char *root, HmbtnNewGame *game,
    char *error, size_t error_size);
bool hmbtn_new_game_ui_draw(HmbtnNewGameUI *ui, const HmbtnNewGame *game,
    unsigned frame, uint8_t *rgba, char *error, size_t error_size);
#endif

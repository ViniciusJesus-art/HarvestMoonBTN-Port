#ifndef HMBTN_NEW_GAME_UI_H
#define HMBTN_NEW_GAME_UI_H
#include "new_game.h"
#include "asset_decode.h"

typedef struct HmbtnNewGameUI HmbtnNewGameUI;
struct HmbtnWorldSprite;
/* Borrow shared icon resources for the original world cursor/shadow (ANI 24). */
void hmbtn_new_game_ui_cursor(const HmbtnNewGameUI *ui, struct HmbtnWorldSprite *sprite,
    const int32_t position[3], unsigned variant);
bool hmbtn_new_game_ui_cue(const HmbtnNewGameUI *ui,struct HmbtnWorldSprite *sprite,
    const int32_t position[3],unsigned icon,unsigned phase,int screen_y);
HmbtnNewGameUI *hmbtn_new_game_ui_create(const char *root, char *error, size_t error_size);
void hmbtn_new_game_ui_destroy(HmbtnNewGameUI *ui);
bool hmbtn_ui_text(HmbtnNewGameUI *ui,uint8_t *rgba,const uint16_t *words,unsigned count,
    int x,int y,int spacing,unsigned palette,char *error,size_t error_size);
bool hmbtn_ui_text_dim(HmbtnNewGameUI *ui,uint8_t *rgba,const uint16_t *words,unsigned count,
    int x,int y,bool dim,char *error,size_t error_size);
bool hmbtn_ui_icon(HmbtnNewGameUI *ui,uint8_t *rgba,unsigned animation,unsigned frame,
    int x,int y,char *error,size_t error_size);
bool hmbtn_new_game_ui_message(HmbtnNewGameUI *ui, const char *root,
    const char *archive, unsigned id, HmbtnMessage *message, char *error, size_t error_size);
bool hmbtn_ui_message_record(HmbtnNewGameUI *ui,const uint8_t *record,size_t size,
    HmbtnMessage *message,char *error,size_t error_size);
bool hmbtn_ui_message_portrait(HmbtnNewGameUI *ui,const HmbtnMessage *message,
    char *error,size_t error_size);
bool hmbtn_new_game_ui_load(HmbtnNewGameUI *ui, const char *root, HmbtnNewGame *game,
    char *error, size_t error_size);
bool hmbtn_new_game_ui_draw(HmbtnNewGameUI *ui, const HmbtnNewGame *game,
    unsigned frame, uint8_t *rgba, char *error, size_t error_size);
#endif

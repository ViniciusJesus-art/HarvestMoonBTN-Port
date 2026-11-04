#ifndef HMBTN_PAUSE_MENU_H
#define HMBTN_PAUSE_MENU_H
#include "preferences.h"
#include "save_library.h"
#include "native_font.h"
typedef enum HmbtnPausePage { HMBTN_PAUSE_HOME,HMBTN_PAUSE_FARMS,HMBTN_PAUSE_SAVES,
    HMBTN_PAUSE_SETTINGS,HMBTN_PAUSE_GRAPHICS,HMBTN_PAUSE_LANGUAGE,HMBTN_PAUSE_SOUND,HMBTN_PAUSE_CONTROLS } HmbtnPausePage;
typedef enum HmbtnPauseAction { HMBTN_PAUSE_NONE,HMBTN_PAUSE_RESUME,HMBTN_PAUSE_LOAD,
    HMBTN_PAUSE_EXIT,HMBTN_PAUSE_CHANGED } HmbtnPauseAction;
typedef struct HmbtnPauseMenu {
    bool open;
    HmbtnNativeFont *font;
    HmbtnPausePage page;
    size_t selected,top,farm;
    int binding;
    HmbtnSaveLibrary library;
    size_t *groups,group_count;
    char *load_path;
    char *root,*slot;
    char error[512];
} HmbtnPauseMenu;
void hmbtn_pause_menu_close(HmbtnPauseMenu *);
void hmbtn_pause_menu_open(HmbtnPauseMenu *,const char *root,const char *slot);
HmbtnPauseAction hmbtn_pause_menu_event(HmbtnPauseMenu *,HmbtnPreferences *,SDL_Renderer *,
    SDL_WindowID,const SDL_Event *);
bool hmbtn_pause_menu_draw(HmbtnPauseMenu *,const HmbtnPreferences *,const HmbtnSession *,SDL_Renderer *);
#endif

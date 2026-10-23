#ifndef HMBTN_APP_ICON_H
#define HMBTN_APP_ICON_H
#include <SDL3/SDL.h>
void hmbtn_app_icon(SDL_Window *window);
/* AppImage per-user integration; packaged DEB/RPM/EXE supply their own links. */
void hmbtn_app_shortcuts(void);
#endif

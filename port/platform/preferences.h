#ifndef HMBTN_PREFERENCES_H
#define HMBTN_PREFERENCES_H
#include <SDL3/SDL.h>
enum { HMBTN_BINDINGS=14 };
typedef struct HmbtnPreferences {
    bool fullscreen,vsync,integer_scale,smooth;
    unsigned window_scale,volume,language; /* language: native menus PT / EN. */
    SDL_Scancode keys[HMBTN_BINDINGS];
} HmbtnPreferences;
extern const uint32_t hmbtn_binding_buttons[HMBTN_BINDINGS];
extern const char *const hmbtn_binding_names[HMBTN_BINDINGS];
void hmbtn_preferences_defaults(HmbtnPreferences *);
bool hmbtn_preferences_load(HmbtnPreferences *,const char *path,char *error,size_t size);
bool hmbtn_preferences_save(const HmbtnPreferences *,const char *path,char *error,size_t size);
bool hmbtn_preferences_apply(const HmbtnPreferences *,SDL_Window *,SDL_Renderer *,char *error,size_t size);
bool hmbtn_preferences_bind(HmbtnPreferences *,unsigned binding,SDL_Scancode);
char *hmbtn_preferences_path(void);
#endif

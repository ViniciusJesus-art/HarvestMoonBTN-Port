#ifndef HMBTN_NATIVE_FONT_H
#define HMBTN_NATIVE_FONT_H
#include <SDL3/SDL.h>
typedef struct HmbtnNativeFont HmbtnNativeFont;
HmbtnNativeFont *hmbtn_native_font_create(SDL_Renderer *);
void hmbtn_native_font_destroy(HmbtnNativeFont *);
float hmbtn_native_font_width(const char *,float size);
/* UTF-8, proportional advances and filtered coverage. y is the top of the
 * font's ascent; max_width clips text with an ellipsis, without splitting UTF-8. */
bool hmbtn_native_font_draw(HmbtnNativeFont *,SDL_Renderer *,const char *,float x,float y,
    float size,float max_width,SDL_Color);
#endif

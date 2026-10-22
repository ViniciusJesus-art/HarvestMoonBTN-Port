#ifndef HMBTN_WINDOW_FRAME_H
#define HMBTN_WINDOW_FRAME_H
#include <stdint.h>
/* Caller supplies width*height*4 RGBA bytes. Original primitive colours are RGB. */
void hmbtn_window_frame_draw(uint8_t *rgba,unsigned width,unsigned height,int x,int y,int w,int h);
#endif

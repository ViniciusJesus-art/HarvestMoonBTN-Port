#include "window_frame.h"
#include <stdio.h>
#include <string.h>
#define CHECK(c) do { if(!(c)) {fprintf(stderr,"Window frame line %d: %s\n",__LINE__,#c);return 1;} }while(0)
int main(void)
{
    uint8_t rgba[32*24*4];memset(rgba,0,sizeof(rgba));
    hmbtn_window_frame_draw(rgba,32,24,2,2,20,18);
    const uint8_t *top=rgba+(4*32+4)*4,*bottom=rgba+(16*32+4)*4;
    /* prim_make_gouraud_rect receives RGB F4DFBD: warm beige, never blue. */
    CHECK(top[0]==247 && top[1]==222 && top[2]==189 && top[3]==255);
    CHECK(bottom[0]>=top[0] && bottom[1]<top[1] && bottom[2]<top[2]);
    CHECK(rgba[0]==0 && rgba[3]==0);
    hmbtn_window_frame_draw(rgba,32,24,-10,-10,20,18); /* Clipped safely. */
    return 0;
}

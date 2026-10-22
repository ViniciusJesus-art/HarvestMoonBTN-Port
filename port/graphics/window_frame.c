#include "window_frame.h"
#include <stddef.h>

static void pixel(uint8_t *rgba,unsigned width,unsigned height,int x,int y,uint32_t rgb)
{
    if (x<0 || y<0 || (unsigned)x>=width || (unsigned)y>=height) return;
    uint8_t *p=rgba+((size_t)y*width+(unsigned)x)*4;
    for(unsigned c=0;c<3;++c) {
        const unsigned value=(rgb>>((2-c)*8))&255;
        p[c]=(uint8_t)((value&248)|(value>>5));
    }
    p[3]=255;
}
static void line(uint8_t *rgba,unsigned width,unsigned height,int x,int y,int end_x,int end_y,uint32_t rgb)
{
    const int dx=end_x>x?1:0,dy=end_y>y?1:0;
    const int count=dx?end_x-x:end_y-y;
    for(int i=0;i<=count;++i) pixel(rgba,width,height,x+i*dx,y+i*dy,rgb);
}
/* window_frame_draw(): original colours, corner pixels and bevel geometry.
 * Primitive colour constants are RGB (prim_make_*), unlike packed BGR555 assets.
 * The vertical Gouraud ramp is quantized to BGR555; GPU dither parity is pending. */
void hmbtn_window_frame_draw(uint8_t *p,unsigned width,unsigned height,int x,int y,int w,int h)
{
    if(w<3 || h<3) return;
    const int right=x+w-2,bottom=y+h-2;
    for(int row=0;row<h-4;++row) {
        uint32_t color=0;
        for(unsigned c=0;c<3;++c) {
            const int a=(0xf4dfbdu>>(c*8))&255, b=(0xf9cf8fu>>(c*8))&255;
            const int value=a+(b-a)*row/(h-4);
            color|=(uint32_t)value<<(c*8);
        }
        line(p,width,height,x+2,y+2+row,x+w-3,y+2+row,color);
    }
    line(p,width,height,x+1,y+1,right-1,y+1,0xfdecd2); line(p,width,height,x+1,y+1,x+1,bottom,0xfdecd2);
    line(p,width,height,right,y+1,right,bottom,0xba8b47); line(p,width,height,x+1,bottom,right-1,bottom,0xba8b47);
    line(p,width,height,x+1,y,right,y,0xfdecd2);line(p,width,height,x,y+1,x,bottom,0xfdecd2);
    line(p,width,height,x+w-1,y+1,x+w-1,bottom,0xba8b47);line(p,width,height,x+1,y+h-1,right,y+h-1,0xba8b47);
    pixel(p,width,height,right,y,0xf8dbaf);pixel(p,width,height,right,y+1,0xdebb84);pixel(p,width,height,x+w-1,y+1,0xc89f60);
    pixel(p,width,height,x+1,bottom,0xe2c08b);pixel(p,width,height,x+1,y+h-1,0xc89f60);pixel(p,width,height,x,bottom,0xf8dbaf);
}

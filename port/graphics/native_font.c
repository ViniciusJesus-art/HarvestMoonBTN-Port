#include "native_font.h"
struct FontGlyph { uint16_t code,x,y,w,h;int16_t left,top;float advance; };
#include "native_font_atlas.inc"
struct HmbtnNativeFont { SDL_Texture *texture; };
static unsigned next(const unsigned char **at)
{
    unsigned c=*(*at)++;
    if(c<128) return c;
    unsigned count=c>=0xc2 && c<=0xdf ? 1 : c>=0xe0 && c<=0xef ? 2 : 0;
    if(!count) return '?';
    unsigned value=c&((1u<<(6-count))-1);
    for(unsigned i=0;i<count;++i) {
        if((**at&0xc0)!=0x80) return '?';
        value=(value<<6)|(*(*at)++&0x3f);
    }
    return value;
}
static const struct FontGlyph *glyph(unsigned code)
{
    size_t low=0,high=SDL_arraysize(font_glyphs);
    while(low<high) { size_t middle=low+(high-low)/2;if(font_glyphs[middle].code<code) low=middle+1;else high=middle; }
    return low<SDL_arraysize(font_glyphs) && font_glyphs[low].code==code ? &font_glyphs[low] : &font_glyphs['?'-32];
}
HmbtnNativeFont *hmbtn_native_font_create(SDL_Renderer *r)
{
    const size_t count=FONT_WIDTH*FONT_HEIGHT;
    uint8_t *pixels=SDL_malloc(count*4);HmbtnNativeFont *font=SDL_calloc(1,sizeof(*font));
    if(!pixels || !font) { SDL_free(pixels);SDL_free(font);return NULL; }
    size_t at=0;
    for(size_t i=0;i<sizeof(font_alpha_runs);i+=2) {
        const unsigned run=font_alpha_runs[i];
        if(!run || at+run>count) { SDL_free(pixels);SDL_free(font);return NULL; }
        for(unsigned n=0;n<run;++n,++at) { pixels[at*4]=pixels[at*4+1]=pixels[at*4+2]=255;pixels[at*4+3]=font_alpha_runs[i+1]; }
    }
    if(at==count) font->texture=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,FONT_WIDTH,FONT_HEIGHT);
    bool ok=font->texture && SDL_UpdateTexture(font->texture,NULL,pixels,FONT_WIDTH*4)
        && SDL_SetTextureBlendMode(font->texture,SDL_BLENDMODE_BLEND) && SDL_SetTextureScaleMode(font->texture,SDL_SCALEMODE_LINEAR);
    SDL_free(pixels);
    if(!ok) { hmbtn_native_font_destroy(font);return NULL; }
    return font;
}
void hmbtn_native_font_destroy(HmbtnNativeFont *font)
{ if(font) { SDL_DestroyTexture(font->texture);SDL_free(font); } }
float hmbtn_native_font_width(const char *text,float size)
{
    float width=0;const unsigned char *at=(const unsigned char *)text;
    while(*at) width+=glyph(next(&at))->advance;
    return width*size/FONT_SIZE;
}
static bool draw_glyph(HmbtnNativeFont *font,SDL_Renderer *r,const struct FontGlyph *g,float x,float baseline,float scale)
{
    if(!g->w || !g->h) return true;
    const SDL_FRect source={(float)g->x,(float)g->y,(float)g->w,(float)g->h};
    const SDL_FRect destination={x+g->left*scale,baseline+g->top*scale,g->w*scale,g->h*scale};
    return SDL_RenderTexture(r,font->texture,&source,&destination);
}
bool hmbtn_native_font_draw(HmbtnNativeFont *font,SDL_Renderer *r,const char *text,float x,float y,float size,float max_width,SDL_Color color)
{
    if(!font || size<=0) return false;
    if(!SDL_SetTextureColorMod(font->texture,color.r,color.g,color.b) || !SDL_SetTextureAlphaMod(font->texture,color.a)) return false;
    const float scale=size/FONT_SIZE,baseline=y+FONT_ASCENT*scale,start=x;
    const struct FontGlyph *ellipsis=glyph(0x2026);
    const bool truncate=max_width>0 && hmbtn_native_font_width(text,size)>max_width;
    const unsigned char *at=(const unsigned char *)text;
    while(*at) {
        const struct FontGlyph *g=glyph(next(&at));
        if(truncate && x-start+(g->advance+ellipsis->advance)*scale>max_width)
            return draw_glyph(font,r,ellipsis,x,baseline,scale);
        if(!draw_glyph(font,r,g,x,baseline,scale)) return false;
        x+=g->advance*scale;
    }
    return true;
}

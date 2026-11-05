#include "pause_menu.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#define TEXT(pt,en) (p->language ? (en) : (pt))
enum { MENU_HEIGHT=720 };
typedef struct MenuLayout { int width,visible;float left,right,row_y,row_h; } MenuLayout;
static bool records(const HmbtnPauseMenu *m)
{ return m->page==HMBTN_PAUSE_FARMS || m->page==HMBTN_PAUSE_SAVES; }
static unsigned visible_rows(const HmbtnPauseMenu *m) { return records(m) ? 5u : 8u; }
static MenuLayout layout(const HmbtnPauseMenu *m,SDL_Renderer *r)
{
    int width=960,height=720;SDL_GetRenderOutputSize(r,&width,&height);
    const int logical_width=height>0 ? (int)((int64_t)width*MENU_HEIGHT/height) : 960;
    const float left=64,right=SDL_min((float)logical_width-48,left+(m->page==HMBTN_PAUSE_HOME ? 360 : records(m) ? 600 : 520));
    return (MenuLayout){logical_width,(int)visible_rows(m),left,right,218,records(m) ? 86.0f : m->page==HMBTN_PAUSE_HOME ? 64.0f : m->page==HMBTN_PAUSE_SOUND ? 84.0f : 54.0f};
}
static size_t row_count(const HmbtnPauseMenu *m)
{
    if(m->page==HMBTN_PAUSE_HOME || m->page==HMBTN_PAUSE_SETTINGS) return 4;
    if(m->page==HMBTN_PAUSE_GRAPHICS) return 5;
    if(m->page==HMBTN_PAUSE_LANGUAGE) return 2;
    if(m->page==HMBTN_PAUSE_SOUND) return 1;
    if(m->page==HMBTN_PAUSE_CONTROLS) return HMBTN_BINDINGS+1;
    if(m->page==HMBTN_PAUSE_FARMS) return m->group_count;
    size_t count=0;
    for(size_t i=0;i<m->library.count;++i) {
        if(hmbtn_save_same_farm(&m->library.entries[i],&m->library.entries[m->farm])) ++count;
    }
    return count;
}
static size_t entry_index(const HmbtnPauseMenu *m,size_t row)
{
    if(m->page==HMBTN_PAUSE_FARMS) return m->groups[row];
    for(size_t i=0;i<m->library.count;++i) {
        if(hmbtn_save_same_farm(&m->library.entries[i],&m->library.entries[m->farm])) {
            if(!row--) return i;
        }
    }
    return 0;
}
void hmbtn_pause_menu_close(HmbtnPauseMenu *m)
{
    hmbtn_native_font_destroy(m->font);
    SDL_DestroyTexture(m->gradient);
    hmbtn_save_library_close(&m->library);SDL_free(m->load_path);SDL_free(m->groups);
    SDL_free(m->root);SDL_free(m->slot);*m=(HmbtnPauseMenu){.binding=-1};
}
void hmbtn_pause_menu_open(HmbtnPauseMenu *m,const char *root,const char *slot)
{
    HmbtnNativeFont *font=m->font;SDL_Texture *gradient=m->gradient;
    m->font=NULL;m->gradient=NULL;
    hmbtn_pause_menu_close(m);m->font=font;m->gradient=gradient;m->open=true;
    m->root=root ? SDL_strdup(root) : NULL;m->slot=slot ? SDL_strdup(slot) : NULL;
}
static void refresh(HmbtnPauseMenu *m)
{
    SDL_free(m->groups);m->groups=NULL;m->group_count=0;m->error[0]=0;
    hmbtn_save_library_scan(&m->library,m->root,m->slot,m->error,sizeof(m->error));
    if(m->library.count) m->groups=SDL_malloc(m->library.count*sizeof(*m->groups));
    if(m->library.count && !m->groups) { SDL_strlcpy(m->error,"Cannot allocate farm list",sizeof(m->error));return; }
    for(size_t i=0;i<m->library.count;++i) {
        bool found=false;
        for(size_t j=0;j<m->group_count && !found;++j)
            found=hmbtn_save_same_farm(&m->library.entries[i],&m->library.entries[m->groups[j]]);
        if(!found) m->groups[m->group_count++]=i;
    }
}
static void page(HmbtnPauseMenu *m,HmbtnPausePage value)
{ m->page=value;m->selected=m->top=0;m->binding=-1; }
static void back(HmbtnPauseMenu *m)
{
    page(m,m->page==HMBTN_PAUSE_SAVES ? HMBTN_PAUSE_FARMS
        : m->page>=HMBTN_PAUSE_GRAPHICS ? HMBTN_PAUSE_SETTINGS : HMBTN_PAUSE_HOME);
}
static void move(HmbtnPauseMenu *m,int direction)
{
    size_t count=row_count(m);if(!count) return;
    m->selected=direction<0 ? (m->selected ? m->selected-1 : count-1) : (m->selected+1)%count;
    if(m->selected<m->top) m->top=m->selected;
    if(m->selected>=m->top+visible_rows(m)) m->top=m->selected-visible_rows(m)+1;
}
static HmbtnPauseAction activate(HmbtnPauseMenu *m,HmbtnPreferences *p,int direction)
{
    if(!row_count(m)) return HMBTN_PAUSE_NONE;
    const unsigned selected=(unsigned)m->selected;
    switch(m->page) {
    case HMBTN_PAUSE_HOME:
        if(!selected) { m->open=false;return HMBTN_PAUSE_RESUME; }
        if(selected==1) { refresh(m);page(m,HMBTN_PAUSE_FARMS); }
        if(selected==2) page(m,HMBTN_PAUSE_SETTINGS);
        if(selected==3) return HMBTN_PAUSE_EXIT;
        break;
    case HMBTN_PAUSE_FARMS:m->farm=entry_index(m,selected);page(m,HMBTN_PAUSE_SAVES);break;
    case HMBTN_PAUSE_SAVES:
        SDL_free(m->load_path);m->load_path=SDL_strdup(m->library.entries[entry_index(m,selected)].path);
        if(m->load_path) return HMBTN_PAUSE_LOAD;
        SDL_snprintf(m->error,sizeof(m->error),"Not enough memory");break;
    case HMBTN_PAUSE_SETTINGS:page(m,(HmbtnPausePage)(HMBTN_PAUSE_GRAPHICS+selected));break;
    case HMBTN_PAUSE_GRAPHICS:
        if(selected==0) p->fullscreen=!p->fullscreen;
        if(selected==1) p->window_scale=direction<0 ? (p->window_scale==2 ? 5 : p->window_scale-1) : (p->window_scale==5 ? 2 : p->window_scale+1);
        if(selected==2) p->vsync=!p->vsync;
        if(selected==3) p->integer_scale=!p->integer_scale;
        if(selected==4) p->smooth=!p->smooth;
        return HMBTN_PAUSE_CHANGED;
    case HMBTN_PAUSE_LANGUAGE:p->language=selected;return HMBTN_PAUSE_CHANGED;
    case HMBTN_PAUSE_SOUND:
        p->volume=direction<0 ? (p->volume>=5 ? p->volume-5 : 0) : (p->volume<=95 ? p->volume+5 : 100);
        return HMBTN_PAUSE_CHANGED;
    case HMBTN_PAUSE_CONTROLS:
        if(selected<HMBTN_BINDINGS) m->binding=(int)selected;
        else { HmbtnPreferences defaults;hmbtn_preferences_defaults(&defaults);memcpy(p->keys,defaults.keys,sizeof(p->keys));return HMBTN_PAUSE_CHANGED; }
        break;
    }
    return HMBTN_PAUSE_NONE;
}
HmbtnPauseAction hmbtn_pause_menu_event(HmbtnPauseMenu *m,HmbtnPreferences *p,SDL_Renderer *renderer,
    SDL_WindowID window,const SDL_Event *e)
{
    if(!m->open) return HMBTN_PAUSE_NONE;
    if(e->type==SDL_EVENT_KEY_DOWN && e->key.windowID==window && !e->key.repeat) {
        const SDL_Scancode key=e->key.scancode;
        if(key==SDL_SCANCODE_ESCAPE) { m->open=false;return HMBTN_PAUSE_RESUME; }
        if(m->binding>=0) {
            if(hmbtn_preferences_bind(p,(unsigned)m->binding,key)) { m->binding=-1;return HMBTN_PAUSE_CHANGED; }
            return HMBTN_PAUSE_NONE;
        }
        if(key==SDL_SCANCODE_UP) move(m,-1);
        else if(key==SDL_SCANCODE_DOWN) move(m,1);
        else if(key==SDL_SCANCODE_PAGEUP || key==SDL_SCANCODE_PAGEDOWN) for(unsigned i=0;i<visible_rows(m);++i) move(m,key==SDL_SCANCODE_PAGEUP ? -1 : 1);
        else if(key==SDL_SCANCODE_BACKSPACE || key==p->keys[7]) back(m);
        else if(key==SDL_SCANCODE_LEFT || key==SDL_SCANCODE_RIGHT) {
            if(m->page==HMBTN_PAUSE_GRAPHICS || m->page==HMBTN_PAUSE_SOUND) return activate(m,p,key==SDL_SCANCODE_LEFT ? -1 : 1);
        } else if(key==SDL_SCANCODE_RETURN || key==SDL_SCANCODE_KP_ENTER || key==p->keys[6]) return activate(m,p,1);
    } else if(e->type==SDL_EVENT_GAMEPAD_BUTTON_DOWN && m->binding<0) {
        const unsigned b=e->gbutton.button;
        if(b==SDL_GAMEPAD_BUTTON_DPAD_UP || b==SDL_GAMEPAD_BUTTON_DPAD_DOWN) move(m,b==SDL_GAMEPAD_BUTTON_DPAD_UP ? -1 : 1);
        else if(b==SDL_GAMEPAD_BUTTON_EAST || b==SDL_GAMEPAD_BUTTON_BACK) back(m);
        else if(b==SDL_GAMEPAD_BUTTON_SOUTH) return activate(m,p,1);
        else if((b==SDL_GAMEPAD_BUTTON_DPAD_LEFT || b==SDL_GAMEPAD_BUTTON_DPAD_RIGHT)
                && (m->page==HMBTN_PAUSE_SOUND || m->page==HMBTN_PAUSE_GRAPHICS)) return activate(m,p,b==SDL_GAMEPAD_BUTTON_DPAD_LEFT ? -1 : 1);
    } else if(e->type==SDL_EVENT_MOUSE_WHEEL && e->wheel.windowID==window && m->binding<0) {
        if(e->wheel.y) move(m,e->wheel.y>0 ? -1 : 1);
    } else if((e->type==SDL_EVENT_MOUSE_MOTION || e->type==SDL_EVENT_MOUSE_BUTTON_DOWN) && m->binding<0) {
        if((e->type==SDL_EVENT_MOUSE_MOTION ? e->motion.windowID : e->button.windowID)!=window) return HMBTN_PAUSE_NONE;
        SDL_Event converted=*e;int w,h;SDL_RendererLogicalPresentation mode;
        SDL_GetRenderLogicalPresentation(renderer,&w,&h,&mode);
        const MenuLayout l=layout(m,renderer);
        SDL_SetRenderLogicalPresentation(renderer,l.width,MENU_HEIGHT,SDL_LOGICAL_PRESENTATION_STRETCH);
        SDL_ConvertEventToRenderCoordinates(renderer,&converted);
        SDL_SetRenderLogicalPresentation(renderer,w,h,mode);
        float x=e->type==SDL_EVENT_MOUSE_MOTION ? converted.motion.x : converted.button.x;
        float y=e->type==SDL_EVENT_MOUSE_MOTION ? converted.motion.y : converted.button.y;
        const bool click=e->type==SDL_EVENT_MOUSE_BUTTON_DOWN && e->button.button==SDL_BUTTON_LEFT;
        if(click && m->page!=HMBTN_PAUSE_HOME && x>=l.left && x<l.left+112 && y>=76 && y<116) { back(m);return HMBTN_PAUSE_NONE; }
        if(x>=l.left && x<l.right && y>=l.row_y && y<l.row_y+l.visible*l.row_h) {
            size_t row=m->top+(size_t)((y-l.row_y)/l.row_h);
            if(row<row_count(m)) {
                m->selected=row;
                if(click && m->page==HMBTN_PAUSE_SOUND) {
                    p->volume=(unsigned)SDL_clamp((int)((x-l.left)*100/(l.right-l.left)),0,100);
                    return HMBTN_PAUSE_CHANGED;
                }
                if(click) return activate(m,p,1);
            }
        }
    }
    return HMBTN_PAUSE_NONE;
}
static bool rect(SDL_Renderer *r,float x,float y,float w,float h,Uint8 alpha)
{ return SDL_SetRenderDrawColor(r,255,255,255,alpha) && SDL_RenderFillRect(r,&(SDL_FRect){x,y,w,h}); }
static bool text(HmbtnPauseMenu *m,SDL_Renderer *r,float x,float y,float size,float width,const char *label,Uint8 alpha)
{ return hmbtn_native_font_draw(m->font,r,label,x,y,size,width,(SDL_Color){255,255,255,alpha}); }
static bool keycap(SDL_Renderer *r,float x,float y,float width,bool selected)
{
    /* Rounded key surface with a transparent outer edge, including on the
     * software renderer. No pixel-font framing or scaled PS1 UI elements. */
    enum { STEPS=8,POINTS=4*(STEPS+1) };
    SDL_Vertex vertices[1+POINTS*2];int indices[POINTS*9];const float radius=6,height=34;
    const SDL_FColor color={0.02f,0.025f,0.035f,selected ? 0.44f : 0.29f};
    vertices[0]=(SDL_Vertex){{x+width/2,y+height/2},color,{0,0}};
    for(unsigned i=0;i<POINTS;++i) {
        unsigned corner=i/(STEPS+1),step=i%(STEPS+1);
        const float cx=x+(corner==0 || corner==3 ? radius : width-radius);
        const float cy=y+(corner<2 ? radius : height-radius);
        const float angle=3.14159265f+(corner+(float)step/STEPS)*1.57079633f;
        for(unsigned ring=0;ring<2;++ring) {
            float rr=radius+(ring ? 0.65f : -0.65f);SDL_FColor c=color;if(ring) c.a=0;
            vertices[1+i+ring*POINTS]=(SDL_Vertex){{cx+cosf(angle)*rr,cy+sinf(angle)*rr},c,{0,0}};
        }
        int a=1+(int)i,b=1+(int)((i+1)%POINTS),out=POINTS;
        const int triangles[9]={0,a,b,a,a+out,b+out,a,b+out,b};
        memcpy(indices+i*9,triangles,sizeof(triangles));
    }
    return SDL_RenderGeometry(r,NULL,vertices,SDL_arraysize(vertices),indices,SDL_arraysize(indices));
}
static const char *heading(const HmbtnPauseMenu *m,const HmbtnPreferences *p)
{
    switch(m->page) {
    case HMBTN_PAUSE_HOME:return TEXT("Pausa","Paused");
    case HMBTN_PAUSE_FARMS:return TEXT("Carregar jogo","Load game");
    case HMBTN_PAUSE_SAVES:return m->library.entries[m->farm].farm[0] ? m->library.entries[m->farm].farm : TEXT("Histórico","History");
    case HMBTN_PAUSE_SETTINGS:return TEXT("Configurações","Settings");
    case HMBTN_PAUSE_GRAPHICS:return TEXT("Gráficos","Graphics");
    case HMBTN_PAUSE_LANGUAGE:return TEXT("Idioma","Language");
    case HMBTN_PAUSE_SOUND:return TEXT("Som","Sound");
    case HMBTN_PAUSE_CONTROLS:return TEXT("Controles","Controls");
    }
    return "";
}
static const char *key_name(SDL_Scancode key,const HmbtnPreferences *p)
{
    switch(key) {
    case SDL_SCANCODE_RETURN:return "Enter";
    case SDL_SCANCODE_KP_ENTER:return "Num Enter";
    case SDL_SCANCODE_SPACE:return TEXT("Espaço","Space");
    case SDL_SCANCODE_UP:return "↑";case SDL_SCANCODE_DOWN:return "↓";
    case SDL_SCANCODE_LEFT:return "←";case SDL_SCANCODE_RIGHT:return "→";
    default:return SDL_GetScancodeName(key);
    }
}
static void labels(const HmbtnPauseMenu *m,const HmbtnPreferences *p,size_t row,char name[128],char value[128],char detail[192])
{
    name[0]=value[0]=detail[0]=0;
    if(m->page==HMBTN_PAUSE_HOME) {
        const char *pt[4]={"Continuar","Carregar jogo","Configurações","Sair do jogo"};
        const char *en[4]={"Continue","Load game","Settings","Exit game"};
        SDL_strlcpy(name,p->language ? en[row] : pt[row],128);
    } else if(records(m)) {
        const HmbtnSaveEntry *e=&m->library.entries[entry_index(m,row)];
        if(m->page==HMBTN_PAUSE_FARMS) {
            size_t count=0;for(size_t i=0;i<m->library.count;++i) count+=hmbtn_save_same_farm(e,&m->library.entries[i]);
            SDL_strlcpy(name,e->farm[0] ? e->farm : TEXT("Fazenda","Farm"),128);
            SDL_snprintf(value,128,"%s · %llu saves",e->player[0] ? e->player : TEXT("Jogador","Player"),(unsigned long long)count);
        } else {
            SDL_DateTime date={0};SDL_TimeToDateTime(e->modified,&date,true);
            if(p->language) SDL_snprintf(name,128,"%04d-%02d-%02d   %02d:%02d",date.year,date.month,date.day,date.hour,date.minute);
            else SDL_snprintf(name,128,"%02d/%02d/%04d   %02d:%02d",date.day,date.month,date.year,date.hour,date.minute);
            SDL_snprintf(value,128,"%s %u",TEXT("Ano","Year"),e->clock.year+1);
        }
        const char *seasons_pt[4]={"Primavera","Verão","Outono","Inverno"},*seasons_en[4]={"Spring","Summer","Autumn","Winter"};
        SDL_snprintf(detail,192,"%s %u · %02u:%02u · %u G · %u Power Berries",p->language ? seasons_en[e->clock.season] : seasons_pt[e->clock.season],
            e->clock.day+1,e->clock.hour,e->clock.minute,e->gold,e->berries);
    } else if(m->page==HMBTN_PAUSE_SETTINGS) {
        const char *pt[4]={"Gráficos","Idioma","Som","Controles"},*en[4]={"Graphics","Language","Sound","Controls"};
        SDL_strlcpy(name,p->language ? en[row] : pt[row],128);
    } else if(m->page==HMBTN_PAUSE_GRAPHICS) {
        const char *pt[5]={"Tela cheia","Janela","VSync","Escala","Filtro"},*en[5]={"Fullscreen","Window","VSync","Scaling","Filter"};
        SDL_strlcpy(name,p->language ? en[row] : pt[row],128);
        if(row==0) SDL_strlcpy(value,p->fullscreen ? TEXT("Ligado","On") : TEXT("Desligado","Off"),128);
        if(row==1) SDL_snprintf(value,128,"%u × %u",320*p->window_scale,240*p->window_scale);
        if(row==2) SDL_strlcpy(value,p->vsync ? TEXT("Ligado","On") : TEXT("Desligado","Off"),128);
        if(row==3) SDL_strlcpy(value,p->integer_scale ? TEXT("Inteira","Integer") : TEXT("Ajustar","Fit"),128);
        if(row==4) SDL_strlcpy(value,p->smooth ? TEXT("Suave","Smooth") : "Pixel",128);
    } else if(m->page==HMBTN_PAUSE_LANGUAGE) {
        SDL_strlcpy(name,row ? "English" : "Português",128);
        if(row==p->language) SDL_strlcpy(value,"•",128);
    } else if(m->page==HMBTN_PAUSE_SOUND) {
        SDL_strlcpy(name,TEXT("Volume geral","Master volume"),128);SDL_snprintf(value,128,"%u%%",p->volume);
    } else if(m->page==HMBTN_PAUSE_CONTROLS) {
        if(row==HMBTN_BINDINGS) SDL_strlcpy(name,TEXT("Restaurar padrão","Restore defaults"),128);
        else {
            const char *pt[HMBTN_BINDINGS]={"Start","Select","Cima","Baixo","Esquerda","Direita","Cross","Circle","Square","Triangle","L1","R1","L2","R2"};
            const char *en[HMBTN_BINDINGS]={"Start","Select","Up","Down","Left","Right","Cross","Circle","Square","Triangle","L1","R1","L2","R2"};
            SDL_strlcpy(name,p->language ? en[row] : pt[row],128);
            SDL_strlcpy(value,m->binding==(int)row ? "…" : key_name(p->keys[row],p),128);
        }
    }
}
static bool prepare_gradient(HmbtnPauseMenu *m,SDL_Renderer *r)
{
    if(m->gradient) return true;
    enum { COLUMNS=1024 };Uint8 pixels[COLUMNS*4];
    for(unsigned x=0;x<COLUMNS;++x) {
        const float t=SDL_min((float)x/((COLUMNS-1)*0.96f),1.0f);
        pixels[x*4]=5;pixels[x*4+1]=7;pixels[x*4+2]=11;
        pixels[x*4+3]=(Uint8)(246*(1-t*t*(3-2*t)));
    }
    SDL_Texture *texture=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,COLUMNS,1);
    if(!texture || !SDL_UpdateTexture(texture,NULL,pixels,sizeof(pixels))
            || !SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND)
            /* Alpha already has 1024 smooth samples. Nearest avoids a costly
             * full-window bilinear pass on the software fallback. */
            || !SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_NEAREST)) { SDL_DestroyTexture(texture);return false; }
    m->gradient=texture;return true;
}
bool hmbtn_pause_menu_draw(HmbtnPauseMenu *m,const HmbtnPreferences *p,const HmbtnSession *s,SDL_Renderer *r)
{
    (void)s;
    if(!m->font) m->font=hmbtn_native_font_create(r);
    if(!m->font || !prepare_gradient(m,r)) return false;
    const MenuLayout l=layout(m,r);int width,height;SDL_RendererLogicalPresentation mode;SDL_BlendMode blend;
    if(!SDL_GetRenderLogicalPresentation(r,&width,&height,&mode) || !SDL_GetRenderDrawBlendMode(r,&blend)
        || !SDL_SetRenderLogicalPresentation(r,l.width,MENU_HEIGHT,SDL_LOGICAL_PRESENTATION_STRETCH)) return false;
    /* One continuous image avoids truncated/gapped one-unit rectangles when
     * the software renderer rounds fractional window scales to whole pixels. */
    const SDL_FRect backdrop={0,0,(float)l.width,MENU_HEIGHT};
    bool ok=SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND)
        && SDL_RenderTexture(r,m->gradient,NULL,&backdrop);
    if(m->page!=HMBTN_PAUSE_HOME) ok=ok && text(m,r,l.left,81,21,120,TEXT("←  Voltar","←  Back"),175);
    ok=ok && text(m,r,l.left,137,40,l.right-l.left,heading(m,p),255);
    const size_t count=row_count(m);
    for(size_t i=m->top;i<count && i<m->top+(size_t)l.visible && ok;++i) {
        const float y=l.row_y+(float)(i-m->top)*l.row_h;char name[128],value[128],detail[192];labels(m,p,i,name,value,detail);
        const bool selected=i==m->selected;const float size=m->page==HMBTN_PAUSE_HOME ? 29.0f : records(m) ? 25.0f : 23.0f;
        float value_width=hmbtn_native_font_width(value,20);value_width=SDL_min(value_width,(l.right-l.left)*0.48f);
        const bool binding=m->page==HMBTN_PAUSE_CONTROLS && value[0];
        const float cap_width=SDL_max(value_width+24,48);
        if(selected) ok=rect(r,l.left-20,y+15,2,22,245);
        if(m->page==HMBTN_PAUSE_CONTROLS) ok=ok && rect(r,l.left,y+l.row_h-2,l.right-l.left,1,18);
        ok=ok && text(m,r,l.left,y+8,size,l.right-l.left-(value[0] ? value_width+40 : 0),name,selected ? 255 : 210);
        if(binding) ok=ok && keycap(r,l.right-cap_width,y+5,cap_width,selected);
        if(value[0]) ok=ok && text(m,r,l.right-value_width-(binding ? 12 : 0),y+11,20,value_width,value,selected ? 255 : 230);
        if(detail[0]) ok=ok && text(m,r,l.left,y+43,17,l.right-l.left,detail,selected ? 205 : 185);
        if(m->page==HMBTN_PAUSE_SOUND) {
            const float at=l.left+(l.right-l.left)*p->volume/100;
            ok=ok && rect(r,l.left,y+62,l.right-l.left,2,58) && rect(r,l.left,y+62,at-l.left,2,235)
                && rect(r,at-2,y+55,4,16,255);
        }
    }
    if(!count) ok=ok && text(m,r,l.left,l.row_y+12,22,l.right-l.left,TEXT("Nenhum save encontrado","No saves found"),175);
    if(count>(size_t)l.visible) {
        const float track=l.visible*l.row_h,thumb=SDL_max(24.0f,track*l.visible/(float)count);
        const float y=l.row_y+(track-thumb)*(float)m->top/(float)(count-l.visible);
        ok=ok && rect(r,l.right+22,l.row_y,2,track,22) && rect(r,l.right+22,y,2,thumb,145);
    }
    if(m->error[0]) ok=ok && hmbtn_native_font_draw(m->font,r,m->error,l.left,680,17,l.right-l.left,(SDL_Color){255,175,175,255});
    bool restored=SDL_SetRenderDrawBlendMode(r,blend);
    return SDL_SetRenderLogicalPresentation(r,width,height,mode) && restored && ok;
}

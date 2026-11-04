#include "preferences.h"
#include "pad_buttons.h"
#include <stdio.h>
#include <string.h>
const uint32_t hmbtn_binding_buttons[HMBTN_BINDINGS]={HMBTN_PAD_START,HMBTN_PAD_SELECT,
    HMBTN_PAD_UP,HMBTN_PAD_DOWN,HMBTN_PAD_LEFT,HMBTN_PAD_RIGHT,HMBTN_PAD_CROSS,HMBTN_PAD_CIRCLE,
    HMBTN_PAD_SQUARE,HMBTN_PAD_TRIANGLE,HMBTN_PAD_L1,HMBTN_PAD_R1,HMBTN_PAD_L2,HMBTN_PAD_R2};
const char *const hmbtn_binding_names[HMBTN_BINDINGS]={"Start","Select","Cima / Up","Baixo / Down","Esquerda / Left","Direita / Right",
    "Cross","Circle","Square","Triangle","L1","R1","L2","R2"};
void hmbtn_preferences_defaults(HmbtnPreferences *p)
{
    *p=(HmbtnPreferences){.vsync=true,.integer_scale=true,.window_scale=3,.volume=100};
    const SDL_Scancode keys[HMBTN_BINDINGS]={SDL_SCANCODE_RETURN,SDL_SCANCODE_TAB,SDL_SCANCODE_UP,SDL_SCANCODE_DOWN,
        SDL_SCANCODE_LEFT,SDL_SCANCODE_RIGHT,SDL_SCANCODE_Z,SDL_SCANCODE_X,SDL_SCANCODE_C,SDL_SCANCODE_V,
        SDL_SCANCODE_Q,SDL_SCANCODE_E,SDL_SCANCODE_1,SDL_SCANCODE_2};
    memcpy(p->keys,keys,sizeof(keys));
}
bool hmbtn_preferences_bind(HmbtnPreferences *p,unsigned binding,SDL_Scancode key)
{
    if(binding>=HMBTN_BINDINGS || key<=SDL_SCANCODE_UNKNOWN || key>=SDL_SCANCODE_COUNT
            || key==SDL_SCANCODE_ESCAPE || key==SDL_SCANCODE_F11) return false;
    for(unsigned i=0;i<HMBTN_BINDINGS;++i) if(i!=binding && p->keys[i]==key) p->keys[i]=p->keys[binding];
    p->keys[binding]=key;return true;
}
char *hmbtn_preferences_path(void)
{
    char *base=SDL_GetPrefPath("hmbtn","HarvestMoonBTN"),*path=NULL;
    if(base) SDL_asprintf(&path,"%ssettings.ini",base);
    SDL_free(base);return path;
}
bool hmbtn_preferences_load(HmbtnPreferences *p,const char *path,char *error,size_t size)
{
    hmbtn_preferences_defaults(p);SDL_PathInfo info;
    if(!path || !SDL_GetPathInfo(path,&info)) return true;
    char text[1024];SDL_IOStream *file=SDL_IOFromFile(path,"rb");
    size_t n=file ? SDL_ReadIO(file,text,sizeof(text)-1) : 0;
    if(file) SDL_CloseIO(file);
    text[n]=0;HmbtnPreferences loaded=*p;unsigned v,f,vs,in,sm;
    int consumed=0;
    bool ok=n>0 && info.size<sizeof(text) && sscanf(text,"HMBTN_SETTINGS %u %u %u %u %u %u %u %u%n",
        &v,&f,&vs,&in,&sm,&loaded.window_scale,&loaded.volume,&loaded.language,&consumed)==8
        && v==1 && f<=1 && vs<=1 && in<=1 && sm<=1 && loaded.window_scale>=2 && loaded.window_scale<=5
        && loaded.volume<=100 && loaded.language<=1;
    for(unsigned i=0;i<HMBTN_BINDINGS && ok;++i) {
        unsigned key;int used=0;
        ok=sscanf(text+consumed," %u%n",&key,&used)==1 && key>0 && key<SDL_SCANCODE_COUNT
            && key!=SDL_SCANCODE_ESCAPE && key!=SDL_SCANCODE_F11;
        if(ok) { loaded.keys[i]=(SDL_Scancode)key;consumed+=used; }
        for(unsigned j=0;j<i && ok;++j) if(loaded.keys[j]==loaded.keys[i]) ok=false;
    }
    if(!ok) { snprintf(error,size,"Invalid settings file; defaults restored");return false; }
    loaded.fullscreen=f!=0;loaded.vsync=vs!=0;loaded.integer_scale=in!=0;loaded.smooth=sm!=0;*p=loaded;return true;
}
bool hmbtn_preferences_save(const HmbtnPreferences *p,const char *path,char *error,size_t size)
{
    char text[1024];int at=SDL_snprintf(text,sizeof(text),"HMBTN_SETTINGS 1 %u %u %u %u %u %u %u\n",
        p->fullscreen,p->vsync,p->integer_scale,p->smooth,p->window_scale,p->volume,p->language);
    for(unsigned i=0;i<HMBTN_BINDINGS;++i) at+=SDL_snprintf(text+at,sizeof(text)-(size_t)at,"%u ",(unsigned)p->keys[i]);
    text[at++]='\n';char *temp=NULL;if(path) SDL_asprintf(&temp,"%s.tmp",path);
    SDL_IOStream *out=temp ? SDL_IOFromFile(temp,"wb") : NULL;
    bool ok=out && SDL_WriteIO(out,text,(size_t)at)==(size_t)at && SDL_FlushIO(out);
    if(out && !SDL_CloseIO(out)) ok=false;
    if(ok) ok=SDL_RenamePath(temp,path);
    if(!ok) { snprintf(error,size,"Cannot save settings: %s",SDL_GetError());if(temp) SDL_RemovePath(temp); }
    SDL_free(temp);return ok;
}
bool hmbtn_preferences_apply(const HmbtnPreferences *p,SDL_Window *window,SDL_Renderer *renderer,char *error,size_t size)
{
    const bool fullscreen=(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN)!=0;
    bool ok=fullscreen==p->fullscreen || SDL_SetWindowFullscreen(window,p->fullscreen);
    if(ok && !p->fullscreen) ok=SDL_SetWindowSize(window,320*(int)p->window_scale,240*(int)p->window_scale);
    if(ok) ok=SDL_SetRenderLogicalPresentation(renderer,320,240,p->integer_scale ? SDL_LOGICAL_PRESENTATION_INTEGER_SCALE : SDL_LOGICAL_PRESENTATION_LETTERBOX);
    int vsync=0;SDL_GetRenderVSync(renderer,&vsync);
    if(ok && vsync!=(int)p->vsync) ok=SDL_SetRenderVSync(renderer,p->vsync ? 1 : 0);
    if(!ok) snprintf(error,size,"Cannot apply display setting: %s",SDL_GetError());
    return ok;
}

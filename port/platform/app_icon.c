#include "app_icon.h"
#include <stdio.h>
#ifndef _WIN32
#include <sys/stat.h>
#endif
#include "app_icon.inc"
void hmbtn_app_icon(SDL_Window *window)
{
    SDL_Surface *icon=SDL_CreateSurfaceFrom(48,48,SDL_PIXELFORMAT_RGBA32,(void*)app_icon_rgba,48*4);
    if(icon) { SDL_SetWindowIcon(window,icon);SDL_DestroySurface(icon); }
}
void hmbtn_app_shortcuts(void)
{
#ifndef _WIN32
    const char *image=SDL_getenv("APPIMAGE");
    if(!image || image[0]!='/') return;
    char *base=SDL_GetPrefPath("hmbtn","HarvestMoonBTN");if(!base) return;
    char *installed=NULL,*icon=NULL,*temporary=NULL;
    SDL_asprintf(&installed,"%sHarvestMoonBTN.AppImage",base);
    SDL_asprintf(&icon,"%shmbtn-native.png",base);
    SDL_asprintf(&temporary,"%sapplication.tmp",base);
    if(!installed || !icon || !temporary) goto done;
    if(SDL_strcmp(image,installed)) {
        if(!SDL_CopyFile(image,temporary) || chmod(temporary,0755) || !SDL_RenamePath(temporary,installed)) goto done;
    }
    SDL_IOStream *out=SDL_IOFromFile(icon,"wb");
    if(!out) goto done;
    bool wrote=SDL_WriteIO(out,app_icon_png,sizeof(app_icon_png))==sizeof(app_icon_png);
    if(!SDL_CloseIO(out)) wrote=false;
    if(!wrote) goto done;
    /* Quote Desktop Entry Exec arguments (no shell is used by desktop launchers). */
    char escaped[8192];size_t at=0;
    for(const char *p=installed;*p && at+5<sizeof(escaped);++p) {
        if(*p=='\n' || *p=='\r') goto done;
        if(*p=='%') escaped[at++]='%';
        if(*p=='\\') { escaped[at++]='\\';escaped[at++]='\\';escaped[at++]='\\'; }
        else if(*p=='"' || *p=='$' || *p=='`') { escaped[at++]='\\';escaped[at++]='\\'; }
        escaped[at++]=*p;
    }
    if(SDL_strlen(installed)*4>=sizeof(escaped)) goto done;
    escaped[at]=0;
    const char *xdg=SDL_getenv("XDG_DATA_HOME"),*home=SDL_GetUserFolder(SDL_FOLDER_HOME);
    char *apps=NULL;
    if(xdg && xdg[0]=='/') SDL_asprintf(&apps,"%s/applications",xdg);
    else if(home) SDL_asprintf(&apps,"%s.local/share/applications",home);
    if(!apps) goto done;
    const char *folders[]={apps,SDL_GetUserFolder(SDL_FOLDER_DESKTOP)};
    for(unsigned i=0;i<SDL_arraysize(folders);++i) if(folders[i] && SDL_CreateDirectory(folders[i])) {
        char *path=NULL;SDL_asprintf(&path,"%s/hmbtn-native-appimage.desktop",folders[i]);
        if(path) {
            out=SDL_IOFromFile(path,"wb");
            if(out) {
                char *entry=NULL;
                SDL_asprintf(&entry,"[Desktop Entry]\nType=Application\nName=Harvest Moon BTN (PC)\nExec=\"%s\"\nIcon=%s\nTerminal=false\nCategories=Game;RolePlaying;\n",escaped,icon);
                if(entry) { SDL_WriteIO(out,entry,SDL_strlen(entry));SDL_free(entry); }
                SDL_CloseIO(out);chmod(path,0755);
            }
            SDL_free(path);
        }
    }
    SDL_free(apps);
done:
    if(temporary) SDL_RemovePath(temporary);
    SDL_free(installed);SDL_free(icon);SDL_free(temporary);SDL_free(base);
#endif
}

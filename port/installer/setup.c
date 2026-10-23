#include "setup.h"
#include "disc_import.h"
#include <stdio.h>
typedef struct Setup {
    const char *destination;
    char *image; char error[512];
    SDL_AtomicInt dialog_done,progress,cancel,done;
    bool success;
} Setup;
static void SDLCALL selected(void *user,const char *const *files,int filter)
{
    Setup *s=user;(void)filter;
    if(!files) SDL_strlcpy(s->error,SDL_GetError(),sizeof(s->error));
    else if(files[0]) s->image=SDL_strdup(files[0]);
    SDL_SetAtomicInt(&s->dialog_done,1);
}
static int SDLCALL importing(void *user)
{
    Setup *s=user;
    s->success=hmbtn_disc_import(s->image,s->destination,&s->progress,&s->cancel,s->error,sizeof(s->error));
    SDL_SetAtomicInt(&s->done,1);return 0;
}
int hmbtn_setup_run(SDL_Window *window,SDL_Renderer *renderer,const char *destination)
{
    Setup s={.destination=destination};SDL_Thread *worker=NULL;
    bool picker=false,open=true,choose=true;int result=0;
    SDL_SetWindowTitle(window,"Harvest Moon BTN - Preparar dados do jogo");
    while(open || picker || worker) {
        SDL_Event event;
        while(SDL_PollEvent(&event)) {
            if(event.type==SDL_EVENT_QUIT || (event.type==SDL_EVENT_KEY_DOWN && event.key.key==SDLK_ESCAPE)) {
                open=false;SDL_SetAtomicInt(&s.cancel,1);
            }
            if(!picker && !worker && open) {
                if((event.type==SDL_EVENT_KEY_DOWN && event.key.key==SDLK_RETURN) || event.type==SDL_EVENT_MOUSE_BUTTON_UP) choose=true;
                if(event.type==SDL_EVENT_DROP_FILE && event.drop.data) {
                    SDL_free(s.image);s.image=SDL_strdup(event.drop.data);
                }
            }
        }
        if(picker && SDL_GetAtomicInt(&s.dialog_done)) {
            picker=false;
            if(s.error[0]) {
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,"Escolha do disco",
                    "Nao foi possivel abrir o seletor de arquivos. Arraste o arquivo .bin para esta janela.",window);
                s.error[0]=0;
            }
        }
        if(s.image && !worker && !picker && open) {
            worker=SDL_CreateThread(importing,"import-resources",&s);
            if(!worker) { SDL_free(s.image);return -1; }
        }
        if(worker && SDL_GetAtomicInt(&s.done)) {
            SDL_WaitThread(worker,NULL);worker=NULL;
            if(s.success) { result=1;break; }
            if(open) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Falha na importacao",s.error,window);
            SDL_free(s.image);s.image=NULL;s.error[0]=0;
            SDL_SetAtomicInt(&s.done,0);SDL_SetAtomicInt(&s.progress,0);
        }
        SDL_SetRenderDrawColor(renderer,25,47,37,255);SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer,245,225,170,255);
        SDL_RenderDebugText(renderer,12,24,"Harvest Moon: Back to Nature");
        SDL_RenderDebugText(renderer,12,48,"Preparar recursos do porte para PC");
        SDL_RenderDebugText(renderer,12,80,"Escolha sua copia .bin SLUS_011.15.");
        SDL_RenderDebugText(renderer,12,96,"Disco americano ou traducao compativel.");
        SDL_RenderDebugText(renderer,12,128,worker ? "Extraindo e verificando recursos..." : "Enter / clique: escolher arquivo");
        SDL_RenderDebugText(renderer,12,144,worker ? "Escape: cancelar com seguranca" : "Ou arraste o .bin para esta janela.");
        if(worker) {
            SDL_FRect bar={12,172,(float)SDL_GetAtomicInt(&s.progress)*2.96f,10};
            SDL_RenderFillRect(renderer,&bar);
            char progress[40];snprintf(progress,sizeof(progress),"%d%%",SDL_GetAtomicInt(&s.progress));
            SDL_RenderDebugText(renderer,12,190,progress);
        } else SDL_RenderDebugText(renderer,12,190,"O .bin nao sera copiado ou alterado.");
        SDL_RenderPresent(renderer);
        if(choose && open && !worker && !picker && !s.image) {
            static const SDL_DialogFileFilter filters[]={{"Disco PS1 (.bin)","bin"}};
            choose=false;picker=true;SDL_SetAtomicInt(&s.dialog_done,0);
            SDL_ShowOpenFileDialog(selected,&s,window,filters,1,NULL,false);
        }
        SDL_Delay(16);
    }
    SDL_free(s.image);return result;
}

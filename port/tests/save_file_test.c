#include "save_fixture.h"
#include "save_file.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned temporaries;
static SDL_EnumerationResult count_temp(void *context,const char *dirname,const char *name)
{
    (void)context;(void)dirname;if(strstr(name,".tmp.")) ++temporaries;return SDL_ENUM_CONTINUE;
}
static HmbtnSession *read_checked(const char *path,unsigned identity)
{
    char error[512]={0};HmbtnSession *s=hmbtn_save_file_read(path,error,sizeof(error));
    if(!s) fprintf(stderr,"%s\n",error);
    assert(s && s->game.economy.gold==0x123456+identity && s->game.inventory.toolbox[63].count==99);
    assert(s->poultry.hens[0].days_sick==3 && s->village.flags[205] && s->daily_date==hmbtn_session_date(&s->game.clock));return s;
}
int main(int argc,char **argv)
{
    if(argc!=3) return 2;
    char error[512]={0};
    if(!strcmp(argv[1],"write")) {
        HmbtnSession *s=save_fixture(1);assert(hmbtn_save_file_write(argv[2],s,error,sizeof(error)));hmbtn_session_destroy(s);return 0;
    }
    if(!strcmp(argv[1],"read")) { HmbtnSession *s=read_checked(argv[2],1);hmbtn_session_destroy(s);return 0; }
    char *a=NULL,*b=NULL,*bad=NULL,*directory=NULL,*missing=NULL;
    assert(SDL_asprintf(&a,"%s/fazenda \xc3\xa1.hms",argv[2])>0 && SDL_asprintf(&b,"%s/second.hms",argv[2])>0
        && SDL_asprintf(&bad,"%s/corrupt.hms",argv[2])>0 && SDL_asprintf(&directory,"%s/directory",argv[2])>0
        && SDL_asprintf(&missing,"%s/missing/farm.hms",argv[2])>0);
    HmbtnSession *one=save_fixture(1),*two=save_fixture(2),*loaded=NULL;
    assert(hmbtn_save_file_write(a,one,error,sizeof(error)) && hmbtn_save_file_write(b,two,error,sizeof(error)));
    loaded=read_checked(a,1);hmbtn_session_destroy(loaded);loaded=read_checked(b,2);hmbtn_session_destroy(loaded);
    one->door_tick=1;assert(!hmbtn_save_file_write(a,one,error,sizeof(error)));one->door_tick=0;
    loaded=read_checked(a,1);hmbtn_session_destroy(loaded); /* old slot intact after refusal */
    assert(!hmbtn_save_file_write(missing,one,error,sizeof(error)));
    assert(SDL_CreateDirectory(directory) && !hmbtn_save_file_write(directory,one,error,sizeof(error)));
    loaded=read_checked(a,1);hmbtn_session_destroy(loaded);
    assert(SDL_EnumerateDirectory(argv[2],count_temp,NULL) && !temporaries);
    assert(SDL_CopyFile(a,bad));SDL_IOStream *out=SDL_IOFromFile(bad,"wb");
    assert(out && SDL_WriteIO(out,"short",5)==5 && SDL_CloseIO(out));assert(!hmbtn_save_file_read(bad,error,sizeof(error)));
    assert(hmbtn_save_file_write(a,two,error,sizeof(error)));loaded=read_checked(a,2);hmbtn_session_destroy(loaded);
    loaded=read_checked(b,2);hmbtn_session_destroy(loaded);assert(one->game.economy.gold+1==two->game.economy.gold);
    hmbtn_session_destroy(one);hmbtn_session_destroy(two);SDL_free(a);SDL_free(b);SDL_free(bad);SDL_free(directory);SDL_free(missing);
    puts("Native save files: UTF-8 paths, independent slots, replacement and failure preservation passed.");return 0;
}

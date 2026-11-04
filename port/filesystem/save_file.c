#include "save_file.h"
#include <SDL3/SDL.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
char *hmbtn_save_default_path(void)
{
    char *base=SDL_GetPrefPath("hmbtn","HarvestMoonBTN"),*path=NULL;
    if(base) SDL_asprintf(&path,"%sfarm.hms",base);
    SDL_free(base);return path;
}
static bool write_bytes(const char *path,const uint8_t *data,size_t bytes,char *error,size_t size)
{
    if(!path || !*path) { snprintf(error,size,"Missing save path");return false; }
    char *temporary=NULL;static unsigned sequence;
    if(SDL_asprintf(&temporary,"%s.tmp.%" PRIu64 ".%u",path,(uint64_t)SDL_GetTicksNS(),++sequence)<0) temporary=NULL;
    SDL_IOStream *out=temporary ? SDL_IOFromFile(temporary,"wb") : NULL;
    bool ok=out!=NULL;
    if(ok) ok=SDL_WriteIO(out,data,bytes)==bytes && SDL_FlushIO(out);
    if(out && !SDL_CloseIO(out)) ok=false;
    /* Never truncate/remove the destination. Same-directory publish is atomic
     * on the supported local filesystems; SDL uses replace-existing on Windows.
     * FlushIO syncs file contents. Directory metadata durability after power
     * loss and concurrent writers to one slot are outside this contract. */
    if(ok) ok=SDL_RenamePath(temporary,path);
    if(!ok) {
        snprintf(error,size,"Cannot write native save '%s': %s",path,SDL_GetError());
        if(temporary) SDL_RemovePath(temporary);
    }
    SDL_free(temporary);return ok;
}
bool hmbtn_save_file_write(const char *path,const HmbtnSession *s,char *error,size_t size)
{
    uint8_t *data=NULL;size_t bytes=0;
    if(!hmbtn_save_encode(s,&data,&bytes,error,size)) return false;
    bool ok=write_bytes(path,data,bytes,error,size);free(data);return ok;
}
bool hmbtn_save_file_archive(const char *source,const char *destination,const uint32_t id[4],char *error,size_t size)
{
    SDL_IOStream *in=SDL_IOFromFile(source,"rb");Sint64 n=in ? SDL_GetIOSize(in) : 0;
    uint8_t *data=n>=24 && n<=HMBTN_SAVE_MAX_BYTES ? malloc((size_t)n) : NULL;
    bool ok=data && SDL_ReadIO(in,data,(size_t)n)==(size_t)n;
    if(in && !SDL_CloseIO(in)) ok=false;
    uint8_t *out=NULL;size_t bytes=0;
    if(ok) ok=hmbtn_save_with_identity(data,(size_t)n,id,&out,&bytes,error,size);
    else snprintf(error,size,"Cannot read previous save for archiving: %s",SDL_GetError());
    if(ok) ok=write_bytes(destination,out,bytes,error,size);
    free(data);free(out);return ok;
}
HmbtnSession *hmbtn_save_file_read(const char *path,char *error,size_t size)
{
    if(!path || !*path) { snprintf(error,size,"Missing save path");return NULL; }
    SDL_IOStream *in=SDL_IOFromFile(path,"rb");
    if(!in) { snprintf(error,size,"Cannot read native save '%s': %s",path,SDL_GetError());return NULL; }
    Sint64 length=SDL_GetIOSize(in);
    if(length<24 || length>HMBTN_SAVE_MAX_BYTES) {
        snprintf(error,size,"Native save file has invalid size");SDL_CloseIO(in);return NULL;
    }
    uint8_t *data=malloc((size_t)length);bool ok=data && SDL_ReadIO(in,data,(size_t)length)==(size_t)length;
    /* Reject a file that grew while it was read, too. */
    uint8_t extra;if(ok && SDL_ReadIO(in,&extra,1)!=0) ok=false;
    if(!SDL_CloseIO(in)) ok=false;
    HmbtnSession *s=NULL;
    if(ok) s=hmbtn_save_decode(data,(size_t)length,error,size);
    else snprintf(error,size,"Cannot read complete native save: %s",SDL_GetError());
    free(data);return s;
}

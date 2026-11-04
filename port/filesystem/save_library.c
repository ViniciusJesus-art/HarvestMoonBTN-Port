#include "save_library.h"
#include "psx_data.h"
#include <SDL3/SDL.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

void hmbtn_save_name(char *out,size_t size,const uint16_t *name)
{
    size_t at=0;
    for(unsigned i=0;i<HMBTN_NAME_CHARS && name[i]!=0xffff && at+1<size;++i) {
        unsigned c=name[i];
        out[at++]=c<26 ? (char)('A'+c) : c>=0x20 && c<0x3a ? (char)('a'+c-0x20)
            : c>=0x40 && c<0x4a ? (char)('0'+c-0x40) : c==0xffd ? ' ' : '?';
    }
    if(size) out[at]=0;
}
char *hmbtn_save_library_root(const char *slot)
{
    if(!slot) return NULL;
    const char *last=NULL;
    for(const char *p=slot;*p;++p) if(*p=='/' || *p=='\\') last=p;
    char *path=NULL;
    if(last) SDL_asprintf(&path,"%.*s/saves",(int)(last-slot),slot);
    else path=SDL_strdup("saves");
    return path;
}
static bool identified(const uint32_t id[4]) { return (id[0]|id[1]|id[2]|id[3])!=0; }
void hmbtn_save_library_adopt(HmbtnSession *s,const char *path)
{
    if(identified(s->farm_id) || !path) return;
    uint8_t header[24];SDL_IOStream *in=SDL_IOFromFile(path,"rb");
    if(!in) return;
    const bool read=SDL_ReadIO(in,header,sizeof(header))==sizeof(header);SDL_CloseIO(in);
    if(!read) return;
    uint32_t hash=2166136261u;
    for(const unsigned char *p=(const unsigned char *)path;*p;++p) hash=(hash^*p)*16777619u;
    s->farm_id[0]=0x4c454741;s->farm_id[1]=hmbtn_read_le32(header+20);s->farm_id[2]=hash;s->farm_id[3]=1;
}
bool hmbtn_save_same_farm(const HmbtnSaveEntry *a,const HmbtnSaveEntry *b)
{
    if(identified(a->farm_id) || identified(b->farm_id)) return !memcmp(a->farm_id,b->farm_id,sizeof(a->farm_id));
    /* A pre-library slot has no identity yet. It is assigned one on next save. */
    return !strcmp(a->path,b->path);
}
bool hmbtn_save_library_write(const char *root,const char *slot,HmbtnSession *s,char *error,size_t size)
{
    if(!root || !slot || !hmbtn_save_available(s,error,size)) return false;
    /* Preserve the previous single slot before the first archival save. */
    HmbtnSession *prior=hmbtn_save_file_read(slot,error,size);
    if(prior && !identified(prior->farm_id)) {
        hmbtn_save_library_adopt(prior,slot);char *folder=NULL,*path=NULL;
        SDL_asprintf(&folder,"%s/%08" PRIx32 "%08" PRIx32 "%08" PRIx32 "%08" PRIx32,root,
            prior->farm_id[0],prior->farm_id[1],prior->farm_id[2],prior->farm_id[3]);
        if(folder) SDL_asprintf(&path,"%s/previous-slot.hms",folder);
        bool preserved=path && SDL_CreateDirectory(folder) && hmbtn_save_file_archive(slot,path,prior->farm_id,error,size);
        SDL_free(path);SDL_free(folder);hmbtn_session_destroy(prior);
        if(!preserved) return false;
    } else hmbtn_session_destroy(prior);
    if(size) error[0]=0;
    SDL_Time time=0;SDL_GetCurrentTime(&time);
    if(!identified(s->farm_id)) {
        s->farm_id[0]=(uint32_t)time;s->farm_id[1]=(uint32_t)((uint64_t)time>>32);
        s->farm_id[2]=SDL_rand_bits();s->farm_id[3]=SDL_rand_bits()|1u;
    }
    char *folder=NULL,*archive=NULL;
    SDL_asprintf(&folder,"%s/%08" PRIx32 "%08" PRIx32 "%08" PRIx32 "%08" PRIx32,root,
        s->farm_id[0],s->farm_id[1],s->farm_id[2],s->farm_id[3]);
    static unsigned serial;
    SDL_PathInfo occupied;
    if(folder) do {
        SDL_free(archive);archive=NULL;
        SDL_asprintf(&archive,"%s/%020" PRIu64 "-%08x.hms",folder,(uint64_t)time,++serial);
    } while(archive && SDL_GetPathInfo(archive,&occupied));
    bool ok=archive && SDL_CreateDirectory(folder);
    if(!ok) snprintf(error,size,"Cannot create save history: %s",SDL_GetError());
    if(ok) ok=hmbtn_save_file_write(archive,s,error,size);
    /* The history is published first: a failed latest-slot replacement cannot
     * destroy the newly recorded snapshot or any other farm's history. */
    if(ok) ok=hmbtn_save_file_write(slot,s,error,size);
    SDL_free(archive);SDL_free(folder);return ok;
}
void hmbtn_save_library_close(HmbtnSaveLibrary *l)
{
    for(size_t i=0;i<l->count;++i) SDL_free(l->entries[i].path);
    SDL_free(l->entries);*l=(HmbtnSaveLibrary){0};
}
typedef struct Scan { HmbtnSaveLibrary *library;unsigned depth;bool failed; } Scan;
static bool add(Scan *scan,const char *path,bool latest)
{
    char error[256];HmbtnSession *s=hmbtn_save_file_read(path,error,sizeof(error));
    if(!s) { ++scan->library->skipped;return true; }
    hmbtn_save_library_adopt(s,path);
    HmbtnSaveEntry entry={0};memcpy(entry.farm_id,s->farm_id,sizeof(entry.farm_id));
    hmbtn_save_name(entry.player,sizeof(entry.player),s->game.registration.player_name);
    hmbtn_save_name(entry.farm,sizeof(entry.farm),s->game.registration.farm_name);
    entry.gold=s->game.economy.gold;entry.berries=s->menu.stats.power_berries;entry.clock=s->game.clock;
    hmbtn_session_destroy(s);
    uint8_t header[24];SDL_IOStream *in=SDL_IOFromFile(path,"rb");
    if(in) { if(SDL_ReadIO(in,header,sizeof(header))==sizeof(header)) entry.checksum=hmbtn_read_le32(header+20);SDL_CloseIO(in); }
    SDL_PathInfo info;if(SDL_GetPathInfo(path,&info)) entry.modified=info.modify_time;
    if(latest) for(size_t i=0;i<scan->library->count;++i) {
        const HmbtnSaveEntry *other=&scan->library->entries[i];
        if(identified(entry.farm_id) && !memcmp(entry.farm_id,other->farm_id,sizeof(entry.farm_id))
                && entry.checksum==other->checksum) return true;
    }
    HmbtnSaveEntry *grown=SDL_realloc(scan->library->entries,(scan->library->count+1)*sizeof(*grown));
    if(!grown) return false;
    scan->library->entries=grown;entry.path=SDL_strdup(path);
    if(!entry.path) return false;
    grown[scan->library->count++]=entry;return true;
}
static SDL_EnumerationResult SDLCALL enumerate(void *context,const char *dir,const char *name)
{
    Scan *scan=context;char *path=NULL;SDL_asprintf(&path,"%s/%s",dir,name);
    if(!path) { scan->failed=true;return SDL_ENUM_FAILURE; }
    SDL_PathInfo info;
    if(SDL_GetPathInfo(path,&info)) {
        if(info.type==SDL_PATHTYPE_DIRECTORY && scan->depth<1) {
            Scan child={scan->library,scan->depth+1,false};
            if(!SDL_EnumerateDirectory(path,enumerate,&child) || child.failed) scan->failed=true;
        } else if(info.type==SDL_PATHTYPE_FILE && SDL_strlen(name)>4 && !SDL_strcasecmp(name+SDL_strlen(name)-4,".hms")) {
            if(!add(scan,path,false)) scan->failed=true;
        }
    }
    SDL_free(path);return scan->failed ? SDL_ENUM_FAILURE : SDL_ENUM_CONTINUE;
}
static int newest(const void *va,const void *vb)
{
    const HmbtnSaveEntry *a=va,*b=vb;
    if(a->modified!=b->modified) return a->modified>b->modified ? -1 : 1;
    return -strcmp(a->path,b->path);
}
bool hmbtn_save_library_scan(HmbtnSaveLibrary *l,const char *root,const char *slot,char *error,size_t size)
{
    hmbtn_save_library_close(l);Scan scan={l,0,false};SDL_PathInfo info;
    if(root && SDL_GetPathInfo(root,&info) && info.type==SDL_PATHTYPE_DIRECTORY)
        if(!SDL_EnumerateDirectory(root,enumerate,&scan)) scan.failed=true;
    if(slot && SDL_GetPathInfo(slot,&info) && info.type==SDL_PATHTYPE_FILE && !add(&scan,slot,true)) scan.failed=true;
    if(scan.failed) { snprintf(error,size,"Cannot list save history: %s",SDL_GetError());return false; }
    if(l->count>1) SDL_qsort(l->entries,l->count,sizeof(*l->entries),newest);
    return true;
}

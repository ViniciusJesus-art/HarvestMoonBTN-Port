#include "disc_import.h"
#include "psx_data.h"
#include "sha256.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "import_members.inc"

typedef struct Entry { uint32_t lba,size; unsigned flags,xa; } Entry;
typedef struct Imported { char name[96],sha256[65]; uint64_t size; } Imported;
typedef struct Import {
    SDL_IOStream *image; uint64_t sectors,total,done;
    SDL_AtomicInt *progress,*cancel;
    Imported files[36]; unsigned count;
    char *staging; char *error; size_t error_size;
} Import;
static bool fail(Import *s,const char *message)
{ snprintf(s->error,s->error_size,"%s",message); return false; }
static bool sector(Import *s,uint32_t lba,uint8_t b[2352])
{
    static const uint8_t sync[]={0,255,255,255,255,255,255,255,255,255,255,0};
    if(s->cancel && SDL_GetAtomicInt(s->cancel)) return fail(s,"Import cancelled.");
    if(lba>=s->sectors || SDL_SeekIO(s->image,(Sint64)lba*2352,SDL_IO_SEEK_SET)<0
            || SDL_ReadIO(s->image,b,2352)!=2352) return fail(s,"Truncated or unreadable MODE2/2352 image.");
    if(memcmp(b,sync,12) || b[15]!=2) return fail(s,"Expected a raw MODE2/2352 data track (.bin).");
    return true;
}
static bool extent_valid(Import *s,const Entry *e)
{ return e->lba<s->sectors && ((uint64_t)e->size+2047)/2048<=s->sectors-e->lba; }
static bool find(Import *s,const Entry *directory,const char *name,Entry *entry)
{
    if(!extent_valid(s,directory) || directory->size>65536) return fail(s,"Invalid ISO directory bounds.");
    for(unsigned n=0;n<(directory->size+2047u)/2048u;++n) {
        uint8_t b[2352]; if(!sector(s,directory->lba+n,b)) return false;
        const uint8_t *p=b+24;
        for(unsigned at=0;at<2048 && p[at];) {
            const unsigned len=p[at];
            if(len<34 || at+len>2048) return fail(s,"Invalid ISO directory record.");
            const uint8_t *r=p+at; const unsigned names=r[32];
            if(33+names>len) return fail(s,"Truncated ISO filename.");
            char filename[256]; memcpy(filename,r+33,names);filename[names]=0;
            char *version=strchr(filename,';'); if(version) *version=0;
            if(!strcmp(filename,name)) {
                const unsigned system=33+names+(1-names%2);
                *entry=(Entry){hmbtn_read_le32(r+2),hmbtn_read_le32(r+10),r[25],0};
                if(system+8<=len && r[system+6]=='X' && r[system+7]=='A')
                    entry->xa=(unsigned)r[system+4]*256+r[system+5];
                if(!extent_valid(s,entry) || (entry->flags&0x80u)) return fail(s,"Invalid or multi-extent ISO file.");
                return true;
            }
            at+=len;
        }
    }
    snprintf(s->error,s->error_size,"Required disc entry missing: %s",name); return false;
}
static bool user_bytes(Import *s,const Entry *e,uint32_t offset,void *out,size_t size)
{
    if(offset>e->size || size>e->size-offset) return fail(s,"A_FILE offset outside its archive.");
    uint8_t *p=out;
    while(size) {
        uint8_t b[2352]; if(!sector(s,e->lba+offset/2048,b)) return false;
        const size_t count=size<2048-offset%2048 ? size : 2048-offset%2048;
        if(b[18]&0x20u) return fail(s,"Expected Form 1 resource sector.");
        memcpy(p,b+24+offset%2048,count);p+=count;offset+=(uint32_t)count;size-=count;
    }
    return true;
}
static bool extract(Import *s,const Entry *e,uint32_t start,uint32_t size,const char *name,bool raw)
{
    char *path=NULL;
    if(SDL_asprintf(&path,"%s/%s",s->staging,name)<0) return fail(s,"Cannot allocate import path.");
    SDL_IOStream *out=SDL_IOFromFile(path,"wb");SDL_free(path);
    if(!out) return fail(s,SDL_GetError());
    Imported *file=&s->files[s->count++]; SDL_strlcpy(file->name,name,sizeof(file->name));
    HmbtnSHA256 hash; hmbtn_sha256_init(&hash);
    bool ok=true;
    for(uint32_t at=0;at<size;) {
        uint8_t b[2352]; size_t count=size-at<2048 ? size-at : 2048;
        if(!sector(s,e->lba+(start+at)/2048,b)) { ok=false;break; }
        const uint8_t *bytes=b+(raw ? 16 : 24);
        if(raw) count=2336;
        else if(b[18]&0x20u) { ok=fail(s,"Form 2 sector inside a Form 1 resource.");break; }
        if(SDL_WriteIO(out,bytes,count)!=count) { ok=fail(s,SDL_GetError());break; }
        hmbtn_sha256_update(&hash,bytes,count);file->size+=count;s->done+=count;
        at+=size-at<2048 ? size-at : 2048;
        if(s->progress) SDL_SetAtomicInt(s->progress,s->total ? (int)(s->done*99/s->total) : 0);
    }
    if(!SDL_CloseIO(out)) ok=fail(s,SDL_GetError());
    if(ok) hmbtn_sha256_finish(&hash,file->sha256);
    return ok;
}
static bool write_text(SDL_IOStream *out,const char *text)
{ const size_t n=strlen(text);return SDL_WriteIO(out,text,n)==n; }
static bool quoted(SDL_IOStream *out,const char *text)
{
    if(!write_text(out,"\"")) return false;
    for(const unsigned char *p=(const unsigned char*)text;*p;++p) {
        char escape[7];
        if(*p<32) { snprintf(escape,sizeof(escape),"\\u%04x",*p);if(!write_text(out,escape)) return false; }
        else {
            if((*p=='"' || *p=='\\') && !write_text(out,"\\")) return false;
            if(SDL_WriteIO(out,p,1)!=1) return false;
        }
    }
    return write_text(out,"\"");
}
static bool manifest(Import *s,const char *image)
{
    char *path=NULL;if(SDL_asprintf(&path,"%s/native-import.json",s->staging)<0) return false;
    SDL_IOStream *out=SDL_IOFromFile(path,"wb");SDL_free(path);if(!out) return false;
    bool ok=write_text(out,"{\n  \"format\": 2, \"game\": \"SLUS_011.15\", \"resources_only\": true,\n  \"source\": ")
        && quoted(out,image) && write_text(out,",\n  \"files\": {\n");
    for(unsigned i=0;ok && i<s->count;++i) {
        char record[192];const Imported *f=&s->files[i];
        snprintf(record,sizeof(record),": {\"size\": %" PRIu64 ", \"sha256\": \"%s\"}%s\n",f->size,f->sha256,i+1==s->count ? "" : ",");
        ok=write_text(out,"    ") && quoted(out,f->name) && write_text(out,record);
    }
    if(ok) ok=write_text(out,"  }\n}\n");
    if(!SDL_CloseIO(out)) ok=false;
    return ok;
}
static void cleanup(Import *s)
{
    if(!s->staging) return;
    /* Only our fixed allowlist; never walk paths taken from the disc. */
    for(unsigned i=0;i<s->count;++i) {
        char *p=NULL;if(SDL_asprintf(&p,"%s/%s",s->staging,s->files[i].name)>=0) { SDL_RemovePath(p);SDL_free(p); }
    }
    const char *dirs[]={"native-import.json","disc/STR","disc","A_FILE",""};
    for(unsigned i=0;i<SDL_arraysize(dirs);++i) {
        char *p=NULL;if(SDL_asprintf(&p,"%s/%s",s->staging,dirs[i])>=0) { SDL_RemovePath(p);SDL_free(p); }
    }
}
bool hmbtn_disc_import(const char *image,const char *destination,SDL_AtomicInt *progress,
    SDL_AtomicInt *cancel,char *error,size_t error_size)
{
    Import s={.progress=progress,.cancel=cancel,.error=error,.error_size=error_size};
    SDL_PathInfo info;bool ok=false;
    if(SDL_GetPathInfo(destination,&info)) return fail(&s,"Destination already exists; extracted data will not be overwritten.");
    s.image=SDL_IOFromFile(image,"rb"); if(!s.image) return fail(&s,SDL_GetError());
    const Sint64 bytes=SDL_GetIOSize(s.image);
    if(bytes<2352*20 || bytes%2352 || bytes>INT64_C(2147483648)) { fail(&s,"Invalid raw disc image size (expected MODE2/2352).");goto done; }
    s.sectors=(uint64_t)bytes/2352;
    uint8_t b[2352];if(!sector(&s,16,b)) goto done;
    if(memcmp(b+24,"\1CD001\1",7)) { fail(&s,"ISO 9660 volume descriptor missing.");goto done; }
    const Entry root={hmbtn_read_le32(b+24+158),hmbtn_read_le32(b+24+166),2,0};
    Entry config,boot,archive,hdt,str,xa[4];
    if(!find(&s,&root,"SYSTEM.CNF",&config) || !find(&s,&root,"SLUS_011.15",&boot)
            || !find(&s,&root,"A_FILE.BIN",&archive) || !find(&s,&root,"A_FILE.HDT",&hdt)
            || !find(&s,&root,"STR",&str)) goto done;
    char cnf[2049]={0};
    if(config.size>2048 || !user_bytes(&s,&config,0,cnf,config.size) || !strstr(cnf,"SLUS_011.15")) {
        fail(&s,"Expected the American SLUS_011.15 layout (including compatible translations).");goto done;
    }
    uint8_t directory[132];uint32_t offsets[33];
    if(hdt.size!=sizeof(directory) || !user_bytes(&s,&hdt,0,directory,sizeof(directory))) { fail(&s,"Invalid A_FILE.HDT size.");goto done; }
    for(unsigned i=0;i<33;++i) {
        offsets[i]=hmbtn_read_le32(directory+i*4);
        if(offsets[i]%2048 || offsets[i]>archive.size || (i && offsets[i]<offsets[i-1])) { fail(&s,"Invalid A_FILE.HDT offsets.");goto done; }
    }
    if(offsets[0] || offsets[32]!=archive.size) { fail(&s,"A_FILE.HDT does not span the archive.");goto done; }
    for(unsigned i=0;i<32;++i) if(import_members[i]) {
        if(offsets[i+1]==offsets[i]) { fail(&s,"Empty resource member.");goto done; }
        s.total+=offsets[i+1]-offsets[i];
    }
    for(unsigned i=0;i<4;++i) {
        char name[16];snprintf(name,sizeof(name),"%02u.XA",i+1);
        if(!find(&s,&str,name,&xa[i])) goto done;
        if(!(xa[i].xa&0x3000u) || !xa[i].size || (xa[i].flags&2u)) { fail(&s,"Invalid XA stream entry.");goto done; }
        s.total+=((uint64_t)xa[i].size+2047)/2048*2336;
    }
    char *parent=SDL_strdup(destination);if(!parent) { fail(&s,"Cannot allocate destination path.");goto done; }
    char *slash=strrchr(parent,'/');
#ifdef _WIN32
    char *back=strrchr(parent,'\\');if(back && (!slash || back>slash)) slash=back;
#endif
    if(slash) { *slash=0;if(*parent && !SDL_CreateDirectory(parent)) { SDL_free(parent);fail(&s,SDL_GetError());goto done; } }
    SDL_free(parent);
    if(SDL_asprintf(&s.staging,"%s.import-%" PRIu64,destination,(uint64_t)SDL_GetPerformanceCounter())<0) { fail(&s,"Cannot allocate staging path.");goto done; }
    if(SDL_GetPathInfo(s.staging,&info)) { fail(&s,"Staging path already exists.");SDL_free(s.staging);s.staging=NULL;goto done; }
    if(!SDL_CreateDirectory(s.staging)) { fail(&s,SDL_GetError());goto done; }
    const char *dirs[]={"A_FILE","disc/STR"};
    for(unsigned i=0;i<SDL_arraysize(dirs);++i) {
        char *p=NULL;if(SDL_asprintf(&p,"%s/%s",s.staging,dirs[i])<0) { fail(&s,"Cannot allocate directory path.");goto done; }
        const bool made=SDL_CreateDirectory(p);SDL_free(p);if(!made) { fail(&s,SDL_GetError());goto done; }
    }
    for(unsigned i=0;i<32;++i) if(import_members[i]) {
        char name[96];snprintf(name,sizeof(name),"A_FILE/%s",import_members[i]);
        if(!extract(&s,&archive,offsets[i],offsets[i+1]-offsets[i],name,false)) goto done;
    }
    for(unsigned i=0;i<4;++i) {
        char name[32];snprintf(name,sizeof(name),"disc/STR/%02u.XA",i+1);
        if(!extract(&s,&xa[i],0,xa[i].size,name,true)) goto done;
    }
    if(!manifest(&s,image)) { fail(&s,"Cannot write resource manifest.");goto done; }
    if(SDL_GetPathInfo(destination,&info) || !SDL_RenamePath(s.staging,destination)) { fail(&s,"Cannot publish imported data; destination exists or is not writable.");goto done; }
    ok=true;if(progress) SDL_SetAtomicInt(progress,100);
done:
    if(s.image) SDL_CloseIO(s.image);
    if(!ok) cleanup(&s);
    SDL_free(s.staging);return ok;
}

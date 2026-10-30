#include "xa_decode.h"
#include "xa_stream.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"XA line %d: %s (%s)\n",__LINE__,#x,error); return 1; } } while (0)

static void sector_make(uint8_t *b, unsigned channel, unsigned coding, bool end)
{
    memset(b,0,HMBTN_XA_SECTOR_BYTES);
    b[0]=1; b[1]=(uint8_t)channel; b[2]=(uint8_t)(end ? 0xE4 : 0x64); b[3]=(uint8_t)coding;
    memcpy(b+4,b,4);
    for (unsigned group=0;group<18;++group) {
        memset(b+8+group*128,8,16); /* shift 8: +/-16 per nibble, predictor 0 */
        memset(b+8+group*128+16,0xF1,112);
    }
}
/* Optional probe: decode a complete original channel to raw native-rate PCM,
 * streamed to disk, for independent comparison with FFmpeg. */
static int original_probe(const char *root, const char *output, unsigned channel)
{
    char error[256]={0}, *path=NULL;
    CHECK(SDL_asprintf(&path,"%s/disc/STR/01.XA",root)>=0);
    SDL_IOStream *in=SDL_IOFromFile(path,"rb"); SDL_free(path);
    FILE *out=fopen(output,"wb");
    CHECK(in && out);
    HmbtnXADecoder d={0}; uint8_t b[HMBTN_XA_SECTOR_BYTES];
    int16_t pcm[HMBTN_XA_MAX_FRAMES*2]; uint64_t total=0,energy=0; bool ended=false;
    while (SDL_ReadIO(in,b,sizeof(b))==sizeof(b)) {
        if (b[0]!=1 || b[1]!=channel || !(b[2]&4u)) continue;
        size_t frames;
        CHECK(hmbtn_xa_decode(&d,b,sizeof(b),pcm,HMBTN_XA_MAX_FRAMES,&frames,error,sizeof(error)));
        uint8_t little[HMBTN_XA_MAX_FRAMES*4];
        for (size_t i=0;i<frames*2;++i) {
            const uint16_t v=(uint16_t)pcm[i]; little[i*2]=(uint8_t)v; little[i*2+1]=(uint8_t)(v>>8);
            energy+=(uint64_t)((int32_t)pcm[i]*pcm[i]);
        }
        CHECK(fwrite(little,4,frames,out)==frames); total+=frames;
        if (b[2]&0x80u) { ended=true; break; }
    }
    CHECK(SDL_CloseIO(in) && fclose(out)==0);
    printf("XA channel %u: %llu stereo frames at %u Hz, energy=%llu, final=%u\n",
        channel,(unsigned long long)total,d.rate,(unsigned long long)energy,ended);
    CHECK(ended && total>0 && energy>0);
    return 0;
}
int main(int argc, char **argv)
{
    if (argc==4) return original_probe(argv[1],argv[2],(unsigned)atoi(argv[3]));
    char error[256]={0}; uint8_t b[HMBTN_XA_SECTOR_BYTES];
    int16_t pcm[HMBTN_XA_MAX_FRAMES*2]; HmbtnXADecoder d={0}; size_t frames;
    sector_make(b,4,1,true);
    CHECK(hmbtn_xa_decode(&d,b,sizeof(b),pcm,HMBTN_XA_MAX_FRAMES,&frames,error,sizeof(error)));
    CHECK(frames==2016 && d.rate==37800 && d.channels==2);
    for (size_t i=0;i<frames;++i) CHECK(pcm[i*2]==16 && pcm[i*2+1]==-16);
    CHECK(!hmbtn_xa_decode(&d,b,sizeof(b)-1,pcm,HMBTN_XA_MAX_FRAMES,&frames,error,sizeof(error)));
    CHECK(!hmbtn_xa_decode(&d,b,sizeof(b),pcm,2015,&frames,error,sizeof(error)));
    b[4]^=1;
    CHECK(!hmbtn_xa_decode(&d,b,sizeof(b),pcm,HMBTN_XA_MAX_FRAMES,&frames,error,sizeof(error)));
    sector_make(b,4,0,true); memset(&d,0,sizeof(d));
    CHECK(hmbtn_xa_decode(&d,b,sizeof(b),pcm,HMBTN_XA_MAX_FRAMES,&frames,error,sizeof(error)));
    CHECK(frames==4032 && pcm[0]==16 && pcm[1]==16 && pcm[56]==-16 && pcm[57]==-16);
    sector_make(b,4,5,true); memset(&d,0,sizeof(d));
    CHECK(hmbtn_xa_decode(&d,b,sizeof(b),pcm,HMBTN_XA_MAX_FRAMES,&frames,error,sizeof(error)) && d.rate==18900);
    /* Predictor history survives sector boundaries; the two channels remain independent. */
    sector_make(b,4,1,true); memset(&d,0,sizeof(d)); d.history[0][0]=64;
    memset(b+8,0x18,16); memset(b+24,0,112);
    CHECK(hmbtn_xa_decode(&d,b,sizeof(b),pcm,HMBTN_XA_MAX_FRAMES,&frames,error,sizeof(error)));
    CHECK(pcm[0]==60 && pcm[1]==0 && pcm[2]==56);
    sector_make(b,4,1,true); memset(&d,0,sizeof(d));
    for (unsigned g=0;g<18;++g) memset(b+8+g*128,0x0F,16);
    CHECK(hmbtn_xa_decode(&d,b,sizeof(b),pcm,HMBTN_XA_MAX_FRAMES,&frames,error,sizeof(error)));
    CHECK(pcm[0]==8 && pcm[1]==-8); /* Reserved shift maps to 9. */
    b[12]=0x40;
    CHECK(!hmbtn_xa_decode(&d,b,sizeof(b),pcm,HMBTN_XA_MAX_FRAMES,&frames,error,sizeof(error)));
    sector_make(b,4,0x11,true);
    CHECK(!hmbtn_xa_decode(&d,b,sizeof(b),pcm,HMBTN_XA_MAX_FRAMES,&frames,error,sizeof(error)));

    /* A tiny extracted file with two interleaved channels, no game assets. */
    CHECK(argc==2);
    char *temp=SDL_strdup(argv[1]), *disc=NULL,*str=NULL,*path=NULL;
    CHECK(temp && SDL_asprintf(&disc,"%sdisc",temp)>=0 && SDL_CreateDirectory(disc));
    CHECK(SDL_asprintf(&str,"%s/STR",disc)>=0 && SDL_CreateDirectory(str));
    CHECK(SDL_asprintf(&path,"%s/01.XA",str)>=0);
    SDL_IOStream *file=SDL_IOFromFile(path,"wb"); CHECK(file);
    sector_make(b,5,1,true); CHECK(SDL_WriteIO(file,b,sizeof(b))==sizeof(b));
    sector_make(b,4,1,true); CHECK(SDL_WriteIO(file,b,sizeof(b))==sizeof(b) && SDL_CloseIO(file));
    HmbtnXAStream stream={0};
    CHECK(hmbtn_xa_stream_start(&stream,temp,4,error,sizeof(error)) && stream.sector==2);
    memset(pcm,0,sizeof(pcm));
    CHECK(hmbtn_xa_stream_mix(&stream,pcm,16,error,sizeof(error)) && pcm[0]==0);
    for (unsigned i=0;i<4;++i) hmbtn_xa_stream_tick(&stream);
    CHECK(hmbtn_xa_stream_mix(&stream,pcm,16,error,sizeof(error)) && pcm[0]>0 && pcm[1]<0);
    memset(pcm,0,sizeof(pcm));
    CHECK(hmbtn_xa_stream_mix(&stream,pcm,HMBTN_XA_MAX_FRAMES,error,sizeof(error)));
    CHECK(stream.ended && pcm[HMBTN_XA_MAX_FRAMES*2-1]==0); /* Final sector drained safely. */
    for (unsigned i=0;i<502;++i) hmbtn_xa_stream_tick(&stream);
    CHECK(!stream.active);
    CHECK(hmbtn_xa_stream_start(&stream,temp,5,error,sizeof(error)) && stream.sector==1);
    for (unsigned i=0;i<641;++i) hmbtn_xa_stream_tick(&stream);
    CHECK(!stream.active);
    hmbtn_xa_stream_close(&stream);
    char *cooking_path=NULL;CHECK(SDL_asprintf(&cooking_path,"%s/04.XA",str)>=0);
    sector_make(b,3,1,true);file=SDL_IOFromFile(cooking_path,"wb");
    CHECK(file && SDL_WriteIO(file,b,sizeof(b))==sizeof(b) && SDL_CloseIO(file));
    CHECK(hmbtn_xa_stream_start(&stream,temp,0x16,error,sizeof(error))
        && stream.track==0x16 && stream.channel==3 && stream.volume==0xe9);
    for(unsigned i=0;i<4;++i) hmbtn_xa_stream_tick(&stream);
    memset(pcm,0,sizeof(pcm));CHECK(hmbtn_xa_stream_mix(&stream,pcm,16,error,sizeof(error)) && pcm[0]>0);
    for(unsigned i=0;i<117;++i) hmbtn_xa_stream_tick(&stream);
    CHECK(stream.active && !stream.volume);
    hmbtn_xa_stream_tick(&stream);CHECK(!stream.active);
    hmbtn_xa_stream_close(&stream);CHECK(SDL_RemovePath(cooking_path));SDL_free(cooking_path);
    CHECK(SDL_RemovePath(path));
    file=SDL_IOFromFile(path,"wb"); CHECK(file && SDL_WriteIO(file,b,5)==5 && SDL_CloseIO(file));
    CHECK(!hmbtn_xa_stream_start(&stream,temp,4,error,sizeof(error)) && !stream.file);
    CHECK(SDL_RemovePath(path) && SDL_RemovePath(str) && SDL_RemovePath(disc));
    SDL_free(path); SDL_free(str); SDL_free(disc); SDL_free(temp);
    puts("XA channel decoding, predictors, PCM, streamed filtering, timers and invalid data passed.");
    return 0;
}

#include "village_art.h"
#include "resource_file.h"
#include "psx_data.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
void hmbtn_village_art_close(HmbtnVillageArt *a)
{
    for(unsigned i=0;i<2;++i) { free(a->blocks[i]);free(a->atlases[i]); }
    memset(a,0,sizeof(*a));
}
static bool load(HmbtnVillageArt *a,unsigned slot,const char *root,unsigned row,char *error,size_t size)
{
    if(a->blocks[slot] && a->keys[slot]==row) return true;
    memset(a->animations[slot],0,sizeof(a->animations[slot]));
    free(a->blocks[slot]);a->blocks[slot]=NULL;
    if(row>=35) { a->keys[slot]=row;return true; }
    const HmbtnVillagePack *toc;uint32_t start;size_t bytes;
    if(!hmbtn_village_pack(row,&start,&bytes,&toc)) return false;
    a->blocks[slot]=malloc(bytes);
    if(!a->atlases[slot]) a->atlases[slot]=malloc(sizeof(*a->atlases[slot]));
    if(!a->blocks[slot] || !a->atlases[slot]) { snprintf(error,size,"Cannot allocate villager animation pack");return false; }
    uint8_t *data=a->blocks[slot];
    if(!hmbtn_resource_read(root,"A_FILE/00_gf.bin",start,data,bytes,error,size)) return false;
    if(hmbtn_read_le32(data)!=toc->count || toc->offsets[toc->count]>=bytes) {
        snprintf(error,size,"Invalid villager animation table");return false;
    }
    for(unsigned i=0;i<toc->count;++i) {
        unsigned id=hmbtn_read_le32(data+4+i*4);
        if(id>=37) continue;
        if(toc->offsets[i]>toc->offsets[i+1] || toc->offsets[i+1]>bytes) {
            snprintf(error,size,"Villager %u pack %u outside original table",id,row);return false;
        }
        char detail[256]={0};
        if(!hmbtn_animation_open(&a->animations[slot][id],data+toc->offsets[i],
                toc->offsets[i+1]-toc->offsets[i],detail,sizeof(detail))) {
            snprintf(error,size,"Villager %u pack %u: %s",id,row,detail);return false;
        }
    }
    if(!hmbtn_bank_decode(data+toc->offsets[toc->count],bytes-toc->offsets[toc->count],a->atlases[slot],error,size)) return false;
    a->keys[slot]=row;return true;
}
bool hmbtn_village_art_update(HmbtnVillageArt *a,const char *root,unsigned primary,const HmbtnFarmClock *c,char *error,size_t size)
{
    const unsigned part=c->hour<6 ? 2 : (c->hour-6)/6;
    return load(a,0,root,primary,error,size) && load(a,1,root,14+c->weekday*3+(part>2 ? 2 : part),error,size);
}
const HmbtnAnimation *hmbtn_village_art_actor(const HmbtnVillageArt *a,unsigned id,const HmbtnImageAtlas **atlas)
{
    if(id>=37 || !atlas) return NULL;
    for(unsigned i=0;i<2;++i) if(a->animations[i][id].data) { *atlas=a->atlases[i];return &a->animations[i][id]; }
    return NULL;
}

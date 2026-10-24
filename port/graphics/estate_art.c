#include "estate_art.h"
#include "resource_file.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
typedef struct HmbtnEstateArtToc { unsigned count;uint32_t offsets[46]; } HmbtnEstateArtToc;
#include "estate_art_tables.inc"
void hmbtn_estate_art_close(HmbtnEstateArt *a) { free(a->data);free(a->atlas);memset(a,0,sizeof(*a)); }
bool hmbtn_estate_art_update(HmbtnEstateArt *a,const char *root,unsigned row,char *error,size_t size)
{
    if(a->data && a->row==row) return true;
    hmbtn_estate_art_close(a);if(row>=10) return true;
    const size_t bytes=estate_art_offsets[row+1]-estate_art_offsets[row];
    const HmbtnEstateArtToc *toc=&estate_art_tocs[row];
    a->data=malloc(bytes);a->atlas=malloc(sizeof(*a->atlas));
    if(!a->data || !a->atlas) { snprintf(error,size,"Cannot allocate original estate sprites");goto fail; }
    if(toc->offsets[toc->count]>=bytes) { snprintf(error,size,"Estate texture bank outside pack");goto fail; }
    if(!hmbtn_resource_read(root,"A_FILE/00_gf.bin",estate_art_offsets[row],a->data,bytes,error,size)
            || !hmbtn_bank_decode(a->data+toc->offsets[toc->count],bytes-toc->offsets[toc->count],a->atlas,error,size)) goto fail;
    for(unsigned i=0;i<toc->count && i<45;++i) {
        unsigned start=toc->offsets[i],end=toc->offsets[toc->count];
        for(unsigned j=0;j<toc->count;++j) if(toc->offsets[j]>start && toc->offsets[j]<end) end=toc->offsets[j];
        if(start>=end || end>bytes) { snprintf(error,size,"Estate animation outside pack");goto fail; }
        if(!hmbtn_animation_open(&a->sprites[i],a->data+start,end-start,error,size)) goto fail;
    }
    a->row=row;return true;
fail:
    hmbtn_estate_art_close(a);return false;
}
const HmbtnAnimation *hmbtn_estate_art_sprite(const HmbtnEstateArt *a,unsigned index)
{
    return index<45 && a->sprites[index].data ? &a->sprites[index] : NULL;
}

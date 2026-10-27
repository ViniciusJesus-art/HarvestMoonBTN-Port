#include "estate_art.h"
#include "farm_field.h"
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

/* src/overlay/11_GF_FARM/field/field.c: field_cell_look_* and
 * field_cell_look_anim; d3c0f38. Resolve original art without GPU calls or
 * gameplay mutation. Seasons/stages come from the shared crop definition. */
bool hmbtn_estate_crop_look(unsigned seed,unsigned stage,unsigned season,
    unsigned *sprite,unsigned *animation)
{
    if(!stage || !sprite || !animation || season>2) return false;
    unsigned live,dead;
    switch(seed) {
    case 0x22:live=7;dead=7;break;
    case 0x23:live=8;dead=8;break;
    case 0x24:live=9;dead=9;break;
    case 0x25:live=10;dead=10;break;
    case 0x26:live=11;dead=11;break;
    case 0x27:live=12;dead=7;break;
    case 0x28:live=13;dead=8;break;
    case 0x29:live=14;dead=9;break;
    case 0x2a:live=15;dead=10;break;
    case 0x2b:live=16;dead=11;break;
    /* Winter has no crop sprite; season>2 is rejected above. */
    case 0x2c:live=12;dead=0;break;
    case 0x2d:live=13;dead=0;break;
    case 0x2e:live=14;dead=0;break;
    case 0x2f:live=15;dead=0;break;
    case 0x30:live=16;dead=0;break;
    case 0x31:live=12;dead=40;break; /* Moondrop: spring / summer remains. */
    case 0x32:live=17;dead=37;break; /* Pink Cat: summer / fall remains. */
    case 0x34:live=13;dead=41;break; /* Toy Flower: spring / summer remains. */
    case 0x33:case 0x178:live=17;dead=0;break; /* Magic Red, blue variant (fall). */
    case 0x179:live=25;dead=0;break; /* Magic Red, rare red variant (fall). */
    default:return false;
    }
    const HmbtnFarmCrop *crop=hmbtn_farm_crop(seed);
    if(season==crop->season) {
        *sprite=live;
        /* Cabbage/Moondrop/Pink Cat have five ranges; Toy/Magic Red have six.
         * field_cell_look_* requests grown+2 at maturity, just past ranges.
         * anim_draw_sprt_parts reads the first frame's zero dx as the offset
         * and displays frame zero (the mature head/flower). Resolve that
         * result explicitly; never relax the native decoder's bounds. */
        const bool mature_zero=seed==0x26 || seed==0x31 || seed==0x32 || seed==0x34 || seed==0x33 || seed==0x178 || seed==0x179;
        *animation=mature_zero && stage==crop->grown ? 0 : stage+2;
        return true;
    }
    if(season==crop->season+1) { *sprite=dead;*animation=0;return true; }
    return false;
}

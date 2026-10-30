/* farm_load_resource(0/1): shared ANI TOC, outdoor and barn texture banks.
 * No PS1 addresses become host pointers; decoded offsets stay within the pack. */
#include "livestock_art.h"
#include "resource_file.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "livestock_art_tables.inc"
void hmbtn_livestock_art_close(HmbtnLivestockArt *a)
{ free(a->data);free(a->atlas);memset(a,0,sizeof(*a)); }
bool hmbtn_livestock_art_load(HmbtnLivestockArt *a,const char *root,char *error,size_t size)
{
    if(a->data) return true;
    a->data=malloc(LIVESTOCK_FILE_BYTES);a->atlas=malloc(sizeof(*a->atlas));
    if(!a->data || !a->atlas) { snprintf(error,size,"Cannot allocate original livestock art");hmbtn_livestock_art_close(a);return false; }
    bool ok=hmbtn_resource_read(root,"A_FILE/00_gf.bin",LIVESTOCK_FILE_START,a->data,LIVESTOCK_FILE_BYTES,error,size);
    for(unsigned i=0;i<10 && ok;++i) ok=livestock_offsets[i]<livestock_offsets[i+1] && livestock_offsets[i+1]<LIVESTOCK_FILE_BYTES
        && hmbtn_animation_open(&a->animation[i],a->data+livestock_offsets[i],livestock_offsets[i+1]-livestock_offsets[i],error,size);
    if(ok) ok=hmbtn_bank_decode(a->data+livestock_offsets[10],LIVESTOCK_FILE_BYTES-livestock_offsets[10],a->atlas,error,size);
    /* world/scene.c also uploads slot 1 in PLACE_5. Its identical ANI
     * head refers to the additional pregnant-animal textures in this bank. */
    const unsigned texture=livestock_offsets[10];
    uint8_t *barn=ok && texture<LIVESTOCK_BARN_BYTES ? malloc(LIVESTOCK_BARN_BYTES-texture) : NULL;
    if(ok) {
        if(!barn) { snprintf(error,size,"Cannot allocate original barn texture bank");ok=false; }
        else ok=hmbtn_resource_read(root,"A_FILE/00_gf.bin",LIVESTOCK_BARN_START+texture,barn,LIVESTOCK_BARN_BYTES-texture,error,size)
            && hmbtn_bank_apply(barn,LIVESTOCK_BARN_BYTES-texture,a->atlas,error,size);
    }
    free(barn);
    if(!ok) hmbtn_livestock_art_close(a);
    return ok;
}

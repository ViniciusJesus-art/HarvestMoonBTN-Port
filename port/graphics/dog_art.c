#include "dog_art.h"
#include "resource_file.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
void hmbtn_dog_art_close(HmbtnDogArt *a) { for(unsigned i=0;i<2;++i) { free(a->data[i]);free(a->atlas[i]); }memset(a,0,sizeof(*a)); }
bool hmbtn_dog_art_load(HmbtnDogArt *a,const char *root,char *error,size_t size)
{
    if(a->animation[0].data && a->animation[1].data) return true;
    /* dog_sprite_load: spans 1/0, banks +0x1420/+0x23c8. Prepare both so
     * a morning promotion changes only the domain kind, without file I/O. */
    const unsigned offsets[2]={0x3805800,0x37f5800},lengths[2]={0x5800,0x10000},banks[2]={0x1420,0x23c8};
    for(unsigned i=0;i<2;++i) {
        a->data[i]=malloc(lengths[i]);a->atlas[i]=malloc(sizeof(*a->atlas[i]));
        if(!a->data[i] || !a->atlas[i]) { snprintf(error,size,"Cannot allocate original pet art");goto fail; }
        if(!hmbtn_resource_read(root,"A_FILE/00_gf.bin",offsets[i],a->data[i],lengths[i],error,size)
                || !hmbtn_animation_open(&a->animation[i],a->data[i],banks[i],error,size)
                || !hmbtn_bank_decode(a->data[i]+banks[i],lengths[i]-banks[i],a->atlas[i],error,size)) goto fail;
    }
    return true;
fail:hmbtn_dog_art_close(a);return false;
}

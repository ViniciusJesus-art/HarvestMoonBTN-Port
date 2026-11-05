#include "horse_art.h"
#include "resource_file.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
void hmbtn_horse_art_close(HmbtnHorseArt *a)
{ for(unsigned i=0;i<2;++i) { free(a->data[i]);free(a->atlas[i]); }memset(a,0,sizeof(*a)); }
bool hmbtn_horse_art_load(HmbtnHorseArt *a,const char *root,char *error,size_t size)
{
    if(a->animation[0].data && a->animation[1].data) return true;
    /* farm_load_anim8(1/0): pony/adult headers 2104/2592, then texture bank. */
    const unsigned offsets[2]={0x37ed800,0x37e3800},lengths[2]={0x8000,0xa000},banks[2]={2104,2592};
    for(unsigned i=0;i<2;++i) {
        a->data[i]=malloc(lengths[i]);a->atlas[i]=malloc(sizeof(*a->atlas[i]));
        if(!a->data[i] || !a->atlas[i]) { snprintf(error,size,"Cannot allocate original horse art");goto fail; }
        if(!hmbtn_resource_read(root,"A_FILE/00_gf.bin",offsets[i],a->data[i],lengths[i],error,size)
                || !hmbtn_animation_open(&a->animation[i],a->data[i],banks[i],error,size)
                || !hmbtn_bank_decode(a->data[i]+banks[i],lengths[i]-banks[i],a->atlas[i],error,size)) goto fail;
    }
    return true;
fail:hmbtn_horse_art_close(a);return false;
}

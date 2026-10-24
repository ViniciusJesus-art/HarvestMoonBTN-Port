#include "poultry_art.h"
#include "resource_file.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
void hmbtn_poultry_art_close(HmbtnPoultryArt *a) { free(a->data);free(a->atlas);memset(a,0,sizeof(*a)); }
bool hmbtn_poultry_art_load(HmbtnPoultryArt *a,const char *root,char *error,size_t size)
{
    if(a->adult.data) return true;
    a->data=malloc(0x5000);a->atlas=malloc(sizeof(*a->atlas));
    if(!a->data || !a->atlas) { snprintf(error,size,"Cannot allocate chicken art");hmbtn_poultry_art_close(a);return false; }
    const bool ok=hmbtn_resource_read(root,"A_FILE/00_gf.bin",0x380B000,a->data,0x5000,error,size)
        && hmbtn_animation_open(&a->adult,a->data+1008,3072-1008,error,size)
        && hmbtn_bank_decode(a->data+5376,0x5000-5376,a->atlas,error,size);
    if(!ok) hmbtn_poultry_art_close(a);
    return ok;
}

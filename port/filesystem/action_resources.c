#include "action_resources.h"
#include "resource_file.h"
#include <stdio.h>
#include "action_resources.inc"
bool hmbtn_action_resource_locate(unsigned row,HmbtnActionResourceRange *range)
{
    if(!range || row>=28 || !player_action_banks[row]) return false;
    const uint64_t offset=(uint64_t)player_action_base+(uint64_t)row*HMBTN_ACTION_RESOURCE_BYTES;
    if(offset+HMBTN_ACTION_RESOURCE_BYTES>UINT32_MAX || player_action_banks[row]>=HMBTN_ACTION_RESOURCE_BYTES) return false;
    *range=(HmbtnActionResourceRange){(uint32_t)offset,HMBTN_ACTION_RESOURCE_BYTES,player_action_banks[row]};return true;
}
bool hmbtn_action_resource_read(const char *root,unsigned row,uint8_t *data,size_t bytes,
    HmbtnActionResourceRange *range,char *error,size_t size)
{
    if(!data || bytes!=HMBTN_ACTION_RESOURCE_BYTES || !hmbtn_action_resource_locate(row,range)) {
        snprintf(error,size,"Invalid player action resource id or destination size");return false;
    }
    return hmbtn_resource_read(root,"A_FILE/00_gf.bin",range->offset,data,bytes,error,size);
}

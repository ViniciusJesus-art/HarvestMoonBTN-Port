#include "event_resources.h"
#include "resource_file.h"
#include <stdio.h>
#include "event_resources.inc"
bool hmbtn_event_resource_locate(HmbtnEventResourceKind kind,unsigned id,HmbtnEventResourceRange *range)
{
    if(!range) return false;
    *range=(HmbtnEventResourceRange){0};
    uint32_t first,last;const char *archive;
    if(kind==HMBTN_EVENT_RESOURCE_SCRIPT && id+1u>id && id+1u<sizeof(event_offsets)/sizeof(*event_offsets)) {
        first=event_offsets[id];last=event_offsets[id+1];archive="A_FILE/12_event_s.bin";
    } else if(kind==HMBTN_EVENT_RESOURCE_TRIGGERS && id<255) {
        first=event_tables[id][0];last=event_tables[id][1];archive="A_FILE/00_gf.bin";
    } else return false;
    if(last<=first || last-first>1024u*1024u) return false;
    *range=(HmbtnEventResourceRange){archive,first,last-first};return true;
}
bool hmbtn_event_resource_read(const char *root,HmbtnEventResourceKind kind,unsigned id,
    uint8_t *data,size_t bytes,char *error,size_t size)
{
    HmbtnEventResourceRange range;
    if(!data || !hmbtn_event_resource_locate(kind,id,&range) || bytes!=range.bytes) {
        snprintf(error,size,"Invalid event resource id or output size");return false;
    }
    return hmbtn_resource_read(root,range.archive,range.offset,data,bytes,error,size);
}

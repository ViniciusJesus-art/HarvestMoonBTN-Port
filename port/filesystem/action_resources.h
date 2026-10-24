#ifndef HMBTN_ACTION_RESOURCES_H
#define HMBTN_ACTION_RESOURCES_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Stable original farm_load_sector_row index, not an archive offset. */
enum { HMBTN_ACTION_RESOURCE_BYTES=7*2048 };
typedef struct HmbtnActionResourceRange { uint32_t offset;size_t bytes,animation_bytes; } HmbtnActionResourceRange;
bool hmbtn_action_resource_locate(unsigned row,HmbtnActionResourceRange *range);
bool hmbtn_action_resource_read(const char *root,unsigned row,uint8_t *data,size_t bytes,
    HmbtnActionResourceRange *range,char *error,size_t size);
#endif

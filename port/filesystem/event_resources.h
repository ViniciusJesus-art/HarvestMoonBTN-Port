#ifndef HMBTN_EVENT_RESOURCES_H
#define HMBTN_EVENT_RESOURCES_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Original script id, or original seasonal section id for a trigger table. */
typedef enum HmbtnEventResourceKind { HMBTN_EVENT_RESOURCE_SCRIPT,HMBTN_EVENT_RESOURCE_TRIGGERS } HmbtnEventResourceKind;
typedef struct HmbtnEventResourceRange { const char *archive;uint32_t offset;size_t bytes; } HmbtnEventResourceRange;
bool hmbtn_event_resource_locate(HmbtnEventResourceKind kind,unsigned id,HmbtnEventResourceRange *range);
bool hmbtn_event_resource_read(const char *root,HmbtnEventResourceKind kind,unsigned id,
    uint8_t *data,size_t bytes,char *error,size_t size);
#endif

#ifndef HMBTN_TEXT_RESOURCES_H
#define HMBTN_TEXT_RESOURCES_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { HMBTN_TEXT_RECORD_BYTES=8192, HMBTN_PORTRAIT_BANK_BYTES=6144 };
typedef enum HmbtnTextResourceKind {
    HMBTN_TEXT_RECORD, HMBTN_TEXT_PORTRAIT
} HmbtnTextResourceKind;
/* Stable identity from the original tables, independent of byte offsets.
 * RECORD: bank 0..4 + record; PORTRAIT: original face-bank id, record must be 0. */
typedef struct HmbtnTextResourceId {
    HmbtnTextResourceKind kind;
    uint32_t bank,record;
} HmbtnTextResourceId;
typedef struct HmbtnTextResourceRange {
    const char *archive;
    uint32_t offset;
    size_t size;
} HmbtnTextResourceRange;
bool hmbtn_text_resource_locate(HmbtnTextResourceId id,HmbtnTextResourceRange *range);
/* Uses the existing bounded BIN reader. Does not decode or rewrite the data. */
bool hmbtn_text_resource_read(const char *root,HmbtnTextResourceId id,
    uint8_t *destination,size_t size,char *error,size_t error_size);
#endif

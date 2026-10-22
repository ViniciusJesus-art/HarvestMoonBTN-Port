#ifndef HMBTN_RESOURCE_FILE_H
#define HMBTN_RESOURCE_FILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Read exactly one bounded interval; no dependency on the source disc image. */
bool hmbtn_resource_read(const char *root, const char *relative_path, uint32_t offset,
    uint8_t *destination, size_t length, char *error, size_t error_size);

#endif

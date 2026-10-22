#ifndef HMBTN_PSX_DATA_H
#define HMBTN_PSX_DATA_H

#include <limits.h>
#include <stdint.h>

_Static_assert(CHAR_BIT == 8, "PS1 data requires eight-bit bytes");
_Static_assert(sizeof(uint16_t) == 2, "PS1 halfwords require 16 bits");
_Static_assert(sizeof(uint32_t) == 4, "PS1 words require 32 bits");

/* Serialized addresses are data, never native pointers. */
typedef uint32_t HmbtnPsxAddress;

/* Callers provide at least 2 or 4 bytes. Unaligned buffers are supported. */
uint16_t hmbtn_read_le16(const uint8_t *data);
uint32_t hmbtn_read_le32(const uint8_t *data);
void hmbtn_write_le16(uint8_t *data, uint16_t value);
void hmbtn_write_le32(uint8_t *data, uint32_t value);

#endif

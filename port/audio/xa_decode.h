#ifndef HMBTN_XA_DECODE_H
#define HMBTN_XA_DECODE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { HMBTN_XA_SECTOR_BYTES = 2336, HMBTN_XA_MAX_FRAMES = 4032 };
typedef struct HmbtnXADecoder {
    int32_t history[2][2];
    unsigned rate, channels;
} HmbtnXADecoder;

/* Imported XA sectors retain the duplicated subheader, without sync/header.
 * Output is stereo PCM at rate (37800/18900); mono is duplicated to both sides.
 * A decoder belongs to one filtered channel, not to the interleaved file. */
bool hmbtn_xa_decode(HmbtnXADecoder *decoder, const uint8_t *sector, size_t size,
    int16_t *stereo, size_t capacity_frames, size_t *frames,
    char *error, size_t error_size);
#endif

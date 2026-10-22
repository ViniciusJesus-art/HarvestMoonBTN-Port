/* Native 4-bit CD-XA ADPCM data decoder. Format reference (no emulator code):
 * https://psx-spx.consoledev.net/dev/ps1/cdr/cdromformat/#cdrom-xa-audio-adpcm-compression
 * Buffers are bounded, with predictor history carried between filtered sectors. */
#include "xa_decode.h"
#include <stdio.h>
#include <string.h>

static int32_t floor_shift(int32_t n, unsigned shift)
{
    const int32_t d = 1 << shift;
    return n >= 0 ? n/d : -((-n+d-1)/d);
}
bool hmbtn_xa_decode(HmbtnXADecoder *d, const uint8_t *sector, size_t size,
    int16_t *pcm, size_t capacity, size_t *frames, char *error, size_t error_size)
{
    static const int coefficients[4][2] = {{0,0},{60,0},{115,-52},{98,-55}};
    if (frames) *frames = 0;
    if (!d || !sector || !pcm || !frames || size != HMBTN_XA_SECTOR_BYTES
            || memcmp(sector,sector+4,4) || (sector[2]&0x64u) != 0x64u
            || (sector[3]&~5u)) {
        snprintf(error,error_size,"Invalid XA subheader or unsupported coding (requires 4-bit PCM)"); return false;
    }
    const unsigned channels = (sector[3]&1u) ? 2u : 1u;
    const unsigned rate = (sector[3]&4u) ? 18900u : 37800u;
    const size_t count = 4032/channels;
    if (capacity < count || (d->rate && (d->rate != rate || d->channels != channels))) {
        snprintf(error,error_size,"XA output buffer too small or channel format changed"); return false;
    }
    /* Validate before changing the predictor or writing any samples. */
    for (unsigned group = 0; group < 18; ++group)
        for (unsigned unit = 0; unit < 8; ++unit)
            if (sector[8+group*128+4+unit]&0xC0u) {
                snprintf(error,error_size,"Invalid XA ADPCM predictor"); return false;
            }
    d->rate = rate; d->channels = channels;
    for (unsigned group = 0; group < 18; ++group) {
        const uint8_t *p = sector+8+group*128;
        for (unsigned unit = 0; unit < 8; ++unit) {
            const unsigned channel = channels == 2 ? unit&1u : 0u;
            const unsigned parameter = p[4+unit], filter = parameter>>4;
            const unsigned shift = (parameter&15u) > 12 ? 9u : parameter&15u;
            for (unsigned sample = 0; sample < 28; ++sample) {
                int32_t n = (p[16+sample*4+unit/2] >> ((unit&1u)*4))&15u;
                if (n >= 8) n -= 16;
                int32_t value = floor_shift(n*4096,shift)
                    + floor_shift(d->history[channel][0]*coefficients[filter][0]
                        + d->history[channel][1]*coefficients[filter][1]+32,6);
                if (value < -32768) value = -32768;
                if (value > 32767) value = 32767;
                d->history[channel][1] = d->history[channel][0]; d->history[channel][0] = value;
                const size_t at = group*(224/channels)+(unit/channels)*28+sample;
                pcm[at*2+channel] = (int16_t)value;
                if (channels == 1) pcm[at*2+1] = (int16_t)value;
            }
        }
    }
    *frames = count; return true;
}

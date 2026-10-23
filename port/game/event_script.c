/* Host adaptation of event_script.c's block reader and 16-byte step loop. */
#include "event_script.h"
#include "psx_data.h"
#include <stdio.h>
#include <string.h>

static int16_t signed16(const uint8_t *p)
{
    const uint16_t v = hmbtn_read_le16(p);
    return (int16_t)(v < 32768 ? (int32_t)v : (int32_t)v - 65536);
}
static int32_t signed32(const uint8_t *p)
{
    const uint32_t v = hmbtn_read_le32(p);
    return v <= INT32_MAX ? (int32_t)v : -1 - (int32_t)(UINT32_MAX - v);
}
bool hmbtn_event_block_open(HmbtnEventBlock *b, const uint8_t *data, size_t size,
    char *error, size_t error_size)
{
    if (b) memset(b,0,sizeof(*b));
    if (!b || !data || size < 60 || size > 1024*1024) goto invalid;
    const unsigned count = hmbtn_read_le16(data+2);
    const size_t payload = 52 + ((size_t)count+1)*4;
    if (!count || count > 24 || payload > size) goto invalid;
    uint32_t previous = 0;
    unsigned marked = 24;
    for (unsigned i = 0; i <= count; ++i) {
        const uint32_t offset = hmbtn_read_le32(data+52+i*4);
        if (offset < previous || offset > size-payload || (!i && offset)) goto invalid;
        previous = offset;
        if (i < count && (hmbtn_read_le16(data+4+i*2) & 0x4000u)) {
            if (marked != 24) goto invalid;
            marked = i;
        }
    }
    if (marked == 24) goto invalid;
    for (unsigned i = count; i < 24; ++i)
        if (hmbtn_read_le16(data+4+i*2)) goto invalid;
    *b = (HmbtnEventBlock){data,size,payload,count,marked,hmbtn_read_le16(data)};
    return true;
invalid:
    snprintf(error,error_size,"Invalid event block offsets, flags or script entry"); return false;
}
bool hmbtn_event_block_entry(const HmbtnEventBlock *b, unsigned index,
    const uint8_t **data, size_t *size, uint16_t *flags)
{
    if (!b || !b->data || index >= b->count || !data || !size) return false;
    const uint32_t start = hmbtn_read_le32(b->data+52+index*4);
    const uint32_t end = hmbtn_read_le32(b->data+56+index*4);
    *data = b->data+b->payload+start; *size = end-start;
    if (flags) *flags = hmbtn_read_le16(b->data+4+index*2);
    return true;
}
bool hmbtn_event_script_open(HmbtnEventScript *s, const uint8_t *data, size_t size,
    char *error, size_t error_size)
{
    if (s) memset(s,0,sizeof(*s));
    if (!s || !data || !size || size%16 || size > 3296) goto invalid;
    const unsigned count = (unsigned)(size/16);
    bool end = false;
    for (unsigned i = 0; i < count; ++i) {
        const uint8_t *p = data+i*16;
        if (p[0] == 0xFF) { end = true; continue; }
        if (p[0] >= 113 || (p[1] != 0xFF && p[1] >= count)
                || (p[2] != 0xFF && p[2] >= count) || (p[3]&~1u)) goto invalid;
    }
    if (!end) goto invalid;
    *s = (HmbtnEventScript){data,count,0,false,false};
    return true;
invalid:
    snprintf(error,error_size,"Invalid event script length, command, jump or terminator"); return false;
}
bool hmbtn_event_tick(HmbtnEventScript *s, HmbtnEventHandler handler, void *ctx,
    char *error, size_t error_size)
{
    if (!s || !s->data || !handler) goto invalid;
    if (s->finished || s->boundary) return true;
    for (unsigned budget = 0; budget < 256; ++budget) {
        if (s->pc >= s->count) goto invalid;
        const uint8_t *p = s->data+s->pc*16;
        if (*p == 0xFF) { s->finished = true; return true; }
        const HmbtnEventCommand c = {p[0],p[1],p[2],p[3],signed16(p+4),signed16(p+6),signed32(p+8),signed32(p+12)};
        const HmbtnEventResult result = handler(ctx,&c,error,error_size);
        if (result == HMBTN_EVENT_ERROR) return false;
        if (result == HMBTN_EVENT_PENDING) return true;
        if (result == HMBTN_EVENT_BOUNDARY) { s->boundary = true; return true; }
        if (result == HMBTN_EVENT_JUMP) {
            if (s->pc >= s->count) goto invalid;
            if (c.flags & 1u) return true;
            continue;
        }
        const unsigned jump = result == HMBTN_EVENT_DONE ? c.next_done : c.next_other;
        s->pc = jump == 0xFF ? s->pc+1 : jump;
        if (c.flags & 1u) return true;
    }
    snprintf(error,error_size,"Event script exceeded 256 instructions without yielding at step %u",s->pc);
    return false;
invalid:
    snprintf(error,error_size,"Event script cursor outside validated steps"); return false;
}

/* Adapted from src/main/geometry/model.c, model_draw_tmd_object. These packs
 * have prelit colors and repurpose the TMD length bytes for ordering flags. */
#include "model_decode.h"
#include "psx_data.h"
#include <stdio.h>
#include <string.h>

static bool span(size_t size, size_t offset, size_t length)
{
    return offset <= size && length <= size - offset;
}

bool hmbtn_model_object(const HmbtnModel *m, uint32_t index, HmbtnModelObject *o)
{
    if (!m || !o || index >= m->object_count) return false;
    const uint8_t *p = m->data + 12 + (size_t)index * 28;
    o->vertices = hmbtn_read_le32(p);
    o->vertex_count = hmbtn_read_le32(p + 4);
    o->primitives = hmbtn_read_le32(p + 16);
    o->primitive_count = hmbtn_read_le32(p + 20);
    return true;
}

bool hmbtn_model_primitive(const HmbtnModel *m, size_t offset, HmbtnModelPrimitive *p)
{
    if (!m || !p || !span(m->size, offset, 4)) return false;
    const uint8_t *b = m->data + offset;
    const unsigned key = hmbtn_read_le16(b + 2) & 0xFDFFu;
    unsigned indices, colors;
    bool textured = false, gouraud = false;
    memset(p, 0, sizeof(*p));
    switch (key) {
    case 0x2101: p->count = 3; p->bytes = 16; indices = 8; colors = 4; break;
    case 0x2901: p->count = 4; p->bytes = 16; indices = 8; colors = 4; break;
    case 0x3101: p->count = 3; p->bytes = 24; indices = 16; colors = 4; gouraud = true; break;
    case 0x3901: p->count = 4; p->bytes = 28; indices = 20; colors = 4; gouraud = true; break;
    case 0x2501: p->count = 3; p->bytes = 28; indices = 20; colors = 16; textured = true; break;
    case 0x2D01: p->count = 4; p->bytes = 32; indices = 24; colors = 20; textured = true; break;
    case 0x3501: p->count = 3; p->bytes = 36; indices = 28; colors = 16; textured = gouraud = true; break;
    case 0x3D01: p->count = 4; p->bytes = 44; indices = 36; colors = 20; textured = gouraud = true; break;
    default: return false;
    }
    if (!span(m->size, offset, p->bytes)) return false;
    p->command = b[3] & 0xFEu; /* model.c clears the raw-texture bit on output. */
    p->flags = hmbtn_read_le16(b);
    if (textured) {
        p->clut = hmbtn_read_le16(b + 6);
        p->page = hmbtn_read_le16(b + 10);
        if (((p->page >> 7) & 3u) == 3u) return false;
    }
    for (unsigned i = 0; i < p->count; ++i) {
        p->vertex[i] = hmbtn_read_le16(b + indices + i * 2);
        memcpy(p->color[i], b + colors + (gouraud ? i * 4 : 0), 3);
        if (textured) memcpy(p->uv[i], b + 4 + i * 4, 2);
    }
    return true;
}

bool hmbtn_model_open(HmbtnModel *m, const uint8_t *data, size_t size,
    char *error, size_t error_size)
{
    if (m) memset(m, 0, sizeof(*m));
    /* House furniture uses id 0x41; model_bind_tmd/model_draw in the original
     * use the same relative object table and prelit packets for both ids. */
    if (!m || !data || size < 12
            || (hmbtn_read_le32(data) != 0x42 && hmbtn_read_le32(data) != 0x41)
            || hmbtn_read_le32(data + 4) != 0) {
        snprintf(error, error_size, "Invalid TMD header; only relative, prelit models are supported");
        return false;
    }
    HmbtnModel candidate = {data, size, hmbtn_read_le32(data + 8), 0};
    if (!candidate.object_count || candidate.object_count > 1024
            || !span(size, 12, (size_t)candidate.object_count * 28)) goto invalid;
    const size_t table_end = (size_t)candidate.object_count * 28;
    if (candidate.object_count > 1 && !span(size, 12 + table_end,
            4 + (size_t)candidate.object_count * 8)) goto invalid;
    for (uint32_t i = 0; i < candidate.object_count; ++i) {
        HmbtnModelObject object;
        if (!hmbtn_model_object(&candidate, i, &object)
                || object.vertex_count > 65536 || object.primitive_count > 16384
                || (object.vertices & 3u) || (object.primitives & 3u)
                || object.vertices < table_end || object.primitives < table_end
                || !span(size - 12, object.vertices, (size_t)object.vertex_count * 8)
                || !span(size - 12, object.primitives, (size_t)object.primitive_count * 4)
                || object.primitive_count > 16384 - candidate.primitive_count) goto invalid;
        size_t at = 12 + (size_t)object.primitives;
        for (uint32_t j = 0; j < object.primitive_count; ++j) {
            HmbtnModelPrimitive primitive;
            if (!hmbtn_model_primitive(&candidate, at, &primitive)) {
                snprintf(error, error_size, "Unsupported or truncated TMD primitive at object %u, primitive %u",
                    (unsigned)i, (unsigned)j);
                return false;
            }
            for (unsigned k = 0; k < primitive.count; ++k)
                if (primitive.vertex[k] >= object.vertex_count) goto invalid;
            at += primitive.bytes;
        }
        candidate.primitive_count += object.primitive_count;
    }
    *m = candidate;
    return true;
invalid:
    snprintf(error, error_size, "TMD object table, offsets, counts or vertex indices outside bounds");
    return false;
}

void hmbtn_model_vertex(const HmbtnModel *m, const HmbtnModelObject *o,
    uint16_t index, int32_t position[3])
{
    const uint8_t *b = m->data + 12 + (size_t)o->vertices + (size_t)index * 8;
    for (unsigned i = 0; i < 3; ++i) {
        const uint16_t value = hmbtn_read_le16(b + i * 2);
        position[i] = value < 32768 ? (int32_t)value : (int32_t)value - 65536;
    }
}

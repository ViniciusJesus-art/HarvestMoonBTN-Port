#ifndef HMBTN_MODEL_DECODE_H
#define HMBTN_MODEL_DECODE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Read-only view of the game's prelit TMD variant. All offsets are relative to
 * the object table, not host pointers. The caller owns the immutable bytes. */
typedef struct HmbtnModel {
    const uint8_t *data;
    size_t size;
    uint32_t object_count, primitive_count;
    const uint8_t *morph_from, *morph_to; /* Validated first-object vertex arrays. */
    unsigned morph_step, morph_steps;
} HmbtnModel;

typedef struct HmbtnModelObject {
    uint32_t vertices, vertex_count, primitives, primitive_count;
} HmbtnModelObject;

typedef struct HmbtnModelPrimitive {
    uint16_t vertex[4], clut, page, flags;
    uint8_t count, bytes, command;
    uint8_t uv[4][2], color[4][3];
} HmbtnModelPrimitive;

bool hmbtn_model_open(HmbtnModel *model, const uint8_t *data, size_t size,
    char *error, size_t error_size);
bool hmbtn_model_object(const HmbtnModel *model, uint32_t index,
    HmbtnModelObject *object);
bool hmbtn_model_primitive(const HmbtnModel *model, size_t offset,
    HmbtnModelPrimitive *primitive);
void hmbtn_model_vertex(const HmbtnModel *model, const HmbtnModelObject *object,
    uint16_t index, int32_t position[3]);
bool hmbtn_model_morph_bind(HmbtnModel *output,const HmbtnModel *from,const HmbtnModel *to);

#endif

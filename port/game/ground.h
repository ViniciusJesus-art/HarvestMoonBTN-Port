#ifndef HMBTN_GROUND_H
#define HMBTN_GROUND_H
#include "model_decode.h"
typedef struct HmbtnGroundHit {
    int32_t height;
    uint16_t flags;
    uint32_t object;
    size_t primitive;
} HmbtnGroundHit;
/* Original ground_find_object/ground_height_at, without global state or mapped
 * PS1 pointers. The model must have passed hmbtn_model_open. First hit wins. */
bool hmbtn_ground_find(const HmbtnModel *model, const int32_t position[3],
    uint16_t skip_mask, unsigned tolerance, HmbtnGroundHit *hit);
#endif

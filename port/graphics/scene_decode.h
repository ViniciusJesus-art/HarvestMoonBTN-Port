#ifndef HMBTN_SCENE_DECODE_H
#define HMBTN_SCENE_DECODE_H
#include "model_render.h"

enum { HMBTN_SCENE_MODELS = 15 };
typedef struct HmbtnSceneData {
    HmbtnModel terrain, models[HMBTN_SCENE_MODELS];
    uint16_t flags[HMBTN_SCENE_MODELS], angle[HMBTN_SCENE_MODELS][3];
    int16_t spin[HMBTN_SCENE_MODELS][3];
    int32_t position[HMBTN_SCENE_MODELS][3];
    uint32_t primitive_capacity;
} HmbtnSceneData;

/* Immutable section bytes remain owned by the caller. Morphs begin at their
 * first key, as scene_populate_objects/morph_reset do in the original. */
bool hmbtn_scene_data_open(HmbtnSceneData *scene, const uint8_t *bytes, size_t size,
    char *error, size_t error_size);
void hmbtn_scene_data_tick(HmbtnSceneData *scene);
unsigned hmbtn_scene_data_instances(const HmbtnSceneData *scene,
    HmbtnModelInstance instances[HMBTN_SCENE_MODELS+1]);
#endif

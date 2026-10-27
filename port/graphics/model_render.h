#ifndef HMBTN_MODEL_RENDER_H
#define HMBTN_MODEL_RENDER_H

#include "asset_decode.h"
#include "model_decode.h"

typedef struct HmbtnCamera {
    int32_t eye[3], target[3];
    uint16_t projection;
} HmbtnCamera;

typedef struct HmbtnModelRenderer HmbtnModelRenderer;

typedef struct HmbtnModelInstance {
    const HmbtnModel *model;
    int32_t position[3];
    uint16_t angle[3]; /* PS1 rotations: 4096 units per turn, Z then X then Y. */
} HmbtnModelInstance;

typedef struct HmbtnWorldSprite {
    const HmbtnAnimation *animation;
    const HmbtnImageAtlas *atlas;
    HmbtnAnimationPlayer player;
    int32_t position[3]; /* Actor origin; +150,-100,-150 follows player_draw(). */
    int screen_y; /* field_cell_look's lift, in logical pixels. */
} HmbtnWorldSprite;

/* Draw immediately after its owner, sharing exactly the owner's world depth.
 * Geometry and nearer actors occlude the complete owner/attachment group. */
typedef struct HmbtnWorldAttachment {
    unsigned sprite;
    const HmbtnAnimation *animation;
    const HmbtnImageAtlas *atlas;
    HmbtnAnimationPlayer player;
    int screen_x,screen_y;
} HmbtnWorldAttachment;
bool hmbtn_model_render_instances_attached(HmbtnModelRenderer *,
    const HmbtnModelInstance *,unsigned,const HmbtnImageAtlas *,const HmbtnCamera *,
    const HmbtnWorldSprite *,unsigned,const HmbtnWorldAttachment *,unsigned,
    uint8_t *,size_t,uint32_t *,char *,size_t);

HmbtnModelRenderer *hmbtn_model_renderer_create(uint32_t primitive_capacity);
void hmbtn_model_renderer_destroy(HmbtnModelRenderer *renderer);
bool hmbtn_model_project_point(const HmbtnCamera *camera,const int32_t position[3],int *x,int *y);
/* Fixed 320x240 output. The caller clears its frame; this overlays the model. */
bool hmbtn_model_render(HmbtnModelRenderer *renderer, const HmbtnModel *model,
    const HmbtnImageAtlas *atlas, const HmbtnCamera *camera, uint8_t *rgba,
    size_t rgba_size, uint32_t *drawn, char *error, size_t error_size);
bool hmbtn_model_render_sprites(HmbtnModelRenderer *renderer, const HmbtnModel *model,
    const HmbtnImageAtlas *atlas, const HmbtnCamera *camera,
    const HmbtnWorldSprite *sprites, unsigned sprite_count, uint8_t *rgba,
    size_t rgba_size, uint32_t *drawn, char *error, size_t error_size);

/* All instances and sprites share one ordering table, including rotated doors. */
bool hmbtn_model_render_instances(HmbtnModelRenderer *renderer,
    const HmbtnModelInstance *instances, unsigned instance_count,
    const HmbtnImageAtlas *atlas, const HmbtnCamera *camera,
    const HmbtnWorldSprite *sprites, unsigned sprite_count, uint8_t *rgba,
    size_t rgba_size, uint32_t *drawn, char *error, size_t error_size);

#endif

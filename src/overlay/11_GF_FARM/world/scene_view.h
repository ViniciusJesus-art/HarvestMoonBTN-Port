#ifndef SCENE_VIEW_H
#define SCENE_VIEW_H

/* The farm's view of the scene: the result bounds, the image slots, the scrolling camera,
 * the lights and the projection. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "scene.h"

void scene_clamp_bounds_default(void);
void scene_clamp_bounds_set(u16 *low, u16 *high);
void scene_clamp_result(struct FarmResultPair *result);
void slots_clear(void);
void slot_add(FarmBlitFrame *frame);
void scene_mode_queue_run(FarmModeScript *queue);
void slot_image_step(FarmImageSlot *slot);
void slot_rect_step(FarmImageSlot *rect);
void slots_step(void);
void scroll_camera_rotate(void);
void scroll_camera_apply(void);
void farm_scroll_reset(void);
void scene_set_lights(void);
void scene_set_projection(void);
void scroll_camera_draw(void);
void scroll_clear_centre(void);

#endif /* SCENE_VIEW_H */

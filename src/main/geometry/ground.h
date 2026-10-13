#ifndef GROUND_H
#define GROUND_H


/* The ground model: height and clipping against the ground triangles. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "model.h"

void ground_set_tolerance(s32 tolerance);
void ground_enable_objects(Model *model);
s32 ground_height_at(void);
s32 ground_find_object(Model *model, VECTOR *pos);
void ground_set_skip_mask(s32 flags);
void *ground_hit_primitive(void);

#endif /* GROUND_H */

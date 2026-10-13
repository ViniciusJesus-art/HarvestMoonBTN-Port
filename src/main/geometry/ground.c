/* Ground height: the floor under a position, from the terrain triangles and the
 * model objects standing on it. */

#include <sys/types.h>
#include <libgte.h>
#include "types.h"
#include "ground.h"
#include "model.h"

static s32 ground_xz_in_clip(s32 x0, s32 y0, s32 x1, s32 y1);
static s32 ground_xyz_in_clip(long *first, long *second);
static s32 ground_primitive_under(TmdObject *table, TmdObject *object);

SVECTOR *ground_vertices;
s32 ground_count;
u8 *ground_primitives;
s32 ground_mask;
s32 ground_edge0;
s32 ground_edge1;
s32 ground_edge2;
s32 ground_pos_x;
s32 ground_pos_z;
s32 ground_dy;
VECTOR *ground_pos;
s32 ground_tolerance;
/* Half-extent of the ground clip. */
s32 ground_clip;
u8 *ground_hit_prim_ptr;
s32 ground_found;

/* The eight object records ground_listed_object_hit scans (-1: none). */
static s32 ground_objects[8];
/* The current ground model's per-object vertex tables. */
static SVECTOR *ground_triangle[4];

/* The primitive flag bits that ground_primitive_under skips. */
s32 ground_flag_bits = 0x8000;

void ground_set_tolerance(s32 tolerance) {
    ground_tolerance = tolerance;
}

/* Enable every object of the model for ground tests (scale 1; a scale of 0 skips it). */
void ground_enable_objects(Model *model) {
    int i;
    for (i = 0; i < model->tmd->n_obj; i++) {
        TmdObject *object = &model->tmd->objects[i];
        object->scale = 1;
    }
}

/* Height of the plane through the ground triangle ground_triangle at the x/z of
 * *ground_pos: solve normal . (pos - origin) = 0 for y. Returns 0 when
 * ground_found is not positive, and the position's own y when the plane is
 * vertical. */
s32 ground_height_at(void) {
    VECTOR normal;
    VECTOR origin;
    VECTOR dot;
    VECTOR edge0;
    VECTOR edge1;
    VECTOR *pos;

    if (ground_found <= 0) {
        return 0;
    }
    origin.vx = ground_triangle[1]->vx;
    origin.vy = ground_triangle[1]->vy;
    origin.vz = ground_triangle[1]->vz;
    edge0.vx = origin.vx - ground_triangle[0]->vx;
    edge0.vy = origin.vy - ground_triangle[0]->vy;
    edge0.vz = origin.vz - ground_triangle[0]->vz;
    edge1.vx = ground_triangle[2]->vx - origin.vx;
    edge1.vy = ground_triangle[2]->vy - origin.vy;
    edge1.vz = ground_triangle[2]->vz - origin.vz;
    OuterProduct0(&edge0, &edge1, &normal);
    pos = ground_pos;
    dot.vx = (pos->vx - origin.vx) * normal.vx;
    dot.vz = (pos->vz - origin.vz) * normal.vz;
    dot.vy = -(dot.vx + dot.vz);
    if (normal.vy == 0) {
        return pos->vy;
    }
    return dot.vy / normal.vy + origin.vy;
}

/* Find the ground object under `pos`: the first enabled object whose centre is
 * within the clip distance on x/z and whose primitives contain `pos`; none ->
 * 0. Then list (ground_objects, up to eight) every enabled object whose centre is
 * within the clip distance of (pos.x, ground height, pos.z), move the found
 * one to the front, and return 1. */
s32 ground_find_object(Model *model, VECTOR *pos) {
    TmdHeader *tmd;
    ModelCenters *centers;
    s32 count;
    s32 found;
    s32 hits;
    s32 i;
    s32 slot;
    s32 k;
    s32 coord;
    VECTOR here;
    VECTOR center;

    ground_hit_prim_ptr = 0;
    ground_found = 0;
    for (k = 0; k < 8; k++) {
        ground_objects[k] = -1;
    }
    found = -1;
    centers = model->centers;
    tmd = model->tmd;
    ground_pos = pos;
    ground_clip = centers->clip;
    count = tmd->n_obj;
    for (i = 0; i < count; i++) {
        if (tmd->objects[i].scale != 0) {
            if (ground_xz_in_clip(centers->coords[i * 4], centers->coords[i * 4 + 2], pos->vx, pos->vz) != 0
                && ground_primitive_under(tmd->objects, &tmd->objects[i]) == 0) {
                found = i;
                break;
            }
        }
    }
    if (found < 0) {
        return 0;
    }
    hits = 0;
    here.vx = pos->vx;
    here.vy = ground_height_at();
    here.vz = pos->vz;
    for (i = 0, coord = 0; i < count; i++, coord += 4) {
        if (tmd->objects[i].scale != 0) {
            center.vx = centers->coords[coord];
            center.vy = centers->coords[coord + 1];
            center.vz = centers->coords[coord + 2];
            if (ground_xyz_in_clip(&here.vx, &center.vx) != 0) {
                ground_objects[hits] = i;
                hits++;
                if (hits >= 8) {
                    break;
                }
            }
        }
    }
    for (slot = 0; slot < 8; slot++) {
        if (found == ground_objects[slot]) {
            ground_objects[slot] = ground_objects[0];
            ground_objects[0] = found;
            break;
        }
    }
    return 1;
}

/* Return 1 when a primitive of one of the listed objects (ground_objects) is under *pos. */
static s32 ground_listed_object_hit(Model *object, VECTOR *pos) {
    TmdHeader *tmd;
    s32 *entry;
    s32 count;
    s32 offset;

    count = 0;
    tmd = object->tmd;
    entry = ground_objects;
    ground_hit_prim_ptr = 0;
    ground_pos = pos;
    do {
        offset = *entry;
        if (offset >= 0) {
            if (ground_primitive_under(tmd->objects, &tmd->objects[offset]) == 0) {
                count++;
                return 1;
            }
        }
        count++;
        entry++;
    } while (count < 8);
    return 0;
}

/* Whether two points are within the clip limit of each other on both axes. */
static s32 ground_xz_in_clip(s32 x0, s32 y0, s32 x1, s32 y1) {
    s32 limit = (ground_clip >> 1) + 600;
    s32 negative_limit = -limit;

    x0 -= x1;
    if (!(negative_limit < x0) || !(x0 < limit)) return 0;
    y0 -= y1;
    if (!(negative_limit < y0) || !(y0 < limit)) return 0;
    return 1;
}

static s32 ground_xyz_in_clip(long *first, long *second) {
    s32 limit;
    s32 negative_limit;
    s32 delta;

    delta = *first - *second++;
    limit = (ground_clip >> 1) + 600;
    negative_limit = -limit;
    first++;
    if ((delta < negative_limit) || (limit < delta)) return 0;
    delta = *first - *second++;
    first++;
    if ((delta < negative_limit) || (limit < delta)) return 0;
    delta = *first - *second;
    if ((delta < negative_limit) || (limit < delta)) return 0;
    return 1;
}

/* A TMD primitive starts with its header word (olen, ilen, flag, mode); the
 * vertex indices are u16s at a per-type byte offset. */
#define PRIM_HEADER(p) (*(u32 *)(p))
#define PRIM_INDEX(p, offset) (*(u16 *)((p) + (offset)))

/* Return 0 when a primitive of `object` is under *ground_pos (the hit is in
 * ground_hit_prim_ptr), 1 when none is. */
static s32 ground_primitive_under(TmdObject *table, TmdObject *object) {
    s32 count;
    SVECTOR *vertices;
    u8 *primitives;

    s32 ground_triangle_under(SVECTOR *a, SVECTOR *b, SVECTOR *c) {
        VECTOR *pos;

        pos = ground_pos;
        ground_dy = (a->vy + b->vy + c->vy) / 3;
        ground_dy = pos->vy - ground_dy;
        if (ground_dy > ground_tolerance || ground_dy < -ground_tolerance) {
            return 1;
        }
        ground_pos_x = pos->vx;
        ground_pos_z = pos->vz;
        ground_edge0 = (b->vx - a->vx) * (ground_pos_z - a->vz) - (b->vz - a->vz) * (ground_pos_x - a->vx);
        ground_edge1 = (c->vx - b->vx) * (ground_pos_z - b->vz) - (c->vz - b->vz) * (ground_pos_x - b->vx);
        ground_edge2 = (a->vx - c->vx) * (ground_pos_z - c->vz) - (a->vz - c->vz) * (ground_pos_x - c->vx);
        if ((ground_edge0 > 0 || ground_edge1 > 0 || ground_edge2 > 0) &&
            (ground_edge0 < 0 || ground_edge1 < 0 || ground_edge2 < 0)) {
            return 1;
        }
        ground_found = 1;
        ground_triangle[0] = a;
        ground_triangle[1] = b;
        ground_triangle[2] = c;
        return 0;
    }

    s32 ground_quad_under(s32 a, s32 b, s32 c, s32 d) {
        if (a == b || a == c) {
            return ground_triangle_under(&ground_vertices[c], &ground_vertices[b], &ground_vertices[d]);
        }
        if (d == b || d == c) {
            return ground_triangle_under(&ground_vertices[a], &ground_vertices[b], &ground_vertices[c]);
        }
        if (ground_triangle_under(&ground_vertices[a], &ground_vertices[b], &ground_vertices[c]) == 0) {
            return 0;
        }
        return ground_triangle_under(&ground_vertices[d], &ground_vertices[c], &ground_vertices[b]);
    }

    count = object->n_primitive;
    /* TMD tables hold offsets from the model base. */
    vertices = (SVECTOR *)((u8 *)table + object->vert_top);
    primitives = (u8 *)table + object->primitive_top;
    /* Walk the object's primitives, grouped by type in this fixed order, until
     * one under *ground_pos is found (return 0, ground_hit_prim_ptr = that primitive).
     * Primitives whose header has a ground_flag_bits bit set are skipped. */
    ground_hit_prim_ptr = 0;
    ground_count = count;
    ground_vertices = vertices;
    ground_primitives = primitives;
    ground_mask = ground_flag_bits;

#define TRIANGLES(label, code, size, first)                                            \
    label:                                                                             \
        if (ground_count == 0) {                                                         \
            return 1;                                                                  \
        }                                                                              \
        if ((PRIM_HEADER(ground_primitives) & TMD_PRIM_CODE_MASK) != code) {                         \
            goto label##_done;                                                         \
        }                                                                              \
        if (!(PRIM_HEADER(ground_primitives) & ground_mask) &&                                \
            ground_triangle_under(&ground_vertices[PRIM_INDEX(ground_primitives, first)],                  \
                          &ground_vertices[PRIM_INDEX(ground_primitives, first + 2)],              \
                          &ground_vertices[PRIM_INDEX(ground_primitives, first + 4)]) == 0) {      \
            goto hit;                                                                  \
        }                                                                              \
        ground_primitives += size;                                                            \
        ground_count--;                                                                  \
        goto label;                                                                    \
    label##_done:

#define QUADS(label, code, size, first)                                                \
    label:                                                                             \
        if (ground_count == 0) {                                                         \
            return 1;                                                                  \
        }                                                                              \
        if ((PRIM_HEADER(ground_primitives) & TMD_PRIM_CODE_MASK) != code) {                         \
            goto label##_done;                                                         \
        }                                                                              \
        if (!(PRIM_HEADER(ground_primitives) & ground_mask) &&                                \
            ground_quad_under(PRIM_INDEX(ground_primitives, first), PRIM_INDEX(ground_primitives, first + 2), \
                          PRIM_INDEX(ground_primitives, first + 4), PRIM_INDEX(ground_primitives, first + 6)) == 0) { \
            goto hit;                                                                  \
        }                                                                              \
        ground_primitives += size;                                                            \
        ground_count--;                                                                  \
        goto label;                                                                    \
    label##_done:

    TRIANGLES(f3, TMD_PRIM_F3_RAW << 16, 0x10, 0x08)
    TRIANGLES(g3, TMD_PRIM_G3_RAW << 16, 0x18, 0x10)
    TRIANGLES(ft3, TMD_PRIM_FT3_RAW << 16, 0x1C, 0x14)
    TRIANGLES(gt3, TMD_PRIM_GT3_RAW << 16, 0x24, 0x1C)
    QUADS(f4, TMD_PRIM_F4_RAW << 16, 0x10, 0x08)
    QUADS(g4, TMD_PRIM_G4_RAW << 16, 0x1C, 0x14)
    QUADS(ft4, TMD_PRIM_FT4_RAW << 16, 0x20, 0x18)
    QUADS(gt4, TMD_PRIM_GT4_RAW << 16, 0x2C, 0x24)
    return 1;

hit:
    ground_hit_prim_ptr = ground_primitives;
    return 0;
}

void ground_set_skip_mask(signed int flags) {
    ground_flag_bits = flags;
}

void *ground_hit_primitive(void) {
    return ground_hit_prim_ptr;
}

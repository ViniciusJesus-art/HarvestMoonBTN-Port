#ifndef MODEL_H
#define MODEL_H


/* TMD models through the GTE, and the vertex projection. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

/* A TMD model as the EXE's draw and ground code see it. */
typedef struct TmdObject {
    u32 vert_top;
    u32 n_vert;
    u32 normal_top;
    u32 n_normal;
    u32 primitive_top;
    u32 n_primitive;
    s32 scale;              /* doubles as the enable flag */
} TmdObject;

/* A TMD primitive's header halfword: the GPU command code in its high byte (POLY_F3 0x20,
 * POLY_FT3 0x24, ... as libgpu.h's setPoly* macros write them) and a flag byte in its low
 * byte. The _RAW names are the raw-texture mode bit with flag 1; the _FLAG4 names are flag 4. */
enum TmdPrimKey {
    TMD_PRIM_F3 = 0x2000,
    TMD_PRIM_F3_FLAG4 = 0x2004,
    TMD_PRIM_F3_RAW = 0x2101,
    TMD_PRIM_FT3 = 0x2400,
    TMD_PRIM_FT3_RAW = 0x2501,
    TMD_PRIM_F4 = 0x2800,
    TMD_PRIM_F4_FLAG4 = 0x2804,
    TMD_PRIM_F4_RAW = 0x2901,
    TMD_PRIM_FT4 = 0x2C00,
    TMD_PRIM_FT4_RAW = 0x2D01,
    TMD_PRIM_G3 = 0x3000,
    TMD_PRIM_G3_FLAG4 = 0x3004,
    TMD_PRIM_G3_RAW = 0x3101,
    TMD_PRIM_GT3 = 0x3400,
    TMD_PRIM_GT3_RAW = 0x3501,
    TMD_PRIM_G4 = 0x3800,
    TMD_PRIM_G4_FLAG4 = 0x3804,
    TMD_PRIM_G4_RAW = 0x3901,
    TMD_PRIM_GT4 = 0x3C00,
    TMD_PRIM_GT4_RAW = 0x3D01,
    TMD_PRIM_LINE_F2_FLAG1 = 0x4001,    /* GPU 0x40, a monochrome line */
    TMD_PRIM_LINE_G2_FLAG1 = 0x5001,    /* GPU 0x50, a gouraud line */
};

/* The command bits of a header word; the semi-transparency bit (0x02 of the command byte) is left out. */
#define TMD_PRIM_CODE_MASK 0xFDFF0000

typedef struct TmdHeader {
    u32 id;
    u32 flags;
    s32 n_obj;
    TmdObject objects[1];
} TmdHeader;

/* The model's second block: the clip distance, then one centre per object. */
typedef struct ModelCenters {
    s32 clip;
    s16 coords[1];
} ModelCenters;

/* A loaded model: its TMD and its centres. */
typedef struct Model {
    TmdHeader *tmd;
    ModelCenters *centers;
} Model;

/* A position on the screen. */
typedef struct FarmScreenXY {
    s32 x;
    s32 y;
} FarmScreenXY;

s32 gte_project_vertex(FarmScreenXY *screen, SVECTOR *vertex);
void camera_init_projection(void);
void model_load_lighting(void);
void camera_set_eye_roll(s32 x, s32 y, s32 z, s32 roll);
void camera_set_target(s32 x, s32 y, s32 z);
void model_save_gte_rot(void);
void model_compose_gte_matrix(MATRIX *m);
void model_bind_tmd(Model *model, char *file);
void model_draw(Model *model);
s16 model_draw_visible(Model *obj, VECTOR *eye, VECTOR *target);

/* Passed the four packed screen xy words of a quad, which it never reads. */

/* The camera: the eye, the point it looks at, a roll angle, and the view
 * matrix it was locked to (lockedView, NULL = build the view from eye and target). */
typedef struct ViewCamera {
    s32 eyeX;
    s32 eyeY;
    s32 eyeZ;
    s32 targetX;
    s32 targetY;
    s32 targetZ;
    s32 roll;
    MATRIX *lockedView;
} ViewCamera;

#endif /* MODEL_H */

/* Camera, GTE lighting and fog, drawing of TMD models, and the vertex projection. */
#include <sys/types.h>
#include <libgte.h>
#include <inline_c.h>
#include <gtemac.h>
#include <libgpu.h>
#include <libgs.h>
#include <memory.h>
#include "types.h"
#include "camera.h"
#include "cpu_asm.h"
#include "frame_buffers.h"
#include "gte.h"
#include "main.h"
#include "math_util.h"
#include "model.h"
#include "prim.h"

static void camera_build_view(void);
static s32 model_point_in_frustum(SVECTOR *pos);
static void model_draw_tmd_object(TmdObject *table, TmdObject *object);
static s32 model_clip_test3(s32 a, s32 b, s32 c);
static s32 model_clip_test4(s32 x0, s32 x1, s32 x2, s32 x3);
static void camera_frustum_setup(VECTOR *eye, VECTOR *target);
static void model_apply_scale(void);

TmdHeader *current_tmd;
FrameState *model_frame;
FrameState *quad_frame;
FrameState *last_quad_frame;
s16 drawn_parts;

static ViewCamera model_camera;
static MATRIX view_matrix;
static MATRIX base_matrix;
static MATRIX light_matrix;
static MATRIX color_matrix;
static MATRIX saved_matrix;
static ClipPlane clip_planes[4];
static VECTOR view_direction;

#define PACKET_WORD(p, off) (*(u32 *)((p) + (off)))
#define PACKET_HALF(p, off) (*(u16 *)((p) + (off)))
#define PRIM_CODE(p) ((p)[7])
#define PRIM_MODE(p) (*(u16 *)((p) + 2))
#define PRIM_CLUT(p) (*(u16 *)((p) + 6))
#define PRIM_TPAGE(p) (*(u16 *)((p) + 0xA))
#define VRAM_PAGE_WIDTH 64
#define VRAM_PAGE_HEIGHT 256
#define CLUT_WIDTH 16
/* Shift a primitive's texture by whole pages and its CLUT by 16-pixel columns and rows. */
#define MOVE_TEXTURE(prim, page_x, page_y, clut_x, clut_y) \
    do { \
        s16 row; \
        PRIM_TPAGE(prim) = PRIM_TPAGE(prim) + (page_x) + ((page_y) << 4); \
        row = (PRIM_CLUT(prim) >> 6) + (clut_y); \
        PRIM_CLUT(prim) = ((PRIM_CLUT(prim) & 0x3F) + (clut_x)) | (row << 6); \
    } while (0)

typedef struct {
    u8 u0, v0;
    u16 clut;
    u8 u1, v1;
    u16 tpage;
    u8 u2, v2;
    u8 pad0A[2];
    u8 u3, v3;
} TexQuad;

typedef struct {
    u32 uvClut0;
    u32 uvTpage1;
    u32 uv2;
    u32 uv3;
    u32 rgbCode;
    SVECTOR corner[4];
} TexturedQuad;

/* Set the GTE offset and screen distance, put the eye and target at their start points, and
 * make the base, light and colour matrices start clear (base is the identity). */
void camera_init_projection(void) {
    SetGeomOffset(160, 120);
    SetGeomScreen(1000);
    camera_set_eye_roll(0, 0, 0x1000, 0x800);
    camera_set_target(0, 0, -0x1000);
    memset(&light_matrix, 0, sizeof(MATRIX));
    memset(&color_matrix, 0, sizeof(MATRIX));
    memset(&base_matrix, 0, sizeof(MATRIX));
    base_matrix.m[2][2] = FIXED_ONE;
    base_matrix.m[1][1] = FIXED_ONE;
    base_matrix.m[0][0] = FIXED_ONE;
}

/* Non-zero while a model scale is in effect. */
s32 scale_enabled = 0;
s32 fog_near = 32000;
CVECTOR back_color = {128, 128, 128, 0};
CVECTOR far_color = {128, 128, 128, 0};
s32 last_quad_depth = 0;

/* Numerator and denominator for each of the three axes, as model_set_scale_axes fills them. */
static s32 scale_pairs[6] = {1, 1, 1, 1, 1, 1};
/* The rotation angles that turn the four frustum side planes: two about Y, two about X. */
static s32 frustum_angles[4] = {2156, -108, 93, 1962};
static u16 model_keys[20] = {
    TMD_PRIM_FT4, TMD_PRIM_GT4, TMD_PRIM_FT3, TMD_PRIM_GT3, TMD_PRIM_F4, TMD_PRIM_G4, TMD_PRIM_F3, TMD_PRIM_G3,
    TMD_PRIM_F3_FLAG4, TMD_PRIM_G3_FLAG4, TMD_PRIM_F4_FLAG4, TMD_PRIM_G4_FLAG4, TMD_PRIM_F3_RAW, TMD_PRIM_G3_RAW,
    TMD_PRIM_FT3_RAW, TMD_PRIM_GT3_RAW, TMD_PRIM_F4_RAW, TMD_PRIM_G4_RAW, TMD_PRIM_FT4_RAW, TMD_PRIM_GT4_RAW,
};
static s32 model_values[20] = {
    0x20, 0x24, 0x18, 0x1C, 0x14, 0x18, 0x10, 0x14, 0x18, 0x1C,
    0x20, 0x24, 0x10, 0x18, 0x1C, 0x24, 0x10, 0x1C, 0x20, 0x2C,
};

/* Build the view, then load the fog, back and far colours and the colour and light matrices
 * into the GTE. */
void model_load_lighting(void) {
    camera_build_view();
    SetFogNear(fog_near, 1000);
    gte_SetBackColor(back_color.r, back_color.g, back_color.b);
    SetFarColor(far_color.r, far_color.g, far_color.b);
    gte_SetColorMatrix(&color_matrix);
    gte_SetLightMatrix(&light_matrix);
}

/* Rebuild the look-at view from the eye, target and roll, unless the view is locked. */
static void camera_build_view(void) {
    VECTOR v;
    VECTOR forward;
    VECTOR side;
    MATRIX roll;
    s32 i;
    long *forward_words;
    long *side_words;

    if (model_camera.lockedView == 0) {
        v.vx = model_camera.targetX - model_camera.eyeX;
        v.vy = model_camera.targetY - model_camera.eyeY;
        v.vz = model_camera.targetZ - model_camera.eyeZ;
        VectorNormalS(&v, (SVECTOR *)view_matrix.m[2]);
        v.vy = 0;
        if (view_matrix.m[2][0] == 0 && view_matrix.m[2][2] == 0) {
            v.vx = FIXED_ONE;
            v.vz = 0;
        } else {
            v.vx = view_matrix.m[2][2];
            v.vz = -view_matrix.m[2][0];
        }
        VectorNormalS(&v, (SVECTOR *)view_matrix.m[0]);
        forward_words = &forward.vx;
        side_words = &side.vx;
        for (i = 0; i < 3; i++) {
            forward_words[i] = view_matrix.m[2][i];
            side_words[i] = view_matrix.m[0][i];
        }
        gte_OuterProduct12(&forward, &side, &v);
        VectorNormalS(&v, (SVECTOR *)view_matrix.m[1]);
        if (model_camera.roll != 0) {
            roll = GsIDMATRIX;
            RotMatrixZ(model_camera.roll, &roll);
            gte_MulMatrix0(&roll, &view_matrix, &view_matrix);
        }
        v.vx = -model_camera.eyeX;
        v.vy = -model_camera.eyeY;
        v.vz = -model_camera.eyeZ;
        ApplyMatrixLV(&view_matrix, &v, (VECTOR *)view_matrix.t);
    }
    gte_SetRotMatrix(&view_matrix);
    gte_SetTransMatrix(&view_matrix);
}

/* Set the eye position and roll, and unlock the view. */
void camera_set_eye_roll(s32 x, s32 y, s32 z, s32 roll) {
    model_camera.eyeX = x;
    model_camera.eyeY = y;
    model_camera.eyeZ = z;
    model_camera.roll = roll;
    model_camera.lockedView = 0;
}

static void camera_get_eye(signed int *x, signed int *y, signed int *z) {
    *x = model_camera.eyeX;
    *y = model_camera.eyeY;
    *z = model_camera.eyeZ;
}

void camera_set_target(signed int x, signed int y, signed int z) {
    ViewCamera *p;
    p = &model_camera;
    p->targetX = x;
    p->targetY = y;
    p->targetZ = z;
    p->lockedView = 0;
}

/* A locked view is copied in and no longer rebuilt. */
static void camera_lock_view(MATRIX *view) {
    view_matrix = *view;
    model_camera.lockedView = view;
}

static void camera_get_view(MATRIX *view) {
    *view = view_matrix;
}

/* The yaw part of the view: a matrix that turns about Y, built from the view's first row. */
static void camera_yaw_matrix(MATRIX *out) {
    out->m[0][0] = view_matrix.m[0][0];
    out->m[0][1] = view_matrix.m[0][1];
    out->m[0][2] = view_matrix.m[0][2];
    out->m[2][0] = view_matrix.m[0][2];
    out->m[2][1] = view_matrix.m[0][1];
    out->m[2][2] = -view_matrix.m[0][0];
    out->m[1][0] = 0;
    out->m[1][1] = FIXED_ONE;
    out->m[1][2] = 0;
}

/* Save the GTE's current rotation matrix to saved_matrix. */
void model_save_gte_rot(void) {
    gte_ReadRotMatrix(&saved_matrix);
}

/* Multiply m onto saved_matrix and load the result into the GTE. */
void model_compose_gte_matrix(MATRIX *m) {
    MATRIX *saved = &saved_matrix;

    gte_SetRotMatrix(saved);
    gte_ldclmv(&m->m[0][0]);
    gte_rtir();
    gte_stclmv(&saved->m[0][0]);
    gte_ldclmv(&m->m[0][1]);
    gte_rtir();
    gte_stclmv(&saved->m[0][1]);
    gte_ldclmv(&m->m[0][2]);
    gte_rtir();
    gte_stclmv(&saved->m[0][2]);
    gte_SetTransMatrix(saved);
    gte_ldlv0(m->t);
    gte_rtv0();
    gte_stlvl(saved->t);
    gte_SetRotMatrix(saved);
    gte_SetTransMatrix(saved);
}

/* Point light `id` along the normalised direction dir, with colour r, g, b (0 to 255,
 * scaled to the GTE's range). */
static void model_set_light_dir(s32 id, SVECTOR *dir, u8 r, u8 g, u8 b) {
    SVECTOR n;

    VectorNormalSS(dir, &n);
    light_matrix.m[id][0] = n.vx;
    light_matrix.m[id][1] = n.vy;
    light_matrix.m[id][2] = n.vz;
    color_matrix.m[0][id] = r << 4;
    color_matrix.m[1][id] = g << 4;
    color_matrix.m[2][id] = b << 4;
}

/* Light `id` along the direction of the two rotation angles ax (about X) then ay (about Y),
 * with colour r, g, b (0 to 255, scaled to the GTE's range). */
static void model_set_light_angles(s32 id, s16 ax, s16 ay, s16 az, u8 r, u8 g, u8 b) {
    SVECTOR angle;
    MATRIX m;

    angle.vx = ax;
    angle.vy = ay;
    angle.vz = az;
    m = GsIDMATRIX;
    m.m[0][0] = -m.m[0][0];
    m.m[1][1] = -m.m[1][1];
    m.m[2][2] = -m.m[2][2];
    RotMatrixX(ax, &m);
    RotMatrixY(ay, &m);
    light_matrix.m[id][0] = m.m[2][0];
    light_matrix.m[id][1] = m.m[2][1];
    light_matrix.m[id][2] = m.m[2][2];
    color_matrix.m[0][id] = r << 4;
    color_matrix.m[1][id] = g << 4;
    color_matrix.m[2][id] = b << 4;
}

static void model_set_back_color(u8 r, u8 g, u8 b) {
    back_color.r = r;
    back_color.g = g;
    back_color.b = b;
}

static void model_set_fog_color(u8 r, u8 g, u8 b) {
    far_color.r = r;
    far_color.g = g;
    far_color.b = b;
}

static void model_set_fog_near(s32 value) {
    fog_near = value;
}

/* With more than one object, the centres block follows the object table. */
void model_bind_tmd(Model *model, char *file) {
    TmdHeader *tmd;
    s32 n;

    tmd = (TmdHeader *)file;
    model->tmd = tmd;
    n = tmd->n_obj;
    if (n >= 2) {
        model->centers = (ModelCenters *)&tmd->objects[n];
    } else {
        model->centers = 0;
    }
}

static void model_unbind(Model *model) {
    model->tmd = 0;
    model->centers = 0;
}

/* Draw every object of the model, with the model scale applied when one is set. */
void model_draw(Model *model) {
    s32 i;

    current_tmd = model->tmd;
    PushMatrix();
    if (scale_enabled != 0) {
        SCRATCHPAD_STACK_ENTER();
        model_apply_scale();
        SCRATCHPAD_STACK_LEAVE();
    }
    for (i = 0; i < current_tmd->n_obj; i++) {
        SCRATCHPAD_STACK_ENTER();
        model_draw_tmd_object(current_tmd->objects, &current_tmd->objects[i]);
        SCRATCHPAD_STACK_LEAVE();
    }
    PopMatrix();
}

/* Draw the parts of a model that lie inside the view frustum. Returns how many were drawn. */
s16 model_draw_visible(Model *obj, VECTOR *eye, VECTOR *target) {
    TmdHeader *model;
    ModelCenters *positions;
    s32 i;

    drawn_parts = 0;
    SCRATCHPAD_STACK_ENTER();
    positions = obj->centers;
    model = obj->tmd;
    if (positions == 0) {
        return 0;
    }
    camera_frustum_setup(eye, target);
    PushMatrix();
    if (scale_enabled != 0) {
        model_apply_scale();
    }
    for (i = 0; i < model->n_obj; i++) {
        if (model_point_in_frustum((SVECTOR *)&positions->coords[i * 4]) != 0) {
            model_draw_tmd_object(model->objects, &model->objects[i]);
        }
    }
    PopMatrix();
    SCRATCHPAD_STACK_LEAVE();
    return drawn_parts;
}

/* Is the point inside the frustum? Its direction from the view point must be on the inner side
 * of all four side planes. */
static s32 model_point_in_frustum(SVECTOR *pos) {
    SVECTOR delta;
    SVECTOR dir;
    SVECTOR *n;
    ClipPlane *plane;
    s32 dot;

    delta.vx = (view_direction.vx - pos->vx) / 128;
    delta.vy = (view_direction.vy - pos->vy) / 128;
    delta.vz = (view_direction.vz - pos->vz) / 128;
    n = &dir;
    VectorNormalSS(&delta, n);
    plane = clip_planes;
    dot = plane->x * n->vx;
    dot += plane->y * n->vy;
    dot += plane->z * n->vz;
    if (dot < 0) {
        return 0;
    }
    plane++;
    dot = plane->x * n->vx;
    dot += plane->y * n->vy;
    dot += plane->z * n->vz;
    if (dot < 0) {
        return 0;
    }
    plane++;
    dot = plane->x * n->vx;
    dot += plane->y * n->vy;
    dot += plane->z * n->vz;
    if (dot < 0) {
        return 0;
    }
    plane++;
    dot = plane->x * n->vx;
    dot += plane->y * n->vy;
    dot += plane->z * n->vz;
    return dot >= 0;
}

/* Transform, cull and queue the primitives of one TMD object into the ordering table.
 * The packet list is a run of each primitive type in turn, and each type has its own pass. */
static void model_draw_tmd_object(TmdObject *table, TmdObject *object) {
    s32 count;
    SVECTOR *verts;
    u8 *packet;
    u8 *prim;
    u_long *ot;
    u32 header;
    s32 depth;

    count = object->n_primitive;
    if (count <= 0) {
        return;
    }
    debug_hook_run(0x2000);
    verts = (SVECTOR *)((u8 *)table + object->vert_top);
    prim = current_frame->primCursor.bytes;
    packet = (u8 *)table + object->primitive_top;
    ot = current_frame->ot;
    model_frame = current_frame;
    /* flat triangles */
loopF3:
    if (count <= 0) {
        goto done;
    }
    header = PACKET_WORD(packet, 0);
    if ((header & TMD_PRIM_CODE_MASK) != (TMD_PRIM_F3_RAW << 16)) {
        goto loopG3;
    }
    {
        u8 *out = prim;
        u8 *source = packet;
        u_long *slot;
        gte_ldv3(&verts[PACKET_HALF(packet, 0x8)], &verts[PACKET_HALF(packet, 0xA)],
                 &verts[PACKET_HALF(packet, 0xC)]);
        packet += 0x10;
        count--;
        gte_rtpt_b();
        PACKET_WORD(out, 0x4) = PACKET_WORD(source, 0x4);
        gte_stflg(&depth);
        if (depth < 0) {
            goto loopF3;
        }
        gte_nclip_b();
        PRIM_CODE(out) = 0x20 | ((header >> 24) & 2);
        gte_stopz(&depth);
        if (depth <= 0) {
            goto loopF3;
        }
        gte_stsxy3_f3(out);
        if (!model_clip_test3(PACKET_WORD(out, 0x8), PACKET_WORD(out, 0xC), PACKET_WORD(out, 0x10))) {
            goto loopF3;
        }
        gte_avsz3_b();
        prim += 0x14;
        gte_stotz(&depth);
        if (!(header & 0x8000)) {
            depth += 0x23;
        }
        if (header & 0x4000) {
            depth += 1;
        }
        slot = &ot[depth / 4];
        LINK_PRIM(slot, out, POLY_F3);
        drawn_parts++;
    }
    goto loopF3;
    /* gouraud triangles */
loopG3:
    if (count <= 0) {
        goto done;
    }
    header = PACKET_WORD(packet, 0);
    if ((header & TMD_PRIM_CODE_MASK) != (TMD_PRIM_G3_RAW << 16)) {
        goto loopFT3;
    }
    {
        u8 *out = prim;
        u8 *source = packet;
        u_long *slot;
        gte_ldv3(&verts[PACKET_HALF(packet, 0x10)], &verts[PACKET_HALF(packet, 0x12)],
                 &verts[PACKET_HALF(packet, 0x14)]);
        packet += 0x18;
        count--;
        gte_rtpt_b();
        PACKET_WORD(out, 0x4) = PACKET_WORD(source, 0x4);
        PACKET_WORD(out, 0xC) = PACKET_WORD(source, 0x8);
        gte_stflg(&depth);
        if (depth < 0) {
            goto loopG3;
        }
        gte_nclip_b();
        PACKET_WORD(out, 0x14) = PACKET_WORD(source, 0xC);
        gte_stopz(&depth);
        if (depth <= 0) {
            goto loopG3;
        }
        gte_stsxy3_g3(out);
        if (!model_clip_test3(PACKET_WORD(out, 0x8), PACKET_WORD(out, 0x10), PACKET_WORD(out, 0x18))) {
            goto loopG3;
        }
        gte_avsz3_b();
        PRIM_CODE(out) = 0x30 | ((header >> 24) & 2);
        prim += 0x1C;
        gte_stotz(&depth);
        if (!(header & 0x8000)) {
            depth += 0x23;
        }
        if (header & 0x4000) {
            depth += 1;
        }
        slot = &ot[depth / 4];
        LINK_PRIM(slot, out, POLY_G3);
        drawn_parts++;
    }
    goto loopG3;
    /* textured flat triangles */
loopFT3:
    if (count <= 0) {
        goto done;
    }
    header = PACKET_WORD(packet, 0);
    if ((header & TMD_PRIM_CODE_MASK) != (TMD_PRIM_FT3_RAW << 16)) {
        goto loopGT3;
    }
    {
        u8 *out = prim;
        u8 *source = packet;
        u_long *slot;
        gte_ldv3(&verts[PACKET_HALF(packet, 0x14)], &verts[PACKET_HALF(packet, 0x16)],
                 &verts[PACKET_HALF(packet, 0x18)]);
        packet += 0x1C;
        count--;
        gte_rtpt_b();
        PACKET_WORD(out, 0xC) = PACKET_WORD(source, 0x4);
        PACKET_WORD(out, 0x14) = PACKET_WORD(source, 0x8);
        gte_stflg(&depth);
        if (depth < 0) {
            goto loopFT3;
        }
        gte_nclip_b();
        PACKET_WORD(out, 0x1C) = PACKET_WORD(source, 0xC);
        gte_stopz(&depth);
        if (depth <= 0) {
            goto loopFT3;
        }
        gte_stsxy3_ft3(out);
        if (!model_clip_test3(PACKET_WORD(out, 0x8), PACKET_WORD(out, 0x10), PACKET_WORD(out, 0x18))) {
            goto loopFT3;
        }
        gte_avsz3_b();
        PACKET_WORD(out, 0x4) = PACKET_WORD(source, 0x10);
        PRIM_CODE(out) = 0x24 | ((header >> 24) & 2);
        gte_stotz(&depth);
        if (!(header & 0x8000)) {
            depth += 0x23;
        }
        if (header & 0x4000) {
            depth += 1;
        }
        slot = &ot[depth / 4];
        LINK_PRIM(slot, out, POLY_FT3);
        drawn_parts++;
        prim += 0x20;
    }
    goto loopFT3;
    /* textured gouraud triangles */
loopGT3:
    if (count <= 0) {
        goto done;
    }
    header = PACKET_WORD(packet, 0);
    if ((header & TMD_PRIM_CODE_MASK) != (TMD_PRIM_GT3_RAW << 16)) {
        goto loopF4;
    }
    {
        u8 *out = prim;
        u8 *source = packet;
        u_long *slot;
        gte_ldv3(&verts[PACKET_HALF(packet, 0x1C)], &verts[PACKET_HALF(packet, 0x1E)],
                 &verts[PACKET_HALF(packet, 0x20)]);
        packet += 0x24;
        count--;
        gte_rtpt_b();
        PACKET_WORD(out, 0x4) = PACKET_WORD(source, 0x10);
        PACKET_WORD(out, 0x10) = PACKET_WORD(source, 0x14);
        gte_stflg(&depth);
        if (depth < 0) {
            goto loopGT3;
        }
        gte_nclip_b();
        PACKET_WORD(out, 0x1C) = PACKET_WORD(source, 0x18);
        gte_stopz(&depth);
        if (depth <= 0) {
            goto loopGT3;
        }
        gte_stsxy3_gt3(out);
        if (!model_clip_test3(PACKET_WORD(out, 0x8), PACKET_WORD(out, 0x14), PACKET_WORD(out, 0x20))) {
            goto loopGT3;
        }
        gte_avsz3_b();
        PACKET_WORD(out, 0xC) = PACKET_WORD(source, 0x4);
        PACKET_WORD(out, 0x18) = PACKET_WORD(source, 0x8);
        gte_stotz(&depth);
        if (!(header & 0x8000)) {
            depth += 0x23;
        }
        if (header & 0x4000) {
            depth += 1;
        }
        PACKET_WORD(out, 0x24) = PACKET_WORD(source, 0xC);
        PRIM_CODE(out) = 0x34 | ((header >> 24) & 2);
        slot = &ot[depth / 4];
        LINK_PRIM(slot, out, POLY_GT3);
        drawn_parts++;
        prim += 0x28;
    }
    goto loopGT3;
    /* flat quads */
loopF4:
    if (count <= 0) {
        goto done;
    }
    header = PACKET_WORD(packet, 0);
    if ((header & TMD_PRIM_CODE_MASK) != (TMD_PRIM_F4_RAW << 16)) {
        goto loopG4;
    }
    {
        u8 *out = prim;
        u8 *source = packet;
        u_long *slot;
        gte_ldv3(&verts[PACKET_HALF(packet, 0x8)], &verts[PACKET_HALF(packet, 0xA)],
                 &verts[PACKET_HALF(packet, 0xC)]);
        packet += 0x10;
        count--;
        gte_rtpt_b();
        PACKET_WORD(out, 0x4) = PACKET_WORD(source, 0x4);
        gte_stflg(&depth);
        if (depth < 0) {
            goto loopF4;
        }
        gte_nclip_b();
        gte_stopz(&depth);
        if (depth <= 0) {
            goto loopF4;
        }
        gte_stsxy3_f4(out);
        gte_ldv0(&verts[PACKET_HALF(source, 0xE)]);
        gte_rtps();
        PRIM_CODE(out) = 0x28 | ((header >> 24) & 2);
        gte_stflg(&depth);
        if (depth < 0) {
            goto loopF4;
        }
        gte_stsxy(&PACKET_WORD(out, 0x14));
        if (!model_clip_test4(PACKET_WORD(out, 0x8), PACKET_WORD(out, 0xC), PACKET_WORD(out, 0x10),
        PACKET_WORD(out, 0x14))) {
            goto loopF4;
        }
        gte_avsz4_b();
        prim += 0x18;
        gte_stotz(&depth);
        if (!(header & 0x8000)) {
            depth += 0x23;
        }
        if (header & 0x4000) {
            depth += 1;
        }
        slot = &ot[depth / 4];
        LINK_PRIM(slot, out, POLY_F4);
        drawn_parts++;
    }
    goto loopF4;
    /* gouraud quads */
loopG4:
    if (count <= 0) {
        goto done;
    }
    header = PACKET_WORD(packet, 0);
    if ((header & TMD_PRIM_CODE_MASK) != (TMD_PRIM_G4_RAW << 16)) {
        goto loopFT4;
    }
    {
        u8 *out = prim;
        u8 *source = packet;
        u_long *slot;
        gte_ldv3(&verts[PACKET_HALF(packet, 0x14)], &verts[PACKET_HALF(packet, 0x16)],
                 &verts[PACKET_HALF(packet, 0x18)]);
        packet += 0x1C;
        count--;
        gte_rtpt_b();
        PACKET_WORD(out, 0x4) = PACKET_WORD(source, 0x4);
        PACKET_WORD(out, 0xC) = PACKET_WORD(source, 0x8);
        gte_stflg(&depth);
        if (depth < 0) {
            goto loopG4;
        }
        gte_nclip_b();
        PACKET_WORD(out, 0x14) = PACKET_WORD(source, 0xC);
        gte_stopz(&depth);
        if (depth <= 0) {
            goto loopG4;
        }
        gte_stsxy3_g4(out);
        gte_ldv0(&verts[PACKET_HALF(source, 0x1A)]);
        gte_rtps();
        PACKET_WORD(out, 0x1C) = PACKET_WORD(source, 0x10);
        gte_stflg(&depth);
        if (depth < 0) {
            goto loopG4;
        }
        gte_stsxy(&PACKET_WORD(out, 0x20));
        if (!model_clip_test4(PACKET_WORD(out, 0x8), PACKET_WORD(out, 0x10), PACKET_WORD(out, 0x18),
                               PACKET_WORD(out, 0x20))) {
            goto loopG4;
        }
        gte_avsz4_b();
        PRIM_CODE(out) = 0x38 | ((header >> 24) & 2);
        prim += 0x24;
        gte_stotz(&depth);
        if (!(header & 0x8000)) {
            depth += 0x23;
        }
        if (header & 0x4000) {
            depth += 1;
        }
        slot = &ot[depth / 4];
        LINK_PRIM(slot, out, POLY_G4);
        drawn_parts++;
    }
    goto loopG4;
    /* textured flat quads */
loopFT4:
    if (count <= 0) {
        goto done;
    }
    header = PACKET_WORD(packet, 0);
    if ((header & TMD_PRIM_CODE_MASK) != (TMD_PRIM_FT4_RAW << 16)) {
        goto loopGT4;
    }
    {
        u8 *out = prim;
        u8 *source = packet;
        u_long *slot;
        gte_ldv3(&verts[PACKET_HALF(packet, 0x18)], &verts[PACKET_HALF(packet, 0x1A)],
                 &verts[PACKET_HALF(packet, 0x1C)]);
        packet += 0x20;
        count--;
        gte_rtpt_b();
        PACKET_WORD(out, 0x4) = PACKET_WORD(source, 0x14);
        PACKET_WORD(out, 0xC) = PACKET_WORD(source, 0x4);
        gte_stflg(&depth);
        if (depth < 0) {
            goto loopFT4;
        }
        gte_nclip_b();
        PACKET_WORD(out, 0x1C) = PACKET_WORD(source, 0xC);
        gte_stopz(&depth);
        if (depth <= 0) {
            goto loopFT4;
        }
        gte_stsxy3_ft4(out);
        gte_ldv0(&verts[PACKET_HALF(source, 0x1E)]);
        gte_rtps();
        PACKET_WORD(out, 0x24) = PACKET_WORD(source, 0x10);
        gte_stflg(&depth);
        if (depth < 0) {
            goto loopFT4;
        }
        gte_stsxy(&PACKET_WORD(out, 0x20));
        if (!model_clip_test4(PACKET_WORD(out, 0x8), PACKET_WORD(out, 0x10), PACKET_WORD(out, 0x18),
                               PACKET_WORD(out, 0x20))) {
            goto loopFT4;
        }
        gte_avsz4_b();
        PACKET_WORD(out, 0x14) = PACKET_WORD(source, 0x8);
        PRIM_CODE(out) = 0x2C | ((header >> 24) & 2);
        gte_stotz(&depth);
        if (!(header & 0x8000)) {
            depth += 0x23;
        }
        if (header & 0x4000) {
            depth += 1;
        }
        slot = &ot[depth / 4];
        LINK_PRIM(slot, out, POLY_FT4);
        drawn_parts++;
        prim += 0x28;
    }
    goto loopFT4;
    /* textured gouraud quads */
loopGT4:
    if (count == 0) {
        goto done;
    }
    header = PACKET_WORD(packet, 0);
    if ((header & TMD_PRIM_CODE_MASK) != (TMD_PRIM_GT4_RAW << 16)) {
        goto done;
    }
    {
        u8 *out = prim;
        u8 *source = packet;
        u_long *slot;
        gte_ldv3(&verts[PACKET_HALF(packet, 0x24)], &verts[PACKET_HALF(packet, 0x26)],
                 &verts[PACKET_HALF(packet, 0x28)]);
        packet += 0x2C;
        count--;
        gte_rtpt_b();
        PACKET_WORD(out, 0x4) = PACKET_WORD(source, 0x14);
        PACKET_WORD(out, 0x10) = PACKET_WORD(source, 0x18);
        gte_stflg(&depth);
        if (depth < 0) {
            goto loopGT4;
        }
        gte_nclip_b();
        PACKET_WORD(out, 0x1C) = PACKET_WORD(source, 0x1C);
        gte_stopz(&depth);
        if (depth <= 0) {
            goto loopGT4;
        }
        gte_stsxy3_gt4(out);
        gte_ldv0(&verts[PACKET_HALF(source, 0x2A)]);
        gte_rtps();
        PACKET_WORD(out, 0x28) = PACKET_WORD(source, 0x20);
        gte_stflg(&depth);
        if (depth < 0) {
            goto loopGT4;
        }
        gte_stsxy(&PACKET_WORD(out, 0x2C));
        if (!model_clip_test4(PACKET_WORD(out, 0x8), PACKET_WORD(out, 0x14), PACKET_WORD(out, 0x20),
        PACKET_WORD(out, 0x2C))) {
            goto loopGT4;
        }
        gte_avsz4();
        PACKET_WORD(out, 0xC) = PACKET_WORD(source, 0x4);
        PACKET_WORD(out, 0x18) = PACKET_WORD(source, 0x8);
        PACKET_WORD(out, 0x24) = PACKET_WORD(source, 0xC);
        PACKET_WORD(out, 0x30) = PACKET_WORD(source, 0x10);
        PRIM_CODE(out) = 0x3C | ((header >> 24) & 2);
        gte_stotz(&depth);
        if (!(header & 0x8000)) {
            depth += 0x23;
        }
        if (header & 0x4000) {
            depth += 1;
        }
        slot = &ot[depth / 4];
        LINK_PRIM(slot, out, POLY_GT4);
        drawn_parts++;
        prim += 0x34;
    }
    goto loopGT4;
done:
    model_frame->primCursor.bytes = prim;
}

/* The clip bands reach this far left of the screen: the tests add it to x. */
#define CLIP_BAND_MARGIN 19

/* Screen clip tests on the x of a packed XY word (its low halfword). The drawing band is
 * x in [-19, 320), the wide band [-19, 400). */
static inline s32 near_x(s16 x) {
    return (u16)(x + CLIP_BAND_MARGIN) < 320 + CLIP_BAND_MARGIN;
}

static inline s32 wide_x(s16 x) {
    return (u16)(x + CLIP_BAND_MARGIN) < 400 + CLIP_BAND_MARGIN;
}

static s32 model_clip_test3(s32 a, s32 b, s32 c) {
    s16 x;

    if (!wide_x(a + 1) && !wide_x(b + 1) && !wide_x(c + 1)) {
        return 0;
    }
    x = a;
    if (near_x(x)) return 1;
    x = b;
    if (near_x(x)) return 1;
    x = c;
    return near_x(x);
}

/* Is any of the four screen corners inside the drawing band? */
static s32 model_clip_test4(s32 x0, s32 x1, s32 x2, s32 x3) {
    s16 x;

    if (x0 == 0) x = 0;
    x = x0;
    if (near_x(x)) return 1;
    x = x1;
    if (near_x(x)) return 1;
    x = x2;
    if (near_x(x)) return 1;
    x = x3;
    return near_x(x);
}

/* Build the four side planes of the view frustum from the eye and target. */
static void camera_frustum_setup(VECTOR *eye, VECTOR *target) {
    MATRIX m[4];
    VECTOR normal;
    VECTOR dir;
    s32 i;
    s16 *out;
    s16 *row;

    out = &clip_planes[0].x;
    dir.vx = target->vx - eye->vx;
    dir.vy = target->vy - eye->vy;
    dir.vz = target->vz - eye->vz;
    VectorNormal(&dir, &normal);
    view_direction.vx = eye->vx - normal.vx * 2;
    m[0] = GsIDMATRIX;
    m[1] = GsIDMATRIX;
    view_direction.vy = eye->vy - normal.vy * 2;
    view_direction.vz = eye->vz - normal.vz * 2;
    m[2] = GsIDMATRIX;
    m[3] = GsIDMATRIX;
    RotMatrixY(frustum_angles[0], &m[0]);
    RotMatrixY(frustum_angles[1], &m[1]);
    RotMatrixX(frustum_angles[2], &m[2]);
    RotMatrixX(frustum_angles[3], &m[3]);
    PushMatrix();
    for (i = 0; i < 4; i++) {
        row = m[i].m[i / 2];
        gte_SetRotMatrix(&m[i]);
        gte_ldclmv(&view_matrix.m[0][0]);
        gte_rtir();
        gte_stclmv(&m[i].m[0][0]);
        gte_ldclmv(&view_matrix.m[0][1]);
        gte_rtir();
        *out++ = *row++;
        gte_stclmv(&m[i].m[0][1]);
        gte_ldclmv(&view_matrix.m[0][2]);
        gte_rtir();
        *out++ = *row++;
        gte_stclmv(&m[i].m[0][2]);
        *out++ = *row++;
    }
    PopMatrix();
}

/* Set every axis to the same numerator and denominator, and enable the scale. */
static void model_set_scale_uniform(signed int numerator, signed int denominator) {
    scale_pairs[4] = numerator;
    scale_pairs[2] = numerator;
    scale_pairs[0] = numerator;
    scale_pairs[5] = denominator;
    scale_pairs[3] = denominator;
    scale_pairs[1] = denominator;
    scale_enabled = 1;
}

/* Set the numerator and denominator of each of the three axes, and enable the scale. */
static void model_set_scale_axes(signed int num0, signed int den0, signed int num1, signed int den1,
                   signed int num2, signed int den2) {
    scale_pairs[0] = num0;
    scale_pairs[1] = den0;
    scale_pairs[2] = num1;
    scale_pairs[3] = den1;
    scale_enabled = 1;
    scale_pairs[4] = num2;
    scale_pairs[5] = den2;
}

static void model_clear_scale(void) {
    scale_enabled = 0;
}

/* Scale the rotation matrix by the (numerator, denominator) pairs in scale_pairs.
 * A pair with a numerator of 0 or 1, or a denominator below 2, is skipped. */
static void model_apply_scale(void) {
    MATRIX m;
    s32 i;

    gte_ReadRotMatrix(&m);
    for (i = 0; i < 3; i++) {
        if ((scale_pairs[0] & ~1) && scale_pairs[1] >= 2) {
            m.m[i][0] = m.m[i][0] * scale_pairs[0] / scale_pairs[1];
        }
        if ((scale_pairs[2] & ~1) && scale_pairs[3] >= 2) {
            m.m[i][1] = m.m[i][1] * scale_pairs[2] / scale_pairs[3];
        }
        if ((scale_pairs[4] & ~1) && scale_pairs[5] >= 2) {
            m.m[i][2] = m.m[i][2] * scale_pairs[4] / scale_pairs[5];
        }
    }
    gte_SetRotMatrix(&m);
}

/* The value paired with key `key` (compared as a halfword) in the two
 * columns at model_keys / model_values, or 0 when no key matches. */
static s32 model_key_value(s32 key) {
    s32 i;

    key &= 0xFFFF;
    for (i = 0; i < 20; i++) {
        if (model_keys[i] == key) {
            return model_values[i];
        }
    }
    return 0;
}

/* Relocate every textured primitive of a TMD by (tex_x, tex_y) in VRAM pixels and
 * its CLUT by (clut_x, clut_y), walking each object's primitive list. */
static void model_relocate_textures(TmdHeader **model, s32 tex_x, s32 tex_y, s32 clut_x, s32 clut_y) {
    TmdHeader *tmd;
    TmdObject *objects;
    TmdObject *object;
    s32 i;
    s32 j;
    s32 count;
    u8 *prim;

    tex_x /= VRAM_PAGE_WIDTH;
    tex_y /= VRAM_PAGE_HEIGHT;
    clut_x /= CLUT_WIDTH;
    tmd = *model;
    objects = tmd->objects;
    for (i = 0; i < tmd->n_obj; i++) {
        object = &tmd->objects[i];
        prim = (u8 *)objects + object->primitive_top;
        count = object->n_primitive;
        for (j = 0; j < count; j++) {
            switch (PRIM_MODE(prim) & 0xFDFF) {
            case TMD_PRIM_FT3:
                MOVE_TEXTURE(prim, tex_x, tex_y, clut_x, clut_y);
                /* fallthrough */
            case TMD_PRIM_F3_FLAG4:
            case TMD_PRIM_G3_RAW:
            case TMD_PRIM_G4:
                prim += 0x18;
                break;
            case TMD_PRIM_F4:
            case TMD_PRIM_G3:
                prim += 0x14;
                break;
            case TMD_PRIM_FT3_RAW:
            case TMD_PRIM_GT3:
                MOVE_TEXTURE(prim, tex_x, tex_y, clut_x, clut_y);
                /* fallthrough */
            case TMD_PRIM_G3_FLAG4:
            case TMD_PRIM_G4_RAW:
                prim += 0x1C;
                break;
            case TMD_PRIM_GT3_RAW:
            case TMD_PRIM_GT4:
                MOVE_TEXTURE(prim, tex_x, tex_y, clut_x, clut_y);
                /* fallthrough */
            case TMD_PRIM_G4_FLAG4:
                prim += 0x24;
                break;
            case TMD_PRIM_FT4:
            case TMD_PRIM_FT4_RAW:
                MOVE_TEXTURE(prim, tex_x, tex_y, clut_x, clut_y);
                /* fallthrough */
            case TMD_PRIM_F4_FLAG4:
                prim += 0x20;
                break;
            case TMD_PRIM_GT4_RAW:
                MOVE_TEXTURE(prim, tex_x, tex_y, clut_x, clut_y);
                prim += 0x2C;
                break;
            case TMD_PRIM_LINE_F2_FLAG1:
                prim += 0xC;
                break;
            case TMD_PRIM_F3:
            case TMD_PRIM_F3_RAW:
            case TMD_PRIM_F4_RAW:
            case TMD_PRIM_LINE_G2_FLAG1:
                prim += 0x10;
                break;
            }
        }
    }
}

/* Set a quad's texture coordinates, page and CLUT for a w x h rectangle at VRAM (x, y). */
static void model_quad_set_texture(TexQuad *quad, s32 x, s32 y, s32 w, s32 h, s32 cx, s32 cy) {
    setUVWH(quad, (x % VRAM_PAGE_WIDTH) * 4, y % VRAM_PAGE_HEIGHT, w - 1, h - 1);
    quad->tpage = GetTPage(0, 0, x, y);
    quad->clut = GetClut(cx, cy);
}

/* Stub: always rejects the quad. */
static signed int model_quad_clip_check(s32 xy0, s32 xy1, s32 xy2, s32 xy3) {
    return 0;
}

/* Transform a textured quad into a POLY_FT4 and link it into the ordering table at its
 * average depth, biased by depthBias. Returns the primitive, or 0 if it is clipped or back-facing. */
static PolyFT4 *model_draw_quad_depth(TexturedQuad *quad, s32 depthBias) {
    PolyFT4 *prim;
    s32 value;

    gte_ldv3(&quad->corner[0], &quad->corner[1], &quad->corner[2]);
    quad_frame = frame_records;
    prim = quad_frame->primCursor.ft4Words;
    gte_rtpt_b();
    prim->uvClut0 = quad->uvClut0;
    prim->uvTpage1 = quad->uvTpage1;
    gte_stflg(&value);
    if (value < 0) {
        return 0;
    }
    gte_nclip_b();
    prim->uv2 = quad->uv2;
    gte_stopz(&value);
    if (value <= 0) {
        return 0;
    }
    gte_stsxy3_ft4(prim);
    gte_ldv0(&quad->corner[3]);
    gte_rtps();
    prim->uv3 = quad->uv3;
    gte_stflg(&value);
    if (value < 0) {
        return 0;
    }
    gte_stsxy(&prim->xy3);
    gte_avsz4();
    if (model_quad_clip_check(prim->xy0, prim->xy1, prim->xy2, prim->xy3) == 0) {
        return 0;
    }
    gte_stotz(&value);
    *(u32 *)&prim->r0 = quad->rgbCode;
    setPolyFT4(prim);
    last_quad_depth = (value + depthBias) >> (14 - quad_frame->otDepthBits);
    ot_insert_prim(last_quad_depth, sizeof(PolyFT4));
    return prim;
}

/* Transform a textured quad and queue it as a POLY_FT4 at ordering-table slot otIndex.
 * Returns the primitive, or 0 if it is clipped or back-facing. */
static PolyFT4 *model_draw_quad_ot(TexturedQuad *quad, s32 otIndex) {
    FrameState *fb;
    PolyFT4 *prim;
    s32 flag;

    gte_ldv3(&quad->corner[0], &quad->corner[1], &quad->corner[2]);
    fb = frame_records;
    prim = fb->primCursor.ft4Words;
    last_quad_frame = fb;
    gte_rtpt_b();
    prim->uvClut0 = quad->uvClut0;
    prim->uvTpage1 = quad->uvTpage1;
    gte_stflg(&flag);
    if (flag < 0) {
        return 0;
    }
    gte_nclip_b();
    prim->uv2 = quad->uv2;
    gte_stopz(&flag);
    if (flag <= 0) {
        return 0;
    }
    gte_stsxy3_ft4(prim);
    gte_ldv0(&quad->corner[3]);
    gte_rtps();
    prim->uv3 = quad->uv3;
    gte_stflg(&flag);
    if (flag < 0) {
        return 0;
    }
    gte_stsxy(&prim->xy3);
    gte_avsz4();
    if (model_quad_clip_check(prim->xy0, prim->xy1, prim->xy2, prim->xy3) == 0) {
        return 0;
    }
    gte_stotz(&flag);
    *(u32 *)&prim->r0 = quad->rgbCode;
    setlen(prim, 9);
    prim->code = 0x2C;
    ot_insert_prim(otIndex, sizeof(PolyFT4));
    return prim;
}

/* The ordering-table depth of the last quad that model_draw_quad_depth linked. */
static s32 model_last_quad_depth(void) {
    return last_quad_depth;
}

/* Projecting vertices through the GTE: the perspective transform, its short-vector and depth variants. */

/* Perspective-transform one vertex with the current GTE matrix. Returns 0 when
 * the transform overflowed (FLAG's error bit 31 set); otherwise stores the
 * screen X/Y into `screen` (if given) and returns SZ3 / 4, the ordering-table
 * depth. */
s32 gte_project_vertex(FarmScreenXY *screen, SVECTOR *vertex) {
    s16 sxy[2];
    s32 value;

    gte_ldv0(vertex);
    gte_rtps();
    gte_stflg(&value);
    if (value < 0) {
        return 0;
    }
    gte_stsxy2(sxy);
    if (screen != 0) {
        screen->x = sxy[0];
        screen->y = sxy[1];
    }
    gte_stszotz(&value);
    return value;
}

/* Project the vertex whose x, y and z are the low halves of in[0], in[2] and in[4], and return
 * its depth as a byte. */
static s32 gte_project_halfwords(void *out, u16 *in) {
    u16 *half = in;
    SVECTOR temp;
    temp.vx = half[0];
    temp.vy = half[2];
    temp.vz = half[4];
    temp.pad = 0;
    return gte_project_vertex(out, &temp) & 0xff;
}

/* Project one vertex with the current GTE matrix and return its ordering-table depth. */
static s32 gte_project_vertex_otz(SVECTOR *vertex) {
    s32 screen[2];
    s32 otz;

    gte_ldv0(vertex);
    gte_rtps();
    gte_stsxy(screen);
    gte_stotz(&otz);
    return otz;
}

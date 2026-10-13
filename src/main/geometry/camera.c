#include <sys/types.h>
#include <libgte.h>
#include "types.h"
#include "camera.h"
#include "math_util.h"

static void camera_set_view(s32 x, s32 y, s32 z);

u8 camera_mode = 0;
u8 camera_ready = 0;
s8 camera_dirty = 0;
s32 camera_mode_arg = 0;
s32 camera_view_settled = 0;
s32 camera_view_x = 0;
s32 camera_view_y = 0;
s32 camera_view_z = 0;
s32 camera_block_mode = 0;
s32 camera_focus_index = 0;
s32 camera_unused_8005E30C = 0;
s32 camera_unused_8005E310 = 0;
s32 camera_unused_8005E314 = 0;

static s32 camera_eye[3] = {0, 0, 0};

static s32 camera_target[3] = {0, 0, 0};

static s32 camera_eye_copy[3] = {0, 0, 0};

/* Initialize the camera geometry and reset its startup state. */
void camera_init(void) {
    s32 *camera;

    camera_set_view(800, 600, 0);
    camera = camera_eye;
    camera_focus_index = -1;
    camera_unused_8005E30C = 0;
    camera_unused_8005E310 = 0;
    camera_unused_8005E314 = 0;
    camera_dirty = 0;
    camera_target[2] = 0;
    camera_target[1] = 0;
    camera_target[0] = 0;
    vec_from_polar(camera, camera_view_x, camera_view_y, camera_view_z);
    vec3_copy(camera_eye_copy, camera);
    camera_ready = 1;
    camera_mode = 2;
}

s8 camera_dirty;
s32 camera_view_settled;
s32 camera_view_x;
s32 camera_view_y;
s32 camera_view_z;

/* Store the view (x, y, z) and, when it moved, mark the camera dirty and unsettled. */
static void camera_set_view(s32 x, s32 y, s32 z) {
    if ((x != camera_view_x) || (y != camera_view_y) || (z != camera_view_z)) {
        camera_dirty = 1;
        camera_view_settled = 0;
    }
    camera_view_x = x;
    camera_view_y = y;
    camera_view_z = z;
}

/* Return the view x and y, and the angle of the eye seen from the target, each when asked for. */
static void camera_get_view_args(signed int *x, signed int *y, signed int *angle) {
    signed int offset[3];

    if (x != 0) {
        *x = camera_view_x;
    }
    if (y != 0) {
        *y = camera_view_y;
    }
    if (angle != 0) {
        vec3_copy(offset, camera_eye);
        vec3_sub(offset, camera_target);
        *angle = ratan2(offset[2], offset[0]);
    }
}

u8 camera_mode;

s32 camera_block_mode;

static s32 camera_block[3] = {0, 0, 0};

/* In mode 2, copy `src` into the camera block when it has changed, unless the block mode is 1. */
static void camera_refresh_block(s32 *src) {
    s32 mode;

    if (camera_mode == 2) {
        if ((src[0] != camera_block[0]) || (src[1] != camera_block[1]) ||
            (src[2] != camera_block[2])) {
            mode = camera_block_mode;
            if (mode != 1) {
                vec3_copy(camera_block, src);
                camera_dirty = 1;
                camera_view_settled = 0;
            }
        }
    }
}

static void camera_get_target(signed int *out) {
    vec3_copy(out, camera_target);
}

static void camera_deactivate(void) {
    camera_mode = 0;
}

signed int camera_mode_arg;
/* Set the mode argument, marking the camera dirty when it changes. */
static void camera_set_mode(signed int mode) {
    if (camera_mode_arg != mode) {
        camera_dirty = 1;
    }
    camera_mode_arg = mode;
}

/* The farm's view of the scene: the result bounds, the image slots, the scrolling camera,
 * the lights and the projection. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include <memory.h>
#include "types.h"
#include "event_flags.h"
#include "farm_probe.h"
#include "farm_state.h"
#include "game_types.h"
#include "model.h"
#include "scene.h"
#include "scene_view.h"

typedef struct FarmResultPair
{
    s16 first;
    u8 pad2[2];
    s16 second;
} FarmResultPair;

static GsF_LIGHT scene_lights[3];
static ResultClampBounds result_clamp_bounds;

/* A slot with this id is empty. Bit 15 of a slot's id marks an image animation. */
#define FARM_SLOT_EMPTY 0xFFFF
#define FARM_SLOT_IMAGE_FLAG 0x8000
/* The distance of the farm's projection plane. */
#define FARM_PROJECTION_DISTANCE 1000

/* Set the clamp bounds to their default values. */
void scene_clamp_bounds_default(void)
{
    result_clamp_bounds.firstMin = -13000;
    result_clamp_bounds.unk2 = 0;
    result_clamp_bounds.secondMax = 4500;
    result_clamp_bounds.firstMax = -8000;
    result_clamp_bounds.unkA = 440;
    result_clamp_bounds.secondMin = 2500;
}

/* Set the clamp bounds from two triples: low sets the minimums and the second maximum, high
 * sets the first maximum and the second minimum. */
void scene_clamp_bounds_set(u16 *low, u16 *high)
{
    result_clamp_bounds.firstMin = low[0];
    result_clamp_bounds.unk2 = low[1];
    result_clamp_bounds.secondMax = low[2];
    result_clamp_bounds.firstMax = high[0];
    result_clamp_bounds.unkA = high[1];
    result_clamp_bounds.secondMin = high[2];
}

/* Clamp the farm result pair against the configured bounds. */
void scene_clamp_result(FarmResultPair *result)
{
    s16 second;
    s16 first;

    if (result->first < result_clamp_bounds.firstMin)
        result->first = result_clamp_bounds.firstMin;
    second = result->second;
    if (result_clamp_bounds.secondMax < second)
        result->second = result_clamp_bounds.secondMax;
    first = result->first;
    if (result_clamp_bounds.firstMax < first)
        result->first = result_clamp_bounds.firstMax;
    if (result->second < result_clamp_bounds.secondMin)
        result->second = result_clamp_bounds.secondMin;
}

/* Empty the eight image slots of the mode script and mark each empty (FARM_SLOT_EMPTY). */
void slots_clear(void) {
    FarmLoadArea *fs;
    FarmImageSlot *p;
    u32 i;
    u16 value;
    memset(&farm_area.mode.slots, 0, sizeof(farm_area.mode.slots));
    i = 0;
    value = FARM_SLOT_EMPTY;
    fs = &farm_area;
    p = fs->mode.slots.entries;
    do {
        p->id = value;
        i++;
        p++;
    } while (i < 8);
}

/* Find the slot already playing this frame's animation, or fill the first free one with it. */
void slot_add(FarmBlitFrame *frame) {
    FarmImageSlot *slot;
    s32 index;

    slot = farm_area.mode.slots.entries;
    index = 0;
    do {
        if (frame->id == slot->id) {
            return;
        }
        index++;
        slot++;
    } while (index < 8);

    slot = farm_area.mode.slots.entries;
    index = 0;
    do {
        if (slot->id == FARM_SLOT_EMPTY) {
            memset(slot, 0, sizeof(FarmImageSlot));
            slot->id = frame->id;
            slot->duration = frame->duration;
            slot->next = frame->next;
            slot->x = frame->x;
            slot->y = frame->y;
            slot->image.x = frame->image.x;
            slot->image.y = frame->image.y;
            slot->image.w = frame->image.w;
            slot->image.h = frame->image.h;
            return;
        }
        index++;
        slot++;
    } while (index < 8);
}

/* Free the first slot that holds `id`. */
static void slot_remove_id(s16 id)
{
    s32 i;
    FarmImageSlot *p;
    p = farm_area.mode.slots.entries;
    i = 0;
    do
    {
        i++;
        if (p->id == id)
        {
            memset(p, 0, sizeof(FarmImageSlot));
            p->id = FARM_SLOT_EMPTY;
            return;
        }
    } while (i < 8);
}

/* Add the queued frames to the scene's slots, skipping empty ones. At PLACE_F nothing
 * is added while it rains or snows. */
void scene_mode_queue_run(FarmModeScript *queue)
{
    FarmBlitFrame *entry;
    FarmBlitFrame *next_entry;
    u32 slot;
    u16 sentinel;
    if (queue->head.place.location == PLACE_F)
    {
        if ((u32)((u16)game_state.world.weather - 1) < 2U)
            return;
    }
    slot = 0;
    sentinel = FARM_SLOT_EMPTY;
    entry = queue->frames;
    next_entry = entry;
    do
    {
        if (entry->id != sentinel)
            slot_add(next_entry);
        next_entry++;
        slot += 1;
        entry++;
    } while (slot < 8U);
}

/* Draw a scene slot's image, then step to its next frame once the current one has
 * shown for its duration. The winter house draws nothing until EVENT_FLAG_45 is set. */
void slot_image_step(FarmImageSlot *slot)
{
    RECT image;
    FarmBlitFrame *frame;

    if (farm_area.mode.data->head.scene == AREA_HOUSE2_WINTER && event_flag_get(EVENT_FLAG_45) == 0)
    {
        return;
    }
    image.x = slot->image.x;
    image.y = slot->image.y;
    image.w = slot->image.w;
    image.h = slot->image.h;
    MoveImage(&image, slot->x, slot->y);
    if (farm_area.mode.place.location == PLACE_2C && (slot->id & 1))
    {
        MoveImage(&image, slot->x + 8, slot->y);
        MoveImage(&image, slot->x + 16, slot->y);
    }
    slot->timer++;
    if (slot->timer >= slot->duration)
    {
        frame = &farm_area.mode.data->frames[slot->next];
        slot->duration = frame->duration;
        slot->next = frame->next;
        slot->x = frame->x;
        slot->y = frame->y;
        slot->image.x = frame->image.x;
        slot->image.y = frame->image.y;
        slot->image.w = frame->image.w;
        slot->image.h = frame->image.h;
        slot->timer = 0;
    }
}

/* Advance a rectangle animation (the slot's image scrolls vertically within itself): move
 * its image in two slices, and step the scroll every `next` frames, wrapping at the image's
 * height. */
void slot_rect_step(FarmImageSlot *rect) {
    RECT image;
    u16 height;
    u16 count;
    u16 next;
    image.x = rect->image.x;
    image.y = rect->image.y;
    image.w = rect->image.w;
    image.h = rect->image.h - rect->timer;
    MoveImage(&image, rect->x, rect->y + rect->timer);
    height = rect->timer;
    if (height != 0) {
        image.h = height;
        image.x = rect->image.x;
        image.y = rect->image.y + rect->image.h - height;
        image.w = rect->image.w;
        MoveImage(&image, rect->x, rect->y);
    }
    count = rect->count;
    if (count == rect->next) {
        next = rect->timer + 2;
        rect->timer = next;
        rect->count = 0;
        if (!(rect->timer < rect->image.h)) {
            rect->timer = 0;
        }
    } else {
        rect->count = count + 1;
    }
}

/* Step every used slot unless the game is paused: an image animation (bit 15 of the id)
 * or a rectangle one. */
void slots_step(void) {
    FarmImageSlot *entry = farm_area.mode.slots.entries;
    s32 i;

    for (i = 0; i < 8; i++, entry++) {
        if (entry->id == FARM_SLOT_EMPTY) continue;
        if (game_state.globalFlags & GLOBAL_FLAG_PAUSED) continue;
        if (entry->id & FARM_SLOT_IMAGE_FLAG) {
            slot_image_step(entry);
        } else {
            slot_rect_step(entry);
        }
    }
}

/* Turn the camera's distance (z) by the yaw into an eye offset, and store the eye's position
 * in the scroll record: x and z from the rotation, y from the eye height. */
void scroll_camera_rotate(void) {
    CameraWork work;

    work.rotation = GsIDMATRIX;
    work.translation = GsIDMATRIX;
    PushMatrix();
    work.angle.vx = 0;
    work.angle.vz = 0;
    work.angle.vy = farm_area.mode.scroll.yaw;
    RotMatrix(&work.angle, &work.rotation);
    SetRotMatrix(&work.rotation);
    SetTransMatrix(&work.translation);
    work.angle.vx = 0;
    work.angle.vy = 0;
    work.angle.vz = farm_area.mode.scroll.z;
    RotTrans(&work.angle, &work.position, &work.flag);
    PopMatrix();
    farm_area.mode.scroll.a18 = work.position.vx;
    farm_area.mode.scroll.b1A = farm_area.mode.scroll.x + farm_area.mode.scroll.centre.vy;
    farm_area.mode.scroll.c1C = work.position.vz;
}

/* Aim the game camera from the stage camera's eye (offset from its centre) at the centre. */
void scroll_camera_apply(void) {
    camera_set_eye_roll(farm_area.mode.scroll.a18 + farm_area.mode.scroll.centre.vx, farm_area.mode.scroll.b1A,
                  farm_area.mode.scroll.c1C + farm_area.mode.scroll.centre.vz, 0);
    camera_set_target(farm_area.mode.scroll.centre.vx, farm_area.mode.scroll.centre.vy,
                  farm_area.mode.scroll.centre.vz);
}

/* Reset the scroll to its start: z -9000, x -4500, yaw -512. */
void farm_scroll_reset(void)
{
    farm_area.mode.scroll.z = -9000;
    farm_area.mode.scroll.x = -4500;
    farm_area.mode.scroll.unused8 = 0;
    farm_area.mode.scroll.yaw = -512;
    farm_area.mode.scroll.unused10 = 0;
}

/* Set the farm's three flat lights, all pointing along (100, 100, 100): light 0
 * light grey, lights 1 and 2 black. */
void scene_set_lights(void)
{
    s32 direction;

    direction = 100;
    scene_lights[0].vx = direction;
    scene_lights[0].vy = direction;
    scene_lights[0].vz = direction;
    scene_lights[0].r = 0xD0;
    scene_lights[0].g = 0xD0;
    scene_lights[0].b = 0xD0;
    GsSetFlatLight(0, &scene_lights[0]);
    scene_lights[1].vx = direction;
    scene_lights[1].vy = direction;
    scene_lights[1].vz = direction;
    scene_lights[1].r = 0;
    scene_lights[1].g = 0;
    scene_lights[1].b = 0;
    GsSetFlatLight(1, &scene_lights[1]);
    scene_lights[2].vx = direction;
    scene_lights[2].vy = direction;
    scene_lights[2].vz = direction;
    scene_lights[2].r = 0;
    scene_lights[2].g = 0;
    scene_lights[2].b = 0;
    GsSetFlatLight(2, &scene_lights[2]);
    GsSetAmbient(0x800, 0x800, 0x800);
    GsSetLightMode(0);
}

/* Set the projection for the farm's 3D view. */
void scene_set_projection(void)
{
    GsSetProjection(FARM_PROJECTION_DISTANCE);
    SetGeomScreen(FARM_PROJECTION_DISTANCE);
}

/* Draw the stage's model as seen from the stage camera's eye toward its centre. */
void scroll_camera_draw(void) {
    VECTOR eye;
    VECTOR target;

    eye.vx = farm_area.mode.scroll.a18 + farm_area.mode.scroll.centre.vx;
    eye.vy = farm_area.mode.scroll.b1A;
    eye.vz = farm_area.mode.scroll.c1C + farm_area.mode.scroll.centre.vz;
    target.vx = farm_area.mode.scroll.centre.vx;
    target.vy = farm_area.mode.scroll.centre.vy;
    target.vz = farm_area.mode.scroll.centre.vz;
    model_draw_visible(&farm_model, &eye, &target);
}

/* Zero the scroll's eye and its centre. */
void scroll_clear_centre(void)
{
    farm_area.mode.scroll.a18 = 0;
    farm_area.mode.scroll.b1A = 0;
    farm_area.mode.scroll.c1C = 0;
    farm_area.mode.scroll.centre.vx = 0;
    farm_area.mode.scroll.centre.vy = 0;
    farm_area.mode.scroll.centre.vz = 0;
}

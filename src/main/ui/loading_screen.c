#include <memory.h>
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "types.h"
#include "anim.h"
#include "loading_screen.h"
#include "main.h"
#include "system.h"
#include "texture_pack.h"

static AnimState loading_anim;
LoadingPackets loading_packets;
LoadingPicture loading_data;

s32 loading_screen_active = 0;
s32 loading_screen_started = 0;

/* Clear the packets and start the loading screen. */
void loading_screen_init(void) {
    memset(&loading_packets, 0, sizeof(loading_packets));
    loading_screen_start(0, 0, 0, 0, 0, 0);
}

/* Unused: passes its six arguments on to loading_screen_start. */
static void loading_screen_start_forward(short unused0, short unused1, short unused2, short unused3, short unused4, short unused5) {
    loading_screen_start(unused0, unused1, unused2, unused3, unused4, unused5);
}

/* Turn the display off, load the loading picture and bind its animation, then mark the screen active. */
void loading_screen_start(s32 unused0, s32 unused1, s32 unused2, s32 unused3, s32 unused4, s32 unused5) {
    LoadingPackets *packets;
    u8 *data;
    AnimState *anim;

    packets = &loading_packets;
    display_off();
    data = loading_data.files;
    texture_pack_load(data + loading_data.packStart);
    anim = &loading_anim;
    anim_bind_file(anim, (AnimFile *)data);
    ClearOTagR(packets->ots[0], LOADING_OT_LENGTH);
    ClearOTagR(packets->ots[1], LOADING_OT_LENGTH);
    anim_select_range(anim, 0);
    loading_screen_active = 1;
}

void loading_screen_stop(void) {
    loading_screen_active = 0;
}

s32 loading_screen_is_active(void) {
    return loading_screen_active;
}

/* Draw the loading picture into the next frame buffer, once per start. */
void loading_screen_draw(void) {
    FrameState *state;
    s32 index;
    LoadingPackets *packets;
    u_long *ot;
    SPRT *sprites;
    DR_MODE *modes;
    packets = &loading_packets;
    if (loading_screen_active != 0) {
        if (loading_screen_started != 1) {
            loading_screen_started = 1;
            DrawSync(0);
            display_on();
            DrawOTag(&packets->ots[frame_index][LOADING_OT_LENGTH - 1]);
            state = frame_records;
            if (current_frame == frame_records) state = &frame_records[1];
            current_frame = state;
            frame_index = state != frame_records;
            PutDispEnv(&state->dispEnv);
            PutDrawEnv(&current_frame->drawEnv);
            index = frame_index;
            ot = packets->ots[index];
            sprites = packets->sprites[index];
            modes = packets->modes[index];
            ClearOTagR(ot, LOADING_OT_LENGTH);
            anim_draw_sprt_slots_advance(ot, &sprites, &modes, &loading_anim, 160, 120);
            loading_screen_started = 0;
        }
    }
}

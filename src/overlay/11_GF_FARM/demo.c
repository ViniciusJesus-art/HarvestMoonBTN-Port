#include "types.h"
#include "demo.h"
#include "farm_state.h"
#include "pad.h"
#include "player.h"

/* Play the next frame of the attract-mode demo, starting it if the title asked for it.
 * Returns 1 on the frame it starts. Once the recording runs out, the farm moves to its
 * demo-end mode when it is idle. */
s32 demo_step(void) {
    FarmState *farm = &game_state;
    InputDemo *demo;
    FarmLoadArea *area;
    u32 flags;

    flags = farm->globalFlags;
    if (flags & GLOBAL_FLAG_INPUT_REPLAY) {
        demo = farm->demo;
        if (demo->frame < DEMO_LENGTH) {
            demo->buttons = demo->recording.inputs[demo->frame];
            farm->demo->frame++;
        } else {
            area = &farm_area;
            if (area->mode.eventMode == FARM_MODE_IDLE) {
                area->mode.eventMode = FARM_MODE_DEMO_END;
            }
        }
    } else if (flags & GLOBAL_FLAG_DEMO_START) {
        InputDemo *start = farm->demo;

        farm->globalFlags = (flags & ~GLOBAL_FLAG_DEMO_START) | GLOBAL_FLAG_INPUT_REPLAY;
        farm_buttons_held = 0;
        farm_buttons_pressed = 0;
        start->buttons = start->recording.inputs[start->frame];
        farm->demo->frame++;
        return 1;
    }
    return 0;
}

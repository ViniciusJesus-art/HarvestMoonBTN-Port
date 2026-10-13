/* The system: the host-debugger poll and the display on and off. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "types.h"
#include "cpu_asm.h"
#include "system.h"

/* Poll the host debugger. Always returns 1, so a loop on it never exits: that is how the callers wait for the host. */
s32 host_poll(void) {
    pollhost();
    return 1;
}

/* Switch the video output on and off. */
void display_on(void) {
    SetDispMask(1);
}

void display_off(void) {
    SetDispMask(0);
}

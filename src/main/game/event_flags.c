#include "types.h"
#include "event_flags.h"
#include "farm_state.h"

/* Read flag `flag`: 0 or 1. */
s32 event_flag_get(u16 flag) {
    u32 idx;
    u32 bit;
    u32 value;

    idx = flag / 32;
    bit = flag % 32;
    value = game_state.eventFlags[idx] >> bit;
    return value & 1;
}

/* Set flag `flag` when `on` is nonzero, clear it otherwise. */
void event_flag_set(u16 flag, u16 on) {
    u32 word = flag >> 5;
    u32 bit = flag & 0x1F;
    if (on) {
        game_state.eventFlags[word] |= (1 << bit);
    } else {
        game_state.eventFlags[word] &= ~(1 << bit);
    }
}

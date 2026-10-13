#include "types.h"
#include "farm_state.h"
#include "main.h"
#include "pad.h"
#include "pad_vibration.h"

/* A script step ends the script when its flags byte is this. */
#define VIBRATION_SCRIPT_END 0xFF

static void pad_vibration_step(s32 slot);
static void pad_vibration_set_script(signed int slot, u8 *script);
static void pad_vibration_clear(int slot);

u16 vibration_repeats[2];
s16 vibration_active[2];
u8 *vibration_cursors[2];
s16 vibration_priorities[4];

static void (*const pad_vibration_handlers[])(s32) = {
    0,
    pad_vibration_step,
};

/* Run each slot's handler for its state (0 idle, 1 step). */
void pad_vibration_tick(void) {
    u32 i;
    void (*const *table)(s32);
    i = 0;
    table = pad_vibration_handlers;
    do {
        void (*f)(s32);
        f = table[vibration_active[i]];
        if (f != 0) f(i);
        i++;
    } while (i < 2);
}

/* One vibration pattern: its priority and the script pad_vibration_step steps through.
 * A script is steps of three bytes: flags (bit 0 drives the big motor, bit 1 the small
 * one at the strength in the next byte), the strength, and how long the step holds. The
 * flags byte VIBRATION_SCRIPT_END ends the script. */
typedef struct Pattern {
    u8  priority;
    u8  pad1[3];
    u8 *script;
} Pattern;

static u8 vibration_script_0[12] = {
    0x02, 0xA0, 0x0A, 0x02, 0x80, 0x0A, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static u8 vibration_script_1[] = {
    0x02, 0xFF, 0x0A, 0xFF, 0x00, 0x00, 0x00, 0x00,
};

static u8 vibration_script_2[12] = {
    0x01, 0x40, 0x04, 0x02, 0x20, 0x04, 0x02, 0x40, 0x04, 0xFF, 0x00, 0x00,
};

static u8 vibration_script_3[20] = {
    0x01, 0x60, 0x04, 0x02, 0x70, 0x03, 0x02, 0x30, 0x02, 0x01, 0x20, 0x03,
    0x03, 0x00, 0x03, 0xFF, 0x00, 0x00, 0x00, 0x00,
};

static u8 vibration_script_4[12] = {
    0x01, 0x10, 0x08, 0x02, 0x04, 0x04, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static u8 vibration_script_5[12] = {
    0x01, 0x00, 0x01, 0x02, 0x01, 0x01, 0x01, 0x00, 0x01, 0xFF, 0x00, 0x00,
};

static u8 vibration_script_6[] = {
    0x01, 0x00, 0x05, 0xFF, 0x00, 0x00, 0x00, 0x00,
};

static u8 vibration_script_7[] = {
    0x02, 0xFF, 0x05, 0xFF, 0x00, 0x00, 0x00, 0x00,
};

static u8 vibration_script_8[] = {
    0x02, 0x40, 0x0A, 0xFF, 0x00, 0x00, 0x00, 0x00,
};

static u8 vibration_script_9[24] = {
    0x03, 0xFF, 0xFF, 0x03, 0xFF, 0xFF, 0x03, 0xFF, 0xFF, 0x03, 0xFF, 0xFF,
    0x03, 0xFF, 0xFF, 0x03, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static u8 vibration_script_10[12] = {
    0x03, 0xFF, 0xFF, 0x03, 0xFF, 0xFF, 0x03, 0xFF, 0xFF, 0xFF, 0x00, 0x00,
};

static Pattern vibration_patterns[11] = {
    {0, {0}, vibration_script_0}, {0, {0}, vibration_script_1}, {0, {0}, vibration_script_2},
    {0, {0}, vibration_script_3}, {0, {0}, vibration_script_4}, {0, {0}, vibration_script_5},
    {0, {0}, vibration_script_6}, {0, {0}, vibration_script_7}, {0, {0}, vibration_script_8},
    {0, {0}, vibration_script_9}, {0, {0}, vibration_script_10},
};

/* Start pattern `id` on `slot` unless GLOBAL_FLAG_INPUT_REPLAY is set or pad_is_started
 * says no; a busy slot is only taken over by a pattern of at least its
 * priority that is not already running there. */
void pad_vibration_start(s32 slot, s32 id) {
    s16 *active;

    if (game_state.globalFlags & GLOBAL_FLAG_INPUT_REPLAY) {
        return;
    }
    if (pad_is_started() == 0) {
        return;
    }
    active = &vibration_active[slot];
    if (*active != 0) {
        if (vibration_priorities[slot] > vibration_patterns[id].priority) {
            return;
        }
        if (vibration_cursors[slot] == vibration_patterns[id].script) {
            return;
        }
    }
    pad_vibration_set_script(slot, vibration_patterns[id].script);
    *active = 1;
    vibration_priorities[slot] = vibration_patterns[id].priority;
}

void pad_vibration_stop(s32 slot) {
    pad_vibration_clear(slot);
}

/* Advance one active entry and update its controller flags. */
static void pad_vibration_step(s32 slot) {
    int shift;
    s32 count;
    u8 duration;
    u8 *entry;
    u16 *counter;
    InputRecord *pad;
    shift = 1;
    pad = input_records;
    entry = vibration_cursors[slot];
    if (entry != 0) {
        duration = entry[2];
        counter = &vibration_repeats[slot];
        count = vibration_repeats[slot] + 1;
        *counter = count;
        if ((duration >> shift) < ((s16)count)) {
            entry += 3;
            vibration_repeats[slot] = 0;
            vibration_cursors[slot] += 3;
        }
        if (entry[0] == VIBRATION_SCRIPT_END) {
            pad_vibration_clear(slot);
            return;
        }
        pad->actuator0 = 0;
        pad->actuator1 = 0;
        if (entry[0] & 1) {
            pad->actuator0 = 1;
        }
        if (entry[0] & 2) {
            pad->actuator1 = entry[1];
        }
    }
}

static void pad_vibration_set_script(signed int slot, u8 *script) {
    vibration_cursors[slot] = script;
}

static void pad_vibration_clear(int slot) {
    input_records[0].actuator0 = 0;
    input_records[0].actuator1 = 0;
    vibration_cursors[slot] = 0;
    vibration_active[slot] = 0;
    vibration_priorities[slot] = 0;
    vibration_repeats[slot] = 0;
}

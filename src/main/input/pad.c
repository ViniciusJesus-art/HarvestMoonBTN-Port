/* Controller input: polls the two pad ports, turns the buttons into held, pressed
 * and auto-repeated states, and drives the vibration motors. */

#include <sys/types.h>
#include <libapi.h>
#include <libgte.h>
#include <libpad.h>
#include <abs.h>
#include <memory.h>
#include "types.h"
#include "farm_state.h"
#include "main.h"
#include "pad.h"

/* What pad_read_buttons returns when the port cannot be read. */
#define PAD_READ_ERROR (-0xFF0001)

static void pad_read_record(InputRecord *record, u8 slot);
static void pad_clear_record(InputRecord *record);
static s32 pad_read_buttons(s32 index);
static void pad_start_ports(void);
static s32 pad_align_actuators(s32 slot);
static u8 pad_dpad_with_stick(s32 port);
static s32 pad_read_analog(s32 index);
static s32 pad_stick_tilt(u8 x, u8 y);
static s16 pad_stick_heading(u8 x, u8 y);
static void pad_set_vibration(u8 motor, u8 force, s32 port);

/* Per port: the pad's mode, 0 while no pad is set up. */
s16 pad_port_mode[2];
/* 1 once the pad library is started. */
s32 pad_started;

/* Per-port pad buffers, and the actuator tables (one half per port). Both are
 * cleared by pad_reset_records. */
static PadBuffer pad_buffers[2];
static u8 pad_actuators[2][8];

/* Start both ports' pads. */
void pad_init(void) {
    pad_start_ports();
    ChangeClearPAD(0);
}

/* Clear both input records, both pad buffers and the actuator tables, then reset the repeat
 * timings. */
void pad_reset_records(void) {
    memset(input_pad1, 0, sizeof(*input_pad1));
    memset(input_pad2, 0, sizeof(*input_pad2));
    memset(pad_buffers, 0, sizeof(pad_buffers));
    memset(pad_actuators, 0, sizeof(pad_actuators));
    pad_set_repeat(PAD_REPEAT_DELAY, PAD_REPEAT_RATE);
    pad_set_repeat2(PAD_REPEAT_DELAY, PAD_REPEAT_RATE);
}

/* Set the first auto-repeat delay and rate, for both ports' records. */
void pad_set_repeat(s32 delay, s32 rate) {
    input_pad2->delay = delay;
    input_pad1->delay = delay;
    input_pad2->rate = rate;
    input_pad1->rate = rate;
}

/* Set the second auto-repeat delay and rate, for both ports' records. */
void pad_set_repeat2(u32 delay, u32 rate) {
    input_pad2->delay2 = delay;
    input_pad1->delay2 = delay;
    input_pad2->rate2 = rate;
    input_pad1->rate2 = rate;
}

/* Per port: 1 while the pad still needs its mode set up. */
s32 pad_needs_mode[2] = {1, 1};
/* The two port numbers. */
PadPorts pad_ports[1] = {{PAD_PORT_1, PAD_PORT_2}};

void pad_set_started(signed int started) {
    pad_started = started;
}

signed int pad_is_started(void) {
    return pad_started;
}

/* Poll both controller ports and refresh each port's input record from the pad's state.
 * With input replay on, a port with no pad reads the recording. A ready pad (stable) is
 * set up once; when its actuators align, its record is cleared and it is polled again. */
void pad_poll_ports(void) {
    PadPorts ports;
    s32 slot;
    s32 again;

    ports = pad_ports[0];
    for (slot = 0; slot < 2; slot++) {
        again = 1;
        do {
            switch (PadGetState(ports.port[slot])) {
            case PadStateDiscon:
            case PadStateFindPad:
                if (game_state.globalFlags & GLOBAL_FLAG_INPUT_REPLAY) {
                    pad_read_record(&input_records[slot], slot);
                    again = 0;
                    break;
                }
                pad_port_mode[slot] = 0;
                pad_needs_mode[slot] = 1;
                pad_clear_record(&input_records[slot]);
                again = 0;
                break;
            case PadStateReqInfo:
            case PadStateExecCmd:
                pad_clear_record(&input_records[slot]);
                again = 0;
                break;
            case PadStateStable:
                if (pad_needs_mode[slot] == 1) {
                    pad_port_mode[slot] = PadInfoMode(ports.port[slot], InfoModeCurExID, 0);
                    if (pad_align_actuators(slot) != 0) {
                        pad_needs_mode[slot] = 0;
                        pad_clear_record(&input_records[slot]);
                        break;
                    }
                }
                /* fallthrough */
            default:
                pad_read_record(&input_records[slot], slot);
                again = 0;
                break;
            }
        } while (again == 1);
    }
}

/* Refresh an input record from its pad: the vibration request, held and pressed buttons,
 * the analog value and both auto-repeat triggers. During input replay, pad 1 takes the
 * recorded buttons (circle and cross swapped), and start sets the demo to its input frame. */
static void pad_read_record(struct InputRecord *record, u8 slot) {
    u16 speed;
    s32 divisor;
    u32 delay;
    s32 rate;
    u32 bits;
    u32 replay;

    pad_set_vibration(record->actuator0, record->actuator1, slot);
    record->held = (u16)~pad_read_buttons(slot);
    record->analog = pad_read_analog(slot);
    if ((game_state.globalFlags & GLOBAL_FLAG_INPUT_REPLAY) && record == input_pad1) {
        if (record->held & PAD_START) {
            game_state.demo->frame = DEMO_INPUTS;
        } else {
            /* Take the replayed buttons, with circle and cross swapped. */
            replay = game_state.demo->buttons;
            bits = (replay >> 1) & PAD_CIRCLE;
            if (replay & PAD_CIRCLE) {
                bits |= PAD_CROSS;
            }
            replay &= ~(PAD_CIRCLE | PAD_CROSS);
            replay |= bits;
            record->held = replay;
            record->analog = 0;
        }
    }
    record->pressed = (record->previous ^ record->held) & record->held;
    if (record->held == record->previous) {
        speed = main_get_speed_mode();
        divisor = 1;
        if (speed == MAIN_SPEED_FAST_2 || speed == MAIN_SPEED_FAST_3) {
            divisor = speed;
        }
        delay = record->delay / divisor;
        rate = record->rate / divisor;
        if (rate <= 0) {
            rate = 1;
        }
        if (record->repeating == 0) {
            record->triggered = 0;
            record->repeatFrames++;
            if (delay < record->repeatFrames) {
                record->repeatFrames = 0;
                record->repeating = 1;
            }
        } else {
            record->repeatFrames++;
            if (record->repeatFrames % rate == 0) {
                record->triggered = record->held;
            } else {
                record->triggered = 0;
            }
        }
        delay = record->delay2 / divisor;
        rate = record->rate2 / divisor;
        if (rate <= 0) {
            rate = 1;
        }
        if (record->repeating2 == 0) {
            record->triggered2 = 0;
            record->repeatFrames2++;
            if (delay < record->repeatFrames2) {
                record->repeatFrames2 = 0;
                record->repeating2 = 1;
            }
        } else {
            record->repeatFrames2++;
            if (record->repeatFrames2 % rate == 0) {
                record->triggered2 = record->held;
            } else {
                record->triggered2 = 0;
            }
        }
    } else {
        record->triggered = record->pressed;
        record->triggered2 = record->pressed;
        record->repeatFrames = 0;
        record->repeating = 0;
        record->repeatFrames2 = 0;
        record->repeating2 = 0;
    }
    record->previous = record->held;
}

/* Reset a controller record: no buttons, no repeat, the analog reading -1. */
static void pad_clear_record(InputRecord *record) {
    record->held = 0;
    record->pressed = 0;
    record->analog = -1;
    record->previous = 0;
    record->repeatFrames = 0;
    record->repeating = 0;
    record->repeatFrames2 = 0;
    record->repeating2 = 0;
    record->triggered = 0;
    record->triggered2 = 0;
}

/* Read the port's buttons as 16 bits, the d-pad in the high byte. Returns PAD_READ_ERROR
 * when the pad reports a status error or is neither digital nor analog. */
static s32 pad_read_buttons(s32 index) {
    u8 type;

    if (pad_buffers[index].status != 0) {
        return PAD_READ_ERROR;
    }
    type = pad_buffers[index].type >> 4;
    if (type != PAD_TYPE_DIGITAL && type != PAD_TYPE_ANALOG) {
        return PAD_READ_ERROR;
    }
    return (pad_dpad_with_stick(index) << 8) | pad_buffers[index].buttonsHigh;
}

/* Hand the port buffers and actuator tables to the pad library, and start polling. */
static void pad_start_ports(void) {
    PadInitDirect((u8 *)&pad_buffers[0], (u8 *)&pad_buffers[1]);
    PadSetAct(PAD_PORT_1, pad_actuators[0], 8);
    PadSetAct(PAD_PORT_2, pad_actuators[1], 8);
    PadStartCom();
    pad_started = 1;
}

/* Actuator alignment for PadSetActAlign. */
static u8 pad_actuator_align[] = {
    0xFF, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0x00, 0x00,
};

/* Align the actuators of the pad in `slot` once it is ready (state 6). */
static s32 pad_align_actuators(s32 slot) {
    PadPorts ports;
    s32 result = 0;

    ports = pad_ports[0];
    if (PadGetState(ports.port[slot]) == PadStateStable) {
        result = PadSetActAlign(ports.port[slot], pad_actuator_align);
    }
    return result;
}

/* The d-pad's four bits in the buttons byte: all set while no direction is pressed. */
#define PAD_DPAD_BITS 0xF0

/* The stick's heading, in degrees, where one d-pad sector meets the next. */
#define PAD_SECTOR_EDGE_0 45
#define PAD_SECTOR_EDGE_1 135
#define PAD_SECTOR_EDGE_2 225
#define PAD_SECTOR_EDGE_3 315

/* The d-pad byte of `port`, with an analog pad's left stick folded in: while no d-pad button
 * is pressed, a stick held in one of four 90-degree sectors presses that direction. */
static u8 pad_dpad_with_stick(s32 port) {
    u8 buttons = pad_buffers[port].buttons;
    s16 angle;

    switch (pad_buffers[port].type >> 4) {
    case PAD_TYPE_DIGITAL:
        return buttons;
    case PAD_TYPE_ANALOG:
        if ((buttons & PAD_DPAD_BITS) != PAD_DPAD_BITS) {
            return buttons;
        }
        angle = pad_stick_heading(pad_buffers[port].leftX, pad_buffers[port].leftY);
        if (angle == -1) {
            buttons = pad_buffers[port].buttons;
        } else if (angle < PAD_SECTOR_EDGE_0 || angle > PAD_SECTOR_EDGE_3) {
            buttons = pad_buffers[port].buttons & ~(PAD_UP >> 8);
        } else if (angle > PAD_SECTOR_EDGE_0 && angle < PAD_SECTOR_EDGE_1) {
            buttons = pad_buffers[port].buttons & ~(PAD_RIGHT >> 8);
        } else if (angle > PAD_SECTOR_EDGE_1 && angle < PAD_SECTOR_EDGE_2) {
            buttons = pad_buffers[port].buttons & ~(PAD_DOWN >> 8);
        } else if (angle > PAD_SECTOR_EDGE_2) {
            buttons = pad_buffers[port].buttons & ~(PAD_LEFT >> 8);
        }
        return buttons;
    default:
        return buttons;
    }
}

/* The stick's tilt for an analog pad whose d-pad is idle, or -1 when there is no reading. */
static s32 pad_read_analog(s32 index) {
    u8 t = pad_buffers[index].type >> 4;
    switch (t) {
    case PAD_TYPE_DIGITAL:
        return -1;
    case PAD_TYPE_ANALOG:
        if ((pad_buffers[index].buttons & PAD_DPAD_BITS) == PAD_DPAD_BITS)
            return pad_stick_tilt(pad_buffers[index].leftX, pad_buffers[index].leftY);
        return -1;
    default:
        return -1;
    }
}

/* Stick position (0..255 per axis, 128 = centre) to a tilt level 0..90:
 * 0 inside the 92..164 dead zone, otherwise the distance from the centre
 * (csqrt in 20.12 fixed point) less 37, clamped. */
static s32 pad_stick_tilt(u8 x, u8 y) {
    s32 dx;
    s32 dy;
    s32 distance;
    s32 dx_squared;

    dx = x;
    dy = y;
    if (dx >= PAD_STICK_DEAD_LO && dx <= PAD_STICK_DEAD_HI && dy <= PAD_STICK_DEAD_HI && dy >= PAD_STICK_DEAD_LO) {
        return 0;
    }
    dx -= 128;
    dy -= 128;
    dx_squared = dx * dx;
    dy = dy * dy + dx_squared;
    distance = csqrt(dy << 12);
    distance = (u8)(distance / 4096);
    distance -= PAD_TILT_OFFSET;
    if (distance < 0) {
        distance = 0;
    }
    if (distance > PAD_TILT_MAX) {
        distance = PAD_TILT_MAX;
    }
    return (u8)distance;
}

/* Stick position (0..255 per axis, 128 = centre) to a 0..359 degree heading, or -1
 * when centred. Positions inside the 92..164 dead zone count as centred. */
static s16 pad_stick_heading(u8 x, u8 y) {
    s32 dx;
    s32 dy;
    s32 angle;

    if (x >= PAD_STICK_DEAD_LO && x <= PAD_STICK_DEAD_HI && y <= PAD_STICK_DEAD_HI && y >= PAD_STICK_DEAD_LO) {
        x = 128;
        y = 128;
    }
    if (x == 128 && y == x) {
        return -1;
    }
    dx = abs(x - 128);
    dy = abs(y - 128);
    if (dy == 0) {
        angle = 90;
    } else if (dx == 0) {
        angle = 0;
    } else {
        angle = ratan2(dx, dy) * 360 / 4096;
    }
    if ((s8)x < 0) {
        if (y > 128) {
            angle = 180 - angle;
        }
    } else {
        if (y > 128) {
            angle += 180;
        } else {
            angle = 360 - angle;
        }
    }
    if (angle == 360) {
        angle = 0;
    }
    return angle;
}

/* Set the vibration request for one pad port. A port whose pad has no mode yet gets the
 * fixed header, with the motor on when there is force; the tables are cleared while no pad
 * is started. */
static void pad_set_vibration(u8 motor, u8 force, s32 port) {
    if (pad_port_mode[port] == 0) {
        pad_actuators[port][0] = 0x40;
        pad_actuators[port][1] = motor != 0;
        if (force) {
            if (motor) motor = 0;
            pad_actuators[port][1] = 1;
        }
        pad_actuators[port][2] = 0;
    } else {
        pad_actuators[port][1] = motor != 0;
        pad_actuators[port][2] = force;
    }
    if (pad_started == 0) {
        memset(pad_actuators, 0, sizeof(pad_actuators));
    }
}

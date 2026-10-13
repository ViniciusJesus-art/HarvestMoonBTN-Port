#ifndef PAD_H
#define PAD_H

/* Both controller ports: the input records, auto-repeat and the sticks. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

/* One controller input record. The repeat pair: `triggered` reports a held
 * button once on press, then every `rate` frames after `delay` frames; the
 * second set does the same with its own timings. */
typedef struct InputRecord {
    u32 triggered;
    u32 held;
    u32 pressed;            /* newly pressed this frame */
    u32 previous;           /* last frame's `held` */
    u32 repeatFrames;       /* frames counted toward the next auto-repeat */
    s32 repeating;
    s32 delay;
    s32 rate;
    s32 repeating2;
    u32 triggered2;
    u32 repeatFrames2;
    s32 delay2;
    s32 rate2;
    s16 analog;
    u8 actuator0;
    u8 actuator1;
} InputRecord;

/* The button bits of InputRecord.held / pressed / triggered. */
enum PadButton {
    PAD_L2       = 0x0001,
    PAD_R2       = 0x0002,
    PAD_L1       = 0x0004,
    PAD_R1       = 0x0008,
    PAD_TRIANGLE = 0x0010,
    PAD_CIRCLE   = 0x0020,
    PAD_CROSS    = 0x0040,
    PAD_SQUARE   = 0x0080,
    PAD_SELECT   = 0x0100,
    PAD_START    = 0x0800,
    PAD_UP       = 0x1000,
    PAD_RIGHT    = 0x2000,
    PAD_DOWN     = 0x4000,
    PAD_LEFT     = 0x8000,

    PAD_ARROWS_MASK = PAD_UP | PAD_RIGHT | PAD_DOWN | PAD_LEFT,
};

/* The high nibble of PadBuffer.type: a digital pad, or an analog pad with its two sticks. */
enum PadType {
    PAD_TYPE_DIGITAL = 4,
    PAD_TYPE_ANALOG = 7,
};

/* The PSY-Q pad API's receive buffer for one port. */
typedef struct PadBuffer {
    u8 status;
    u8 type;
    u8 buttons;
    u8 buttonsHigh;
    u8 rightX;
    u8 rightY;
    u8 leftX;
    u8 leftY;
    u8 pad8[0x1A];
} PadBuffer;

typedef struct PadPorts {
    u8 port[2];
} PadPorts;

void pad_init(void);
void pad_reset_records(void);
void pad_set_repeat(s32 delay, s32 rate);
void pad_set_repeat2(u32 delay, u32 rate);
void pad_set_started(s32 started);
s32 pad_is_started(void);
void pad_poll_ports(void);

/* The title's attract-mode demo: a recording of pad 1, one input per frame. The title
 * copies a recording in and sets GLOBAL_FLAG_DEMO_START; the farm then plays it back
 * while GLOBAL_FLAG_INPUT_REPLAY is set. Pressing Start skips to the end. */
#define DEMO_INPUTS 2048           /* room in a recording */
#define DEMO_LENGTH 1848            /* frames the demo plays */

/* A recording as the title stores and copies it. */
typedef struct DemoRecording {
    u8 bytes[DEMO_INPUTS * 2];
} DemoRecording;

typedef struct InputDemo {
    u32 frame;                  /* the next input to play */
    u16 buttons;                /* the input being played */
    union {
        DemoRecording copy;
        u16 inputs[DEMO_INPUTS];
    } recording;
} InputDemo;

/* The auto-repeat timing after a press: the first repeat delay and the repeat rate, in frames. */
#define PAD_REPEAT_DELAY 20
#define PAD_REPEAT_RATE 7

/* The two controller ports, as PadSetAct and the port table name them. */
#define PAD_PORT_1 0x00
#define PAD_PORT_2 0x10

/* The stick's dead zone: raw values from PAD_STICK_DEAD_LO to PAD_STICK_DEAD_HI read as centre. */
#define PAD_STICK_DEAD_LO 92
#define PAD_STICK_DEAD_HI 164

/* The stick's tilt level: its distance from centre less PAD_TILT_OFFSET, up to PAD_TILT_MAX. */
#define PAD_TILT_OFFSET 37
#define PAD_TILT_MAX 90

extern PadPorts pad_ports[];

#endif /* PAD_H */

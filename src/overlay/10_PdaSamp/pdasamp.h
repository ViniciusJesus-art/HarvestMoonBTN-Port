#ifndef PDASAMP_H
#define PDASAMP_H

/* Overlay-independent types for the PDA sample screen. */

#include <sys/types.h>
#include <libapi.h>
#include "types.h"

/* The memory card's date and time, as McxGetTime writes it. */
typedef struct PdaCardInfo {
    u8 b0;
    u8 b1;
    u8 b2;
    u8 b3;
    u8 b4;                  /* the weekday, an index into pda_time_weekday_names */
    u8 b5;
    u8 b6;
    u8 b7;
} PdaCardInfo;

/* The device block the memory-card calls fill and send: the device number,
 * the 32-byte fixed part and the 128-byte variable part. The editors move a
 * cursor from -1 to 31 over fix[], where -1 is the device byte, so they reach
 * the cursor byte through `bytes`. */
typedef union {
    u8 bytes[176];
    struct {
        u8 device;
        u8 fix[32];
        u8 read[128];
    } field;
} PdaDeviceBlock;

s32 pda_sample_run(void);

#endif /* PDASAMP_H */

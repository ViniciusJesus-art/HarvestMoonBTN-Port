#ifndef PAD_VIBRATION_H
#define PAD_VIBRATION_H


/* Vibration patterns per controller slot. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

void pad_vibration_tick(void);
void pad_vibration_start(s32 slot, s32 id);
void pad_vibration_stop(s32 slot);

#endif /* PAD_VIBRATION_H */

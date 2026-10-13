#ifndef OPTIONS_MENU_H
#define OPTIONS_MENU_H

/* The options menu and its settings. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

/* The options menu's six settings. */
typedef struct FarmSettings {
    s16 messageType;
    s16 vibration;      /* 1 when the pad vibrates */
    s16 stereo;         /* 1 for stereo, 0 for mono */
    s16 statusLatch;    /* 1 sets the one-shot status latch */
    s16 unused8;
    s16 unusedA;
} FarmSettings;
void options_menu_open(void);
void options_menu_close(void);
void options_apply_settings(void);


#endif /* OPTIONS_MENU_H */

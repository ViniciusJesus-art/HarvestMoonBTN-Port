#ifndef LOADING_SCREEN_H
#define LOADING_SCREEN_H


/* The loading screen, drawn while the game loads. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

/* A loading picture: where its sprite file and the sprite's texture pack start, counted
 * from the end of these two words, then the files themselves. */
typedef struct LoadingPicture {
    s32 spriteStart;
    s32 packStart;
    u8 files[0x1800 - 8];
} LoadingPicture;

/* The entries in each of the loading screen's ordering tables. */
#define LOADING_OT_LENGTH 32

/* The loading screen's own packets. */
typedef struct LoadingPackets {
    u_long ots[2][LOADING_OT_LENGTH];
    SPRT sprites[2][32];
    DR_MODE modes[2][32];
    u8 pad900[0x1300 - 0x900];
} LoadingPackets;

void loading_screen_init(void);
void loading_screen_start(s32 unused0, s32 unused1, s32 unused2, s32 unused3, s32 unused4, s32 unused5);
void loading_screen_stop(void);
s32 loading_screen_is_active(void);
void loading_screen_draw(void);

extern LoadingPackets loading_packets;
extern LoadingPicture loading_data;

#endif /* LOADING_SCREEN_H */

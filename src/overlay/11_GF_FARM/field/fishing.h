#ifndef FISHING_H
#define FISHING_H

/* Fishing: the bite, the catch and the streaks. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "picture_cells.h"
#include "player.h"

/* Three halfwords of the fishing marker's origin. */
typedef struct FishingMarkerPoint {
    s16 x;
    u16 y;
    u16 z;
} FishingMarkerPoint;


/* State of the current cast. */
typedef struct FarmFishing {
    s32 pull;          /* presses left to land the catch */
    s32 wait;          /* frames until the next bite chance */
    s32 kind;          /* fishing spot kind; -1: no catch */
} FarmFishing;

s32 fishing_player_frame(FarmPlayer *player);
s32 fishing_bite_wait(FarmFishing *fishing);
s16 fishing_roll_catch(FarmFishing *fishing);
void fishing_draw_marker(s32 animate);
void farm_overview_start(void);
void farm_overview_drift(void);
void farm_overview_camera(void);
s32 cell_texture_fill(FarmCellTexture *context, s32 id);

void farm_retexture_all_cells(void);
s32 farm_cell_retexture(s16 col, s16 row, FarmCellTexture *texture);

extern FishingMarkerPoint fishing_marker_origin;
extern s32 fishing_marker_active;
/* The fishing marker's drawing value and frame counter. */
extern s32 fishing_marker_value;
extern s32 fishing_marker_frame;
/* Ordinary catches in a row (the fishing streak). */
extern s32 fishing_streak;

#endif /* FISHING_H */

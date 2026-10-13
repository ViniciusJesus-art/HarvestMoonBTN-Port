#ifndef GREENHOUSE_H
#define GREENHOUSE_H

/* The greenhouse: its crop grid and its drawing. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

/* The greenhouse grid holds 25 items, five to a row; a row is one greenhouse kind. */
#define GREENHOUSE_KINDS 5
#define GREENHOUSE_TOOLS 25

/* Texture rectangle of a greenhouse quad: four corner pairs and a palette. */
typedef struct GreenhouseQuadUv {
    u8 corner[4][2];
    u16 clut;
} GreenhouseQuadUv;

s32 greenhouse_item_column(s16 item);
s32 greenhouse_level_value(s16 index, s16 variant);
s32 greenhouse_kind_index(s16 kind);
s32 greenhouse_probe_sweep(s16 restart);
s32 greenhouse_world_to_cell(VECTOR *position, s16 *column, s16 *row);
s32 greenhouse_retexture_quad(s16 column, s16 row, s16 index);
void greenhouse_update_cells(void);

#endif /* GREENHOUSE_H */

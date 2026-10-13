#ifndef FIELD_H
#define FIELD_H

/* The farm fields: the daily crop tick and clearing tiles. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "items.h"

/* An 8-byte farm tile. */
typedef struct FarmFieldTile {
    ItemId value;       /* the cell's id; PLACE_EVENT_CELL marks an event cell */
    u16 ground;         /* the soil under the cell (ITEM_SOIL..ITEM_WATERED_SOWN_SOIL); the cell returns to it */
    u8  kind;           /* the event level */
    u8  unused5;
    u16 growthFlags;    /* bit 15 skips one day's growth; the low nibble is the level's sub-step */
} FarmFieldTile;

/* The special field tiles: the stones, rocks and stumps a field clears, and the lumber
 * they leave, each in the three sizes the field's tile table lists. */
enum FieldTileSpecial {
    FIELD_TILE_BIG_STONE_1 = 0x16C,
    FIELD_TILE_BIG_STONE_2 = 0x16D,
    FIELD_TILE_BIG_STONE_3 = 0x16E,
    FIELD_TILE_ROCK_1 = 0x16F,
    FIELD_TILE_ROCK_2 = 0x170,
    FIELD_TILE_ROCK_3 = 0x171,
    FIELD_TILE_STUMP_1 = 0x172,
    FIELD_TILE_STUMP_2 = 0x173,
    FIELD_TILE_STUMP_3 = 0x174,
    FIELD_TILE_LUMBER = 0x175,
};

/* The mask of the growthFlags sub-step, and the bit that skips one day's growth. */
enum { FIELD_TILE_STEP_MASK = 0xF, FIELD_TILE_SKIP_DAY = 0x8000 };

/* The cell looks of the soil: the first five entries of the field's look table. The
 * crop looks start at FIELD_LOOK_CROP, 8 per growth stage and shape. */
enum FieldCellLook {
    FIELD_LOOK_BARE = 0,
    FIELD_LOOK_TILLED = 1,
    FIELD_LOOK_WATERED = 2,
    FIELD_LOOK_SOWN = 3,
    FIELD_LOOK_WATERED_SOWN = 4,
    FIELD_LOOK_CROP = 5,
};

/* The farm's grids: the field (FIELD_ROWS x FIELD_COLS tiles) and the greenhouse
 * (GREENHOUSE_SIZE squared). */
#define FIELD_ROWS      25
#define FIELD_COLS      45
#define GREENHOUSE_SIZE 7

/* The world position of the centre of tile (0, 0): the farm's and the greenhouse's. */
#define FIELD_ORIGIN_X      (-4200)
#define FIELD_ORIGIN_Z      1400
#define GREENHOUSE_ORIGIN_X (-1420)
#define GREENHOUSE_ORIGIN_Z 1020

/* A tile value of a crop that has grown. Not an item id. */
#define FIELD_TILE_GROWN 0x36

/* FarmCrop.season of a crop that grows in any season (grass only). */
#define CROP_SEASON_ANY 4

typedef struct FarmSlotRow { FarmFieldTile rows[GREENHOUSE_SIZE]; } FarmSlotRow;

typedef struct FarmTileRow { FarmFieldTile rows[FIELD_COLS]; } FarmTileRow;

s32 field_probe_sweep(s16 restart);
void greenhouse_clear_cells(void);
void field_daily_tick(void);
s32 field_world_to_cell(VECTOR *position, s16 *column, s16 *row);
s32 field_coords_check(s16 tileX, s16 tileY, s16 checkTile, s16 tableIndex);
void field_result_coords(s16 x, s16 y, VECTOR *pos, s16 which);
s32 field_event_dispatch(s16 tileX, s16 tileY, s16 layer, s16 itemId);
s32 field_gate_open(s16 layer, s16 tileX, s16 tileY, s32 unused);
s32 field_request_dispatch(s16 mode, s16 x, s16 y);
s32 field_tile_item_stage(FarmFieldTile *tile);
s32 field_tile_harvest(s16, s16, s16);
s32 field_tile_clear_tool(s16 which, s16 x, s16 y, s16 reach, s16 power);
s32 field_tile_clear_tool_alt(s16 which, s16 x, s16 y, s16 reach, s16 power);
void field_table_refresh(s32 mode);
void field_storm_damage(void);
s32 field_find_ready_grown(VECTOR *position);
void field_draw_grid(void);
void greenhouse_draw(void);
void item_art_bind_sprites(s32 id);

s32 field_cell_retexture(s16 column, s16 row, s16 look);
void picture_cells_tick(void);
FarmFieldTile *field_tile_at(s32 which, s16 x, s16 y);
void field_tile_dry(FarmFieldTile *tile);
s32 field_find_free_tile(s16 *column, s16 *row, s32 mode, FarmFieldTile *request, s32 flags);

/* Table of checks for event requests. */
extern s32 (*const event_request_checks[])(void);

#endif /* FIELD_H */

/* The farm field: the 25 x 45 outdoor grid and the 7 x 7 greenhouse, their crop
 * growth, their cell textures, their drawing and the tools that clear them. */

#include "types.h"
#include "anim.h"
#include "animals.h"
#include "clock.h"
#include "estate.h"
#include "event_script.h"
#include "farm_data.h"
#include "farm_probe.h"
#include "farm_state.h"
#include "farm_tables.h"
#include "field.h"
#include "field_item_requests.h"
#include "greenhouse.h"
#include "ground.h"
#include "items.h"
#include "main.h"
#include "model.h"
#include "picture_cells.h"
#include "scene.h"
#include "screen_fade.h"
#include "weather.h"

/* The growth half of a field tile: stage, watering days and the skip-a-day bit. */
typedef struct FarmGrowth {
    u8 stage;
    u8 days;  /* days watered at this stage */
    u16 flags; /* bit 15 skips one day */
} FarmGrowth;

typedef s32 (*FarmEventHandler)(s16, s16, s16);

typedef s32 (*FarmCellLook)(s32 code, s32 kind, s32 *attr, s32 *sprite, s32 *lift);

/* The field sizes, per field (0: farm 25 x 45, 1: greenhouse 7 x 7). */
typedef struct FarmFieldSize {
    s16 rows;
    s16 cols;
} FarmFieldSize;

/* A field cell is 400 units on each side. */
#define FIELD_CELL_SIZE 400
/* The cell look of the snow-covered ground; every cell takes it in winter. */
#define CELL_LOOK_WINTER 29

static s32 field_cell_look_spring(s32 code, s32 kind, s32 *attr, s32 *sprite, s32 *lift);
static s32 field_cell_look_summer(s32 code, s32 kind, s32 *attr, s32 *sprite, s32 *lift);
static s32 field_cell_look_fall(s32 code, s32 kind, s32 *attr, s32 *sprite, s32 *lift);
static s32 field_cell_look_winter(s32 code, s32 unused, s32 *attr, s32 *sprite, s32 *lift);
static s32 field_cell_look_anim(s32 kind, s32 attr, s32 sprite, s32 *outAttr, s32 *outSprite);
static void field_tile_reset(FarmFieldTile *tile);
static void field_tile_season_result(FarmFieldTile *tile);
static void field_tile_roll_to_lumber(u16 *p, u32 range);
static void field_tile_wither(FarmFieldTile *tile, u32 range);
static void field_tile_regrow_roll(FarmFieldTile *tile, s32 range);
static void field_reshape_grown(s16 y, s16 x, s16 which, s16 merge, s16 quiet);
static void field_request_submit_scaled(ItemId id, s16 mode, s16 start, s16 end);
static void field_grown_cell_request(s16 y, s16 x, s32 which);

/* One cell look: four (u, v) corners and a CLUT. */
typedef struct FarmCellUV {
    u8 uv[8];
    u16 cba;
} FarmCellUV;

/* Copy the look's four (u, v) corners into the quad, in order when
 * `in_order` is set, else mirrored (corner 3 first). */
static inline void set_quad_uv(FarmTmdQuad *quad, FarmCellUV *cell, s32 in_order) {
    if (in_order) {
        quad->u0 = cell->uv[0];
        quad->v0 = cell->uv[1];
        quad->u1 = cell->uv[2];
        quad->v1 = cell->uv[3];
        quad->u2 = cell->uv[4];
        quad->v2 = cell->uv[5];
        quad->u3 = cell->uv[6];
        quad->v3 = cell->uv[7];
    } else {
        quad->u0 = cell->uv[6];
        quad->v0 = cell->uv[7];
        quad->u1 = cell->uv[4];
        quad->v1 = cell->uv[5];
        quad->u2 = cell->uv[2];
        quad->v2 = cell->uv[3];
        quad->u3 = cell->uv[0];
        quad->v3 = cell->uv[1];
    }
}

static FarmCellUV cell_looks[30] = {
    {{96, 207, 96, 176, 127, 207, 127, 176}, 0x3E55},
    {{128, 207, 128, 176, 159, 207, 159, 176}, 0x3E95},
    {{160, 207, 160, 176, 191, 207, 191, 176}, 0x3E5F},
    {{192, 207, 192, 176, 223, 207, 223, 176}, 0x3E5B},
    {{224, 207, 224, 176, 255, 207, 255, 176}, 0x3E9B},
    {{0, 191, 0, 160, 31, 191, 31, 160}, 0x6774},
    {{32, 191, 63, 191, 32, 160, 63, 160}, 0x6774},
    {{64, 191, 95, 191, 64, 160, 95, 160}, 0x6774},
    {{96, 191, 96, 160, 127, 191, 127, 160}, 0x6774},
    {{159, 191, 128, 191, 159, 160, 128, 160}, 0x6774},
    {{160, 191, 191, 191, 160, 160, 191, 160}, 0x6774},
    {{192, 191, 223, 160, 192, 160, 223, 191}, 0x6774},
    {{224, 191, 224, 160, 255, 191, 255, 160}, 0x6774},
    {{0, 223, 0, 192, 31, 223, 31, 192}, 0x67B4},
    {{32, 223, 63, 223, 32, 192, 63, 192}, 0x67B4},
    {{64, 223, 95, 223, 64, 192, 95, 192}, 0x67B4},
    {{96, 223, 96, 192, 127, 223, 127, 192}, 0x67B4},
    {{159, 223, 128, 223, 159, 192, 128, 192}, 0x67B4},
    {{160, 223, 191, 223, 160, 192, 191, 192}, 0x67B4},
    {{192, 223, 223, 192, 192, 192, 223, 223}, 0x67B4},
    {{224, 223, 224, 192, 255, 223, 255, 192}, 0x67B4},
    {{0, 255, 0, 224, 31, 255, 31, 224}, 0x67F4},
    {{32, 255, 63, 255, 32, 224, 63, 224}, 0x67F4},
    {{64, 255, 95, 255, 64, 224, 95, 224}, 0x67F4},
    {{96, 255, 96, 224, 127, 255, 127, 224}, 0x67F4},
    {{159, 255, 128, 255, 159, 224, 128, 224}, 0x67F4},
    {{160, 255, 191, 255, 160, 224, 191, 224}, 0x67F4},
    {{192, 255, 223, 224, 192, 224, 223, 255}, 0x67F4},
    {{224, 255, 224, 224, 255, 255, 255, 224}, 0x67F4},
    {{160, 215, 160, 184, 191, 215, 191, 184}, 0x3E58},
};

static u8 column_mirror_table[48] = {
    0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x01, 0x00, 0x00, 0x00,
};

/* Give the field quad under (column, row) the texture of cell look `look`
 * (CELL_LOOK_WINTER in winter): copy its corners, mirrored unless
 * column_mirror_table[column] is set, and pick the texture page. Returns 0 once
 * retextured (or when the place is not the farm), 1 when the quad is not a cell quad. */
s32 field_cell_retexture(s16 column, s16 row, s16 look) {
    FarmTmdQuad *quad;
    FarmCellUV *cell;
    u32 header;

    if (farm_area.mode.place.location != 1) {
        return 0;
    }
    quad = farm_area.mode.quads[row][column];
    if (game_state.progress.clock.date.season == SEASON_WINTER) {
        look = CELL_LOOK_WINTER;
    }
    header = quad->header;
    if (header & 8) {
        cell = &cell_looks[look];
        if ((header & TMD_PRIM_CODE_MASK) == (TMD_PRIM_GT4_RAW << 16)) {
            set_quad_uv(quad, cell, column_mirror_table[column]);
            quad->cba = cell->cba;
            if (look == CELL_LOOK_WINTER) {
                quad->tsb = 7;
                return 0;
            } else if (look >= FIELD_LOOK_CROP && look != CELL_LOOK_WINTER) {
                quad->tsb = 29;
                if (game_state.progress.clock.date.season == SEASON_FALL) {
                    quad->cba = (cell->cba & 0xFFC0) | 0x35;
                }
            } else {
                quad->tsb = 8;
            }
            return 0;
        } else if ((header & TMD_PRIM_CODE_MASK) == (TMD_PRIM_FT4_RAW << 16)) {
            set_quad_uv(quad, cell, column_mirror_table[column]);
            quad->cba = cell->cba;
            if (look != CELL_LOOK_WINTER) {
                if (look >= FIELD_LOOK_CROP) {
                    quad->tsb = 29;
                    if (game_state.progress.clock.date.season == SEASON_FALL) {
                        quad->cba = (cell->cba & 0xFFC0) | 0x35;
                    }
                } else {
                    quad->tsb = 8;
                }
            } else {
                quad->tsb = 7;
                return 0;
            }
            return 0;
        }
    }
    return 1;
}

/* Retexture every field cell from its soil or crop state. */
void picture_cells_tick(void) {
    FarmFieldTile *entry;
    u32 row;
    u32 column;
    u32 value;
    s16 category;
    s16 encoded;
    u16 type;

    entry = game_state.fieldTiles[0].rows;
    row = 0;
    do {
        column = 0;
        do {
            value = entry->ground;
            category = 0;
            if (value - ITEM_SOIL < 5) {
                switch (value) {
                case ITEM_SOIL:
                    category = FIELD_LOOK_BARE;
                    break;
                case ITEM_TILLED_SOIL:
                    category = FIELD_LOOK_TILLED;
                    break;
                case ITEM_WATERED_SOIL:
                    category = FIELD_LOOK_WATERED;
                    break;
                case ITEM_SOWN_SOIL:
                    category = FIELD_LOOK_SOWN;
                    break;
                case ITEM_WATERED_SOWN_SOIL:
                    category = FIELD_LOOK_WATERED_SOWN;
                    break;
                }
            }
            type = entry->value;
            if (type == FIELD_TILE_GROWN && entry->kind != 0) {
                encoded = entry->kind;
                encoded--;
                category = (encoded << 3) | FIELD_LOOK_CROP;
                encoded = entry->growthFlags & FIELD_TILE_STEP_MASK;
                category += encoded;
            }
            field_cell_retexture(column, row, category);
            column++;
            entry++;
        } while (column < FIELD_COLS);
        row++;
    } while (row < FIELD_ROWS);
}


static s32 field_column;
static s32 field_row;
/* The point the field sweep is at on the ground. */
static VECTOR field_probe_point;
static FarmCellLook field_cell_look;
/* The neighbour being reshaped, and its new shape (-1 while none is picked). */
static u16 field_neighbour_x;
static u16 field_neighbour_y;
static s16 field_tile_shape;

/* Sweep the grid of probe points over the ground and keep every hit whose bit 3 is
 * set. A zero restart begins at the top; otherwise the sweep resumes where it stopped. */
s32 field_probe_sweep(s16 restart) {
    FarmModeGlobal *grid;
    FarmTmdQuad *hit;

    if (restart == 0) {
        field_row = 0;
        field_probe_point.vy = 0;
        field_probe_point.vz = 1500;
    }
    while (field_row < FIELD_ROWS) {
        grid = &farm_area.mode;
        field_probe_point.vx = FIELD_ORIGIN_X;
        field_column = 0;
        do {
            farm_settle_on_ground(&field_probe_point, 0xC000);
            hit = ground_hit_primitive();
            if (hit->header & 8) {
                grid->quads[field_row][field_column] = hit;
            }
            field_column++;
            field_probe_point.vx += FIELD_CELL_SIZE;
        } while (field_column < FIELD_COLS);
        field_row++;
        field_probe_point.vz -= FIELD_CELL_SIZE;
    }
    return 1;
}

/* Clear every greenhouse cell to bare soil. */
void greenhouse_clear_cells(void) {
    s32 i;
    s32 j;
    FarmFieldTile *rec;

    for (i = 0; i < GREENHOUSE_SIZE; i++) {
        for (j = 0; j < GREENHOUSE_SIZE; j++) {
            rec = &game_state.slots7x7[i].rows[j];
            rec->value = ITEM_SOIL;
            rec->ground = ITEM_SOIL;
            rec->kind = 0;
            rec->unused5 = 0;
            rec->growthFlags = 0;
        }
    }
}

static int field_stub_1(void) {
    return 0;
}

/* The daily crop tick for both fields; the greenhouse is only ticked once it
 * is built. Out of season a crop withers back to soil, except that last
 * season's crop past its first stage drops back to stage 1. In winter weeds
 * are cleared and grown tiles go back to stage 1. A watered crop counts a day
 * and moves to its next stage once it has had the days that stage needs. */
void field_daily_tick(void) {
    s32 which;
    s32 rows;
    s32 cols;
    s32 row;
    s32 col;
    FarmFieldTile *tile;
    FarmGrowth *growth;
    FarmCrop *crop;
    ItemInfo *item;
    s32 isCrop;
    s32 season;
    u16 cropSeason;

    for (which = 0; which < 2; which++) {
        if (which == 0) {
            rows = FIELD_ROWS;
            cols = FIELD_COLS;
        } else {
            rows = GREENHOUSE_SIZE;
            cols = GREENHOUSE_SIZE;
            if (game_state.progress.estate.greenhouseLevel == 0) {
                continue;
            }
        }
        for (row = 0; row < rows; row++) {
            for (col = 0; col < cols; col++) {
                tile = field_tile_at(which, col, row);
                if (tile == 0) {
                    continue;
                }
                item = &game_state.itemInfo[tile->value];
                crop = &game_state.cropTable[item->crop];
                growth = (FarmGrowth *)&tile->kind;
                if (which == 0) {
                    field_tile_season_result(tile);
                }
                isCrop = item_is_seed(tile->value);
                if (which == 0) {
                    if (game_state.progress.clock.date.season == SEASON_WINTER) {
                        if (tile->value == ITEM_WEEDS) {
                            field_tile_reset(tile);
                        } else if (tile->value == FIELD_TILE_GROWN && tile->kind != 0) {
                            tile->kind = 1;
                            growth->days = 0;
                            growth->flags = 0;
                        }
                    }
                    if (!isCrop) {
                        field_tile_dry(tile);
                        continue;
                    }
                    cropSeason = crop->season;
                    season = game_state.progress.clock.date.season;
                    if (cropSeason != season) {
                        if (season == SEASON_WINTER) {
                            if (tile->value != FIELD_TILE_GROWN) {
                                tile->value = ITEM_TILLED_SOIL;
                                tile->ground = ITEM_TILLED_SOIL;
                                growth->stage = 0;
                                growth->days = 0;
                                growth->flags = 0;
                            }
                        } else if (cropSeason != CROP_SEASON_ANY) {
                            if (growth->stage != 0 && cropSeason == season - 1) {
                                growth->stage = 1;
                            } else {
                                tile->value = ITEM_TILLED_SOIL;
                                tile->ground = ITEM_TILLED_SOIL;
                                growth->stage = 0;
                                growth->flags = 0;
                            }
                            growth->days = 0;
                            field_tile_dry(tile);
                            continue;
                        }
                    }
                } else if (!isCrop) {
                    field_tile_dry(tile);
                    continue;
                }
                if (tile->ground == ITEM_WATERED_SOIL || tile->ground == ITEM_WATERED_SOWN_SOIL || tile->value == FIELD_TILE_GROWN) {
                    field_tile_dry(tile);
                    if (growth->stage != crop->grownStage) {
                        if (growth->flags & FIELD_TILE_SKIP_DAY) {
                            growth->flags &= ~FIELD_TILE_SKIP_DAY;
                        } else {
                            growth->days++;
                        }
                        if (crop->days[growth->stage] <= growth->days) {
                            tile->ground = ITEM_TILLED_SOIL;
                            growth->days = 0;
                            growth->stage++;
                        }
                        if (tile->value == FIELD_TILE_GROWN) {
                            field_reshape_grown(row, col, which, 0, 1);
                        }
                    }
                }
            }
        }
    }
}

/* 1 when the farm is in `mode` and the cell's tile has no bit 3 hit. */
static inline s32 farm_tile_free(s32 x, s32 y, s32 mode) {
    FarmTmdQuad *tile;

    tile = farm_area.mode.quads[y][x];
    if (farm_area.mode.place.location != mode || (tile->header & 8)) {
        return 0;
    }
    return 1;
}

/* Convert a world position to a cell of the 25 x 45 farm grid (399 units a
 * cell). Returns 2 when the position lies just before the grid (the cell is
 * stepped back by one), 3 when it lies beyond it, else 1 for a free cell and 0
 * for an occupied one. */
s32 field_world_to_cell(VECTOR *position, s16 *column, s16 *row) {
    VECTOR copy;
    s16 x;
    s16 z;
    s32 rawZ;

    copy = *position;
    *column = position->vx + 4400;
    rawZ = 1600 - (u16)position->vz;
    *row = rawZ;
    x = *column;
    *column = x / 399;
    *row = *row / 399;
    z = rawZ;
    if (x < 0) {
        *column -= 1;
        return 2;
    }
    if (*column >= FIELD_COLS) {
        return 3;
    }
    if (z < 0) {
        *row -= 1;
        return 2;
    }
    if (*row >= FIELD_ROWS) {
        return 3;
    }
    if (*column >= 0 && *column < FIELD_COLS && *row >= 0) {
        return farm_tile_free(*column, *row, 1);
    }
    return 1;
}

/* The retexture handler of the farm (0) and of the greenhouse (1). */
static FarmEventHandler field_retexture_handlers[2] = {field_cell_retexture, greenhouse_retexture_quad};

/* The {columns, rows} limits of the farm (0) and the greenhouse (1). */
static short field_limits[2][2] = {{FIELD_COLS, FIELD_ROWS}, {GREENHOUSE_SIZE, GREENHOUSE_SIZE}};

/* Check a tile position against the limits of field `tableIndex` (0 farm, 1 greenhouse).
 * When `checkTile` is set the tile's quad must also carry bit 3. Returns 1 when the
 * check fails, else 0. */
s32 field_coords_check(s16 tileX, s16 tileY, s16 checkTile, s16 tableIndex) {
    s32 x;
    s32 y;
    s32 checkFlag;
    s16 *limits;
    FarmTmdQuad *tile;

    x = tileX;
    y = tileY;
    checkFlag = checkTile;
    limits = field_limits[tableIndex];
    if (x < 0 || x >= limits[0]) {
        return 1;
    }
    if (y < 0 || y >= limits[1]) {
        return 1;
    }
    tile = farm_area.mode.quads[y][x];
    if (checkFlag != 0 && (tile->header & 8) == 0) {
        return 1;
    }
    return 0;
}

/* Turn a tile position into a world position: on the farm (which 0) or in the
 * greenhouse (which 1). */
void field_result_coords(s16 x, s16 y, VECTOR *pos, s16 which) {
    s32 value0;
    s32 value1;

    if (which == 0) {
        value0 = x * FIELD_CELL_SIZE + FIELD_ORIGIN_X;
        value1 = y * FIELD_CELL_SIZE;
        pos->vx = value0;
        value0 = FIELD_ORIGIN_Z - value1;
    } else {
        value0 = x * FIELD_CELL_SIZE + GREENHOUSE_ORIGIN_X;
        value1 = y * FIELD_CELL_SIZE;
        pos->vx = value0;
        value0 = GREENHOUSE_ORIGIN_Z - value1;
    }
    pos->vz = value0;
}

/* Plant a seed on a tilled or watered tile: the soil becomes sown (or watered sown),
 * the tile takes the seed (magic red seeds may give a rare flower), and the handler
 * redraws it. */
s32 field_event_dispatch(s16 tileX, s16 tileY, s16 layer, s16 itemId) {
    FarmFieldTile *tile;
    s32 action;
    s32 x;
    s32 y;
    s32 state;
    s32 which;
    FarmEventHandler *handlers;
    x = tileX;
    y = tileY;
    state = layer;
    which = itemId;
    tile = field_tile_at(x, y, state);
    if (tile == 0) return 0;
    if ((u32)(tile->value - ITEM_TILLED_SOIL) >= 2U) return 0;
    if (item_is_seed(which) == 0) return 0;
    if (game_state.progress.clock.date.season == SEASON_WINTER && x == 0) return 0;
    if (tile->ground == ITEM_TILLED_SOIL) {
        tile->ground = ITEM_SOWN_SOIL;
        action = FIELD_LOOK_SOWN;
    } else {
        tile->ground = ITEM_WATERED_SOWN_SOIL;
        action = FIELD_LOOK_WATERED_SOWN;
    }
    if (which == ITEM_MAGIC_RED_SEEDS) {
        if (rand_range(25) == 0)
            tile->value = ITEM_RED_MAGIC_RED_PLANT;
        else
            tile->value = ITEM_BLUE_MAGIC_RED_PLANT;
    } else
        tile->value = which;
    tile->kind = 0;
    tile->unused5 = 0;
    handlers = field_retexture_handlers;
    tile->growthFlags &= ~FIELD_TILE_SKIP_DAY;
    handlers[x](y, state, action);
    return 1;
}

/* Water a dry soil tile at a map position and redraw it. Returns 1 when watered. */
s32 field_gate_open(s16 layer, s16 tileX, s16 tileY, s32 unused) {
    FarmFieldTile *record;
    s32 which;
    s32 x;
    s32 y;
    u32 type;
    s32 mode;

    which = layer;
    x = tileX;
    y = tileY;
    record = field_tile_at(which, x, y);
    if (record == 0) goto fail;
    type = record->ground;
    if ((u16)type != ITEM_SOWN_SOIL && (u16)type != ITEM_TILLED_SOIL) return 0;
    type = record->ground;
    if (record->value == FIELD_TILE_GROWN && record->kind != 0) {
    fail:
        return 0;
    }
    if (game_state.progress.clock.date.season == SEASON_WINTER && which == 0) goto fail;
    if (type == ITEM_TILLED_SOIL) {
        record->ground = ITEM_WATERED_SOIL;
        mode = FIELD_LOOK_WATERED;
    } else {
        record->ground = ITEM_WATERED_SOWN_SOIL;
        mode = FIELD_LOOK_WATERED_SOWN;
    }
    field_retexture_handlers[which](x, y, mode);
    return 1;
}

/* Handle a request on a field tile: water or till it, spawn a farm object at the tile, and
 * redraw it. */
s32 field_request_dispatch(s16 mode, s16 x, s16 y) {
    FarmFieldTile *entry;
    s32 callback_index;
    u16 item;
    u8 subitem;
    VECTOR result;
    entry = field_tile_at(mode, x, y);
    if (entry == 0) return 0;
    if (game_state.progress.clock.date.season == SEASON_WINTER && mode == 0) return 0;
    item = entry->value;
    subitem = 0;
    if (item_is_seed(item) == 1) subitem = entry->kind;
    if (item_tile_request_ok(item, subitem) == 0) return 0;
    if (entry->value == ITEM_WATERED_SOIL) {
        callback_index = FIELD_LOOK_WATERED;
        entry->value = ITEM_WATERED_SOIL;
        entry->ground = ITEM_WATERED_SOIL;
    } else if (entry->ground == ITEM_WATERED_SOWN_SOIL) {
        callback_index = FIELD_LOOK_WATERED;
        entry->value = ITEM_WATERED_SOIL;
        entry->ground = ITEM_WATERED_SOIL;
    } else {
        callback_index = FIELD_LOOK_TILLED;
        entry->value = ITEM_TILLED_SOIL;
        entry->ground = ITEM_TILLED_SOIL;
    }
    if (mode == 0 && game_state.world.weather == WEATHER_RAIN) {
        callback_index = FIELD_LOOK_WATERED;
        entry->value = ITEM_WATERED_SOIL;
        entry->ground = ITEM_WATERED_SOIL;
    }
    entry->kind = 0;
    entry->unused5 = 0;
    entry->value = entry->ground;
    entry->growthFlags &= ~FIELD_TILE_SKIP_DAY;
    field_result_coords(x, y, &result, mode);
    result.vy = 0;
    farm_create_object(result, 0);
    field_retexture_handlers[mode](x, y, callback_index);
    field_reshape_grown(y, x, mode, 1, 0);
    return 1;
}

/* Harvest a tile's item: a crop regrows to its regrow stage or the tile is cleared. Returns
 * the item picked up, with the season's substitutions, or 0 when there is nothing to
 * harvest. */
s32 field_tile_item_stage(FarmFieldTile *tile) {
    ItemId item = tile->value;
    u16 result;
    s16 stage;

    if (item_is_field_object(item) == 0 && item_is_seed(item) == 0) {
        return 0;
    }
    if (item_is_seed(item) == 1) {
        if (tile->kind != crop_grown_stage(item)) {
            return 0;
        }
        if (tile->value == FIELD_TILE_GROWN) {
            return 0;
        }
        result = crop_harvest_item(item);
        stage = crop_regrow_stage(item);
        if (stage == -1) {
            tile->kind = 0;
            tile->unused5 = 0;
            tile->growthFlags = 0;
            tile->value = tile->ground;
        } else {
            tile->kind = stage;
            tile->growthFlags |= FIELD_TILE_SKIP_DAY;
        }
        tile->unused5 = 0;
    } else {
        result = item;
        tile->kind = 0;
        tile->unused5 = 0;
        tile->growthFlags = 0;
        tile->value = tile->ground;
    }

    if (game_state.progress.clock.date.season == SEASON_FALL) {
        if (result == ITEM_WEEDS) {
            result = ITEM_DEAD_WEED;
        }
    } else if (game_state.progress.clock.date.season == SEASON_WINTER) {
        if (result == ITEM_STONE) {
            result = ITEM_WINTER_STONE;
        } else if (result == ITEM_BRANCH) {
            result = ITEM_WINTER_BRANCH;
        } else if (result == ITEM_LUMBER) {
            result = ITEM_WINTER_LUMBER;
        }
    }
    return result;
}

s32 field_tile_harvest(s16 which, s16 x, s16 y) {
    FarmFieldTile *tile;
    u8 kind;
    VECTOR pos;

    tile = field_tile_at(which, x, y);
    if (tile == 0) {
        return 0;
    }
    kind = 0;
    if (item_is_seed(tile->value) == 1) {
        kind = tile->kind;
    }
    if (item_tile_hand_pickable(tile->value, kind) == 1) {
        if (tile->value == FIELD_TILE_GROWN) {
            estate_fodder_add(FODDER_ADD_PICKED, 0);
            field_request_submit_scaled(tile->value, which, x, y);
            field_grown_cell_request(y, x, which);
        } else {
            field_request_submit_scaled(tile->value, which, x, y);
            tile->kind = 0;
            tile->unused5 = 0;
            tile->growthFlags = 0;
            tile->value = tile->ground;
        }
        if (tile->value == FIELD_TILE_GROWN || tile->value == ITEM_WEEDS) {
            field_result_coords(x, y, &pos, which);
            pos.vy = 0;
            farm_create_object(pos, 1);
        }
        return 1;
    }
    return 0;
}

/* Clear the tile at (which, x, y) with a tool of the given power and reach,
 * when its item allows it. A 2x2 stone or rock clears all four of its tiles and is
 * submitted from one of them. Returns 1 when cleared. */
s32 field_tile_clear_tool(s16 which, s16 x, s16 y, s16 reach, s16 power) {
    s32 result;
    FarmFieldTile *tile;
    u16 item;
    u8 kind;

    result = 0;
    tile = field_tile_at(which, x, y);
    if (tile == 0) {
        return 0;
    }
    item = tile->value;
    kind = 0;
    if (item_is_seed(tile->value) == 1) {
        kind = tile->kind;
    }
    if (item_tile_clear_by_tool(item, kind) == 1 && power >= item_tile_min_power(item, kind) &&
        reach >= item_tile_min_reach(item, kind)) {
        switch (item) {
        case ITEM_BIG_STONE:
        case ITEM_ROCK:
            field_tile_reset(tile);
            field_tile_reset(&game_state.fieldTiles[y + 1].rows[x]);
            field_tile_reset(&game_state.fieldTiles[y].rows[x + 1]);
            field_tile_reset(&game_state.fieldTiles[y + 1].rows[x + 1]);
            field_request_submit_scaled(item, which, x + 1, y + 1);
            break;
        case FIELD_TILE_BIG_STONE_1:
        case FIELD_TILE_ROCK_1:
            field_tile_reset(tile);
            field_tile_reset(&game_state.fieldTiles[y + 1].rows[x]);
            field_tile_reset(&game_state.fieldTiles[y].rows[x - 1]);
            field_tile_reset(&game_state.fieldTiles[y + 1].rows[x - 1]);
            field_request_submit_scaled(item, which, x, y + 1);
            break;
        case FIELD_TILE_BIG_STONE_3:
        case FIELD_TILE_ROCK_3:
            field_tile_reset(tile);
            field_tile_reset(&game_state.fieldTiles[y - 1].rows[x]);
            field_tile_reset(&game_state.fieldTiles[y].rows[x + 1]);
            field_tile_reset(&game_state.fieldTiles[y - 1].rows[x + 1]);
            field_request_submit_scaled(item, which, x + 1, y);
            break;
        case FIELD_TILE_BIG_STONE_2:
        case FIELD_TILE_ROCK_2:
            field_tile_reset(tile);
            field_tile_reset(&game_state.fieldTiles[y - 1].rows[x]);
            field_tile_reset(&game_state.fieldTiles[y - 1].rows[x - 1]);
            field_tile_reset(&game_state.fieldTiles[y].rows[x - 1]);
            field_request_submit_scaled(item, which, x, y);
            break;
        case ITEM_LUMBER:
        case ITEM_STONE:
        case FIELD_TILE_LUMBER:
            field_request_submit_scaled(item, which, x, y);
            /* fall through */
        default:
            field_tile_reset(tile);
            field_retexture_handlers[which](x, y, FIELD_LOOK_BARE);
            break;
        }
        result = 1;
    }
    return result;
}

/* Clear the tile at (which, x, y) with the alternate tool when its item allows
 * it. A 2x2 stump clears all four of its tiles and is re-announced from one of
 * them; a branch and plain tiles clear just the one. Returns 1 when cleared. */
s32 field_tile_clear_tool_alt(s16 which, s16 x, s16 y, s16 reach, s16 power) {
    s32 result;
    FarmFieldTile *tile;
    u16 item;
    u8 kind;

    result = 0;
    tile = field_tile_at(which, x, y);
    if (tile == 0) {
        return 0;
    }
    item = tile->value;
    kind = 0;
    if (item_is_seed(tile->value) == 1) {
        kind = tile->kind;
    }
    if (item_tile_clear_by_alt_tool(item, kind) == 1 && power >= item_tile_min_power(item, kind) &&
        reach >= item_tile_min_reach(item, kind)) {
        switch (item) {
        case ITEM_STUMP:
            estate_lumber_add(LUMBER_ADD_STUMP, 0);
            field_tile_reset(tile);
            field_tile_reset(&game_state.fieldTiles[y + 1].rows[x]);
            field_tile_reset(&game_state.fieldTiles[y].rows[x + 1]);
            field_tile_reset(&game_state.fieldTiles[y + 1].rows[x + 1]);
            field_request_submit_scaled(ITEM_STUMP, which, x + 1, y + 1);
            break;
        case FIELD_TILE_STUMP_1:
            estate_lumber_add(LUMBER_ADD_STUMP, 0);
            field_tile_reset(tile);
            field_tile_reset(&game_state.fieldTiles[y + 1].rows[x]);
            field_tile_reset(&game_state.fieldTiles[y].rows[x - 1]);
            field_tile_reset(&game_state.fieldTiles[y + 1].rows[x - 1]);
            field_request_submit_scaled(ITEM_STUMP, which, x, y + 1);
            break;
        case FIELD_TILE_STUMP_3:
            estate_lumber_add(LUMBER_ADD_STUMP, 0);
            field_tile_reset(tile);
            field_tile_reset(&game_state.fieldTiles[y - 1].rows[x]);
            field_tile_reset(&game_state.fieldTiles[y].rows[x + 1]);
            field_tile_reset(&game_state.fieldTiles[y - 1].rows[x + 1]);
            field_request_submit_scaled(ITEM_STUMP, which, x + 1, y);
            break;
        case FIELD_TILE_STUMP_2:
            estate_lumber_add(LUMBER_ADD_STUMP, 0);
            field_tile_reset(tile);
            field_tile_reset(&game_state.fieldTiles[y - 1].rows[x]);
            field_tile_reset(&game_state.fieldTiles[y - 1].rows[x - 1]);
            field_tile_reset(&game_state.fieldTiles[y].rows[x - 1]);
            field_request_submit_scaled(ITEM_STUMP, which, x, y);
            break;
        case ITEM_BRANCH:
            estate_lumber_add(LUMBER_ADD_PICKED, 0);
            field_tile_reset(tile);
            field_request_submit_scaled(ITEM_BRANCH, which, x, y);
            break;
        default:
            field_tile_reset(tile);
            break;
        }
        result = 1;
    }
    return result;
}

/* Roll each field tile toward lumber. The range is 32 for mode 0, else 16. */
void field_table_refresh(s32 mode) {
    s32 row;
    s32 column;
    u32 amount;

    amount = 16;
    if (mode == 0) {
        amount = 32;
    }
    row = 0;
    do {
        column = 0;
        do {
            field_tile_roll_to_lumber(&game_state.fieldTiles[row].rows[column].value, amount);
            column++;
        } while (column < FIELD_COLS);
        row++;
    } while (row < FIELD_ROWS);
}

/* The storm on the farm field: a seed may wither to tilled soil, a grown crop may regrow,
 * and lumber may turn to field lumber. */
void field_storm_damage(void) {
    s32 row;
    s32 column;
    FarmFieldTile *entry;

    for (row = 0; row < FIELD_ROWS; row++) {
        for (column = 0; column < FIELD_COLS; column++) {
            entry = &game_state.fieldTiles[row].rows[column];
            field_tile_wither(entry, 16);
            field_tile_regrow_roll(entry, 8);
            field_tile_roll_to_lumber(&entry->value, 16);
        }
    }
}

/* Find a grown tile ready to harvest (FIELD_TILE_GROWN, at least the grass's grown
 * stage) and request it: first in the 3 x 3 block around the position's tile, then
 * anywhere on the grid. Returns 1 when one was found. */
s32 field_find_ready_grown(VECTOR *position) {
    s16 col;
    s16 row;
    s16 grownStage;
    s32 y;
    s32 x;
    s32 result;
    FarmFieldTile *tile;

    result = 0;
    grownStage = crop_grown_stage(ITEM_GRASS_SEEDS);
    if (field_world_to_cell(position, &col, &row) == 0) {
        row -= 1;
        row = (row < 0) ? 0 : row;
        col -= 1;
        col = (col < 0) ? 0 : col;
        for (y = row; y < row + 3 && y < FIELD_ROWS; y++) {
            for (x = col; x < col + 3 && x < FIELD_COLS; x++) {
                tile = &game_state.fieldTiles[y].rows[x];
                if (tile->value == FIELD_TILE_GROWN && tile->kind >= grownStage) {
                    field_grown_cell_request(y, x, 0);
                    result = 1;
                    goto done;
                }
            }
        }
    }
    for (y = 0; y < FIELD_ROWS; y++) {
        for (x = 0; x < FIELD_COLS; x++) {
            tile = &game_state.fieldTiles[y].rows[x];
            if (tile->value == FIELD_TILE_GROWN && tile->kind >= grownStage) {
                field_grown_cell_request(y, x, 0);
                result = 1;
                goto done;
            }
        }
    }
done:
    return result;
}

/* Find the cells of two opposite corners of the square 3600 units either side of the
 * scroll centre, clamped to the grid. out[0] and out[1] are the first corner's column
 * and row; out[2] and out[3] the second's. */
static void field_square_cells(s32 *out) {
    s32 pass;
    s32 worldX;
    s32 worldZ;
    s32 column;
    s32 row;
    s16 centreX;
    s16 centreZ;

    centreX = farm_area.mode.scroll.centre.vx;
    centreZ = farm_area.mode.scroll.centre.vz;
    for (pass = 0; pass < 2; pass++) {
        if (pass != 0) {
            worldX = centreX + 3600;
            worldZ = centreZ - 3600;
        } else {
            worldX = centreX - 3600;
            worldZ = centreZ + 3600;
        }
        column = (worldX + 4400) / 400;
        row = (worldZ - 1600) / -400;
        if (column < 0) {
            column = 0;
        } else if (column >= 46) {
            column = 45;
        }
        if (row < 0) {
            row = 0;
        } else if (row >= 26) {
            row = 25;
        }
        out[pass * 2] = column;
        out[pass * 2 + 1] = row;
    }
}

/* Draw the farm cells near the scroll centre. The look depends on the season;
 * each cell is projected and drawn with its sprite state, lifted by the look's
 * offset. */
void field_draw_grid(void) {
    s32 bounds[4];
    SVECTOR input;
    FarmScreenPoint result;
    s32 attr;
    s32 sprite;
    s32 lift;
    s32 row;
    s32 col;
    if (game_state.debugFlags & DEBUG_FLAG_NO_FIELD_DRAW) {
        return;
    }
    field_square_cells(bounds);
    if (game_state.progress.clock.date.season == SEASON_SPRING) {
        field_cell_look = field_cell_look_spring;
    } else if (game_state.progress.clock.date.season == SEASON_SUMMER) {
        field_cell_look = field_cell_look_summer;
    } else if (game_state.progress.clock.date.season == SEASON_FALL) {
        field_cell_look = field_cell_look_fall;
    } else {
        field_cell_look = field_cell_look_winter;
    }
    for (row = bounds[1]; row < bounds[3]; row++) {
        for (col = bounds[0]; col < bounds[2]; col++) {
            input.vx = col * FIELD_CELL_SIZE - 4050;
            input.vy = -100;
            input.vz = 1250 - row * FIELD_CELL_SIZE;
            if (estate_point_project(&input, &result) == 0) {
                continue;
            }
            if (field_cell_look(game_state.fieldTiles[row].rows[col].value, game_state.fieldTiles[row].rows[col].kind,
                           &attr, &sprite, &lift) == 0) {
                continue;
            }
            anim_draw_sprt_cursor(&current_frame->ot[result.depth >> 2],
                          &game_state.work.sprites[sprite], attr, 0, result.x, result.y + lift);
        }
    }
}

/* The look of a field cell in spring: fixed items get a sprite, attribute and lift;
 * seed crops are animated. Returns 0 for cells that are not drawn. */
static s32 field_cell_look_spring(s32 code, s32 kind, s32 *attr, s32 *sprite, s32 *lift) {
    s32 result;

    result = 1;
    *attr = 0;
    *sprite = 0;
    *lift = 0;
    switch (code) {
    case ITEM_LUMBER:
        *sprite = 5;
        break;
    case FIELD_TILE_LUMBER:
        *attr = 1;
        *sprite = 5;
        break;
    case ITEM_WEEDS:
        *sprite = 6;
        break;
    case ITEM_STONE:
        *sprite = 0;
        break;
    case ITEM_BRANCH:
        *sprite = 4;
        break;
    case FIELD_TILE_BIG_STONE_2:
        *sprite = 1;
        *lift = -10;
        break;
    case FIELD_TILE_ROCK_2:
        *sprite = 2;
        *lift = -10;
        break;
    case FIELD_TILE_STUMP_2:
        *sprite = 3;
        *lift = -15;
        break;
    case ITEM_BASKET:
        *sprite = 15;
        break;
    case ITEM_TURNIP_SEEDS:
        result = field_cell_look_anim(kind, kind + 2, 7, attr, sprite);
        break;
    case ITEM_POTATO_SEEDS:
        result = field_cell_look_anim(kind, kind + 2, 8, attr, sprite);
        break;
    case ITEM_CUCUMBER_SEEDS:
        result = field_cell_look_anim(kind, kind + 2, 9, attr, sprite);
        break;
    case ITEM_STRAWBERRY_SEEDS:
        result = field_cell_look_anim(kind, kind + 2, 10, attr, sprite);
        break;
    case ITEM_CABBAGE_SEEDS:
        result = field_cell_look_anim(kind, kind + 2, 11, attr, sprite);
        break;
    case ITEM_MOONDROP_SEEDS:
        result = field_cell_look_anim(kind, kind + 2, 12, attr, sprite);
        break;
    case ITEM_TOY_FLOWER_SEEDS:
        result = field_cell_look_anim(kind, kind + 2, 13, attr, sprite);
        break;
    default:
        result = 0;
    }
    return result;
}

static s32 field_cell_look_summer(s32 code, s32 kind, s32 *attr, s32 *sprite, s32 *lift) {
    s32 result;

    result = 1;
    *attr = 0;
    *sprite = 0;
    *lift = 0;
    switch (code) {
    case ITEM_LUMBER:
        *sprite = 5;
        break;
    case FIELD_TILE_LUMBER:
        *attr = 1;
        *sprite = 5;
        break;
    case ITEM_WEEDS:
        *sprite = 6;
        break;
    case ITEM_STONE:
        *sprite = 0;
        break;
    case ITEM_BRANCH:
        *sprite = 4;
        break;
    case FIELD_TILE_BIG_STONE_2:
        *sprite = 1;
        *lift = -10;
        break;
    case FIELD_TILE_ROCK_2:
        *sprite = 2;
        *lift = -10;
        break;
    case FIELD_TILE_STUMP_2:
        *sprite = 3;
        *lift = -15;
        break;
    case ITEM_TOMATO_SEEDS:
        result = field_cell_look_anim(kind, kind + 2, 12, attr, sprite);
        break;
    case ITEM_CORN_SEEDS:
        result = field_cell_look_anim(kind, kind + 2, 13, attr, sprite);
        break;
    case ITEM_ONION_SEEDS:
        result = field_cell_look_anim(kind, kind + 2, 14, attr, sprite);
        break;
    case ITEM_PUMPKIN_SEEDS:
        result = field_cell_look_anim(kind, kind + 2, 15, attr, sprite);
        break;
    case ITEM_PINEAPPLE_SEEDS:
        result = field_cell_look_anim(kind, kind + 2, 16, attr, sprite);
        break;
    case ITEM_PINK_CAT_SEEDS:
        result = field_cell_look_anim(kind, kind + 2, 17, attr, sprite);
        break;
    case ITEM_ORANGE_CUP_SEEDS:
        result = field_cell_look_anim(kind, kind + 2, 18, attr, sprite);
        break;
    case ITEM_TURNIP_SEEDS:
        result = field_cell_look_anim(kind, 0, 7, attr, sprite);
        break;
    case ITEM_POTATO_SEEDS:
        result = field_cell_look_anim(kind, 0, 8, attr, sprite);
        break;
    case ITEM_CUCUMBER_SEEDS:
        result = field_cell_look_anim(kind, 0, 9, attr, sprite);
        break;
    case ITEM_STRAWBERRY_SEEDS:
        result = field_cell_look_anim(kind, 0, 10, attr, sprite);
        break;
    case ITEM_CABBAGE_SEEDS:
        result = field_cell_look_anim(kind, 0, 11, attr, sprite);
        break;
    case ITEM_MOONDROP_SEEDS:
        result = field_cell_look_anim(kind, 0, 40, attr, sprite);
        break;
    case ITEM_TOY_FLOWER_SEEDS:
        result = field_cell_look_anim(kind, 0, 41, attr, sprite);
        break;
    default:
        result = 0;
    }
    return result;
}

static s32 field_cell_look_fall(s32 code, s32 kind, s32 *attr, s32 *sprite, s32 *lift) {
    s32 result;

    result = 1;
    *attr = 0;
    *sprite = 0;
    *lift = 0;
    switch (code) {
    case ITEM_LUMBER:
        *sprite = 5;
        break;
    case FIELD_TILE_LUMBER:
        *attr = 1;
        *sprite = 5;
        break;
    case ITEM_WEEDS:
        *sprite = 39;
        break;
    case ITEM_STONE:
        *sprite = 0;
        break;
    case ITEM_BRANCH:
        *sprite = 4;
        break;
    case FIELD_TILE_BIG_STONE_2:
        *sprite = 1;
        *lift = -10;
        break;
    case FIELD_TILE_ROCK_2:
        *sprite = 2;
        *lift = -10;
        break;
    case FIELD_TILE_STUMP_2:
        *sprite = 3;
        *lift = -15;
        break;
    case ITEM_EGGPLANT_SEEDS:
        result = field_cell_look_anim(kind, kind + 2, 12, attr, sprite);
        break;
    case ITEM_CARROT_SEEDS:
        result = field_cell_look_anim(kind, kind + 2, 13, attr, sprite);
        break;
    case ITEM_SWEET_POTATO_SEEDS:
        result = field_cell_look_anim(kind, kind + 2, 14, attr, sprite);
        break;
    case ITEM_SPINACH_SEEDS:
        result = field_cell_look_anim(kind, kind + 2, 15, attr, sprite);
        break;
    case ITEM_GREEN_PEPPER_SEEDS:
        result = field_cell_look_anim(kind, kind + 2, 16, attr, sprite);
        break;
    case ITEM_RED_MAGIC_RED_PLANT:
        result = field_cell_look_anim(kind, kind + 2, 25, attr, sprite);
        break;
    case ITEM_BLUE_MAGIC_RED_PLANT:
        result = field_cell_look_anim(kind, kind + 2, 17, attr, sprite);
        break;
    case ITEM_TOMATO_SEEDS:
        result = field_cell_look_anim(kind, 0, 7, attr, sprite);
        break;
    case ITEM_CORN_SEEDS:
        result = field_cell_look_anim(kind, 0, 8, attr, sprite);
        break;
    case ITEM_ONION_SEEDS:
        result = field_cell_look_anim(kind, 0, 9, attr, sprite);
        break;
    case ITEM_PUMPKIN_SEEDS:
        result = field_cell_look_anim(kind, 0, 10, attr, sprite);
        break;
    case ITEM_PINEAPPLE_SEEDS:
        result = field_cell_look_anim(kind, 0, 11, attr, sprite);
        break;
    case ITEM_PINK_CAT_SEEDS:
        result = field_cell_look_anim(kind, 0, 37, attr, sprite);
        break;
    case ITEM_ORANGE_CUP_SEEDS:
        result = field_cell_look_anim(kind, 0, 38, attr, sprite);
        break;
    default:
        result = 0;
    }
    return result;
}

static s32 field_cell_look_winter(s32 code, s32 unused, s32 *attr, s32 *sprite, s32 *lift) {
    s32 result;

    result = 1;
    *attr = 0;
    *sprite = 0;
    *lift = 0;
    switch (code) {
    case ITEM_LUMBER:
        *sprite = 5;
        break;
    case FIELD_TILE_LUMBER:
        *attr = 1;
        *sprite = 5;
        break;
    case ITEM_STONE:
        *sprite = 0;
        break;
    case ITEM_BRANCH:
        *sprite = 4;
        break;
    case FIELD_TILE_BIG_STONE_2:
        *sprite = 1;
        *lift = -10;
        break;
    case FIELD_TILE_ROCK_2:
        *sprite = 2;
        *lift = -10;
        break;
    case FIELD_TILE_STUMP_2:
        *sprite = 3;
        *lift = -15;
        break;
    default:
        result = 0;
    }
    return result;
}

static s32 field_cell_look_anim(s32 kind, s32 attr, s32 sprite, s32 *outAttr, s32 *outSprite) {
    if (kind == 0) return 0;
    *outAttr = attr;
    *outSprite = sprite;
    return 1;
}

/* Draw the 7 x 7 greenhouse cells into the active ordering table. */
void greenhouse_draw(void) {
    SVECTOR input;
    FarmScreenPoint result;
    s32 attr;
    s32 sprite;
    s32 row;
    s32 col;
    s32 kind;
    u16 code;

    for (row = 0; row < GREENHOUSE_SIZE; row++) {
        for (col = 0; col < GREENHOUSE_SIZE; col++) {
            input.vx = col * FIELD_CELL_SIZE - 1250;
            input.vy = -100;
            input.vz = 850 - row * FIELD_CELL_SIZE;
            if (estate_point_project(&input, &result) == 0) {
                continue;
            }
            attr = 0;
            sprite = 0;
            code = game_state.slots7x7[row].rows[col].value;
            kind = game_state.slots7x7[row].rows[col].kind;
            switch (code) {
            case ITEM_LUMBER:
                sprite = 3;
                break;
            case FIELD_TILE_LUMBER:
                attr = 1;
                sprite = 3;
                break;
            case ITEM_STONE:
                sprite = 0;
                break;
            case ITEM_BRANCH:
                sprite = 2;
                break;
            case ITEM_TURNIP_SEEDS:
                if (field_cell_look_anim(kind, kind + 2, 4, &attr, &sprite) == 0) {
                    continue;
                }
                break;
            case ITEM_POTATO_SEEDS:
                if (field_cell_look_anim(kind, kind + 2, 5, &attr, &sprite) == 0) {
                    continue;
                }
                break;
            case ITEM_CUCUMBER_SEEDS:
                if (field_cell_look_anim(kind, kind + 2, 6, &attr, &sprite) == 0) {
                    continue;
                }
                break;
            case ITEM_STRAWBERRY_SEEDS:
                if (field_cell_look_anim(kind, kind + 2, 7, &attr, &sprite) == 0) {
                    continue;
                }
                break;
            case ITEM_CABBAGE_SEEDS:
                if (field_cell_look_anim(kind, kind + 2, 8, &attr, &sprite) == 0) {
                    continue;
                }
                break;
            case ITEM_TOMATO_SEEDS:
                if (field_cell_look_anim(kind, kind + 2, 9, &attr, &sprite) == 0) {
                    continue;
                }
                break;
            case ITEM_CORN_SEEDS:
                if (field_cell_look_anim(kind, kind + 2, 10, &attr, &sprite) == 0) {
                    continue;
                }
                break;
            case ITEM_ONION_SEEDS:
                if (field_cell_look_anim(kind, kind + 2, 11, &attr, &sprite) == 0) {
                    continue;
                }
                break;
            case ITEM_PUMPKIN_SEEDS:
                if (field_cell_look_anim(kind, kind + 2, 12, &attr, &sprite) == 0) {
                    continue;
                }
                break;
            case ITEM_PINEAPPLE_SEEDS:
                if (field_cell_look_anim(kind, kind + 2, 13, &attr, &sprite) == 0) {
                    continue;
                }
                break;
            case ITEM_EGGPLANT_SEEDS:
                if (field_cell_look_anim(kind, kind + 2, 14, &attr, &sprite) == 0) {
                    continue;
                }
                break;
            case ITEM_CARROT_SEEDS:
                if (field_cell_look_anim(kind, kind + 2, 15, &attr, &sprite) == 0) {
                    continue;
                }
                break;
            case ITEM_SWEET_POTATO_SEEDS:
                if (field_cell_look_anim(kind, kind + 2, 16, &attr, &sprite) == 0) {
                    continue;
                }
                break;
            case ITEM_SPINACH_SEEDS:
                if (field_cell_look_anim(kind, kind + 2, 17, &attr, &sprite) == 0) {
                    continue;
                }
                break;
            case ITEM_GREEN_PEPPER_SEEDS:
                if (field_cell_look_anim(kind, kind + 2, 18, &attr, &sprite) == 0) {
                    continue;
                }
                break;
            case ITEM_MOONDROP_SEEDS:
                if (field_cell_look_anim(kind, kind + 2, 20, &attr, &sprite) == 0) {
                    continue;
                }
                break;
            case ITEM_PINK_CAT_SEEDS:
                if (field_cell_look_anim(kind, kind + 2, 21, &attr, &sprite) == 0) {
                    continue;
                }
                break;
            case ITEM_BLUE_MAGIC_RED_PLANT:
                if (field_cell_look_anim(kind, kind + 2, 22, &attr, &sprite) == 0) {
                    continue;
                }
                break;
            case ITEM_RED_MAGIC_RED_PLANT:
                if (field_cell_look_anim(kind, kind + 2, 27, &attr, &sprite) == 0) {
                    continue;
                }
                break;
            case ITEM_TOY_FLOWER_SEEDS:
                if (field_cell_look_anim(kind, kind + 2, 23, &attr, &sprite) == 0) {
                    continue;
                }
                break;
            case ITEM_ORANGE_CUP_SEEDS:
                if (field_cell_look_anim(kind, kind + 2, 24, &attr, &sprite) == 0) {
                    continue;
                }
                break;
            default:
                continue;
            }
            anim_draw_sprt_cursor(&current_frame->ot[result.depth >> 2],
                          &game_state.work.sprites[sprite], attr, 0, result.x, result.y);
        }
    }
}

static void field_tile_reset(FarmFieldTile *tile) {
    tile->ground = ITEM_SOIL;
    tile->value = ITEM_SOIL;
    tile->kind = 0;
    tile->unused5 = 0;
    tile->growthFlags = 0;
}

/* Choose the season's weed roll and, when it hits, turn a bare, tilled or watered tile
 * into weeds: 1 in 128 in spring and fall, 1 in 64 in summer, never in winter. */
static void field_tile_season_result(FarmFieldTile *tile) {
    s16 season;
    s32 code;

    season = game_state.progress.clock.date.season;
    switch (season) {
    case SEASON_SPRING:
        code = 0x80;
        break;
    case SEASON_SUMMER:
        code = 0x40;
        break;
    case SEASON_FALL:
        code = 0x80;
        break;
    case SEASON_WINTER:
        code = 0;
        break;
    default:
        code = 0;
        break;
    }
    if (code == 0) {
        return;
    }
    if ((u32)((s16)tile->value - ITEM_SOIL) >= 3U) {
        return;
    }
    if (rand_range(code) != 0) {
        return;
    }
    tile->value = ITEM_WEEDS;
    tile->ground = ITEM_SOIL;
}

/* Turn the lumber at *p into field lumber, with a 1 in `range` chance. */
static void field_tile_roll_to_lumber(u16 *p, u32 range) {
    if (range && *p == ITEM_LUMBER && rand_range((u16)range) == 0) *p = FIELD_TILE_LUMBER;
}

/* A seed tile that has not grown withers to tilled soil, with a 1 in `range` chance. */
static void field_tile_wither(FarmFieldTile *tile, u32 range) {
    if (range != 0 && tile->value != FIELD_TILE_GROWN) {
        if (item_is_seed(tile->value) == 1) {
            if (rand_range(range & 0xFFFF) == 0) {
                tile->value = ITEM_TILLED_SOIL;
                tile->ground = ITEM_TILLED_SOIL;
                tile->kind = 0;
                tile->unused5 = 0;
                tile->growthFlags = 0;
            }
        }
    }
}

/* A grown crop with a regrow stage goes back to that stage, with a 1 in `range` chance. */
static void field_tile_regrow_roll(FarmFieldTile *tile, s32 range) {
    if (range != 0 && tile->value == FIELD_TILE_GROWN && tile->kind != 0) {
        if (rand_range((u16)range) == 0) {
            tile->kind = crop_regrow_stage(ITEM_GRASS_SEEDS);
            tile->unused5 = 0;
            tile->growthFlags |= FIELD_TILE_SKIP_DAY;
        }
    }
}

/* Bind the item's art, one animation file per sprite slot, up to 44 slots. */
void item_art_bind_sprites(s32 id) {
    s32 count;
    s32 slot;
    AnimState *state;
    AnimFile *entry;

    count = farm_item_tocs[id]->count;
    state = game_state.work.sprites;
    if (count >= 45) {
        count = 44;
    }
    slot = 0;
    if (count > 0) {
        do {
            entry = (AnimFile *)(farm_area.itemArt + ANIM_PACK_START(farm_item_tocs[id], slot));
            slot++;
            anim_bind_file(state, entry);
            state++;
        } while (slot < count);
    }
}

static FarmFieldSize field_grid_sizes[2] = {{FIELD_ROWS, FIELD_COLS}, {GREENHOUSE_SIZE, GREENHOUSE_SIZE}};

/* The 3 x 3 neighbourhood as nine (row, column) offset pairs; the centre is the fifth
 * pair. */
static s16 field_neighbour_offsets[18] = {
    -1, -1, -1, 0, -1, 1, 0, -1, 0, 0,
    0, 1, 1, -1, 1, 0, 1, 1,
};

/* Re-shape the grown tiles (FIELD_TILE_GROWN) around (x, y). For each tile of the 3 x 3
 * block, build a 9-bit mask of which of its 3 x 3 neighbours are grown and take the
 * shape (0-7) the mask gives; store it in the tile's low flag nibble and, unless
 * `quiet`, tell the field's handler. With `merge`, a lone or sparse centre tile
 * falls back to shape 0. */
static void field_reshape_grown(s16 y, s16 x, s16 which, s16 merge, s16 quiet) {
    FarmFieldSize *size;
    FarmFieldTile *tile;
    s32 i;
    s32 j;
    s32 k;
    u32 mask;
    s32 count;
    s16 nx;
    s16 ny;

    size = &field_grid_sizes[which];
    if (merge == 0 && quiet == 0) {
        field_retexture_handlers[which](x, y, FIELD_LOOK_CROP);
    }
    tile = field_tile_at(which, x, y);
    if (tile == 0) {
        return;
    }
    tile->growthFlags &= ~FIELD_TILE_STEP_MASK;
    for (i = 0; i < 9; i++) {
        field_neighbour_x = x + field_neighbour_offsets[(i * 2) | 1];
        field_neighbour_y = y + field_neighbour_offsets[i * 2];
        mask = 0;
        for (j = 0; j < 9; j++) {
            nx = field_neighbour_x + field_neighbour_offsets[j * 2 + 1];
            ny = field_neighbour_y + field_neighbour_offsets[j * 2];
            if (nx >= 0 && ny >= 0 && nx < size->cols && ny < size->rows) {
                tile = field_tile_at(which, nx, ny);
                if (tile != 0 && tile->value == FIELD_TILE_GROWN && tile->kind != 0) {
                    mask |= 1 << j;
                }
            }
        }
        field_tile_shape = -1;
        if (field_tile_shape == -1 && (mask & 0x1B8) == 0x1B8) {
            field_tile_shape = 6;
        }
        if (field_tile_shape == -1 && (mask & 0xDB) == 0xDB && (mask & 0x24) != 0x24 && (mask & 0x120) != 0x120) {
            field_tile_shape = 2;
        }
        if (field_tile_shape == -1 && (mask & 0x1BB) == 0x1BB) {
            field_tile_shape = 6;
        }
        if (field_tile_shape == -1 && (mask & 0xFE) == 0xFE) {
            field_tile_shape = 6;
        }
        if (field_tile_shape == -1 && (mask & 0x3F) == 0x3F && (mask & 0xC0) != 0xC0 && (mask & 0x180) != 0x180) {
            field_tile_shape = 4;
        }
        if (field_tile_shape == -1 && (mask & 0x1B6) == 0x1B6 && (mask & 0x9) != 0x9 && (mask & 0x48) != 0x48) {
            field_tile_shape = 6;
        }
        if (field_tile_shape == -1 && (mask & 0x1B) == 0x1B && (mask & 0xC0) != 0xC0 && (mask & 0x24) != 0x24) {
            field_tile_shape = 3;
        }
        if (field_tile_shape == -1 && (mask & 0xD8) == 0xD8 && (mask & 0x3) != 0x3 && (mask & 0x120) != 0x120) {
            field_tile_shape = 1;
        }
        if (field_tile_shape == -1 && (mask & 0x36) == 0x36 && (mask & 0x9) != 0x9 && (mask & 0x180) != 0x180) {
            field_tile_shape = 5;
        }
        if (field_tile_shape == -1 && (mask & 0x1B0) == 0x1B0 && (mask & 0x48) != 0x48 && (mask & 0x6) != 0x6) {
            field_tile_shape = 7;
        }
        if ((mask & 0x10) && merge != 0) {
            count = 0;
            for (k = 0; k < 9; k++) {
                if (mask & (1 << k)) {
                    count++;
                }
            }
            if (field_tile_shape == -1 && count == 2) {
                field_tile_shape = 0;
            }
            if (field_tile_shape == -1 && count == 3) {
                field_tile_shape = 0;
            }
            if (field_tile_shape == -1 && (count >= 4 && count <= 6)) {
                if ((mask & 0x1A0) != 0x1A0) {
                    field_tile_shape = 0;
                }
                if (field_tile_shape == -1 && (mask & 0x26) != 0x1A0) {
                    field_tile_shape = 0;
                }
                if (field_tile_shape == -1 && (mask & 0x4A) != 0x4A) {
                    field_tile_shape = 0;
                }
                if (field_tile_shape == -1 && (mask & 0xB) != 0x1A) {
                    field_tile_shape = 0;
                }
            }
        }
        if (field_tile_shape == -1) {
            continue;
        }
        tile = field_tile_at(which, field_neighbour_x, field_neighbour_y);
        if (tile == 0) {
            continue;
        }
        if (quiet == 0) {
            field_retexture_handlers[which](field_neighbour_x, field_neighbour_y,
                                            field_tile_shape + FIELD_LOOK_CROP + (tile->kind - 1) * 8);
        }
        tile->growthFlags &= ~FIELD_TILE_STEP_MASK;
        tile->growthFlags |= field_tile_shape;
    }
}

/* Submit an item request at tile (x, y) of the farm (mode 0) or the greenhouse. The
 * position is the tile's world point, 400 units a cell; the greenhouse's base offsets
 * differ from the farm's. */
static void field_request_submit_scaled(ItemId id, s16 mode, s16 x, s16 y) {
    VECTOR pos;

    if (mode == 0) {
        pos.vx = x * FIELD_CELL_SIZE + FIELD_ORIGIN_X;
        pos.vy = 0;
        pos.vz = FIELD_ORIGIN_Z - y * FIELD_CELL_SIZE;
    } else {
        pos.vx = x * FIELD_CELL_SIZE - 1400;
        pos.vy = 0;
        pos.vz = 1000 - y * FIELD_CELL_SIZE;
    }
    item_request_submit(id, pos);
}

/* If the cell at (x, y) is a grown tile, mark it ready to harvest and redraw it at its
 * growth level. */
static void field_grown_cell_request(s16 y, s16 x, s32 which) {
    FarmFieldTile *tile;
    FarmEventHandler *handlers;
    s16 look;

    tile = field_tile_at(which, x, y);
    if (tile == 0) return;
    if (tile->value != FIELD_TILE_GROWN) return;
    tile->kind = crop_regrow_stage(tile->value);
    tile->unused5 = 0;
    tile->growthFlags |= FIELD_TILE_SKIP_DAY;
    if (tile->kind != 0) {
        look = (((tile->kind - 1) << 3) | FIELD_LOOK_CROP) + (tile->growthFlags & FIELD_TILE_STEP_MASK);
        handlers = field_retexture_handlers;
        handlers[which](x, y, look);
    }
}

s32 (*const event_request_checks[])(void) = {event_state_step};


/* Search the farm field (mode other than 0) or the greenhouse (mode 0; none when it is not
 * built) for tiles from a random start, wrapping around. Without flags 1, 2 and 4 the
 * value, ground or kind must match the request; each of these flags lets its field
 * match anything. Each match writes its column and row. Flag 8 keeps the search going
 * after a match; without it the first match ends the search. Returns the number of
 * matches. */
s32 field_find_free_tile(s16 *column, s16 *row, s32 mode, FarmFieldTile *request, s32 flags)
{
    s32 index;
    s32 count;
    s32 divisor;
    s32 limit;
    FarmFieldTile *base;
    s32 start;
    s32 anyValue;
    s32 anyGround;
    s32 anyKind;
    s32 allMatches;
    if (mode != 0)
    {
        divisor = FIELD_COLS;
        limit = FIELD_ROWS * FIELD_COLS;
        base = game_state.fieldTiles[0].rows;
    }
    else
    {
        divisor = GREENHOUSE_SIZE;
        if (game_state.progress.estate.greenhouseLevel == 0)
        {
            return 0;
        }
        limit = GREENHOUSE_SIZE * GREENHOUSE_SIZE;
        base = game_state.slots7x7[0].rows;
    }
    start = rand_range(limit);

    index = start;
    count = 0;
    anyValue = flags & 1;
    anyGround = flags & 2;
    anyKind = flags & 4;
    allMatches = flags & 0x8;
    do
    {
        if ((anyValue != 0 || request->value == base[index].value) &&
            (anyGround != 0 || request->ground == base[index].ground) &&
            (anyKind != 0 || request->kind == base[index].kind))
        {
            *column = index % divisor;
            *row = index / divisor;
            count++;
            if (allMatches == 0)
            {
                return count;
            }
        }
        index++;
        if (index >= limit)
        {
            index = 0;
        }
    } while (index != start);
    return count;
}

/* Dry a watered tile's ground: watered soil goes back to tilled (its value too, when the
 * value was the same watered soil), watered sown soil to sown. */
void field_tile_dry(FarmFieldTile *tile)
{
    u16 ground = tile->ground;
    if (ground == ITEM_WATERED_SOIL)
    {
        u16 value = tile->value;
        tile->ground = ITEM_TILLED_SOIL;
        if (value == ground)
            tile->value = ITEM_TILLED_SOIL;
    }
    else if (ground == ITEM_WATERED_SOWN_SOIL)
    {
        tile->ground = ITEM_SOWN_SOIL;
    }
}

/* The tile at (x, y) of the farm field (which 0) or the greenhouse (which 1), or 0 outside
 * its grid. */
FarmFieldTile *field_tile_at(s32 which, s16 x, s16 y)
{
    FarmFieldTile *result = 0;
    FarmFieldTile *table;
    s32 max_x;
    s32 max_y;
    if (which == 0)
    {
        max_y = FIELD_ROWS;
        max_x = FIELD_COLS;
        table = game_state.fieldTiles[0].rows;
    }
    else
    {
        max_y = GREENHOUSE_SIZE;
        max_x = GREENHOUSE_SIZE;
        table = game_state.slots7x7[0].rows;
    }
    if (x >= 0 && x < max_x && y >= 0 && y < max_y)
        result = &table[y * max_x + x];
    return result;
}

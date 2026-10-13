/* The greenhouse: a 7 x 7 grid of cells, each drawn as a quad of the farm's quad table. */
#include "types.h"
#include "farm_probe.h"
#include "farm_state.h"
#include "field.h"
#include "greenhouse.h"
#include "ground.h"
#include "items.h"
#include "model.h"
#include "scene.h"

static s32 greenhouse_column;
static s32 greenhouse_row;
static VECTOR greenhouse_probe_point;

/* The column (0 to 4) of greenhouse item `item`, which lie in rows of five from item 1;
 * -1 when the item is out of range. */
s32 greenhouse_item_column(s16 item) {
    s32 index = item - 1;
    if ((u16)index >= 25) return -1;
    return index % 5;
}

/* The level values for variant 0 and the alternate table for variant 1, by level index. */
static u16 greenhouse_level_values[6] = {
    0, 6000, 18000, 36000, 0xFFFF, 0,
};

static u16 greenhouse_level_alt_values[6] = {
    0, 1000, 2000, 3000, 5000, 31074,
};

s32 greenhouse_level_value(s16 index, s16 variant) {
    if (variant == 0) {
        return greenhouse_level_values[index];
    }
    return greenhouse_level_alt_values[index];
}

/* The ore kind's slot in the level tables: gold 3, silver 2, copper 1, mythril 4, else 0. */
s32 greenhouse_kind_index(s16 kind) {
    if (kind == ITEM_GOLD_ORE) return 3;
    if (kind == ITEM_SILVER_ORE) return 2;
    return kind == ITEM_COPPER_ORE ? 1 : (kind == ITEM_MYSTRILE) * 4;
}

/* Sample a 7x7 grid of points (400 apart) and record every hit whose flags have
 * FARM_QUAD_CELL_BIT set. Restart 0 begins at the first row; otherwise the sweep
 * continues from greenhouse_row. */
s32 greenhouse_probe_sweep(s16 restart) {
    FarmModeGlobal *grid;
    FarmTmdQuad *hit;

    if (restart == 0) {
        greenhouse_probe_point.vx = -1620;
        greenhouse_row = 0;
        greenhouse_probe_point.vy = 0;
        greenhouse_probe_point.vz = 1020;
    }
    while (greenhouse_row < GREENHOUSE_SIZE) {
        grid = &farm_area.mode;
        greenhouse_probe_point.vx = -1420;
        greenhouse_column = 0;
        do {
            farm_settle_on_ground(&greenhouse_probe_point, 0xC000);
            hit = ground_hit_primitive();
            if (hit->header & FARM_QUAD_CELL_BIT) {
                grid->quads[greenhouse_row][greenhouse_column] = hit;
            }
            greenhouse_column++;
            greenhouse_probe_point.vx += 400;
        } while (greenhouse_column < GREENHOUSE_SIZE);
        greenhouse_row++;
        greenhouse_probe_point.vz -= 400;
    }
    return 1;
}

/* Convert a world position to a 7x7 farm-grid cell (400 units per cell) and
 * classify it: 2 = before the grid on x or z (the cell is stepped back by one),
 * 3 = past the grid, 1 = a free greenhouse cell (its quad has no FARM_QUAD_CELL_BIT),
 * 0 = not the greenhouse, or the cell is taken. */
s32 greenhouse_world_to_cell(VECTOR *position, s16 *column, s16 *row) {
    VECTOR copy;
    s16 x;
    s16 z;
    s32 rawZ;
    FarmTmdQuad *hit;

    copy = *position;
    *column = position->vx + 1620;
    rawZ = 1220 - (u16)position->vz;
    *row = rawZ;
    x = *column;
    *column = x / 400;
    *row = *row / 400;
    z = rawZ;
    if (x < 0) {
        *column -= 1;
        return 2;
    }
    if (*column >= GREENHOUSE_SIZE) {
        return 3;
    }
    if (z < 0) {
        *row -= 1;
        return 2;
    }
    if (*row >= GREENHOUSE_SIZE) {
        return 3;
    }
    hit = farm_area.mode.quads[*row][*column];
    if (farm_area.mode.place.location != PLACE_GREENHOUSE || (hit->header & FARM_QUAD_CELL_BIT)) {
        return 0;
    }
    return 1;
}

/* Copy the rectangle's corners a, b, c, d into the quad's four corners. */
#define SET_CORNERS(quad, uv, a, b, c, d) \
    {                                     \
        (quad)->u0 = (uv)->corner[a][0];  \
        (quad)->v0 = (uv)->corner[a][1];  \
        (quad)->u1 = (uv)->corner[b][0];  \
        (quad)->v1 = (uv)->corner[b][1];  \
        (quad)->u2 = (uv)->corner[c][0];  \
        (quad)->v2 = (uv)->corner[c][1];  \
        (quad)->u3 = (uv)->corner[d][0];  \
        (quad)->v3 = (uv)->corner[d][1];  \
    }

/* The UV rectangle and CLUT of each cell category: 0 to 4 are the soil kinds, 5 and up
 * the grown crops (texture page 0x1D). */
static GreenhouseQuadUv greenhouse_quad_uvs[29] = {
    {{{159, 95}, {128, 95}, {159, 64}, {128, 64}}, 0x3CA4},
    {{{191, 95}, {160, 95}, {191, 64}, {160, 64}}, 0x3C64},
    {{{175, 234}, {144, 234}, {175, 203}, {144, 203}}, 0x3E65},
    {{{111, 234}, {80, 234}, {111, 203}, {80, 203}}, 0x3E25},
    {{{143, 234}, {112, 234}, {143, 203}, {112, 203}}, 0x3DE5},
    {{{31, 191}, {31, 160}, {0, 191}, {0, 160}}, 0x6774},
    {{{63, 191}, {63, 160}, {32, 191}, {32, 160}}, 0x6774},
    {{{95, 160}, {95, 191}, {64, 160}, {64, 191}}, 0x6774},
    {{{127, 191}, {127, 160}, {96, 191}, {96, 160}}, 0x6774},
    {{{128, 191}, {159, 160}, {159, 191}, {128, 160}}, 0x6774},
    {{{191, 191}, {160, 160}, {160, 191}, {191, 160}}, 0x6774},
    {{{192, 160}, {223, 191}, {192, 191}, {223, 160}}, 0x6774},
    {{{255, 191}, {255, 160}, {224, 191}, {224, 160}}, 0x6774},
    {{{31, 223}, {31, 192}, {0, 223}, {0, 192}}, 0x67B4},
    {{{63, 223}, {63, 192}, {32, 223}, {32, 192}}, 0x67B4},
    {{{95, 192}, {95, 223}, {64, 192}, {64, 223}}, 0x67B4},
    {{{127, 223}, {127, 192}, {96, 223}, {96, 192}}, 0x67B4},
    {{{128, 223}, {159, 192}, {159, 223}, {128, 192}}, 0x67B4},
    {{{191, 223}, {160, 192}, {160, 223}, {191, 192}}, 0x67B4},
    {{{192, 192}, {223, 223}, {192, 223}, {223, 192}}, 0x67B4},
    {{{255, 223}, {255, 192}, {224, 223}, {224, 192}}, 0x67B4},
    {{{31, 255}, {31, 224}, {0, 255}, {0, 224}}, 0x67F4},
    {{{63, 255}, {63, 224}, {32, 255}, {32, 224}}, 0x67F4},
    {{{95, 224}, {95, 255}, {64, 224}, {64, 255}}, 0x67F4},
    {{{127, 255}, {127, 224}, {96, 255}, {96, 224}}, 0x67F4},
    {{{128, 255}, {159, 224}, {159, 255}, {128, 224}}, 0x67F4},
    {{{191, 255}, {160, 224}, {160, 255}, {191, 224}}, 0x67F4},
    {{{192, 224}, {223, 255}, {192, 255}, {223, 224}}, 0x67F4},
    {{{255, 255}, {255, 224}, {224, 255}, {224, 224}}, 0x67F4},
};

/* Retexture the quad at (column, row) of the farm's quad table with
 * rectangle `index` of greenhouse_quad_uvs, mirrored when row is 0. Does nothing
 * outside the greenhouse; returns 0 when it retextured (or was outside), 1 when the
 * quad is not a textured 0x2D01/0x3D01 kind. */
s32 greenhouse_retexture_quad(s16 column, s16 row, s16 index) {
    FarmTmdQuad *quad;
    GreenhouseQuadUv *uv;
    u32 header;

    if (farm_area.mode.place.location != PLACE_GREENHOUSE) {
        return 0;
    }
    quad = farm_area.mode.quads[row][column];
    header = quad->header;
    if (header & FARM_QUAD_CELL_BIT) {
        uv = &greenhouse_quad_uvs[index];
        if ((header & TMD_PRIM_CODE_MASK) == (TMD_PRIM_GT4_RAW << 16)) {
            if (row != 0) {
                SET_CORNERS(quad, uv, 0, 1, 2, 3);
            } else {
                SET_CORNERS(quad, uv, 3, 2, 1, 0);
            }
            quad->tsb = index >= 5 ? 0x1D : 9;
            quad->cba = uv->clut;
            return 0;
        } else if ((header & TMD_PRIM_CODE_MASK) == (TMD_PRIM_FT4_RAW << 16)) {
            if (row != 0) {
                SET_CORNERS(quad, uv, 0, 1, 2, 3);
            } else {
                SET_CORNERS(quad, uv, 3, 2, 1, 0);
            }
            quad->tsb = index >= 5 ? 0x1D : 9;
            quad->cba = uv->clut;
            return 0;
        }
    }
    return 1;
}

/* Give each cell of the 7x7 grid its texture category (by its ground, or by level and
 * growth step for a grown crop) and retexture its quad. */
void greenhouse_update_cells(void) {
    FarmFieldTile *record;
    u32 row;
    u32 slot;
    s16 category;
    s16 level;
    s16 step;

    record = game_state.slots7x7[0].rows;
    for (row = 0; row < GREENHOUSE_SIZE; row++) {
        for (slot = 0; slot < GREENHOUSE_SIZE; slot++) {
            category = 0;
            switch (record->ground) {
            case ITEM_SOIL:
                category = 0;
                break;
            case ITEM_TILLED_SOIL:
                category = 1;
                break;
            case ITEM_WATERED_SOIL:
                category = 2;
                break;
            case ITEM_SOWN_SOIL:
                category = 3;
                break;
            case ITEM_WATERED_SOWN_SOIL:
                category = 4;
                break;
            }
            if (record->value == FIELD_TILE_GROWN && record->kind != 0) {
                level = record->kind;
                level--;
                category = (level << 3) | 5;
                step = record->growthFlags & FIELD_TILE_STEP_MASK;
                category += step;
            }
            greenhouse_retexture_quad(slot, row, category);
            record++;
        }
    }
}

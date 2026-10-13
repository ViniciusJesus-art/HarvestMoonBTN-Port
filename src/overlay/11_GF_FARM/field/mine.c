/* Mine grid: the cell-digging puzzle in the farm's mine, one layer at a time. */

#include "types.h"
#include "event_flags.h"
#include "farm.h"
#include "farm_probe.h"
#include "farm_state.h"
#include "ground.h"
#include "item_storage.h"
#include "items.h"
#include "main.h"
#include "mine.h"
#include "model.h"
#include "picture_cells.h"
#include "player.h"
#include "scene.h"
#include "screen_fade.h"
#include "shop_dialogues.h"
#include "text.h"

/* State of the field-clearing (mine) minigame: ten layers of a ten-by-ten grid of
 * cell bytes, the current layer, and flags. */
typedef struct FarmClearData {
    s16 layer;
    s16 exitChance;
    s16 savedColumn;
    s16 savedRow;
    s16 column;
    s16 row;
    u8 layerHasExit[10];        /* one per layer: set once its exit is dug */
    u8 grid[10][100];           /* per layer, row-major 10x10 */
    u8 pad3FE[0x400 - 0x3FE];
    s32 flags;
} FarmClearData;

/* FarmClearData.flags. */
enum MineFlag {
    MINE_FLAG_PROBE_PENDING = 0x2000,   /* the layer changed: the ground probe runs again */
    MINE_FLAG_RETURN = 0x4000,          /* a clear cell that leads back out of the mine */
    MINE_FLAG_NEXT_LAYER = 0x8000,      /* a clear cell that descends a layer */
};

#define MINE_GRID_SIZE 10
/* The farm grid: cells of 400 units, the first one starting at (-2000, 2000). */
#define MINE_ORIGIN_X (-2000)
#define MINE_ORIGIN_Z 2000
#define MINE_CELL_SIZE 400
/* Cells this close to the exit above, in each direction, are cleared and never become an exit. */
#define MINE_EXIT_RADIUS 2
#define MINE_RETURN_TICKS 68
#define MINE_RETURN_TICKS_43 72
/* Frames of the screen fade into a layer change, and of the walk back from the exit when the message is closed. */
#define MINE_FADE_FRAMES 10
#define MINE_STEP_BACK_FRAMES 10

/* A cell of a layer's grid: dug, cleared (a way down or back out is marked) or the exit. */
enum MineCell {
    MINE_CELL_EMPTY = 0,
    MINE_CELL_CLEAR = 1,
    MINE_CELL_EXIT = 2,
};

typedef struct {
    VECTOR position;
    s16 column;
    s16 row;
} FarmPointData;

static s32 mine_cell_hit(s32 alternate, s32 layer, s16 column, s16 row);
static s32 mine_retexture_cell(s16 col, s16 row, s16 kind);
static s32 mine_world_to_cell(VECTOR *position, s16 *column, s16 *row);
static s32 mine_roll_event(s16 place);
static void mine_place_model(void);

static s32 mine_column;
static s32 mine_row;
static VECTOR mine_probe_point;
static FarmClearData mine_clear_state;

/* Clear every layer's cell grid and exit flag, and reset the layer and the flags. */
void mine_clear_grid(void) {
    u32 outer;
    u32 inner;
    u32 cell;

    outer = 0;
    do {
        mine_clear_state.layerHasExit[outer] = 0;
        inner = 0;
        do {
            cell = 0;
            do {
                mine_clear_state.grid[outer][inner * MINE_GRID_SIZE + cell] = MINE_CELL_EMPTY;
                cell++;
            } while (cell < MINE_GRID_SIZE);
            inner++;
        } while (inner < MINE_GRID_SIZE);
        outer++;
    } while (outer < MINE_GRID_SIZE);
    mine_clear_state.flags = 0;
    mine_clear_state.layer = 0;
}

/* Stepping onto an exit cell starts a transition: the exit of this layer goes down,
 * the exit of the layer above goes back out. */
s32 mine_check_player(FarmPlayer *player) {
    FarmPointData point;
    s16 remaining;
    if (farm_area.mode.place.value == PLACE_MINE) {
        event_flag_set(EVENT_FLAG_MINE_VISITED, 1);
    }
    if (farm_area.mode.place.location == PLACE_48) {
        event_flag_set(EVENT_FLAG_PLACE_48_VISITED, 1);
    }
    if ((player->stateFlags & PLAYER_FLAG_AUTO_WALK) == 0) {
        if ((mine_clear_state.flags & (MINE_FLAG_PROBE_PENDING | MINE_FLAG_RETURN | MINE_FLAG_NEXT_LAYER)) == 0) {
            point.position.vx = player->position.vx;
            point.position.vy = player->position.vy;
            point.position.vz = player->position.vz;
            if (mine_world_to_cell(&point.position, &point.column, &point.row) == 0) {
                remaining = mine_clear_state.layer;
                if (remaining != 0) {
                    if (mine_cell_hit(0, remaining - 1, point.column, point.row) != 0) {
                        return 1;
                    }
                }
                if (mine_cell_hit(1, mine_clear_state.layer, point.column, point.row) != 0) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

/* Exit check for one cell of layer `layer`'s grid: when the cell is the exit, pause
 * the game and set the transition flag (MINE_FLAG_NEXT_LAYER when `alternate`, else
 * MINE_FLAG_RETURN). A return also updates the message window. Returns 1 on a hit. */
static s32 mine_cell_hit(s32 alternate, s32 layer, s16 column, s16 row) {
    if (mine_clear_state.grid[layer][row + column * MINE_GRID_SIZE] == MINE_CELL_EXIT) {
        game_state.globalFlags |= (GLOBAL_FLAG_PAUSED | GLOBAL_FLAG_MENU_OPEN);
        mine_clear_state.flags |= alternate ? MINE_FLAG_NEXT_LAYER : MINE_FLAG_RETURN;
        event_flag_set(EVENT_FLAG_EVENT_ACTIVE, 1);
        if (alternate == 0) {
            shop_result_update(&current_frame->otTags[0][15], -1, 0);
        }
        player_reset_pose();
        return 1;
    }
    return 0;
}

/* Sweep a grid of probe points (400 apart) and record every ground quad
 * that has the cell bit set. Resumes from mine_row, so it can run over several frames. */
s32 mine_probe_grid(s16 restart) {
    FarmModeGlobal *grid;
    FarmTmdQuad *hit;

    mine_clear_state.exitChance = MINE_GRID_SIZE;
    mine_clear_state.layerHasExit[9] = 1;
    if (restart == 0) {
        mine_probe_point.vx = MINE_ORIGIN_X;
        mine_row = 0;
        mine_probe_point.vy = 0;
        mine_probe_point.vz = MINE_ORIGIN_Z - MINE_CELL_SIZE / 2;
    }
    while (mine_row < MINE_GRID_SIZE) {
        grid = &farm_area.mode;
        mine_probe_point.vx = MINE_ORIGIN_X + MINE_CELL_SIZE / 2;
        mine_column = 0;
        do {
            farm_settle_on_ground(&mine_probe_point, 0xC000);
            hit = ground_hit_primitive();
            if (hit->header & FARM_QUAD_CELL_BIT) {
                grid->quads[mine_row][mine_column] = hit;
            }
            mine_column++;
            mine_probe_point.vx += MINE_CELL_SIZE;
        } while (mine_column < MINE_GRID_SIZE);
        mine_row++;
        mine_probe_point.vz -= MINE_CELL_SIZE;
    }
    mine_clear_state.flags &= ~MINE_FLAG_PROBE_PENDING;
    return 1;
}

/* Walk the current layer's grid and hand every cell marked 1 or 2 to
 * mine_retexture_cell. */
static void mine_retexture_grid(void) {
    u8 *cell;
    u32 row;
    u32 col;

    cell = mine_clear_state.grid[mine_clear_state.layer];
    row = 0;
    do {
        col = 0;
        do {
            if (*cell == MINE_CELL_CLEAR) {
                mine_retexture_cell(col, row, MINE_CELL_CLEAR);
            }
            if (*cell == MINE_CELL_EXIT) {
                mine_retexture_cell(col, row, MINE_CELL_EXIT);
            }
            col++;
            cell++;
        } while (col < MINE_GRID_SIZE);
        row++;
    } while (row < MINE_GRID_SIZE);
}

static inline void mine_retexture_prim(FarmTmdQuad *prim, FarmCellTexture *texture) {
    prim->u0 = texture->uv[0];
    prim->v0 = texture->uv[1];
    prim->u1 = texture->uv[2];
    prim->v1 = texture->uv[3];
    prim->u2 = texture->uv[4];
    prim->v2 = texture->uv[5];
    prim->u3 = texture->uv[6];
    prim->v3 = texture->uv[7];
    prim->cba = texture->clut;
}

static s16 mine_layer_weights[10][8] = {
    {40, 30, 20, 10, 0, 0, 0, 0},
    {40, 25, 20, 10, 5, 0, 0, 0},
    {40, 20, 20, 15, 5, 0, 0, 0},
    {30, 20, 20, 15, 10, 5, 0, 0},
    {30, 15, 15, 15, 10, 5, 5, 5},
    {20, 10, 15, 20, 10, 15, 5, 5},
    {15, 10, 15, 20, 15, 10, 10, 5},
    {10, 5, 10, 25, 20, 15, 10, 5},
    {5, 5, 15, 25, 20, 15, 10, 5},
    {0, 0, 10, 30, 25, 20, 10, 5},
};

static s16 mine_layer_weights_mine[10][8] = {
    {40, 30, 20, 10, 0, 0, 0, 0},
    {40, 20, 20, 10, 5, 5, 0, 0},
    {35, 20, 20, 15, 5, 5, 0, 0},
    {25, 20, 20, 15, 10, 10, 0, 0},
    {25, 15, 20, 15, 10, 10, 5, 0},
    {20, 10, 20, 15, 15, 15, 5, 0},
    {15, 10, 20, 20, 15, 15, 5, 0},
    {10, 5, 15, 25, 20, 20, 5, 0},
    {0, 0, 0, 0, 25, 25, 0, 50},
    {100, 0, 10, 30, 25, 20, 10, 5},
};

static s16 mine_layer_events[8] = {
    0x0000, ITEM_COIN, ITEM_JUNK_ORE, ITEM_COPPER_ORE,
    ITEM_SILVER_ORE, ITEM_GOLD_ORE, ITEM_MYSTRILE, ITEM_POWER_BERRY,
};

static s16 mine_layer_events_mine[8] = {
    0x0000, ITEM_JUNK_ORE, ITEM_COIN, ITEM_MYSTRILE,
    ITEM_ORICHALCUM, ITEM_ADAMANTITE, ITEM_POWER_BERRY, ITEM_KETCHUP_RECIPE,
};

/* Byte offsets into mine_messages of the two messages: the way down, and the climb back out. */
enum MineMessage {
    MINE_MSG_NEXT_LAYER = 0,
    MINE_MSG_RETURN = 0x34,
};

static unsigned short mine_messages[68] = {
    0x0013, 0x0027, 0x0024, 0x0031, 0x0024, TEXT_CODE_SPACE, 0x0028, 0x0032, TEXT_CODE_SPACE, 0x0020, TEXT_CODE_SPACE, 0x0027,
    0x002E, 0x002B, 0x0024, 0x006C, TEXT_LINE_BREAK, 0x0006, 0x002E, TEXT_CODE_SPACE, 0x0023, 0x002E, 0x0036, 0x002D,
    0x0062, TEXT_END, 0x0002, 0x002B, 0x0028, 0x002C, 0x0021, TEXT_CODE_SPACE, 0x0033, 0x0027, 0x0024, TEXT_CODE_SPACE,
    0x002B, 0x0020, 0x0023, 0x0023, 0x0024, 0x0031, TEXT_CODE_SPACE, 0x0033, 0x002E, TEXT_CODE_SPACE, 0x0026, 0x0024,
    0x0033, TEXT_CODE_SPACE, 0x002E, 0x0034, 0x0033, TEXT_CODE_SPACE, 0x002E, 0x0025, TEXT_LINE_BREAK, 0x0033, 0x0027, 0x0024,
    TEXT_CODE_SPACE, 0x002C, 0x0028, 0x002D, 0x0024, 0x0062, TEXT_END, 0x0000,
};

static FarmCellTexture mine_cell_textures[3] = {
    {{255, 31, 255, 0, 27, 31, 27, 0}, 0x3E64},
    {{131, 31, 131, 0, 159, 31, 159, 0}, 0x3DE4},
    {{195, 31, 195, 0, 223, 31, 223, 0}, 0x3D64},
};

/* Retexture the grid cell at (col, row) with texture `kind`, if its primitive
 * is one of the two textured quad kinds. Returns 0 when it was changed, 1 when not. */
static s32 mine_retexture_cell(s16 col, s16 row, s16 kind) {
    FarmTmdQuad *prim;
    u32 header;
    FarmCellTexture *texture;

    prim = farm_area.mode.quads[row][col];
    header = prim->header;
    if (header & FARM_QUAD_CELL_BIT) {
        texture = &mine_cell_textures[kind];
        if ((header & TMD_PRIM_CODE_MASK) == (TMD_PRIM_GT4_RAW << 16)) {
            mine_retexture_prim(prim, texture);
            return 0;
        } else if ((header & TMD_PRIM_CODE_MASK) == (TMD_PRIM_FT4_RAW << 16)) {
            mine_retexture_prim(prim, texture);
            return 0;
        }
    }
    return 1;
}

/* Steps for the eight facings: 0..3 cross a cell (400 units), 4..7 step 300 units. */
#define FACING_STEP_LONG 400
#define FACING_STEP_SHORT 300

FarmStep facing_steps[8] = {
    {0, -FACING_STEP_LONG},
    {-FACING_STEP_LONG, 0},
    {0, FACING_STEP_LONG},
    {FACING_STEP_LONG, 0},
    {0, -FACING_STEP_SHORT},
    {-FACING_STEP_SHORT, 0},
    {0, FACING_STEP_SHORT},
    {FACING_STEP_SHORT, 0},
};

/* Dig the cell in front of the player. An edge cell, or one beside the previous layer's
 * exit (unless that cell is the exit itself), is cleared. Otherwise, while this layer has
 * no exit, a roll under exitChance can make the cell the exit. Failing that the cell is
 * cleared and mine_roll_event may give an item: returns 1 when one is shown, else 0. */
s32 mine_dig(FarmPlayer *player) {
    FarmPointData point;
    FarmStoredItem event;
    FarmStep *step;
    s32 layer;
    s32 exitRow;
    s32 exitColumn;

    step = &facing_steps[player->facing38];
    point.position.vy = player->position.vy;
    point.position.vx = player->position.vx + step->x;
    point.position.vz = player->position.vz + step->z;
    if (mine_world_to_cell(&point.position, &point.column, &point.row) != 0) {
        return 0;
    }
    if (mine_clear_state.grid[mine_clear_state.layer][point.row + point.column * MINE_GRID_SIZE] == MINE_CELL_EMPTY) {
        if (point.column == 0 || point.row == 0 || point.column == MINE_GRID_SIZE - 1 || point.row == MINE_GRID_SIZE - 1) {
            mine_retexture_cell(point.column, point.row, MINE_CELL_CLEAR);
            mine_clear_state.grid[mine_clear_state.layer][point.row + point.column * MINE_GRID_SIZE] = MINE_CELL_CLEAR;
            return 0;
        }
        layer = mine_clear_state.layer;
        if (layer != 0) {
            if (mine_clear_state.grid[layer - 1][point.row + point.column * MINE_GRID_SIZE] == MINE_CELL_EXIT) {
                return 0;
            }
            exitColumn = mine_clear_state.savedColumn;
            if (point.column >= exitColumn - MINE_EXIT_RADIUS && point.column <= exitColumn + MINE_EXIT_RADIUS) {
                exitRow = mine_clear_state.savedRow;
                if (point.row >= exitRow - MINE_EXIT_RADIUS && point.row <= exitRow + MINE_EXIT_RADIUS) {
                    mine_retexture_cell(point.column, point.row, MINE_CELL_CLEAR);
                    mine_clear_state.grid[mine_clear_state.layer][point.row + point.column * MINE_GRID_SIZE] = MINE_CELL_CLEAR;
                    return 0;
                }
            }
        }
        if (mine_clear_state.exitChance > rand_range(100) && mine_clear_state.layerHasExit[mine_clear_state.layer] == 0) {
            mine_clear_state.grid[mine_clear_state.layer][point.row + point.column * MINE_GRID_SIZE] = MINE_CELL_EXIT;
            mine_clear_state.layerHasExit[mine_clear_state.layer] = 1;
            mine_retexture_cell(point.column, point.row, MINE_CELL_EXIT);
            mine_clear_state.column = point.column;
            mine_clear_state.row = point.row;
        } else {
            mine_retexture_cell(point.column, point.row, MINE_CELL_CLEAR);
            mine_clear_state.grid[mine_clear_state.layer][point.row + point.column * MINE_GRID_SIZE] = MINE_CELL_CLEAR;
            event.count = 1;
            event.modifier = 0;
            event.id = mine_roll_event(farm_area.mode.place.value);
            if (event.id >= ITEM_FISHING_MARKER) {
                event.id = ITEM_FOX;
            }
            if (event.id < 0) {
                event.id = ITEM_MONKEY;
            }
            if (event.id != 0) {
                player_show_item(&event);
                mine_clear_state.exitChance += 2;
                return 1;
            }
            mine_clear_state.exitChance++;
        }
    }
    return 0;
}

/* Convert a world position to a farm-grid cell (400 units per cell,
 * origin at -2000 on x and +2000 on z) and classify it: 2 = before the grid
 * (the cell is stepped back by one), 3 = beyond it, otherwise whether the
 * cell's primitive has bit 3 clear. */
static s32 mine_world_to_cell(VECTOR *position, s16 *column, s16 *row) {
VECTOR copy = *position;
    s16 x;
    s16 z;
    u32 mask;

    *column = position->vx - MINE_ORIGIN_X;
    *row = MINE_ORIGIN_Z - position->vz;
    x = *column;
    z = *row;
    *column /= MINE_CELL_SIZE;
    *row /= MINE_CELL_SIZE;
    if (x < 0) {
        (*column)--;
        return 2;
    }
    if (*column < MINE_GRID_SIZE) {
        if (z < 0) {
            (*row)--;
            return 2;
        }
        if (*row < MINE_GRID_SIZE) {
            mask = FARM_QUAD_CELL_BIT;
            return (farm_area.mode.quads[*row][*column]->header & mask) == 0;
        }
    }
    return 3;
}

/* Run the transition message. When it finishes (1), descend a layer or climb out, then
 * clear the flags; when it is closed early (2), walk the player back from the exit. */
void mine_update_transition(void) {
    s32 flags;
    s32 messageOffset;
    s32 result;
    flags = mine_clear_state.flags;
    if ((flags & (MINE_FLAG_RETURN | MINE_FLAG_NEXT_LAYER)) == 0) return;
    messageOffset = MINE_MSG_NEXT_LAYER;
    if (flags & MINE_FLAG_RETURN) messageOffset = MINE_MSG_RETURN;
    result = shop_result_update(&current_frame->otTags[0][15], 32, 32);
    text_draw_in_frame(&current_frame->otTags[0][15], &current_frame->otTags[0][10], &mine_messages[messageOffset / 2]);
    if (result == 1) {
        farm_area.mode.eventMode = FARM_MODE_EXIT_ARRIVE;
        fade_start_blend(MINE_FADE_FRAMES);
        if (mine_clear_state.flags & MINE_FLAG_NEXT_LAYER) {
            farm_area.mode.transitionTicks++;
            mine_clear_state.layer++;
            event_flag_set(EVENT_FLAG_MINE_NEXT_LAYER, 1);
            mine_clear_state.flags |= MINE_FLAG_PROBE_PENDING;
        } else {
            if (farm_area.mode.place.value == PLACE_MINE) {
                farm_area.mode.transitionTicks = MINE_RETURN_TICKS;
                event_flag_set(EVENT_FLAG_MINE_RETURN_37, 1);
            }
            if (farm_area.mode.place.value == PLACE_MINE_43) {
                farm_area.mode.transitionTicks = MINE_RETURN_TICKS_43;
                event_flag_set(EVENT_FLAG_MINE_RETURN_43, 1);
            }
            mine_clear_grid();
        }
        mine_clear_state.flags &= ~(MINE_FLAG_RETURN | MINE_FLAG_NEXT_LAYER);
        game_state.globalFlags &= ~GLOBAL_FLAG_MENU_OPEN;
        game_state.globalFlags &= ~GLOBAL_FLAG_PAUSED;
        event_flag_set(EVENT_FLAG_EVENT_ACTIVE, 0);
    } else if (result == 2) {
        mine_clear_state.flags &= ~(MINE_FLAG_RETURN | MINE_FLAG_NEXT_LAYER);
        game_state.globalFlags &= ~GLOBAL_FLAG_MENU_OPEN;
        game_state.globalFlags &= ~GLOBAL_FLAG_PAUSED;
        event_flag_set(EVENT_FLAG_EVENT_ACTIVE, 0);
        player_walk_start(MINE_STEP_BACK_FRAMES);
    }
}

/* Pick the event a dug cell gives: a weighted roll over the layer's table. The power
 * berry, and the ketchup recipe in the mine, are given once: their event flag is set when rolled. */
static s32 mine_roll_event(s16 place) {
    s16 *values;
    const s16 *events;
    s16 event;
    s32 limit;
    s32 total;
    u32 slot;
    values = mine_layer_weights[mine_clear_state.layer];
    events = mine_layer_events;
    if (place == PLACE_MINE) {
        values = mine_layer_weights_mine[mine_clear_state.layer];
        events = mine_layer_events_mine;
    }
    total = 0;
    limit = rand_range(100);
    slot = 0;
    do {
        total += *values;
        if (limit < total) {
            if (place == PLACE_MINE) {
                if (*events == ITEM_POWER_BERRY) {
                    event = 0;
                    if (event_flag_get(EVENT_FLAG_MINE_37_EVENT_67_ROLLED) != 0) return event;
                    event_flag_set(EVENT_FLAG_MINE_37_EVENT_67_ROLLED, 1);
                }
                if (*events == ITEM_KETCHUP_RECIPE) {
                    event = 0;
                    if (event_flag_get(EVENT_FLAG_MINE_37_EVENT_63_ROLLED) != 0) return event;
                    event_flag_set(EVENT_FLAG_MINE_37_EVENT_63_ROLLED, 1);
                }
                return *events;
            }
            event = *events;
            if (event == ITEM_POWER_BERRY) {
                event = 0;
                if (event_flag_get(EVENT_FLAG_MINE_EVENT_67_ROLLED) != 0) return event;
                event_flag_set(EVENT_FLAG_MINE_EVENT_67_ROLLED, 1);
                return *events;
            }
            return event;
        }
        events++;
        slot++;
        values++;
    } while (slot < 8);
    return 0;
}

/* After a descent, put the player two cells past the dug exit of the new layer, then place
 * its exit model. */
void mine_next_layer_check(void) {
    s32 placeModel;
    s32 nextLayer;
    placeModel = game_state.player.place.location == PLACE_42;
    nextLayer = event_flag_get(EVENT_FLAG_MINE_NEXT_LAYER);
    if (nextLayer == 1) {
        game_state.player.position.vy = 0;
        game_state.player.position.vx = (mine_clear_state.column * MINE_CELL_SIZE) + MINE_ORIGIN_X + MINE_CELL_SIZE / 2;
        game_state.player.position.vz = MINE_ORIGIN_Z - ((mine_clear_state.row + 2) * MINE_CELL_SIZE);
        event_flag_set(EVENT_FLAG_MINE_NEXT_LAYER, 0);
        placeModel = 1;
    }
    if (placeModel != 0) {
        mine_place_model();
    }
}

/* Move the exit model to the layer's last dug exit, and save that exit's cell as the
 * saved column and row that mine_dig keeps clear below it. Layer 0 has no exit model. */
static void mine_place_model(void) {
    s16 column;
    s16 row;

    if (mine_clear_state.layer == 0) return;

    column = mine_clear_state.column;
    row = mine_clear_state.row;
    farm_area.mode.script.models[0].flags &= ~SCENE_ENTRY_HIDDEN;
    farm_area.mode.script.models[0].position.vy = 0;
    farm_area.mode.script.models[0].position.vx = MINE_CELL_SIZE * column + MINE_ORIGIN_X;
    farm_area.mode.script.models[0].position.vz = MINE_ORIGIN_Z - MINE_CELL_SIZE / 2 - MINE_CELL_SIZE * row;
    mine_clear_state.savedColumn = mine_clear_state.column;
    mine_clear_state.savedRow = mine_clear_state.row;
}

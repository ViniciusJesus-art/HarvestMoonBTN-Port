/* Farm queries: the ground under an actor, the zones, the tile and the villager or
 * record at a point, the frame's callback count and the status cache. */

#include <memory.h>
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "types.h"
#include "actor.h"
#include "animal_actions.h"
#include "chicken.h"
#include "clock.h"
#include "estate.h"
#include "event_commands.h"
#include "farm.h"
#include "farm_load.h"
#include "farm_main.h"
#include "farm_probe.h"
#include "farm_state.h"
#include "farm_tables.h"
#include "field.h"
#include "game_types.h"
#include "greenhouse.h"
#include "ground.h"
#include "harvest_sprites.h"
#include "items.h"
#include "livestock.h"
#include "mine.h"
#include "model.h"
#include "picture_cells.h"
#include "player.h"
#include "scene.h"
#include "villager.h"

/* A placement box of the location pick: a spot inside its x and z bounds (exclusive) on
 * the place location `kind` is refused. */
typedef struct FarmChoice {
    s32 xMin;
    s32 zMin;
    s32 xMax;
    s32 zMax;
    u16 kind;
} FarmChoice;

typedef struct {
    u16 id;
    u8 pad02[0x2E];
    s16 x30;
    u8 pad32[2];
    u32 flags;
    s16 x38;
    u8 pad3A[6];
    s16 z40;
} FarmObject;

typedef struct {
    s32 flags;
} GroundHitState;
/* farm_tile_classify's result: a tile is passable (1) or blocked (2); 0 is no tile.
 * farm_probe_tile returns the same codes. */
enum TileClass {
    TILE_CLASS_PASSABLE = 1,
    TILE_CLASS_BLOCKED = 2,
};

/* The player's box for a tile or point query: half its width and depth on x and z, half its height on y. */
enum PlayerBoxHalf {
    PLAYER_BOX_HALF_XZ = 218,
    PLAYER_BOX_HALF_Y = 20,
};

/* The box a point search covers around its point: half its width and depth on x and z,
 * half its height on y. */
enum SearchBoxHalf {
    SEARCH_BOX_HALF_XZ = 150,
    SEARCH_BOX_HALF_Y = 20,
};

/* The box farm_villager_find_box tests a villager against, around the query point. */
enum VillagerBoxHalf {
    VILLAGER_BOX_HALF_XZ = 200,
    VILLAGER_BOX_HALF_Y = 20,
};

/* The hitFlags a point query carries for each object class; the callee writes them to
 * its flags on a hit. */
enum FarmHitClass {
    FARM_HIT_PLAYER = 0x1,
    FARM_HIT_CHOICE = 0x10,
    FARM_HIT_TILE = 0x20,
    FARM_HIT_VILLAGER = 0x80,
    FARM_HIT_HORSE = 0x200,
    FARM_HIT_HELPER = 0x400,
    FARM_HIT_CHICKEN = 0x2008,
    FARM_HIT_DOG = 0x2100,
    FARM_HIT_ACTION = 0x2800,
    FARM_HIT_CATTLE = 0x14002,
    FARM_HIT_SHEEP = 0x14004,
};

static s32 farm_tile_classify(FarmTileQuery *query, s32 unused);
static s32 farm_villager_find_box(FarmTileQuery *query);
static s32 farm_tile_state(s16 column, s16 row, s32 mode);
static s32 farm_tile_is_special(FarmTileQuery *query);

/* Cached status record and its low 16 flag bits. */
static u32 *farm_cached_status;
static u32 farm_cached_flags;
static u32 farm_callbacks_queued;
/* The farm callbacks a frame may queue before farm_callback_capped gives up. */
#define FARM_CALLBACK_CAP 50
Model farm_model;

/* Settle `position` on the ground. On the farm, mode 0xC080 first runs an occupant
 * check, and with no occupant the height is set to 0 (a negative height fails). Modes
 * 0xC080 and 0xE080 then run a second check; failing those, the ground lookup sets the
 * height. Returns 1 on failure. */
s32 farm_settle_on_ground(VECTOR *position, s32 mode) {
    void *first0;
    s32 first1;
    void *second0;
    s32 second1;
    s32 found;
    s32 height;

    if (mode == 0xC080 && farm_area.mode.place.location == PLACE_FARM &&
        farm_callback_count(position, farm_area.mode.place, 0, 0, &first0, &first1,
                      0, OCCUPANT_CHOICE) == 0) {
        if (position->vy < 0) {
            return 1;
        }
        position->vy = 0;
        return 0;
    }
    if (mode == 0xC080 || mode == 0xE080) {
        if (farm_callback_count(position, farm_area.mode.place, 0, 0, &second0,
                          &second1, 0, OCCUPANT_CHOICE | OCCUPANT_FLAGGED_CHOICES) == 0 &&
            position->vy >= 0) {
            position->vy = 0;
            return 0;
        }
    }
    ground_set_skip_mask(mode);
    found = ground_find_object(&farm_model, position);
    height = ground_height_at();
    if (found != 0) {
        position->vy = height;
        return 0;
    }
    return 1;
}

static int farm_main_stub_1(void) {
    return -1;
}

/* Find the first live zone whose x/z box strictly contains `pos`; dead and empty zones
 * are skipped. Trigger zones (0x100) fire their mode and marker zones (0x80) set the
 * player's marker when `interact` is set, and the search goes on past both. Any other
 * zone is the result, and it clears the loaded slot unless its id is ZONE_ID_NO_RESET. */
FarmZone *farm_zone_find(FarmModeScript *data, VECTOR *pos, s32 interact)
{
    u32 i;
    ZonePoint min;
    ZonePoint max;
    u16 flags;
    u16 id;

    for (i = 0; i < FARM_ZONE_COUNT; i++) {
        id = data->zones[i].id;
        flags = data->zones[i].flags;
        if (id != ZONE_ID_EMPTY && !(flags & ZONE_FLAG_DEAD)) {
            min = data->zones[i].min;
            max = data->zones[i].max;
            if (min.x < pos->vx && pos->vx < max.x &&
                min.z < pos->vz && pos->vz < max.z) {
                if (flags & ZONE_FLAG_TRIGGER) {
                    if (interact) {
                        event_trigger_mode_set(7);
                    }
                } else if (flags & ZONE_FLAG_MARKER) {
                    if (interact) {
                        game_state.player.latches |= PLAYER_LATCH_MARKER;
                    }
                } else {
                    if (id != ZONE_ID_NO_RESET) {
                        farm_clear_loaded_slot();
                    }
                    return &data->zones[i];
                }
            }
        }
    }
    return 0;
}

/* Find the first active zone whose box contains the point. The y test has
 * slack: 128 below and 120 above. */
FarmZone *farm_zone_at_probe(FarmModeScript *data, ZonePoint *probe) {
    u32 slot;
    ZonePoint min;
    ZonePoint max;

    for (slot = 0; slot < FARM_ZONE_COUNT; slot++) {
        if ((data->zones[slot].flags & ZONE_FLAG_ACTIVE) != 0) {
            min = data->zones[slot].min;
            max = data->zones[slot].max;
            if (min.x < probe->x && probe->x < max.x) {
                if (min.z < probe->z && probe->z < max.z) {
                    if (min.y - 128 < probe->y && probe->y < max.y + 120) {
                        return &data->zones[slot];
                    }
                }
            }
        }
    }
    return 0;
}

/* How far farm_probe_tile steps when the tile in front is blocked. */
#define FARM_PROBE_STEP 400

/* Check the tile at `pos`. A tile of kind 0x2C faced right is always passable. When
 * the tile is blocked (2) and `flag` is 0, step FARM_PROBE_STEP units in `mode`'s
 * direction and check again with `flag` 1. Returns 1 when passable, 2 when blocked. */
s32 farm_probe_tile(VECTOR pos, LimitPair place, s16 mode, s32 flag) {
    FarmTileQuery query;
    s16 kind;
    s16 aux;
    s32 scratch;
    void *tile;
    s32 flags;
    s32 result;

    tile = &scratch;
    query.pos = &pos;
    query.place = &place;
    query.tile = &tile;
    query.flags = &flags;
    query.current = 0;
    query.mask = 0;
    field_world_to_cell(&pos, &kind, &aux);
    if (kind == 0x2C && mode == FACING_RIGHT) {
        return 1;
    }
    result = farm_tile_classify(&query, 0);
    if (result == TILE_CLASS_BLOCKED) {
        if (flag == 0) {
            switch (mode) {
            case FACING_DOWN:
                pos.vz -= FARM_PROBE_STEP;
                break;
            case FACING_LEFT:
                pos.vx -= FARM_PROBE_STEP;
                break;
            case FACING_UP:
                pos.vz += FARM_PROBE_STEP;
                break;
            case FACING_RIGHT:
                pos.vx += FARM_PROBE_STEP;
                break;
            }
            if (farm_probe_tile(pos, place, mode, 1) != 0) {
                result = TILE_CLASS_PASSABLE;
                goto done;
            }
        }
        return TILE_CLASS_BLOCKED;
    }
done:
    return result;
}

static FarmChoice farm_choices_default[25] = {
    {-4500, -9300, 13500, 2700, 0x0001}, {-400, 2600, 1453, 2800, 0x0001},
    {953, 2600, 1453, 3200, 0x0001}, {-5292, -9000, -4400, 950, 0x0001},
    {-6400, -2500, -5100, 950, 0x0001}, {-6400, 700, -5450, 1850, 0x0001},
    {-6500, 0, -6400, 1200, 0x0001}, {-6800, 900, -6300, 1200, 0x0001},
    {-6100, 1850, -4400, 2600, 0x0001}, {-4500, 2600, -2800, 5500, 0x0001},
    {3700, 2500, 5900, 3200, 0x0001}, {4400, 3100, 4800, 3700, 0x0001},
    {5900, 2600, 13500, 5400, 0x0001}, {-1900, -1900, 1750, 1500, 0x0006},
    {1750, -1300, 2300, 750, 0x0006}, {2300, -400, 2500, 400, 0x0006},
    {-5800, -7400, -5200, -6200, 0x0001}, {-2900, 0, -1600, 2900, 0x0030},
    {-1800, -3200, 3150, -2150, 0x0008}, {-700, -2300, 2850, -1300, 0x0008},
    {-700, -1400, 1750, 2300, 0x0008}, {-250, 2200, 1750, 2600, 0x0008},
    {300, -600, 500, 700, 0x0003}, {-500, -1300, 100, -100, 0x0003},
    {-500, -600, 500, -100, 0x0003},
};

static FarmChoice farm_choices_barn0[7] = {
    {1200, -1900, 2000, 2050, 0x0005}, {1000, -2650, 2500, -1900, 0x0005},
    {300, -1950, 3000, -1800, 0x0005}, {400, 1800, 1350, 2200, 0x0005},
    {2200, 1800, 2850, 2200, 0x0005}, {500, 1800, 2800, 2050, 0x0005},
    {1300, -2900, 1900, -2600, 0x0005},
};

static FarmChoice farm_choices_barn1[9] = {
    {1200, -2650, 2000, 2300, 0x0005}, {-2000, -2650, -1200, 2300, 0x0005},
    {250, -2650, 2500, -1800, 0x0005}, {-2150, -2650, -650, -1800, 0x0005},
    {-2900, -1950, 2900, -1800, 0x0005}, {-2900, 1750, 3000, 2300, 0x0005},
    {-200, -1800, 100, 1800, 0x0005}, {-650, -2000, 250, -1800, 0x0005},
    {1300, -2900, 1900, -2650, 0x0005},
};

static FarmChoice farm_choices_coop0[5] = {
    {-1350, -2300, 1100, -1600, 0x0004}, {-1200, -1600, 1700, -500, 0x0004},
    {1700, -1500, 1900, -900, 0x0004}, {-1550, -700, -1300, -250, 0x0004},
    {1100, -1600, 1700, -250, 0x0004},
};

static FarmChoice farm_choices_coop1[10] = {
    {-1350, -2300, 1100, -1600, 0x0004}, {-1200, -1600, 1700, -500, 0x0004},
    {1700, -1550, 1900, -900, 0x0004}, {-1350, -700, -1300, -200, 0x0004},
    {-1350, -700, -1200, -500, 0x0004}, {1200, -700, 1700, 0, 0x0004},
    {-1350, -200, -1300, 500, 0x0004}, {-1350, 300, 1700, 800, 0x0004},
    {-1200, 800, 1700, 1900, 0x0004}, {1200, 0, 1700, 2100, 0x0004},
};

static FarmChoice farm_choices_flagged[4] = {
    {-3100, -970, 2500, -50, 0x0014}, {-5000, -970, 6000, -50, 0x001D},
    {-340, -100, 320, 1150, 0x001D}, {3200, -4800, 3570, 3700, 0x0030},
};

static FarmZoneRow farm_zone_rows[4] = {
    {{{953, 3000, 1453, 3400, 0x0001, 0x0005, 1534, -2000}, {1200, -3000, 2000, -2800, 0x0005, 0x0001, 1240, 2800}}},
    {{{4400, 3300, 4800, 3700, 0x0001, 0x0003, -173, -400}, {-600, -1320, 200, -1200, 0x0003, 0x0001, 4550, 3200}}},
    {{{-7000, 900, -6500, 1200, 0x0001, 0x0006, 1900, 0}, {2400, 300, 2500, 300, 0x0006, 0x0001, -6000, 900}}},
    {{{-5900, -7100, -5700, -6800, 0x0001, 0x0004, 1300, -1255}, {1800, -1550, 1900, -800, 0x0004, 0x0001, -5400, -7100}}},
};

/* Find the first zone of `row` whose kind matches and whose open x/z rectangle
 * contains `pos`; copy its results out and return 1, or return 0. */
s32 farm_zone_row_lookup(VECTOR pos, s16 kind, s32 *result, u16 *result_kind, u32 row)
{
    FarmZoneBox *zone;
    u32 i;

    if (row >= 4) {
        row = 0;
    }
    zone = farm_zone_rows[row].zones;
    for (i = 0; i < 2; i++, zone++) {
        if (zone->kind == kind && zone->x_min < pos.vx && pos.vx < zone->x_max
            && zone->z_min < pos.vz && pos.vz < zone->z_max) {
            result[0] = zone->result0;
            result[1] = 0;
            result[2] = zone->result2;
            *result_kind = zone->result_kind;
            return 1;
        }
    }
    return 0;
}

/* Forward a farm callback to farm_tile_occupant while fewer than FARM_CALLBACK_CAP
 * are queued; once full, clear *out_flags and report done. */
s32 farm_callback_capped(VECTOR *pos, LimitPair limits, s32 width, s32 depth, void **out_tile,
                  s32 *out_flags, void *walker, s32 mask) {
    s32 count = farm_callbacks_queued;

    if (count < FARM_CALLBACK_CAP) {
        farm_callbacks_queued = count + 1;
        return farm_tile_occupant(pos, limits, out_tile, out_flags, walker, mask);
    }
    *out_flags = 0;
    return 1;
}

/* Count one more farm callback and forward the arguments to farm_tile_occupant.
 * width and depth are unused. */
s32 farm_callback_count(VECTOR *pos, LimitPair limits, s32 width, s32 depth, void **out_tile,
                  s32 *out_flags, void *walker, s32 mask) {
    farm_callbacks_queued++;
    return farm_tile_occupant(pos, limits, out_tile, out_flags, walker, mask);
}

void farm_callback_reset(void) {
    farm_callbacks_queued = 0;
}

/* Test whether the query point lies inside the footprint of `thing`: an object on the
 * query's place that is not the query's current one and is not in an event, held or
 * claimed (the adult dog counts unless the date index is 8). A hit sets the query's flags
 * and tile. */
static s32 farm_object_contains(FarmTileQuery *query, void *thing)
{
    FarmObject *object = thing;
    s16 id;
    s16 left;
    s16 right;
    s16 x;
    s16 half;

    id = object->id;
    if (object->x30 == query->place->location && query->current != object
        && ((object->flags & (ACTOR_FLAG_IN_EVENT | ACTOR_FLAG_HELD | ACTOR_FLAG_CLAIMED)) == 0
            || (object->id == ITEM_ADULT_DOG
                && clock_date_index(game_state.progress.clock.date.season, game_state.progress.clock.date.day) != 8))) {
        x = object->x38;
        half = game_state.itemInfo[id].halfWidth;
        left = x - half;
        right = x + half;
        if (left < query->pos->vx && query->pos->vx < right) {
            x = object->z40;
            half = game_state.itemInfo[id].halfDepth;
            left = x - half;
            right = x + half;
            if (left < query->pos->vz && query->pos->vz < right) {
                *query->flags = query->hitFlags | 0x2000;
                if ((u32)(object->id - ITEM_CALF) <= ITEM_VISITING_SHEEP - ITEM_CALF) {
                    *query->flags &= ~0x2000;
                    *query->flags |= 0x14000;
                }
                if (object->id == ITEM_CAT || object->id == ITEM_17A || object->id == ITEM_17B
                    || object->id == ITEM_STRAY_DOG) {
                    *query->flags &= ~0x2000;
                }
                *query->tile = object;
                return 1;
            }
        }
    }
    return 0;
}

/* Test whether the query point lies in the player's box on the query's place. */
static s32 farm_player_match(FarmTileQuery *query) {
    FarmPlayer *player;
    s16 center;
    s16 low;
    s16 high;
    s32 coord;

    player = &game_state.player;
    if (player->place.location != query->place->location) {
        return 0;
    }
    center = player->position.vx;
    low = center - PLAYER_BOX_HALF_XZ;
    high = center + PLAYER_BOX_HALF_XZ;
    coord = query->pos->vx;
    if (low < coord && coord < high) {
        center = player->position.vz;
        low = center - PLAYER_BOX_HALF_XZ;
        high = center + PLAYER_BOX_HALF_XZ;
        coord = query->pos->vz;
        if (low < coord && coord < high) {
            center = player->position.vy;
            low = center - PLAYER_BOX_HALF_Y;
            high = center + PLAYER_BOX_HALF_Y;
            coord = query->pos->vy;
            if (low < coord && coord < high) {
                *query->flags = query->hitFlags;
                *query->tile = player;
                return 1;
            }
        }
    }
    return 0;
}

/* The location pick: refuses a point inside a placement box of the query's place (the
 * barn or coop table, else the default table, or the flagged table when
 * OCCUPANT_FLAGGED_CHOICES is set). Returns 1 when the point is free. */
static s32 farm_pick_location(FarmTileQuery *query) {
    s16 kind;
    u32 limit;
    FarmChoice *choice;
    u32 index;
    s32 x;

    kind = query->place->location;
    if (kind == PLACE_5) {
        limit = 9;
        if (game_state.progress.estate.barnLevel == 0) {
            limit = 7;
            choice = farm_choices_barn0;
        } else {
            choice = farm_choices_barn1;
        }
    } else {
        limit = 25;
        if (kind == PLACE_4) {
            limit = 10;
            if (game_state.progress.estate.coopLevel == 0) {
                limit = 5;
                choice = farm_choices_coop0;
            } else {
                choice = farm_choices_coop1;
            }
        } else {
            choice = farm_choices_default;
        }
    }
    if (query->mask & OCCUPANT_FLAGGED_CHOICES) {
        limit = 4;
        choice = farm_choices_flagged;
    }

    index = 0;
    if (limit != 0) {
        do {
            if (choice->kind == query->place->location) {
                x = query->pos->vx;
                if (choice->xMin < x) {
                    if (x < choice->xMax) {
                        if (choice->zMin < query->pos->vz) {
                            if (query->pos->vz < choice->zMax) {
                                return 0;
                            }
                        }
                    }
                }
            }
            index++;
            choice++;
        } while (index < limit);
    }
    return 1;
}

/* Classify the cell under query->pos: the field on place 1, the greenhouse on place 6,
 * and nothing on other places. A cell with a tile rule fills the query's flags and tile
 * and returns TILE_CLASS_BLOCKED or TILE_CLASS_PASSABLE by the rule's status. Returns 0
 * for no cell, no rule, or soil (ITEM_SOIL). */
static s32 farm_tile_classify(FarmTileQuery *query, s32 unused)
{
    VECTOR point;
    s16 column;
    s16 row;
    s16 setting;
    s32 mode;
    FarmFieldTile *record;
    FarmState *farm;
    s16 ruleIndex;
    s32 status;
    u16 id;
    u8 kind;
    s8 crop;

    point.vx = query->pos->vx;
    point.vy = query->pos->vy;
    point.vz = query->pos->vz;
    setting = query->place->location;
    mode = (setting != 1) * 2;
    if (setting == 6) {
        mode = 1;
    }
    if (mode == 2) {
        return 0;
    }
    if (mode != 0) {
        if (greenhouse_world_to_cell(&point, &column, &row) == 0) {
            goto found;
        }
        return 0;
    }
    if (field_world_to_cell(&point, &column, &row) != 0) {
        goto invalid;
    }
found:
    record = &game_state.fieldTiles[row].rows[column];
    if (mode != 0) {
        record = &game_state.slots7x7[row].rows[column];
    }
    if (record->value == ITEM_SOIL) {
        return 0;
    }
    farm = &game_state;
    id = record->value;
    kind = record->kind;
    ruleIndex = farm->itemInfo[id].tileRule[kind];
    status = farm->tileRules[ruleIndex].status;
    crop = farm->itemInfo[id].crop;
    if (status == TILE_RULE_NONE) {
        *query->flags = 0;
        *query->tile = record;
        return 0;
    }
    if (status < 0) {
        return 0;
    }
    if (status > TILE_RULE_OPEN) {
        return 0;
    }
    *query->flags = query->hitFlags | 0x4000;
    if (id == ITEM_BASKET) {
        *query->flags &= ~0x4000;
    }
    switch (farm->itemInfo[id].itemFlags) {
    case ITEM_FLAG_TOOL | ITEM_FLAG_SEED:
        if (farm->cropTable[crop].grownStage != kind) {
            break;
        }
    case ITEM_FLAG_BIT8:
    case ITEM_FLAG_PICKUP:
    case ITEM_FLAG_PICKUP | ITEM_FLAG_BIT4:
        *query->flags |= 0x2000;
        break;
    }
    *query->tile = record;
    if (game_state.tileRules[ruleIndex].status == TILE_RULE_BLOCKED) {
        return TILE_CLASS_BLOCKED;
    }
    return TILE_CLASS_PASSABLE;
invalid:
    return 0;
}

/* Season bit masks for the record flags: a record is live in the season whose bit is set. */
static u16 record_season_masks[4] = {0x0002, 0x0004, 0x0008, 0x0010};

/* Test whether the point lies in the footprint of an estate record whose item is live in
 * the current season; a hit sets the query's flags and tile. */
static s32 farm_record_hit_test(FarmTileQuery *query, FarmEstateRecord *record) {
    s16 extent;
    s16 coordinate;
    s16 lower;
    s16 upper;
    s32 coord;
    u16 *mask_table;
    if (((s16)record->itemId) != 0) {
        mask_table = record_season_masks;
        if (record->recordFlags & mask_table[game_state.progress.clock.date.season]) {
            extent = game_state.itemInfo[record->itemId].halfWidth;
            coordinate = record->position.vx;
            coord = query->pos->vx;
            lower = coordinate - extent;
            upper = coordinate + extent;
            if (lower < coord) {
                s32 *status;
                if (coord < upper) {
                    coordinate = record->position.vz;
                    extent = game_state.itemInfo[record->itemId].halfDepth;
                    coord = query->pos->vz;
                    lower = coordinate - extent;
                    upper = coordinate + extent;
                    if (lower < coord) {
                        if (coord < upper) {
                            if (estate_record_item_id(record) != 0) {
                                extent = game_state.itemInfo[record->itemId].itemFlags;
                                *query->flags |= 0x4000;
                                *query->tile = record;
                                if (record->itemId >= ITEM_BASKET && record->itemId <= ITEM_DOG_BALL) {
                                    *query->flags &= ~0x4000;
                                }
                                if (item_is_egg(record->itemId) != 0) {
                                    *query->flags &= ~0x4000;
                                }
                                if ((extent & 0x8000) != 0) {
                                    status = query->flags;
                                    *status |= 0x2000;
                                }
                                return 1;
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}

/* Look for whatever occupies the tile at `pos`, trying each object class
 * `mask` selects in turn. Each class's flag goes in query.hitFlags for the
 * callee; the first hit returns 1 (some also mark *flags), else 0. */
s32 farm_tile_occupant(VECTOR *pos, LimitPair place, void **tile, s32 *flags, void *current,
                  s32 mask) {
    FarmTileQuery query;
    FarmEstateRecord *spot;
    FarmAction *object;
    FarmLivestock *item;
    FarmChicken *plot;
    FarmHelper *animal;
    u32 i;
    u32 n;
    u32 bit;

    query.pos = pos;
    query.place = &place;
    query.tile = tile;
    query.current = current;
    query.flags = flags;
    query.mask = mask;
    if (mask & OCCUPANT_TILE) {
        query.hitFlags = FARM_HIT_TILE;
        switch (farm_tile_classify(&query, 0)) {
        case TILE_CLASS_PASSABLE:
            return 1;
        case TILE_CLASS_BLOCKED:
            bit = mask & OCCUPANT_TILE_BIT6;
            return bit == 0;
        }
    }
    if (mask & OCCUPANT_ESTATE_RECORD) {
        spot = game_state.runs.records[0];
        query.hitFlags = 0;
        for (i = 0; i < ESTATE_RECORD_COUNT; i++, spot++) {
            if (spot->location == query.place->location && (spot->recordFlags & ESTATE_RECORD_ENABLED) &&
                farm_record_hit_test(&query, spot) != 0) {
                *flags |= 0x8000;
                return 1;
            }
        }
    }
    if (mask & OCCUPANT_VILLAGER) {
        query.hitFlags = FARM_HIT_VILLAGER;
        if (farm_villager_find_box(&query) != 0) {
            return 1;
        }
    }
    if (mask & OCCUPANT_PLAYER) {
        query.hitFlags = FARM_HIT_PLAYER;
        if (farm_player_match(&query) != 0) {
            return 1;
        }
    }
    if (mask & OCCUPANT_ACTION) {
        query.hitFlags = FARM_HIT_ACTION;
        for (object = game_state.animals.actions, i = 0; i < 6; i++, object++) {
            if (object->actor.kind != 0 && farm_object_contains(&query, object) != 0) {
                return 1;
            }
        }
    }
    if (farm_area.mode.data->head.place.value == 0 && (mask & OCCUPANT_CATTLE)) {
        query.hitFlags = FARM_HIT_CATTLE;
        for (item = game_state.animals.livestock, i = 0; i < 20; i++, item++) {
            if ((u16)(item->actor.kind - ITEM_CALF) <= ITEM_VISITING_COW - ITEM_CALF && farm_object_contains(&query, item) != 0) {
                return 1;
            }
        }
    }
    if (farm_area.mode.data->head.place.value == 0 && (mask & OCCUPANT_SHEEP)) {
        query.hitFlags = FARM_HIT_SHEEP;
        for (item = game_state.animals.livestock, i = 0; i < 20; i++, item++) {
            if ((u16)(item->actor.kind - ITEM_LAMB) <= ITEM_VISITING_SHEEP - ITEM_LAMB && farm_object_contains(&query, item) != 0) {
                return 1;
            }
        }
    }
    if (mask & OCCUPANT_CHICKEN) {
        query.hitFlags = FARM_HIT_CHICKEN;
        for (plot = game_state.animals.chickens, n = 0; n < 10; n++, plot++) {
            if (farm_object_contains(&query, plot) != 0) {
                *flags |= 0x2000;
                return 1;
            }
        }
    }
    if (mask & OCCUPANT_HORSE) {
        query.hitFlags = FARM_HIT_HORSE;
        if (farm_object_contains(&query, &game_state.animals.horse) != 0) {
            *flags = 0x14200;
            return 1;
        }
    }
    if ((mask & OCCUPANT_DOG) && !(game_state.animals.dog.actor.flags & ACTOR_FLAG_HELD) &&
        game_state.animals.dog.actor.place.location == query.place->location) {
        query.hitFlags = FARM_HIT_DOG;
        if (farm_object_contains(&query, &game_state.animals.dog) != 0) {
            return 1;
        }
    }
    if (mask & OCCUPANT_HELPER) {
        query.hitFlags = FARM_HIT_HELPER;
        for (animal = game_state.helpers, i = 0; i < HELPER_COUNT; i++, animal++) {
            if (!(animal->flags & HELPER_BIT5) && farm_object_contains(&query, animal) != 0) {
                return 1;
            }
        }
    }
    if (mask & OCCUPANT_CHOICE) {
        query.hitFlags = FARM_HIT_CHOICE;
        if (farm_pick_location(&query) != 0) {
            return 1;
        }
    }
    if ((mask & OCCUPANT_SPECIAL) && farm_tile_is_special(&query) != 0) {
        return 1;
    }
    return 0;
}

/* Find a villager whose box on the query's place contains the point: VILLAGER_BOX_HALF_XZ
 * on x and z and VILLAGER_BOX_HALF_Y on y. A villager in an event is tested at its event
 * position while present. Clears VILLAGER_FLAG_BIT2 on each villager it visits; returns 0
 * while the player is auto-walking. */
static s32 farm_villager_find_box(FarmTileQuery *query) {
    s32 range[6];
    s32 *limits;
    VECTOR *bounds;
    FarmVillager *candidate;
    u16 flags;
    u16 clean_flags;
    s32 coord;
    s32 lower;
    s32 count;

    bounds = query->pos;
    range[0] = bounds->vx - VILLAGER_BOX_HALF_XZ;
    range[1] = bounds->vx + VILLAGER_BOX_HALF_XZ;
    range[2] = bounds->vz - VILLAGER_BOX_HALF_XZ;
    range[3] = bounds->vz + VILLAGER_BOX_HALF_XZ;
    candidate = &game_state.villagers[1];
    range[4] = bounds->vy - VILLAGER_BOX_HALF_Y;
    range[5] = bounds->vy + VILLAGER_BOX_HALF_Y;
    limits = range;
    if (game_state.player.stateFlags & PLAYER_FLAG_AUTO_WALK) return 0;
    count = 1;
    do {
        flags = candidate->flags;
        if (!(flags & VILLAGER_FLAG_IN_EVENT)) {
            clean_flags = flags & ~VILLAGER_FLAG_BIT2;
            candidate->flags = clean_flags;
            if (candidate->place.location == query->place->location) {
                if (clean_flags & VILLAGER_FLAG_ON_MAP) {
                    if (!(clean_flags & (VILLAGER_FLAG_EVENT_PENDING | VILLAGER_FLAG_EVENT_STARTED))) {
                        if (!(candidate->placeFlags & VILLAGER_PLACE_EVENT_CELL)) {
                            lower = limits[0];
                            coord = candidate->position.vx;
                            if ((lower < coord) && (coord < limits[1])) {
                                lower = limits[2];
                                coord = candidate->position.vz;
                                if ((lower < coord) && (coord < limits[3])) {
                                    lower = limits[4];
                                    coord = candidate->position.vy;
                                    if ((lower < coord) && (coord < limits[5])) {
                                        *query->flags = query->hitFlags;
                                        if (candidate->delay < 30) *query->flags |= 0x4000;
                                        *query->tile = candidate;
                                        return 1;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        } else if ((flags & VILLAGER_FLAG_PRESENT) && (limits[0] < candidate->ePosition.vx) &&
                   (candidate->ePosition.vx < limits[1]) && (limits[2] < candidate->ePosition.vz) &&
                   (candidate->ePosition.vz < limits[3])) {
            *query->flags = query->hitFlags;
            *query->tile = candidate;
            *query->flags |= 0x4000;
            return 1;
        }
        count++;
        candidate++;
    } while (count < 37);
    return 0;
}

/* Find the first villager standing at the search point: within 150 of it on x
 * and z and 20 on y, at its map position (flag 0x100) or, in an event (0x1000),
 * its event position (0x800). The second argument is not read by the search. */
FarmVillager *farm_villager_at_point(ZonePoint *point, s16 *unused1) {
    s32 range[6];
    s32 *limits;
    FarmVillager *item;
    VECTOR *box;
    u16 flags;
    u32 mask;
    u32 slot;

    range[0] = point->x - SEARCH_BOX_HALF_XZ;
    range[1] = point->x + SEARCH_BOX_HALF_XZ;
    range[2] = point->z - SEARCH_BOX_HALF_XZ;
    range[3] = point->z + SEARCH_BOX_HALF_XZ;
    range[4] = point->y - SEARCH_BOX_HALF_Y;
    range[5] = point->y + SEARCH_BOX_HALF_Y;
    limits = range;
    item = game_state.villagers;
    for (slot = 0; slot < CHARACTER_COUNT; slot++, item++) {
        flags = item->flags;
        if (flags & VILLAGER_FLAG_IN_EVENT) {
            mask = VILLAGER_FLAG_PRESENT;
            box = &item->ePosition;
        } else {
            mask = VILLAGER_FLAG_ON_MAP;
            box = &item->position;
        }
        if ((item->flags & mask) != 0 && limits[0] < box->vx && box->vx < limits[1] &&
            limits[2] < box->vz && box->vz < limits[3] && limits[4] < box->vy && box->vy < limits[5]) {
            return item;
        }
    }
    return 0;
}

/* Find the estate record of the place's key whose min corner lies in the search box around
 * `point` (SEARCH_BOX_HALF_XZ and SEARCH_BOX_HALF_Y), and return its result; 0 when none. */
s32 farm_record_find_key(ZonePoint *point, LimitPair *place) {
    s32 bounds[4];
    s32 *bounds_ptr;
    u32 count;
    FarmEstateRecord *record;
    bounds_ptr = bounds;
    count = 0;
    record = game_state.runs.records[0];
    bounds_ptr[0] = point->x - SEARCH_BOX_HALF_XZ;
    bounds_ptr[1] = point->x + SEARCH_BOX_HALF_XZ;
    bounds_ptr[2] = point->z - SEARCH_BOX_HALF_XZ;
    bounds_ptr[3] = point->z + SEARCH_BOX_HALF_XZ;
    do {
        if (place->location == record->location) {
            if (bounds_ptr[0] < record->position.vx && record->position.vx < bounds_ptr[1] &&
                bounds_ptr[2] < record->position.vz && record->position.vz < bounds_ptr[3])
                return record->itemId;
        }
        count++;
        record++;
    } while (count < ESTATE_RECORD_COUNT);
    return 0;
}

/* The top bit of the first byte of the ground hit primitive. The caller passes a primitive. */
unsigned int farm_status_upper_bits(void) {
    return *(u8 *)ground_hit_primitive() >> 7;
}

/* The kind of the ground hit primitive's flags: 2 for 0x4, 3 for 0x40, 1 for 0x2, else 0;
 * 0 when there is no primitive. */
s32 farm_status_kind(void) {
    GroundHitState *p;
    s32 flags;
    p = ground_hit_primitive();
    if (p == 0) return 0;
    flags = p->flags;
    if (flags & 4) return 2;
    if (flags & 0x40) return 3;
    if (flags & 2) return 1;
    return 0;
}

/* Caches the status record pointer and its low 16 flag bits. Returns 1 when
 * there is no record, else 0. */
s32 farm_status_cache(void) {
    u32 *status;
    u32 flags;

    status = ground_hit_primitive();
    farm_cached_status = status;
    if (status == 0) {
        return 1;
    }
    flags = *status;
    farm_cached_flags = flags & 0xFFFF;
    if (flags & 0x2000) {
        return 0;
    }
    if (farm_cached_flags == 0x8000) {
        return flags & 0x2000;
    }
    return 0;
}

static int farm_main_stub_2(void) {
    return 0;
}

/* Whether the step `index` (a facing) from `request` is clear: 1 when the tile one step
 * ahead is blocked and the tile two steps ahead is off the grid or free, else 0. */
s32 farm_probe_steps(FarmProbePosition *request, s32 index) {
    VECTOR first;
    VECTOR second;
    s16 column1;
    s16 row1;
    s16 column2;
    s16 row2;
    s16 location;
    s32 mode;
    s32 secondState;
    FarmStep *step;
    location = farm_area.mode.place.location;
    mode = (location != 1) * 2;
    if (location == 6) mode = 1;
    if (mode == 2) return 0;
    step = &facing_steps[index];
    first.vx = request->x + step->x;
    first.vy = request->y;
    first.vz = request->z + step->z;
    second.vx = request->x + step->x * 2;
    second.vy = request->y;
    second.vz = request->z + step->z * 2;
    if (mode != 0) {
        if (greenhouse_world_to_cell(&first, &column1, &row1) != 0) return 0;
        secondState = greenhouse_world_to_cell(&second, &column2, &row2);
    } else {
        if (field_world_to_cell(&first, &column1, &row1) != 0) return 0;
        if (column1 == 0x2C && index == 3) return 0;
        secondState = field_world_to_cell(&second, &column2, &row2);
    }
    if (farm_tile_state(column1, row1, mode) == 1 &&
        (secondState != 0 || farm_tile_state(column2, row2, mode) == 2)) return 1;
    return 0;
}

/* The state of the tile at column, row: 1 when its rule blocks, 2 for soil or a tile with
 * no rule, 0 when it is open. `mode` picks the greenhouse grid when set, else the field. */
static s32 farm_tile_state(s16 column, s16 row, s32 mode) {
    FarmFieldTile *record;
    ItemInfo *entry;
    s16 ruleIndex;
    s32 status;

    if (mode != 0) {
        record = &game_state.slots7x7[row].rows[column];
    } else {
        record = &game_state.fieldTiles[row].rows[column];
    }
    if (record->value == ITEM_SOIL) {
        return 2;
    }
    entry = game_state.itemInfo;
    ruleIndex = entry[record->value].tileRule[record->kind];
    status = game_state.tileRules[ruleIndex].status;
    switch (status) {
    case TILE_RULE_BLOCKED:
        return 1;
    case TILE_RULE_NONE:
        return 2;
    case TILE_RULE_OPEN:
        return 0;
    default:
        return 0;
    }
}

/* Is the tile under the query point one of the special farm tiles (0x3B-0x3D,
 * 0x3F, 0x16C-0x174)? Only map 1 is checked. */
static s32 farm_tile_is_special(FarmTileQuery *query) {
    VECTOR point;
    s16 column;
    s16 row;
    s32 mode;
    FarmFieldTile *record;

    point.vx = query->pos->vx;
    point.vy = query->pos->vy;
    point.vz = query->pos->vz;
    mode = (query->place->location != 1) * 2;
    if (mode == 2) {
        return 0;
    }
    if (field_world_to_cell(&point, &column, &row) != 0) {
        return 0;
    }
    if (mode != 0) {
        record = &game_state.slots7x7[row].rows[column];
    } else {
        record = &game_state.fieldTiles[row].rows[column];
    }
    switch (record->value) {
    case ITEM_BIG_STONE ... ITEM_STUMP:
    case ITEM_LUMBER:
    case FIELD_TILE_BIG_STONE_1 ... FIELD_TILE_STUMP_3:
        return 1;
    }
    return 0;
}

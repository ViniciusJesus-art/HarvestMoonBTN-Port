/* Item pictures and the field's cell sprites: the item tables, the picture grid and their draw. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <memory.h>
#include "types.h"
#include "afile.h"
#include "anim.h"
#include "compiled_out.h"
#include "estate.h"
#include "farm_data.h"
#include "farm_state.h"
#include "farm_tables.h"
#include "item_storage.h"
#include "items.h"
#include "main.h"
#include "picture_cells.h"
#include "player.h"
#include "scene.h"
#include "texture_pack.h"

/* Input of the picture_record_get_* getters: an item, and an offset from its picture's
 * grid record (the record index is picture * 5 + offset). */
typedef struct { s16 index; s16 offset; } FarmRecQuery;

#define PICTURE_RECORD_GETTER(name, field)                                        \
    s32 name(FarmRecQuery *in) {                                              \
        PictureGrid *grid;                                                    \
        s16 n = game_state.itemInfo[in->index].picture;                       \
                                                                              \
        if (n == -1) return -1;                                               \
        n *= 5;                                                               \
        n += in->offset;                                                      \
        grid = &game_state.pictureGrids[n];                                   \
        return grid->field;                                                   \
    }

#define PICTURE_CELL_SIZE 32
#define PICTURE_CLUT_RESERVED_X 1008    /* the CLUT column every row keeps for its pictures */
#define PICTURE_TPAGE_RESERVED 0x3C0
#define PICTURE_TPAGE_PLAIN 0x340

typedef s32 (*DispatchRecordCallback)(s8, s8, s16, s32);

typedef struct FarmDispatchRecord {
    DispatchRecordCallback callback;
    s8 firstArg;
    s8 secondArg;
    s16 thirdArg;
} FarmDispatchRecord;

/* One entry of the picture grid: two sprite cuts, each with a UV, size and draw offset, and the CLUT position. */
typedef struct FarmSpriteCell {
    s16 u[2][2];      /* [cut][u, v]; u is in 4-pixel units */
    s16 size[2][2];   /* [cut][w, h] */
    s16 offset[2][2]; /* [cut][dx, dy] */
    s16 clutX;
    u16 clutY;
    u8 pad1C[0x1E - 0x1C];
} FarmSpriteCell;

typedef struct FarmOrigin {
    s16 x;
    s16 y;
} FarmOrigin;

static void grid13_copy_oriented(u8 mode, u8 dst[13][13], u8 src[13][13]);
static void *picture_cell_draw(s16 column, s16 row, s16 part, u_long *ot, SPRT *sprite, s32 x, s32 y, s32 flags);

static RECT picture_move_rect;

void picture_cell_place_item(FarmStoredItem *item) {
    item->id = 0;
    item->modifier = 0;
    item->count = 0;
}

s32 item_picture_row(s16 index) {
    s16 flags;
    s32 result;
    flags = game_state.itemInfo[index].itemFlags;
    result = PICTURE_ROW_OTHER;
    if (flags & ITEM_FLAG_TOOL) {
        result = PICTURE_ROW_TOOL;
    } else if (flags & (ITEM_FLAG_PICKUP | ITEM_FLAG_BIT8)) {
        result = PICTURE_ROW_PICKUP;
    }
    return result;
}

/* The class of the item's classFlags word: 0, 1 or 2 for bits 28, 29 and 30; 3 for none. */
s32 item_class_index(s16 arg) {
    s16 i = arg;
    s32 flags;
    s32 result;

    flags = game_state.itemInfo[i].classFlags.word;
    result = 3;
    if (flags & 0x10000000) {
        result = 0;
    } else if (flags & 0x20000000) {
        result = 1;
    } else if (flags & 0x40000000) {
        result = 2;
    }
    return result;
}

/* 1 when bit 31 of the item's classFlags word is set. */
s32 item_class_word_negative(s16 item) {
    s32 flags = game_state.itemInfo[item].classFlags.word;
    return flags < (item ^ item);
}

s8 item_category(s16 item) {
    return game_state.itemInfo[item].category;
}

s32 item_is_seed(s16 index) {
    s32 flags = (s16)game_state.itemInfo[index].itemFlags;
    if (flags & ITEM_FLAG_SEED) return 1;
    return 0;
}

s32 item_is_field_object(s16 index) {
    s16 i = index;
    return ((s16)game_state.itemInfo[i].itemFlags & (ITEM_FLAG_PICKUP | ITEM_FLAG_BIT8)) != 0;
}

/* Run the item's use callback with its arguments: the stored modifier as the first when
 * itemFlags has 0x800, else the table's first argument. Returns -1 with no callback. */
s32 item_run_use(FarmStoredItem *item) {
    ItemInfo *record;
    FarmDispatchRecord *dispatch;
    s8 argument;

    record = game_state.itemInfo + item->id;
    dispatch = (FarmDispatchRecord *)&record->use;
    if ((record->itemFlags & 0x800) != 0) {
        argument = item->modifier;
    } else {
        argument = dispatch->firstArg;
    }
    if (dispatch != 0 && dispatch->callback != 0) {
        return dispatch->callback(argument, dispatch->secondArg, dispatch->thirdArg, 0);
    }
    return -1;
}

/* Rotate the 13x13 grid of the picture of item *item + offset and find the cell of
 * part `part` (a cell holds part + 1); report its position relative to the centre
 * (6, 6). If several cells hold it, the last row's first match wins. */
s32 picture_grid_find_cell(s16 *item, s32 part, s16 rotation, s16 *delta_x, s16 *delta_y, s16 offset) {
    PictureGrid grid;
    s16 index;
    s32 found;
    s32 x;
    s32 y;

    index = (game_state.itemInfo + *item + offset)->picture;
    if (index == -1) {
        return 0;
    }
    grid13_copy_oriented(rotation, grid.cells, game_state.pictureGrids[index].cells);
    part++;
    found = 0;
    for (y = 0; y < 13; y++) {
        for (x = 0; x < 13; x++) {
            if (grid.cells[y][x] == part) {
                found = 1;
                *delta_x = x - 6;
                *delta_y = y - 6;
                break;
            }
        }
    }
    return found;
}

PICTURE_RECORD_GETTER(picture_record_get_aa, unkAA)

PICTURE_RECORD_GETTER(picture_record_get_ac, unkAC)

PICTURE_RECORD_GETTER(picture_record_get_ae, unkAE)

/* The growth stage a crop of this item is mature at, or -1 for an item with no crop. */
s32 crop_grown_stage(s16 item) {
    FarmState *farm = &game_state;
    s8 index = farm->itemInfo[item].crop;

    if (index == -1) return -1;
    return farm->cropTable[index].grownStage;
}

/* The stage a harvested crop of this item goes back to, or -1 when the plant is removed. */
s32 crop_regrow_stage(s16 item) {
    FarmState *farm = &game_state;
    s8 index = farm->itemInfo[item].crop;

    if (index == -1) return -1;
    return farm->cropTable[index].regrowStage;
}

/* The item a crop of this item yields when it is harvested, or 0 for an item with no crop. */
s32 crop_harvest_item(s16 item) {
    s8 index;

    index = game_state.itemInfo[item].crop;
    if (index == -1) return 0;
    return game_state.cropTable[index].harvestItem;
}

u32 item_tile_hand_pickable(u16 item, u16 kind) {
    ItemInfo *items;
    FarmTileRule *rules;
    s16 ruleIndex;
    u16 flags;

    items = game_state.itemInfo;
    ruleIndex = items[item].tileRule[kind];
    if (ruleIndex == -1) return 0;
    rules = game_state.tileRules;
    flags = rules[ruleIndex].usableBy & TILE_USABLE_HAND;
    return flags != 0;
}

s32 item_tile_clear_by_tool(u16 item, u16 kind) {
    ItemInfo *items;
    FarmTileRule *rules;
    FarmState *farm;
    s16 ruleIndex;
    u32 flags;

    farm = &game_state;
    items = farm->itemInfo;
    ruleIndex = items[item].tileRule[kind];
    if (ruleIndex == -1) return 0;
    rules = farm->tileRules;
    flags = rules[ruleIndex].usableBy;
    flags &= TILE_USABLE_TOOL;
    return flags != 0;
}

s32 item_tile_clear_by_alt_tool(u16 item, u16 kind) {
    ItemInfo *items = game_state.itemInfo;
    FarmTileRule *rules;
    s16 ruleIndex = items[item].tileRule[kind];
    u16 flags;

    if (ruleIndex == -1) return 0;
    rules = game_state.tileRules;
    flags = rules[ruleIndex].usableBy;
    flags &= TILE_USABLE_ALT_TOOL;
    return flags != 0;
}

/* The rule's minimum power for the item on this tile kind, or -1 when no hand, tool or alt
 * tool can use it. */
u32 item_tile_min_power(u16 item, u16 kind) {
    ItemInfo *items;
    FarmTileRule *rules;
    s16 ruleIndex;
    u16 flags;
    u16 value;

    items = game_state.itemInfo;
    ruleIndex = items[item].tileRule[kind];
    if (ruleIndex == -1) {
        return -1;
    }
    rules = game_state.tileRules;
    value = rules[ruleIndex].minPower;
    flags = rules[ruleIndex].usableBy;
    if ((flags & (TILE_USABLE_HAND | TILE_USABLE_TOOL | TILE_USABLE_ALT_TOOL)) == 0) {
        return -1;
    }
    return value;
}

/* The rule's minimum reach for the item on this tile kind, or -1 when no hand, tool or alt
 * tool can use it. */
u32 item_tile_min_reach(u16 item, u16 kind) {
    ItemInfo *items;
    FarmTileRule *rules;
    s16 ruleIndex;
    u16 flags;
    u16 value;

    items = game_state.itemInfo;
    ruleIndex = items[item].tileRule[kind];
    if (ruleIndex == -1) {
        return -1;
    }
    rules = game_state.tileRules;
    value = rules[ruleIndex].minReach;
    flags = rules[ruleIndex].usableBy;
    if ((flags & (TILE_USABLE_HAND | TILE_USABLE_TOOL | TILE_USABLE_ALT_TOOL)) == 0) {
        return -1;
    }
    return value;
}

s32 item_tile_request_ok(u16 item, u16 kind) {
    FarmState *farm;
    ItemInfo *items;
    FarmTileRule *rules;
    s16 ruleIndex;
    u16 flags;

    item &= 0xffff;
    kind &= 0xffff;
    farm = &game_state;
    items = farm->itemInfo;
    ruleIndex = items[item].tileRule[kind];
    if (ruleIndex == -1) return 0;
    rules = farm->tileRules;
    flags = rules[ruleIndex].usableBy;
    flags &= TILE_USABLE_REQUEST;
    return flags != 0;
}

/* Store the picture record of item `value` in p: an egg is ITEM_EGG with its grade as
 * p[1] (0 normal, 1 good, 2 excellent), any other item is stored as itself. p[2] is 0. */
static void picture_cell_set_item(u16 *p, u16 value) {
    value &= 0xFFFF;
    switch (value) {
    case ITEM_NORMAL_EGG:
        p[0] = ITEM_EGG;
        p[1] = 0;
        break;
    case ITEM_GOOD_EGG:
        p[0] = ITEM_EGG;
        p[1] = 1;
        break;
    case ITEM_EXCELLENT_EGG:
        p[0] = ITEM_EGG;
        p[1] = 2;
        break;
    default:
        p[0] = value;
        p[1] = 0;
        break;
    }
    p[2] = 0;
}

s32 item_buy_price(u16 index) {
    return game_state.itemInfo[index].buyPrice;
}

s32 item_sell_price(u16 item) {
    u32 row = item;
    s32 bonus = 0;

    if (row == ITEM_HONEY && (game_state.progress.estate.estateFlags & ESTATE_FLAG_ROW97_BONUS)) {
        bonus = 10;
    }
    return bonus + game_state.itemInfo[row].sellPrice;
}

/* 1 for a pickup (ITEM_FLAG_PICKUP without BIT8) whose category is not 1. */
s32 item_is_pickup_not_tool(u16 index) {
    u16 i = index;
    s8 x = game_state.itemInfo[i].category;
    if ((game_state.itemInfo[i].itemFlags & (ITEM_FLAG_PICKUP | ITEM_FLAG_BIT8)) != ITEM_FLAG_PICKUP) return 0;
    x ^= 1;
    return x != 0;
}

/* The item a gift holds (its modifier) when the item is ITEM_GIFT, else 0. */
s16 stored_item_contents(FarmStoredItem *item) {
    s16 result;
    if (item->id != ITEM_GIFT)
        result = 0;
    else
        result = item->modifier;
    return result;
}

/* Read the 3-bit code for `slot` of farm record `id`. The record's picture picks
 * a 16-byte row of FarmState.pictureCodes holding ten 3-bit codes per word; returns
 * -1 when the record has no row and -2 when the item is a tool (ITEM_FLAG_TOOL).
 * When flags 0x180 are set, codes 0-2 read as 5 and 3-4 as 6. */
s32 item_villager_reaction(s16 id, s16 slot) {
    s16 picture;
    u16 flags;
    s32 reaction;
    s32 *codes;

    picture = game_state.itemInfo[id].picture;
    flags = game_state.itemInfo[id].itemFlags;
    if (picture == -1) {
        return -1;
    }
    if (flags & ITEM_FLAG_TOOL) {
        return -2;
    }
    codes = (s32 *)(picture * 16 + (s32)game_state.pictureCodes) + (slot / 10);
    reaction = *codes >> ((slot % 10) * 3);
    reaction &= 7;
    if (flags & 0x180) {
        switch (reaction) {
        case ITEM_REACTION_BEST:
        case ITEM_REACTION_GOOD:
        case ITEM_REACTION_OK:
            reaction = ITEM_REACTION_QUIET;
            break;
        case ITEM_REACTION_BAD:
        case ITEM_REACTION_WORST:
            reaction = ITEM_REACTION_UPSET;
            break;
        }
    }
    return reaction;
}

/* The season this crop's item grows in (0 spring .. 3 winter, 4 any), or -1 for an item with no crop. */
s32 crop_season(s16 item) {
    ItemInfo *records = game_state.itemInfo;
    s8 index = records[item].crop;

    if (index == -1) return -1;
    return game_state.cropTable[index].season;
}

/* Add signed deltas to the player's stamina, fatigue and happiness, clamping each: stamina to
 * [0, max], fatigue to [0, 100] (99 when the clock's time word is 6 and fatigue reaches 100),
 * happiness to [0, 0xFFFF]. A positive fatigue delta is skipped while PLAYER_LATCH_FATIGUE_LOCKED
 * is set. unused3 is not read. */
s32 farm_clamp_status(s8 stamina, s8 fatigue, s16 happiness, s32 unused3) {
    FarmPlayer *player = &game_state.player;

    if (stamina != 0) {
        player->stamina += stamina;
        player->stamina = (player->stamina < 0) ? 0 : player->stamina;
        player->stamina =
            (player->stamina < player->maxStamina) ? player->stamina : player->maxStamina;
    }
    if (fatigue < 0 || (fatigue > 0 && !(player->latches & PLAYER_LATCH_FATIGUE_LOCKED))) {
        player->fatigue += fatigue;
        player->fatigue = (player->fatigue < 0) ? 0 : player->fatigue;
        player->fatigue = (player->fatigue > 100) ? 100 : player->fatigue;
        if (game_state.progress.clock.time.word == 6 && player->fatigue >= 100) {
            player->fatigue = 99;
        }
    }
    if (happiness != 0) {
        game_state.player.happiness += happiness;
        game_state.player.happiness = (player->happiness < 0) ? 0 : player->happiness;
        game_state.player.happiness = (player->happiness > 65535) ? 65535 : player->happiness;
    }
    return 1;
}

/* Changes the estate's gold by amount, with reset passed to estate_gold_change, and stores
 * the result in *out when it is given. unused is not read. */
s32 item_use_coin(s8 amount, s8 reset, s32 unused, s32 *out) {
    s32 result;
    result = estate_gold_change(amount, reset);
    if (out != 0) {
        *out = result;
    }
    return 1;
}

/* Add the two signed deltas to the player's maximum stamina (0..255) and fatigue (0..100).
 * A nonzero maximum delta, up or down, refills stamina to the new maximum and counts one
 * in progress.estate.powerBerries. */
s32 farm_adjust_stamina_fatigue(s8 maxStaminaDelta, s8 fatigueDelta) {
    FarmPlayer *player;
    FarmState *farm;

    player = &game_state.player;
    if (maxStaminaDelta != 0) {
        player->maxStamina += maxStaminaDelta;
        player->maxStamina = (player->maxStamina < 0) ? 0 : player->maxStamina;
        player->maxStamina = (player->maxStamina >= 256) ? 255 : player->maxStamina;
        player->stamina = player->maxStamina;
        farm = &game_state;
        farm->progress.estate.powerBerries++;
    }
    if (fatigueDelta != 0) {
        player->fatigue += fatigueDelta;
        player->fatigue = (player->fatigue < 0) ? 0 : player->fatigue;
        player->fatigue = (player->fatigue > 100) ? 100 : player->fatigue;
    }
    return 1;
}

/* The orientations grid13_copy_oriented writes the grid in. */
enum GridOrientation {
    GRID_ORIENT_TRANSPOSE = 0,
    GRID_ORIENT_MIRROR = 1,
    GRID_ORIENT_TRANSPOSE_FLIP = 2,
    GRID_ORIENT_COPY = 3,
};

/* Copy the 13 x 13 farm grid `src` into `dst` in one of four orientations. */
static void grid13_copy_oriented(u8 mode, u8 dst[13][13], u8 src[13][13]) {
    s32 row;
    s32 col;

    switch (mode) {
    case GRID_ORIENT_TRANSPOSE:
        for (row = 0; row < 13; row++) {
            for (col = 12; col >= 0; col--) {
                dst[row][col] = src[col][row];
            }
        }
        break;
    case GRID_ORIENT_MIRROR:
        for (row = 0; row < 13; row++) {
            for (col = 12; col >= 0; col--) {
                dst[row][12 - col] = src[row][col];
            }
        }
        break;
    case GRID_ORIENT_TRANSPOSE_FLIP:
        for (row = 12; row >= 0; row--) {
            for (col = 0; col < 13; col++) {
                dst[12 - row][col] = src[col][row];
            }
        }
        break;
    case GRID_ORIENT_COPY:
        for (row = 0; row < 13; row++) {
            for (col = 0; col < 13; col++) {
                dst[row][col] = src[row][col];
            }
        }
        break;
    }
}

void picture_grid_clear(void) {
    memset(farm_area.mode.pictureIds, 0, sizeof(farm_area.mode.pictureIds));
    memset(farm_area.mode.pictureEntries, 0xFF, sizeof(farm_area.mode.pictureEntries));
}

/* Free id's place: find it in its owner's row (item_picture_row's class, where 2
 * counts as row 1), clear the id and reset the place's picture to 0xFF bytes. */
void picture_release_item(s16 id) {
    s32 kind;
    u32 i;

    if (id == 0) {
        return;
    }
    kind = item_picture_row(id);
    if (kind == PICTURE_ROW_OTHER) {
        kind = PICTURE_ROW_PICKUP;
    }
    for (i = 0; i < PICTURE_SLOTS; i++) {
        if (farm_area.mode.pictureIds[kind][i] == id) {
            farm_area.mode.pictureIds[kind][i] = 0;
            memset(&farm_area.mode.pictureEntries[kind][i], 0xFF, sizeof(FarmGridEntry));
            return;
        }
    }
}

/* Claim the first empty slot of picture row `row` for id: returns the slot, or -1 when the
 * row is full. */
s32 picture_slot_claim(s16 id, s16 row) {
    s16 *p = farm_area.mode.pictureIds[row];
    u32 i = 0;
    while (i < PICTURE_SLOTS) {
        if (*p == 0) {
            *p = id;
            return i;
        }
        i++;
        p++;
    }
    return -1;
}

/* 1 when no picture in row `arg` uses the reserved CLUT column (an empty entry, clutX -1,
 * does not count), else 0. */
s32 picture_row_clut_free(s16 arg) {
    FarmGridEntry *p = farm_area.mode.pictureEntries[arg];
    s32 ok = 1;
    u32 i = 0;
    do {
        s16 v = p->clutX;
        if (v == -1) {
        } else if (v >= PICTURE_CLUT_RESERVED_X) {
            ok = 0;
        }
        i++;
        p++;
    } while (i < PICTURE_SLOTS);
    return ok;
}

/* The first VRAM slot 0..7 (its x is slot * 8) that no picture of row `arg` uses outside the
 * reserved CLUT column; 0 when all eight are used. */
s32 picture_free_slot(s16 arg) {
    int slot_code;
    u32 slot;
    u32 column;
    s32 available;
    s32 base_offset;
    FarmGridEntry *entry;
    slot = 0;
    base_offset = (arg * 9) * 0x1e;
    entry = (FarmGridEntry *)(((u8 *)farm_area.mode.pictureEntries) + base_offset);
    while (1) {
        slot_code = slot * 8;
        available = 1;
        column = 0;
        while (column < PICTURE_SLOTS) {
            if ((slot_code == entry->x) && (entry->clutX < PICTURE_CLUT_RESERVED_X)) {
                available = 0;
            }
            column++;
            entry++;
        }

        if (available != 0) {
            if (entry) {
                return slot;
            } else {
                return slot;
            }
        }
        slot++;
        if (slot >= 8) {
            return 0;
        }
        entry = (FarmGridEntry *)(((u8 *)farm_area.mode.pictureEntries) + base_offset);
    }
}

/* The .ANI of the item art pack the current mode uses that is for item `packed_id`:
 * resource_lists's list of item ids (count first) runs parallel to the pack's offsets. */
void *item_art_address(s16 packed_id) {
    FarmModeScript *resource;
    s16 *resource_list;
    s32 *address_list;
    s16 resource_count;
    s32 resource_index;
    s32 entry_count;
    s32 id;

    id = packed_id;
    resource = farm_area.mode.data;
    if (resource == 0) {
        return 0;
    }
    resource_count = resource->head.resourceSet;
    if (resource_count == -1) {
        return 0;
    }
    resource_list = resource_lists[resource_count];
    address_list = &farm_item_tocs[resource_count]->count;
    if (resource_list == 0) {
        return 0;
    }
    resource_index = 1;
    entry_count = resource_list[0] + 1;
    resource_list++;
    address_list++;
    if (resource_index < entry_count) {
        do {
            if (id == resource_list[0]) {
                return farm_area.itemArt + address_list[0];
            }
            resource_index++;
            resource_list++;
            address_list++;
        } while (resource_index < entry_count);
    }
    return 0;
}

/* Copy the parts, last first, into VRAM: for each part move its pixels
 * (offset by the page offset) to (x, y) and a 16x1 strip of its palette
 * (offset by the palette offset) to (x2, y2). */
static void picture_move_parts(AnimState *set, AnimPart *pairs, s16 count, s16 x, s16 y,
                   s16 x2, s16 y2) {
    s32 i;
    AnimPart *pair;
    AnimImage *rects;
    AnimImage *first;
    AnimImage *second;

    pair = &pairs[count - 1];
    for (i = count - 1; i >= 0; i--, pair--) {
        rects = set->images;
        first = &rects[pair->image];
        second = &rects[pair->palette];
        picture_move_rect.x = first->x + set->pageOffX;
        picture_move_rect.y = first->y + set->pageOffY;
        picture_move_rect.w = first->w;
        picture_move_rect.h = first->h;
        MoveImage(&picture_move_rect, x, y);
        picture_move_rect.x = second->x + set->clutOffX;
        picture_move_rect.y = second->y + set->clutOffY;
        picture_move_rect.w = 16;
        picture_move_rect.h = 1;
        MoveImage(&picture_move_rect, x2, y2);
    }
}

/* Place the sprite `id` in the first free cell of its kind's grid row and fill
 * in the cell's UV, CLUT, sizes and offsets from its sprite file -- the copy in the
 * item file (item_art_address) when there is one, else the disc sector for the id. A sprite
 * from a file is copied to VRAM tile by tile, otherwise the whole block is
 * uploaded. Returns 0 when there is no id or no free cell. */
s32 picture_load_to_vram(s32 sprite)
{
    s16 id = sprite;
    FarmOrigin origin;
    AnimState file;
    AnimState *f;
    s32 from_file = 0;
    s32 count0 = 0;
    s32 count1 = 0;
    s32 kind;
    s32 slot;
    void *address;
    s32 clut_free;
    AnimFrame *frame;
    AnimPartList *list;
    AnimPart *parts;
    AnimPart *last0;
    AnimPart *last1;
    AnimImage *rect0;
    AnimImage *rect1;
    FarmSpriteCell *cell;

    if (id == 0) {
        return 0;
    }
    kind = item_picture_row(id);
    if (kind == PICTURE_ROW_OTHER) {
        kind = PICTURE_ROW_PICKUP;
    }
    slot = picture_slot_claim(id, kind);
    if (slot == -1) {
        return 0;
    }
    address = item_art_address(id);
    if (address == 0) {
        afile_read_sectors(0, 0x3C19000 + (id << 11), 0x800, 0, game_state.work.sector);
        anim_bind_file(&file, (AnimFile *)game_state.work.sector);
    } else {
        from_file = 1;
        anim_bind_file(&file, address);
    }
    f = &file;
    rect0 = 0;
    last0 = 0;
    rect1 = 0;
    last1 = 0;
    cell = &((FarmSpriteCell (*)[9])farm_area.mode.pictureEntries)[kind][slot];
    frame = ANIM_FRAMES(f, &f->ranges[0]);
    if (frame->parts < 0xFFFE) {
        list = &f->partLists[frame->parts];
        parts = ANIM_PARTS(f, list);
        count0 = list->count;
        last0 = &parts[count0 - 1];
        rect0 = &f->images[last0->image];
    }
    frame = ANIM_FRAMES(f, &f->ranges[1]);
    if (frame->parts < 0xFFFE) {
        list = &f->partLists[frame->parts];
        parts = ANIM_PARTS(f, list);
        count1 = list->count;
        last1 = &parts[count1 - 1];
        rect1 = &f->images[last1->image];
    }
    clut_free = picture_row_clut_free(kind);
    cell->size[0][0] = 0;
    cell->size[0][1] = 0;
    cell->offset[0][0] = 0;
    cell->offset[0][1] = 0;
    cell->size[1][0] = 0;
    cell->size[1][1] = 0;
    cell->offset[1][0] = 0;
    cell->offset[1][1] = 0;
    if (clut_free) {
        cell->clutX = PICTURE_CLUT_RESERVED_X;
        cell->clutY = kind + 254;
        cell->u[0][0] = kind * 8 + 40;
        cell->u[0][1] = 189;
        origin.x = 640;
    } else {
        slot = picture_free_slot(kind);
        cell->clutX = (slot - slot / 4 * 4) * 16 + 832;
        cell->clutY = kind * 2 + slot / 4 + 251;
        cell->u[0][0] = slot * 8;
        cell->u[0][1] = kind * 32 + 155;
        origin.x = 512;
    }
    origin.y = 0;
    if (rect0) {
        cell->size[0][0] = rect0->w * 4;
        cell->size[0][1] = rect0->h;
        cell->offset[0][0] = last0->dx;
        cell->offset[0][1] = last0->dy;
    }
    if (rect1) {
        cell->size[1][0] = rect1->w * 4;
        cell->size[1][1] = rect1->h;
        cell->offset[1][0] = last1->dx;
        cell->offset[1][1] = last1->dy;
    }
    if (last1) {
        cell->u[1][0] = cell->u[0][0];
        if (rect0->y == rect1->y && rect0->x == rect1->x) {
            cell->u[1][1] = cell->u[0][1];
        } else {
            cell->u[1][1] = cell->u[0][1] + cell->size[0][1];
        }
    }
    if (from_file) {
        if (last0) {
            picture_move_parts(f, last0, count0, origin.x + cell->u[0][0] + 320,
                          origin.y + cell->u[0][1], cell->clutX, cell->clutY);
        }
        if (last1) {
            picture_move_parts(f, last1, count1, origin.x + cell->u[0][0] + 320,
                          origin.y + cell->u[1][1], cell->clutX, cell->clutY);
            return 1;
        }
    } else {
        texture_pack_load_at(game_state.work.sector + 0x100, origin.x + cell->u[0][0], origin.y + cell->u[0][1],
                      cell->clutX - 320, cell->clutY - 254);
    }
    return 1;
}

/* Find `key` in farm_area.mode.pictureIds (row 0 first, then row 1, column by column) and,
 * when found, build the cell's primitive at (u, v). Flags 1, 4 or 8 first draw the empty
 * marker there; flag 8 centres it using the cell's recorded size; flag 4 shifts it by 16
 * and adds flag 2. */
void picture_cell_build_prim(s16 key, s16 part, u_long *object, s16 u, s16 v, s32 flags) {
    s32 row;
    u32 column;
    s16 *row0;
    s16 *row1;

    row = -1;
    row0 = farm_area.mode.pictureIds[0];
    row1 = farm_area.mode.pictureIds[1];
    column = 0;
    while (column < PICTURE_SLOTS) {
        if (*row0 == key) {
            row = 0;
            break;
        }
        if (*row1 == key) {
            row = 1;
            break;
        }
        column++;
        row0++;
        row1++;
    }
    if (row == -1) return;

    if (flags & (PICTURE_CELL_BIT0 | PICTURE_CELL_HALF_SHIFT | PICTURE_CELL_CENTER)) {
        item_empty_marker_draw(&object[1], u, v);
    }
    if (flags & PICTURE_CELL_CENTER) {
        u += (PICTURE_CELL_SIZE - farm_area.mode.pictureEntries[row][column].w) / 2;
        v += (PICTURE_CELL_SIZE - farm_area.mode.pictureEntries[row][column].h) / 2;
    }
    if (flags & PICTURE_CELL_HALF_SHIFT) {
        u += 16;
        v += 16;
        flags |= PICTURE_CELL_BIT1;
    }
    gs_work_release(picture_cell_draw(column, row, part, object, gs_work_alloc(0), u, v, flags));
}

/* Queue picture [row][column] -- its second part when `part` is set -- as a
 * textured sprite at (x, y), moved by that part's draw offset under flag 2,
 * into `ot`; then the draw-mode packet for its texture page (PICTURE_TPAGE_RESERVED
 * when the CLUT sits in the reserved column, else PICTURE_TPAGE_PLAIN). Returns the
 * next free packet. */
static void *picture_cell_draw(s16 column, s16 row, s16 part, u_long *ot, SPRT *sprite, s32 x, s32 y, s32 flags) {
    FarmGridEntry *picture;
    DR_MODE *mode;
    s16 px;
    s16 py;
    s16 u;
    s16 v;
    s16 w;
    s16 h;
    u32 tpageX;

    px = x;
    py = y;
    picture = &farm_area.mode.pictureEntries[row][column];
    setSprt(sprite);
    sprite->clut = getClut(picture->clutX, picture->clutY);
    setRGB0(sprite, 0x80, 0x80, 0x80);
    setShadeTex(sprite, 0);
    if (part != 0) {
        u = picture->x2;
        v = picture->y2;
        w = picture->w2;
        h = picture->h2;
        if (flags & PICTURE_CELL_BIT1) {
            px += picture->offset[1][0];
            py += picture->offset[1][1];
        }
    } else {
        u = picture->x;
        v = picture->y;
        w = picture->w;
        h = picture->h;
        if (flags & PICTURE_CELL_BIT1) {
            px += picture->offset[0][0];
            py += picture->offset[0][1];
        }
    }
    sprite->x0 = px;
    sprite->y0 = py;
    sprite->w = w;
    sprite->h = h;
    sprite->u0 = u * 4;
    sprite->v0 = v;
    AddPrim(ot, sprite);
    sprite++;
    mode = (DR_MODE *)sprite;
    FARM_DEBUG_CHECK();
    tpageX = PICTURE_TPAGE_PLAIN;
    if (picture->clutX >= PICTURE_CLUT_RESERVED_X) {
        tpageX = PICTURE_TPAGE_RESERVED;
    }
    setlen(mode, 1);
    mode->code[0] = 0xE1000600 | (tpageX >> 6);
    AddPrim(ot, mode);
    return mode + 1;
}

/* Adds amount to a stored item's count. Returns 1 when the count reaches 0; a count below 0
 * is pinned to -1. */
s32 counter_step(FarmStoredItem *item, s16 amount) {
    s32 sum;

    sum = amount + (u16)item->count;
    item->count = sum;
    if ((s16)sum == 0) {
        return 1;
    }
    if ((s16)sum < 0) {
        item->count = -1;
    }
    return 0;
}

void stored_item_clear(FarmStoredItem *item) {
    item->id = 0;
    item->modifier = 0;
    item->count = 0;
}

void stored_item_set(FarmStoredItem *item, s16 id, s16 modifier, s16 count) {
    item->id = id;
    item->modifier = modifier;
    item->count = count;
}

void stored_item_copy(FarmStoredItem *dst, FarmStoredItem *src) {
    dst->id = src->id;
    dst->modifier = src->modifier;
    dst->count = src->count;
}

s32 stored_item_kind(FarmStoredItem *item) {
    s16 record_id;
    u16 flags;

    record_id = item->id;
    flags = (game_state.itemInfo + record_id)->itemFlags;
    if (flags & 0x400) {
        return STORED_ITEM_DRINK;
    }
    if (flags & 0x1800) {
        return STORED_ITEM_FOOD;
    }
    if (flags & ITEM_FLAG_SEED) {
        return STORED_ITEM_SEED;
    }
    if (flags & ITEM_FLAG_BIT8) {
        return STORED_ITEM_STASH;
    }
    if (record_id == ITEM_DOG_BALL || record_id == ITEM_LUMBER || record_id == ITEM_STONE || record_id == ITEM_BRANCH ||
        record_id == ITEM_BASKET) {
        return STORED_ITEM_CARRY;
    }
    return STORED_ITEM_GOODS;
}

/* The medal price of a shop item, 0 for an item with none. */
s32 shop_medal_price(s32 item) {
    s32 code;
    s32 result;

    code = item & 0xFFFF;
    result = 0;
    switch (code) {
    case ITEM_POWER_BERRY:
        result = 1001;
        break;
    case 0x185:
        result = 97;
        break;
    case ITEM_BRACELET:
        result = 42;
        break;
    case ITEM_NECKLACE:
        result = 35;
        break;
    case ITEM_EARRINGS:
        result = 33;
        break;
    case ITEM_TURBOJOLT_XL:
        result = 21;
        break;
    case ITEM_BODIGIZER_XL:
        result = 15;
        break;
    case ITEM_TRUFFLE:
        result = 12;
        break;
    case ITEM_HONEY:
        result = 7;
        break;
    case ITEM_MYSTRILE:
        result = 3;
        break;
    }
    return result;
}

/* The egg items: the three egg grades, the golden egg and Popuri's egg. */
s32 item_is_egg(s16 id) {
    if ((u16)(id - ITEM_NORMAL_EGG) < 3 || id == ITEM_GOLDEN_EGG || id == ITEM_POPURIS_EGG) return 1;
    return 0;
}

/* The milk items: the three milk sizes and ITEM_MILK_G. */
s32 item_is_milk(s16 id) {
    if ((u16)(id - ITEM_MILK_S) < 3 || id == ITEM_MILK_G) return 1;
    return 0;
}

/* The wool items: the three wool sizes and ITEM_WOOL_G. */
s32 item_is_wool(s16 id) {
    if ((u16)(id - ITEM_WOOL_S) < 3 || id == ITEM_WOOL_G) return 1;
    return 0;
}


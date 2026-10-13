#ifndef PICTURE_CELLS_H
#define PICTURE_CELLS_H

/* Item pictures placed in VRAM cells. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "item_storage.h"

/* One picture shown on the farm: where its two parts and its CLUT sit in VRAM. */
typedef struct FarmGridEntry {
    s16 x, y;           /* first part */
    s16 x2, y2;         /* second part */
    s16 w, h;
    s16 w2, h2;
    s16 offset[2][2];   /* draw offset of each part (dx, dy) */
    s16 clutX, clutY;   /* the row's reserved CLUT column */
    s16 hidden;         /* not drawn while set */
} FarmGridEntry;

/* One of the parked pictures. */
typedef struct FarmPictureSlot {
    FarmGridEntry picture;
    u8 used;
    u8 pad1F;
    u16 id;                 /* 0: free */
} FarmPictureSlot;

/* FarmLoadArea.mode's two rows of nine pictures. */
#define PICTURE_ROWS  2
#define PICTURE_SLOTS 9

/* Flags for building a cell's primitive. */
enum PictureCellFlag {
    PICTURE_CELL_BIT0 = 0x1,
    PICTURE_CELL_BIT1 = 0x2,
    PICTURE_CELL_HALF_SHIFT = 0x4,      /* moves the picture by half a cell and sets BIT1 */
    PICTURE_CELL_CENTER = 0x8,          /* centres the picture in its cell */
};

/* The UV pairs and palette of one cell texture. */
typedef struct FarmCellTexture {
    u8 uv[8];
    u16 clut;
} FarmCellTexture;

void picture_cell_place_item(FarmStoredItem *item);
s32 item_picture_row(s16 index);
s32 item_class_index(s16 arg);
s32 item_class_word_negative(s16 item);
s8 item_category(s16 item);
s32 item_is_seed(s16 index);
s32 item_is_field_object(s16 index);
s32 item_run_use(FarmStoredItem *item);
s32 picture_grid_find_cell(s16 *item, s32 part, s16 rotation, s16 *delta_x, s16 *delta_y, s16 offset);
s32 crop_grown_stage(s16 item);
s32 crop_regrow_stage(s16 item);
s32 crop_harvest_item(s16 item);
u32 item_tile_hand_pickable(u16 item, u16 kind);
s32 item_tile_clear_by_tool(u16 item, u16 kind);
s32 item_tile_clear_by_alt_tool(u16 item, u16 kind);
u32 item_tile_min_power(u16 item, u16 kind);
u32 item_tile_min_reach(u16 item, u16 kind);
s32 item_tile_request_ok(u16 item, u16 kind);
s32 item_buy_price(u16 index);
s32 item_sell_price(u16 item);
s32 item_is_pickup_not_tool(u16 index);
s16 stored_item_contents(FarmStoredItem *item);
/* The reaction a villager gives a gift: the 3-bit code of the item's picture row, best
 * first. The codes 5 and 6 are what a flagged item (itemFlags 0x180) reads as. */
enum ItemReaction {
    ITEM_REACTION_BEST = 0,
    ITEM_REACTION_GOOD = 1,
    ITEM_REACTION_OK = 2,
    ITEM_REACTION_BAD = 3,
    ITEM_REACTION_WORST = 4,
    ITEM_REACTION_QUIET = 5,
    ITEM_REACTION_UPSET = 6,
};
s32 item_villager_reaction(s16 id, s16 slot);
s32 crop_season(s16 item);
s32 farm_clamp_status(s8 stamina, s8 fatigue, s16 happiness, s32 unused3);
s32 item_use_coin(s8 a, s8 b, s32 c, s32 *out);
s32 farm_adjust_stamina_fatigue(s8 maxStaminaDelta, s8 fatigueDelta);
void picture_grid_clear(void);
void picture_release_item(s16);
s32 picture_slot_claim(s16 id, s16 row);
s32 picture_row_clut_free(s16 arg);
s32 picture_free_slot(s16 arg);
void *item_art_address(s16 packed_id);
s32 picture_load_to_vram(s32 sprite);
void picture_cell_build_prim(s16 key, s16 part, u_long *object, s16 u, s16 v, s32 flags);
s32 counter_step(FarmStoredItem *item, s16 amount);
void stored_item_clear(FarmStoredItem *item);
void stored_item_set(FarmStoredItem *item, s16 id, s16 modifier, s16 count);
void stored_item_copy(FarmStoredItem *dst, FarmStoredItem *src);
/* The kind of a stored item, by how the player handles it. */
enum StoredItemKind {
    STORED_ITEM_STASH = 0,          /* itemFlags ITEM_FLAG_BIT8 */
    STORED_ITEM_SEED = 1,           /* itemFlags ITEM_FLAG_SEED */
    STORED_ITEM_GOODS = 2,          /* none of the flags below */
    STORED_ITEM_FOOD = 3,           /* itemFlags 0x1800: eaten as it is, or a dish */
    STORED_ITEM_DRINK = 4,          /* itemFlags 0x400 */
    STORED_ITEM_CARRY = 5,          /* carried in both hands: the dog ball, lumber, stone, branch, basket */
};
s32 stored_item_kind(FarmStoredItem *item);
s32 shop_medal_price(s32 item);
s32 item_is_egg(s16 id);
s32 item_is_milk(s16 id);
s32 item_is_wool(s16 id);


/* The picture row an item falls in: ITEM_FLAG_TOOL (tools and seeds), ITEM_FLAG_PICKUP |
 * ITEM_FLAG_BIT8 (pickups), or anything else. */
enum PictureRow {
    PICTURE_ROW_TOOL = 0,
    PICTURE_ROW_PICKUP = 1,
    PICTURE_ROW_OTHER = 2,
};

#endif /* PICTURE_CELLS_H */

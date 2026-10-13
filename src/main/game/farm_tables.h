#ifndef FARM_TABLES_H
#define FARM_TABLES_H

/* The game's static tables and the types of their rows. */

#include <sys/types.h>
#include <libgte.h>
#include "types.h"
#include "items.h"

#define PICTURE_GRID_COUNT     30
#define TILE_RULE_COUNT        17
#define CROP_COUNT             22
#define ESTATE_RECORD_COUNT    197
#define RECIPE_COUNT           96

typedef struct FarmEstateRecord {
    s16 location;   /* the map the item lies in */
    ItemId itemId;
    u16 recordFlags;     /* bit 0 enables the record; 0x4000 marks a claimed slot */
    u16 unk6;
    VECTOR position;
} FarmEstateRecord;

/* FarmTileRule.status. */
enum TileRuleStatus {
    TILE_RULE_NONE = 0,
    TILE_RULE_BLOCKED = 1,
    TILE_RULE_OPEN = 2,
};

/* FarmTileRule.usableBy: one bit per action that may use the row. */
enum TileUsable {
    TILE_USABLE_REQUEST = 0x800,
    TILE_USABLE_TOOL = 0x2000,
    TILE_USABLE_ALT_TOOL = 0x4000,
    TILE_USABLE_HAND = 0x8000,
};

typedef struct FarmTileRule {
    u16 minPower;   /* the tool tier needed to clear the tile */
    u16 minReach;   /* the tool reach needed to clear the tile */
    u16 status;     /* a TileRuleStatus */
    u16 usableBy;   /* an enum TileUsable mask */
} FarmTileRule;

enum Crop {
    CROP_NONE = -1,
    CROP_TURNIP,
    CROP_POTATO,
    CROP_CUCUMBER,
    CROP_STRAWBERRY,
    CROP_CABBAGE,
    CROP_TOMATO,
    CROP_CORN,
    CROP_ONION,
    CROP_PUMPKIN,
    CROP_PINEAPPLE,
    CROP_EGGPLANT,
    CROP_CARROT,
    CROP_SWEET_POTATO,
    CROP_SPINACH,
    CROP_GREEN_PEPPER,
    CROP_GRASS,
    CROP_MOON_DROP,
    CROP_PINK_CAT,
    CROP_BLUE_MAGIC_RED,
    CROP_RED_MAGIC_RED,
    CROP_TOY_FLOWER,
    CROP_ORANGE_CUP,
};

typedef struct FarmCrop {
    u16 season;     /* 0 spring .. 3 winter, 4 any season (grass only) */
    u16 grownStage; /* the tile kind at which the crop is mature */
    s16 regrowStage; /* the tile kind a harvest leaves behind; -1 removes the plant */
    u16 days[4];    /* days spent in each growth stage */
    ItemId harvestItem; /* the item a harvest yields; 0xFFFF for grass, which yields none */
} FarmCrop;

typedef union FlagWord {
    u16 half[2];
    s32 word;
} FlagWord;

typedef s32 (*ItemUseFn)(s8 stamina, s8 fatigue, s16 happiness, s32 unused);

/* ItemInfo.category: the kind of item, which the storage screen and the merges sort by. */
enum ItemCategory {
    ITEM_CATEGORY_CROP = 0,         /* crops and dishes */
    ITEM_CATEGORY_TOOL = 1,         /* tools and unique items */
    ITEM_CATEGORY_SEED = 2,         /* seeds */
    ITEM_CATEGORY_CONSUMABLE = 3,   /* consumables and livestock */
    ITEM_CATEGORY_OBJECT = 4,       /* objects and fish */
};

/* ItemInfo.itemFlags bits. TOOL is set on tools and seeds, SEED on seeds, PICKUP on the
 * field pickups. */
enum ItemInfoFlag {
    ITEM_FLAG_BIT4 = 0x10,
    ITEM_FLAG_SEED = 0x40,
    ITEM_FLAG_BIT8 = 0x100,
    ITEM_FLAG_TOOL = 0x4000,
    ITEM_FLAG_PICKUP = 0x8000,
};

typedef struct ItemInfo {
    u16 itemFlags;     /* an enum ItemInfoFlag, plus the bits it does not name */
    u8  category;   /* an enum ItemCategory */
    s8  crop;       /* row of the crop table, CROP_NONE for none */
    s16 tileRule[5]; /* rows of the tile rule table, -1 for none */
    s16 picture;    /* a row of the picture codes, -1 for none */
    s16 buyPrice;
    s16 sellPrice;
    FlagWord targets; /* all ones means "any target" */
    FlagWord classFlags; /* bits 28..30: the item class */
    ItemUseFn use;
    s8  stamina;    /* the stamina a use adds */
    s8  fatigue;    /* the fatigue a use adds */
    s16 plantAnimation; /* 0..7: the animation a seed item plants with */
    s16 halfWidth;  /* half-extent for hit tests */
    s16 halfDepth;  /* half-extent for hit tests, along the second axis */
} ItemInfo;

typedef struct PictureGrid {
    u8  cells[13][13];      /* value n + 1 is part n of the picture; 0 is empty */
    u8  pad[1];
    u16 unkAA;
    u16 unkAC;
    u16 unkAE;              /* 0, 50 or 100 */
} PictureGrid;

/* The recipes: what the kitchen can make. The entries are tried in order, and the first
 * whose needs the player's pick matches is taken; the last entry, the failed dish, ends
 * the table. */

/* The utensils and seasonings a recipe needs, one bit each. */
enum SeasoningFlag {
    SEASONING_FLAG_SUGAR     = 1 << 7,
    SEASONING_FLAG_SALT      = 1 << 8,
    SEASONING_FLAG_VINEGAR   = 1 << 9,
    SEASONING_FLAG_SOY_SAUCE = 1 << 10,
    SEASONING_FLAG_MISO      = 1 << 11,
};

/* What an ingredient, utensil or seasoning adds to the dish's quality. A list
 * ends with a zero id. The lists are shared, which is why the quality of a salad
 * varies with its vegetables. */
typedef struct RecipeBonus {
    ItemId id;      /* ITEM_SUGAR and the other seasonings are wildcards here too */
    s8  points;     /* some ingredients cost quality */
    u8  pad3;
} RecipeBonus;

typedef struct Recipe {
    u32 nameOffset; /* the dish's name, as an offset into text_bank.messages */
    ItemId id;      /* the dish made; ITEM_FOOD_FIASCO ends the table */
    u8  quality;    /* the dish's base quality */
    u8  pad07;
    u16 tools;      /* UTENSIL_FLAG_ and SEASONING_FLAG_ bits that must be set */
    ItemId ingredients[8]; /* ITEM_ANY_* wildcards are allowed; 0 is an unused slot */
    u8  pad1A[2];
    RecipeBonus *bonuses;
} Recipe;
/* The horse name at byte offset `offset` into horse_names. */
#define HORSE_NAME(offset) ((u16 *)((u8 *)horse_names + (offset)))

void farm_tables_register(s32 skipRuns);
void farm_entry_args_clear(void);
void *farm_work_sector_get(void);
void farm_work_sector_release(void);

#endif /* FARM_TABLES_H */

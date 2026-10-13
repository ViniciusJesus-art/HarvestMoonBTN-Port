#ifndef SCENE_H
#define SCENE_H

/* The current scene: its rebuild, lights, camera and zones. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "game_types.h"
#include "model.h"

struct FarmResultPair;

/* One slot of a VRAM-copy animation table. A free slot has 0xFFFF in `id`. With
 * bit 15 of the id the slot steps through script frames; without it the image
 * scrolls vertically within itself. */
typedef struct FarmImageSlot {
    u16 id;                 /* 0xFFFF = free */
    s16 duration;           /* how long the frame shows */
    s16 x;                  /* the destination */
    s16 y;
    u16 next;               /* the next frame; a scroll's ticks per step */
    u16 timer;              /* ticks shown; a scroll's offset */
    u16 count;              /* a scroll's tick count */
    RECT image;             /* the source */
} FarmImageSlot;

/* One frame of the zone script's VRAM-copy animations. */
typedef struct FarmBlitFrame {
    u16 id;                 /* FarmImageSlot.id; 0xFFFF ends the queue */
    s16 duration;
    u16 next;
    s16 x;
    s16 y;
    RECT image;
} FarmBlitFrame;

/* The place codes: a FarmZoneHeader's `place` (.value the map, .location the area) and
 * the place of every actor. */
enum Place {
    PLACE_FARM = 1,                 /* the outdoor farm, and the outdoor flock */
    PLACE_2 = 2,
    PLACE_3 = 3,
    PLACE_4 = 4,
    PLACE_5 = 5,
    PLACE_GREENHOUSE = 6,           /* the greenhouse */
    PLACE_8 = 8,
    PLACE_9 = 9,
    PLACE_B = 0xB,
    PLACE_C = 0xC,
    PLACE_D = 0xD,
    PLACE_E = 0xE,
    PLACE_F = 0xF,
    PLACE_13 = 0x13,
    PLACE_14 = 0x14,
    PLACE_18 = 0x18,
    PLACE_19 = 0x19,
    PLACE_1A = 0x1A,
    PLACE_1B = 0x1B,
    PLACE_1C = 0x1C,
    PLACE_1D = 0x1D,
    PLACE_1E = 0x1E,
    PLACE_1F = 0x1F,
    PLACE_22 = 0x22,
    PLACE_27 = 0x27,
    PLACE_28 = 0x28,
    PLACE_2A = 0x2A,
    PLACE_2C = 0x2C,
    PLACE_30 = 0x30,
    PLACE_33 = 0x33,
    PLACE_34 = 0x34,
    PLACE_35 = 0x35,
    PLACE_EVENT_CELL = 0x36,        /* a villager moved by an event */
    PLACE_MINE = 0x37,              /* a mine map */
    PLACE_38 = 0x38,
    PLACE_ANY = 0x3F,               /* the wildcard place of an event that fires anywhere */
    PLACE_42 = 0x42,
    PLACE_MINE_43 = 0x43,           /* the other mine map */
    PLACE_44 = 0x44,
    PLACE_48 = 0x48,
};

/* FarmZoneHeader.flags. */
enum AreaFlag {
    AREA_FLAG_BIT12 = (1 << 12),
    AREA_FLAG_BIT15 = (1 << 15),
};

/* FarmZoneHeader.scene. The house areas come in one set per season and house level;
 * AREA_MILESTONE_RESULT is the scene that shows the milestone result. */
enum AreaScene {
    AREA_HOUSE0_SPRING = 0x50,
    AREA_HOUSE0_SUMMER = 0x51,
    AREA_HOUSE0_FALL = 0x52,
    AREA_HOUSE0_WINTER = 0x53,
    AREA_HOUSE1_SPRING = 0x54,
    AREA_HOUSE1_SUMMER = 0x55,
    AREA_HOUSE1_FALL = 0x56,
    AREA_HOUSE1_WINTER = 0x57,
    AREA_HOUSE2_SPRING = 0x58,
    AREA_HOUSE2_SUMMER = 0x59,
    AREA_HOUSE2_FALL = 0x5A,
    AREA_HOUSE2_WINTER = 0x5B,
    AREA_MILESTONE_RESULT = 0xE5,
};

/* A zone header's resourceId and sceneId when the area has none. */
#define SCENE_NONE 999

/* The scene ids of the farm's buildings and mine entrances; the season is added. */
#define HOUSE_SCENE_BASE 0x50
#define BARN_SCENE_BASE 0x60
#define COOP_SCENE_BASE 0x70
#define MINE_37_SCENE_BASE 0x44
#define MINE_43_SCENE_BASE 0x48

/* The box the entry markers must project into: x from -100 to 420, y from -100 to 340. */
#define SCREEN_BOX_LEFT -100
#define SCREEN_BOX_WIDTH 521
#define SCREEN_BOX_TOP -100
#define SCREEN_BOX_BOTTOM 341

/* The head of an area's data, and a copy of the previous area's head. */
typedef struct FarmZoneHeader {
    LimitPair place;        /* .value the map, .location the area */
    s32 scene;              /* an enum AreaScene */
    u16 flags;              /* AREA_FLAG_* */
    s16 resourceSet;
    s16 resourceId;         /* SCENE_NONE = none */
    s16 sceneId;            /* SCENE_NONE = none */
} FarmZoneHeader;

/* x, y, z and a pad half, read unsigned. */
typedef struct USVector {
    u16 vx, vy;
    u16 vz, pad;
} USVector;

/* FarmZoneModel.flags. */
enum SceneEntryFlag {
    SCENE_ENTRY_SLOT_MASK = 0xF,    /* the entry's slot number */
    SCENE_ENTRY_SLOTTED   = (1 << 8),
    SCENE_ENTRY_MARKED    = (1 << 4),
    SCENE_ENTRY_TILT      = (1 << 5),   /* the camera tilt object */
    SCENE_ENTRY_PAN       = (1 << 6),   /* the camera pan object */
    SCENE_ENTRY_LINK      = (1 << 7),
    SCENE_ENTRY_TWIN      = (1 << 10),  /* a door with a mirrored twin model */
    SCENE_ENTRY_HIDDEN    = (1 << 12),
    SCENE_ENTRY_SPIN      = (1 << 13),  /* spins by its spin each frame */
    SCENE_ENTRY_MORPH     = (1 << 14),
    SCENE_ENTRY_ABSENT    = (1 << 15),  /* a free entry */
};

/* The scene models of an area's data, and the scene entries made from them. */
#define SCENE_MODEL_COUNT 15
/* The offset of the model names in a scene's section buffer. */
#define SECTION_STRINGS_OFFSET 0x2000

/* One of the scene models of an area's data. */
typedef struct FarmZoneModel {
    s32 flags;              /* SCENE_ENTRY_* */
    s32 name;               /* the model's file name, an offset into the A-file's string section */
    union {
        s32 frames[2];      /* a morph's other two key frames */
        s16 twin;           /* a door's twin model index */
    } link;
    u16 steps;              /* a morph's step count */
    u8  pad12[0x14 - 0x12];
    SVECTOR position;
    USVector angle;
    USVector spin;
} FarmZoneModel;

/* A point in zone space. */
typedef struct ZonePoint {
    s16 x;
    s16 y;
    s16 z;
} ZonePoint;

/* FarmZone.event: the zone has no event script. */
#define ZONE_EVENT_NONE 0x63

/* FarmZone.id: an empty slot, and a zone that needs no reset. */
#define ZONE_ID_EMPTY 0xFFFF
#define ZONE_ID_NO_RESET 0xFFFE

/* The zones of an area's data. */
#define FARM_ZONE_COUNT 50

/* FarmZone.flags. ZONE_FLAG_DEAD is the set of bits that keep a zone out of the
 * walk-in search. */
enum ZoneFlag {
    ZONE_FLAG_BIT3       = (1 << 3),
    ZONE_FLAG_MARKER     = (1 << 7),   /* sets the player's marker when entered on interact */
    ZONE_FLAG_TRIGGER    = (1 << 8),   /* fires trigger mode 7 when entered on interact */
    ZONE_FLAG_HAS_EVENT  = (1 << 9),
    ZONE_FLAG_BIT13      = (1 << 13),
    ZONE_FLAG_ACTIVE     = (1 << 14),
    ZONE_FLAG_FACING     = (1 << 12),  /* gated on the facing it is entered from */
    ZONE_FLAG_DEAD       = (ZONE_FLAG_BIT3 | ZONE_FLAG_BIT13 | ZONE_FLAG_ACTIVE),
};

/* One of the fifty trigger zones of an area's data. */
typedef struct FarmZone {
    u16 flags;              /* ZONE_FLAG_* */
    u16 id;                 /* 0xFFFF: an empty slot, 0xFFFE: no reset needed; otherwise the zone's kind */
    s16 param;
    u16 storyFlag;             /* story flag */
    s16 facing;             /* the facing it must be entered from, 999 = any */
    s16 event;              /* ZONE_EVENT_NONE = none */
    s16 modelIndex;
    ZonePoint min;
    ZonePoint max;
    u16 targetX;
    u16 targetY;
    u16 targetZ;
    u8  pad20[0x26 - 0x20];
    ZonePoint entry;        /* where an exit zone puts the player in the next scene */
} FarmZone;

/* An area's data: the head of the area's file, refreshed whole on every area change. */
typedef struct FarmModeScript {
    FarmZoneHeader head;
    FarmZone zones[FARM_ZONE_COUNT];
    FarmBlitFrame frames[8];    /* the VRAM-copy animations */
    u16 clampMin[4];
    u16 clampMax[4];
    FarmZoneModel models[SCENE_MODEL_COUNT];
} FarmModeScript;

/* The whole block as bytes, for copying at once. */
typedef struct FarmModeScriptImage {
    u8 bytes[0xBDC];
} FarmModeScriptImage;

/* The slot table: eight VRAM-copy animation slots. */
typedef struct FarmSlotTable {
    u8 pad0[0x4];
    FarmImageSlot entries[8];
} FarmSlotTable;

/* A textured-quad packet of the farm field's TMD. */
typedef struct FarmTmdQuad {
    u32 header;             /* bit 3 marks a cell */
    u8 u0;
    u8 v0;
    u16 cba;
    u8 u1;
    u8 v1;
    u16 tsb;
    u8 u2;
    u8 v2;
    u16 pad0E;
    u8 u3;
    u8 v3;
} FarmTmdQuad;

/* FarmZoneEntry.id: the door's state. */
enum FarmZoneEntryId {
    FARM_ZONE_FREE = 0,
    FARM_ZONE_CLOSED = 1,
    FARM_ZONE_OPENING = 2,
    FARM_ZONE_HELD_OPEN = 3,
    FARM_ZONE_CLOSING = 4,
};

/* One of the farm's zone entries. */
typedef struct FarmZoneEntry {
    u16 id;                 /* an enum FarmZoneEntryId; a slot is free when 0 */
    u16 facing;
    u16 holdTimer;          /* incremented by 5 each trigger */
    u8  pad6[0x8 - 0x6];
    s32 swingSpeed;
    s16 turnX;
    s16 turnY;
    s16 turnZ;
    u8  pad12[0x14 - 0x12];
    FarmZone *zone;         /* the zone this entry belongs to */
} FarmZoneEntry;

/* The overview camera's steps. */
enum FarmOverviewCameraStep {
    OVERVIEW_CAMERA_SET_UP = 0,
    OVERVIEW_CAMERA_RETEXTURE = 1,
    OVERVIEW_CAMERA_MOVE = 2,
    OVERVIEW_CAMERA_FADE_OUT = 3,
};

/* The overview camera's state: a step, a drift, and the camera's position. */
typedef struct FarmOverviewCamera {
    u16 step;         /* an enum FarmOverviewCameraStep; 0 at start */
    u8  pad002[2];
    s32 driftSpeed;   /* decreases by 4 each frame */
    s32 driftY;       /* accumulates driftSpeed */
    s32 cameraX;         /* camera x, -999..10000 */
    u8  pad010[4];
    s32 cameraZ;         /* camera z, -7399..600 */
} FarmOverviewCamera;

/* One rectangular zone of the x/z plane and the result it yields. */
typedef struct FarmZoneBox {
    s32 x_min;
    s32 z_min;
    s32 x_max;
    s32 z_max;
    u16 kind;
    u16 result_kind;
    s32 result0;
    s32 result2;
} FarmZoneBox;

typedef struct FarmZoneRow {
    FarmZoneBox zones[2];
} FarmZoneRow;
void scene_exit_clear(void);
void scene_ground_tolerance_set(void);
void scene_daily_announce(void);
s32 scene_resolve_selection(s32 selection);
void scene_enter(s32 enter, s32 arg1);
void scene_entries_clear(void);
void scene_populate_objects(void);
void scene_entry_set_position(s16 index, const u16 *src);
void scene_entry_set_turn(s16 index, const s16 *src);
void scene_spin_entries(void);
void scene_morphs_clear(void);
s32 scene_place_mismatch(s16 *p);
void farm_copy_words(u32 *dst, const u32 *src, u32 count);
void farm_copy_halfwords(u16 *dst, const u16 *src, u32 count);
s32 farm_layout_index(void);
s32 scene_pan_is_held(void);
s32 scene_tilt_is_held(void);
void scene_change_area(u16 area, s32 unused, u16 cached);

/* FarmMorph.flags. */
enum MorphFlag {
    MORPH_FLAG_FREE = (1 << 15),
};

/* The morphs a scene can run at once. */
#define SCENE_MORPH_COUNT 3

/* A morphing model: two key frames blended into the one that is drawn. */
typedef struct FarmMorph {
    Model links[3];         /* from, to, and the model drawn */
    u16 x;
    u16 y;
    u16 z;
    u8 pad1E[0x20 - 0x1E];
    SVECTOR *from;
    SVECTOR *to;
    SVECTOR *out;
    u16 flags;              /* MORPH_FLAG_FREE */
    s16 step;
    s16 steps;
    u8 pad32[0x34 - 0x32];
} FarmMorph;

/* A scene entry: its model, placement, spin, and the morph it draws instead
 * when SCENE_ENTRY_MORPH is set. */
typedef struct FarmSceneModel {
    Model model;
    SVECTOR position;
    USVector angle;         /* vy and vz are the camera pans' offsets */
    USVector spin;          /* added to the angle each frame with SCENE_ENTRY_SPIN */
    USVector turn;
    u16 flags;              /* SCENE_ENTRY_* */
    u8 pad2A[0x2C - 0x2A];
    FarmMorph *morph;
} FarmSceneModel;

/* FarmZone.id: the kind of a farm zone, the action the player takes on it
 * and the result a thrown item gets from it. */
enum ZoneKind {
    ZONE_OPTIONS = 1,               /* the options menu */
    ZONE_PICK_MENU = 2,             /* the pick menu, unless a request is busy */
    ZONE_MESSAGE_MACHINE = 3,
    ZONE_STORAGE_7 = 4,             /* item storage screen, state 7 */
    ZONE_COOKING_MENU = 5,
    ZONE_PENDING_OP = 6,            /* starts the pending estate operation */
    ZONE_CALENDAR = 8,
    ZONE_FODDER_STORE = 9,          /* takes fodder, or puts it back */
    ZONE_FISH_FOOD_STORE = 10,      /* takes fish food */
    ZONE_KIND_12 = 12,
    ZONE_LUMBER_STOCK = 13,
    ZONE_EGG_BOX = 14,              /* takes an egg */
    ZONE_WOOL_BOX = 15,             /* takes wool */
    ZONE_VALUE_MENU = 16,           /* a menu of the zone's own value */
    ZONE_LUMBER_SPOT = 18,          /* lumber is used here (AREA_HOUSE2_WINTER) */
    ZONE_REQUEST_BOX = 20,          /* the estate request */
    ZONE_FEE_DESK = 21,             /* the fee shop dialogue */
    ZONE_STORAGE_6 = 22,            /* item storage screen, state 6 */
    ZONE_STORAGE_4 = 23,            /* item storage screen, state 4 */
    ZONE_COOKING_RECORDS = 24,
    ZONE_CHICKEN_FEED_STORE = 26,   /* takes chicken feed, or puts it back */
    ZONE_MARRIAGE_MENU = 30,        /* a menu whose text follows the marriage flags */
    ZONE_DAY_TIER_MENU = 31,        /* a menu of the day's tier */
    ZONE_ITEM_CHECK = 32,
    ZONE_MILK_BOX = 33,             /* takes milk */
    ZONE_SHIPPING_BOX = 34,         /* submits the held item */
    ZONE_ENDING_TRACK = 35,
    ZONE_DELIVERY = 36,             /* delivers the held item to the villager at the target */
    ZONE_SHOP_COUNTER = 37,
    ZONE_EGG_TARGET = 38,           /* a thrown egg lands here */
    ZONE_EVENT_TRIGGER = 39,        /* fires event trigger mode 7 */
    ZONE_COOP_FEED_BIN = 40,        /* the coop's feed bin takes chicken feed */
    ZONE_BARN_FEED_BIN = 41,        /* the barn's feed bin takes fodder */
    ZONE_KIND_42 = 42,
    ZONE_KIND_43 = 43,
    ZONE_GIFT_TARGET = 44,          /* a thrown gift lands here (event mode 4) */
    ZONE_POND = 45,                 /* takes a fish from the pond */
    ZONE_HONEY = 47,                /* takes the honey once */
    ZONE_TREASURE = 48,             /* gives its item once */
    ZONE_KIND_49 = 49,
};

typedef struct ResultClampBounds {
    s16 firstMin;           /* the lower bound of result->first */
    s16 unk2;               /* the middle of the low set: stored, never read */
    s16 secondMax;          /* the upper bound of result->second */
    u16 pad6;
    s16 firstMax;           /* the upper bound of result->first */
    s16 unkA;               /* the middle of the high set: stored, never read */
    s16 secondMin;          /* the lower bound of result->second */
} ResultClampBounds;

/* FarmTmdQuad.header: the bit that marks a cell of the farm grid. */
#define FARM_QUAD_CELL_BIT 0x8

#endif /* SCENE_H */

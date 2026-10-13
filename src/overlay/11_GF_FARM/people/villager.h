#ifndef VILLAGER_H
#define VILLAGER_H

/* The villagers: their records, birthdays and schedules. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "actor.h"
#include "anim.h"
#include "game_types.h"

struct FarmPlayer;

/* A villager's birthday: two halfwords copied as one 2-aligned unit. */
typedef struct FarmBirthday {
    s16 season;
    s16 day;
} FarmBirthday;

/* The index of a villager record. It is also the record's name number. */
enum Character {
    CHARACTER_UNNAMED_0      = 0,
    CHARACTER_JEFF           = 1,
    CHARACTER_SASHA          = 2,
    CHARACTER_KAREN          = 3,   /* a bachelorette */
    CHARACTER_SAIBARA        = 4,
    CHARACTER_GRAY           = 5,   /* Mary's rival groom */
    CHARACTER_DOUG           = 6,
    CHARACTER_ANN            = 7,   /* a bachelorette */
    CHARACTER_DOCTOR         = 8,   /* Elli's rival groom */
    CHARACTER_ELLI           = 9,   /* a bachelorette */
    CHARACTER_PASTOR_CARTER  = 10,
    CHARACTER_CLIFF          = 11,  /* Ann's rival groom */
    CHARACTER_BASIL          = 12,
    CHARACTER_ANNA           = 13,
    CHARACTER_MARY           = 14,  /* a bachelorette */
    CHARACTER_BARLEY         = 15,
    CHARACTER_MAY            = 16,
    CHARACTER_MAYOR_THOMAS   = 17,
    CHARACTER_LILLIA         = 18,
    CHARACTER_RICK           = 19,  /* Karen's rival groom */
    CHARACTER_POPURI         = 20,  /* a bachelorette */
    CHARACTER_DUKE           = 21,
    CHARACTER_MANNA          = 22,
    CHARACTER_ELLEN          = 23,
    CHARACTER_GOTZ           = 24,
    CHARACTER_KAI            = 25,  /* Popuri's rival groom */
    CHARACTER_UNNAMED_26     = 26,
    CHARACTER_UNNAMED_27     = 27,  /* the scratch record event actors borrow */
    CHARACTER_HARRIS         = 28,
    CHARACTER_KANO           = 29,
    CHARACTER_GOURMET        = 30,
    CHARACTER_LOUIS          = 31,
    CHARACTER_GREG           = 32,
    CHARACTER_UNNAMED_33     = 33,
    CHARACTER_STU            = 34,
    CHARACTER_WON            = 35,
    CHARACTER_CHILD          = 36,  /* the player's baby */
    CHARACTER_COUNT          = 37,  /* the number of villager records: 0..36 */
    CHARACTER_PLAYER         = 41,  /* the player: an actor id, outside the villager array */
    CHARACTER_ALL_VILLAGERS  = 42,  /* every record 1..36: an actor id, outside the villager array */
};

typedef s16 CharacterId;

/* One 8-byte step of a villager's day schedule. When the clock reaches `time`
 * the villager takes the step: its place becomes the step's, and its route row
 * and destination come from `route`. */
typedef struct VillagerStep {
    s16 time;           /* minute of the day it starts */
    u16 route;          /* low byte: the route row in the farm grid;
                         * high byte: the destination */
    LimitPair place;    /* the map and area it is spent in */
} VillagerStep;

#define VILLAGER_SCHEDULE_STEPS 30
/* The most friendship a villager has: friendship runs 0..FRIENDSHIP_MAX. */
#define FRIENDSHIP_MAX 255

/* A facing. The d-pad sets it: DOWN, LEFT, UP, RIGHT. FACING_ANY on a zone or a hit
 * matches the player whichever way he faces. */
enum Facing {
    FACING_DOWN  = 0,
    FACING_LEFT  = 1,
    FACING_UP    = 2,
    FACING_RIGHT = 3,
    FACING_ANY   = 5,
};

/* FarmVillager.flags. */
enum VillagerFlag {
    VILLAGER_FLAG_EVENT_PENDING = (1 << 0),
    VILLAGER_FLAG_EVENT_STARTED = (1 << 1),
    VILLAGER_FLAG_BIT2         = (1 << 2),
    VILLAGER_FLAG_WEDDING_ROLE = (1 << 3),  /* on the groom and bride records of a wedding */
    VILLAGER_FLAG_BIT4         = (1 << 4),
    VILLAGER_FLAG_BIT5         = (1 << 5),
    VILLAGER_FLAG_BIT6         = (1 << 6),
    VILLAGER_FLAG_BIT7         = (1 << 7),
    VILLAGER_FLAG_ON_MAP       = (1 << 8),
    VILLAGER_FLAG_TALKED_TODAY = (1 << 9),
    VILLAGER_FLAG_BIT10        = (1 << 10),
    VILLAGER_FLAG_PRESENT      = (1 << 11),
    VILLAGER_FLAG_IN_EVENT     = (1 << 12),
    VILLAGER_FLAG_BIT14        = (1 << 14),
    VILLAGER_FLAG_JUST_RESET   = (1 << 15),
    VILLAGER_FLAG_RESET_KEEP   = (VILLAGER_FLAG_WEDDING_ROLE | VILLAGER_FLAG_BIT6 | VILLAGER_FLAG_TALKED_TODAY | VILLAGER_FLAG_BIT10 | VILLAGER_FLAG_PRESENT | VILLAGER_FLAG_IN_EVENT | VILLAGER_FLAG_JUST_RESET),
};

/* FarmVillager.placeFlags. */
enum VillagerPlaceFlag {
    VILLAGER_PLACE_EVENT_CELL = (1 << 12),
    VILLAGER_PLACE_KEPT_PLACE = (1 << 13),
    VILLAGER_PLACE_CURSOR_ALT      = (1 << 14),
    VILLAGER_PLACE_NO_MARKER  = (1 << 15),  /* the marker is not drawn while set */
};

/* FarmVillager.action and prevAction: what a villager is doing. Setting an action also
 * sets its animation and lock countdown. */
enum VillagerAction {
    VILLAGER_ACTION_NONE     = 0,   /* a cleared record: at reset, and at a route's end before it stands */
    VILLAGER_ACTION_STANDING = 1,   /* at rest on a cell: a route row's end, a standing cell, a new row */
    VILLAGER_ACTION_WALKING  = 2,   /* walking its route, or waiting at a cell for its wait ticks */
    VILLAGER_ACTION_TALKING  = 3,   /* greeted by the player: stands 150 ticks, its delay counts up */
    VILLAGER_ACTION_EVENT    = 5,   /* at an event cell: stays until it walks off the door cell */
};

/* FarmCell.kind: what a route cell does. */
enum FarmCellKind {
    ROUTE_CELL_STAND = 0,           /* the route ends: the villager stands */
    ROUTE_CELL_WAIT_WALK = 1,       /* waits cell->id ticks, then walks on */
    ROUTE_CELL_ENTER = 2,           /* walks on (a jump's target) */
    ROUTE_CELL_EVENT = 3,           /* an event cell: starts the villager's event */
    ROUTE_CELL_SCHEDULE = 4,        /* the next schedule step: a new route row */
    ROUTE_CELL_ENTER_FLAGGED = 5,   /* walks on and sets VILLAGER_FLAG_BIT14 */
    ROUTE_CELL_WAIT_WALK_2 = 6,     /* as ROUTE_CELL_WAIT_WALK */
    ROUTE_CELL_KEPT_PLACE = 7,      /* walks on; the villager's position is kept if VILLAGER_PLACE_KEPT_PLACE is set */
    ROUTE_CELL_SHIPMENT = 9,        /* the shipment box cell */
    ROUTE_CELL_STEP = 0x10,         /* advances one column and walks on */
    ROUTE_CELL_EVENT_2 = 0x11,      /* as ROUTE_CELL_EVENT */
    ROUTE_CELL_EVENT_FLAGGED = 0x12, /* as ROUTE_CELL_EVENT, and sets VILLAGER_PLACE_EVENT_CELL */
    ROUTE_CELL_STAND_2 = 0xFFFF,    /* as ROUTE_CELL_STAND */
    ROUTE_CELL_JUMP = 0x8000,       /* jumps to the column in cell->id */
};

#define ROUTE_ROW_CELLS 17

/* A villager's route grid: ROUTE_ROW_CELLS cells a row, one row per route. A villager runs
 * the cells of its row in turn, starting from its column. */
typedef struct FarmCell {
    u16 kind;  /* a FarmCellKind */
    s16 id;    /* the event, the ticks to wait, or the column a jump cell goes to */
    u16 pose;  /* the villager's facing */
    s16 x;
    s16 y;
    s16 z;
} FarmCell;

typedef FarmCell FarmRouteRow[ROUTE_ROW_CELLS];

/* One villager. A villager walks its schedule: each step names a row of the farm grid,
 * and the villager runs that row's cells from `column`. Not a FarmActor. */
typedef struct FarmVillager {
    CharacterId id;     /* 名前ＮＯ, the name number (its index); -1 = a free record */
    s16 friendship;     /* 0..255, 友好度 */
    u16 affection;      /* 0..0xFFFF, 愛情度 (the girls' hearts) */
    s16 facing;         /* 0..3, 向き; also the pose a route cell sets */
    s16 lockCount;      /* 操作禁止ＤＣ: a countdown (-2: none, -3: held); at -1 the next cell runs */
    s16 delay;          /* Ｓ遅延時間: talk ticks, taken off the next step's time */
    s16 column;         /* ＬＳ　ＮＯ: its cell in the route row */
    s16 row;            /* the route row */
    s16 wait;           /* ticks to stand at the cell */
    s16 talkCooldown;   /* ticks before it can be talked to */
    s16 destination;    /* 目的地 */
    s16 prevDestination; /* 前回の目的地 */
    u16 unused18;
    u16 action;         /* 行動: a VillagerAction */
    u16 prevAction;     /* 昔の行動: a VillagerAction */
    u16 flags;          /* フラグ: VILLAGER_FLAG_* */
    u16 placeFlags;     /* VILLAGER_PLACE_* */
    u16 step;           /* ＬＳ　時間: the schedule step it is on */
    FarmBirthday birthday;
    s32 scriptOffset;
    LimitPair place;    /* 場所: .value the map (大域), .location the area (地域) */
    VECTOR position;    /* Ｘ/Ｙ/Ｚ座標 */
    VECTOR fixedPosition;   /* position in 16.16 */
    VECTOR speed;       /* スピードＸ/Ｙ/Ｚ, 16.16 */
    VECTOR ePosition;   /* ＥＸ/ＥＹ/ＥＺ座標 */
    VECTOR eSpeed;      /* ＥスピードＸ/Ｙ/Ｚ */
    AnimState sprite80; /* a second sprite state, copied over `sprite` for every record */
    AnimState sprite;
    ItemCue cue;
    VillagerStep schedule[VILLAGER_SCHEDULE_STEPS];
} FarmVillager;

void farm_item_history_resume(void);
s32 villager_load_request(s32 id, AnimState *dst);
void villager_reset_all(void);
void villager_clear_delays(void);
void villager_clear_all(void);
void villager_clear_flag_bit4_all(void);
s32 villager_is_bachelorette(CharacterId villager);
void family_flags_update(void);
void player_records_tick(void);
s32 player_start_nearby_actions(struct FarmPlayer *player);
void villager_affection_decay(void);
void farm_time_event_tick(void);

extern s32 one_minute;
/* Villager speeds outdoors and indoors, and a position correction. */
extern s32 villager_speed_outdoors;
extern s32 villager_speed_indoors;
extern u32 position_correction;

#endif /* VILLAGER_H */

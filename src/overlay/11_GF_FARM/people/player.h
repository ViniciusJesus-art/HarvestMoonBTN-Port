#ifndef PLAYER_H
#define PLAYER_H

/* The player: input, state, tools, movement and the trail. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "actor.h"
#include "anim.h"
#include "clock.h"
#include "family.h"
#include "game_types.h"
#include "item_storage.h"
#include "villager.h"

/* The animation sampled with each trail point. */
typedef struct TrailPose {
    u16 animation;
    s16 facing;
} TrailPose;

/* Bits in FarmPlayer.latches. */
enum PlayerLatch {
    PLAYER_LATCH_MARKER              = (1 << 2),   /* draw the marker */
    PLAYER_LATCH_FATIGUE_LOCKED      = (1 << 3),   /* fatigue cannot rise */
    PLAYER_LATCH_SPRITE_REQUEST      = (1 << 4),   /* an event's sprite request is queued */
    PLAYER_LATCH_DOOR_ENTRY          = (1 << 5),   /* a door at the farm's north edge is entered */
    PLAYER_LATCH_UNPAUSE                = (1 << 6),
    PLAYER_LATCH_JUMP_PENDING        = (1 << 7),   /* a jump is due */
    PLAYER_LATCH_JUMPING             = (1 << 8),   /* the jump is in its arc */
    PLAYER_LATCH_FATIGUE_MAX         = (1 << 9),   /* fatigue is 100 */
    PLAYER_LATCH_FATIGUE_80          = (1 << 10),  /* fatigue >= 80 */
    PLAYER_LATCH_FATIGUE_50          = (1 << 11),  /* fatigue >= 50 */
    PLAYER_LATCH_STAMINA_EMPTY       = (1 << 12),  /* stamina is 0 */
    PLAYER_LATCH_STAMINA_UNDER_10    = (1 << 13),  /* stamina < 1/10 of maxStamina */
    PLAYER_LATCH_STAMINA_UNDER_30    = (1 << 14),  /* stamina < 3/10 of maxStamina */
    PLAYER_LATCH_STAMINA_UNDER_50    = (1 << 15),  /* stamina < 5/10 of maxStamina */
};

/* Bits in FarmPlayer.stateFlags. */
enum PlayerFlag {
    PLAYER_FLAG_STILL             = (1 << 0),   /* not moving */
    PLAYER_FLAG_NO_INPUT          = (1 << 1),   /* the pad is ignored */
    PLAYER_FLAG_AIRBORNE          = (1 << 30),  /* in the air */
    PLAYER_FLAG_BIT2              = (1 << 2),
    PLAYER_FLAG_AUTO_WALK         = (1 << 3),   /* walking forward on its own */
    PLAYER_FLAG_FISHING           = (1 << 4),   /* fishing */
    PLAYER_FLAG_FISHING_LINE      = (1 << 5),   /* the line is out */
    PLAYER_FLAG_ITEM_IN_FLIGHT    = (1 << 6),   /* an item (or the bobber) is thrown */
    PLAYER_FLAG_RIDING_COW        = (1 << 7),   /* riding a cow */
    PLAYER_FLAG_RIDING            = (1 << 8),   /* riding (horse or cow) */
    PLAYER_FLAG_HALF_FATIGUE      = (1 << 9),   /* the held item halves the fatigue cost */
    PLAYER_FLAG_SCENE_CHANGE      = (1 << 10),  /* the farm is changing scene */
    PLAYER_FLAG_ACTION_BANK       = (1 << 11),  /* the next mode loads the action's own sprite bank */
    PLAYER_FLAG_ACTION            = (1 << 12),  /* an action is armed */
    PLAYER_FLAG_ACTION_START      = PLAYER_FLAG_ACTION_BANK | PLAYER_FLAG_ACTION, /* an action starts: its own bank, and armed */
    PLAYER_FLAG_REFRESH_HELD      = (1 << 13),  /* the held item changed */
    PLAYER_FLAG_BIT14             = (1 << 14),
    PLAYER_FLAG_SHOWING_ITEM      = (1 << 15),  /* an item is shown overhead */
    PLAYER_FLAG_BIT16             = (1 << 16),
    PLAYER_FLAG_POSE_RESET_PENDING = (1 << 17), /* the reach was lost; the pose resets once idle */
    PLAYER_FLAG_BIT18             = (1 << 18),
    PLAYER_FLAG_BIT19             = (1 << 19),
    PLAYER_FLAG_EVENT_SPRITE      = (1 << 20),  /* an event drives the player's sprite */
    PLAYER_FLAG_REACHING          = (1 << 21),  /* the player reaches for a target in front */
    PLAYER_FLAG_ASLEEP            = (1 << 22),  /* asleep or collapsed */
    PLAYER_FLAG_SCRIPTED          = (1 << 23),  /* a script or dialogue drives the player */
    PLAYER_FLAG_BIT24             = (1 << 24),
    PLAYER_FLAG_IDLE_POSE         = (1 << 26),  /* at rest in the idle pose */
    PLAYER_FLAG_CARRY_BIG_ITEM    = (1 << 27),  /* carrying a large item */
    PLAYER_FLAG_CARRY_ITEM        = (1 << 28),  /* carrying an item overhead */
    PLAYER_FLAG_MODE_ANIM         = (1 << 29),  /* a timed mode is playing */
    PLAYER_FLAG_BIT31             = (1 << 31),
};

#define PLAYER_TOYS         5
#define TRAIL_LENGTH        10
#define PLAYER_DAY_COUNTS   6
#define PLAYER_BAG_SLOTS    8
#define PLAYER_TOOL_KINDS   5

/* The fatigue limit: fatigue runs 0..PLAYER_FATIGUE_MAX. */
#define PLAYER_FATIGUE_MAX  100

/* The most gold the player can hold; the bound is negative too. */
#define FARM_MONEY_MAX      9999999

/* FarmPlayer.mode. While PLAYER_FLAG_FISHING is set the same numbers mean the fishing steps. */
enum PlayerMode {
    PLAYER_MODE_IDLE = 1,           /* the mode after each action ends */
    PLAYER_MODE_POSE_2 = 2,         /* a pose at facing + 1 (the horse's frame) */
    PLAYER_MODE_SHOW_ITEM = 3,      /* shows the held item overhead; clears PLAYER_FLAG_SHOWING_ITEM */
    PLAYER_MODE_POSE_4 = 4,         /* a pose at facing + 6 or + 9 */
    PLAYER_MODE_SHOW_ITEM_2 = 0x31, /* as PLAYER_MODE_SHOW_ITEM */
    PLAYER_MODE_DROP_ITEM = 9,      /* drops the held item */
    PLAYER_MODE_CHARGE_TOOL = 0x16, /* charging a farm tool; one mode per ToolKind, + 2 * kind */
    PLAYER_MODE_SWING_TOOL = 0x17,  /* the swing after the charge; one mode per ToolKind, + 2 * kind */
    PLAYER_MODE_USE_ANIMAL_TOOL = 0x4D, /* one mode per AnimalToolKind: milker, clippers, brush, bell */
    PLAYER_MODE_REFILL_CAN = 0x52,  /* the watering can refilled from the water source */
    PLAYER_MODE_USE_MEDICINE = 0x51,    /* the animal medicine */
    PLAYER_MODE_USE_POTION = 0x55,  /* a miracle potion */
    PLAYER_MODE_SELL_COIN = 0x33,       /* sells the coin, at the coin sell price */
    PLAYER_MODE_RUCKSACK_UPGRADE = 0x34, /* the rucksack upgrade item: sets rucksackLevel and itemSlots */
    PLAYER_MODE_THROW_ITEM = 0x2F,      /* throws the held item */
    PLAYER_MODE_SELECT_RESULT = 0x38,   /* the selection becomes the result */
    PLAYER_MODE_PLANT_SEED = 0x53,      /* plants a seed item */
    PLAYER_MODE_FALL_ASLEEP = 0x2C,     /* goes to PLAYER_MODE_ASLEEP with the sleep animation */
    PLAYER_MODE_ASLEEP = 0x2D,          /* asleep: wakes when the first input flags or the scripted sleep end */
    PLAYER_MODE_LIE_DOWN = 0x54,        /* lies down into the sleep animation */
    PLAYER_MODE_USE_HELD_ITEM = 0x49,   /* the held item is used, then released */
    PLAYER_MODE_SHIP_BASKET = 0x46,     /* the held item goes into the shipping basket */
    PLAYER_MODE_GO_TO_BED = 0x4A,       /* the sleep sequence, overnight or in an event (EVENT_FLAG_SLEEPING) */
    PLAYER_MODE_BASKET_ADD = 0x6F,      /* adds the held item to the shipping basket */
    PLAYER_MODE_RESULT_STEP = 0x68,     /* steps the result scene */
    PLAYER_MODE_MAKE_CHEESE = 0x59,     /* the held milk becomes cheese */
    PLAYER_MODE_MAKE_YARN = 0x5A,       /* the held wool becomes a yarn ball */
    PLAYER_MODE_MAKE_MAYONNAISE = 0x5B, /* the held egg becomes mayonnaise */
    PLAYER_MODE_FEED_CHICKENS = 0x5C,   /* adds one chicken feed */
    PLAYER_MODE_DELIVER_LUMBER = 0x6A,  /* lumber delivered in AREA_HOUSE2_WINTER: sets EVENT_FLAG_45 */
    PLAYER_MODE_GIFT = 0x6E,            /* the held gift goes to the event */
    PLAYER_MODE_62 = 0x62,              /* stamps the marker where the player faces */
    PLAYER_MODE_DISMOUNT = 0x3A,        /* puts the riding cow or horse down */
    PLAYER_MODE_STEP_SOUND = 0x4F,      /* plays the step sound on a frame of the walk animation */
    PLAYER_MODE_STAMINA_HALF = 0x3D,    /* stamina under 5/10 of the maximum; the action ends */
    PLAYER_MODE_STAMINA_LOW = 0x3E,     /* stamina under 3/10; also set when a tool charge runs past 60 frames */
    PLAYER_MODE_STAMINA_CRITICAL = 0x3F, /* stamina under 1/10 */
    PLAYER_MODE_EXHAUSTED = 0x40,       /* stamina 0, or fatigue 100 */
    PLAYER_MODE_41 = 0x41,
    PLAYER_MODE_FATIGUE_50 = 0x42,      /* fatigue 50 or more */
    PLAYER_MODE_FATIGUE_80 = 0x43,      /* fatigue 80 or more */
    PLAYER_MODE_JUMP_CROUCH = 0x3B,     /* the jump's crouch; then it goes to PLAYER_MODE_JUMP_TAKEOFF */
    PLAYER_MODE_JUMP_TAKEOFF = 0x56,    /* the takeoff: sets PLAYER_LATCH_JUMPING and the rise animation */
    PLAYER_MODE_JUMP_RISE = 0x58,       /* the rising part of a jump, up to the top of the arc */
    PLAYER_MODE_JUMP_HANG = 0x57,       /* the top of the arc */
    PLAYER_MODE_JUMP_LAUNCH = 0x4B,     /* launched upward; then the player turns round and walks */
    PLAYER_MODE_STOCK_LUMBER = 0x5D,    /* lumber stocked in the storage */
    PLAYER_MODE_PLACE_CHICKEN_FEED = 0x5E, /* chicken feed placed in a feed bin */
    PLAYER_MODE_PLACE_FODDER = 0x5F,    /* fodder placed in a feed bin */
    PLAYER_MODE_ITEM_CHECK = 0x61,      /* a held item the record accepts */
    PLAYER_MODE_ADD_CHICKEN_FEED = 0x64, /* adds one chicken feed to the estate */
    PLAYER_MODE_ADD_FODDER = 0x65,      /* adds one fodder to the estate */
    PLAYER_MODE_SUBMIT_ITEM = 0x66,     /* the submitted item */
    PLAYER_MODE_REQUEST_ITEM = 0x6B,    /* a request for the held item begins */
    PLAYER_MODE_69 = 0x69,
    PLAYER_MODE_SHOW_ITEM_PICKUP = 0x4C, /* shows a picked-up item; then PLAYER_MODE_SHOW_ITEM_2 */
    PLAYER_MODE_USE_FEATHER = 0x32,     /* the blue feather starts its use */
    PLAYER_MODE_EXCHANGE_ITEM = 0xA,    /* after an exchange, the exchanged item is shown */
    PLAYER_MODE_SHOW_TOOL = 0x15,       /* shows a tool from the tool slots */
    PLAYER_MODE_STORE_ITEM = 0xC,       /* the carried item goes into the rucksack, or is released when the slots are full */
    PLAYER_MODE_HOLD_OUT_ITEM = 0x30,   /* holds the shown item out to the viewer */
    PLAYER_MODE_CARRY_BIG_ITEM = 0x67,  /* lifts a big item */
    PLAYER_MODE_RIDE = 0x39,            /* riding the horse or cow (PLAYER_FLAG_RIDING) */
    PLAYER_MODE_0D = 0x0D,
    PLAYER_MODE_06 = 0x06,
    PLAYER_MODE_0E = 0x0E,
    PLAYER_MODE_28 = 0x28,
    PLAYER_MODE_FISHING = 0x45,         /* the mode when fishing starts; the first fishing step has the same value */
};

/* One of the five sprites the player carries toys with. */
typedef struct FarmPlayerToy {
    u8 pad00[0x04];
    AnimState sprite;
} FarmPlayerToy;

/* The two slots of FarmPlayer.held. */
enum HeldSlot {
    HELD_TOOL = 0,              /* the tool in hand */
    HELD_ITEM = 1,              /* the item carried */
};

typedef struct FarmPlayer {
    u16 name[NAME_LENGTH];
    s16 stamina;        /* clamped to [0, maxStamina] */
    s16 maxStamina;     /* 0..255; raising it refills stamina */
    s16 fatigue;        /* 0..100 */
    s16 toolCharge;     /* / 20 is the tool's level */
    s16 idleFrames;
    s16 modeState;      /* -2: idle, -3: held; otherwise the frames left in the mode */
    s16 rucksackLevel;  /* 0..2; the item slots are 2 << rucksackLevel. リュックLV */
    s16 itemSlots;      /* the item slots, kept with rucksackLevel */
    CharacterId talkVillager;   /* -1 or the villager record talked to */
    s16 talkAnimal;     /* the animal talked to, 0..10 */
    s16 scriptRunning;  /* 1 while a harvest helper holds the player in its script */
    s16 currentAction;  /* the item being used */
    u16 wokeAt;         /* the minuteOfDay stamp of waking */
    u16 sleptAt;        /* the minuteOfDay stamp of going to bed */
    s16 countdown;
    s16 facingIndex;    /* the index of the step table */
    s16 chickToy;       /* 0/1, the chick's toy: non-zero draws the toys */
    s16 cowReins;       /* 0/1 */
    u16 latches;        /* PLAYER_LATCH_*: states that stay set until cleared */
    u16 facing38;       /* indexes the step tables */
    u16 prevFacing3A;   /* facing38 last frame */
    u16 mode;           /* a PlayerMode (or a FishingStep while PLAYER_FLAG_FISHING is set) */
    s16 previousMode;
    u16 toolExp[PLAYER_TOOL_KINDS];     /* sickle, hoe, ax, hammer, watering can */
    s16 *scriptSteps;      /* 3-halfword steps, ended by 9999 */
    void *target;       /* a record pointer the player acts on */
    s32 targetFlags;
    s32 happiness;      /* 0..0xFFFF; 幸せ指数 */
    s32 gold;           /* 0..FARM_MONEY_MAX; お金 */
    s32 medals;         /* the horse-race medals; コイン */
    s32 scriptHeld;     /* the buttons held that replace the pad while PLAYER_FLAG_SCRIPTED is set */
    s32 scriptPressed;  /* likewise the buttons newly pressed */
    u32 stateFlags;     /* PLAYER_FLAG_* */
    /* Six per-day counters, each capped at 15. */
    s16 counts70[PLAYER_DAY_COUNTS];
    u8  seq7C[0x7E - 0x7C];     /* the sound sequence to play */
    FarmBirthday birthday;      /* season 0..3, day 0..29 */
    u8  pad82[0x84 - 0x82];
    FarmStoredItem held[2];     /* indexed by HeldSlot: HELD_TOOL, HELD_ITEM */
    FarmStoredItem tools[PLAYER_BAG_SLOTS];
    FarmStoredItem items[PLAYER_BAG_SLOTS];
    FarmStoredItem shown;       /* the item being shown or used */
    u8  padF6[0xFC - 0xF6];
    LimitPair place;            /* .value the map, .location the area */
    VECTOR position;
    s32 velocity[3];            /* this frame's step, y the fall speed */
    u8  pad11C[0x120 - 0x11C];
    VECTOR trail[TRAIL_LENGTH];         /* the position history, shifted each frame */
    VECTOR slowTrail[TRAIL_LENGTH];     /* a slower-sampled history */
    AnimState secondSprite;        /* the player's second sprite */
    AnimState sprite;           /* the player's sprite */
    FarmWedding wedding;        /* the player's own wedding */
    ItemCue cue;
    FarmPlayerToy toys[PLAYER_TOYS];
    TrailPose poses[TRAIL_LENGTH];      /* the animation sampled with each slowTrail point */
} FarmPlayer;
void player_frame_update(void);
void player_draw(VECTOR *position, s32 unused);
void player_update_position(void);
void thrown_items_update(void);
s32 player_zone_action(FarmPlayer *player);
void player_show_tool(FarmPlayer *player, s32 use_index, s32 index, s32 state);
void player_start_rucksack(FarmPlayer *player);
void player_load_item_pictures(void);
void player_new_day_reset(void);
void player_stop_motion(void);
void farm_draw_cursor(u_long *ot, s32 pos_x, s32 pos_y, s32 frame);
void player_count_idle(FarmPlayer *player);
s32 player_reset_pose(void);
s32 player_show_item(FarmStoredItem *item);
s32 player_use_held_item(s32 held);
/* The follow-up pose of the receive-item step, by the state it is given. */
enum ReceiveItemState {
    RECEIVE_ITEM_HOLD = 0,          /* put in hand */
    RECEIVE_ITEM_HOLD_2 = 1,        /* put in hand, the same as RECEIVE_ITEM_HOLD */
    RECEIVE_ITEM_EAT = 2,           /* shown with the eat pose */
    RECEIVE_ITEM_DRINK = 3,         /* the drink pose */
};
void player_receive_item(FarmPlayer *player, s32 value, s32 state);
void player_drop_held_item(FarmPlayer *player);
s32 player_can_act(void);
void thrown_items_reset(void);
void player_latch_place(void);
void player_morning_reset(void);
void player_refresh_carry_state(FarmPlayer *player);
void player_end_action(FarmPlayer *player);
void house_door_position(VECTOR *out);
s32 player_fatigue_cost(s32 adjustment);
void player_hourly_status_tick(void);
void player_go_to_sleep(void);
void player_walk_start(s32 frames);
s32 player_init_toys(FarmPlayer *player);
s32 player_reset_trail(FarmPlayer *player);
void player_start_result_scene(FarmPlayer *player);
void player_load_winter_anim(void);

/* FarmThrownItem.phase: where a thrown item is in its flight, its landing and its end. */
enum ThrownItemPhase {
    THROWN_ITEM_FLY = 0,            /* in the air */
    THROWN_ITEM_HOLD = 1,           /* landed with nothing to do: held a moment, then blinks */
    THROWN_ITEM_BLINK = 2,          /* blinks out before it is cleared */
    THROWN_ITEM_LANDED = 3,         /* settled on a target; waits for the landing animation */
    THROWN_ITEM_EGG_LANDED = 4,     /* an egg-group item: waits, then a boiled egg is shown */
};

/* One of the three thrown-item slots: a free slot has kind 0. */
typedef struct FarmThrownItem {
    s16 blinking;       /* 1 once the item starts blinking before it is cleared */
    s16 kind;
    s16 timer;
    u16 phase;
    u16 dir;
    u8 pad0A[2];
    s32 velocityY;
    s32 groundY;
    s32 vx;
    s32 vy;
    s32 vz;
    u8 pad20[4];
    VECTOR position;
} FarmThrownItem;

extern FarmHourEvent hour_events[4];

/* Stamina cost of each tool: a row of five levels per tool. */
extern s16 tool_stamina_costs[5][5];
/* The two farm input words. */
extern s32 farm_buttons_held;
extern s32 farm_buttons_pressed;

#endif /* PLAYER_H */

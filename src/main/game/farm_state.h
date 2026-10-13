#ifndef FARM_STATE_H
#define FARM_STATE_H

/* FarmState, the farm's whole state, and the records it holds. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "anim.h"
#include "animals.h"
#include "clock.h"
#include "estate.h"
#include "event_script.h"
#include "family.h"
#include "farm_tables.h"
#include "field.h"
#include "game_types.h"
#include "harvest_sprites.h"
#include "options_menu.h"
#include "pad.h"
#include "picture_cells.h"
#include "player.h"
#include "scene.h"
#include "shop_dialogues.h"
#include "villager.h"
#include "weather.h"

#define FARM_REQUEST_SLOTS 5

/* The farm overlay image: the load window holds it, and the section buffer follows it. */
#define FARM_IMAGE_SIZE 0x49000

/* One entry of the five-record request table. */
typedef struct FarmRequestSlot {
    s16 value;          /* -1 is the empty sentinel */
    s16 args[5];
} FarmRequestSlot;

/* One of the five pending (id, value) results. */
typedef struct PendingResult {
    s16 id;             /* -1 = free */
    s16 value;
} PendingResult;

/* FarmSystem.menuState: the menu's state machine. */
enum MenuState {
    MENU_STATE_IDLE = 0,
    MENU_STATE_BEGIN_LOAD = 1,
    MENU_STATE_LOAD_RESULT = 2,
    MENU_STATE_OPEN = 3,
    MENU_STATE_RUN = 4,
    MENU_STATE_CLOSE = 5,
    MENU_STATE_FINISH = 6,
};

/* FarmSystem.overnightStep: the end-of-day steps. */
enum OvernightStep {
    OVERNIGHT_FADE_OUT = 0,
    OVERNIGHT_FADE_SETTLED = 1,
    OVERNIGHT_GROW_FARM = 2,
    OVERNIGHT_HOLD = 3,
    OVERNIGHT_CHANGE_SCENE = 4,
    OVERNIGHT_UPDATES = 5,
    OVERNIGHT_FINISH = 6,
};

/* Per-frame system values shared by the menu, the clock and the end-of-day code. */
typedef struct FarmSystem {
    u8  menuState;      /* a MenuState, in one byte */
    u8  workSectorBusy; /* work.sector is only handed out while this is 0 */
    u8  pad02[0x04 - 0x02];
    s16 overnightStep;  /* an enum OvernightStep: the end-of-day step */
    u32 clockFrames;    /* frames counted toward the next game minute */
} FarmSystem;

/* The farm's work buffers, never saved. */
#define FARM_EVENT_SLOTS 100
#define FARM_SPRITES     45

typedef struct FarmWork {
    u8  sector[0x2000];     /* scratch space for the A-file loader; free while workSectorBusy is 0 */
    u8  results[0xCE0];     /* the script results */
    FarmEventEntry events[FARM_EVENT_SLOTS];
    AnimState sprites[FARM_SPRITES];    /* the farm's sprite states */
} FarmWork;

/* The farm's progress, saved as one block. */
typedef struct FarmProgress {
    /* The overlay-entry argument words, which the overlay dispatches on when it loads. */
    u32 entryStep;
    u32 entrySubStep;
    s32 entryArgs[5];
    s32 entryArgs2[5];
    FarmClock clock;        /* the calendar and the time of day */
    FarmEstate estate;
} FarmProgress;

/* The request's step: the event script runs it through start, run and finish, and waits for a load. */
enum FarmRequestState {
    FARM_REQUEST_START = 0,
    FARM_REQUEST_RUN = 1,
    FARM_REQUEST_FINISH = 2,
    FARM_REQUEST_LOAD_WAIT = 3,
};

/* The pending requests. */
typedef struct FarmRequests {
    enum FarmRequestState state;
    FarmRequestSlot items[FARM_REQUEST_SLOTS];
} FarmRequests;

/* The weather, the options and the rival weddings. */
typedef struct FarmWorld {
    s16 weather;            /* today's weather */
    s16 weatherTomorrow;    /* rolled for the next day */
    FarmSettings options;
    FarmWedding rivalWeddings[RIVAL_COUPLE_COUNT];
} FarmWorld;

/* The minigames' results, never saved. */
typedef struct FarmMinigames {
    u16 chickenSkill;
    u16 *textSource;        /* the name the text window shows: an actor name */
    s32 mg2Done;
    s16 mg3Team;            /* the captain's villager id (7, 9, 20, 22), -1 none */
    s16 courseId;
    u32 mg3Done;
    s32  mg4PlayerWon;      /* 1 when the player's dog wins the race */
    s16 mg4Winner;          /* the winning rival's message id */
    s32  mg5PlayerWon;      /* 1 when the player wins the swimming race */
    s16 mg5Slot;
} FarmMinigames;

/* The ten dialogue tracks of the ending and the visit counts of track 9. */
typedef struct FarmEndingTracks {
    u8 stages[10];          /* track 0..9's stage; 0 = not started */
    u8 visits[2];           /* track 9's two branches; each counts up to 2 */
} FarmEndingTracks;

/* The ending. */
typedef struct FarmEnding {
    u16 route;              /* the ending route, 0..16 */
    u8  offers[9];          /* nine offer slots, one per message: 0 free, 1 offered,
                             * 2 accepted, 3 paid; a paid slot becomes 0xFF once advanced */
    u8  pad0B;
    FarmEndingTracks tracks;
} FarmEnding;

/* FarmState.debugFlags: the debug menu's toggles and house choices. */
enum FarmDebugFlag {
    DEBUG_FLAG_DRAW_EVENT_NUMBERS     = (1 << 7),
    DEBUG_FLAG_NO_HARVEST_SPRITES     = (1 << 8),
    DEBUG_FLAG_NO_FIELD_DRAW          = (1 << 9),
    DEBUG_FLAG_NO_BREAKING_ANIMATIONS = (1 << 10),
    DEBUG_FLAG_NO_EVENT_TRIGGERS      = (1 << 11),
    DEBUG_FLAG_NO_EVENTS              = (1 << 12),
    DEBUG_FLAG_NO_ANIMALS             = (1 << 13),
    DEBUG_FLAG_NO_2D_FIXTURES         = (1 << 14),
    DEBUG_FLAG_DRAW_MESSAGE_NUMBERS   = (1 << 15),
    DEBUG_FLAG_WINTER_HOUSE      = (1 << 16),
    DEBUG_FLAG_AUTUMN_HOUSE      = (1 << 17),
    DEBUG_FLAG_SUMMER_HOUSE      = (1 << 18),
    DEBUG_FLAG_SPRING_HOUSE      = (1 << 19),
    DEBUG_FLAG_PUT_TIME                = (1 << 23),
    DEBUG_FLAG_HISPEED_TIME            = (1 << 24),
    DEBUG_FLAG_PEOPLE_PRO              = (1 << 25),
    DEBUG_FLAG_DOOR_SCHEDULE           = (1 << 26),
    DEBUG_FLAG_SCR_LIMIT               = (1 << 27),
    DEBUG_FLAG_MYPOS_PUT               = (1 << 28),
    DEBUG_FLAG_ENTER_ARTR              = (1 << 29),
    DEBUG_FLAG_MUTEKI                  = (1 << 30),
    DEBUG_FLAG_CAMERA_CONTROL          = (1 << 31),
};

/* FarmState.statusFlags. */
enum FarmStatusFlag {
    STATUS_FLAG_DEBUG_EVENTS_300_AND_UP = (1 << 0),
    STATUS_FLAG_NO_TARGET_ACTION              = (1 << 1),
    STATUS_FLAG_COLLAPSED                     = (1 << 2),
    STATUS_FLAG_SKIP_ANIMAL_UPDATE            = (1 << 3),
    STATUS_FLAG_EVENT_HOLDS_RESOURCE_E        = (1 << 4),
    STATUS_FLAG_GIFT_PENDING                  = (1 << 5),
    STATUS_FLAG_STARTUP_PENDING               = (1 << 6),
    STATUS_FLAG_FROM_MG1                      = (1 << 7),
    STATUS_FLAG_FROM_MG2                      = (1 << 8),
    STATUS_FLAG_FROM_MG3                      = (1 << 9),
    STATUS_FLAG_FROM_MG4                      = (1 << 10),
    STATUS_FLAG_FROM_MG5                      = (1 << 11),
    STATUS_FLAG_ASLEEP                 = (1 << 12),
    STATUS_FLAG_TARGET_RELOAD              = (1 << 13),
    STATUS_FLAG_MUSIC_HOLD                    = (1 << 14),
};

/* Any of the five minigame return flags. */
#define STATUS_FLAG_FROM_MG_ANY (STATUS_FLAG_FROM_MG1 | STATUS_FLAG_FROM_MG2 | STATUS_FLAG_FROM_MG3 | STATUS_FLAG_FROM_MG4 | STATUS_FLAG_FROM_MG5)

/* FarmState.globalFlags. */
enum GlobalFlag {
    GLOBAL_FLAG_BIT12            = (1 << 12),
    GLOBAL_FLAG_NEW_GAME         = (1 << 13),
    GLOBAL_FLAG_CONTINUE         = (1 << 14),   /* a saved game is continued, not started */
    GLOBAL_FLAG_DEMO_START       = (1 << 15),
    GLOBAL_FLAG_INPUT_REPLAY     = (1 << 16),
    GLOBAL_FLAG_TRANSITION_BEGIN = (1 << 17),
    GLOBAL_FLAG_TRANSITION_END   = (1 << 18),
    GLOBAL_FLAG_MINIGAME_EXIT    = (1 << 19),
    GLOBAL_FLAG_MINUTE_PASSED    = (1 << 20),
    GLOBAL_FLAG_BIT21            = (1 << 21),
    GLOBAL_FLAG_CAMERA_BUSY      = (1 << 22),
    GLOBAL_FLAG_TRANSITION       = (1 << 23),
    GLOBAL_FLAG_HOUR_PASSED      = (1 << 25),
    GLOBAL_FLAG_PAUSED           = (1 << 27),
    GLOBAL_FLAG_MENU_OPEN        = (1 << 28),
    GLOBAL_FLAG_CLOCK_TICKED     = (1 << 30),
};

typedef struct FarmState {
    u32 unk00;
    u32 unk04;
    u32 frameCount;         /* counts every main frame */
    u32 globalFlags;        /* GLOBAL_FLAG_* */
    u32 debugFlags;            /* DEBUG_FLAG_* */
    s32 lastCaller;         /* the return address of the last traced sprite draw */
    s32 blockIndex;
    InputDemo *demo;
    ItemInfo *itemInfo;     /* the item table */
    PictureGrid *pictureGrids;  /* the picture grid table */
    FarmTileRule *tileRules;    /* the tile rules table */
    FarmCrop *cropTable;    /* the crop table */
    FarmRequestSlot *request;   /* the request being run; its args[0] is the event id */
    u32 (*pictureCodes)[4];
    /* The event flags, one bit each. */
    u32 eventFlags[37];
    FarmSystem system;
    FarmWork work;
    s16 pendingMode;        /* the farm mode waiting to be set; -1 when none */
    PendingResult pendingResults[5];
    u32 statusFlags;          /* STATUS_FLAG_* */
    struct FarmEventEntry *currentEvent;
    CharacterId villagerCursor; /* the villager record the debug page shows, 0..0x24 */
    FarmGridEntry image;    /* the farm image being drawn */
    VECTOR camera;          /* a camera position */
    FarmProgress progress;
    FarmPlayer player;      /* the player */
    FarmTileRow fieldTiles[FIELD_ROWS];
    FarmSlotRow slots7x7[GREENHOUSE_SIZE];
    FarmRequests requests;
    FarmAnimals animals;
    FarmVillager villagers[CHARACTER_COUNT];
    FarmRuns runs;
    FarmHelper helpers[HELPER_COUNT];
    FarmWorld world;
    FarmRaceBets raceBets;
    FarmMinigames minigames;
    FarmDayCounts days;
    FarmDrift drift;
    FarmFamily family;
    FarmEnding ending;
} FarmState;

/* FarmModeGlobal.eventMode: the farm's event state machine. Each value is a stage of a
 * scene change, a sleep or a scene request. */
enum FarmMode {
    FARM_MODE_IDLE = 0,
    FARM_MODE_ZONE_WALK = 1,            /* the player walks onto the zone just entered */
    FARM_MODE_ZONE_WALK_WAIT = 2,       /* the walk runs; it ends idle or in a scene change */
    FARM_MODE_EXIT_LOAD = 3,            /* at the exit: the next area's section starts loading */
    FARM_MODE_CYCLE_AREA = 4,           /* area steps on by one (wrapping at 256), the season follows */
    FARM_MODE_EXIT_ARRIVE = 5,          /* the next area's section loads; then it fades in */
    FARM_MODE_MG_DEBUG_SCREEN = 6,      /* after the fade, scene 0x11 (the MG debug screen) */
    FARM_MODE_SLEEP = 12,               /* fade out, then the player goes to sleep */
    FARM_MODE_ASLEEP = 13,              /* asleep: waits for the fade */
    FARM_MODE_SAVE_SCREEN = 14,         /* after the fade, scene 7 (save) */
    FARM_MODE_LOAD_SCREEN = 15,         /* after the fade, scene 8 (load) */
    FARM_MODE_PDA = 16,                 /* after the fade, scene 9 (PDA) */
    FARM_MODE_LIVESTOCK_TRADE = 17,     /* after the fade, scene 0xA (livestock trade) */
    FARM_MODE_MINIGAME_1 = 18,          /* after the fade, scene 3 (horse race) */
    FARM_MODE_MINIGAME_2 = 19,          /* after the fade, scene 4 (cockfight) */
    FARM_MODE_MINIGAME_3 = 20,          /* after the fade, scene 5 (tomato) */
    FARM_MODE_MINIGAME_4 = 21,          /* after the fade, scene 0xB (dog) */
    FARM_MODE_MINIGAME_5 = 22,          /* after the fade, scene 0xC (swimming race) */
    FARM_MODE_OVERVIEW = 23,            /* the overview camera */
    FARM_MODE_TARGET_LOAD = 24,         /* the target area starts loading */
    FARM_MODE_TARGET_ARRIVE = 25,       /* the target area loads; then it fades in */
    FARM_MODE_DEMO_END = 26,            /* the input replay is spent: fade out to the title */
    FARM_MODE_TO_TITLE = 27,            /* after the fade, scene 0xD */
    FARM_MODE_STAFF_ROLL = 28,          /* after the fade, scene 0x10 (staff roll) */
    FARM_MODE_OVERNIGHT_ASLEEP = 30,    /* asleep at the end of the day: waits for the fade */
    FARM_MODE_OVERNIGHT = 31,           /* the end of the day: fade out, then the player sleeps */
};

/* FarmModeGlobal.view: which view the farm draws and runs. FARM_VIEW_NORMAL runs
 * the event state machine; FARM_VIEW_OVERVIEW is the overview camera. Values 1 and 2
 * both skip the per-frame work. */
enum FarmView {
    FARM_VIEW_NORMAL = 0,
    FARM_VIEW_IDLE_1 = 1,
    FARM_VIEW_IDLE_2 = 2,
    FARM_VIEW_OVERVIEW = 3,
};

/* The farm's working state: the second half of FarmLoadArea. */
typedef struct FarmModeGlobal {
    u8  pad0[0x1C08 - 0x0];
    s32 resourceBuffer[0x680];
    FarmTmdQuad *quads[FIELD_ROWS][FIELD_COLS];     /* the field quad under each cell */
    FarmModeScript script;
    FarmZoneHeader previousHead;    /* the head of the area left last */
    FarmModeScript *data;           /* the current area's data; always points at script */
    FarmZoneHeader *previous;       /* points at previousHead */
    FarmZone *exit;                 /* the zone the player left the last scene by */
    FarmScroll scroll;
    FarmSlotTable slots;
    u8  pad5470[0x5474 - 0x5470];
    LimitPair place;                /* where the player is */
    s32 loadedSlot;
    s32 area;
    s32 transitionTicks;
    s32 enteredArea;
    s32 absentArea;
    s32 view;
    u8  pad5490[0x5498 - 0x5490];
    s32 modeScene;
    s32 currentMode;
    u32 eventMode;                  /* a FarmMode */
    s16 pictureIds[PICTURE_ROWS][PICTURE_SLOTS];
    FarmGridEntry pictureEntries[PICTURE_ROWS][PICTURE_SLOTS];
} FarmModeGlobal;

/* The farm's load buffers, then the working state in `mode`. */
typedef struct FarmLoadArea {
    u8 *sectionBuffer;
    u8 *loadWindow;         /* the load window the title screen reads into */
    u8  anim8[0x1000];      /* the farm-wide animation file */
    u8  sectorBuffer[0x3800];    /* sector staging buffer, a CD destination */
    u8  spanBuffer[0x10000];
    u8  slotAnims[0x6000];
    u8  itemArt[0x2000];
    u8  stageData[0x1400];
    u8  seasonArt[0x1800];
    u8  slotSixArt[0xBF8];
    FarmModeGlobal mode;
} FarmLoadArea;

/* The 18-byte payload loaded into a mode's record. */
typedef struct FarmPayload {
    u8 data[0x10];
    s16 value;
} FarmPayload;

/* FarmEnding.offers: the state of each offer slot. */
enum EndingOfferState {
    ENDING_OFFER_FREE = 0,
    ENDING_OFFER_OFFERED = 1,
    ENDING_OFFER_ACCEPTED = 2,
    ENDING_OFFER_PAID = 3,
    ENDING_OFFER_CLOSED = 0xFF,   /* the paid offer, once advance has taken it */
};

void farm_state_init(void);

/* The farm-sim global state block. It is split into sub-structs:
 * FarmWork, FarmProgress (with FarmClock and FarmEstate),
 * FarmRequests, FarmAnimals, FarmRuns, FarmWorld, FarmMinigames, FarmDayCounts,
 * FarmDrift, FarmFamily and FarmEnding, and every animal and
 * actor record starts with the shared FarmActor head. */
extern FarmState game_state;
extern FarmLoadArea farm_area;

#endif /* FARM_STATE_H */

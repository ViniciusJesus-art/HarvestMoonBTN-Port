/* The save/load screens: choosing a slot, saving and loading the farm to the memory card. */

#include <sys/types.h>
#include <memory.h>
#include <libgte.h>
#include <libgpu.h>
#include <libsnd.h>
#include <libmcrd.h>
#include <libmcx.h>
#include "types.h"
#include "actor.h"
#include "afile.h"
#include "anim.h"
#include "animals.h"
#include "bg_tile.h"
#include "chicken.h"
#include "clib.h"
#include "clock.h"
#include "estate.h"
#include "farm_state.h"
#include "items.h"
#include "livestock.h"
#include "loading_screen.h"
#include "main.h"
#include "menu_window.h"
#include "pad.h"
#include "saveload.h"
#include "screen_fade.h"
#include "sound.h"
#include "sound_request.h"
#include "text.h"
#include "text_bank.h"
#include "texture_pack.h"
#include "ui_icons.h"
#include "villager.h"

/* Memory-card save header: the first block of a save file. */
typedef struct
{
    char magic[2];
    u8 icon_flag;
    u8 blocks;
    char title[0x40];
    u8 reserved[0x1C];
    u16 clut[16];
} SaveHeader;

/* Byte images, copied whole with no alignment assumed. */
typedef struct
{
    u8 bytes[0x20];
} IconClut;

typedef struct
{
    u8 bytes[0x80];
} IconFrame;

/* Byte offsets of the three icon TIMs inside the loaded icon file. */
typedef struct
{
    u32 frame[3];
} IconOffsets;

typedef struct
{
    u8 bytes[0xB4];
} FarmChickenImage;

typedef struct
{
    u8 bytes[0xB8];
} FarmLivestockImage;

typedef struct
{
    u32 words[0xC / 4];
} BlockC;

typedef struct
{
    u32 words[0x14 / 4];
} Block14;

typedef struct
{
    u32 words[0x18 / 4];
} Block18;

typedef struct
{
    u32 words[0x24 / 4];
} Block24;

typedef struct
{
    u32 words[0x30 / 4];
} Block30;

typedef struct
{
    u32 words[0x34 / 4];
} Block34;

typedef struct
{
    u32 words[0x3C / 4];
} Block3C;

typedef struct
{
    u32 words[0x40 / 4];
} Block40;

typedef struct
{
    u32 words[0x74 / 4];
} Block74;

typedef struct
{
    u32 words[0x94 / 4];
} Block94;

typedef struct
{
    u32 words[0xF0 / 4];
} BlockF0;

typedef struct
{
    u32 words[0x108 / 4];
} Block108;

typedef struct
{
    u32 words[0x188 / 4];
} Block188;

typedef struct
{
    u32 words[0x3FC / 4];
} Block3FC;

typedef struct
{
    u32 words[0x6C8 / 4];
} Block6C8;

typedef struct
{
    u32 words[0xCFC / 4];
} BlockCFC;

typedef struct
{
    u32 words[0x2328 / 4];
} Block2328;

typedef struct
{
    u32 words[0x1F34 / 4];
} Block1F34;

/* One villager record in the save file, 14 bytes. */
typedef struct SaveRecord
{
    FarmBirthday birthday;
    u16 friendship;
    u16 affection;
    u16 flags;
    u16 placeFlags;
    u16 step;
} SaveRecord;

typedef struct
{
    FarmAnimals slots;
} Pack800742EC;

typedef struct
{
    Block18 flagWords;
    u8 pad0018[0x0020 - 0x0018];
    s32 itemInfo;
    s32 pictureGrids;
    s32 tileRules;
    s32 cropTable;
    s32 request;
    s32 pictureCodes;
    Block94 eventFlags;
    u8 pad00CC[0x99E8 - 0x00CC];
    FarmVillager villagers[37];
    u8 padDD8C[0xDDCC - 0xDD8C];
    BlockF0 *run16;
    u8 padDDD0[0xDDD4 - 0xDDD0];
    Block30 *run18;
} Pack8006C838;

typedef struct
{
    s32 entryStep;
    s32 entrySubStep;
    u32 entryArgs[0x14 / 4];
    u32 entryArgs2[0x14 / 4];
    s32 clock[5];
    BlockCFC estate;
} Pack80070CC0;

typedef struct
{
    Block3FC player;
} Pack80071A00;

typedef struct
{
    Block2328 fieldTiles;
} Pack80071DFC;

typedef struct
{
    Block188 slots7x7;
} Pack80074124;

typedef struct
{
    Block40 requests;
} Pack800742AC;

typedef struct
{
    Block74 runs;
} Pack8007A5C4;

typedef struct
{
    Block6C8 helpers;
} Pack8007A638;

typedef struct
{
    s32 weatherPair;
    u32 options[0xC / 4];
    Block3C rivalWeddings;
} Pack8007AD00;

typedef struct
{
    Block108 days;
} Pack8007AE78;

typedef struct
{
    Block24 drift;
} Pack8007AF80;

typedef struct
{
    Block34 family;
} Pack8007AFA4;

typedef struct
{
    BlockC endingHead;
    u32 endingTracks[0xC / 4];
} Pack8007AFD8;

typedef struct
{
    FarmAnimals slots;
    SaveRecord records[37];
    u8 pad213A[0x213C - 0x213A];
    BlockF0 run16;
    Block30 run18;
    Block18 flagWords;
    s32 itemInfo;
    s32 pictureGrids;
    s32 tileRules;
    s32 cropTable;
    s32 request;
    s32 pictureCodes;
    Block94 eventFlags;
    s32 entryStep;
    s32 entrySubStep;
    Block14 entryArgs;
    Block14 entryArgs2;
    s32 clock[5];
    BlockCFC estate;
    Block3FC player;
    Block2328 fieldTiles;
    Block188 slots7x7;
    Block40 requests;
    Block74 runs;
    Block6C8 helpers;
    s32 weatherPair;
    BlockC options;
    Block3C rivalWeddings;
    u8 pad60D4[0x6200 - 0x60D4];
    Block108 days;
    Block24 drift;
    Block34 family;
    BlockC endingHead;
    BlockC endingTracks;
} SaveFile;

typedef struct SavedProgress
{
    s32 entryStep;
    s32 entrySubStep;
    Block14 entryArgs;
    Block14 entryArgs2;
    s32 clock[5];
    BlockCFC estate;
} SavedProgress;

typedef struct State8007AD00
{
    s32 weatherPair;
    BlockC options;
    Block3C rivalWeddings;
} State8007AD00;

typedef struct State8007AFD8
{
    BlockC endingHead;
    BlockC endingTracks;
} State8007AFD8;

typedef struct SaveImage
{
    Block1F34 animals;
    SaveRecord records[37];
    u8 pad213A[2];
    BlockF0 run16;
    Block30 run18;
    s32 flagWords[6];
    u8 pad2274[0x2284 - 0x2274];
    FarmRequestSlot *request;
    u32 (*pictureCodes)[4];
    Block94 eventFlags;
    SavedProgress progress;
    Block3FC player;
    Block2328 fieldTiles;
    Block188 slots7x7;
    Block40 requests;
    u8 pad594C[0x59C0 - 0x594C];
    Block6C8 helpers;
    State8007AD00 world;
    u8 pad60D4[0x6200 - 0x60D4];
    Block108 days;
    Block24 drift;
    Block34 family;
    State8007AFD8 ending;
} SaveImage;

/* The loaded file image, copied whole into the working buffer. */
typedef struct
{
    u8 bytes[0x2D74];
} SaveSheet;

static void save_reset_state(void);
static void save_run_step(void);
static void save_begin_message(void);
static void save_probe_slots_pick_screen(void);
static void save_no_card_screen(void);
static void save_probe_result_screen(void);
static void save_read_slot_headers(void);
static void save_slot_select_screen(void);
static void save_overwrite_confirm_screen(void);
static void save_probe_and_check_space(void);
static void save_format_prompt_screen(void);
static void save_format_card(void);
static void save_create_file_step(void);
static void save_build_header(void);
static void save_write_file_step(void);
static void save_confirm_result(void);
static void save_abort(void);
static void save_done_screen(void);
static void load_run_step(void);
static void load_begin_message(void);
static void load_probe_both_status(void);
static void load_no_card_screen(void);
static void load_probe_result_screen(void);
static void load_read_slot_headers(void);
static void load_slot_select_screen(void);
static void load_begin_file(void);
static void load_read_file_step(void);
static void load_verify_image_step(void);
static void load_abort_confirm_screen(void);
static void load_abort(void);
static void load_done_screen(void);
static void save_transfer_run_step(void);
static void save_transfer_begin_message(void);
static void save_transfer_probe_slots(void);
static void save_transfer_no_card_screen(void);
static void save_transfer_probe_result_screen(void);
static void save_transfer_read_slot_headers(void);
static void save_transfer_slot_select_screen(void);
static void save_transfer_begin_file(void);
static void save_transfer_read_file_step(void);
static void save_transfer_verify_image_step(void);
static void save_transfer_check_same_game_step(void);
static void save_transfer_same_game_screen(void);
static void save_transfer_farm_name_step(void);
static void save_transfer_save_name_step(void);
static void save_check_transfer_fit(void);
static void save_transfer_confirm_screen(void);
static void save_transfer_slot_confirm_screen(void);
static void save_transfer_write_step(void);
static void save_transfer_abort_confirm_screen(void);
static void save_transfer_abort(void);
static void save_transfer_done_screen(void);
static void save_transfer_stash(void);
static void save_transfer_restore(void);
static void *save_find_free_record(FarmAnimals *tables, s16 kind, s16 *index_out);
static s32 save_count_records(FarmAnimals *animals, s16 kind);
static void save_draw_transfer_lists(void);
static s32 save_draw_name_panel(s16 side);
static s32 save_transfer_list_step(s16 side);
static void save_transfer_add(void *p, s16 value);
static void save_transfer_remove(void *p, s16 index);
static s32 save_transfer_contains(void *p, s16 value);
static void save_draw_entry_prompt(s16 kind, s32 selected, u16 *text, s16 affection, s16 x, s16 y);
static void save_draw_summary_line(FarmAnimals *records, s16 index, s16 y);
static s16 save_read_year(s16 *year);
static void save_draw_digit_row(s16 count, s16 x, s16 value);
static s16 save_count_page_entries(FarmAnimals *inv, s16 want);
static void save_reset_work_tables(void);
static void save_card_title(s16 slot);
static void save_format_two_digits(s16 value, char *dst, s16 mode);
static void save_season_name(s16 idx, char *dst);
static void save_slot_record_fill(s16 slot);
static void save_pack_farm_state(void);
static void save_unpack_farm_state(void);
static void save_image_shift_in(void);
static void save_image_shift_out(void);
static void save_image_checksum_write(void);
static s32 save_image_checksum_verify(void);
static s32 save_card_probe(s32 slot);
static s32 save_card_poll(s16 port);
static s16 save_card_check_space(s16 port, s16 slot);
static s16 save_card_read_header(s16 port, s16 slot, SaveSlotRecord *record);
static s32 save_card_format(s16 port);
static s32 save_card_create_file(s16 port, s16 file_index);
static s32 save_card_write_file(s16 port, s16 slot, u8 *data);
static s32 save_card_read_file(s16 port, s16 slot, u8 *buf);
static s32 save_menu_choose_three(s32 mode);
static void save_draw_slot_row(s16 slot, s16 mode);
static void save_format_number_suffix(s16 value, u16 *out, s32 width);
static s32 save_confirm_prompt(void);
static void save_screen_begin(s16 load_image);
static void save_screen_begin_hook_1(void);
static void save_screen_begin_hook_2(void);
static void save_load_payload(void);
static void save_screen_begin_hook_3(void);
static void save_load_image_file(void);
static void save_file_name(char *name, u32 slot);
static void save_place_grid_spot(FarmActor *object, s16 slot);
static void save_place_row_spot(FarmActor *object, s16 slot);

enum SaveStep
{
    SAVE_STEP_BEGIN_MESSAGE = 0,
    SAVE_STEP_PROBE_SLOTS = 1,
    SAVE_STEP_NO_CARD = 2,
    SAVE_STEP_CHOOSE_CARD = 3,
    SAVE_STEP_PROBE_RESULT = 4,
    SAVE_STEP_NO_CARD_SLOT = 5,
    SAVE_STEP_DAMAGED_CARD = 6,
    SAVE_STEP_READ_SLOT_HEADERS = 7,
    SAVE_STEP_SLOT_SELECT = 8,
    SAVE_STEP_OVERWRITE_CONFIRM = 9,
    SAVE_STEP_PROBE_AND_CHECK_SPACE = 10,
    SAVE_STEP_FORMAT_PROMPT = 11,
    SAVE_STEP_FORMAT_CARD = 13,
    SAVE_STEP_FORMAT_FAILED = 14,
    SAVE_STEP_CREATE_FILE = 15,
    SAVE_STEP_PROCESS_FAILED = 16,
    SAVE_STEP_NOT_ENOUGH_SPACE = 17,
    SAVE_STEP_NO_FREE_SPACE = 18,
    SAVE_STEP_BUILD_HEADER = 19,
    SAVE_STEP_WRITE_FILE = 20,
    SAVE_STEP_DISCONNECTED = 21,
    SAVE_STEP_CONFIRM_RESULT = 22,
    SAVE_STEP_ABORT = 23,
    SAVE_STEP_DONE = 24,
};

enum LoadStep
{
    LOAD_STEP_BEGIN_MESSAGE = 0,
    LOAD_STEP_PROBE_BOTH = 1,
    LOAD_STEP_NO_CARD = 2,
    LOAD_STEP_CHOOSE_CARD = 3,
    LOAD_STEP_PROBE_RESULT = 4,
    LOAD_STEP_NO_CARD_SLOT = 5,
    LOAD_STEP_DAMAGED_CARD = 6,
    LOAD_STEP_READ_SLOT_HEADERS = 7,
    LOAD_STEP_NO_LOADABLE = 8,
    LOAD_STEP_SLOT_SELECT = 9,
    LOAD_STEP_NO_DIARY_HERE = 10,
    LOAD_STEP_BEGIN_FILE = 11,
    LOAD_STEP_READ_FILE = 12,
    LOAD_STEP_VERIFY_IMAGE = 13,
    LOAD_STEP_DAMAGED_DIARY = 14,
    LOAD_STEP_FAILED = 15,
    LOAD_STEP_DISCONNECTED = 16,
    LOAD_STEP_ABORT_CONFIRM = 17,
    LOAD_STEP_ABORT = 18,
    LOAD_STEP_DONE = 19,
};

enum TransferStep
{
    TRANSFER_STEP_BEGIN_MESSAGE = 0,
    TRANSFER_STEP_PROBE_SLOTS = 1,
    TRANSFER_STEP_NO_CARD = 2,
    TRANSFER_STEP_CHOOSE_CARD = 3,
    TRANSFER_STEP_PROBE_RESULT = 4,
    TRANSFER_STEP_NO_CARD_SLOT = 5,
    TRANSFER_STEP_DAMAGED_CARD = 6,
    TRANSFER_STEP_PROBE_THREE_SLOTS = 7,
    TRANSFER_STEP_NO_LOADABLE = 8,
    TRANSFER_STEP_SLOT_SELECT = 9,
    TRANSFER_STEP_NO_DIARY_HERE = 10,
    TRANSFER_STEP_BEGIN_FILE = 11,
    TRANSFER_STEP_READ_FILE = 12,
    TRANSFER_STEP_VERIFY_IMAGE = 13,
    TRANSFER_STEP_DAMAGED_DIARY = 14,
    TRANSFER_STEP_FAILED = 15,
    TRANSFER_STEP_CHECK_SAME_GAME = 16,
    TRANSFER_STEP_SAME_GAME = 17,
    TRANSFER_STEP_FARM_NAME = 18,
    TRANSFER_STEP_SAVE_NAME = 19,
    TRANSFER_STEP_CHECK_FIT = 20,
    TRANSFER_STEP_NO_ANIMAL = 21,
    TRANSFER_STEP_HENS_FULL = 22,
    TRANSFER_STEP_PARTNER_HENS_FULL = 23,
    TRANSFER_STEP_COWS_FULL = 24,
    TRANSFER_STEP_PARTNER_COWS_FULL = 25,
    TRANSFER_STEP_CONFIRM = 26,
    TRANSFER_STEP_SLOT_CONFIRM = 27,
    TRANSFER_STEP_WRITE_FAILED = 28,
    TRANSFER_STEP_WRITE = 29,
    TRANSFER_STEP_DISCONNECTED = 30,
    TRANSFER_STEP_ABORT_CONFIRM = 31,
    TRANSFER_STEP_ABORT = 32,
    TRANSFER_STEP_DONE = 33,
};

/* What the save_card_* helpers return: a card state (save_card_probe), a poll
 * or check result, or the outcome of a card operation. */
enum SaveCardResult
{
    SAVE_CARD_OK = 0,
    SAVE_CARD_READY = 1,
    SAVE_CARD_FAILED = 2,
    SAVE_CARD_BUSY = 3,
    SAVE_CARD_NO_CARD = 4,
    SAVE_CARD_NEW = 5,
    SAVE_CARD_UNFORMATTED = 6,
    SAVE_CARD_UNFORMATTED_MCX = 7,
    SAVE_CARD_DAMAGED = 8,
    SAVE_CARD_NO_FILE = 9,
    SAVE_CARD_NOT_ENOUGH = 11,
    SAVE_CARD_ERROR = 12,
    SAVE_CARD_BAD_CHECKSUM = 14,
    SAVE_CARD_FULL = 15,
};

/* MemCardSync's status: busy, finished (read the result), or nothing pending. */
enum SaveSyncStatus
{
    SAVE_SYNC_BUSY = 0,
    SAVE_SYNC_DONE = 1,
    SAVE_SYNC_IDLE = -1,
};

/* The memory card slot a save or load uses: slot 1 is channel 0, slot 2 is channel 0x10. */
enum SaveCardPort
{
    SAVE_PORT_1 = 0,
    SAVE_PORT_2 = 1,
    SAVE_PORT_BOTH = 2,
};

/* The side of the transfer screen a name panel or list is for: the farm, or the saved file. */
enum SaveSide
{
    SAVE_SIDE_FARM = 0,
    SAVE_SIDE_SAVED = 1,
};

/* The transfer panel's rows and the save_entries kinds: chickens, cattle, sheep. */
enum SavePanelRow
{
    SAVE_PANEL_CHICKENS = 0,
    SAVE_PANEL_CATTLE = 1,
    SAVE_PANEL_SHEEP = 2,
};

/* The record table a save or load searches: the chickens or the livestock. */
enum SaveRecordTable
{
    SAVE_TABLE_CHICKENS = 0,
    SAVE_TABLE_LIVESTOCK = 1,
};

/* What a save/load step leaves in save_retry_flag for the next tick. */
enum SaveRetryFlag
{
    SAVE_FLAG_ABORT = -1,
    SAVE_FLAG_CLEAR = 0,
    SAVE_FLAG_DONE = 1,
    SAVE_FLAG_RUNNING = 2,
};

/* What the slot list shows for each save slot. */
#define SAVE_FADE_IN_FRAMES 60
#define SAVE_FADE_OUT_FRAMES 10
#define SAVE_MENU_INDEX_MAX 255
#define SAVE_PROBE_SETTLE_FRAMES 2
#define SAVE_NAME_END 0xFFFF
#define SAVE_SLOT_TAG 0x14
#define SAVE_ENTRY_EMPTY -1
#define SAVE_CHECKSUM_BAD -1
#define SAVE_LIST_ENTRIES 5
#define SAVE_TRANSFER_MAX 5
#define SAVE_HEADER_OFFSET 0x200
#define SAVE_HEADER_SIZE 0x80
#define COOP_CAPACITY_BASIC 5
#define COOP_CAPACITY_UPGRADED 10
#define BARN_CAPACITY_BASIC 10
#define BARN_CAPACITY_UPGRADED 20
#define SAVE_FILE_BLOCKS 4
#define SAVE_PROBE_RETRIES 8
#define SAVE_CARD_RETRIES 7
#define SAVE_POLL_RETRIES 31
#define SAVE_FARM_BLOCKS 4
#define SAVE_PDA_BLOCKS 10
#define SAVE_PDA_SLOT 3
#define SAVE_CARD_FILES 15
#define SAVE_SPRITE_FILE 0xE
#define SAVE_SPRITE_SIZE 0x8E90
#define SAVE_SPRITE_COUNT 5
#define SAVE_FILE_BYTES 0x9690
#define SAVE_ICON_FLAG 0x13
#define SAVE_SLOT_ROW_STEP 49
#define SAVE_LIST_ROW_HEIGHT 24
#define SAVE_SEASON_COUNT 4
#define SAVE_DIGIT_WIDTH 8

enum SaveSlotStatus
{
    SAVE_SLOT_DIARY = 0,
    SAVE_SLOT_NO_DIARY = 1,
    SAVE_SLOT_SHORT_OF_SPACE = 2,
    SAVE_SLOT_NO_FREE_SPACE = 3,
};

/* Saveload step/screen id. */
static s8 save_step = SAVE_STEP_BEGIN_MESSAGE;
static s8 save_step_prev = 0;
static s8 save_retry_flag = SAVE_FLAG_CLEAR; /* -1 after an abort, 1 when done */
static s8 save_confirm_cursor = 0;
static s8 save_confirm_flag = 0;
static s16 save_card_index = 0;
static s16 save_probe_result = SAVE_PORT_BOTH;
static s16 save_slot = 0;

/* Cleared by save_reset_state; nothing reads it. */
static s16 saveload_unused_80137590 = 0;

static s16 save_card_status[2] = {0, 0};
static s16 save_retry_count = 0;
static s16 save_menu_index = 0;
static s16 save_format_state = 0;
static s16 save_panel_row = SAVE_PANEL_CHICKENS;
static s16 save_first_row = 0;
static s16 save_cursor_row = 0;

static s16 save_transfer_count = 0;

/* The slot digit a card message inserts, indexed by SaveCardPort (so SAVE_PORT_BOTH gives 3). */
static u16 save_slot_digits[3][2] = {{0x41, 0xFFFF}, {0x42, 0xFFFF}, {0x43, 0xFFFF}};

static IconOffsets save_icon_offsets[3] = {
    {0x000, 0x0C0, 0x180},
    {0x240, 0x300, 0x3C0},
    {0x480, 0x540, 0x600},
};

/* Start of each row of five save_place_grid_spot entries (400 apart in x). */
static VECTOR save_slot_row_starts[2] = {{-760, 0, -600, 0}, {-750, 0, 1600, 0}};

static VECTOR save_page_positions[6] = {
    {1980, 0, -1268, 0},
    {1250, 0, -1268, 0},
    {-1250, 0, -1268, 0},
    {-1980, 0, -1268, 0},
    {2400, 0, -2450, 0},
    {-2050, 0, -2450, 0},
};

/* The cursor sprite's frame for each save_panel_row row (save_draw_name_panel). */
static u8 save_cursor_frames[] = {15, 7, 11};

static u16 save_date_text[16];
static char save_slot_text[24];
static char save_block_text[8];
/* The save image in use. */
static void *save_image;
static SaveSlotRecord save_slot_records[3];
static u8 saveload_unused_801377A0[128];
static s16 save_slot_status[4];
static u8 *save_icon_base;
static u8 save_file_header[0x80];
static IconFrame save_icon_frame_0;
static IconFrame save_icon_frame_1;
static IconFrame save_icon_frame_2;
static u8 save_slot_record[0x80];
static u8 save_image_data[0x9690 - 0x280];
static char save_file_name_buf[32];
static u16 save_name_line[24];
static SaveHeader save_header;
static SaveEntry save_entries[2][SAVE_LIST_ENTRIES];
static u8 save_work_table[5488];
static u8 save_out_buffer[928];
static u8 save_sheet[sizeof(SaveSheet)];
/* The five sprites save_load_image_file unpacks from file 0xE; [1] is the cursor. */
static AnimState save_sprites[5];
static u8 saveload_unused_80145760[0x688];

/* Reset every piece of save/load state for a fresh session. */
static void save_reset_state(void)
{
    long cmd;
    long result;
    s32 i;
    SaveSlotRecord *records;

    MemCardSync(0, &cmd, &result);
    save_step = SAVE_STEP_BEGIN_MESSAGE;
    save_step_prev = 0;
    save_retry_flag = SAVE_FLAG_CLEAR;
    save_confirm_cursor = 0;
    save_confirm_flag = 0;
    save_retry_count = 0;
    save_card_index = 0;
    save_probe_result = SAVE_PORT_BOTH;
    save_slot = 0;
    saveload_unused_80137590 = 0;
    save_card_status[0] = SAVE_CARD_NO_CARD;
    save_card_status[1] = SAVE_CARD_NO_CARD;
    save_slot_status[0] = SAVE_SLOT_SHORT_OF_SPACE;
    save_slot_status[1] = SAVE_SLOT_SHORT_OF_SPACE;
    save_slot_status[2] = SAVE_SLOT_SHORT_OF_SPACE;
    save_menu_index = 0;
    save_icon_base = farm_area.sectorBuffer;
    memset(&save_slot_records[0], 0, sizeof(SaveSlotRecord));
    memset(&save_slot_records[1], 0, sizeof(SaveSlotRecord));
    memset(&save_slot_records[2], 0, sizeof(SaveSlotRecord));
    memset(save_file_header, 0, SAVE_FILE_BYTES);
    memset(saveload_unused_801377A0, 0, sizeof(saveload_unused_801377A0));
    save_reset_work_tables();
    save_image = save_file_header + 0x280;
    memset(save_image, 0, 0x6378);
    records = save_slot_records;
    for (i = 0; i < 3; i++)
    {
        records[i].farmName[0] = SAVE_NAME_END;
        records[i].farmName[NAME_CHARS] = SAVE_NAME_END;
        records[i].playerName[0] = SAVE_NAME_END;
        records[i].playerName[NAME_CHARS] = SAVE_NAME_END;
    }
}
/* Frame handler of the save screen: fade in, run save_run_step each frame until
 * it finishes (save_retry_flag = 1) or aborts (-1), then fade out and leave. */
void save_screen_tick(void)
{
    u_long *window = &current_frame->otTags[0][11];
    u32 state;

    state = game_state.progress.entrySubStep;
    if (state == 0)
    {
        sound_seq_volume_if_idle(0, 0, 0, 0, 0);
        SsUtAllKeyOff(0);
        loading_screen_stop();
        menu_reset();
        save_reset_state();
        save_step = SAVE_STEP_BEGIN_MESSAGE;
        save_screen_begin(0);
        fade_start_in(SAVE_FADE_IN_FRAMES, 0, 2);
        sound_fade_in(SAVE_FADE_IN_FRAMES);
        ui_blink_tick_60(-1);
        game_state.progress.entrySubStep = 1;
    }
    else if (state == 1)
    {
        bg_tile_mode_sprite(window);
        if (fade_is_settled() == 1)
        {
            game_state.progress.entrySubStep = 2;
        }
    }
    else if (state == 2)
    {
        save_retry_flag = SAVE_FLAG_RUNNING;
        text_set_source(0, save_slot_digits[save_probe_result]);
        bg_tile_mode_sprite(window);
        ui_blink_tick_60(0);
        if (save_step != save_step_prev)
        {
            save_menu_index = 0;
            save_step_prev = save_step;
        }
        else if (save_menu_index < SAVE_MENU_INDEX_MAX)
        {
            save_menu_index++;
        }
        text_set_spacing(0, 2);
        save_run_step();
        text_set_spacing(0, 0);
        switch (save_retry_flag)
        {
        case SAVE_FLAG_ABORT:
            game_state.progress.entrySubStep = 4;
            break;
        case SAVE_FLAG_DONE:
            game_state.progress.entrySubStep = 3;
            break;
        case SAVE_FLAG_RUNNING:
        default:
            return;
        }
        fade_start_out(SAVE_FADE_OUT_FRAMES, 0, 2);
        sound_fade_out(SAVE_FADE_OUT_FRAMES);
    }
    else if (state == 3 || state == 4)
    {
        bg_tile_mode_sprite(window);
        if (fade_is_settled() == 1)
        {
            game_state.progress.entrySubStep = game_state.progress.entrySubStep == 3 ? 3 : 4;
            main_request_scene(SCENE_FARM_SAVED);
            sound_seq_volume_if_idle(0, 0, 0, 80, 10);
        }
    }
}
/* Same frame handler for the screen driven by load_run_step. */
void load_screen_tick(void)
{
    u_long *window = &current_frame->otTags[0][11];
    u32 state;

    state = game_state.progress.entrySubStep;
    if (state == 0)
    {
        sound_seq_volume_if_idle(0, 0, 0, 0, 0);
        SsUtAllKeyOff(0);
        loading_screen_stop();
        menu_reset();
        save_reset_state();
        save_step = LOAD_STEP_BEGIN_MESSAGE;
        save_screen_begin(0);
        fade_start_in(SAVE_FADE_IN_FRAMES, 0, 2);
        sound_fade_in(SAVE_FADE_IN_FRAMES);
        ui_blink_tick_60(-1);
        game_state.progress.entrySubStep = 1;
    }
    else if (state == 1)
    {
        bg_tile_mode_sprite(window);
        if (fade_is_settled() == 1)
        {
            game_state.progress.entrySubStep = 2;
        }
    }
    else if (state == 2)
    {
        save_retry_flag = SAVE_FLAG_RUNNING;
        text_set_source(0, save_slot_digits[save_probe_result]);
        bg_tile_mode_sprite(window);
        ui_blink_tick_60(0);
        if (save_step != save_step_prev)
        {
            save_menu_index = 0;
            save_step_prev = save_step;
        }
        else if (save_menu_index < SAVE_MENU_INDEX_MAX)
        {
            save_menu_index++;
        }
        text_set_spacing(0, 2);
        load_run_step();
        text_set_spacing(0, 0);
        switch (save_retry_flag)
        {
        case SAVE_FLAG_ABORT:
            game_state.progress.entrySubStep = 4;
            break;
        case SAVE_FLAG_DONE:
            game_state.progress.entrySubStep = 3;
            break;
        case SAVE_FLAG_RUNNING:
        default:
            return;
        }
        fade_start_out(SAVE_FADE_OUT_FRAMES, 0, 2);
        sound_fade_out(SAVE_FADE_OUT_FRAMES);
    }
    else if (state == 3 || state == 4)
    {
        bg_tile_mode_sprite(window);
        if (fade_is_settled() == 1)
        {
            sound_seq_volume_if_idle(0, 0, 0, 80, 10);
            game_state.progress.entrySubStep = game_state.progress.entrySubStep == 3 ? 3 : 4;
            if (game_state.progress.entrySubStep == 4)
            {
                if (game_state.globalFlags & GLOBAL_FLAG_CONTINUE)
                {
                    main_request_scene(SCENE_TITLE);
                }
                else
                {
                    main_request_scene(SCENE_FARM_MENU_RETURN);
                }
            }
            else
            {
                if (game_state.globalFlags & GLOBAL_FLAG_CONTINUE)
                {
                    game_state.globalFlags &= ~GLOBAL_FLAG_CONTINUE;
                }
                main_request_scene(SCENE_FARM_LOADED);
            }
        }
    }
}
/* Same frame handler for the screen driven by save_transfer_run_step. */
void save_transfer_screen_tick(void)
{
    u_long *window = &current_frame->otTags[0][11];
    u32 state;

    state = game_state.progress.entrySubStep;
    if (state == 0)
    {
        sound_seq_volume_if_idle(0, 0, 0, 0, 0);
        SsUtAllKeyOff(0);
        loading_screen_stop();
        menu_reset();
        save_reset_state();
        save_step = TRANSFER_STEP_BEGIN_MESSAGE;
        save_screen_begin(1);
        fade_start_in(SAVE_FADE_IN_FRAMES, 0, 2);
        sound_fade_in(SAVE_FADE_IN_FRAMES);
        ui_blink_tick_60(-1);
        game_state.progress.entrySubStep = 1;
    }
    else if (state == 1)
    {
        bg_tile_mode_sprite(window);
        if (fade_is_settled() == 1)
        {
            game_state.progress.entrySubStep = 2;
        }
    }
    else if (state == 2)
    {
        save_retry_flag = SAVE_FLAG_RUNNING;
        text_set_source(0, save_slot_digits[save_probe_result]);
        bg_tile_mode_sprite(window);
        ui_blink_tick_60(0);
        if (save_step != save_step_prev)
        {
            save_menu_index = 0;
            save_step_prev = save_step;
        }
        else if (save_menu_index < SAVE_MENU_INDEX_MAX)
        {
            save_menu_index++;
        }
        text_set_spacing(0, 2);
        save_transfer_run_step();
        text_set_spacing(0, 0);
        switch (save_retry_flag)
        {
        case SAVE_FLAG_ABORT:
            game_state.progress.entrySubStep = 4;
            break;
        case SAVE_FLAG_DONE:
            game_state.progress.entrySubStep = 3;
            break;
        case SAVE_FLAG_RUNNING:
        default:
            return;
        }
        fade_start_out(SAVE_FADE_OUT_FRAMES, 0, 2);
        sound_fade_out(SAVE_FADE_OUT_FRAMES);
    }
    else if (state == 3 || state == 4)
    {
        bg_tile_mode_sprite(window);
        if (fade_is_settled() == 1)
        {
            sound_seq_volume_if_idle(0, 0, 0, 80, 10);
            game_state.progress.entrySubStep = game_state.progress.entrySubStep == 3 ? 3 : 4;
            main_request_scene(SCENE_FARM_MENU_RETURN);
        }
    }
}

/* Run the current save/load step; for the steps that poll the card, refresh the active slot's status and react to a change. */
static void save_run_step(void)
{
    s32 status;

    switch (save_step)
    {
    case SAVE_STEP_BEGIN_MESSAGE:
        save_begin_message();
        return;
    case SAVE_STEP_PROBE_SLOTS:
        save_probe_slots_pick_screen();
        return;
    case SAVE_STEP_NO_CARD:
        save_no_card_screen();
        break;
    case SAVE_STEP_CHOOSE_CARD:
        save_choose_card_screen();
        break;
    case SAVE_STEP_PROBE_RESULT:
        save_probe_result_screen();
        return;
    case SAVE_STEP_NO_CARD_SLOT:
        save_no_card_slot_screen();
        break;
    case SAVE_STEP_DAMAGED_CARD:
        save_damaged_card_screen();
        break;
    case SAVE_STEP_READ_SLOT_HEADERS:
        save_read_slot_headers();
        return;
    case SAVE_STEP_SLOT_SELECT:
        save_slot_select_screen();
        break;
    case SAVE_STEP_OVERWRITE_CONFIRM:
        save_overwrite_confirm_screen();
        break;
    case SAVE_STEP_PROBE_AND_CHECK_SPACE:
        save_probe_and_check_space();
        return;
    case SAVE_STEP_FORMAT_PROMPT:
        save_format_prompt_screen();
        break;
    case SAVE_STEP_FORMAT_CARD:
        save_format_card();
        return;
    case SAVE_STEP_FORMAT_FAILED:
        save_format_failed_screen();
        break;
    case SAVE_STEP_CREATE_FILE:
        save_create_file_step();
        return;
    case SAVE_STEP_PROCESS_FAILED:
        save_process_failed_screen();
        break;
    case SAVE_STEP_NOT_ENOUGH_SPACE:
        save_not_enough_space_screen();
        break;
    case SAVE_STEP_NO_FREE_SPACE:
        save_no_free_space_screen();
        break;
    case SAVE_STEP_BUILD_HEADER:
        save_build_header();
        break;
    case SAVE_STEP_WRITE_FILE:
        save_write_file_step();
        return;
    case SAVE_STEP_DISCONNECTED:
        save_disconnected_screen();
        break;
    case SAVE_STEP_CONFIRM_RESULT:
        save_confirm_result();
        break;
    case SAVE_STEP_ABORT:
        save_abort();
        return;
    case SAVE_STEP_DONE:
        save_done_screen();
        return;
    }

    status = save_card_poll(save_probe_result == SAVE_PORT_BOTH ? save_card_index : save_probe_result);
    if (status == SAVE_CARD_BUSY)
    {
        return;
    }
    if (status != save_card_status[save_probe_result == SAVE_PORT_BOTH ? save_card_index : save_probe_result])
    {
        if (status == SAVE_CARD_NEW)
        {
            save_step = SAVE_STEP_BEGIN_MESSAGE;
        }
        if (status == SAVE_CARD_NO_CARD)
        {
            if (save_probe_result == SAVE_PORT_BOTH)
            {
                save_probe_result = save_card_index;
            }
            save_step = SAVE_STEP_DISCONNECTED;
        }
    }
    if (save_probe_result == SAVE_PORT_BOTH)
    {
        if (save_card_status[save_card_index] != SAVE_CARD_UNFORMATTED && save_card_status[save_card_index] != SAVE_CARD_UNFORMATTED_MCX)
        {
            save_card_status[save_card_index] = status;
        }
        save_card_index = save_card_index == 0;
    }
    else
    {
        if (save_card_status[save_probe_result] != SAVE_CARD_UNFORMATTED && save_card_status[save_probe_result] != SAVE_CARD_UNFORMATTED_MCX)
        {
            save_card_status[save_probe_result] = status;
        }
    }
}

/* Start a save: say the card is being searched, then probe the slots. */
static void save_begin_message(void)
{
    FrameState *s = current_frame;
    SHOW_MESSAGE(s->otTags[0], LOCALIZED(text_bank.messages.searching_the_memory_card));
    save_reset_state();
    save_step = SAVE_STEP_PROBE_SLOTS;
}

/* Refresh the save/load display, record both slots' storage status and pick
 * the screen: 2 when both slots report 4, otherwise 3. */
static void save_probe_slots_pick_screen(void)
{
    u_long *ot;
    s32 slot;
    s32 status;

    ot = current_frame->otTags[0];
    SHOW_MESSAGE(ot, LOCALIZED(text_bank.messages.searching_the_memory_card));
    if (save_menu_index < SAVE_PROBE_SETTLE_FRAMES)
    {
        return;
    }
    for (slot = 0; slot < 2; slot++)
    {
        status = save_card_probe((s16)slot);
        if (status == SAVE_CARD_READY)
        {
            status = SAVE_CARD_OK;
        }
        if (status == SAVE_CARD_UNFORMATTED_MCX)
        {
            status = SAVE_CARD_UNFORMATTED;
        }
        if (status != save_card_status[slot])
        {
            save_card_status[slot] = status;
        }
    }
    if (save_card_status[0] == SAVE_CARD_NO_CARD && save_card_status[1] == SAVE_CARD_NO_CARD)
    {
        save_step = SAVE_STEP_NO_CARD;
    }
    else
    {
        save_step = SAVE_STEP_CHOOSE_CARD;
    }
}

/* Say neither slot holds a card; the confirm press goes on to SAVE_STEP_CONFIRM_RESULT. */
static void save_no_card_screen(void)
{
    u_long *p = current_frame->otTags[0];
    SHOW_MESSAGE(p, LOCALIZED(text_bank.messages.no_memory_card_in_either_slot));
    save_step = save_menu_wait_confirm(SAVE_STEP_NO_CARD, SAVE_STEP_CONFIRM_RESULT);
    save_confirm_flag = 1;
}

/* Refresh the save/load display and probe the storage; falls to SAVE_STEP_CONFIRM_RESULT when the probe fails. */
SAVELOAD_PROBE_ENTRY(save_choose_card_screen, SAVE_STEP_CONFIRM_RESULT, SAVE_STEP_PROBE_RESULT)

/* Refresh the save/load display and choose the storage result screen. */
static void save_probe_result_screen(void)
{
    s32 card_code;

    {
        u_long *ot;
        ot = current_frame->otTags[0];
        SHOW_MESSAGE(ot, LOCALIZED(text_bank.messages.searching_the_memory_card));
    }
    if (save_menu_index < SAVE_PROBE_SETTLE_FRAMES)
    {
        return;
    }
    card_code = save_card_probe(save_probe_result);
    switch (card_code)
    {
    case SAVE_CARD_NO_CARD:
        if (save_card_status[save_probe_result] == SAVE_CARD_OK)
        {
            save_step = SAVE_STEP_DISCONNECTED;
        }
        else
        {
            save_step = SAVE_STEP_NO_CARD_SLOT;
        }
        break;
    case SAVE_CARD_NEW:
        save_step = SAVE_STEP_PROBE_RESULT;
        break;
    case SAVE_CARD_DAMAGED:
        save_step = SAVE_STEP_DAMAGED_CARD;
        break;
    default:
        save_step = SAVE_STEP_READ_SLOT_HEADERS;
        break;
    }
}

SAVELOAD_MENU_ENTRY(save_no_card_slot_screen, text_bank.messages.no_memory_card_in_slot, SAVE_STEP_NO_CARD_SLOT, SAVE_STEP_BEGIN_MESSAGE)

SAVELOAD_MENU_ENTRY(save_damaged_card_screen, text_bank.messages.memory_card_in_slot_damaged, SAVE_STEP_DAMAGED_CARD, SAVE_STEP_BEGIN_MESSAGE)

/* Read the save header of each of the three slots into save_slot_records and set its status; a damaged card stops at SAVE_STEP_DAMAGED_CARD. */
static void save_read_slot_headers(void)
{
    u_long *ot;
    s32 i;
    s16 card_code;

    ot = current_frame->otTags[0];
    SHOW_MESSAGE(ot, LOCALIZED(text_bank.messages.searching_the_memory_card));
    for (i = 0; i < 3; i++)
    {
        save_slot_status[i] = SAVE_SLOT_SHORT_OF_SPACE;
        card_code = save_card_read_header(save_probe_result, i, &save_slot_records[i]);
        if (card_code == SAVE_CARD_DAMAGED)
        {
            save_step = SAVE_STEP_DAMAGED_CARD;
            return;
        }
        if (card_code == SAVE_CARD_NO_CARD || card_code == SAVE_CARD_NEW)
        {
            save_step = SAVE_STEP_DISCONNECTED;
            return;
        }
        if (card_code == SAVE_CARD_NO_FILE)
        {
            card_code = save_card_check_space(save_probe_result, i);
            if (card_code == SAVE_CARD_OK)
            {
                save_slot_status[i] = SAVE_SLOT_NO_DIARY;
            }
            else if (card_code == SAVE_CARD_FULL)
            {
                save_slot_status[i] = SAVE_SLOT_NO_FREE_SPACE;
            }
            else
            {
                save_slot_status[i] = SAVE_SLOT_SHORT_OF_SPACE;
            }
        }
        else if (card_code == SAVE_CARD_OK)
        {
            save_slot_status[i] = SAVE_SLOT_DIARY;
        }
        if (save_card_status[save_probe_result] == SAVE_CARD_UNFORMATTED || save_card_status[save_probe_result] == SAVE_CARD_UNFORMATTED_MCX)
        {
            save_slot_status[i] = SAVE_SLOT_NO_DIARY;
        }
    }
    save_confirm_cursor = 0;
    save_step = SAVE_STEP_SLOT_SELECT;
}

/* Refresh the save/load display and classify the storage probe result. */
static void save_slot_select_screen(void)
{
    u_long *ot;
    s32 choice;
    s16 status;

    ot = current_frame->otTags[0];
    SHOW_MESSAGE(ot, LOCALIZED(text_bank.messages.select_a_diary_to_save));
    choice = save_menu_choose_three(1);
    if (choice < 3)
    {
        if (choice < 0)
        {
            if (choice == -1)
            {
                save_probe_result = SAVE_PORT_BOTH;
                save_step = SAVE_STEP_BEGIN_MESSAGE;
            }
        }
        else
        {
            status = save_slot_status[(s16)choice];
            save_slot = choice;
            if (status == SAVE_SLOT_DIARY)
            {
                save_confirm_flag = 1;
                save_step = SAVE_STEP_OVERWRITE_CONFIRM;
            }
            else if (status == SAVE_SLOT_NO_DIARY)
            {
                save_step = SAVE_STEP_PROBE_AND_CHECK_SPACE;
            }
            else if (status == SAVE_SLOT_SHORT_OF_SPACE)
            {
                save_step = SAVE_STEP_NOT_ENOUGH_SPACE;
            }
            else if (status == SAVE_SLOT_NO_FREE_SPACE)
            {
                save_step = SAVE_STEP_NO_FREE_SPACE;
            }
        }
    }
}

/* Draw the save/load display, redraw the three slots and pick the next step
 * from the confirm prompt's answer. */
static void save_overwrite_confirm_screen(void)
{
    u_long *ot;
    s32 slot;
    s32 answer;

    slot = 0;
    ot = current_frame->otTags[0];
    SHOW_MESSAGE(ot, LOCALIZED(text_bank.messages.ok_to_overwrite_this_diary));
    do
    {
        save_draw_slot_row(slot, 1);
        slot++;
    } while (slot < 3);
    ui_draw_icon_0b_1(&current_frame->otTags[0][1], 57, save_confirm_cursor * SAVE_SLOT_ROW_STEP + 24);
    answer = save_confirm_prompt();
    switch (answer)
    {
    case SAVE_CARD_READY:
    case -1:
        save_confirm_cursor = 0;
        save_step = SAVE_STEP_SLOT_SELECT;
        break;
    case SAVE_CARD_OK:
        save_step = SAVE_STEP_BUILD_HEADER;
        break;
    }
}

/* Probe the card once the settle delay has passed and check that it has room to save. */
static void save_probe_and_check_space(void)
{
    u_long *ot;
    s32 card_code;

    ot = current_frame->otTags[0];
    SHOW_MESSAGE(ot, LOCALIZED(text_bank.messages.searching_the_memory_card));
    if (save_menu_index < SAVE_PROBE_SETTLE_FRAMES)
    {
        return;
    }
    card_code = save_card_probe(save_probe_result);
    if (card_code == SAVE_CARD_READY)
    {
        card_code = SAVE_CARD_OK;
    }
    if (card_code == SAVE_CARD_UNFORMATTED_MCX)
    {
        card_code = SAVE_CARD_UNFORMATTED;
    }
    switch (card_code)
    {
    case SAVE_CARD_OK:
        switch (save_card_check_space(save_probe_result, save_slot))
        {
        case SAVE_CARD_OK:
            save_step = SAVE_STEP_CREATE_FILE;
            break;
        case SAVE_CARD_NO_CARD:
            if (save_card_status[save_probe_result] == SAVE_CARD_OK)
            {
                save_step = SAVE_STEP_DISCONNECTED;
            }
            else
            {
                save_step = SAVE_STEP_NO_CARD_SLOT;
            }
            break;
        case SAVE_CARD_DAMAGED:
            save_step = SAVE_STEP_DAMAGED_CARD;
            break;
        case SAVE_CARD_NOT_ENOUGH:
            save_step = SAVE_STEP_NOT_ENOUGH_SPACE;
            break;
        case SAVE_CARD_FULL:
            save_step = SAVE_STEP_NO_FREE_SPACE;
            break;
        }
        break;
    case SAVE_CARD_NEW:
        save_step = SAVE_STEP_BEGIN_MESSAGE;
        break;
    case SAVE_CARD_NO_CARD:
        if (save_card_status[save_probe_result] == SAVE_CARD_OK)
        {
            save_step = SAVE_STEP_DISCONNECTED;
        }
        else
        {
            save_step = SAVE_STEP_NO_CARD_SLOT;
        }
        break;
    case SAVE_CARD_DAMAGED:
        save_step = SAVE_STEP_DAMAGED_CARD;
        break;
    case SAVE_CARD_UNFORMATTED:
        save_confirm_flag = 1;
        save_format_state = 0;
        save_step = SAVE_STEP_FORMAT_PROMPT;
        break;
    }
}

/* The format prompt: asks whether to format a card that is not formatted yet. */
static void save_format_prompt_screen(void)
{
    s16 state;
    s32 answer;
    state = save_format_state;
    switch (state)
    {
    case 0:
        text_draw_in_frame(&current_frame->otTags[0][6], &current_frame->otTags[0][5],
                           LOCALIZED(text_bank.messages.memory_card_in_slot_not_formatted));

        if ((input_pad1->pressed & PAD_CROSS) != 0)
        {
            sound_play(SOUND_MENU_CONFIRM);
            answer = 1;
            state = answer;
            save_format_state = state;
        }
        break;
    case 1:
        text_draw_in_frame(&current_frame->otTags[0][6], &current_frame->otTags[0][5],
                           LOCALIZED(text_bank.messages.want_to_format));

        answer = save_confirm_prompt();
        switch (answer)
        {
        case SAVE_CARD_READY:
        case -1:
            save_step = SAVE_STEP_BEGIN_MESSAGE;
            break;
        case SAVE_CARD_OK:
            save_step = SAVE_STEP_FORMAT_CARD;
            break;
        }
        break;
    }
}

/* Refresh the save/load display and select the storage status message. */
static void save_format_card(void)
{
    u_long *ot;
    s32 card_code;

    ot = current_frame->otTags[0];
    SHOW_MESSAGE(ot, LOCALIZED(text_bank.messages.formatting_the_memory_card_in_slot));
    if (save_menu_index >= SAVE_PROBE_SETTLE_FRAMES)
    {
        card_code = save_card_format(save_probe_result);
        if (card_code == SAVE_CARD_OK)
        {
            save_step = SAVE_STEP_CREATE_FILE;
        }
        else if (card_code == SAVE_CARD_NO_CARD)
        {
            save_step = SAVE_STEP_DISCONNECTED;
        }
        else
        {
            save_step = SAVE_STEP_FORMAT_FAILED;
        }
    }
}

SAVELOAD_MENU_ENTRY(save_format_failed_screen, text_bank.messages.formatting_failed_for_the_memory_card, SAVE_STEP_FORMAT_FAILED, SAVE_STEP_BEGIN_MESSAGE)

/* Refresh the save/load display and select the storage status message. */
static void save_create_file_step(void)
{
    u_long *ot;
    s32 card_code;

    ot = current_frame->otTags[0];
    {
        SHOW_MESSAGE(ot, LOCALIZED(text_bank.messages.saving_to_memory_card));
    }
    if (save_menu_index >= SAVE_PROBE_SETTLE_FRAMES)
    {
        card_code = save_card_create_file(save_probe_result, save_slot);
        if (card_code == SAVE_CARD_OK)
        {
            save_step = SAVE_STEP_BUILD_HEADER;
        }
        else if (card_code == SAVE_CARD_NOT_ENOUGH)
        {
            save_step = SAVE_STEP_NOT_ENOUGH_SPACE;
        }
        else if (card_code == SAVE_CARD_NO_CARD)
        {
            save_step = SAVE_STEP_DISCONNECTED;
        }
        else
        {
            save_step = SAVE_STEP_PROCESS_FAILED;
        }
    }
}

SAVELOAD_MENU_ENTRY(save_process_failed_screen, text_bank.messages.save_process_failed, SAVE_STEP_PROCESS_FAILED, SAVE_STEP_BEGIN_MESSAGE)

SAVELOAD_MENU_ENTRY(save_not_enough_space_screen, text_bank.messages.cannot_save_not_enough_free_space, SAVE_STEP_NOT_ENOUGH_SPACE, SAVE_STEP_BEGIN_MESSAGE)

SAVELOAD_MENU_ENTRY(save_no_free_space_screen, text_bank.messages.cannot_save_no_free_space_on, SAVE_STEP_NO_FREE_SPACE, SAVE_STEP_BEGIN_MESSAGE)

/* Build the save header and the three icon frames for the selected slot. */
static void save_build_header(void)
{
    u_long *ot = current_frame->otTags[0];
    const IconOffsets *icon = &save_icon_offsets[save_slot];
    u32 clut;

    SHOW_MESSAGE(ot, LOCALIZED(text_bank.messages.saving_to_memory_card));
    save_slot_record_fill(save_slot);
    save_pack_farm_state();
    save_image_shift_in();
    save_image_checksum_write();
    save_file_name(save_file_name_buf, save_slot);
    strcpy(save_header.magic, "SC");
    save_header.icon_flag = SAVE_ICON_FLAG;
    save_header.blocks = SAVE_FILE_BLOCKS;
    save_card_title(save_slot);
    memset(save_header.reserved, 0, sizeof(save_header.reserved));

    clut = icon->frame[0] + 0x14;
    *(IconClut *)save_header.clut = *(IconClut *)(save_icon_base + clut);
    *(SaveHeader *)save_file_header = save_header;
    save_icon_frame_0 = *(IconFrame *)(icon->frame[0] + 0x40 + save_icon_base);
    save_icon_frame_1 = *(IconFrame *)(icon->frame[1] + 0x40 + save_icon_base);
    save_icon_frame_2 = *(IconFrame *)(icon->frame[2] + 0x40 + save_icon_base);
    save_step = SAVE_STEP_WRITE_FILE;
}

/* Refresh the save/load display, then dispatch the storage result to a screen step. */
static void save_write_file_step(void)
{
    u_long *ot;
    s32 card_code;

    ot = current_frame->otTags[0];
    SHOW_MESSAGE(ot, LOCALIZED(text_bank.messages.saving_to_memory_card));
    card_code = save_card_write_file(save_probe_result, save_slot, save_file_header);
    switch (card_code)
    {
    case SAVE_CARD_FAILED:
    case SAVE_CARD_NEW:
        save_step = SAVE_STEP_PROCESS_FAILED;
        break;
    case SAVE_CARD_NO_CARD:
        save_step = SAVE_STEP_DISCONNECTED;
        break;
    case SAVE_CARD_OK:
        save_step = SAVE_STEP_DONE;
        break;
    case SAVE_CARD_READY:
    case SAVE_CARD_BUSY:
        break;
    }
}

SAVELOAD_MENU_ENTRY(save_disconnected_screen, text_bank.messages.memory_card_has_been_disconnected_from, SAVE_STEP_DISCONNECTED, SAVE_STEP_BEGIN_MESSAGE)

/* Refresh the save/load display and record the storage probe result. */
static void save_confirm_result(void)
{
    u_long *ot;
    s32 answer;

    ot = current_frame->otTags[0];
    SHOW_MESSAGE(ot, LOCALIZED(text_bank.messages.aborting_save_is_that_ok));
    answer = save_confirm_prompt();
    switch (answer)
    {
    case SAVE_CARD_OK:
        save_confirm_cursor = 0;
        save_step = SAVE_STEP_ABORT;
        break;
    case SAVE_CARD_READY:
    case -1:
        save_step = SAVE_STEP_BEGIN_MESSAGE;
        break;
    }
}

/* Abandon the save and flag it aborted. */
static void save_abort(void)
{
    save_reset_state();
    save_retry_flag = SAVE_FLAG_ABORT;
}

/* Refresh the save/load screen buffer and advance its current state. */
static void save_done_screen(void)
{
    u_long *ot;

    ot = current_frame->otTags[0];
    SHOW_MESSAGE(ot, LOCALIZED(text_bank.messages.data_has_been_saved));
    save_step = save_menu_wait_confirm(SAVE_STEP_DONE, SAVE_STEP_BEGIN_MESSAGE);
    if (save_menu_index >= SAVE_DONE_TIMEOUT)
    {
        save_step = SAVE_STEP_BEGIN_MESSAGE;
    }
    if (save_step != SAVE_STEP_DONE)
    {
        save_reset_state();
        save_retry_flag = SAVE_FLAG_DONE;
    }
}

/* Run the current load step (save_step). */
static void load_run_step(void)
{
    s32 card_code;

    switch (save_step)
    {
    case LOAD_STEP_BEGIN_MESSAGE:
        load_begin_message();
        return;
    case LOAD_STEP_PROBE_BOTH:
        load_probe_both_status();
        return;
    case LOAD_STEP_NO_CARD:
        load_no_card_screen();
        break;
    case LOAD_STEP_CHOOSE_CARD:
        load_choose_card_screen();
        break;
    case LOAD_STEP_PROBE_RESULT:
        load_probe_result_screen();
        return;
    case LOAD_STEP_NO_CARD_SLOT:
        load_no_card_slot_screen();
        break;
    case LOAD_STEP_DAMAGED_CARD:
        load_damaged_card_screen();
        break;
    case LOAD_STEP_READ_SLOT_HEADERS:
        load_read_slot_headers();
        return;
    case LOAD_STEP_NO_LOADABLE:
        load_no_loadable_screen();
        break;
    case LOAD_STEP_SLOT_SELECT:
        load_slot_select_screen();
        break;
    case LOAD_STEP_NO_DIARY_HERE:
        load_no_diary_here_screen();
        break;
    case LOAD_STEP_BEGIN_FILE:
        load_begin_file();
        break;
    case LOAD_STEP_READ_FILE:
        load_read_file_step();
        return;
    case LOAD_STEP_VERIFY_IMAGE:
        load_verify_image_step();
        break;
    case LOAD_STEP_DAMAGED_DIARY:
        load_damaged_diary_screen();
        break;
    case LOAD_STEP_FAILED:
        load_failed_screen();
        break;
    case LOAD_STEP_DISCONNECTED:
        load_disconnected_screen();
        break;
    case LOAD_STEP_ABORT_CONFIRM:
        load_abort_confirm_screen();
        break;
    case LOAD_STEP_ABORT:
        load_abort();
        return;
    case LOAD_STEP_DONE:
        load_done_screen();
        return;
    }

    card_code = save_card_poll(save_probe_result == SAVE_PORT_BOTH ? save_card_index : save_probe_result);
    if (card_code == SAVE_CARD_BUSY)
    {
        return;
    }
    if (card_code != save_card_status[save_probe_result == SAVE_PORT_BOTH ? save_card_index : save_probe_result])
    {
        if (card_code == SAVE_CARD_NEW)
        {
            save_step = LOAD_STEP_BEGIN_MESSAGE;
        }
        if (card_code == SAVE_CARD_NO_CARD)
        {
            if (save_probe_result == SAVE_PORT_BOTH)
            {
                save_probe_result = save_card_index;
            }
            save_step = LOAD_STEP_DISCONNECTED;
        }
    }
    if (save_probe_result == SAVE_PORT_BOTH)
    {
        if (save_card_status[save_card_index] != SAVE_CARD_UNFORMATTED && save_card_status[save_card_index] != SAVE_CARD_UNFORMATTED_MCX)
        {
            save_card_status[save_card_index] = card_code;
        }
        save_card_index = save_card_index == 0;
    }
    else
    {
        if (save_card_status[save_card_index] != SAVE_CARD_UNFORMATTED && save_card_status[save_card_index] != SAVE_CARD_UNFORMATTED_MCX)
        {
            save_card_status[save_probe_result] = card_code;
        }
    }
}

/* Start a load: say the card is being searched, then probe both slots. */
static void load_begin_message(void)
{
    u_long *p = current_frame->otTags[0];
    SHOW_MESSAGE(p, LOCALIZED(text_bank.messages.searching_the_memory_card));
    save_reset_state();
    save_step = LOAD_STEP_PROBE_BOTH;
}

/* Refresh the save/load display and record the status of both storage slots. */
static void load_probe_both_status(void)
{
    u_long *ot = current_frame->otTags[0];
    s32 i;
    s32 status;

    SHOW_MESSAGE(ot, LOCALIZED(text_bank.messages.searching_the_memory_card));
    if (save_menu_index >= SAVE_PROBE_SETTLE_FRAMES)
    {
        for (i = 0; i < 2; i++)
        {
            status = save_card_probe((s16)i);
            if (status == SAVE_CARD_READY)
            {
                status = SAVE_CARD_OK;
            }
            if (status == SAVE_CARD_UNFORMATTED_MCX)
            {
                status = SAVE_CARD_UNFORMATTED;
            }
            if (status != save_card_status[i])
            {
                save_card_status[i] = status;
            }
        }
        if (save_card_status[0] == SAVE_CARD_NO_CARD && save_card_status[1] == SAVE_CARD_NO_CARD)
        {
            save_step = LOAD_STEP_NO_CARD;
        }
        else
        {
            save_step = LOAD_STEP_CHOOSE_CARD;
        }
    }
}

/* Say neither slot holds a card; the confirm press goes on to LOAD_STEP_ABORT_CONFIRM. */
static void load_no_card_screen(void)
{
    u_long *p = current_frame->otTags[0];
    SHOW_MESSAGE(p, LOCALIZED(text_bank.messages.no_memory_card_in_either_slot));
    save_step = save_menu_wait_confirm(LOAD_STEP_NO_CARD, LOAD_STEP_ABORT_CONFIRM);
    save_confirm_flag = 1;
}

/* Refresh the save/load display and probe the storage; falls to LOAD_STEP_ABORT_CONFIRM when the probe fails. */
SAVELOAD_PROBE_ENTRY(load_choose_card_screen, LOAD_STEP_ABORT_CONFIRM, LOAD_STEP_PROBE_RESULT)

/* Refresh the save/load display and select the resulting screen. */
static void load_probe_result_screen(void)
{
    u_long *ot;
    s32 card_code;
    s16 *status;
    ot = current_frame->otTags[0];
    SHOW_MESSAGE(ot, LOCALIZED(text_bank.messages.searching_the_memory_card));
    if (save_menu_index < SAVE_PROBE_SETTLE_FRAMES)
        return;
    card_code = save_card_probe(save_probe_result);
    card_code = (s16)(card_code - 4);
    switch (card_code)
    {
    case 0:
        status = save_card_status;
        if (status[save_probe_result] == SAVE_CARD_OK)
        {
            save_step = LOAD_STEP_DISCONNECTED;
            break;
        }
        save_step = LOAD_STEP_NO_CARD_SLOT;
        break;
    case 1:
        save_step = LOAD_STEP_PROBE_RESULT;
        break;
    case 4:
        save_step = LOAD_STEP_DAMAGED_CARD;
        break;
    case 2:
    case 3:
        save_step = LOAD_STEP_NO_LOADABLE;
        break;
    default:
        save_step = LOAD_STEP_READ_SLOT_HEADERS;
        break;
    }
}

SAVELOAD_MENU_ENTRY(load_no_card_slot_screen, text_bank.messages.no_memory_card_in_slot, LOAD_STEP_NO_CARD_SLOT, LOAD_STEP_BEGIN_MESSAGE)

SAVELOAD_MENU_ENTRY(load_damaged_card_screen, text_bank.messages.memory_card_in_slot_damaged, LOAD_STEP_DAMAGED_CARD, LOAD_STEP_BEGIN_MESSAGE)

/* Probe the three save slots, record each slot's status and pick the next step. */
static void load_read_slot_headers(void)
{
    u_long *ot;
    s32 i;
    s16 card_code;
    s32 check;
    u16 kind;

    ot = current_frame->otTags[0];
    SHOW_MESSAGE(ot, LOCALIZED(text_bank.messages.searching_the_memory_card));
    for (i = 0; i < 3; i++)
    {
        save_slot_status[i] = SAVE_SLOT_SHORT_OF_SPACE;
        card_code = save_card_read_header(save_probe_result, i, &save_slot_records[i]);
        if (card_code == SAVE_CARD_DAMAGED)
        {
            save_step = LOAD_STEP_DAMAGED_CARD;
            return;
        }
        if (card_code == SAVE_CARD_NO_CARD || card_code == SAVE_CARD_NEW)
        {
            save_step = LOAD_STEP_DISCONNECTED;
            return;
        }
        if (card_code == SAVE_CARD_NO_FILE)
        {
            check = save_card_check_space(save_probe_result, i);
            if (check == SAVE_CARD_OK)
            {
                save_slot_status[i] = SAVE_SLOT_NO_DIARY;
            }
            else if (check == SAVE_CARD_FULL)
            {
                save_slot_status[i] = SAVE_SLOT_NO_FREE_SPACE;
            }
            else
            {
                save_slot_status[i] = SAVE_SLOT_SHORT_OF_SPACE;
            }
        }
        else if (card_code == SAVE_CARD_OK)
        {
            save_slot_status[i] = SAVE_SLOT_DIARY;
        }
        kind = save_card_status[save_probe_result];
        if (kind == SAVE_CARD_UNFORMATTED || kind == SAVE_CARD_UNFORMATTED_MCX)
        {
            save_slot_status[i] = SAVE_SLOT_NO_DIARY;
        }
    }
    if (save_slot_status[0] != SAVE_SLOT_DIARY && save_slot_status[1] != SAVE_SLOT_DIARY && save_slot_status[2] != SAVE_SLOT_DIARY)
    {
        save_step = LOAD_STEP_NO_LOADABLE;
        return;
    }
    save_confirm_cursor = 0;
    save_step = LOAD_STEP_SLOT_SELECT;
}

SAVELOAD_MENU_ENTRY(load_no_loadable_screen, text_bank.messages.no_loadable_diary_in_memory_card, LOAD_STEP_NO_LOADABLE, LOAD_STEP_BEGIN_MESSAGE)

/* Refresh the save/load display and classify the storage probe result. */
static void load_slot_select_screen(void)
{
    u_long *ot;
    s32 choice;

    ot = current_frame->otTags[0];
    SHOW_MESSAGE(ot, LOCALIZED(text_bank.messages.select_a_diary_to_load));
    choice = save_menu_choose_three(0);
    if (choice < 3)
    {
        if (choice < 0)
        {
            if (choice == -1)
            {
                save_probe_result = SAVE_PORT_BOTH;
                save_step = LOAD_STEP_BEGIN_MESSAGE;
            }
        }
        else
        {
            if (save_slot_status[choice] != 0)
            {
                save_step = LOAD_STEP_NO_DIARY_HERE;
            }
            else
            {
                save_slot = choice;
                save_step = LOAD_STEP_BEGIN_FILE;
            }
        }
    }
}

SAVELOAD_MENU_ENTRY(load_no_diary_here_screen, text_bank.messages.there_is_no_diary_here, LOAD_STEP_NO_DIARY_HERE, LOAD_STEP_BEGIN_MESSAGE)

/* Say the chosen slot's file is loading, pick its file name and read it. */
static void load_begin_file(void)
{
    u_long *p = current_frame->otTags[0];
    SHOW_MESSAGE(p, LOCALIZED(text_bank.messages.loading_from_memory_card));
    save_file_name(save_file_name_buf, save_slot);
    save_step = LOAD_STEP_READ_FILE;
}

/* Refresh the save/load display, then dispatch the storage result to a screen step. */
static void load_read_file_step(void)
{
    u_long *ot;
    s32 card_code;

    ot = current_frame->otTags[0];
    SHOW_MESSAGE(ot, LOCALIZED(text_bank.messages.loading_from_memory_card));
    card_code = save_card_read_file(save_probe_result, save_slot, save_file_header);
    switch (card_code)
    {
    case SAVE_CARD_NO_CARD:
        save_step = LOAD_STEP_DISCONNECTED;
        break;
    case SAVE_CARD_FAILED:
    case SAVE_CARD_NEW:
        save_step = LOAD_STEP_FAILED;
        break;
    case SAVE_CARD_OK:
        save_step = LOAD_STEP_VERIFY_IMAGE;
        break;
    case SAVE_CARD_READY:
    case SAVE_CARD_BUSY:
        break;
    }
}

/* Refresh the load display, check the checksum of the saved image, and choose the next step. */
static void load_verify_image_step(void)
{
    u_long *ot;
    s32 check;
    ot = current_frame->otTags[0];
    text_draw_in_frame(&ot[6], &ot[5], LOCALIZED(text_bank.messages.loading_from_memory_card));
    check = save_image_checksum_verify();
    if (check == SAVE_CHECKSUM_BAD)
    {
        save_step = LOAD_STEP_DAMAGED_DIARY;
    }
    else if (save_slot_records[save_slot].tag != SAVE_SLOT_TAG)
    {
        save_step = LOAD_STEP_DAMAGED_DIARY;
    }
    else
    {
        save_step = LOAD_STEP_DONE;
    }
}

SAVELOAD_MENU_ENTRY(load_damaged_diary_screen, text_bank.messages.the_diary_is_damaged_loading_failed, LOAD_STEP_DAMAGED_DIARY, LOAD_STEP_BEGIN_MESSAGE)

SAVELOAD_MENU_ENTRY(load_failed_screen, text_bank.messages.loading_has_failed, LOAD_STEP_FAILED, LOAD_STEP_BEGIN_MESSAGE)

SAVELOAD_MENU_ENTRY(load_disconnected_screen, text_bank.messages.memory_card_has_been_disconnected_from, LOAD_STEP_DISCONNECTED, LOAD_STEP_BEGIN_MESSAGE)

/* Ask whether to abort the load; the answer picks the next step. */
static void load_abort_confirm_screen(void)
{
    u_long *ot;
    s32 answer;

    ot = current_frame->otTags[0];
    SHOW_MESSAGE(ot, LOCALIZED(text_bank.messages.aborting_load_is_that_ok));
    answer = save_confirm_prompt();
    switch (answer)
    {
    case SAVE_CARD_OK:
        save_confirm_cursor = 0;
        save_step = LOAD_STEP_ABORT;
        break;
    case SAVE_CARD_READY:
    case -1:
        save_step = LOAD_STEP_BEGIN_MESSAGE;
        break;
    }
}

/* Abandon the load and flag it aborted. */
static void load_abort(void)
{
    save_reset_state();
    save_retry_flag = SAVE_FLAG_ABORT;
}

/* Show that the data has been loaded and wait for the player to confirm. */
static void load_done_screen(void)
{
    u_long *ot;

    ot = current_frame->otTags[0];
    SHOW_MESSAGE(ot, LOCALIZED(text_bank.messages.data_has_been_loaded));
    save_step = save_menu_wait_confirm(LOAD_STEP_DONE, LOAD_STEP_BEGIN_MESSAGE);
    if (save_menu_index >= SAVE_DONE_TIMEOUT)
        save_step = LOAD_STEP_BEGIN_MESSAGE;
    if (save_step != LOAD_STEP_DONE)
    {
        save_image_shift_out();
        save_unpack_farm_state();
        save_reset_state();
        save_retry_flag = SAVE_FLAG_DONE;
    }
}

/* Run the current save/load step, then poll the memory card and react to a
 * change in its state. */
static void save_transfer_run_step(void)
{
    s32 status;

    switch (save_step)
    {
    case TRANSFER_STEP_BEGIN_MESSAGE:
        save_transfer_begin_message();
        return;
    case TRANSFER_STEP_PROBE_SLOTS:
        save_transfer_probe_slots();
        return;
    case TRANSFER_STEP_NO_CARD:
        save_transfer_no_card_screen();
        break;
    case TRANSFER_STEP_CHOOSE_CARD:
        save_transfer_choose_card_screen();
        break;
    case TRANSFER_STEP_PROBE_RESULT:
        save_transfer_probe_result_screen();
        return;
    case TRANSFER_STEP_NO_CARD_SLOT:
        save_transfer_no_card_slot_screen();
        break;
    case TRANSFER_STEP_DAMAGED_CARD:
        save_transfer_damaged_card_screen();
        break;
    case TRANSFER_STEP_PROBE_THREE_SLOTS:
        save_transfer_read_slot_headers();
        return;
    case TRANSFER_STEP_NO_LOADABLE:
        save_transfer_no_loadable_screen();
        break;
    case TRANSFER_STEP_SLOT_SELECT:
        save_transfer_slot_select_screen();
        break;
    case TRANSFER_STEP_NO_DIARY_HERE:
        save_transfer_no_diary_here_screen();
        break;
    case TRANSFER_STEP_BEGIN_FILE:
        save_transfer_begin_file();
        break;
    case TRANSFER_STEP_READ_FILE:
        save_transfer_read_file_step();
        return;
    case TRANSFER_STEP_VERIFY_IMAGE:
        save_transfer_verify_image_step();
        break;
    case TRANSFER_STEP_DAMAGED_DIARY:
        save_transfer_damaged_diary_screen();
        break;
    case TRANSFER_STEP_FAILED:
        save_transfer_failed_screen();
        break;
    case TRANSFER_STEP_DISCONNECTED:
        save_transfer_disconnected_screen();
        break;
    case TRANSFER_STEP_CHECK_SAME_GAME:
        save_transfer_check_same_game_step();
        break;
    case TRANSFER_STEP_SAME_GAME:
        save_transfer_same_game_screen();
        break;
    case TRANSFER_STEP_FARM_NAME:
        save_transfer_farm_name_step();
        break;
    case TRANSFER_STEP_SAVE_NAME:
        save_transfer_save_name_step();
        break;
    case TRANSFER_STEP_CHECK_FIT:
        save_check_transfer_fit();
        break;
    case TRANSFER_STEP_NO_ANIMAL:
        save_transfer_no_animal_screen();
        break;
    case TRANSFER_STEP_HENS_FULL:
        save_transfer_hens_full_screen();
        break;
    case TRANSFER_STEP_PARTNER_HENS_FULL:
        save_transfer_partner_hens_full_screen();
        break;
    case TRANSFER_STEP_COWS_FULL:
        save_transfer_cows_full_screen();
        break;
    case TRANSFER_STEP_PARTNER_COWS_FULL:
        save_transfer_partner_cows_full_screen();
        break;
    case TRANSFER_STEP_CONFIRM:
        save_transfer_confirm_screen();
        break;
    case TRANSFER_STEP_SLOT_CONFIRM:
        save_transfer_slot_confirm_screen();
        break;
    case TRANSFER_STEP_WRITE:
        save_transfer_write_step();
        return;
    case TRANSFER_STEP_WRITE_FAILED:
        save_transfer_write_failed_screen();
        break;
    case TRANSFER_STEP_ABORT_CONFIRM:
        save_transfer_abort_confirm_screen();
        break;
    case TRANSFER_STEP_ABORT:
        save_transfer_abort();
        return;
    case TRANSFER_STEP_DONE:
        save_transfer_done_screen();
        return;
    }

    status = save_card_poll(save_probe_result == SAVE_PORT_BOTH ? save_card_index : save_probe_result);
    if (status == SAVE_CARD_BUSY)
    {
        return;
    }
    if (status != save_card_status[save_probe_result == SAVE_PORT_BOTH ? save_card_index : save_probe_result])
    {
        if (status == SAVE_CARD_NEW)
        {
            save_step = TRANSFER_STEP_BEGIN_MESSAGE;
        }
        if (status == SAVE_CARD_NO_CARD)
        {
            if (save_probe_result == SAVE_PORT_BOTH)
            {
                save_probe_result = save_card_index;
            }
            save_step = TRANSFER_STEP_DISCONNECTED;
        }
    }
    if (save_probe_result == SAVE_PORT_BOTH)
    {
        if (save_card_status[save_card_index] != SAVE_CARD_UNFORMATTED && save_card_status[save_card_index] != SAVE_CARD_UNFORMATTED_MCX)
        {
            save_card_status[save_card_index] = status;
        }
        save_card_index = save_card_index == 0;
    }
    else if (save_card_status[save_card_index] != SAVE_CARD_UNFORMATTED && save_card_status[save_card_index] != SAVE_CARD_UNFORMATTED_MCX)
    {
        save_card_status[save_probe_result] = status;
    }
}

/* Start a transfer: say the card is being searched, clear the transfer tables and probe the slots. */
static void save_transfer_begin_message(void)
{
    u_long *ot = current_frame->otTags[0];
    SHOW_MESSAGE(ot, LOCALIZED(text_bank.messages.searching_the_memory_card));
    save_reset_state();
    save_step = TRANSFER_STEP_PROBE_SLOTS;
    save_reset_work_tables();
}
/* Probe the slots once the settle delay has passed. */
static void save_transfer_probe_slots(void)
{
    u_long *ot;
    s32 card_code;
    s32 slot;

    ot = current_frame->otTags[0];
    SHOW_MESSAGE(ot, LOCALIZED(text_bank.messages.searching_the_memory_card));
    if (save_menu_index < SAVE_PROBE_SETTLE_FRAMES)
    {
        return;
    }
    slot = 0;
    do
    {
        card_code = save_card_probe((s16)slot);
        if (card_code == SAVE_CARD_READY)
        {
            card_code = SAVE_CARD_OK;
        }
        if (card_code == SAVE_CARD_UNFORMATTED_MCX)
        {
            card_code = SAVE_CARD_UNFORMATTED;
        }
        if (card_code != save_card_status[slot])
        {
            save_card_status[slot] = card_code;
        }
        slot++;
    } while (slot < 2);
    if (save_card_status[0] == SAVE_CARD_NO_CARD && save_card_status[1] == SAVE_CARD_NO_CARD)
    {
        save_step = TRANSFER_STEP_NO_CARD;
    }
    else
    {
        save_step = TRANSFER_STEP_CHOOSE_CARD;
    }
}
/* Say neither slot holds a card; the confirm press goes on to TRANSFER_STEP_ABORT_CONFIRM. */
static void save_transfer_no_card_screen(void)
{
    u_long *p = current_frame->otTags[0];
    SHOW_MESSAGE(p, LOCALIZED(text_bank.messages.no_memory_card_in_either_slot));
    save_step = save_menu_wait_confirm(TRANSFER_STEP_NO_CARD, TRANSFER_STEP_ABORT_CONFIRM);
    save_confirm_flag = 1;
}
SAVELOAD_PROBE_ENTRY(save_transfer_choose_card_screen, TRANSFER_STEP_ABORT_CONFIRM, TRANSFER_STEP_PROBE_RESULT)

/* Refresh the save/load display and choose the storage result screen. */
static void save_transfer_probe_result_screen(void)
{
    s32 card_code;

    {
        u_long *ot;
        ot = current_frame->otTags[0];
        SHOW_MESSAGE(ot, LOCALIZED(text_bank.messages.searching_the_memory_card));
    }
    if (save_menu_index < SAVE_PROBE_SETTLE_FRAMES)
    {
        return;
    }
    card_code = save_card_probe(save_probe_result);
    switch (card_code)
    {
    case SAVE_CARD_NO_CARD:
        if (save_card_status[save_probe_result] == SAVE_CARD_OK)
        {
            save_step = TRANSFER_STEP_DISCONNECTED;
        }
        else
        {
            save_step = TRANSFER_STEP_NO_CARD_SLOT;
        }
        break;
    case SAVE_CARD_DAMAGED:
        save_step = TRANSFER_STEP_DAMAGED_CARD;
        break;
    default:
        save_step = TRANSFER_STEP_PROBE_THREE_SLOTS;
        break;
    }
}

SAVELOAD_MENU_ENTRY(save_transfer_no_card_slot_screen, text_bank.messages.no_memory_card_in_slot, TRANSFER_STEP_NO_CARD_SLOT, TRANSFER_STEP_BEGIN_MESSAGE)

SAVELOAD_MENU_ENTRY(save_transfer_damaged_card_screen, text_bank.messages.memory_card_in_slot_damaged, TRANSFER_STEP_DAMAGED_CARD, TRANSFER_STEP_BEGIN_MESSAGE)

/* Probe the three save slots, record each one's status in save_slot_status and pick
 * the next screen: TRANSFER_STEP_DAMAGED_CARD or TRANSFER_STEP_DISCONNECTED on a probe
 * error, TRANSFER_STEP_NO_LOADABLE when no slot holds a loadable diary, else TRANSFER_STEP_SLOT_SELECT. */
static void save_transfer_read_slot_headers(void)
{
    u_long *ot;
    s32 i;
    s16 card_code;
    s32 check;
    s16 *status;

    ot = current_frame->otTags[0];
    SHOW_MESSAGE(ot, LOCALIZED(text_bank.messages.searching_the_memory_card));
    for (i = 0; i < 3; i++)
    {
        save_slot_status[i] = SAVE_SLOT_SHORT_OF_SPACE;
        card_code = save_card_read_header(save_probe_result, i, &save_slot_records[i]);
        if (card_code == SAVE_CARD_DAMAGED)
        {
            save_step = TRANSFER_STEP_DAMAGED_CARD;
            return;
        }
        if (card_code == SAVE_CARD_NO_CARD || card_code == SAVE_CARD_NEW)
        {
            save_step = TRANSFER_STEP_DISCONNECTED;
            return;
        }
        if (card_code == SAVE_CARD_NO_FILE)
        {
            check = save_card_check_space(save_probe_result, i);
            if (check == SAVE_CARD_OK)
            {
                save_slot_status[i] = SAVE_SLOT_NO_DIARY;
            }
            else if (check == SAVE_CARD_FULL)
            {
                save_slot_status[i] = SAVE_SLOT_NO_FREE_SPACE;
            }
            else
            {
                save_slot_status[i] = SAVE_SLOT_SHORT_OF_SPACE;
            }
        }
        else if (card_code == SAVE_CARD_OK)
        {
            save_slot_status[i] = SAVE_SLOT_DIARY;
        }
        if (save_card_status[save_probe_result] == SAVE_CARD_UNFORMATTED || save_card_status[save_probe_result] == SAVE_CARD_UNFORMATTED_MCX)
        {
            save_slot_status[i] = SAVE_SLOT_NO_DIARY;
        }
    }
    status = save_slot_status;
    if (status[0] != 0 && status[1] != 0 && status[2] != 0)
    {
        save_step = TRANSFER_STEP_NO_LOADABLE;
        return;
    }
    save_confirm_cursor = 0;
    save_step = TRANSFER_STEP_SLOT_SELECT;
}

SAVELOAD_MENU_ENTRY(save_transfer_no_loadable_screen, text_bank.messages.no_loadable_diary_in_memory_card, TRANSFER_STEP_NO_LOADABLE, TRANSFER_STEP_BEGIN_MESSAGE)

/* Refresh the save/load display and classify the storage probe result. */
static void save_transfer_slot_select_screen(void)
{
    u_long *ot;
    s32 choice;

    ot = current_frame->otTags[0];
    SHOW_MESSAGE(ot, LOCALIZED(text_bank.messages.select_a_diary_to_load));
    choice = save_menu_choose_three(0);
    if (choice < 3)
    {
        if (choice < 0)
        {
            if (choice == -1)
            {
                save_probe_result = SAVE_PORT_BOTH;
                save_step = TRANSFER_STEP_BEGIN_MESSAGE;
            }
        }
        else
        {
            if (save_slot_status[choice] != 0)
            {
                save_step = TRANSFER_STEP_NO_DIARY_HERE;
            }
            else
            {
                save_slot = choice;
                save_step = TRANSFER_STEP_BEGIN_FILE;
            }
        }
    }
}

SAVELOAD_MENU_ENTRY(save_transfer_no_diary_here_screen, text_bank.messages.there_is_no_diary_here, TRANSFER_STEP_NO_DIARY_HERE, TRANSFER_STEP_BEGIN_MESSAGE)

/* Say the chosen slot's file is loading, pick its file name and read it. */
static void save_transfer_begin_file(void)
{
    u_long *p = current_frame->otTags[0];
    SHOW_MESSAGE(p, LOCALIZED(text_bank.messages.loading_from_memory_card));
    save_file_name(save_file_name_buf, save_slot);
    save_step = TRANSFER_STEP_READ_FILE;
}

/* Refresh the save/load display, then dispatch the storage result to a screen step. */
static void save_transfer_read_file_step(void)
{
    u_long *ot;
    s32 card_code;

    ot = current_frame->otTags[0];
    SHOW_MESSAGE(ot, LOCALIZED(text_bank.messages.loading_from_memory_card));
    card_code = save_card_read_file(save_probe_result, save_slot, save_file_header);
    switch (card_code)
    {
    case SAVE_CARD_NO_CARD:
        save_step = TRANSFER_STEP_DISCONNECTED;
        break;
    case SAVE_CARD_FAILED:
    case SAVE_CARD_NEW:
        save_step = TRANSFER_STEP_FAILED;
        break;
    case SAVE_CARD_OK:
        save_step = TRANSFER_STEP_VERIFY_IMAGE;
        break;
    case SAVE_CARD_READY:
    case SAVE_CARD_BUSY:
        break;
    }
}

/* Refresh the save/load display and record whether the selected slot is ready. */
static void save_transfer_verify_image_step(void)
{
    u_long *ot;
    s32 check;
    ot = current_frame->otTags[0];
    SHOW_MESSAGE(ot, LOCALIZED(text_bank.messages.loading_from_memory_card));
    check = save_image_checksum_verify();
    if (check == SAVE_CHECKSUM_BAD)
    {
        save_step = TRANSFER_STEP_DAMAGED_DIARY;
    }
    else if (save_slot_records[save_slot].tag != SAVE_SLOT_TAG)
    {
        save_step = TRANSFER_STEP_DAMAGED_DIARY;
    }
    else
    {
        save_image_shift_out();
        save_step = TRANSFER_STEP_CHECK_SAME_GAME;
    }
}

SAVELOAD_MENU_ENTRY(save_transfer_damaged_diary_screen, text_bank.messages.the_diary_is_damaged_loading_failed, TRANSFER_STEP_DAMAGED_DIARY, TRANSFER_STEP_BEGIN_MESSAGE)

SAVELOAD_MENU_ENTRY(save_transfer_failed_screen, text_bank.messages.loading_has_failed, TRANSFER_STEP_FAILED, TRANSFER_STEP_BEGIN_MESSAGE)

SAVELOAD_MENU_ENTRY(save_transfer_disconnected_screen, text_bank.messages.memory_card_has_been_disconnected_from, TRANSFER_STEP_DISCONNECTED, TRANSFER_STEP_BEGIN_MESSAGE)

/* Compare the file's header with game_state: a file from this same farm goes to TRANSFER_STEP_SAME_GAME, any other to TRANSFER_STEP_FARM_NAME. */
static void save_transfer_check_same_game_step(void)
{
    u_long *p = current_frame->otTags[0];
    SHOW_MESSAGE(p, LOCALIZED(text_bank.messages.loading_from_memory_card));
    if (memcmp(&((SaveFile *)save_image)->flagWords, &game_state, 8) == 0)
    {
        save_step = TRANSFER_STEP_SAME_GAME;
    }
    else
    {
        save_reset_work_tables();
        save_step = TRANSFER_STEP_FARM_NAME;
    }
}

/* Say the file is from this same game and cannot be exchanged; the confirm press or a timeout goes back to slot select. */
static void save_transfer_same_game_screen(void)
{
    u_long *p;
    p = current_frame->otTags[0];
    SHOW_MESSAGE(p, LOCALIZED(text_bank.messages.cannot_exchange_with_diary_from_same));
    save_step = save_menu_wait_confirm(TRANSFER_STEP_SAME_GAME, TRANSFER_STEP_SLOT_SELECT);
    if (save_menu_index >= SAVE_DONE_TIMEOUT)
        save_step = TRANSFER_STEP_SLOT_SELECT;
}

/* Draw the farm-name panel; when it is confirmed, move on to the save-name step. */
static void save_transfer_farm_name_step(void)
{
    s32 panel;

    panel = save_draw_name_panel(0);
    switch (panel)
    {
    case SAVE_CARD_OK:
        break;
    case SAVE_CARD_READY:
        save_reset_cursor();
        save_step = TRANSFER_STEP_SAVE_NAME;
        break;
    case -1:
        save_step = TRANSFER_STEP_SLOT_SELECT;
        break;
    }
}

/* Run the name panel for the transferred file: confirm goes on to the fit check, cancel back to the farm name. */
static void save_transfer_save_name_step(void)
{
    s32 panel;
    panel = save_draw_name_panel(1);
    switch (panel)
    {
    case 0:
        break;
    case 1:
        save_step = TRANSFER_STEP_CHECK_FIT;
        break;
    case -1:
        save_reset_cursor();
        save_step = TRANSFER_STEP_FARM_NAME;
        break;
    }
}

/* Check that the queued transfers fit in both the current game and the save file. */
static void save_check_transfer_fit(void)
{
    u_long *ot;
    s16 total[2];
    s16 hens[2];
    s16 cows[2];
    s16 haveHens[2];
    s16 haveCows[2];
    s16 hensLimit[2];
    s16 cowsLimit[2];
    s32 i;
    s32 j;

    ot = current_frame->otTags[0];
    SHOW_MESSAGE(ot, LOCALIZED(text_bank.messages.select_up_to_5_animals_cant));
    for (i = 0; i < 2; i++)
    {
        total[i] = hens[i] = cows[i] = 0;
        for (j = 0; j < SAVE_LIST_ENTRIES; j++)
        {
            if (save_entries[i][j].kind == 0)
            {
                hens[i]++;
            }
            if (save_entries[i][j].kind == 1)
            {
                cows[i]++;
            }
            if (save_entries[i][j].kind == 2)
            {
                cows[i]++;
            }
        }
        total[i] = hens[i] + cows[i];
    }
    haveHens[0] = save_count_records(&game_state.animals, SAVE_TABLE_CHICKENS);
    haveHens[1] = save_count_records(save_image, SAVE_TABLE_CHICKENS);
    haveCows[0] = save_count_records(&game_state.animals, SAVE_TABLE_LIVESTOCK);
    haveCows[1] = save_count_records(save_image, SAVE_TABLE_LIVESTOCK);
    hensLimit[0] = game_state.progress.estate.coopLevel == 0 ? COOP_CAPACITY_BASIC : COOP_CAPACITY_UPGRADED;
    hensLimit[1] = ((FarmEstate *)&((SaveFile *)save_image)->estate)->coopLevel == 0 ? COOP_CAPACITY_BASIC : COOP_CAPACITY_UPGRADED;
    cowsLimit[0] = game_state.progress.estate.barnLevel == 0 ? BARN_CAPACITY_BASIC : BARN_CAPACITY_UPGRADED;
    cowsLimit[1] = ((FarmEstate *)&((SaveFile *)save_image)->estate)->barnLevel == 0 ? BARN_CAPACITY_BASIC : BARN_CAPACITY_UPGRADED;
    if (total[0] == 0 && total[1] == 0)
    {
        save_step = TRANSFER_STEP_NO_ANIMAL;
    }
    else if (hensLimit[0] < haveHens[0] - hens[0] + hens[1])
    {
        save_step = TRANSFER_STEP_HENS_FULL;
    }
    else if (hensLimit[1] < haveHens[1] - hens[1] + hens[0])
    {
        save_step = TRANSFER_STEP_PARTNER_HENS_FULL;
    }
    else if (cowsLimit[0] < haveCows[0] - cows[0] + cows[1])
    {
        save_step = TRANSFER_STEP_COWS_FULL;
    }
    else if (cowsLimit[1] < haveCows[1] - cows[1] + cows[0])
    {
        save_step = TRANSFER_STEP_PARTNER_COWS_FULL;
    }
    else
    {
        save_confirm_flag = 1;
        save_step = TRANSFER_STEP_CONFIRM;
    }
}

SAVELOAD_CONFIRM_ENTRY(save_transfer_no_animal_screen, text_bank.messages.no_animal_selected, TRANSFER_STEP_NO_ANIMAL, TRANSFER_STEP_FARM_NAME)

SAVELOAD_CONFIRM_ENTRY(save_transfer_hens_full_screen, text_bank.messages.that_puts_too_many_hens_into, TRANSFER_STEP_HENS_FULL, TRANSFER_STEP_FARM_NAME)

SAVELOAD_CONFIRM_ENTRY(save_transfer_partner_hens_full_screen, text_bank.messages.that_gives_partner_too_many_hens, TRANSFER_STEP_PARTNER_HENS_FULL, TRANSFER_STEP_FARM_NAME)

SAVELOAD_CONFIRM_ENTRY(save_transfer_cows_full_screen, text_bank.messages.that_puts_too_many_cows_sheep, TRANSFER_STEP_COWS_FULL, TRANSFER_STEP_FARM_NAME)

SAVELOAD_CONFIRM_ENTRY(save_transfer_partner_cows_full_screen, text_bank.messages.that_gives_partner_too_many_cows, TRANSFER_STEP_PARTNER_COWS_FULL, TRANSFER_STEP_FARM_NAME)

/* Ask whether to trade the livestock; the answer picks the next step. */
static void save_transfer_confirm_screen(void)
{
    s32 answer;
    u_long *ot;

    save_draw_transfer_lists();
    ot = current_frame->otTags[0];
    SHOW_MESSAGE(ot, LOCALIZED(text_bank.messages.trading_livestock_is_that_ok));
    answer = save_confirm_prompt();
    switch (answer)
    {
    case SAVE_CARD_READY:
    case -1:
        save_reset_cursor();
        save_step = TRANSFER_STEP_SAVE_NAME;
        break;
    case SAVE_CARD_OK:
        save_confirm_flag = 1;
        save_step = TRANSFER_STEP_SLOT_CONFIRM;
        break;
    }
}

/* Draw the slot rows, then ask whether to overwrite the diary in the chosen slot. */
static void save_transfer_slot_confirm_screen(void)
{
    s32 slot;
    s32 answer;
    slot = 0;
    text_draw_in_frame(&current_frame->otTags[0][6], &current_frame->otTags[0][5],
                       LOCALIZED(text_bank.messages.ok_to_overwrite_this_diary));
    do
    {
        save_draw_slot_row(slot, 0);
        slot++;
    } while (slot < 3);
    ui_draw_icon_0b_1(&current_frame->otTags[0][1], 57, (save_confirm_cursor * SAVE_SLOT_ROW_STEP) + 24);
    answer = save_confirm_prompt();
    switch (answer)
    {
    case SAVE_CARD_READY:
    case -1:
        save_reset_cursor();
        save_confirm_flag = 1;
        save_step = TRANSFER_STEP_CONFIRM;
        break;
    case SAVE_CARD_OK:
        save_transfer_stash();
        save_image_shift_in();
        save_image_checksum_write();
        save_step = TRANSFER_STEP_WRITE;
        break;
    }
}

/* Refresh the save/load display and advance the screen state. */
static void save_transfer_write_step(void)
{
    u_long *ot;
    s32 card_code;

    ot = current_frame->otTags[0];
    SHOW_MESSAGE(ot, LOCALIZED(text_bank.messages.saving_to_memory_card));
    card_code = save_card_write_file(save_probe_result, save_slot, save_file_header);
    switch (card_code)
    {
    case SAVE_CARD_FAILED:
    case SAVE_CARD_NEW:
        save_step = TRANSFER_STEP_WRITE_FAILED;
        break;
    case SAVE_CARD_NO_CARD:
        save_step = TRANSFER_STEP_DISCONNECTED;
        break;
    case SAVE_CARD_OK:
        save_transfer_restore();
        save_step = TRANSFER_STEP_DONE;
        break;
    case SAVE_CARD_READY:
    case SAVE_CARD_BUSY:
        break;
    }
}

/* The card title: full-width "HARVEST MOON FILE" in Shift-JIS. */
static const char save_title_text[36] =
    "\202g\202`\202q\202u\202d\202r\202s\201@\202l\202n\202n\202m\201@\202e\202h\202k\202d\000\000";
/* Shift-JIS "　Ｙｒ" (full-width space, "Yr"): between the slot number and the year. */
static const char save_year_label[] = "\201@\202x\202\222";
/* Shift-JIS "　", a full-width space. */
static const char save_full_space[] = "\201@";
/* Shift-JIS "０", a full-width zero; save_format_two_digits adds the digit to its second byte. */
static const char save_full_zero[] = "\202O";

/* Season names for the save slot display, full-width Shift-JIS. */
static char *save_season_names[4] = {
    "\202r\202\220\202\222\202\211\202\216\202\207",
    "\202r\202\225\202\215\202\215\202\205\202\222",
    "\202e\202\201\202\214\202\214\201@\201@",
    "\202v\202\211\202\216\202\224\202\205\202\222",
};

/* Five u32 offsets into the loaded payload, one per sprite (save_load_image_file). */
static u32 save_sprite_offsets[5] = {0, 0xA88, 0x1500, 0x1F64, 0x2BB0};

SAVELOAD_MENU_ENTRY(save_transfer_write_failed_screen, text_bank.messages.save_process_failed, TRANSFER_STEP_WRITE_FAILED, TRANSFER_STEP_BEGIN_MESSAGE)

/* Ask whether to abort the exchange; the answer picks the next step. */
static void save_transfer_abort_confirm_screen(void)
{
    u_long *ot;
    s32 answer;

    ot = current_frame->otTags[0];
    SHOW_MESSAGE(ot, LOCALIZED(text_bank.messages.aborting_exchange_is_that_ok));
    answer = save_confirm_prompt();
    switch (answer)
    {
    case SAVE_CARD_OK:
        save_confirm_cursor = 0;
        save_step = TRANSFER_STEP_ABORT;
        break;
    case SAVE_CARD_READY:
    case -1:
        save_step = TRANSFER_STEP_BEGIN_MESSAGE;
        break;
    }
}

/* Abandon the transfer and flag it aborted. */
static void save_transfer_abort(void)
{
    save_reset_state();
    save_retry_flag = SAVE_FLAG_ABORT;
}

/* Show that the exchange has ended and wait for the player to confirm. */
static void save_transfer_done_screen(void)
{
    u_long *ot;

    ot = current_frame->otTags[0];
    SHOW_MESSAGE(ot, LOCALIZED(text_bank.messages.exchange_has_ended));
    save_step = save_menu_wait_confirm(TRANSFER_STEP_DONE, TRANSFER_STEP_BEGIN_MESSAGE);
    if (save_menu_index >= SAVE_DONE_TIMEOUT)
    {
        save_step = TRANSFER_STEP_BEGIN_MESSAGE;
    }
    if (save_step != TRANSFER_STEP_DONE)
    {
        save_reset_state();
        save_retry_flag = SAVE_FLAG_DONE;
    }
}

/* Place an object on grid spot `slot`: rows of five spots 400 apart in x, starting at
 * the row's position in the table. It faces a random direction, and its area is 4. */
static void save_place_grid_spot(FarmActor *object, s16 slot) {
    s16 row;
    s16 col;

    row = slot / 5;
    col = slot % 5;
    object->position.vx = save_slot_row_starts[row].vx + col * 400;
    object->position.vy = save_slot_row_starts[row].vy;
    object->position.vz = save_slot_row_starts[row].vz;
    object->facing = rand_range(4);
    object->place.location = 4;
}

/* Place an animal on the spot for `slot`: slots 0.. are rows of five, 600 apart in z, and -1
 * and -2 the two extra spots (rows 4 and 5). Odd rows face 1, even rows face 3; the
 * object's area is 5. */
static void save_place_row_spot(FarmActor *object, s16 slot) {
    s16 row;
    s16 col;

    if (slot == -1) {
        row = 4;
        col = 0;
    } else if (slot == -2) {
        row = 5;
        col = 0;
    } else {
        row = slot / 5;
        col = slot % 5;
    }
    object->position.vx = save_page_positions[row].vx;
    object->position.vy = save_page_positions[row].vy;
    object->position.vz = save_page_positions[row].vz + col * 600;
    if (row & 1) {
        object->facing = 1;
    } else {
        object->facing = 3;
    }
    object->place.location = 5;
}

#define SAVE_CHARS ((FarmAnimals *)save_image)
#define FARM_CHARS (game_state.animals)
/* Pack the entries selected in list 1 into save_out_buffer and clear them from the farm's animals. */
static void save_transfer_stash(void)
{
    u8 *out;
    s16 kind;
    s16 index;
    s16 slot;
    s32 i;
    void *dst;

    out = save_out_buffer;
    slot = 0;
    for (i = 0; i < SAVE_LIST_ENTRIES; i++)
    {
        kind = save_entries[1][i].kind;
        index = save_entries[1][i].index;
        if (kind == SAVE_ENTRY_EMPTY)
        {
            continue;
        }
        switch (kind)
        {
        case SAVE_PANEL_CHICKENS:
            SAVE_CHARS->chickens[index].actor.sprite.source = 0;
            save_place_grid_spot(&SAVE_CHARS->chickens[index].actor, index);
            memcpy(out, &SAVE_CHARS->chickens[index], sizeof(FarmChicken));
            save_entries[1][i].data = out;
            out += sizeof(FarmChicken);
            memset(&SAVE_CHARS->chickens[index], 0, sizeof(FarmChicken));
            break;
        case SAVE_PANEL_CATTLE:
        case SAVE_PANEL_SHEEP:
            SAVE_CHARS->livestock[index].actor.sprite.source = 0;
            save_place_row_spot(&SAVE_CHARS->livestock[index].actor, index);
            memcpy(out, &SAVE_CHARS->livestock[index], sizeof(FarmLivestock));
            save_entries[1][i].data = out;
            out += sizeof(FarmLivestock);
            memset(&SAVE_CHARS->livestock[index], 0, sizeof(FarmLivestock));
            break;
        }
    }

    for (i = 0; i < SAVE_LIST_ENTRIES; i++)
    {
        kind = save_entries[0][i].kind;
        index = save_entries[0][i].index;
        if (kind == SAVE_ENTRY_EMPTY)
        {
            continue;
        }
        switch (kind)
        {
        case SAVE_PANEL_CHICKENS:
            dst = save_find_free_record(save_image, SAVE_TABLE_CHICKENS, &slot);
            FARM_CHARS.chickens[index].actor.sprite.source = 0;
            save_place_grid_spot(&FARM_CHARS.chickens[index].actor, slot);
            memcpy(dst, &FARM_CHARS.chickens[index], sizeof(FarmChicken));
            memset(&FARM_CHARS.chickens[index], 0, sizeof(FarmChicken));
            break;
        case SAVE_PANEL_CATTLE:
        case SAVE_PANEL_SHEEP:
            dst = save_find_free_record(save_image, SAVE_TABLE_LIVESTOCK, &slot);
            FARM_CHARS.livestock[index].actor.sprite.source = 0;
            save_place_row_spot(&FARM_CHARS.livestock[index].actor, slot);
            memcpy(dst, &FARM_CHARS.livestock[index], sizeof(FarmLivestock));
            memset(&FARM_CHARS.livestock[index], 0, sizeof(FarmLivestock));
            break;
        }
    }
}
#undef SAVE_CHARS
#undef FARM_CHARS

/* Restore every pending entry of the second table into its record. */
static void save_transfer_restore(void)
{
    s32 i;
    s16 slot = 0;
    s16 kind;
    void *dst;

    for (i = 0; i < SAVE_LIST_ENTRIES; i++)
    {
        kind = save_entries[1][i].kind;
        if (kind == -1)
        {
            continue;
        }
        switch (kind)
        {
        case SAVE_PANEL_CHICKENS:
            dst = save_find_free_record(&game_state.animals, SAVE_TABLE_CHICKENS, &slot);
            *(FarmChickenImage *)dst = *(FarmChickenImage *)save_entries[1][i].data;
            save_place_grid_spot(&game_state.animals.chickens[slot].actor, slot);
            break;
        case SAVE_PANEL_CATTLE:
        case SAVE_PANEL_SHEEP:
            dst = save_find_free_record(&game_state.animals, SAVE_TABLE_LIVESTOCK, &slot);
            *(FarmLivestockImage *)dst = *(FarmLivestockImage *)save_entries[1][i].data;
            save_place_row_spot(&game_state.animals.livestock[slot].actor, slot);
            break;
        }
    }
}

/* Return the first free record of the chosen table (0: 10 entries of 180 bytes, 1: 20 of 184)
 * and its index, or NULL when the table is full. */
static void *save_find_free_record(FarmAnimals *tables, s16 kind, s16 *index_out)
{
    void *found = 0;
    s32 i;

    switch (kind)
    {
    case SAVE_TABLE_CHICKENS:
        for (i = 0; i < MAX_CHICKENS; i++)
        {
            if (tables->chickens[i].actor.kind == 0 && !(tables->chickens[i].actor.flags & ACTOR_FLAG_CLAIMED))
            {
                found = &tables->chickens[i];
                *index_out = i;
                break;
            }
        }
        break;
    case SAVE_TABLE_LIVESTOCK:
        for (i = 0; i < MAX_LIVESTOCK; i++)
        {
            if (tables->livestock[i].actor.kind == 0 && !(tables->livestock[i].actor.flags & ACTOR_FLAG_CLAIMED))
            {
                found = &tables->livestock[i];
                *index_out = i;
                break;
            }
        }
        break;
    }
    return found;
}

/* Count the used entries of the requested save-slot table. */
static s32 save_count_records(FarmAnimals *animals, s16 kind)
{
    s16 count;
    s32 i;

    count = 0;
    switch (kind)
    {
    case SAVE_TABLE_CHICKENS:
        for (i = 0; i < 10; i++)
        {
            if (animals->chickens[i].actor.kind != 0 || (animals->chickens[i].actor.flags & ACTOR_FLAG_CLAIMED))
                count++;
        }
        break;
    case SAVE_TABLE_LIVESTOCK:
        for (i = 0; i < 20; i++)
        {
            if (animals->livestock[i].actor.kind != 0)
            {
                count++;
                if (animals->livestock[i].pregnancy != 0)
                    count++;
            }
        }
        break;
    }
    return count;
}

/* Draw the two column headers, then the five listed records of the current
 * game (left) and of the loaded save (right). */
static void save_draw_transfer_lists(void)
{
    s32 side;
    s32 i;
    s16 x;
    s16 y;
    FarmAnimals *records;
    FarmActor *entry;
    s16 selected;

    x = 40;
    y = 34;
    window_frame_draw_at(&current_frame->otTags[0][10], 1, 36, 16, 112, 141);
    text_draw_plain(&current_frame->otTags[0][5], LOCALIZED(text_bank.messages.giving), 60, 20);
    window_frame_draw_at(&current_frame->otTags[0][10], 1, 172, 16, 112, 141);
    text_draw_plain(&current_frame->otTags[0][5], LOCALIZED(text_bank.messages.getting), 196, 20);
    for (side = 0; side < 2; side++)
    {
        if (side != 0)
        {
            records = save_image;
        }
        else
        {
            records = &game_state.animals;
        }
        for (i = 0; i < SAVE_LIST_ENTRIES; i++)
        {
            if (save_entries[side][i].kind == SAVE_ENTRY_EMPTY)
            {
                continue;
            }
            if (save_entries[side][i].kind == 0)
            {
                entry = &records->chickens[save_entries[side][i].index].actor;
                selected = records->chickens[save_entries[side][i].index].daysSick;
            }
            else
            {
                entry = &records->livestock[save_entries[side][i].index].actor;
                selected = records->livestock[save_entries[side][i].index].daysSick;
            }
            save_draw_entry_prompt(entry->kind, selected, entry->name, entry->affection, x, y);
            y += 24;
        }
        x += 136;
        y = 34;
    }
}

/* Draw the save/load name panel: frames, title, row cursor, and the 9-character name of the current farm or the loaded save. */
static s32 save_draw_name_panel(s16 side)
{
    u_long *ot;
    u_long *cursor_ot;
    u_long *text_ot;
    u16 *name;
    s32 triggered;
    s32 i;

    ot = current_frame->otTags[0];
    cursor_ot = &ot[1];
    text_ot = &ot[5];
    window_frame_draw_at(&ot[10], 1, 24, 32, 272, 116);
    window_frame_draw_at(&current_frame->otTags[0][9], 1, 80, 16, 160, 32);
    text_draw_plain(text_ot, LOCALIZED(text_bank.messages.name_panel_title), 208, 26);
    anim_draw_sprt_cursor(cursor_ot, &save_sprites[1], save_cursor_frames[save_panel_row], 0, 184, 18);
    ui_blink_tick(0);
    ui_draw_icon_0d_phase(cursor_ot, 32, 24);
    ui_draw_icon_0c_phase(cursor_ot, 264, 24);
    triggered = input_pad1->triggered;
    if (triggered & PAD_RIGHT)
    {
        sound_play(SOUND_MENU_CURSOR);
        save_panel_row++;
        if (save_panel_row >= 3)
        {
            save_panel_row = SAVE_PANEL_CHICKENS;
        }
        save_first_row = save_cursor_row = 0;
    }
    else if (triggered & PAD_LEFT)
    {
        sound_play(SOUND_MENU_CURSOR);
        save_panel_row--;
        if (save_panel_row < 0)
        {
            save_panel_row = SAVE_PANEL_SHEEP;
        }
        save_first_row = 0;
        save_cursor_row = 0;
    }
    ui_draw_icon_0b_1(cursor_ot, 32, ((save_cursor_row - save_first_row) * SAVE_LIST_ROW_HEIGHT + 60));
    if (save_first_row > 0)
    {
        ui_draw_icon_29_phase(cursor_ot, 288, 48);
    }
    if (side == SAVE_SIDE_FARM)
    {
        name = game_state.progress.estate.farmName;
    }
    else
    {
        name = (u16 *)&((SaveImage *)save_image)->progress.estate.words[5];
    }
    for (i = 0; i < NAME_LENGTH; i++)
    {
        if (name[i] == SAVE_NAME_END)
        {
            /* the name ended early: append the localized suffix */
            name = LOCALIZED(text_bank.messages.farm);
            save_name_line[i] = name[0];
            save_name_line[i + 1] = name[1];
            save_name_line[i + 2] = name[2];
            save_name_line[i + 3] = name[3];
            save_name_line[i + 4] = SAVE_NAME_END;
            break;
        }
        save_name_line[i] = name[i];
    }
    text_draw_plain(text_ot, save_name_line, 82, 26);
    return save_transfer_list_step(side);
}

/* One frame of the item list: move the cursor with up/down, scroll the
 * four-row window, draw the rows and handle confirm/cancel. Returns -1 on
 * cancel, 1 on PAD_START, 0 otherwise. */
static s32 save_transfer_list_step(s16 side)
{
    FarmAnimals *inv;
    u_long *ot;
    u_long *list;
    s32 count;
    s32 triggered;
    s32 pressed;
    s32 i;
    s32 row;
    s32 next;
    s32 result;

    result = 0;
    if (side)
    {
        inv = save_image;
    }
    else
    {
        inv = &game_state.animals;
    }
    ot = current_frame->otTags[0];
    list = &ot[5];
    SHOW_MESSAGE(ot, LOCALIZED(text_bank.messages.select_up_to_5_animals_cant));
    count = save_count_page_entries(inv, -1);

    triggered = input_pad1->triggered;
    if (triggered & PAD_UP)
    {
        sound_play(SOUND_MENU_CURSOR);
        save_cursor_row--;
    }
    else if (triggered & PAD_DOWN)
    {
        sound_play(SOUND_MENU_CURSOR);
        save_cursor_row++;
    }
    if (save_cursor_row < 0)
    {
        save_cursor_row = count - 1;
    }
    if (count - 1 < save_cursor_row)
    {
        save_cursor_row = 0;
    }
    if (count >= 5)
    {
        if (save_first_row + 3 < save_cursor_row)
        {
            save_first_row = save_cursor_row - 3;
        }
        if (save_cursor_row < save_first_row)
        {
            save_first_row = save_cursor_row;
        }
    }
    if (count == 0)
    {
        save_cursor_row = 0;
        save_first_row = 0;
    }
    if (count < save_first_row)
    {
        ui_draw_icon_29_phase(list, 288, 48);
    }
    if (save_first_row + 4 < count)
    {
        ui_draw_icon_28_phase(list, 288, 128);
    }
    ui_draw_icon_0b_1(list, 32, ((save_cursor_row - save_first_row) * SAVE_LIST_ROW_HEIGHT + 60));

    for (i = 0; i < 4; i = row)
    {
        next = i + 1;
        if (count < save_first_row + next)
        {
            break;
        }
        row = i + 1;
        text_draw_number((save_first_row + row), 2, &current_frame->otTags[0][5], 32,
                         (i * SAVE_LIST_ROW_HEIGHT + 54));
        save_draw_summary_line(inv, save_count_page_entries(inv, (save_first_row + row)),
                               (i * SAVE_LIST_ROW_HEIGHT + 48));
        if (save_transfer_contains(inv, save_count_page_entries(inv, (save_first_row + row))) == 1)
        {
            ui_draw_icon_1e(list, 32, (i * SAVE_LIST_ROW_HEIGHT + 54));
        }
    }

    pressed = input_pad1->pressed;
    if (pressed & PAD_CIRCLE)
    {
        sound_play(SOUND_MENU_CANCEL);
        result = -1;
    }
    else if (pressed & PAD_CROSS)
    {
        if (count == 0)
        {
            sound_play(SOUND_MENU_REFUSE);
        }
        else if (save_transfer_contains(inv, save_count_page_entries(inv, (save_cursor_row + 1))) == 1)
        {
            save_transfer_remove(inv, save_count_page_entries(inv, (save_cursor_row + 1)));
            sound_play(SOUND_MENU_CANCEL);
        }
        else if (save_transfer_count >= SAVE_TRANSFER_MAX)
        {
            sound_play(SOUND_MENU_REFUSE);
        }
        else
        {
            save_transfer_add(inv, save_count_page_entries(inv, (save_cursor_row + 1)));
            sound_play(SOUND_MENU_CONFIRM);
        }
    }
    else if (pressed & PAD_START)
    {
        sound_play(SOUND_MENU_CONFIRM);
        result = 1;
    }
    return result;
}

/* Record (current key, value) in the first free entry of the table for p. */
static void save_transfer_add(void *p, s16 value)
{
    s32 group = p != game_state.animals.chickens;
    s16 key = save_panel_row;
    s32 i;

    for (i = 0; i < SAVE_LIST_ENTRIES; i++)
    {
        if (save_entries[group][i].kind == SAVE_ENTRY_EMPTY)
            break;
    }
    if (i < SAVE_LIST_ENTRIES)
    {
        save_entries[group][i].kind = key;
        save_entries[group][i].index = value;
        save_entries[group][i].data = 0;
        save_transfer_count++;
    }
}

/* Clear the current page's list entry for `index`; `p` picks the chickens or the livestock list. */
static void save_transfer_remove(void *p, s16 index)
{
    s16 key = save_panel_row;
    s32 row = p != game_state.animals.chickens;
    s32 i;

    for (i = 0; i < SAVE_LIST_ENTRIES; i++)
    {
        if (save_entries[row][i].kind == key && save_entries[row][i].index == index)
        {
            break;
        }
    }
    if (i < SAVE_LIST_ENTRIES)
    {
        save_entries[row][i].kind = -1;
        save_entries[row][i].index = -1;
        save_entries[row][i].data = 0;
        save_transfer_count--;
    }
}

/* Whether the current page's list already holds `value`; `p` picks the chickens or the livestock list. */
static s32 save_transfer_contains(void *p, s16 value)
{
    s16 key = save_panel_row;
    s32 i = 0;
    u32 different = p != game_state.animals.chickens;
    SaveEntry *base = (SaveEntry *)save_entries;
    s32 offset = different * 5;
    SaveEntry *entries = base + offset;
    do
    {
        if (entries[i].kind == key && entries[i].index == value)
            return 1;
        i++;
    } while (i < SAVE_LIST_ENTRIES);
    return 0;
}

/* Draw one list entry: the animal's sprite (chosen by item kind), its name, and its affection as one icon per 25 points. */
static void save_draw_entry_prompt(s16 kind, s32 selected, u16 *text, s16 affection, s16 x, s16 y)
{
    s32 selector;
    s16 flag;
    selected = selected << 16;
    flag = selected != 0;
    selector = 0;
    if (kind == ITEM_CHICK)
    {
        selector = 0xE;
        flag = 0;
    }
    if (kind == ITEM_CHICKEN)
    {
        selector = 0xF;
        y += 0;
    }
    if (kind == ITEM_CALF)
    {
        selector = 6;
    }
    if (kind == ITEM_YOUNG_COW)
    {
        selector = 7;
    }
    if (kind == ITEM_COW)
    {
        selector = 8;
    }
    if (kind == ITEM_VISITING_COW)
    {
        selector = 9;
    }
    if (kind == ITEM_LAMB)
    {
        selector = 0xA;
    }
    if (kind == ITEM_SHEEP)
    {
        selector = 0xB;
    }
    if (kind == ITEM_VISITING_SHEEP)
    {
        selector = 0xD;
    }
    anim_draw_sprt_cursor(&current_frame->otTags[0][1], &save_sprites[1], selector, flag & 0xFF, x, y);
    text_draw_plain(&current_frame->otTags[0][5], text, x + 24, y);
    flag = y + 12;
    save_draw_digit_row(affection / 25, x + 90, flag);
}

/* Draw the summary line of save entry `index` (two record layouts) at row `y`. */
static void save_draw_summary_line(FarmAnimals *records, s16 index, s16 y)
{
    u_long *ot;
    FarmActor *entry;
    s16 selected;
    s16 column;
    s16 text_x;
    u32 i;
    u16 *end;

    ot = &current_frame->otTags[0][5];
    column = 88;
    if (save_panel_row == SAVE_PANEL_CHICKENS)
    {
        entry = &records->chickens[index].actor;
        selected = records->chickens[index].daysSick;
    }
    else
    {
        entry = &records->livestock[index].actor;
        selected = records->livestock[index].daysSick;
    }
    save_draw_entry_prompt(entry->kind, selected, entry->name, entry->affection, 64, y);
    for (i = 0; i < 15; i++)
    {
        save_date_text[i] = 0xFFD;
    }
    if (entry->flags & 0x40)
    {
        save_date_text[0] = 0x90;
    }
    save_date_text[4] = 0x71;
    text_format_number(entry->date.day + 1, &save_date_text[5], 2);
    save_date_text[8] = 0;
    save_date_text[9] = 0x26;
    save_date_text[10] = 0x24;
    end = text_format_number(save_read_year(&entry->date.year), &save_date_text[11], 2);
    *end = SAVE_NAME_END;
    text_x = column + 88;
    text_draw_plain(ot, save_date_text, text_x, y);
    ui_draw_icon_2b_sub(ot, text_x + 16, y, entry->date.season);
}

static s16 save_read_year(s16 *year)
{
    return year[0];
}

/* Draw `count` save/load labels, stepping the x position 8 pixels left each time. */
static void save_draw_digit_row(s16 count, s16 x, s16 value)
{
    s32 i;

    for (i = 0; i < count; i++)
    {
        ui_draw_icon_2a(&current_frame->otTags[0][1], x, value);
        x -= SAVE_DIGIT_WIDTH;
    }
}

/* Count the entries of the current page (save_panel_row) that qualify, or, when
 * `want` is not -1, return the index of the entry that brings the count to
 * `want`. */
static s16 save_count_page_entries(FarmAnimals *inv, s16 want)
{
    s16 count;
    s16 found;
    s32 i;

    count = 0;
    found = 0;
    switch (save_panel_row)
    {
    case SAVE_PANEL_CHICKENS:
        for (i = 0; i < 10; i++)
        {
            if ((inv->chickens[i].actor.kind == ITEM_CHICK && !(inv->chickens[i].actor.flags & (ACTOR_FLAG_HELD | ACTOR_FLAG_CLAIMED))) || inv->chickens[i].actor.kind == ITEM_CHICKEN)
            {
                if (!(inv->chickens[i].actor.flags & ACTOR_FLAG_CONTEST_ENTRANT))
                {
                    count++;
                }
                if (count == want)
                {
                    found = i;
                    break;
                }
            }
        }
        break;
    case SAVE_PANEL_CATTLE:
        for (i = 0; i < 20; i++)
        {
            if ((u32)(inv->livestock[i].actor.kind - 352) < 3)
            {
                if (inv->livestock[i].pregnancy == 0)
                {
                    count++;
                }
                if (count == want)
                {
                    found = i;
                    break;
                }
            }
        }
        break;
    case SAVE_PANEL_SHEEP:
        for (i = 0; i < 20; i++)
        {
            if ((u32)(inv->livestock[i].actor.kind - 0x164) < 2)
            {
                if (inv->livestock[i].pregnancy == 0)
                {
                    count++;
                }
                if (count == want)
                {
                    found = i;
                    break;
                }
            }
        }
        break;
    }
    if (want == -1)
    {
        return count;
    }
    return found;
}

/* Reset the save-entry work tables and clear the overlay scratch buffer. */

static void save_reset_work_tables(void)
{
    s16 i;
    s16 j;

    save_reset_cursor();
    for (i = 0; i < 2; i++)
    {
        for (j = 0; j < 5; j++)
        {
            save_entries[i][j].kind = -1;
            save_entries[i][j].index = -1;
            save_entries[i][j].data = 0;
        }
    }
    memset(save_out_buffer, 0, 0x398);
}

/* Reset the transfer list to the chickens page, the top row and an empty transfer count. */
void save_reset_cursor(void)
{
    save_panel_row = SAVE_PANEL_CHICKENS;
    save_first_row = 0;
    save_cursor_row = 0;
    save_transfer_count = 0;
    memset(&save_work_table, 0, 0x1568);
}

/* Build the memory-card title of save slot `slot` in the save header:
 * "HARVEST MOON FILE", the slot number, the day and the season/year text. */
static void save_card_title(s16 slot)
{
    char *text = save_header.title;
    s16 date[4];

    memset(text, 0, sizeof(save_header.title));
    save_header.title[0] = 0;
    date[1] = save_slot_records[slot].date.season;
    date[3] = save_slot_records[slot].date.weekday;
    date[2] = save_slot_records[slot].date.day + 1;
    date[0] = save_slot_records[slot].date.year + 1;
    strcat(text, save_title_text);
    save_format_two_digits(slot + 1, save_slot_text, 1);
    strcat(text, save_slot_text);
    strcat(text, save_year_label);
    save_format_two_digits(date[0], save_slot_text, 2);
    strcat(text, save_slot_text);
    strcat(text, save_full_space);
    save_season_name(date[1], save_slot_text);
    strcat(text, save_slot_text);
}

/* Append `value` to dst as two full-width digits; mode 1 writes the ones
 * digit only, otherwise a zero tens digit is shown as a full-width space. */
static void save_format_two_digits(s16 value, char *dst, s16 mode)
{
    dst[0] = 0;
    save_block_text[0] = 0;
    if (mode != 1)
    {
        if ((value / 10) == 0)
        {
            strcat(save_block_text, save_full_space);
        }
        else
        {
            strcat(save_block_text, save_full_zero);
            save_block_text[1] += value / 10;
        }
        strcat(dst, save_block_text);
    }
    save_block_text[0] = 0;
    strcat(save_block_text, save_full_zero);
    save_block_text[1] += value - (value / 10) * 10;
    strcat(dst, save_block_text);
}

/* Store the clamped save-file label in the destination buffer. */
static void save_season_name(s16 idx, char *dst)
{
    dst[0] = 0;
    idx = (idx < 0) ? 0 : ((idx < SAVE_SEASON_COUNT) ? idx : 3);
    strcat(dst, save_season_names[idx]);
}

/* Fill save slot `slot` from the farm state, clear the save buffer and put a
 * copy of the slot record at byte 512. */
static void save_slot_record_fill(s16 slot)
{
    memcpy(&save_slot_records[slot].date, &game_state.progress.clock.date, sizeof(FarmDate));
    memcpy(save_slot_records[slot].farmName, game_state.progress.estate.farmName, sizeof(save_slot_records[slot].farmName));
    memcpy(save_slot_records[slot].playerName, game_state.player.name, sizeof(save_slot_records[slot].playerName));
    memcpy(save_slot_records[slot].id, &game_state, sizeof(save_slot_records[slot].id));
    save_slot_records[slot].tag = SAVE_SLOT_TAG;
    memset(save_file_header, 0, SAVE_FILE_BYTES);
    memcpy(save_file_header + SAVE_HEADER_OFFSET, &save_slot_records[slot], sizeof(SaveSlotRecord));
}

#define SAVE_FILE ((SaveFile *)save_image)
#define PACK_800742EC (*(Pack800742EC *)&game_state.animals.chickens)
#define PACK_8006C838 (*(Pack8006C838 *)&game_state)
#define PACK_80070CC0 (*(Pack80070CC0 *)&game_state.progress.entryStep)
#define PACK_80071A00 (*(Pack80071A00 *)&game_state.player)
#define PACK_800742AC (*(Pack800742AC *)&game_state.requests.state)
#define PACK_8007A5C4 (*(Pack8007A5C4 *)game_state.runs.records)
#define PACK_8007A638 (*(Pack8007A638 *)&game_state.helpers)
#define PACK_8007AF80 (*(Pack8007AF80 *)&game_state.drift)
#define PACK_80071DFC (*(Pack80071DFC *)game_state.fieldTiles)
#define PACK_80074124 (*(Pack80074124 *)game_state.slots7x7)
#define PACK_8007AD00 (*(Pack8007AD00 *)&game_state.world.weather)
#define PACK_8007AE78 (*(Pack8007AE78 *)game_state.days.count0)
#define PACK_8007AFA4 (*(Pack8007AFA4 *)&game_state.family.addressMode)
#define PACK_8007AFD8 (*(Pack8007AFD8 *)&game_state.ending.route)
/* Pack the farm state (game_state and the regions after it) into the save-file
 * image at save_image_data. */
static void save_pack_farm_state(void)
{
    s32 i;

    save_image = save_image_data;
    SAVE_FILE->slots = PACK_800742EC.slots;
    SAVE_FILE->flagWords = PACK_8006C838.flagWords;
    SAVE_FILE->itemInfo = PACK_8006C838.itemInfo;
    SAVE_FILE->pictureGrids = PACK_8006C838.pictureGrids;
    SAVE_FILE->tileRules = PACK_8006C838.tileRules;
    SAVE_FILE->cropTable = PACK_8006C838.cropTable;
    SAVE_FILE->request = PACK_8006C838.request;
    SAVE_FILE->pictureCodes = PACK_8006C838.pictureCodes;
    SAVE_FILE->eventFlags = PACK_8006C838.eventFlags;
    SAVE_FILE->entryStep = PACK_80070CC0.entryStep;
    SAVE_FILE->entrySubStep = PACK_80070CC0.entrySubStep;
    SAVE_FILE->entryArgs = *(Block14 *)PACK_80070CC0.entryArgs;
    SAVE_FILE->entryArgs2 = *(Block14 *)PACK_80070CC0.entryArgs2;
    SAVE_FILE->clock[0] = PACK_80070CC0.clock[0];
    SAVE_FILE->clock[1] = PACK_80070CC0.clock[1];
    SAVE_FILE->clock[2] = PACK_80070CC0.clock[2];
    SAVE_FILE->clock[3] = PACK_80070CC0.clock[3];
    SAVE_FILE->clock[4] = PACK_80070CC0.clock[4];
    SAVE_FILE->estate = PACK_80070CC0.estate;
    SAVE_FILE->player = PACK_80071A00.player;
    SAVE_FILE->fieldTiles = PACK_80071DFC.fieldTiles;
    SAVE_FILE->slots7x7 = PACK_80074124.slots7x7;
    SAVE_FILE->requests = PACK_800742AC.requests;
    SAVE_FILE->runs = PACK_8007A5C4.runs;
    SAVE_FILE->helpers = PACK_8007A638.helpers;
    SAVE_FILE->weatherPair = PACK_8007AD00.weatherPair;
    SAVE_FILE->options = *(BlockC *)PACK_8007AD00.options;
    SAVE_FILE->rivalWeddings = PACK_8007AD00.rivalWeddings;
    SAVE_FILE->days = PACK_8007AE78.days;
    SAVE_FILE->drift = PACK_8007AF80.drift;
    SAVE_FILE->family = PACK_8007AFA4.family;
    SAVE_FILE->endingHead = PACK_8007AFD8.endingHead;
    SAVE_FILE->endingTracks = *(BlockC *)PACK_8007AFD8.endingTracks;
    for (i = 0; i < CHARACTER_COUNT; i++)
    {
        SAVE_FILE->records[i].birthday = game_state.villagers[i].birthday;
        SAVE_FILE->records[i].friendship = game_state.villagers[i].friendship;
        SAVE_FILE->records[i].affection = game_state.villagers[i].affection;
        SAVE_FILE->records[i].flags = game_state.villagers[i].flags;
        SAVE_FILE->records[i].placeFlags = game_state.villagers[i].placeFlags;
        SAVE_FILE->records[i].step = game_state.villagers[i].step;
    }
    SAVE_FILE->run16 = *PACK_8006C838.run16;
    SAVE_FILE->run18 = *PACK_8006C838.run18;
    for (i = 0; i < 10; i++)
    {
        SAVE_FILE->slots.chickens[i].actor.sprite.source = 0;
    }
    for (i = 0; i < 20; i++)
    {
        SAVE_FILE->slots.livestock[i].actor.sprite.source = 0;
    }
    memset(SAVE_FILE->slots.extraChickens, 0, sizeof(SAVE_FILE->slots.extraChickens));
}
#undef SAVE_FILE
#undef PACK_800742EC
#undef PACK_8006C838
#undef PACK_80070CC0
#undef PACK_80071A00
#undef PACK_800742AC
#undef PACK_8007A5C4
#undef PACK_8007A638
#undef PACK_8007AF80
#undef PACK_80071DFC
#undef PACK_80074124
#undef PACK_8007AD00
#undef PACK_8007AE78
#undef PACK_8007AFA4
#undef PACK_8007AFD8

#define SAVE_IMAGE ((SaveImage *)save_image)
#define SAVED_PROGRESS (*(SavedProgress *)&game_state.progress.entryStep)
/* Copy the loaded save image into the live game state and the saved-progress block. */
static void save_unpack_farm_state(void)
{
    s32 i;

    save_image = save_image_data;
    *(BlockF0 *)game_state.runs.records[16] = SAVE_IMAGE->run16;
    *(Block30 *)game_state.runs.records[18] = SAVE_IMAGE->run18;
    for (i = 0; i < CHARACTER_COUNT; i++)
    {
        game_state.villagers[i].birthday = SAVE_IMAGE->records[i].birthday;
        game_state.villagers[i].friendship = SAVE_IMAGE->records[i].friendship;
        game_state.villagers[i].affection = SAVE_IMAGE->records[i].affection;
        game_state.villagers[i].flags = SAVE_IMAGE->records[i].flags;
        game_state.villagers[i].placeFlags = SAVE_IMAGE->records[i].placeFlags;
        game_state.villagers[i].step = SAVE_IMAGE->records[i].step;
    }
    *(Block1F34 *)game_state.animals.chickens = SAVE_IMAGE->animals;
    game_state.unk00 = SAVE_IMAGE->flagWords[0];
    game_state.unk04 = SAVE_IMAGE->flagWords[1];
    game_state.frameCount = SAVE_IMAGE->flagWords[2];
    game_state.globalFlags = SAVE_IMAGE->flagWords[3];
    game_state.debugFlags = SAVE_IMAGE->flagWords[4];
    game_state.lastCaller = SAVE_IMAGE->flagWords[5];
    game_state.request = SAVE_IMAGE->request;
    game_state.pictureCodes = SAVE_IMAGE->pictureCodes;
    *(Block94 *)game_state.eventFlags = SAVE_IMAGE->eventFlags;
    SAVED_PROGRESS.entryStep = SAVE_IMAGE->progress.entryStep;
    SAVED_PROGRESS.entrySubStep = SAVE_IMAGE->progress.entrySubStep;
    *(Block14 *)&SAVED_PROGRESS.entryArgs = SAVE_IMAGE->progress.entryArgs;
    *(Block14 *)&SAVED_PROGRESS.entryArgs2 = SAVE_IMAGE->progress.entryArgs2;
    SAVED_PROGRESS.clock[0] = SAVE_IMAGE->progress.clock[0];
    SAVED_PROGRESS.clock[1] = SAVE_IMAGE->progress.clock[1];
    SAVED_PROGRESS.clock[2] = SAVE_IMAGE->progress.clock[2];
    SAVED_PROGRESS.clock[3] = SAVE_IMAGE->progress.clock[3];
    SAVED_PROGRESS.clock[4] = SAVE_IMAGE->progress.clock[4];
    *(BlockCFC *)&SAVED_PROGRESS.estate = SAVE_IMAGE->progress.estate;
    *(Block3FC *)&game_state.player = SAVE_IMAGE->player;
    *(Block2328 *)game_state.fieldTiles = SAVE_IMAGE->fieldTiles;
    *(Block188 *)game_state.slots7x7 = SAVE_IMAGE->slots7x7;
    *(Block40 *)&game_state.requests.state = SAVE_IMAGE->requests;
    *(Block6C8 *)game_state.helpers = SAVE_IMAGE->helpers;
    *(s32 *)&game_state.world.weather = SAVE_IMAGE->world.weatherPair;
    *(BlockC *)&game_state.world.options = SAVE_IMAGE->world.options;
    *(Block3C *)game_state.world.rivalWeddings = SAVE_IMAGE->world.rivalWeddings;
    *(Block108 *)game_state.days.count0 = SAVE_IMAGE->days;
    *(Block24 *)&game_state.drift = SAVE_IMAGE->drift;
    *(Block34 *)&game_state.family.addressMode = SAVE_IMAGE->family;
    *(BlockC *)&game_state.ending.route = SAVE_IMAGE->ending.endingHead;
    *(BlockC *)&game_state.ending.tracks = SAVE_IMAGE->ending.endingTracks;
}
#undef SAVE_IMAGE
#undef SAVED_PROGRESS

/* Shift the save image's 0x80-byte blocks by one byte before a save; save_image_shift_out undoes it. */
static void save_image_shift_in(void)
{
    u8 *p;
    s32 value;
    s32 count;
    s32 result;

    save_image = save_image_data;
    p = save_image_data;
    value = 0x6441;
    count = 200;
    do
    {
        memmove(p + 128, p + 127, value);
        p += 0x80;
        value -= 0x80;
    } while (--count >= 0);
    result = rand_range(256);
    save_file_header[0x7FFE] = result;
}
/* Undo save_image_shift_in before a load unpacks the image. */
static void save_image_shift_out(void)
{
    u8 *p;
    s32 value;
    s32 count;
    save_image = save_image_data;
    p = save_image_data;
    value = 0x6441;
    count = 200;
    do
    {
        memmove(p + 127, p + 128, value);
        p += 0x7F;
        value -= 0x7F;
    } while (--count >= 0);
}

/* Recompute the save image's block checksums. */
static void save_image_checksum_write(void)
{
    u8 total;
    u8 sum;
    s32 i;
    s32 j;
    u8 *p;

    total = 0;
    for (i = 5; i < 256; i++)
    {
        sum = 0;
        for (j = 0; j < 127; j++)
        {
            sum ^= save_file_header[i * 0x80 + j];
        }
        save_file_header[i * 0x80 + j] = sum;
        total ^= sum;
    }
    save_slot_record[0x38] = total;
    sum = 0;
    p = save_slot_record;
    for (i = 0; i < 127; i++)
    {
        sum ^= *p++;
    }
    save_file_header[0x27F] = sum;
}

/* Verify the save image's block checksums. */
static s32 save_image_checksum_verify(void)
{
    u8 total;
    u8 sum;
    s32 i;
    s32 j;
    u8 *p;

    total = 0;
    for (i = 5; i < 256; i++)
    {
        sum = 0;
        for (j = 0; j < 127; j++)
        {
            sum ^= save_file_header[i * 0x80 + j];
        }
        total ^= sum;
        if (save_file_header[i * 0x80 + j] != sum)
        {
            return -1;
        }
    }
    if (save_slot_record[0x38] != total)
    {
        return -1;
    }
    sum = 0;
    p = save_slot_record;
    for (i = 0; i < 127; i++)
    {
        sum ^= *p++;
    }
    if (save_file_header[0x27F] != sum)
    {
        return -1;
    }
    return 0;
}

/* Probe the memory card in `slot` (0 or 1), retrying up to eight times, and return its state as a save/load result code. */
static s32 save_card_probe(s32 slot)
{
    long cmd;
    long result;
    s32 chan = 0;
    s32 type;

    slot = (s16)slot;
    if (slot == 0)
    {
        chan = 0;
    }
    else if (slot == 1)
    {
        chan = 0x10;
    }
    else
    {
        return SAVE_CARD_ERROR;
    }
    save_retry_count = 0;
    MemCardSync(0, &cmd, &result);
    while (1)
    {
        if (MemCardAccept(chan) != 1)
        {
            return SAVE_CARD_ERROR;
        }
        MemCardSync(0, &cmd, &result);
        switch (result)
        {
        case McErrCardNotExist:
            if (save_retry_count >= SAVE_PROBE_RETRIES)
            {
                return SAVE_CARD_NO_CARD;
            }
            save_retry_count++;
            break;
        case McErrCardInvalid:
            if (save_retry_count >= SAVE_PROBE_RETRIES)
            {
                return SAVE_CARD_DAMAGED;
            }
            save_retry_count++;
            break;
        case McErrNotFormat:
            if (save_retry_count >= SAVE_PROBE_RETRIES)
            {
                save_retry_count = 0;
                slot <<= 4;
            unformatted:
                type = McxCardType(slot);
                if (type != 1)
                {
                    return SAVE_CARD_ERROR;
                }
                McxSync(0, &cmd, &result);
                switch (result)
                {
                case McxErrSuccess:
                    return SAVE_CARD_UNFORMATTED_MCX;
                case McxErrNoCard:
                    return SAVE_CARD_NO_CARD;
                case McxErrInvalid:
                    if (save_retry_count >= SAVE_PROBE_RETRIES)
                    {
                        return SAVE_CARD_UNFORMATTED;
                    }
                    save_retry_count++;
                    goto unformatted;
                case McxErrNewCard:
                    return SAVE_CARD_NEW;
                default:
                    return SAVE_CARD_ERROR;
                }
            }
            save_retry_count++;
            break;
        case McErrNewCard:
            return SAVE_CARD_NEW;
        case McErrNone:
            save_retry_count = 0;
            slot <<= 4;
        formatted:
            type = McxCardType(slot);
            if (type != 1)
            {
                return SAVE_CARD_ERROR;
            }
            McxSync(0, &cmd, &result);
            switch (result)
            {
            case McxErrSuccess:
                return SAVE_CARD_READY;
            case McxErrNoCard:
                return SAVE_CARD_NO_CARD;
            case McxErrNewCard:
                return SAVE_CARD_NEW;
            case McxErrInvalid:
                if (save_retry_count >= SAVE_PROBE_RETRIES)
                {
                    return SAVE_CARD_OK;
                }
                save_retry_count++;
                goto formatted;
            default:
                return SAVE_CARD_ERROR;
            }
        default:
            return SAVE_CARD_ERROR;
        }
    }
}

/* Poll the memory card and translate its state into a save/load result code. */
static s32 save_card_poll(s16 port)
{
    long cmd;
    long result;
    s32 chan;
    s32 status;
    s32 state;

    chan = 0;
    switch (port)
    {
    case SAVE_PORT_1:
        break;
    case SAVE_PORT_2:
        chan = 0x10;
        break;
    default:
        return SAVE_CARD_ERROR;
    }
    save_retry_count++;
    status = MemCardSync(1, &cmd, &result);
    if (status == SAVE_SYNC_BUSY)
    {
        return SAVE_CARD_BUSY;
    }
    if (status == SAVE_SYNC_IDLE)
    {
        save_retry_count = 0;
    }
    else
    {
        if (cmd != 1)
        {
            return SAVE_CARD_BUSY;
        }
        switch (result)
        {
        case McErrNone:
            return SAVE_CARD_OK;
        case McErrCardNotExist:
        case McErrCardInvalid:
            break;
        case McErrNewCard:
            return SAVE_CARD_NEW;
        }
        if (save_retry_count >= SAVE_POLL_RETRIES)
        {
            switch (result)
            {
            case McErrCardNotExist:
                return SAVE_CARD_NO_CARD;
            case McErrCardInvalid:
                return SAVE_CARD_DAMAGED;
            }
        }
        MemCardSync(0, &cmd, &result);
    }
    state = MemCardExist(chan);
    if (state != 1)
    {
        return SAVE_CARD_ERROR;
    }
    return SAVE_CARD_BUSY;
}

static const char save_every_file_pattern[4] = {0x2A};

/* Check that the memory card in `port` (0 or 1) can take save file `slot`:
 * read the save's own directory entry, then all of them, and count the 8 KB blocks
 * in use. Returns SAVE_CARD_OK if the save already exists or there is room,
 * SAVE_CARD_FULL if the card is full, SAVE_CARD_NOT_ENOUGH if fewer than `needed`
 * blocks (SAVE_FARM_BLOCKS, SAVE_PDA_BLOCKS for slot SAVE_PDA_SLOT) are free,
 * SAVE_CARD_NO_CARD / SAVE_CARD_DAMAGED when the card is missing / unreadable after
 * SAVE_PROBE_RETRIES tries, SAVE_CARD_ERROR for a bad port. */
static s16 save_card_check_space(s16 port, s16 slot)
{
    struct DIRENTRY dir[SAVE_CARD_FILES];
    long cmd;
    long result;
    long save_files;
    long all_files;
    s32 needed;
    s32 chan;
    s32 used;
    s32 blocks;
    s32 i;
    s32 short_of_space;

    needed = SAVE_FARM_BLOCKS;
    save_files = 0;
    all_files = 0;
    if (slot == SAVE_PDA_SLOT)
    {
        needed = SAVE_PDA_BLOCKS;
    }
    chan = 0;
    if (port != SAVE_PORT_1)
    {
        if (port != SAVE_PORT_2)
        {
            return SAVE_CARD_ERROR;
        }
        chan = 0x10;
    }
    save_file_name(save_file_name_buf, slot);
    save_retry_count = 0;

    memset(dir, 0, sizeof(dir));
    MemCardSync(0, &cmd, &result);
retry_save:
    result = MemCardGetDirentry(chan, save_file_name_buf, dir, &save_files, 0, 1);
    switch (result)
    {
    case McErrCardNotExist:
        if (save_retry_count < SAVE_CARD_RETRIES)
        {
            save_retry_count++;
            goto retry_save;
        }
        return SAVE_CARD_NO_CARD;
    case McErrCardInvalid:
        if (save_retry_count < SAVE_CARD_RETRIES)
        {
            save_retry_count++;
            goto retry_save;
        }
        return SAVE_CARD_DAMAGED;
    case McErrNone:
    default:
        save_retry_count = 0;
        break;
    }

    memset(dir, 0, sizeof(dir));
    MemCardSync(0, &cmd, &result);
    while (1)
    {
        result = MemCardGetDirentry(chan, (char *)save_every_file_pattern, dir, &all_files, 0, SAVE_CARD_FILES);
        switch (result)
        {
        case McErrCardNotExist:
            if (save_retry_count < SAVE_CARD_RETRIES)
            {
                save_retry_count++;
                continue;
            }
            return SAVE_CARD_NO_CARD;
        case McErrCardInvalid:
            if (save_retry_count < SAVE_CARD_RETRIES)
            {
                save_retry_count++;
                continue;
            }
            return SAVE_CARD_DAMAGED;
        case McErrNone:
        default:
            break;
        }
        break;
    }

    used = 0;
    for (i = 0; i < SAVE_CARD_FILES; i++)
    {
        blocks = dir[i].size / 0x2000;
        if (dir[i].size & 0x1FFF)
        {
            used += blocks + 1;
        }
        else
        {
            used += blocks;
        }
    }
    if (save_files != 0)
    {
        return SAVE_CARD_OK;
    }
    if (used == SAVE_CARD_FILES)
    {
        return SAVE_CARD_FULL;
    }
    short_of_space = SAVE_CARD_FILES - used < needed;
    if (short_of_space)
    {
        return SAVE_CARD_NOT_ENOUGH;
    }
    return SAVE_CARD_OK;
}

/* Read the 0x80-byte header of save file `slot` from the memory card in port
 * `port` (0 or 1), copy the slot record at its start into `record`, and check
 * the XOR checksum held in its last byte. Returns SAVE_CARD_OK when it is valid,
 * SAVE_CARD_BAD_CHECKSUM when it is not, and SAVE_CARD_NO_CARD, SAVE_CARD_NEW,
 * SAVE_CARD_DAMAGED or SAVE_CARD_NO_FILE, or SAVE_CARD_ERROR, when the card cannot
 * be read. */
static s16 save_card_read_header(s16 port, s16 slot, SaveSlotRecord *record)
{
    u8 buffer[0x80];
    long cmd;
    long result;
    long chan;
    u8 checksum;
    s32 i;

    chan = 0;
    if (port != SAVE_PORT_1)
    {
        if (port != SAVE_PORT_2)
        {
            return SAVE_CARD_ERROR;
        }
        chan = 0x10;
    }
    save_file_name(save_file_name_buf, slot);
    save_retry_count = 0;
    MemCardSync(0, &cmd, &result);
    while (1)
    {
        MemCardReadFile(chan, save_file_name_buf, (unsigned long *)buffer, SAVE_HEADER_OFFSET, SAVE_HEADER_SIZE);
        MemCardSync(0, &cmd, &result);
        switch (result)
        {
        case McErrCardNotExist:
            if (save_retry_count < SAVE_CARD_RETRIES)
            {
                save_retry_count++;
                continue;
            }
            return SAVE_CARD_NO_CARD;
        case McErrCardInvalid:
            if (save_retry_count < SAVE_CARD_RETRIES)
            {
                save_retry_count++;
                continue;
            }
            return SAVE_CARD_DAMAGED;
        case McErrFileNotExist:
            if (save_retry_count < SAVE_CARD_RETRIES)
            {
                save_retry_count++;
                continue;
            }
            return SAVE_CARD_NO_FILE;
        case McErrNewCard:
            return SAVE_CARD_NEW;
        case McErrNone:
        case McErrNotFormat:
            break;
        }
        break;
    }
    *record = *(SaveSlotRecord *)buffer;
    checksum = 0;
    for (i = 0; i < 127; i++)
    {
        checksum ^= buffer[i];
    }
    if (checksum != buffer[0x7F])
    {
        return SAVE_CARD_BAD_CHECKSUM;
    }
    return SAVE_CARD_OK;
}

/* Format the memory card on port `port` (0 or 1), retrying up to seven times. */
static s32 save_card_format(s16 port)
{
    long cmd;
    long result;
    long chan;

    chan = 0;
    if (port != SAVE_PORT_1)
    {
        if (port != SAVE_PORT_2)
        {
            return SAVE_CARD_ERROR;
        }
        chan = 0x10;
    }
    save_retry_count = 0;
    MemCardSync(0, &cmd, &result);
    while (1)
    {
        result = MemCardFormat(chan);
        switch (result)
        {
        case McErrCardNotExist:
            if (save_retry_count < SAVE_CARD_RETRIES)
            {
                save_retry_count++;
                continue;
            }
            return SAVE_CARD_NO_CARD;
        case McErrCardInvalid:
            if (save_retry_count < SAVE_CARD_RETRIES)
            {
                save_retry_count++;
                continue;
            }
            return SAVE_CARD_FAILED;
        case McErrNone:
            return SAVE_CARD_OK;
        default:
            return SAVE_CARD_FAILED;
        }
    }
}

/* Create save file `file_index` on memory card port `port` (0 or 1), retrying up to 7
 * times; returns 0 on success, or a save/load result code. */
static s32 save_card_create_file(s16 port, s16 file_index)
{
    long cmd;
    long result;
    long chan;

    chan = 0;
    if (port != SAVE_PORT_1)
    {
        if (port != SAVE_PORT_2)
        {
            return SAVE_CARD_ERROR;
        }
        chan = 0x10;
    }
    save_file_name(save_file_name_buf, file_index);
    save_retry_count = 0;
    MemCardSync(0, &cmd, &result);
    while (1)
    {
        result = MemCardCreateFile(chan, save_file_name_buf, SAVE_FILE_BLOCKS);
        switch (result)
        {
        case McErrCardNotExist:
        case McErrCardInvalid:
        case McErrNotFormat:
        case McErrAlreadyExist:
        case McErrBlockFull:
            if (save_retry_count < SAVE_CARD_RETRIES)
            {
                save_retry_count++;
                continue;
            }
            switch (result)
            {
            case McErrBlockFull:
                return SAVE_CARD_NOT_ENOUGH;
            case McErrCardNotExist:
                return SAVE_CARD_NO_CARD;
            }
            return SAVE_CARD_FAILED;
        case McErrNone:
            return SAVE_CARD_OK;
        default:
            return SAVE_CARD_FAILED;
        }
    }
}

/* Write `data` (0x8000 bytes) to save file `slot` on the memory card in port
 * `port` (0 or 1). save_file_name copies the slot's file name into
 * save_file_name_buf. Returns SAVE_CARD_BUSY once the write has been issued; SAVE_CARD_OK,
 * SAVE_CARD_FAILED, SAVE_CARD_NO_CARD or SAVE_CARD_NEW for the card results that end the
 * attempt, SAVE_CARD_ERROR for anything unexpected. */
static s32 save_card_write_file(s16 port, s16 slot, u8 *data)
{
    long cmd;
    long result;
    long status;
    long chan;

    chan = 0;
    if (port != SAVE_PORT_1)
    {
        if (port != SAVE_PORT_2)
        {
            return SAVE_CARD_ERROR;
        }
        chan = 0x10;
    }
    save_file_name(save_file_name_buf, slot);
    save_retry_count = 0;
    status = MemCardSync(1, &cmd, &result);
    while (1)
    {
        if (cmd != 4)
        {
            switch (status)
            {
            case SAVE_SYNC_BUSY:
                return SAVE_CARD_ERROR;
            case SAVE_SYNC_DONE:
            case SAVE_SYNC_IDLE:
                MemCardWriteFile(chan, save_file_name_buf, (unsigned long *)data, 0, 0x8000);
                save_retry_count = 0;
                return SAVE_CARD_BUSY;
            }
            break;
        }
        switch (status)
        {
        case SAVE_SYNC_BUSY:
            return SAVE_CARD_BUSY;
        case SAVE_SYNC_IDLE:
            MemCardWriteFile(chan, save_file_name_buf, (unsigned long *)data, 0, 0x8000);
            save_retry_count = 0;
            return SAVE_CARD_BUSY;
        case SAVE_SYNC_DONE:
            switch (result)
            {
            case McErrCardInvalid:
            case McErrFileNotExist:
                if (save_retry_count < SAVE_CARD_RETRIES)
                {
                    save_retry_count++;
                    continue;
                }
                return SAVE_CARD_FAILED;
            case McErrCardNotExist:
                if (save_retry_count < SAVE_CARD_RETRIES)
                {
                    save_retry_count++;
                    continue;
                }
                return SAVE_CARD_NO_CARD;
            case McErrNewCard:
                return SAVE_CARD_NEW;
            case McErrNone:
                return SAVE_CARD_OK;
            }
            break;
        }
        break;
    }
    return SAVE_CARD_ERROR;
}

/* Read save file `slot` (0x8000 bytes) from the memory card in port `port`
 * (0 or 1) into `buf`. save_file_name copies the slot's file name into
 * save_file_name_buf. Returns SAVE_CARD_BUSY once the read has been issued; SAVE_CARD_OK,
 * SAVE_CARD_FAILED, SAVE_CARD_NO_CARD or SAVE_CARD_NEW for the card results that end the
 * attempt, SAVE_CARD_ERROR for anything unexpected. */
static s32 save_card_read_file(s16 port, s16 slot, u8 *buf)
{
    long cmd;
    long result;
    long status;
    long chan;

    chan = 0;
    if (port != SAVE_PORT_1)
    {
        if (port != SAVE_PORT_2)
        {
            return SAVE_CARD_ERROR;
        }
        chan = 0x10;
    }
    save_file_name(save_file_name_buf, slot);
    save_retry_count = 0;
    status = MemCardSync(1, &cmd, &result);
    while (1)
    {
        if (cmd != 3)
        {
            switch (status)
            {
            case SAVE_SYNC_BUSY:
                return SAVE_CARD_ERROR;
            case SAVE_SYNC_DONE:
            case SAVE_SYNC_IDLE:
                MemCardReadFile(chan, save_file_name_buf, (unsigned long *)buf, 0, 0x8000);
                save_retry_count = 0;
                return SAVE_CARD_BUSY;
            }
            break;
        }
        switch (status)
        {
        case SAVE_SYNC_BUSY:
            return SAVE_CARD_BUSY;
        case SAVE_SYNC_IDLE:
            MemCardReadFile(chan, save_file_name_buf, (unsigned long *)buf, 0, 0x8000);
            save_retry_count = 0;
            return SAVE_CARD_BUSY;
        case SAVE_SYNC_DONE:
            switch (result)
            {
            case McErrCardNotExist:
                if (save_retry_count < SAVE_CARD_RETRIES)
                {
                    save_retry_count++;
                    continue;
                }
                return SAVE_CARD_NO_CARD;
            case McErrCardInvalid:
            case McErrFileNotExist:
                if (save_retry_count < SAVE_CARD_RETRIES)
                {
                    save_retry_count++;
                    continue;
                }
                return SAVE_CARD_FAILED;
            case McErrNewCard:
                return SAVE_CARD_NEW;
            case McErrNone:
                return SAVE_CARD_OK;
            }
            break;
        }
        break;
    }
    return SAVE_CARD_ERROR;
}

/* Update the save/load cursor, draw its panels, and report the selected action. */
s32 save_menu_choose_two(void)
{
    s32 pad;
    window_frame_draw_at(&current_frame->otTags[0][6], 1, 78, 72, 163, 34);
    text_draw_plain(&current_frame->otTags[0][5], LOCALIZED(text_bank.messages.memory_card_slots), 95, 76);
    pad = input_pad1->triggered;
    if (pad & PAD_DOWN)
    {
        sound_play(SOUND_MENU_CURSOR);
        save_confirm_cursor += 1;
    }
    else if (pad & PAD_UP)
    {
        sound_play(SOUND_MENU_CURSOR);
        save_confirm_cursor -= 1;
    }
    if (save_confirm_cursor < 0)
    {
        save_confirm_cursor = 1;
    }
    if (save_confirm_cursor >= 2)
    {
        save_confirm_cursor = 0;
    }
    ui_draw_icon_0b_1(&current_frame->otTags[0][1], 92, (save_confirm_cursor * 13 + 82));
    pad = input_pad1->pressed;
    if (pad & PAD_CROSS)
    {
        sound_play(SOUND_MENU_CONFIRM);
        return save_confirm_cursor;
    }
    if (!(pad & PAD_CIRCLE))
    {
        return 2;
    }
    sound_play(SOUND_MENU_CANCEL);
    return -1;
}

/* Draw the three slot rows, move the cursor with up/down (wrapping over 0..2) and
 * return the chosen row on confirm, -1 on cancel, or 3 while still choosing. */
static s32 save_menu_choose_three(s32 mode)
{
    s16 screen = mode;
    s32 slot;
    s32 buttons;

    for (slot = 0; slot < 3; slot++)
    {
        save_draw_slot_row(slot, screen);
    }
    buttons = input_pad1->triggered;
    if (buttons & PAD_DOWN)
    {
        sound_play(SOUND_MENU_CURSOR);
        save_confirm_cursor++;
    }
    else if (buttons & PAD_UP)
    {
        sound_play(SOUND_MENU_CURSOR);
        save_confirm_cursor--;
    }
    if (save_confirm_cursor < 0)
    {
        save_confirm_cursor = 2;
    }
    if (save_confirm_cursor >= 3)
    {
        save_confirm_cursor = 0;
    }
    ui_draw_icon_0b_1(&current_frame->otTags[0][1], 57, save_confirm_cursor * SAVE_SLOT_ROW_STEP + 24);
    buttons = input_pad1->pressed;
    if (buttons & PAD_CROSS)
    {
        sound_play(SOUND_MENU_CONFIRM);
        return save_confirm_cursor;
    }
    if (buttons & PAD_CIRCLE)
    {
        sound_play(SOUND_MENU_CANCEL);
        return -1;
    }
    return 3;
}

/* Draw save-slot row `slot`: the frame, the slot label, then either the slot's status message (chosen by `mode` for statuses 2 and 3) or, for a valid slot, its year, season, day and the two names. */
static void save_draw_slot_row(s16 slot, s16 mode)
{
    s16 y;
    s32 i;
    u16 *end;

    y = slot * SAVE_SLOT_ROW_STEP + 20;
    /* Message window for the slot's status text. */
    window_frame_draw_at(&current_frame->otTags[0][6], 1, 48, slot * SAVE_SLOT_ROW_STEP + 16, 228, 48);
    text_set_source(1, save_slot_digits[slot]);
    text_draw_plain(&current_frame->otTags[0][5], LOCALIZED(text_bank.messages.diary_number), 65, y);
    switch (save_slot_status[slot])
    {
    case SAVE_SLOT_NO_DIARY:
        text_draw_plain(&current_frame->otTags[0][5], LOCALIZED(text_bank.messages.no_diary), 65, y + 21);
        break;
    case SAVE_SLOT_SHORT_OF_SPACE:
        if (mode == 0)
        {
            text_draw_plain(&current_frame->otTags[0][5], LOCALIZED(text_bank.messages.no_diary), 65, y + 21);
        }
        else if (mode == 1)
        {
            text_draw_plain(&current_frame->otTags[0][5], LOCALIZED(text_bank.messages.not_enough_free_space), 65, y + 21);
        }
        break;
    case SAVE_SLOT_NO_FREE_SPACE:
        if (mode == 0)
        {
            text_draw_plain(&current_frame->otTags[0][5], LOCALIZED(text_bank.messages.no_diary), 65, y + 21);
        }
        else if (mode == 1)
        {
            text_draw_plain(&current_frame->otTags[0][5], LOCALIZED(text_bank.messages.no_free_space), 65, y + 21);
        }
        break;
    case SAVE_SLOT_DIARY:
        for (i = 0; i < NAME_CHARS; i++)
        {
            if (save_slot_records[slot].farmName[i] > 0xE00 && save_slot_records[slot].farmName[i] != 0xFFD)
            {
                save_slot_records[slot].farmName[i] = SAVE_NAME_END;
            }
        }
        for (i = 0; i < NAME_CHARS; i++)
        {
            if (save_slot_records[slot].playerName[i] > 0xE00 && save_slot_records[slot].playerName[i] != 0xFFD)
            {
                save_slot_records[slot].playerName[i] = SAVE_NAME_END;
            }
        }
        y += 14;
        memset(save_name_line, 0xFF, 0x28);
        save_format_number_suffix(save_slot_records[slot].date.year + 1, save_name_line, 2);
        text_set_source(2, save_name_line);
        end = text_write_season(save_slot_records[slot].date.season, &save_name_line[5]);
        *(s16 *)end = -1;
        text_set_source(3, &save_name_line[5]);
        save_format_number_suffix(save_slot_records[slot].date.day + 1, &save_name_line[0xD], 2);
        text_set_source(4, &save_name_line[0xD]);
        save_slot_records[slot].farmName[NAME_CHARS] = SAVE_NAME_END;
        text_set_source(5, save_slot_records[slot].farmName);
        text_draw_plain(&current_frame->otTags[0][5], LOCALIZED(text_bank.messages.diary_date), 65, y);
        y += 14;
        text_set_source(6, save_slot_records[slot].playerName);
        save_slot_records[slot].playerName[NAME_CHARS] = SAVE_NAME_END;
        text_draw_plain(&current_frame->otTags[0][5], LOCALIZED(text_bank.messages.diary_farm_name), 65, y);
        break;
    }
}

/* Format `value` as a `width`-digit number into `out`, then append the
 * localized suffix string chosen by the value (1, 2, 3, anything else). */
static void save_format_number_suffix(s16 value, u16 *out, s32 width)
{
    s32 suffix;

    if (value == 1)
    {
        suffix = offsetof(TextMessages, ordinal_st);
    }
    else if (value == 2)
    {
        suffix = offsetof(TextMessages, ordinal_nd);
    }
    else if (value == 3)
    {
        suffix = offsetof(TextMessages, ordinal_rd);
    }
    else
    {
        suffix = offsetof(TextMessages, ordinal_th);
    }
    text_copy_string(text_format_number(value, out, width), TEXT_AT_OFFSET(text_bank_starts.messages + suffix));
}

/* Draw the yes/no prompt and handle its buttons. */
static s32 save_confirm_prompt(void)
{
    s32 triggered;
    s32 pressed;

    text_draw_plain(&current_frame->otTags[0][2], LOCALIZED(text_bank.messages.save_yes_no), 43, 196);
    pressed = input_pad1->pressed;
    if (pressed & PAD_CROSS)
    {
        if (save_confirm_flag == 0)
        {
            sound_play(SOUND_MENU_CONFIRM);
        }
        else
        {
            sound_play(SOUND_MENU_CANCEL);
        }
        return save_confirm_flag;
    }
    if (pressed & PAD_CIRCLE)
    {
        sound_play(SOUND_MENU_CANCEL);
        return -1;
    }
    triggered = input_pad1->triggered;
    if (triggered & PAD_DOWN)
    {
        sound_play(SOUND_MENU_CURSOR);
        save_confirm_flag++;
    }
    else if (triggered & PAD_UP)
    {
        sound_play(SOUND_MENU_CURSOR);
        save_confirm_flag--;
    }
    if (save_confirm_flag < 0)
    {
        save_confirm_flag = 1;
    }
    if (save_confirm_flag >= 2)
    {
        save_confirm_flag = 0;
    }
    ui_draw_icon_0b_1(&current_frame->otTags[0][1], 35, (save_confirm_flag * 14 + 200));
    return 2;
}
/* Show the confirm icon; a press plays the confirm sound and returns pressResult, otherwise idleResult. */
s32 save_menu_wait_confirm(s16 idleResult, s16 pressResult)
{
    s16 idle = idleResult;
    s16 press = pressResult;
    ui_draw_icon_07_phase(&current_frame->otTags[0][1], 285, 217);
    if (input_pad1->pressed != 0)
    {
        sound_play(SOUND_MENU_CONFIRM);
        return press;
    }
    return idle;
}
/* Start the save screen: run the setup hooks, load the payload and, when load_image is set, the card file. */
static void save_screen_begin(s16 load_image)
{
    s32 value = load_image;
    save_screen_begin_hook_1();
    save_screen_begin_hook_2();
    save_load_payload();
    save_screen_begin_hook_3();
    if (value != 0)
        save_load_image_file();
}
/* Empty hooks, called in order by save_screen_begin. */
static void save_screen_begin_hook_1(void)
{
}
static void save_screen_begin_hook_2(void)
{
}

/* Load the save-data payload in chunks until the file read completes. */
static void save_load_payload(void)
{
    u8 *result;

    DrawSync(0);
    ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
    afile_read_bytes(0x10, 0, 0x900, save_icon_base, 1, 1);
    do
    {
        result = afile_read_bytes(0x10, 0, 0x900, save_icon_base, 1, 0);
    } while (result == SAVE_CARD_OK);
}

static void save_screen_begin_hook_3(void)
{
}

/* Load file 0xE, keep a copy of its image and unpack its five parts. */
static void save_load_image_file(void)
{
    u32 i;

    DrawSync(0);
    ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
    afile_read_bytes(SAVE_SPRITE_FILE, 0, SAVE_SPRITE_SIZE, save_file_header, 1, 1);
    while (afile_read_bytes(SAVE_SPRITE_FILE, 0, SAVE_SPRITE_SIZE, save_file_header, 1, 0) == 0)
    {
    }
    texture_pack_load(save_file_header + sizeof(SaveSheet));
    *(SaveSheet *)save_sheet = *(SaveSheet *)save_file_header;
    for (i = 0; i < SAVE_SPRITE_COUNT; i++)
    {
        anim_bind_file(&save_sprites[i], (AnimFile *)(save_sheet + save_sprite_offsets[i]));
    }
}

/* The save files' names on the card, by slot: the three farm slots, then the PDA file. */
static char *save_card_file_names[4] = {
    "BASLUS-01115-FARM0",
    "BASLUS-01115-FARM1",
    "BASLUS-01115-FARM2",
    "BISLPSP02489-FARMP",
};

/* Copy the card file name for `slot` into name; any slot 4 or above uses the first name. */
static void save_file_name(char *name, u32 slot)
{
    if (slot >= 4)
    {
        slot = 0;
    }
    strcpy(name, save_card_file_names[slot]);
}

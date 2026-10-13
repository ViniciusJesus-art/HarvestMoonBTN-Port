/* The farm item storage screens (tools, rucksack, cabinet and icebox) and the
 * pictures of the items shown on them. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <memory.h>
#include "types.h"
#include "sound.h"
#include "afile.h"
#include "anim.h"
#include "bg_tile.h"
#include "event_flags.h"
#include "farm_main.h"
#include "farm_state.h"
#include "farm_tables.h"
#include "item_storage.h"
#include "items.h"
#include "main.h"
#include "math_util.h"
#include "menu_window.h"
#include "pad.h"
#include "pad_vibration.h"
#include "picture_cells.h"
#include "player.h"
#include "scene.h"
#include "sound_request.h"
#include "text.h"
#include "text_bank.h"
#include "texture_pack.h"
#include "ui_icons.h"

/* One marker on the storage screen: its position, then twelve bytes of data. */
typedef struct FarmMarker {
    s16 x;
    s16 y;
    u8 next_cell[12];
} FarmMarker;

/* The screen's messages: the opening message of each list, and the headings of the
 * three lists. */
#define MSG_STORAGE_OPEN_TOOLBOX 0x378
#define MSG_STORAGE_OPEN_ICEBOX 0x379
#define MSG_STORAGE_OPEN_CABINET 0x37A
#define MSG_STORAGE_LIST_TOOLBOX 0x3AD
#define MSG_STORAGE_LIST_ICEBOX 0x3AE
#define MSG_STORAGE_LIST_CABINET 0x3AF

/* The item file is read in 2 KB sectors, 80 at a time. BLOCK and STEP differ by one
 * window, so each window follows the one before. */
#define ITEM_FILE_BASE 0x3C19000
#define ITEM_FILE_SECTOR 0x800
#define ITEM_PICTURE_WINDOW 80
#define ITEM_PICTURE_BLOCK 1024
#define ITEM_PICTURE_STEP 944
#define ITEM_PICTURE_SLOTS 72
#define ITEM_NOT_HELD 0xFF
#define GIFT_TEXT_BASE 0x3E8

static s32 item_pictures_load_step(s32 group);
static void item_result_draw(void);
static s32 item_select_input(void);
/* The category of a storage-screen slot (column): 0 the held item, 1 the bag row (slots
 * 1 to 8), 2 the storage grid (slot 9 and up). */
enum ItemSlot {
    ITEM_SLOT_HELD = 0,
    ITEM_SLOT_ROW = 1,
    ITEM_SLOT_GRID = 2,
};

/* The lists the picture loader and the state machine pick between. */
enum ItemListKind {
    ITEM_LIST_TOOLBOX = 0,
    ITEM_LIST_ICEBOX = 1,
    ITEM_LIST_CABINET = 2,
};

static s32 item_merge_dispatch(s32 first_col, s32 second_col, s32 first_row, s32 second_row);
static s32 item_merge_resolve(FarmStoredItem *item, s32 valueCol, FarmStoredItem *other, s32 otherCol);
static s32 item_merge_pair(FarmStoredItem *first, s32 first_arg, FarmStoredItem *second, s32 second_arg);
static s32 item_merge_values(FarmStoredItem *item, s32 first_col, FarmStoredItem *target, s32 second_col);
static void item_list_compact(FarmStoredItem *entries, s16 size);
static FarmStoredItem *item_row_at(s32 col, s32 row);
static s32 item_slot_category(s32 slot);
static void item_stack_merge(FarmStoredItem *from, FarmStoredItem *to);
static void item_swap(FarmStoredItem *a, FarmStoredItem *b);
static void item_copy(FarmStoredItem *from, FarmStoredItem *to);
static void item_cursor_move(s32 count);
static void item_cursor_message(u_long *source, u_long *text, s32 col, s32 row);
static void item_marker_draw_range(u_long *ot, s32 start, s32 count, FarmStoredItem *entry);
static void item_marker_draw(u_long *ot, s32 index, FarmStoredItem *item);
static void item_draw_status_numbers(u_long *display, s32 value);
static void item_status_draw(u_long *display, u_long *source, u16 *first, u16 *second);
static void item_picture_draw(u_long *display, s16 slot, s16 x, s16 y);
static void item_picture_queue(void *object, s16 slot, s16 x, s16 y, s32 scale);
static void item_merge_categories(FarmStoredItem *first, s32 firstSlot, FarmStoredItem *second, s32 secondSlot, s32 context);
static void item_picture_swap(u16 removed, u16 added, s32 context);
static void item_pictures_release_unused(void);

static RECT item_picture_rect;
static FarmPictureSlot item_picture_slots[ITEM_PICTURE_SLOTS];
static u16 item_number_scratch[12];

s16 item_picture_offset = 0;

/* The eight tool pictures appended to the icebox list (its entries 64 to 71). */
static u16 item_picture_ids[8] = {
    ITEM_KNIFE, ITEM_FRYING_PAN, ITEM_POT, ITEM_MIXER, ITEM_WHISK, ITEM_ROLLING_PIN, ITEM_OVEN, ITEM_SEASONING_SET};

/* Storage screen states. LIST shows one of the three lists, LOAD loads that list's
 * pictures first, PAN and TILT open the screen (messages 0x378 and 0x379). */
enum ItemResultState {
    ITEM_RESULT_LIST_TOOLBOX = 0,
    ITEM_RESULT_LIST_CABINET = 1,
    ITEM_RESULT_LIST_ICEBOX = 2,
    ITEM_RESULT_LOAD_TOOLBOX = 3,
    ITEM_RESULT_LOAD_CABINET = 4,
    ITEM_RESULT_LOAD_ICEBOX = 5,
    ITEM_RESULT_PAN = 6,
    ITEM_RESULT_TILT = 7,
    ITEM_RESULT_CLOSING = 0xFE,
    ITEM_RESULT_IDLE = 0xFF,
};

static s32 item_result_state = ITEM_RESULT_IDLE;

static s16 item_cursor_x = 0;

s16 item_drop_x = 0;

s16 item_held_x = ITEM_NOT_HELD;

static s16 item_cursor_y = 0;

s16 item_held_y = 0;

s16 item_held_list = ITEM_NOT_HELD;

static s16 item_anim_frame = 0;

static s16 item_anim_tick = 0;

/* Back to idle: the cursor at the first cell, nothing held, no pictures loaded. */
static void item_result_reset(void) {
    item_result_state = ITEM_RESULT_IDLE;
    item_cursor_x = 0;
    item_drop_x = 0;
    item_held_x = ITEM_NOT_HELD;
    item_cursor_y = 0;
    item_held_y = 0;
    item_held_list = ITEM_NOT_HELD;
    item_anim_frame = 0;
    item_anim_tick = 0;
    item_picture_offset = 0;
    item_picture_slots_clear();
}

/* Open the storage screen with the pan to the toolbox (the PAN state). */
void item_result_begin_state6(void) {
    item_result_reset();
    item_result_state = ITEM_RESULT_PAN;
    event_flag_set(EVENT_FLAG_ITEM_RESULT_STATE_6, 1);
    pad_vibration_start(0, 2);
}

/* Open the storage screen with the tilt to the icebox (the TILT state). */
void item_result_begin_state7(void) {
    item_result_reset();
    item_result_state = ITEM_RESULT_TILT;
    event_flag_set(EVENT_FLAG_ITEM_RESULT_STATE_7, 1);
    pad_vibration_start(0, 2);
}

/* Open the storage screen straight on the cabinet list, without a pan. */
void item_result_begin_state4(void) {
    item_result_reset();
    game_state.globalFlags |= GLOBAL_FLAG_TRANSITION_BEGIN;
    sound_play(SOUND_MENU_OPEN_CLOSE);
    item_result_state = ITEM_RESULT_LOAD_CABINET;
    menu_set_sound_enabled(0);
    text_set_input(0);
    menu_request_open(3, MSG_STORAGE_OPEN_CABINET);
}

/* Storage screen state machine, run each frame; 0xFF is idle. PAN and TILT wait for
 * the pan or tilt, then open message 0x378 or 0x379 and go to the LOAD state. LOAD
 * waits for the menu and the pictures, then goes to its LIST. CLOSING waits for the
 * menu window to close. Other states redraw the screen. */
void item_result_step(void) {
    int (*start)(void);
    s32 message;
    s32 next;
    s32 mode;
    s32 saved;

    if (item_result_state == ITEM_RESULT_IDLE) {
        return;
    }
    if (item_result_state == ITEM_RESULT_PAN || item_result_state == ITEM_RESULT_TILT) {
        if (item_result_state == ITEM_RESULT_PAN) {
            start = scene_pan_is_held;
            message = MSG_STORAGE_OPEN_TOOLBOX;
            next = ITEM_RESULT_LOAD_TOOLBOX;
        } else {
            start = scene_tilt_is_held;
            message = MSG_STORAGE_OPEN_ICEBOX;
            next = ITEM_RESULT_LOAD_ICEBOX;
        }
        if (start() != 0) {
            game_state.globalFlags |= GLOBAL_FLAG_TRANSITION_BEGIN;
            sound_play(SOUND_MENU_OPEN_CLOSE);
            menu_set_sound_enabled(0);
            text_set_input(0);
            menu_request_open(3, message);
            item_result_state = next;
        }
        return;
    }
    bg_tile_mode_sprite(&current_frame->otTags[0][27]);
    if (item_result_state == ITEM_RESULT_LOAD_TOOLBOX || item_result_state == ITEM_RESULT_LOAD_ICEBOX || item_result_state == ITEM_RESULT_LOAD_CABINET) {
        if (item_result_state == ITEM_RESULT_LOAD_TOOLBOX) {
            mode = ITEM_LIST_TOOLBOX;
            next = ITEM_RESULT_LIST_TOOLBOX;
        } else if (item_result_state == ITEM_RESULT_LOAD_ICEBOX) {
            mode = ITEM_LIST_ICEBOX;
            next = ITEM_RESULT_LIST_ICEBOX;
        } else {
            mode = ITEM_LIST_CABINET;
            next = ITEM_RESULT_LIST_CABINET;
        }
        if (menu_phase_is_result() != 0 && item_pictures_load_step(mode) == 0) {
            menu_begin_close();
            item_result_state = next;
        }
    } else if (item_result_state == ITEM_RESULT_CLOSING) {
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            menu_reset();
            item_result_state = ITEM_RESULT_IDLE;
            farm_unpause();
            game_state.globalFlags |= GLOBAL_FLAG_TRANSITION_END;
            game_state.player.stateFlags |= PLAYER_FLAG_REFRESH_HELD;
            event_flag_set(EVENT_FLAG_ITEM_RESULT_STATE_6, 0);
            event_flag_set(EVENT_FLAG_ITEM_RESULT_STATE_7, 0);
        }
    } else {
        item_picture_slots_unmark();
        saved = text_get_palette();
        text_set_palette(3);
        item_result_draw();
        text_set_palette(saved);
    }
}

/* Forget every picture slot. */
void item_picture_slots_clear(void) {
    s32 i;

    for (i = 0; i < ITEM_PICTURE_SLOTS; i++) {
        item_picture_slots[i].used = 0;
        item_picture_slots[i].id = 0;
    }
}

/* Mark every picture slot unused; the pictures drawn this frame mark theirs again. */
void item_picture_slots_unmark(void) {
    int i;
    for (i = ITEM_PICTURE_SLOTS - 1; i >= 0; i--)
        item_picture_slots[i].used = 0;
}

/* Load picture `id` (one 2 KB sector of the item file) into the first free
 * slot: read its size from the first frame, place it in the slot's VRAM cell
 * (eight 8-pixel cells per row, 24 per page) and upload its texture and CLUT. */
static void item_picture_load(u16 id) {
    struct AnimState anim;
    struct AnimState *state;
    s32 slot;
    AnimPartList *list;
    AnimPart *part;
    AnimImage *image;
    s32 left;

    slot = 0;
    while (slot < ITEM_PICTURE_SLOTS) {
        if (item_picture_slots[slot].id == 0) {
            break;
        }
        slot++;
    }
    if (slot == ITEM_PICTURE_SLOTS) {
        return;
    }
    afile_read_sectors(0, ITEM_FILE_BASE + id * ITEM_FILE_SECTOR, ITEM_FILE_SECTOR, 0, game_state.work.sector);
    state = &anim;
    anim_bind_file(state, (struct AnimFile *)game_state.work.sector);
    list = &state->partLists[ANIM_FRAMES(state, &state->ranges[0])->parts];
    part = ANIM_PARTS(state, list);
    part += list->count - 1;
    image = &state->images[part->image];
    item_picture_slots[slot].id = id;
    item_picture_slots[slot].picture.w = image->w * 4;
    item_picture_slots[slot].picture.h = image->h;
    item_picture_slots[slot].picture.x = slot % 8 * 8 + (left = slot / 24 * 64 + 320);
    item_picture_slots[slot].picture.y = slot % 24 / 8 * 64;
    item_picture_slots[slot].picture.clutX = slot % 4 * 16 + left;
    item_picture_slots[slot].picture.clutY = slot % 24 / 4 + 192;
    texture_pack_load_at(game_state.work.sector + 0x100, item_picture_slots[slot].picture.x - 320,
                  item_picture_slots[slot].picture.y,
                  item_picture_slots[slot].picture.clutX - 320, item_picture_slots[slot].picture.clutY - 254);
}

/* Draw the picture in the slot holding this id, marking the slot used; id 0
 * draws the empty marker. */
static void item_entry_find(u_long *ot, u16 key, s16 x, s16 y) {
    s32 i;

    if (key == 0) {
        item_empty_marker_draw(&ot[1], x, y);
    } else {
        i = 0;
        do {
            if (item_picture_slots[i].id == key) {
                item_picture_slots[i].used = 1;
                item_picture_draw(ot, i, x, y);
                break;
            }
            i++;
        } while (i < ITEM_PICTURE_SLOTS);
    }
}

/* Refresh the entry with this id, or recycle the last unused one and create it. */
void item_entry_refresh(u_long *object, u16 value, s16 x, s16 y) {
    s32 slot;

    if (value == 0) {
        item_empty_marker_draw(&object[1], x, y);
        return;
    }
    slot = 0;
    do {
        if (item_picture_slots[slot].id == value) {
            item_picture_slots[slot].used = 1;
            item_picture_draw(object, slot, x, y);
            return;
        }
        slot++;
    } while (slot < ITEM_PICTURE_SLOTS);
    for (slot = ITEM_PICTURE_SLOTS - 1; slot >= 0; slot--) {
        if (item_picture_slots[slot].used == 0) {
            item_picture_slots[slot].id = 0;
            item_picture_load(value);
            item_entry_find(object, value, x, y);
            return;
        }
    }
}

/* Find an existing farm entry, or recycle the last available entry. */
static void item_entry_place(void *object, u16 id, s16 x, s16 y, s32 extra) {
    u16 value;
    s32 slot;

    value = id;
    if (value != 0) {
        slot = 0;
        for (; slot < ITEM_PICTURE_SLOTS; slot++) {
            if (item_picture_slots[slot].id == value) {
                item_picture_slots[slot].used = 1;
                item_picture_queue(object, slot, x, y, extra);
                return;
            }
        }
        for (slot = ITEM_PICTURE_SLOTS - 1; slot >= 0; slot--) {
            if (item_picture_slots[slot].used == 0) {
                item_picture_slots[slot].id = 0;
                item_picture_load(value);
                item_entry_place(object, value, x, y, extra);
                return;
            }
        }
    }
}

/* Draw the cursor icon at (x, y), offset by (12, 24) to the cell. */
static void item_cursor_draw(void *ot, s32 x, s32 y) {
    x = x << 16;
    y = y << 16;
    x = x + (12 << 16);
    y = y + (24 << 16);
    x >>= 16;
    y >>= 16;
    ui_draw_icon_0b_1(ot, x, y);
}

/* Draw the effect icon at (x, y) as a textured quad. */
void item_effect_submit(u_long *ot, s16 posX, s16 posY) {
    s32 x;
    s32 y;

    x = posX;
    y = posY;
    anim_select_range(&ui_icon_anim, 0xB);
    anim_set_frame(&ui_icon_anim, 0);
    anim_draw_ft4_cursor(ot, &ui_icon_anim, ui_icon_anim.animation, ui_icon_anim.frame, x, y, 0x1000, 0, 0);
}

void item_empty_marker_draw(u_long *ot, s16 x, s16 y) {
    anim_select_range(&ui_icon_anim, 11);
    anim_set_frame(&ui_icon_anim, 2);
    anim_draw_sprt_cursor(ot, &ui_icon_anim, ui_icon_anim.animation, ui_icon_anim.frame, x, y);
}

/* Draw a seed's or crop's count as a badge: a fixed icon, then the tens digit (left out
 * when zero) and the units digit. */
void item_anim_draw(u_long *ot, s16 posX, s16 posY, s32 value) {
    s16 x;
    s16 y;
    s32 count;
    AnimState *sprite;

    x = posX;
    y = posY;
    sprite = &ui_icon_anim;
    anim_select_range(sprite, 0xA);
    x = x + 2;
    y = y + 22;
    anim_set_frame(sprite, 0xA);
    anim_draw_sprt_cursor(ot, sprite, sprite->animation, sprite->frame, x, y);
    x = x + 10;
    count = value / 10;
    if (count != 0) {
        anim_set_frame(sprite, count & 0xFF);
        anim_draw_sprt_cursor(ot, sprite, sprite->animation, sprite->frame, x, y);
    }
    x = x + 10;
    anim_set_frame(sprite, (value - count * 10) & 0xFF);
    anim_draw_sprt_cursor(ot, sprite, sprite->animation, sprite->frame, x, y);
}

/* Read one window of the item file (at the block's sector) into the staging buffer. */
static void item_pictures_read_block(s32 block) {
    afile_read_bytes(0, ITEM_FILE_BASE + ((block << 16) >> 5), ITEM_PICTURE_WINDOW * ITEM_FILE_SECTOR, STAGING_BUFFER, 1, 1);
}

/* The picture slots: a page holds 24 pictures in 8-wide rows; in VRAM a slot is 8 pixels
 * from its neighbour, a row 64 lines below the last, a page 64 pixels right of the last,
 * and the first page starts at x 0x140. */
#define STORAGE_PAGE_SLOTS 24
#define STORAGE_ROW_SLOTS  8
#define STORAGE_SLOT_PX    8
#define STORAGE_ROW_PX     64
#define STORAGE_PAGE_PX    64
#define STORAGE_BASE_X     320
#define VRAM_X(slot) ((slot) % STORAGE_ROW_SLOTS * STORAGE_SLOT_PX + (s16)((slot) / STORAGE_PAGE_SLOTS * STORAGE_PAGE_PX + STORAGE_BASE_X))
#define VRAM_Y(slot) (((slot) - (slot) / STORAGE_PAGE_SLOTS * STORAGE_PAGE_SLOTS) / STORAGE_ROW_SLOTS * STORAGE_ROW_PX)

static FarmMarker item_markers[18] = {
    {57, 34, {0x0D, 0x0D, 0x0D, 9, 9, 5, 2, 4, 4, 1, 1, 1}},
    {141, 20, {0x0D, 0x0D, 0x0D, 9, 9, 5, 0, 0, 0, 2, 2, 2}},
    {175, 20, {0x0E, 0x0E, 0x0E, 0x0A, 0x0A, 6, 1, 1, 1, 0, 3, 3}},
    {209, 20, {0, 0x0F, 0x0F, 0, 0x0B, 7, 0, 2, 2, 0, 4, 4}},
    {243, 20, {0, 0x10, 0x10, 0, 0x0C, 8, 0, 3, 3, 0, 0, 0}},
    {141, 54, {0, 0, 1, 0, 0, 9, 0, 0, 0, 0, 0, 6}},
    {175, 54, {0, 0, 2, 0, 0, 0x0A, 0, 0, 5, 0, 0, 7}},
    {209, 54, {0, 0, 3, 0, 0, 0x0B, 0, 0, 6, 0, 0, 8}},
    {243, 54, {0, 0, 4, 0, 0, 0x0C, 0, 0, 7, 0, 0, 0}},
    {141, 94, {1, 1, 5, 0x0D, 0x0D, 0x0D, 0x0C, 0x0C, 0x0C, 0x0A, 0x0A, 0x0A}},
    {175, 94, {2, 2, 6, 0x0E, 0x0E, 0x0E, 9, 9, 9, 0x0B, 0x0B, 0x0B}},
    {209, 94, {2, 3, 7, 0x0F, 0x0F, 0x0F, 0x0A, 0x0A, 0x0A, 0x0C, 0x0C, 0x0C}},
    {243, 94, {2, 4, 8, 0x10, 0x10, 0x10, 0x0B, 0x0B, 0x0B, 9, 9, 9}},
    {141, 128, {9, 9, 9, 1, 1, 1, 0x10, 0x10, 0x10, 0x0E, 0x0E, 0x0E}},
    {175, 128, {0x0A, 0x0A, 0x0A, 2, 2, 2, 0x0D, 0x0D, 0x0D, 0x0F, 0x0F, 0x0F}},
    {209, 128, {0x0B, 0x0B, 0x0B, 2, 3, 3, 0x0E, 0x0E, 0x0E, 0x10, 0x10, 0x10}},
    {243, 128, {0x0C, 0x0C, 0x0C, 2, 4, 4, 0x0F, 0x0F, 0x0F, 0x0D, 0x0D, 0x0D}},
    {0, 0, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
};

static s16 item_list_sizes[4] = {ITEM_STORE_SLOTS, ITEM_STORE_SLOTS + 8, ITEM_STORE_SLOTS, 3382};

/* Load the pictures of the list `group` (an ItemListKind; negative for the toolbox)
 * whose ids lie in the ITEM_PICTURE_WINDOW ids from `first`, out of the window already
 * read into the staging buffer. Returns 1 while that read is pending, -1 when the
 * slots run out, else 0. */
static s32 item_pictures_load(s16 first, s16 group) {
    AnimState anim;
    AnimState *state;
    s32 count;
    s32 mode;
    s32 i;
    u16 id;
    AnimFrame *frame;
    AnimPartList *list;
    AnimPart *parts;
    AnimPart *part1;
    AnimImage *image1;
    AnimPart *part2;
    AnimImage *image2;
    s32 slot;
    s32 row;
    FarmPictureSlot *picture;

    if (afile_read_bytes(0, ITEM_FILE_BASE + first * ITEM_FILE_SECTOR, ITEM_PICTURE_WINDOW * ITEM_FILE_SECTOR, STAGING_BUFFER, 1, 0) == 0) {
        return 1;
    }
    if (group >= 0) {
        mode = 3;
        if ((u16)group < 4) {
            mode = group;
        }
    } else {
        mode = 0;
    }
    count = item_list_sizes[mode];
    for (i = 0; i < count; i++) {
        switch (mode) {
        case ITEM_LIST_TOOLBOX:
            id = game_state.progress.estate.toolboxItems[i].id;
            break;
        case ITEM_LIST_ICEBOX:
            if (i < ITEM_STORE_SLOTS) {
                id = game_state.progress.estate.iceboxItems[i].id;
            } else {
                id = item_picture_ids[i - ITEM_STORE_SLOTS];
            }
            break;
        default:
            id = game_state.progress.estate.cabinetItems[i].id;
            break;
        }
        if (id == 0 || id < first || id >= first + ITEM_PICTURE_WINDOW) {
            continue;
        }
        image1 = 0;
        part1 = 0;
        anim_bind_file(&anim, (struct AnimFile *)&STAGING_BUFFER[(id - first) * ITEM_FILE_SECTOR]);
        state = &anim;
        image2 = 0;
        part2 = 0;
        frame = ANIM_FRAMES(state, &state->ranges[0]);
        if (frame->parts <= 0xFFFD) {
            list = &state->partLists[frame->parts];
            parts = ANIM_PARTS(state, list);
            part1 = &parts[list->count - 1];
            image1 = &state->images[part1->image];
        }
        frame = ANIM_FRAMES(state, &state->ranges[1]);
        if (frame->parts <= 0xFFFD) {
            list = &state->partLists[frame->parts];
            parts = ANIM_PARTS(state, list);
            part2 = &parts[list->count - 1];
            image2 = &state->images[part2->image];
        }
        for (slot = 0; slot < ITEM_PICTURE_SLOTS; slot++) {
            if (item_picture_slots[slot].id == 0) {
                break;
            }
        }
        picture = item_picture_slots;
        if (slot == ITEM_PICTURE_SLOTS) {
            return -1;
        }
        memset(&picture[slot], 0, sizeof(FarmPictureSlot));
        item_picture_slots[slot].id = id;
        item_picture_slots[slot].picture.x = VRAM_X(slot);
        item_picture_slots[slot].picture.y = VRAM_Y(slot);
        if (image1) {
            item_picture_slots[slot].picture.w = image1->w * 4;
            item_picture_slots[slot].picture.h = image1->h;
            item_picture_slots[slot].picture.offset[0][0] = part1->dx;
            item_picture_slots[slot].picture.offset[0][1] = part1->dy;
        }
        if (part2) {
            item_picture_slots[slot].picture.x2 = VRAM_X(slot);
            item_picture_slots[slot].picture.y2 = VRAM_Y(slot) + 32;
        }
        if (image2) {
            item_picture_slots[slot].picture.w2 = image2->w * 4;
            item_picture_slots[slot].picture.h2 = image2->h;
            item_picture_slots[slot].picture.offset[1][0] = part2->dx;
            item_picture_slots[slot].picture.offset[1][1] = part2->dy;
        }
        item_picture_slots[slot].picture.clutX = slot % 4 * 16 + (s16)(slot / 24 * 64 + 320);
        item_picture_slots[slot].picture.clutY = (row = (slot - slot / 24 * 24) / 4) + 192;
        texture_pack_load_at(&STAGING_BUFFER[(id - first) * ITEM_FILE_SECTOR + 0x100], item_picture_slots[slot].picture.x - 320,
                      item_picture_slots[slot].picture.y, item_picture_slots[slot].picture.clutX - 320,
                      row - 62);
    }
    return 0;
}

/* Restart the picture load (reset -1), or run one step of the icebox list's load. */
s16 item_pictures_load_poll(s16 reset) {
    if (reset == -1) {
        item_picture_offset = 0;
        return 0;
    }
    return item_pictures_load_step(ITEM_LIST_ICEBOX);
}

/* One step of the picture load: read the next window while the offset is below
 * ITEM_PICTURE_BLOCK, else load the window read last and step back by ITEM_PICTURE_STEP.
 * Returns 1 while more steps are needed. */
static s32 item_pictures_load_step(s32 group) {
    s16 amount;

    if (item_picture_offset < ITEM_PICTURE_BLOCK) {
        item_pictures_read_block(item_picture_offset);
        item_picture_offset += ITEM_PICTURE_BLOCK;
        return 1;
    }
    amount = item_picture_offset - ITEM_PICTURE_BLOCK;
    if (item_pictures_load(amount, group) != 0) {
        return 1;
    }
    item_picture_offset -= ITEM_PICTURE_STEP;
    return item_picture_offset < 319;
}

/* Draw the list screen: its headings and status panels, the toolbox or bag and the
 * list's entries with the cursor, and open the list's message. START closes the screen. */
static void item_result_draw(void) {
    s32 first;
    s32 second;
    FarmStoredItem *rows;
    FarmStoredItem *header;
    FarmStoredItem *entries;
    s32 message;
    u_long *panel;
    u_long *text;
    u_long *source;
    u_long *list;

    text_set_spacing(0, 2);
    if (menu_phase_is_result() != 0 && (input_pad1->pressed & PAD_START)) {
        item_result_state = ITEM_RESULT_CLOSING;
        menu_begin_close();
        sound_play(SOUND_MENU_OPEN_CLOSE);
        return;
    }
    if (item_result_state == ITEM_RESULT_LIST_TOOLBOX) {
        first = offsetof(TextMessages, storage_equip);
        second = offsetof(TextMessages, storage_toolbox);
        message = MSG_STORAGE_LIST_TOOLBOX;
        rows = game_state.progress.estate.toolboxItems;
        header = &game_state.player.held[HELD_TOOL];
        entries = game_state.player.tools;
    } else if (item_result_state == ITEM_RESULT_LIST_ICEBOX) {
        first = offsetof(TextMessages, storage_rucksack);
        second = offsetof(TextMessages, storage_icebox);
        message = MSG_STORAGE_LIST_ICEBOX;
        rows = game_state.progress.estate.iceboxItems;
        header = &game_state.player.held[HELD_ITEM];
        entries = game_state.player.items;
    } else if (item_result_state == ITEM_RESULT_LIST_CABINET) {
        first = offsetof(TextMessages, storage_rucksack);
        second = offsetof(TextMessages, storage_cabinet);
        message = MSG_STORAGE_LIST_CABINET;
        rows = game_state.progress.estate.cabinetItems;
        header = &game_state.player.held[HELD_ITEM];
        entries = game_state.player.items;
    } else {
        return;
    }
    panel = current_frame->otTags[0];
    text = &panel[26];
    source = &panel[25];
    list = &panel[23];
    item_status_draw(text, source, TEXT_AT_OFFSET(text_bank_starts.messages + first), TEXT_AT_OFFSET(text_bank_starts.messages + second));
    item_draw_status_numbers(source, ITEM_STORE_SLOTS);
    item_select_input();
    item_marker_draw(list, 0, header);
    item_marker_draw_range(list, 1, math_int_pow(2, game_state.player.rucksackLevel + 1), entries);
    item_cursor_move(ITEM_STORE_SLOTS);
    item_marker_draw_range(list, 9, 8, &rows[item_cursor_y * 8]);
    item_cursor_message(source, text, item_cursor_x, item_cursor_y);
    if (item_held_list == ITEM_NOT_HELD) {
        menu_set_sound_enabled(0);
        text_set_input(0);
        menu_request_open(3, message);
    }
}

/* Handle this frame's pad presses on the list. Cross picks up the item under the cursor
 * or drops the held one (merging it), circle cancels a held item or, with none held,
 * closes the list, triangle closes it on the item under the cursor, select tidies the
 * list. Returns 1 when a drop is committed. */
static s32 item_select_input(void) {
    s16 idle;
    s16 held;
    u32 flags;
    s32 result;

    idle = item_held_list;
    if (idle != ITEM_NOT_HELD || menu_phase_is_result() == 0) {
        return 0;
    }
    flags = input_pad1->pressed;
    if (flags & PAD_TRIANGLE) {
        if (item_row_at(item_cursor_x, item_cursor_y)->id != 0) {
            menu_begin_close();
            sound_play(SOUND_MENU_CONFIRM);
            item_held_list = 0;
            return 0;
        }
        sound_play(SOUND_MENU_REFUSE);
        return 0;
    }
    if (flags & PAD_CIRCLE) {
        if (item_held_x == idle) {
            menu_begin_close();
            item_result_state = ITEM_RESULT_CLOSING;
            sound_play(SOUND_MENU_OPEN_CLOSE);
            return 0;
        }
        sound_play(SOUND_MENU_CANCEL);
        item_held_x = idle;
        return 0;
    }
    if (flags & PAD_CROSS) {
        held = item_held_x;
        if (held == idle) {
            sound_play(SOUND_MENU_CONFIRM);
            item_held_x = item_cursor_x;
            item_held_y = item_cursor_y;
        } else {
            item_drop_x = item_cursor_x;
            if (held == item_drop_x && (item_held_y == item_cursor_y || held < 9)) {
                item_cursor_x = 0;
            }
            item_pictures_release_unused();
            result = item_merge_dispatch(item_held_x, item_cursor_x, item_held_y, item_cursor_y);
            item_pictures_release_unused();
            item_cursor_x = item_drop_x;
            if (result == 1) {
                sound_play(SOUND_MENU_CONFIRM);
                item_held_x = ITEM_NOT_HELD;
                return 1;
            }
            sound_play(SOUND_MENU_REFUSE);
            return 0;
        }
    } else if (flags & PAD_SELECT) {
        if (item_held_x == idle) {
            sound_play(SOUND_MENU_CONFIRM);
            switch (item_result_state) {
            case ITEM_RESULT_LIST_TOOLBOX:
                item_list_compact(game_state.progress.estate.toolboxItems, ITEM_STORE_SLOTS);
                break;
            case ITEM_RESULT_LIST_ICEBOX:
                item_list_compact(game_state.progress.estate.iceboxItems, ITEM_STORE_SLOTS);
                break;
            case ITEM_RESULT_LIST_CABINET:
                item_list_compact(game_state.progress.estate.cabinetItems, ITEM_STORE_SLOTS);
                break;
            default:
                return 0;
            }
            return 0;
        }
        sound_play(SOUND_MENU_REFUSE);
    }
    return 0;
}

/* Merge the items at two storage positions, if they may be combined. */
static s32 item_merge_dispatch(s32 first_col, s32 second_col, s32 first_row, s32 second_row) {
    FarmStoredItem *first;
    FarmStoredItem *second;

    if ((first_col != second_col) || (first_row != second_row)) {
        first = item_row_at(first_col, first_row);
        second = item_row_at(second_col, second_row);
        if (first->id == 0 && second->id == 0) return 1;
        if (second->id == 0) return item_merge_resolve(first, first_col, second, second_col);
        if (first->id == 0) return item_merge_resolve(second, second_col, first, first_col);
        if (first->id != second->id)
            return item_merge_pair(first, first_col, second, second_col);
        return item_merge_values(first, first_col, second, second_col);
    }
    return 0;
}

/* Move `item` into the empty cell `other`, or split one off its stack into it. Returns 1
 * when the drop is made. */
static s32 item_merge_resolve(FarmStoredItem *item, s32 valueCol, FarmStoredItem *other, s32 otherCol) {
    s32 result;
    s32 status;
    s32 count;
    status = item_slot_category(otherCol);
    result = status;
    if (result == 1) {
        goto one;
    }
    if (result < 2) {
        if (result == 0) {
            goto zero;
        }
        return 0;
    }
    if (status != 2) {
        return 0;
    }
    if (item_class_word_negative(item->id) == 0) {
        return 0;
    }
    if (item_result_state != item_class_index(item->id)) {
        goto done;
    }
    goto merge;
one:
    if (item_class_word_negative(item->id) == 0) {
        return 0;
    }

zero:
    status = item_category(item->id);

    if (status == 0) {
        goto failed;
    }
    if (status < 0) {
        return 0;
    }
    if (status >= 3) {
        return 0;
    }
merge:
    item_merge_categories(item, valueCol, other, otherCol, 0);

    item_swap(item, other);
    return 1;
failed:

    item_merge_categories(item, valueCol, other, otherCol, 0);
    item_copy(item, other);
    other->count = 1;
    count = (u16)item->count - 1;
    item->count = count;
    result = 1;
    if ((count << 16) > 0) {
        return result;
    }
    picture_cell_place_item(item);
    return 1;
done:
    return 0;
}

/* Check that two different items may trade places (each must suit the cell it moves to),
 * then merge their pictures and swap them. Returns 1 when they are swapped. */
static s32 item_merge_pair(FarmStoredItem *first, s32 first_arg, FarmStoredItem *second, s32 second_arg) {
    s32 valueCol;
    s32 merge;

    valueCol = item_slot_category(second_arg);
    switch (valueCol) {
    case ITEM_SLOT_GRID:
        if (item_class_word_negative(first->id) == 0) {
            return 0;
        }
        if (item_result_state != item_class_index(first->id)) {
            return 0;
        }
        break;
    case ITEM_SLOT_ROW:
        if (item_class_word_negative(first->id) == 0) {
            return 0;
        }
        /* fall through */
    case ITEM_SLOT_HELD:
        if (item_category(first->id) == ITEM_CATEGORY_CROP && first->count >= 2) {
            return 0;
        }
        break;
    }

    valueCol = item_slot_category(first_arg);
    merge = 1;
    switch (valueCol) {
    case ITEM_SLOT_GRID:
        if (item_class_word_negative(second->id) == 0) {
            return 0;
        }
        if (item_result_state != item_class_index(second->id)) {
            return 0;
        }
        item_merge_categories(first, first_arg, second, second_arg, merge);
        item_swap(first, second);
        return 1;
    case ITEM_SLOT_ROW:
        if (item_class_word_negative(second->id) == 0) {
            return 0;
        }
        /* fall through */
    case ITEM_SLOT_HELD:
        if (item_category(second->id) == ITEM_CATEGORY_CROP && second->count >= 2) {
            return 0;
        }
        item_merge_categories(first, first_arg, second, second_arg, 1);
        item_swap(first, second);
        return 1;
    }
    return 0;
}

/* Stack `item` onto `target` (the cell at second_col) when the category allows: a tool
 * never stacks, and a held or bag-row cell takes seeds only. */
static s32 item_merge_values(FarmStoredItem *item, s32 first_col, FarmStoredItem *target, s32 second_col) {
    switch (item_slot_category(second_col)) {
    case 0:
    case 1:
        if (item_category(item->id) != ITEM_CATEGORY_SEED) {
            return 0;
        }
        /* fall through */
    case ITEM_SLOT_GRID:
        if (item_category(item->id) == ITEM_CATEGORY_TOOL) {
            return 0;
        }
        item_stack_merge(item, target);
        return 1;
    default:
        return 0;
    }
}

/* Compact a list of item records in place (id 0 marks an empty one): each
 * empty entry takes the first occupied one at or after it, and the entry
 * moved is cleared. */
static void item_list_compact(FarmStoredItem *entries, s16 size) {
    s32 i;
    s32 j;
    FarmStoredItem *entry;
    FarmStoredItem *empty;

    for (i = 0; i < size; i++) {
        entry = &entries[i];
        if (entry->id != 0) {
            continue;
        }
        empty = entry;
        for (j = 0; j < size - i; j++) {
            entry = &entries[i] + j;
            if (entry->id != 0) {
                *empty = *entry;
                memset(entry, 0, sizeof(FarmStoredItem));
                break;
            }
        }
    }
}

/* The stored item at column col, row row of the open screen: the held tool or item for
 * column 0, the bag cell for columns 1 to 8, and the list entry for the rest. */
static FarmStoredItem *item_row_at(s32 col, s32 row) {
    s32 index;
    s32 offset;
    s32 result;
    s32 state;
    FarmStoredItem *table;
    FarmStoredItem *value;

    offset = row * 8 - 9;
    index = offset + col;
    if (item_result_state == ITEM_RESULT_LIST_TOOLBOX) {
        table = game_state.player.held;
        value = &game_state.player.held[1 + col];
    } else {
        table = &game_state.player.held[1];
        value = &game_state.player.held[9 + col];
    }
    result = item_slot_category(col);
    switch (result) {
    case ITEM_SLOT_HELD:
        return table;
    case ITEM_SLOT_ROW:
        return value;
    default:
        break;
    }
    state = item_result_state;
    if (state == ITEM_RESULT_LIST_TOOLBOX) return &game_state.progress.estate.toolboxItems[index];
    if (state == ITEM_RESULT_LIST_ICEBOX) return &game_state.progress.estate.iceboxItems[index];
    return &game_state.progress.estate.cabinetItems[index];
}

static s32 item_slot_category(s32 slot) {
    if (slot == 0) return ITEM_SLOT_HELD;
    if (slot < 9) return ITEM_SLOT_ROW;
    return ITEM_SLOT_GRID;
}

/* Add `from`'s count to `to`, capped at 99; the overflow stays in `from`. Otherwise `from`
 * is placed in the picture grid. */
static void item_stack_merge(FarmStoredItem *from, FarmStoredItem *to) {
    s32 sum = (u16)to->count + (u16)from->count;

    to->count = sum;
    if ((s16)sum >= 100) {
        to->count = 99;
        from->count = sum - 99;
    } else {
        picture_cell_place_item(from);
    }
}

/* Swap two stored items. */
static void item_swap(FarmStoredItem *a, FarmStoredItem *b) {
    FarmStoredItem tmp;
    tmp = *a;
    *a = *b;
    *b = tmp;
}

/* Copy a stored item over another. */
static void item_copy(FarmStoredItem *from, FarmStoredItem *to) {
    *to = *from;
}

/* Move the storage cursor with the d-pad and the shoulder buttons, wrapping the
 * page over count / 8, then draw the held marker and the cursor. */
static void item_cursor_move(s32 count) {
    s32 delta = 0;
    s32 dir = 0;
    u32 pad;
    u32 buttons;

    if (menu_phase_is_result() != 0) {
        pad = input_pad1->triggered;
        if (pad & PAD_ARROWS_MASK) {
            if (pad & PAD_UP) {
                dir = 0;
            } else if (pad & PAD_DOWN) {
                dir = 1;
            } else if (pad & PAD_LEFT) {
                if (item_cursor_x == 9 || item_cursor_x == 13) {
                    delta = count / 8 - 1;
                }
                dir = 2;
            } else {
                if (item_cursor_x == 12 || item_cursor_x == 16) {
                    delta = 1;
                }
                dir = 3;
            }
            if (item_held_list == ITEM_NOT_HELD) {
                /* The marker to the left or right of the cursor in the marker table. */
                item_cursor_x = item_markers[0].next_cell[game_state.player.rucksackLevel + dir * 3 + item_cursor_x * 16];
                sound_play(SOUND_MENU_CURSOR);
            }
        }
        if (item_held_list == ITEM_NOT_HELD) {
            buttons = input_pad1->triggered;
            if (buttons & (PAD_L2 | PAD_L1)) {
                sound_play(SOUND_MENU_CURSOR);
                delta += count / 8 - 1;
            } else if (buttons & (PAD_R2 | PAD_R1)) {
                sound_play(SOUND_MENU_CURSOR);
                delta++;
            }
        }
    }
    if (item_held_list == ITEM_NOT_HELD) {
        item_cursor_y = item_cursor_y + delta - (item_cursor_y + delta) / (count / 8) * (count / 8);
    }
    if (item_held_x != ITEM_NOT_HELD) {
        s32 state = item_slot_category(item_held_x);

        if (state >= 0 && (state < 2 || (state == 2 && item_cursor_y == item_held_y))) {
            item_effect_submit(&current_frame->otTags[0][21], item_markers[item_held_x].x, item_markers[item_held_x].y);
        }
    }
    item_cursor_draw(&current_frame->otTags[0][20], item_markers[item_cursor_x].x, item_markers[item_cursor_x].y);
}

/* Open the message for the item under the cursor (a gift shows its contents' message).
 * When the menu reports TEXT_WINDOW_CLOSED the list is left and the screen closes. */
static void item_cursor_message(u_long *source, u_long *text, s32 col, s32 row) {
    FarmStoredItem *record;
    s32 value;

    if (item_held_list != ITEM_NOT_HELD) {
        record = item_row_at(col, row);
        value = (record->id == ITEM_GIFT) ? stored_item_contents(record) + GIFT_TEXT_BASE : record->id;
        menu_request_open(3, value);
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            item_held_list = ITEM_NOT_HELD;
            menu_begin_close();
        }
    }
}

/* Draw the markers of `count` entries from index `start`, taking them in order from `entry`. */
static void item_marker_draw_range(u_long *ot, s32 start, s32 count, FarmStoredItem *entry) {
    s32 i;

    for (i = start; i < start + count; i++) {
        item_marker_draw(ot, i, entry);
        entry++;
    }
}

/* Draw the marker for storage entry `index`: an empty entry draws the empty marker, a filled
 * one its picture, and a seed or a grid crop its count badge. */
static void item_marker_draw(u_long *ot, s32 index, FarmStoredItem *item) {
    if (item->id == 0) {
        item_empty_marker_draw(&ot[1], item_markers[index].x, item_markers[index].y);
        return;
    }
    if (index < 9) {
        picture_cell_build_prim(item->id, 0, ot, item_markers[index].x, item_markers[index].y, 8);
    } else {
        item_entry_refresh(ot, item->id, item_markers[index].x, item_markers[index].y);
    }
    switch (item_category(item->id)) {
    case ITEM_CATEGORY_TOOL:
        break;
    case ITEM_CATEGORY_SEED:
        item_anim_draw(&current_frame->otTags[0][22], item_markers[index].x,
                      item_markers[index].y, item->count);
        break;
    case ITEM_CATEGORY_CROP:
        if (item_slot_category(index) == ITEM_SLOT_GRID) {
            item_anim_draw(&current_frame->otTags[0][22], item_markers[index].x,
                          item_markers[index].y, item->count);
        }
        break;
    }
}

/* Draw the two effect frames of the current page and step the page every 9 ticks. */
static void item_page_effect_draw(u_long *target) {
    anim_select_range(&ui_icon_anim, 0xD);
    anim_set_frame(&ui_icon_anim, item_anim_frame);
    anim_draw_sprt_cursor(target, &ui_icon_anim, ui_icon_anim.animation, ui_icon_anim.frame, 125, 144);
    anim_select_range(&ui_icon_anim, 0xC);
    anim_set_frame(&ui_icon_anim, item_anim_frame);
    anim_draw_sprt_cursor(target, &ui_icon_anim, ui_icon_anim.animation, ui_icon_anim.frame, 277, 144);
    item_anim_tick++;
    if (item_anim_tick >= 9) {
        s32 next = item_anim_frame + 1;

        item_anim_tick = 0;
        item_anim_frame = item_anim_frame + 1 - next / 4 * 4;
    }
}

/* Draw the row number and the page count either side of the slash label. */
static void item_draw_status_numbers(u_long *display, s32 value) {
    u16 *end;

    end = item_number_scratch;
    end = text_format_number(item_cursor_y + 1, end, 1);
    *end = 0xFFFF;
    text_draw_plain(display, item_number_scratch, 40, 110);
    text_draw_plain(display, LOCALIZED(text_bank.messages.slash), 53, 110);
    *text_format_number(value / 8, item_number_scratch, 1) = 0xFFFF;
    text_draw_plain(display, item_number_scratch, 66, 110);
}

/* Draw the storage screen's three status panels, the page effect and the two list headings. */
static void item_status_draw(u_long *display, u_long *source, u16 *first, u16 *second) {
    window_frame_draw_at(display, 1, 25, 16, 105, 74);
    window_frame_draw_at(display, 1, 121, 16, 174, 74);
    window_frame_draw_at(display, 1, 25, 90, 270, 74);
    item_page_effect_draw(source);
    text_draw_plain(source, first, 29, 20);
    text_draw_plain(source, second, 29, 94);
}

/* Draw the frame for picture `slot` at (x, y), then the picture as a sprite,
 * centred in a 32 x 32 cell when smaller, with its texture-page primitive after it. */
static void item_picture_draw(u_long *display, s16 slot, s16 x, s16 y) {
    u_long *ot = display;
    s32 pageX;
    s32 pageY;
    SPRT *sprite;
    DR_MODE *mode;

    item_empty_marker_draw(ot + 1, x, y);
    if (item_picture_slots[slot].picture.w < 32) {
        x += (32 - item_picture_slots[slot].picture.w) / 2;
    }
    if (item_picture_slots[slot].picture.h < 32) {
        y += (32 - item_picture_slots[slot].picture.h) / 2;
    }
    sprite = gs_work_alloc(0);
    setSprt(sprite);
    sprite->clut = GetClut(item_picture_slots[slot].picture.clutX, item_picture_slots[slot].picture.clutY);
    setRGB0(sprite, 0x80, 0x80, 0x80);
    setXY0(sprite, x, y);
    setSemiTrans(sprite, 0);
    setWH(sprite, item_picture_slots[slot].picture.w, item_picture_slots[slot].picture.h);
    setUV0(sprite, (item_picture_slots[slot].picture.x - 320) * 4, item_picture_slots[slot].picture.y);
    addPrim(ot, sprite);
    sprite++;
    mode = (DR_MODE *)sprite;
    setlen(mode, 1);
    pageY = item_picture_slots[slot].picture.y;
    pageX = item_picture_slots[slot].picture.x;
    mode->code[0] = (((pageY & 0x100) >> 4) | ((pageX & 0x3C0) >> 6) | ((pageY & 0x200) << 2)) | 0xE1000600;
    addPrim(ot, mode);
    gs_work_release(mode + 1);
}

/* Queue picture `slot` of the item_picture_slots table as a POLY_FT4 centred on
 * (x, y), its size scaled by scale / 4096, into the ordering-table entry
 * `object`, and advance the primitive buffer. */
static void item_picture_queue(void *object, s16 slot, s16 x, s16 y, s32 scale) {
    s32 pageX;
    s32 pageY;
    POLY_FT4 *prim;
    s16 w;
    s16 left;
    s16 top;
    s16 h;

    w = item_picture_slots[slot].picture.w;
    h = item_picture_slots[slot].picture.h;
    w *= scale / 4096;
    h *= scale / 4096;
    prim = current_frame->primCursor.ft4;
    setPolyFT4(prim);
    prim->clut = getClut(item_picture_slots[slot].picture.clutX, item_picture_slots[slot].picture.clutY);
    setRGB0(prim, 0x80, 0x80, 0x80);
    setShadeTex(prim, 0);
    pageX = item_picture_slots[slot].picture.x;
    pageY = item_picture_slots[slot].picture.y;
    prim->tpage = ((pageY & 0x100) >> 4) | ((pageX & 0x3C0) >> 6) |
                  ((item_picture_slots[slot].picture.y & 0x200) << 2);
    setUVWH(prim, item_picture_slots[slot].picture.x, item_picture_slots[slot].picture.y, item_picture_slots[slot].picture.w,
            item_picture_slots[slot].picture.h);
    left = x - w / 2;
    top = y - h / 2;
    setXYWH(prim, left, top, w, h);
    addPrim(object, prim);
    current_frame->primCursor.ft4 = prim + 1;
}

/* Move the item pictures between two storage positions, depending on their categories. */
static void item_merge_categories(FarmStoredItem *first, s32 firstSlot, FarmStoredItem *second, s32 secondSlot, s32 context) {
    s32 second_category;
    s32 first_category;

    second_category = item_slot_category(secondSlot);
    first_category = item_slot_category(firstSlot);
    if (first_category == 2) {
        if (second_category != 2) item_picture_swap(second->id, first->id, context);
    } else if (second_category == 2) {
        item_picture_swap(first->id, second->id, context);
    }
}

#define REMAINDER(a, n) ((a) - (a) / (n) * (n))

/* Take picture `removed` off the farm and put picture `added` on it (either
 * may be 0). A removed picture is saved to the staging buffer and parked in a
 * free slot; an added one is moved from its parking slot onto the farm. Context
 * 1 also releases the unused pictures first and the removed one. */
static void item_picture_swap(u16 removed, u16 added, s32 context) {
    FarmPictureSlot saved;
    s16 i;
    s16 slot;
    s16 row;
    s16 addedRow;
    s16 reserved;
    s16 column;
    s32 clutTop;

    if (context == 1) {
        item_pictures_release_unused();
    }
    reserved = 0;
    column = 0;

    if (removed != 0) {
        row = (s16)item_picture_row(removed) != 0;
        for (i = 0; i < 9; i++) {
            if (farm_area.mode.pictureIds[row][i] == removed) {
                break;
            }
        }
        if (i == 9) {
            return;
        }
        if (farm_area.mode.pictureEntries[row][i].clutX >= 1008) {
            reserved = 1;
        }
        item_picture_rect.w = 8;
        item_picture_rect.x = farm_area.mode.pictureEntries[row][i].x;
        item_picture_rect.y = farm_area.mode.pictureEntries[row][i].y;
        item_picture_rect.h = row == 0 ? 32 : 64;
        if (reserved == 1) {
            item_picture_rect.x += 960;
        } else {
            item_picture_rect.x += 832;
        }
        StoreImage(&item_picture_rect, (u_long *)STAGING_BUFFER);
        DrawSync(0);
        item_picture_rect.w = 16;
        item_picture_rect.h = 1;
        item_picture_rect.x = farm_area.mode.pictureEntries[row][i].clutX;
        item_picture_rect.y = farm_area.mode.pictureEntries[row][i].clutY;
        StoreImage(&item_picture_rect, (u_long *)STAGING_CLUT);
        DrawSync(0);
        saved.used = 1;
        saved.id = removed;
        saved.picture = farm_area.mode.pictureEntries[row][i];
        if (context == 1) {
            picture_release_item(removed);
        }
    }

    if (added != 0) {
        addedRow = (s16)item_picture_row(added) != 0;
        for (slot = 0; slot < ITEM_PICTURE_SLOTS; slot++) {
            if (item_picture_slots[slot].id == added) {
                break;
            }
        }
        if (slot == ITEM_PICTURE_SLOTS) {
            return;
        }
        i = picture_slot_claim(added, addedRow);
        if (i == -1) {
            return;
        }
        reserved = picture_row_clut_free(addedRow);
        farm_area.mode.pictureIds[addedRow][i] = added;
        farm_area.mode.pictureEntries[addedRow][i] = item_picture_slots[slot].picture;
        item_picture_rect.w = 8;
        item_picture_rect.x = item_picture_slots[slot].picture.x;
        item_picture_rect.y = item_picture_slots[slot].picture.y;
        item_picture_rect.h = addedRow == 0 ? 32 : 64;
        if (reserved == 1) {
            farm_area.mode.pictureEntries[addedRow][i].x = addedRow * 8 + 40;
            farm_area.mode.pictureEntries[addedRow][i].y = 189;
            farm_area.mode.pictureEntries[addedRow][i].x2 = farm_area.mode.pictureEntries[addedRow][i].x;
            farm_area.mode.pictureEntries[addedRow][i].y2 = farm_area.mode.pictureEntries[addedRow][i].y + farm_area.mode.pictureEntries[addedRow][i].h;
            MoveImage2(&item_picture_rect, addedRow * 8 + 1000, 189);
        } else {
            column = picture_free_slot(addedRow);
            farm_area.mode.pictureEntries[addedRow][i].x = column * 8;
            farm_area.mode.pictureEntries[addedRow][i].y = addedRow * 32 + 155;
            farm_area.mode.pictureEntries[addedRow][i].x2 = farm_area.mode.pictureEntries[addedRow][i].x;
            farm_area.mode.pictureEntries[addedRow][i].y2 = farm_area.mode.pictureEntries[addedRow][i].y + farm_area.mode.pictureEntries[addedRow][i].h;
            MoveImage2(&item_picture_rect, column * 8 + 832, addedRow * 32 + 155);
        }
        item_picture_rect.w = 16;
        item_picture_rect.x = item_picture_slots[slot].picture.clutX;
        item_picture_rect.h = 1;
        item_picture_rect.y = item_picture_slots[slot].picture.clutY;
        if (reserved == 1) {
            farm_area.mode.pictureEntries[addedRow][i].clutX = 1008;
            farm_area.mode.pictureEntries[addedRow][i].clutY = addedRow + 254;
            MoveImage2(&item_picture_rect, 1008, addedRow + 254);
        } else {
            farm_area.mode.pictureEntries[addedRow][i].clutX = REMAINDER(column, 4) * 16 + 832;
            farm_area.mode.pictureEntries[addedRow][i].clutY = column / 4 + (clutTop = addedRow * 2 + 251);
            MoveImage2(&item_picture_rect, REMAINDER(column, 4) * 16 + 832, column / 4 + clutTop);
        }
    }

    if (removed != 0) {
        for (slot = 0; slot < ITEM_PICTURE_SLOTS; slot++) {
            if (item_picture_slots[slot].id == removed) {
                return;
            }
        }
        row = (s16)item_picture_row(removed) != 0;
        for (slot = 0; slot < ITEM_PICTURE_SLOTS; slot++) {
            if (item_picture_slots[slot].id == 0) {
                break;
            }
        }
        if (slot == ITEM_PICTURE_SLOTS) {
            return;
        }
        saved.picture.x = REMAINDER(slot, 8) * 8 + (s16)(slot / 24 * 64 + 320);
        saved.picture.y = REMAINDER(slot, 24) / 8 * 64;
        item_picture_rect.x = saved.picture.x;
        item_picture_rect.y = saved.picture.y;
        item_picture_rect.w = 8;
        item_picture_rect.h = row == 0 ? 32 : 64;
        LoadImage(&item_picture_rect, (u_long *)STAGING_BUFFER);
        DrawSync(0);
        saved.picture.clutX = REMAINDER(slot, 4) * 16 + (s16)(slot / 24 * 64 + 320);
        saved.picture.clutY = REMAINDER(slot, 24) / 4 + 192;
        item_picture_rect.x = saved.picture.clutX;
        item_picture_rect.y = saved.picture.clutY;
        item_picture_rect.w = 16;
        item_picture_rect.h = 1;
        LoadImage(&item_picture_rect, (u_long *)STAGING_CLUT);
        DrawSync(0);
        item_picture_slots[slot].id = removed;
        item_picture_slots[slot].picture = saved.picture;
    }
}

/* Each item the player carries cancels one placed picture with the same id;
 * placed pictures that nobody carries are released. */
static void item_pictures_release_unused(void) {
    s16 ids[36];
    s32 i;
    s32 j;
    s16 id;

    for (i = 0; i < 18; i++) {
        if (i < 8) {
            ids[i] = game_state.player.tools[i].id;
        } else if (i == 8) {
            ids[i] = game_state.player.held[HELD_TOOL].id;
        } else if (i < 17) {
            ids[i] = game_state.player.items[i - 9].id;
        } else if (i == 17) {
            ids[i] = game_state.player.held[HELD_ITEM].id;
        }
    }
    for (i = 0; i < 18; i++) {
        id = farm_area.mode.pictureIds[0][i];
        if (id == 0) {
            continue;
        }
        for (j = 0; j < 18; j++) {
            s16 *slot = &ids[j];

            if (*slot != 0 && id == *slot) {
                id = 0;
                *slot = 0;
                break;
            }
        }
        if (id != 0) {
            picture_release_item(id);
        }
    }
}

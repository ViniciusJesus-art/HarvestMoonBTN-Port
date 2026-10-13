/* The item-swap screen: moving and stacking items between the player's held slots
 * and the rucksack. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "farm_main.h"
#include "farm_state.h"
#include "farm_tables.h"
#include "item_storage.h"
#include "main.h"
#include "math_util.h"
#include "menu_window.h"
#include "pad.h"
#include "picture_cells.h"
#include "player.h"
#include "prim.h"
#include "sound_request.h"
#include "storage_swap.h"
#include "text.h"
#include "text_bank.h"
#include "ui_icons.h"

/* A frame rectangle. */
typedef struct FarmFrameRect {
    s16 x;
    s16 y;
    s16 w;
    s16 h;
} FarmFrameRect;

/* One spot of the storage_spots table. */
typedef struct FarmSpot {
    s16 x;
    s16 y;
    u8 next_spot[24];  /* the spot the cursor moves to, per direction and rucksack level */
} FarmSpot;

#define SPOT_NONE 0xFF
#define SPOT_HELD_TOOL 0
#define SPOT_HELD_ITEM 9
#define SPOT_TOOL_FIRST 1
#define SPOT_ITEM_FIRST 10
#define STACK_MAX 99
#define GIFT_TEXT_BASE 0x3E8
#define MSG_STORAGE_SWAP_HINT 0x3AC

/* The spot-swap screen's states: browsing the spots, taking the spot under the cursor,
 * and leaving the screen. */
enum StorageSwapState {
    SWAP_IDLE = 0,
    SWAP_BROWSE = 1,
    SWAP_TAKE = 2,
    SWAP_LEAVE = 3,
};

/* The cursor's directions; a spot that is marked shifts them to the second block of
 * the next_spot table. */
enum SpotDirection {
    SPOT_UP = 0,
    SPOT_DOWN = 1,
    SPOT_LEFT = 2,
    SPOT_RIGHT = 3,
    SPOT_LINKS_MARKED = 4,
};

/* The draw layers of the spot-swap screen in the frame's ot. */
enum StorageSwapLayer {
    SWAP_LAYER_SHADE = 26,
    SWAP_LAYER_FRAMES = 25,
    SWAP_LAYER_TEXT = 24,
    SWAP_LAYER_SPOTS = 23,
    SWAP_LAYER_COUNTS = 22,
    SWAP_LAYER_MARK = 21,
    SWAP_LAYER_CURSOR = 20,
};

static void storage_swap_screen(void);
static void storage_item_message(FarmStoredItem *item);
static s32 storage_swap_spots(s32 from, s32 to);
static s32 storage_move_item(FarmStoredItem *item, s32 spot, FarmStoredItem *other, s32 otherSpot);
static s32 storage_try_swap(FarmStoredItem *first, s32 firstSpot, FarmStoredItem *second, s32 secondSpot);
static s32 storage_stack_items(FarmStoredItem *item, s32 spot, FarmStoredItem *other, s32 otherSpot);
static s32 storage_spot_is_held(s32 spot);
static void storage_stack_count(FarmStoredItem *from, FarmStoredItem *to);
static void storage_item_swap(FarmStoredItem *a, FarmStoredItem *b);
static FarmStoredItem *storage_spot_item(s32 spot);
static void storage_draw_spot(u_long *ot, s32 index, FarmStoredItem *item);

static s32 storage_swap_state = SWAP_IDLE;

static s32 storage_swap_cursor = 0;

static s32 storage_swap_pick = 0;

static s32 storage_swap_marked = SPOT_NONE;

static void storage_swap_reset(void) {
    storage_swap_state = SWAP_IDLE;
    storage_swap_cursor = 0;
    storage_swap_pick = 0;
    storage_swap_marked = SPOT_NONE;
}

/* Open the spot-swap screen from idle. */
void storage_swap_begin(void) {
    if (storage_swap_state == SWAP_IDLE) {
        storage_swap_reset();
        storage_swap_state = SWAP_BROWSE;
        sound_play(3);
    }
}

/* Per-frame tick: shade the screen and draw the spot-swap screen while it is open. When
 * it is leaving, wait for the menu window to close, then return the player to play. */
void storage_swap_tick(void) {
    s32 saved_mode;

    if (storage_swap_state == SWAP_IDLE) {
        return;
    }
    prim_add_tile(&current_frame->otTags[0][SWAP_LAYER_SHADE], 0, 0, HWD0, VWD0, 0, 1);
    if (storage_swap_state == SWAP_LEAVE) {
        if (menu_get_phase() != TEXT_WINDOW_CLOSED) {
            return;
        }
        menu_reset();
        storage_swap_state = SWAP_IDLE;
        farm_unpause();
        game_state.globalFlags |= GLOBAL_FLAG_TRANSITION_END;
        game_state.player.stateFlags |= PLAYER_FLAG_REFRESH_HELD;
        return;
    }
    saved_mode = text_get_palette();
    text_set_palette(3);
    storage_swap_screen();
    text_set_palette(saved_mode);
    player_refresh_carry_state(&game_state.player);
}

/* The spots of the swap screen: each one's position, then where the cursor goes from it.
 * next_spot holds the four directions by three rucksack levels; the second block (the
 * directions offset by SPOT_LINKS_MARKED) is the same for a spot that is marked. */
static FarmSpot storage_spots[18] = {
    {57, 36, {9, 9, 9, 9, 9, 9, 2, 4, 4, 1, 1, 1,
               0, 0, 5, 0, 0, 5, 2, 4, 4, 1, 1, 1}},
    {141, 20, {0x0A, 0x0A, 0x0E, 0x0A, 0x0A, 5, 0, 0, 0, 2, 2, 2,
               1, 1, 5, 1, 1, 5, 0, 0, 0, 2, 2, 2}},
    {175, 20, {0x0B, 0x0B, 0x0F, 0x0B, 0x0B, 6, 1, 1, 1, 0, 3, 3,
               2, 2, 6, 2, 2, 6, 1, 1, 1, 0, 3, 3}},
    {209, 20, {0, 0x0C, 0x10, 0, 0x0C, 7, 0, 2, 2, 0, 4, 4,
               0, 3, 7, 0, 3, 7, 0, 2, 2, 0, 4, 4}},
    {243, 20, {0, 0x0D, 0x11, 0, 0x0D, 8, 0, 3, 3, 0, 0, 0,
               0, 4, 8, 0, 4, 8, 0, 3, 3, 0, 0, 0}},
    {141, 54, {0, 0, 1, 0, 0, 0x0A, 0, 0, 0, 0, 0, 6,
               0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 6}},
    {175, 54, {0, 0, 2, 0, 0, 0x0B, 0, 0, 5, 0, 0, 7,
               0, 0, 2, 0, 0, 2, 0, 0, 5, 0, 0, 7}},
    {209, 54, {0, 0, 3, 0, 0, 0x0C, 0, 0, 6, 0, 0, 8,
               0, 0, 3, 0, 0, 3, 0, 0, 6, 0, 0, 8}},
    {243, 54, {0, 0, 4, 0, 0, 0x0D, 0, 0, 7, 0, 0, 0,
               0, 0, 4, 0, 0, 4, 0, 0, 7, 0, 0, 0}},
    {57, 110, {0, 0, 0, 0, 0, 0, 0x0B, 0x0D, 0x0D, 0x0A, 0x0A, 0x0A,
               9, 9, 0x0E, 9, 9, 0x0E, 0x0B, 0x0D, 0x0D, 0x0A, 0x0A, 0x0A}},
    {141, 94, {1, 1, 5, 1, 1, 0x0E, 9, 9, 9, 0x0B, 0x0B, 0x0B,
               0x0A, 0x0A, 0x0E, 0x0A, 0x0A, 0x0E, 9, 9, 9, 0x0B, 0x0B, 0x0B}},
    {175, 94, {2, 2, 6, 2, 2, 0x0F, 0x0A, 0x0A, 0x0A, 9, 0x0C, 0x0C,
               0x0B, 0x0B, 0x0F, 0x0B, 0x0B, 0x0F, 0x0A, 0x0A, 0x0A, 9, 0x0C, 0x0C}},
    {209, 94, {0, 3, 7, 0, 3, 0x10, 0, 0x0B, 0x0B, 0, 0x0D, 0x0D,
               0, 0x0C, 0x10, 0, 0x0C, 0x10, 0, 0x0B, 0x0B, 0, 0x0D, 0x0D}},
    {243, 94, {0, 4, 8, 0, 4, 0x11, 0, 0x0C, 0x0C, 9, 9, 9,
               0, 0x0D, 0x11, 0, 0x0D, 0x11, 0, 0x0C, 0x0C, 9, 9, 9}},
    {141, 128, {0, 0, 0x0A, 0, 0, 1, 0, 0, 9, 0, 0, 0x0F,
               0, 0, 0x0A, 0, 0, 0x0A, 0, 0, 9, 0, 0, 0x0F}},
    {175, 128, {0, 0, 0x0B, 0, 0, 2, 0, 0, 0x0E, 0, 0, 0x10,
               0, 0, 0x0B, 0, 0, 0x0B, 0, 0, 0x0E, 0, 0, 0x10}},
    {209, 128, {0, 0, 0x0C, 0, 0, 3, 0, 0, 0x0F, 0, 0, 0x11,
               0, 0, 0x0C, 0, 0, 0x0C, 0, 0, 0x0F, 0, 0, 0x11}},
    {243, 128, {0, 0, 0x0D, 0, 0, 4, 0, 0, 0x10, 0, 0, 9,
               0, 0, 0x0D, 0, 0, 0x0D, 0, 0, 0x10, 0, 0, 9}},
};

/* The four frames the spot-swap screen draws (storage_swap_screen). */
static FarmFrameRect storage_swap_frames[4] = {
    {25, 16, 96, 74},
    {121, 16, 174, 74},
    {25, 90, 96, 74},
    {121, 90, 174, 74},
};

/* The spot-swap screen: draw the frames, captions and every spot's marker, then
 * (in state 1) move the cursor along the spot links. Cross picks a spot and swaps it
 * with a second pick, triangle takes the spot (state 2), circle drops the pick and
 * start leaves (state 3). */
static void storage_swap_screen(void) {
    u_long *rows;
    u_long *text;
    s32 i;
    s32 dir;
    s32 refused;

    text = &current_frame->otTags[0][SWAP_LAYER_TEXT];
    rows = &current_frame->otTags[0][SWAP_LAYER_SPOTS];
    text_set_spacing(0, 2);
    for (i = 0; (u32)i < 4; i++) {
        window_frame_draw_at(&current_frame->otTags[0][SWAP_LAYER_FRAMES], 1, storage_swap_frames[i].x, storage_swap_frames[i].y,
                      storage_swap_frames[i].w, storage_swap_frames[i].h);
    }
    text_draw_plain(text, LOCALIZED(text_bank.messages.swap_equip), 29, 20);
    text_draw_plain(text, LOCALIZED(text_bank.messages.swap_rucksack), 29, 94);
    storage_draw_spot(rows, SPOT_HELD_TOOL, &game_state.player.held[HELD_TOOL]);
    storage_draw_spot(rows, SPOT_HELD_ITEM, &game_state.player.held[HELD_ITEM]);
    for (i = 0; i < math_int_pow(2, game_state.player.rucksackLevel + 1); i++) {
        storage_draw_spot(rows, i + SPOT_TOOL_FIRST, &game_state.player.tools[i]);
        storage_draw_spot(rows, i + SPOT_ITEM_FIRST, &game_state.player.items[i]);
    }
    if (storage_swap_marked != SPOT_NONE) {
        dir = SPOT_LINKS_MARKED;
        item_effect_submit(&current_frame->otTags[0][SWAP_LAYER_MARK],
                      storage_spots[storage_swap_marked].x,
                      storage_spots[storage_swap_marked].y);
    } else {
        dir = 0;
    }
    if (storage_swap_state != SWAP_BROWSE) {
        storage_item_message(storage_spot_item(storage_swap_cursor));
        return;
    }
    menu_set_sound_enabled(0);
    text_set_input(0);
    menu_request_open(3, MSG_STORAGE_SWAP_HINT);
    if (menu_phase_is_result() != 0) {
        if (input_pad1->pressed & PAD_TRIANGLE) {
            if (storage_swap_state == SWAP_BROWSE) {
                if (storage_spot_item(storage_swap_cursor)->id != 0) {
                    sound_play(0);
                    menu_begin_close();
                    storage_swap_state = SWAP_TAKE;
                } else {
                    sound_play(2);
                }
            }
        } else if (input_pad1->pressed & PAD_CIRCLE) {
            if (storage_swap_marked == SPOT_NONE) {
                menu_begin_close();
                storage_swap_state = SWAP_LEAVE;
                sound_play(3);
                return;
            }
            sound_play(1);
            storage_swap_marked = SPOT_NONE;
        } else if (input_pad1->pressed & PAD_CROSS) {
            if (storage_swap_marked == SPOT_NONE) {
                sound_play(0);
                storage_swap_marked = storage_swap_cursor;
            } else {
                storage_swap_pick = storage_swap_cursor;
                if (storage_swap_marked == storage_swap_cursor) {
                    storage_swap_cursor = (storage_swap_marked < SPOT_HELD_ITEM) ? SPOT_HELD_TOOL : SPOT_HELD_ITEM;
                }
                refused = storage_swap_spots(storage_swap_marked, storage_swap_cursor);
                storage_swap_cursor = storage_swap_pick;
                if (refused == 0) {
                    sound_play(0);
                    storage_swap_marked = SPOT_NONE;
                } else {
                    sound_play(2);
                }
            }
        } else if (input_pad1->pressed & PAD_START) {
            menu_begin_close();
            sound_play(3);
            storage_swap_state = SWAP_LEAVE;
        } else {
            if (input_pad1->triggered & PAD_ARROWS_MASK) {
                switch (input_pad1->triggered & PAD_ARROWS_MASK) {
                case PAD_UP:
                    break;
                case PAD_DOWN:
                    dir += SPOT_DOWN;
                    break;
                case PAD_LEFT:
                    dir += SPOT_LEFT;
                    break;
                case PAD_RIGHT:
                    dir += SPOT_RIGHT;
                    break;
                }
                storage_swap_cursor = storage_spots[storage_swap_cursor].next_spot[game_state.player.rucksackLevel + dir * 3];
                sound_play(5);
            }
        }
    }
    ui_draw_icon_0b_1(&current_frame->otTags[0][SWAP_LAYER_CURSOR], storage_spots[storage_swap_cursor].x + 12,
                  storage_spots[storage_swap_cursor].y + 24);
}

/* The message of the item at the cursor: a gift shows its contents' text. Closing the
 * message returns to browsing. */
static void storage_item_message(FarmStoredItem *item) {
    s32 message = item->id;
    if (message == ITEM_GIFT) {
        message = stored_item_contents(item) + GIFT_TEXT_BASE;
    }
    menu_request_open(3, message);
    if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
        storage_swap_state = SWAP_BROWSE;
        menu_begin_close();
    }
}

/* Move, stack or swap the items of two spots. Returns 1 when refused, 0 when done. */
static s32 storage_swap_spots(s32 from, s32 to) {
    FarmStoredItem *first;
    FarmStoredItem *second;

    if (from == to) {
        return 1;
    }
    first = storage_spot_item(from);
    second = storage_spot_item(to);
    if (first->id == 0) {
        if (second->id == 0) {
            return 0;
        }
    } else if (second->id == 0) {
        return storage_move_item(first, from, second, to);
    }
    if (first->id == 0) {
        return storage_move_item(second, to, first, from);
    }
    if (first->id == second->id) {
        return storage_stack_items(first, from, second, to);
    }
    return storage_try_swap(first, from, second, to);
}

/* Move `item` into the empty spot `other`. Returns 1 when refused, 0 when moved. */
static s32 storage_move_item(FarmStoredItem *item, s32 spot, FarmStoredItem *other, s32 otherSpot) {
    switch (storage_spot_is_held(otherSpot)) {
    case 0:
        if (item_class_word_negative(item->id) == 0) {
            return 1;
        }
    case 1:
        storage_item_swap(item, other);
        return 0;
    }
    return 1;
}

/* Try a swap of two spots that cannot simply move or stack: refuse (return 1) when
 * the pair is not allowed, otherwise swap the two items (return 0). */
static s32 storage_try_swap(FarmStoredItem *first, s32 firstSpot, FarmStoredItem *second, s32 secondSpot) {
    switch (storage_spot_is_held(secondSpot)) {
    case 0:
        if (item_class_word_negative(first->id) == 0) {
            return 1;
        }
    case 1:
        if (item_category(first->id) == ITEM_CATEGORY_CROP && first->count >= 2) {
            return 1;
        }
    }
    switch (storage_spot_is_held(firstSpot)) {
    case 0:
        if (item_class_word_negative(second->id) == 0) {
            return 1;
        }
    case 1:
        if (item_category(second->id) == ITEM_CATEGORY_CROP && second->count >= 2) {
            return 1;
        }
        storage_item_swap(first, second);
        return 0;
    }
    return 1;
}

/* Stack a seed onto the same kind in `other`. Other kinds are refused (return 1). */
static s32 storage_stack_items(FarmStoredItem *item, s32 spot, FarmStoredItem *other, s32 otherSpot) {
    s32 result;
    if (item_category(item->id) == ITEM_CATEGORY_SEED) {
        storage_stack_count(item, other);
        result = 0;
    } else {
        result = 1;
    }
    return result;
}

static s32 storage_spot_is_held(s32 spot) {
    if (spot == SPOT_HELD_TOOL || spot == SPOT_HELD_ITEM) {
        return 1;
    }
    return 0;
}

/* Add `from`'s count to `to`, capped at STACK_MAX with the excess left in `from`. When it
 * all fits, `from` is handed to picture_cell_place_item. */
static void storage_stack_count(FarmStoredItem *from, FarmStoredItem *to) {
    s32 sum = (u16)to->count + (u16)from->count;
    to->count = sum;
    if ((s16)sum > STACK_MAX) {
        to->count = STACK_MAX;
        from->count = sum - STACK_MAX;
    } else {
        picture_cell_place_item(from);
    }
}

/* Swap two stored items. */
static void storage_item_swap(FarmStoredItem *a, FarmStoredItem *b) {
    FarmStoredItem tmp;
    tmp = *a;
    *a = *b;
    *b = tmp;
}

/* The item at `spot`: the two held slots, then the player's slots in spot order. */
static FarmStoredItem *storage_spot_item(s32 spot) {
    if (spot == SPOT_HELD_TOOL) return &game_state.player.held[0];
    if (spot == SPOT_HELD_ITEM) return &game_state.player.held[1];
    if (spot < SPOT_HELD_ITEM) return &game_state.player.held[1 + spot];
    return &game_state.player.held[spot];
}

/* Draw the marker for spot `index`; with a stored item there, place it through
 * picture_cell_build_prim and draw its count when it stacks. */
static void storage_draw_spot(u_long *ot, s32 index, FarmStoredItem *item) {
    if (item->id == 0) {
        item_empty_marker_draw(ot, storage_spots[index].x, storage_spots[index].y);
        return;
    }
    picture_cell_build_prim(item->id, 0, ot, storage_spots[index].x, storage_spots[index].y, PICTURE_CELL_CENTER);
    if (item_category(item->id) == 2) {
        item_anim_draw(&current_frame->otTags[0][SWAP_LAYER_COUNTS], storage_spots[index].x,
                      storage_spots[index].y, item->count);
    }
}

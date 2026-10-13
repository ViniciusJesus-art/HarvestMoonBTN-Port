/* The house menus: the sleep menu by the bed, and cooking: the cooking menu, the recipe
 * table and the records of what was cooked. */

#include "types.h"
#include "bg_tile.h"
#include "cd_audio.h"
#include "cooking_recipes.h"
#include "estate.h"
#include "event_flags.h"
#include "farm_main.h"
#include "farm_state.h"
#include "farm_tables.h"
#include "house.h"
#include "item_storage.h"
#include "items.h"
#include "main.h"
#include "math_util.h"
#include "menu_window.h"
#include "pad.h"
#include "picture_cells.h"
#include "player.h"
#include "screen_fade.h"
#include "shop_page.h"
#include "sound_request.h"
#include "text.h"
#include "text_bank.h"
#include "ui_icons.h"

/* The id of the item under the cursor. */
#define CURSOR_ID() (farm_stored_item_at(cooking_menu_page, cooking_menu_cursor)->id)
/* A slot value meaning no slot is picked, and the one for the fiasco dish. */
#define COOKING_NO_SLOT 0xFF
#define COOKING_FIASCO_SLOT 0xFE
/* The seasoning bits of the option flags: the five from sugar to miso. */
#define COOKING_SEASONING_MASK 0xF80
/* The ingredient slots of a recipe, the option bits of its header, and the utensil cells. */
#define COOKING_INGREDIENT_SLOTS 8
#define COOKING_OPTION_BITS 12
#define COOKING_UTENSIL_CELLS 7
/* The icon grid: its origin, the cell pitch, and the pages the item picker wraps through. */
#define COOKING_CELL_X 135
#define COOKING_CELL_Y 103
#define COOKING_CELL_PITCH 34
#define COOKING_PAGES 9
/* The recipe records: four groups of twenty, the record menu wraps through the groups. */
#define COOKING_GROUPS 4
#define COOKING_RECORDS_PER_GROUP 20
/* The gift's message ids start here, one per gift item. */
#define COOKING_GIFT_TEXT_BASE 0x3E8

/* The menu messages of the cooking menu, by the prompt each one shows. */
enum CookingMessage {
    COOKING_MSG_FIRST_CHOICE = 0x3B6,
    COOKING_MSG_SECOND_CHOICE = 0x3B7,
    COOKING_MSG_RECORD_LIST = 0x3B8,
    COOKING_MSG_RECORD_VIEW = 0x3B9,
    COOKING_MSG_RECORD_CHECK = 0x3BA,
    COOKING_MSG_RECORD_CONFIRM = 0x3BB,
    COOKING_MSG_GRID = 0x3BC,
    COOKING_MSG_SEASONINGS = 0x3BD,
    COOKING_MSG_ITEM_MENU = 0x3BE,
    COOKING_MSG_COOK_CONFIRM = 0x3BF,
    COOKING_MSG_RESULT_EQUAL = 0x3C0,
    COOKING_MSG_RESULT_REPEAT = 0x3C1,
    COOKING_MSG_RESULT_FIASCO = 0x3C2,
    COOKING_MSG_RESULT_BETTER = 0x3C3,
    COOKING_MSG_RESULT_WORSE = 0x3C4,
};

/* What farm_cue_add_delta did with an ingredient: it was not grouped, it added the
 * group's bonus, or the recipe has no bonus for the group. */
enum CueAddResult
{
    CUE_ADD_NOT_GROUPED = 0,
    CUE_ADD_ADDED = 1,
    CUE_ADD_MISSING = 2,
};

/* A timer record: an item id and how long it has left to run. */
typedef struct FarmTimerRecord
{
    s16 id;
    s16 pad02;
    u16 timer;
} FarmTimerRecord;

static void cooking_menu_first_choice(void);
static void cooking_menu_second_choice(void);
static void cooking_record_list_tick(void);
static void cooking_record_view_tick(void);
static void cooking_record_check_tick(void);
static void cooking_record_confirm_tick(void);
static void cooking_draw_result_group(u8 group);
static u32 cooking_recipe_name_offset(u32 id);
static s32 cooking_record_available(u8 index);
static void cooking_draw_value_grid(u8 index, u8 mode);
static s32 cooking_utensil_owned(u16 utensil);
static s32 cooking_item_stored(u16 id);
static void cooking_load_record(u8 index);
static void cooking_grid_input(void);
static void cooking_utensil_prompt_tick(s8 slot);
static void cooking_row_input(void);
static void cooking_draw_pending_flags(void);
static void cooking_draw_utensils(void);
static s32 cooking_option_flag_set(s8 cell);
static u16 cooking_cell_mask(s8 cell);
static void cooking_item_menu_input(void);
static void cooking_item_prompt_tick(s32 first, s32 second);
static void cooking_cook_confirm_tick(void);
static void cooking_draw_staged_grid(void);
static void cooking_draw_timer_grid(void);
static s32 cooking_item_selected(s8 page, s8 cell);
static void cooking_apply_result(void);
static void cooking_clear_selected_timers(void);
static void cooking_timer_decrement(s32 id);
static void cooking_hold_result(u16 value, s16 modifier);
static u8 cooking_find_record(u16 *id, u16 *value);
static s32 cooking_claim_recipe_tools(Recipe *recipe);
static s32 cooking_claim_entry_slots(Recipe *recipe);
static s32 cooking_sum_wanted_cues(Recipe *recipe, u16 *value);
static s32 cooking_clear_pattern_value(u16 code);
static s32 farm_cue_add_delta(Recipe *recipe, u16 code, s16 *value);
static void cooking_result_equal_tick(void);
static void cooking_result_repeat_tick(void);
static void cooking_result_fiasco_tick(void);
static void cooking_result_better_tick(void);
static void cooking_result_worse_tick(void);
static void cooking_record_store(void);
static s32 options_pick_menu(void);

/* The choices of the sleep menu. Each one's text is in text_bank.messages.sleep_menu. */
enum SleepMenuChoice {
    SLEEP_MENU_SLEEP = 0,
    SLEEP_MENU_CAKE_DIARY_SLEEP = 1,
    SLEEP_MENU_READ_DIARY = 2,
    SLEEP_MENU_EXCHANGE_ANIMALS = 3,
};

static u16 options_four_cursor = SLEEP_MENU_SLEEP;

/* The pick state: 0 draws the menu, 0xFE waits for it to close, 0xFF is idle. */
static s16 options_pick_choice = 255;

/* Put the sleep menu back to its first row and idle. */
void options_pick_reset(void) {
    options_four_cursor = SLEEP_MENU_SLEEP;
    options_pick_choice = 0xFF;
}

/* Reset, then start the sleep menu: the next tick draws it. */
void options_pick_clear(void) {
    options_pick_reset();
    options_pick_choice = 0;
}

/* Run the sleep menu. Once it has returned a row, wait for it to close, then unpause
 * the farm for every result but the animal exchange (4), which changes mode. */
void options_pick_tick(void) {
    s16 state;

    state = options_pick_choice;
    if (state == 0) {
        options_pick_result = options_pick_menu();
        if (options_pick_result != 0) {
            options_pick_choice = 0xFE;
            return;
        }
    } else {
        if (state == 0xFE) {
            if (menu_phase_is_result() != 0) {
                options_pick_choice = 0xFF;
                menu_reset();
                if (options_pick_result == -1 || options_pick_result == 3 || options_pick_result == 2 || options_pick_result == 1) {
                    farm_unpause();
                }
            }
        }
    }
}

/* One frame of the four-entry pick menu: draw it, step the selection on up/down, and on
 * confirm run the chosen entry. */
static s32 options_pick_menu(void) {
    s32 result;
    s32 modeFlags;
    s32 flags;
    u16 mode;
    s16 menuValue;

    screen_fill_quad(0x40000, 0x40000, 0x40000);
    menu_set_sound_enabled(0);
    text_set_input(0);
    menu_request_open(3, 0x1B7);
    window_frame_draw_at(&current_frame->otTags[0][22], 1, 16, 16, 172, 88);
    text_set_spacing(0, 2);
    text_draw_plain(&current_frame->otTags[0][21], LOCALIZED(text_bank.messages.sleep_menu), 18, 18);
    text_set_spacing(0, 0);
    result = menu_phase_is_result();
    if (result != 0) {
        modeFlags = input_pad1->triggered;
        if (modeFlags & PAD_DOWN) {
            sound_play(SOUND_MENU_CURSOR);
            options_four_cursor = options_four_cursor + 1;
        } else if (modeFlags & PAD_UP) {
            sound_play(SOUND_MENU_CURSOR);
            options_four_cursor = options_four_cursor + 3;
        }
        options_four_cursor &= 3;
    }
    if (options_four_cursor == SLEEP_MENU_SLEEP) {
        menuValue = options_four_cursor;
    } else if (options_four_cursor == SLEEP_MENU_CAKE_DIARY_SLEEP) {
        menuValue = options_four_cursor + 1;
    } else {
        menuValue = options_four_cursor + 2;
    }
    ui_draw_icon_0b_1(&current_frame->otTags[0][20], 22, (menuValue * 14) + 24);
    if (result != 0) {
        flags = input_pad1->pressed;
        if (flags & (PAD_START | PAD_CIRCLE)) {
            sound_play(SOUND_MENU_CANCEL);
            menu_begin_close();
            return -1;
        }
        if (flags & PAD_CROSS) {
            sound_play(SOUND_MENU_CONFIRM);
            menu_begin_close();
            mode = options_four_cursor;
            switch (mode) {
            case SLEEP_MENU_CAKE_DIARY_SLEEP:
                event_flag_set(EVENT_FLAG_48, 1);
                /* fallthrough */
            case SLEEP_MENU_SLEEP:
                player_start_result_scene(&game_state.player);
                break;
            case SLEEP_MENU_READ_DIARY:
                farm_area.mode.eventMode = FARM_MODE_LOAD_SCREEN;
                /* fallthrough */
            case SLEEP_MENU_EXCHANGE_ANIMALS:
                if (options_four_cursor == SLEEP_MENU_EXCHANGE_ANIMALS) {
                    farm_area.mode.eventMode = FARM_MODE_LIVESTOCK_TRADE;
                }
                game_state.progress.entrySubStep = 0;
                fade_start_out(10, 0, 2);
                sound_fade_out(10);
                break;
            }
            return options_four_cursor + 1;
        }
    }
    return 0;
}


s32 options_pick_result;
/* The eight picked ingredients of the pending record. */
u16 cooking_record_values[8];

void (*farm_menu_handler)(void) = 0;

void (*farm_menu_handler_last)(void) = 0;

u8 cooking_menu_step = 0x00;

static u8 cooking_menu_substep = 0x00;

static u8 cooking_menu_selection = COOKING_NO_SLOT;

s8 cooking_menu_cursor = 0;

static s8 cooking_menu_group = 0x00;

static s8 cooking_toggle_row = 0;

s8 cooking_menu_page = 0;

static u8 cooking_unused_801273EB = 0x00;

static u8 cooking_record_index = 0x00;

static u16 cooking_option_flags = 0x0000;

u16 cooking_item_chosen = 0;

static u16 cooking_utensil_items[8] = {ITEM_KNIFE, ITEM_FRYING_PAN, ITEM_POT, ITEM_MIXER, ITEM_WHISK, ITEM_ROLLING_PIN, ITEM_OVEN, ITEM_SEASONING_SET};

/* Four farm value codes per pattern. */
static u16 cooking_value_codes[4][4] = {
    {ITEM_NORMAL_EGG, ITEM_GOOD_EGG, ITEM_EXCELLENT_EGG, ITEM_GOLDEN_EGG},
    {ITEM_CHEESE_S, ITEM_CHEESE_M, ITEM_CHEESE_L, ITEM_CHEESE_G},
    {ITEM_MILK_S, ITEM_MILK_M, ITEM_MILK_L, ITEM_MILK_G},
    {ITEM_MAYONNAISE_S, ITEM_MAYONNAISE_M, ITEM_MAYONNAISE_L, ITEM_MAYONNAISE_G},
};

static u16 cooking_unused_80127424 = 0x0000;

/* Reset the farm menu state and clear its eight-entry selection buffer. */
static void cooking_menu_reset(void)
{
    s32 count;
    u16 *base;
    u16 *selection;

    count = 7;
    base = cooking_record_values;
    selection = base + 7;
    farm_menu_handler = 0;
    farm_menu_handler_last = 0;
    cooking_menu_step = 0;
    cooking_menu_substep = 0;
    cooking_menu_selection = COOKING_NO_SLOT;
    cooking_menu_cursor = 0;
    cooking_menu_group = 0;
    cooking_toggle_row = 0;
    cooking_menu_page = 0;
    cooking_unused_801273EB = 0;
    cooking_option_flags = 0;
    do
    {
        *selection = 0;
        count -= 1;
        selection -= 1;
    } while (count >= 0);
    cooking_item_chosen = 0;
    cooking_record_index = 0;
    cooking_unused_80127424 = 0;
}

/* Open the cooking menu at its first question (cook or not). */
void cooking_menu_open_choose(void)
{
    game_state.globalFlags |= GLOBAL_FLAG_TRANSITION_BEGIN;
    cooking_menu_reset();
    item_picture_slots_clear();
    cooking_menu_step = 1;
    cooking_menu_substep = 0;
    farm_menu_handler = cooking_menu_first_choice;
    sound_play(SOUND_MENU_OPEN_CLOSE);
    item_pictures_load_poll(-1);
    menu_set_sound_enabled(0);
    text_set_input(0);
    menu_request_open(3, 0x3B5);
}

/* Open the cooking menu straight at the record list. */
void cooking_menu_open_records(void)
{
    game_state.globalFlags |= GLOBAL_FLAG_TRANSITION_BEGIN;
    cooking_menu_reset();
    item_picture_slots_clear();
    cooking_menu_step = 1;
    cooking_menu_substep = 1;
    farm_menu_handler = cooking_record_list_tick;
    sound_play(SOUND_MENU_OPEN_CLOSE);
    item_pictures_load_poll(-1);
    menu_set_sound_enabled(0);
    text_set_input(0);
    menu_request_open(3, 0x3B5);
}

/* Per-frame driver of the cooking menu: runs the current handler while the menu is open.
 * Step 8 holds the screen until the result track has finished, then fades in. */
void cooking_menu_tick(void)
{
    s32 saved_mode;
    u8 state;
    u_long *display;
    display = &current_frame->otTags[0][30];
    state = cooking_menu_step;
    if (state != 0)
    {
        if (state == 1)
        {
            bg_tile_mode_sprite(display);
            if (menu_phase_is_result() != 0 && item_pictures_load_poll(0) == 0)
            {
                menu_begin_close();
                cooking_menu_step = 3;
            }
        }
        else
        {
            bg_tile_mode_cursor(display);
            if (cooking_menu_step == 8)
            {
                if (cd_audio_get_state() == CD_AUDIO_IDLE)
                {
                    cooking_menu_step = 3;
                    fade_start_in(10, 0, 2);
                }
            }
            else
            {
                if (farm_menu_handler_last != farm_menu_handler)
                {
                    cooking_menu_page = 0;
                    cooking_toggle_row = 0;
                    cooking_menu_group = 0;
                    cooking_menu_cursor = 0;
                }
                saved_mode = text_get_palette();
                text_set_palette(3);
                text_set_spacing(1, 2);
                item_picture_slots_unmark();
                farm_menu_handler_last = farm_menu_handler;
                farm_menu_handler_last();
                text_set_palette(saved_mode);
                text_set_spacing(0, 2);
            }
        }
    }
}

/* Does the recipe table have a recipe with id `key`? */
s32 cooking_table_has_id(s16 key)
{
    u16 *p;
    u16 end;
    if (recipes[0].id != ITEM_FOOD_FIASCO)
    {
        end = ITEM_FOOD_FIASCO;
        p = &recipes[0].id;
        do
        {
            if (*p == key)
                return 1;
            p += 16;
        } while (*p != end);
    }
    return 0;
}

/* Ask whether to cook: yes goes to the second question, no closes. */
static void cooking_menu_first_choice(void)
{
    menu_request_open(3, COOKING_MSG_FIRST_CHOICE);
    if (menu_get_phase() == TEXT_WINDOW_CLOSED)
    {
        if ((s16)menu_choice() == 1)
            farm_menu_handler = cooking_menu_second_choice;
        else
            farm_menu_handler = shop_page_close;
    }
}

/* The second question: choice 1 opens the record list, 2 the cooking grid, anything else closes. */
static void cooking_menu_second_choice(void)
{
    s32 state;
    menu_request_open(3, COOKING_MSG_SECOND_CHOICE);
    state = menu_get_phase();
    if (state == TEXT_WINDOW_CLOSED)
    {
        s32 value = (s16)menu_choice();
        if (value == 1)
            farm_menu_handler = cooking_record_list_tick;
        else if (value == state)
            farm_menu_handler = cooking_grid_input;
        else
            farm_menu_handler = shop_page_close;
    }
}

/* The record list: draw group cooking_menu_group, take the confirm/cancel buttons
 * (each picks the next handler), move the cursor cooking_menu_cursor over the two
 * columns of ten with the d-pad -- left/right past the edge turns the group --
 * then draw the cursor and remember the record it points at. */
static void cooking_record_list_tick(void)
{
    s8 slot;

    menu_set_sound_enabled(0);
    text_set_input(0);
    menu_request_open(3, COOKING_MSG_RECORD_LIST);
    cooking_draw_result_group(cooking_menu_group);
    if (menu_phase_is_result() != 0)
    {
        switch (input_pad1->pressed & (PAD_TRIANGLE | PAD_CIRCLE | PAD_CROSS | PAD_START))
        {
        case PAD_START:
            menu_begin_close();
            farm_menu_handler = shop_page_close;
            return;
        case PAD_CIRCLE:
            menu_begin_close();
            sound_play(SOUND_MENU_CANCEL);
            farm_menu_handler = cooking_menu_second_choice;
            return;
        case PAD_TRIANGLE:
            if (game_state.progress.estate.recipeRecords[cooking_menu_group * COOKING_RECORDS_PER_GROUP + cooking_menu_cursor].id != 0)
            {
                menu_begin_close();
                sound_play(SOUND_MENU_CONFIRM);
                farm_menu_handler = cooking_record_view_tick;
                return;
            }
            sound_play(SOUND_MENU_REFUSE);
            break;
        case PAD_CROSS:
            if (game_state.progress.estate.recipeRecords[cooking_menu_group * COOKING_RECORDS_PER_GROUP + cooking_menu_cursor].id == 0)
            {
                sound_play(SOUND_MENU_REFUSE);
                break;
            }
            if (cooking_record_available(cooking_menu_cursor + cooking_menu_group * COOKING_RECORDS_PER_GROUP) == 0)
            {
                menu_begin_close();
                sound_play(SOUND_MENU_REFUSE);
                farm_menu_handler = cooking_record_check_tick;
            }
            else
            {
                menu_begin_close();
                sound_play(SOUND_MENU_CONFIRM);
                farm_menu_handler = cooking_record_confirm_tick;
            }
            return;
        }

        switch (input_pad1->triggered & PAD_ARROWS_MASK)
        {
        case PAD_UP:
            sound_play(SOUND_MENU_CURSOR);
            cooking_menu_cursor--;
            if (cooking_menu_cursor == -1 || cooking_menu_cursor == 9)
            {
                cooking_menu_cursor += 10;
            }
            break;
        case PAD_DOWN:
            sound_play(SOUND_MENU_CURSOR);
            cooking_menu_cursor++;
            if (cooking_menu_cursor == 10 || cooking_menu_cursor == 20)
            {
                cooking_menu_cursor -= 10;
            }
            break;
        case PAD_LEFT:
            sound_play(SOUND_MENU_CURSOR);
            cooking_menu_cursor -= 10;
            if (cooking_menu_cursor < 0)
            {
                cooking_menu_cursor += 20;
                cooking_menu_group--;
            }
            break;
        case PAD_RIGHT:
            sound_play(SOUND_MENU_CURSOR);
            cooking_menu_cursor += 10;
            if (cooking_menu_cursor >= 20)
            {
                cooking_menu_cursor -= 20;
                cooking_menu_group++;
            }
            break;
        }

        if (cooking_menu_group < 0)
        {
            cooking_menu_group = COOKING_GROUPS - 1;
        }
        else if (cooking_menu_group >= COOKING_GROUPS)
        {
            cooking_menu_group = 0;
        }
    }
    slot = cooking_menu_cursor;
    ui_draw_icon_0b_1(&current_frame->otTags[0][21], (slot / 10) * 144 + 23,
                      (slot - (slot / 10) * 10) * 14 + 24);
    cooking_record_index = cooking_menu_cursor + cooking_menu_group * COOKING_RECORDS_PER_GROUP;
}

/* Show the values of the record under the cursor; the buttons leave or go back to the list. */
static void cooking_record_view_tick(void)
{
    s32 flags;

    menu_set_sound_enabled(0);
    text_set_input(0);
    menu_request_open(3, COOKING_MSG_RECORD_VIEW);
    cooking_draw_value_grid(cooking_record_index, 0);
    if (menu_phase_is_result() != 0)
    {
        flags = input_pad1->pressed;
        if (flags & PAD_START)
        {
            menu_begin_close();
            farm_menu_handler = shop_page_close;
        }
        else if (flags & (PAD_TRIANGLE | PAD_CIRCLE | PAD_CROSS))
        {
            menu_begin_close();
            sound_play(SOUND_MENU_CANCEL);
            farm_menu_handler = cooking_record_list_tick;
        }
    }
}

/* Show the record under the cursor with the entries the player lacks marked. */
static void cooking_record_check_tick(void)
{
    s32 flags;

    menu_set_sound_enabled(0);
    text_set_input(0);
    menu_request_open(3, COOKING_MSG_RECORD_CHECK);
    cooking_draw_value_grid(cooking_record_index, 1);
    if (menu_phase_is_result() != 0)
    {
        flags = input_pad1->pressed;
        if (flags & PAD_START)
        {
            menu_begin_close();
            farm_menu_handler = shop_page_close;
        }
        else if (flags & (PAD_TRIANGLE | PAD_CIRCLE | PAD_CROSS))
        {
            menu_begin_close();
            sound_play(SOUND_MENU_CANCEL);
            farm_menu_handler = cooking_record_list_tick;
        }
    }
}

/* Ask whether to cook the record: yes applies it, no goes back to the grid, choice 3 to the list. */
static void cooking_record_confirm_tick(void)
{
    s32 state;
    s32 status;

    cooking_draw_value_grid(cooking_record_index, 0);
    menu_request_open(3, COOKING_MSG_RECORD_CONFIRM);
    state = menu_get_phase();
    if (state == TEXT_WINDOW_CLOSED)
    {
        status = (s16)menu_choice();
        if (status == 1)
        {
            cooking_menu_selection = cooking_record_index;
            cooking_load_record(cooking_record_index);
            fade_start_out(10, 0, 2);
            farm_menu_handler = cooking_apply_result;
        }
        else if (status == state)
        {
            cooking_load_record(cooking_record_index);
            farm_menu_handler = cooking_grid_input;
        }
        else if (status == 3)
        {
            farm_menu_handler = cooking_record_list_tick;
        }
        else
        {
            farm_menu_handler = shop_page_close;
        }
    }
}

/* Draw the panel for one group of result records: two columns of up to ten
 * localized names, each looked up by the record's id, stopping a column at the
 * first empty record. */
static void cooking_draw_result_group(u8 group)
{
    u_long *ot;
    u_long *layer60;
    u_long *layer70;
    s32 column;
    s32 line;
    s16 x;
    s16 y;
    u16 id;

    ot = current_frame->otTags[0];
    layer60 = &ot[24];
    layer70 = &ot[28];
    window_frame_draw_at(&ot[29], 1, 14, 16, 292, 148);
    ui_draw_icon_0d_phase(layer60, 6, 90);
    ui_draw_icon_0c_phase(layer60, 298, 90);
    text_set_spacing(0, 2);
    x = 28;
    for (column = 0; column < 2; column++)
    {
        y = 21;
        for (line = 0; line < 10; line++)
        {
            id = game_state.progress.estate.recipeRecords[group * COOKING_RECORDS_PER_GROUP + column * 10 + line].id;
            if (id == 0)
            {
                break;
            }
            text_draw_plain(layer70, TEXT_AT_OFFSET(text_bank_starts.messages + cooking_recipe_name_offset(id)), x, y);
            y += 14;
        }
        x += 144;
    }
    text_set_spacing(1, 2);
}

/* The message offset of the recipe's localized name, or the dish_err message if the table has no such id. */
static u32 cooking_recipe_name_offset(u32 id)
{
    Recipe *base = recipes;
    Recipe *p;
    u16 end;
    if (base->id != ITEM_FOOD_FIASCO)
    {
        id &= 0xFFFF;
        end = ITEM_FOOD_FIASCO;
        p = base;
        while (p->id != end)
        {
            if (p->id == id)
                return p->nameOffset;
            p++;
        }
    }
    return offsetof(TextMessages, dish_err);
}

/* Can the record be cooked now: every utensil its header needs is owned and every
 * ingredient is in the rucksack or the icebox. */
static s32 cooking_record_available(u8 index)
{
    FarmRecipe *record;
    s32 mask;
    s32 count;
    u16 *values;

    record = &game_state.progress.estate.recipeRecords[index & 0xFF];
    mask = 1;
    count = 0;
    while (count < COOKING_OPTION_BITS)
    {
        if ((record->options & mask) != 0)
        {
            if (cooking_utensil_owned(mask) == 0)
            {
                return 0;
            }
        }
        mask = (mask << 1) & 0xFFFE;
        count++;
    }
    count = 0;
    values = record->values;
    while (count < COOKING_INGREDIENT_SLOTS)
    {
        if (cooking_item_stored(values[0]) == 0)
        {
            return 0;
        }
        count++;
        values++;
    }
    return 1;
}

/* Draw result record `index` of the farm's records: two frames, their two captions,
 * the seven toggle markers of its options in a grid of rows 0x22 apart (wrapping
 * at x 0xEB), the eighth (id ITEM_SEASONING_SET, the options' seasoning group), then its eight
 * values on the rows below. With `mode` 1, an entry the player does not have
 * yet (cooking_utensil_owned / cooking_item_stored) is marked as well. */
static void cooking_draw_value_grid(u8 index, u8 mode)
{
    u16 x;
    u16 y;
    u16 bit;
    s32 i;
    u_long *ot;
    u_long *head_frame;
    u_long *head_text;
    u_long *head_on;
    u_long *head_off;
    u_long *head_mark;
    FarmRecipe *record;

    ot = current_frame->otTags[0];
    head_frame = &ot[29];
    head_text = &ot[28];
    head_on = &ot[26];
    head_off = &ot[27];
    head_mark = &ot[24];
    record = &game_state.progress.estate.recipeRecords[index];
    window_frame_draw_at(head_frame, 1, 22, 16, 270, 74);
    window_frame_draw_at(head_frame, 1, 22, 90, 270, 74);
    text_draw_plain(head_text, LOCALIZED(text_bank.messages.utensils), 33, 32);
    text_draw_plain(head_text, LOCALIZED(text_bank.messages.ingredient), 33, 106);
    x = 132;
    y = 20;
    bit = 1;
    for (i = 0; i < COOKING_UTENSIL_CELLS; i++)
    {
        if (record->options & bit)
        {
            item_entry_refresh(head_on, cooking_utensil_items[i], x, y);
            if (mode == 1 && cooking_utensil_owned(bit) == 0)
            {
                item_effect_submit(head_mark, x, y);
            }
        }
        else
        {
            item_empty_marker_draw(head_off, x, y);
        }
        x += 34;
        if (x >= 235)
        {
            x = 132;
            y += 34;
        }
        bit <<= 1;
    }
    if (record->options & COOKING_SEASONING_MASK)
    {
        item_entry_refresh(head_on, ITEM_SEASONING_SET, x, y);
        if (mode == 1 && cooking_utensil_owned(bit) == 0)
        {
            item_effect_submit(head_mark, x, y);
        }
    }
    else
    {
        item_empty_marker_draw(head_off, x, y);
    }
    x = 132;
    y = 94;
    for (i = 0; i < COOKING_INGREDIENT_SLOTS; i++)
    {
        item_entry_refresh(head_on, record->values[i], x, y);
        if (mode == 1 && cooking_item_stored(record->values[i]) == 0)
        {
            item_effect_submit(head_mark, x, y);
        }
        x += 34;
        if (x >= 235)
        {
            x = 132;
            y += 34;
        }
    }
}

/* Does the farm own the utensil `utensil` (a seasoning bit asks for the seasoning set)? */
static s32 cooking_utensil_owned(u16 utensil)
{
    if (utensil < SEASONING_FLAG_SUGAR)
    {
        if (game_state.progress.estate.utensils & utensil)
            goto yes;
        return 0;
    }
    else
    {
        if (game_state.progress.estate.utensils & UTENSIL_FLAG_SEASONING_SET)
            goto yes;
        return 0;
    }
yes:
    return 1;
}

/* Is the item `id` in the rucksack (up to 2^(level+1) slots) or in the icebox? 0 always counts as held. */
static s32 cooking_item_stored(u16 id)
{
    s32 slot;
    u8 found;

    found = 0;
    if (id == 0)
    {
        return 1;
    }
    slot = 0;
    while (1)
    {
        if (slot >= math_int_pow(2, game_state.player.rucksackLevel + 1))
        {
            break;
        }
        if (game_state.player.items[slot].id == id)
        {
            found = 1;
            break;
        }
        slot++;
    }
    for (slot = 0; slot < ITEM_STORE_SLOTS; slot++)
    {
        if (game_state.progress.estate.iceboxItems[slot].id == id)
        {
            found++;
            break;
        }
    }
    return found != 0;
}

/* Copy a stored record's header and ingredient values into the pending record. */
static void cooking_load_record(u8 index)
{
    FarmRecipe *r = &game_state.progress.estate.recipeRecords[index];
    s32 i;
    cooking_option_flags = r->options;
    for (i = 0; i < COOKING_INGREDIENT_SLOTS; i++)
    {
        cooking_record_values[i] = r->values[i];
    }
}

/* The toggle grid of the record being edited: draw the frames, captions and
 * markers, hand over to the utensil prompt (state 4) or the seasoning rows (state 5), else
 * move the cursor cooking_menu_cursor over the 4 x 2 grid and take the buttons:
 * PAD_START leaves for cooking_item_menu_input, PAD_CIRCLE clears the toggles (or goes back),
 * PAD_TRIANGLE names an owned utensil (state 4), PAD_CROSS toggles one (the seasoning
 * cell opens the rows, state 5). */
static void cooking_grid_input(void)
{
    u_long *ot;
    u_long *frame;
    u_long *text;
    s32 flags;
    s32 slot;

    ot = current_frame->otTags[0];
    frame = &ot[29];
    text = &ot[28];
    window_frame_draw_at(frame, 1, 22, 16, 270, 74);
    window_frame_draw_at(frame, 1, 22, 90, 270, 74);
    text_draw_plain(text, LOCALIZED(text_bank.messages.use), 33, 32);
    text_draw_plain(text, LOCALIZED(text_bank.messages.utensils), 33, 106);
    cooking_draw_pending_flags();
    cooking_draw_utensils();
    if (cooking_menu_step == 4)
    {
        cooking_utensil_prompt_tick(cooking_menu_cursor);
        return;
    }
    if (cooking_menu_step == 5)
    {
        cooking_row_input();
        return;
    }
    menu_set_sound_enabled(0);
    text_set_input(0);
    menu_request_open(3, COOKING_MSG_GRID);
    if (menu_phase_is_result() != 0)
    {
        switch (input_pad1->triggered)
        {
        case PAD_DOWN:
        case PAD_UP:
            cooking_menu_cursor = (cooking_menu_cursor >= 4) ? cooking_menu_cursor - 4 : cooking_menu_cursor + 4;
            sound_play(SOUND_MENU_CURSOR);
            break;
        case PAD_RIGHT:
            cooking_menu_cursor++;
            if (cooking_menu_cursor == cooking_menu_cursor / 4 * 4)
            {
                cooking_menu_cursor -= 4;
            }
            sound_play(SOUND_MENU_CURSOR);
            break;
        case PAD_LEFT:
            cooking_menu_cursor--;
            if (cooking_menu_cursor == 3 || cooking_menu_cursor == -1)
            {
                cooking_menu_cursor += 4;
            }
            sound_play(SOUND_MENU_CURSOR);
            break;
        }
    }
    slot = cooking_menu_cursor;
    ui_draw_icon_0b_1(&current_frame->otTags[0][21], slot % 4 * COOKING_CELL_PITCH + COOKING_CELL_X, slot / 4 * COOKING_CELL_PITCH + COOKING_CELL_Y);
    if (menu_phase_is_result() == 0)
    {
        return;
    }
    flags = input_pad1->pressed;
    if (flags & PAD_START)
    {
        menu_begin_close();
        sound_play(SOUND_MENU_CONFIRM);
        cooking_item_chosen = 0;
        farm_menu_handler = cooking_item_menu_input;
    }
    else if (flags & PAD_CIRCLE)
    {
        sound_play(SOUND_MENU_CANCEL);
        if (cooking_option_flags != 0)
        {
            cooking_option_flags = 0;
        }
        else if (cooking_menu_substep == 0)
        {
            menu_begin_close();
            farm_menu_handler = cooking_menu_second_choice;
        }
        else
        {
            menu_begin_close();
            farm_menu_handler = cooking_record_list_tick;
        }
    }
    else if (flags & PAD_TRIANGLE)
    {
        if (game_state.progress.estate.utensils & cooking_cell_mask(cooking_menu_cursor))
        {
            menu_begin_close();
            sound_play(SOUND_MENU_CONFIRM);
            cooking_menu_step = 4;
        }
        else
        {
            sound_play(SOUND_MENU_REFUSE);
        }
    }
    else if (flags & PAD_CROSS)
    {
        if (cooking_option_flag_set(cooking_menu_cursor) == 1)
        {
            sound_play(SOUND_MENU_CANCEL);
            if (cooking_menu_cursor < COOKING_UTENSIL_CELLS)
            {
                cooking_option_flags &= ~cooking_cell_mask(cooking_menu_cursor);
            }
            else
            {
                menu_begin_close();
                cooking_menu_step = 5;
            }
        }
        else if (game_state.progress.estate.utensils & cooking_cell_mask(cooking_menu_cursor))
        {
            sound_play(SOUND_MENU_CONFIRM);
            if (cooking_menu_cursor < COOKING_UTENSIL_CELLS)
            {
                cooking_option_flags |= cooking_cell_mask(cooking_menu_cursor);
            }
            else
            {
                menu_begin_close();
                cooking_menu_step = 5;
            }
        }
        else
        {
            sound_play(SOUND_MENU_REFUSE);
        }
    }
}

/* Show the name of utensil `slot`; step 3 when the message closes. */
static void cooking_utensil_prompt_tick(s8 slot)
{
    menu_request_open(3, cooking_utensil_items[slot]);
    if (menu_get_phase() == TEXT_WINDOW_CLOSED)
    {
        cooking_menu_step = 3;
    }
}

/* The five-row seasoning menu over cooking_option_flags's 0xF80 group: up/left and
 * down/right move the cursor (cooking_toggle_row, wrapping mod 5), cross toggles the
 * row's bit (with a sound for on and off), circle closes the menu (state 3).
 * Each row's caption is drawn dimmed while its bit is clear. */
static void cooking_row_input(void)
{
    s32 flags;
    u_long *head;

    menu_set_sound_enabled(0);
    text_set_input(0);
    menu_request_open(3, COOKING_MSG_SEASONINGS);
    window_frame_draw_at(&current_frame->otTags[0][23], 1, 166, 88, 104, 72);
    if (menu_phase_is_result() != 0)
    {
        switch (input_pad1->triggered)
        {
        case PAD_UP:
        case PAD_LEFT:
            cooking_toggle_row += 4;
            sound_play(SOUND_MENU_CURSOR);
            break;
        case PAD_RIGHT:
        case PAD_DOWN:
            cooking_toggle_row += 1;
            sound_play(SOUND_MENU_CURSOR);
            break;
        }
        cooking_toggle_row = cooking_toggle_row - (cooking_toggle_row / 5) * 5;
    }
    ui_draw_icon_0b_1(&current_frame->otTags[0][21], 175, cooking_toggle_row * 14 + 93);
    if (menu_phase_is_result() != 0)
    {
        flags = input_pad1->pressed;
        if (flags & PAD_CIRCLE)
        {
            menu_begin_close();
            sound_play(SOUND_MENU_CANCEL);
            cooking_menu_step = 3;
        }
        else if (flags & PAD_CROSS)
        {
            cooking_option_flags ^= 1 << (cooking_toggle_row + 7);
            if ((cooking_option_flags >> (cooking_toggle_row + 7)) & 1)
            {
                sound_play(SOUND_MENU_CONFIRM);
            }
            else
            {
                sound_play(SOUND_MENU_CANCEL);
            }
        }
    }
    head = &current_frame->otTags[0][22];
    text_draw_semi(head, LOCALIZED(text_bank.messages.seasonings.sugar), 177, 90, ((cooking_option_flags >> 7) ^ 1) & 1);
    text_draw_semi(head, LOCALIZED(text_bank.messages.seasonings.salt), 177, 104, ((cooking_option_flags >> 8) ^ 1) & 1);
    text_draw_semi(head, LOCALIZED(text_bank.messages.seasonings.vinegar), 177, 118, ((cooking_option_flags >> 9) ^ 1) & 1);
    text_draw_semi(head, LOCALIZED(text_bank.messages.seasonings.soy_sauce), 177, 132, ((cooking_option_flags >> 10) ^ 1) & 1);
    text_draw_semi(head, LOCALIZED(text_bank.messages.seasonings.miso_paste), 177, 146, ((cooking_option_flags >> 11) ^ 1) & 1);
}

/* Draw the seven toggle markers in a grid of rows 0x22 apart (wrapping at x 0xEB),
 * then the eighth. Each is "on" when its bit in cooking_option_flags is set. The
 * eighth uses id ITEM_SEASONING_SET and is on when any of the seasoning group is set. */
static void cooking_draw_pending_flags(void)
{
    u16 x;
    u16 y;
    u8 bit;
    s32 i;
    u_long *ot;
    u_long *head_on;
    u_long *head_off;

    x = 132;
    y = 20;
    bit = 1;
    ot = current_frame->otTags[0];
    head_on = &ot[26];
    head_off = &ot[27];
    for (i = 0; i < COOKING_UTENSIL_CELLS; i++)
    {
        if (cooking_option_flags & bit)
        {
            item_entry_refresh(head_on, cooking_utensil_items[i], x, y);
        }
        else
        {
            item_empty_marker_draw(head_off, x, y);
        }
        x += 34;
        if (x >= 235)
        {
            x = 132;
            y += 34;
        }
        bit <<= 1;
    }
    if (cooking_option_flags & COOKING_SEASONING_MASK)
    {
        item_entry_refresh(head_on, ITEM_SEASONING_SET, x, y);
    }
    else
    {
        item_empty_marker_draw(head_off, x, y);
    }
}

/* Draw the seven toggle markers in a grid of rows 0x22 apart (wrapping at x 0xEB),
 * then the eighth. Each is "on" when its bit in the farm's utensils is set, and
 * marked as well when its bit in cooking_option_flags is. */
static void cooking_draw_utensils(void)
{
    u16 x;
    u16 y;
    u8 bit;
    s32 i;
    u_long *ot;
    u_long *head_on;
    u_long *head_off;
    u_long *head_mark;

    x = 132;
    y = 94;
    bit = 1;
    ot = current_frame->otTags[0];
    head_on = &ot[26];
    head_off = &ot[27];
    head_mark = &ot[24];
    for (i = 0; i < COOKING_UTENSIL_CELLS; i++)
    {
        if (game_state.progress.estate.utensils & bit)
        {
            item_entry_refresh(head_on, cooking_utensil_items[i], x, y);
            if (cooking_option_flags & bit)
            {
                item_effect_submit(head_mark, x, y);
            }
        }
        else
        {
            item_empty_marker_draw(head_off, x, y);
        }
        x += 34;
        if (x >= 235)
        {
            x = 132;
            y += 34;
        }
        bit <<= 1;
    }
    if (game_state.progress.estate.utensils & UTENSIL_FLAG_SEASONING_SET)
    {
        item_entry_refresh(head_on, ITEM_SEASONING_SET, x, y);
        if (cooking_option_flags & COOKING_SEASONING_MASK)
        {
            item_effect_submit(head_mark, x, y);
        }
    }
    else
    {
        item_empty_marker_draw(head_off, x, y);
    }
}

/* Is the pending option of `cell` set? Cell 7 (the seasoning set) is set by any of the 0xF80 bits. */
static s32 cooking_option_flag_set(s8 cell)
{
    u16 mask;
    if (cell < COOKING_UTENSIL_CELLS)
    {
        mask = cooking_cell_mask(cell);
        return (cooking_option_flags & mask) != 0;
    }
    return (cooking_option_flags & COOKING_SEASONING_MASK) != 0;
}

static u16 cooking_cell_mask(s8 cell)
{
    return 1 << cell;
}

/* The item picker: a frame, the page title, the 4x2 item grid
 * (cooking_draw_staged_grid) and the eight picked ids (cooking_draw_timer_grid).
 * Steps 6 and 7 hand over to the item prompt and the cook confirm. Otherwise, while
 * menu_phase_is_result reports ready, the d-pad moves the cursor (wrapping
 * across pages, 0..8) and the buttons pick, drop, confirm or leave. */
static void cooking_item_menu_input(void)
{
    u_long *ot;
    u_long *frame;
    u_long *text;
    s32 i;
    s16 first_empty;
    u32 flags;

    ot = current_frame->otTags[0];
    frame = &ot[29];
    text = &ot[28];
    window_frame_draw_at(frame, 1, 22, 16, 270, 74);
    window_frame_draw_at(frame, 1, 22, 90, 270, 74);
    text_draw_plain(text, LOCALIZED(text_bank.messages.ingredient_title), 33, 32);
    if (cooking_menu_page == 0)
    {
        text_draw_plain(text, LOCALIZED(text_bank.messages.ingredient_rucksack), 33, 106);
    }
    else
    {
        text_draw_plain(text, LOCALIZED(text_bank.messages.ingredient_icebox), 33, 106);
        text_draw_number(cooking_menu_page, 1, text, 42, 120);
        text_draw_plain(text, LOCALIZED(text_bank.messages.ingredient_slash), 51, 120);
        text_draw_number(8, 1, text, 60, 120);
    }
    cooking_draw_staged_grid();
    cooking_draw_timer_grid();
    first_empty = COOKING_INGREDIENT_SLOTS;
    for (i = 0; i < COOKING_INGREDIENT_SLOTS; i++)
    {
        if (cooking_record_values[i] == 0)
        {
            first_empty = i;
            break;
        }
    }
    if (cooking_menu_step == 6)
    {
        cooking_item_prompt_tick(cooking_menu_cursor, cooking_menu_page);
        return;
    }
    if (cooking_menu_step == 7)
    {
        cooking_cook_confirm_tick();
        return;
    }
    menu_set_sound_enabled(0);
    text_set_input(0);
    menu_request_open(3, COOKING_MSG_ITEM_MENU);
    if (menu_phase_is_result() != 0)
    {
        switch (input_pad1->triggered)
        {
        case PAD_UP:
        case PAD_DOWN:
            cooking_menu_cursor = (cooking_menu_cursor >= 4) ? cooking_menu_cursor - 4 : cooking_menu_cursor + 4;
            sound_play(SOUND_MENU_CURSOR);
            break;
        case PAD_RIGHT:
            if (cooking_menu_cursor == 3 || cooking_menu_cursor == 7)
            {
                cooking_menu_page++;
            }
            cooking_menu_cursor++;
            if (cooking_menu_cursor == cooking_menu_cursor / 4 * 4)
            {
                cooking_menu_cursor -= 4;
            }
            sound_play(SOUND_MENU_CURSOR);
            break;
        case PAD_LEFT:
            if (cooking_menu_cursor == 0 || cooking_menu_cursor == 4)
            {
                cooking_menu_page--;
            }
            cooking_menu_cursor--;
            if (cooking_menu_cursor == 3 || cooking_menu_cursor == -1)
            {
                cooking_menu_cursor += 4;
            }
            sound_play(SOUND_MENU_CURSOR);
            break;
        }
        if (input_pad1->triggered & (PAD_R2 | PAD_R1))
        {
            cooking_menu_page++;
            sound_play(SOUND_MENU_CURSOR);
        }
        if (input_pad1->triggered & (PAD_L2 | PAD_L1))
        {
            cooking_menu_page--;
            sound_play(SOUND_MENU_CURSOR);
        }
        if (cooking_menu_page < 0)
        {
            cooking_menu_page = COOKING_PAGES - 1;
        }
        if (cooking_menu_page >= COOKING_PAGES)
        {
            cooking_menu_page = 0;
        }
    }
    ui_draw_icon_0b_1(&current_frame->otTags[0][21], (cooking_menu_cursor - cooking_menu_cursor / 4 * 4) * COOKING_CELL_PITCH + COOKING_CELL_X,
                      cooking_menu_cursor / 4 * COOKING_CELL_PITCH + COOKING_CELL_Y);
    if (menu_phase_is_result() == 0)
    {
        return;
    }
    flags = input_pad1->pressed;
    if (flags & PAD_START)
    {
        for (i = 0; i < COOKING_INGREDIENT_SLOTS; i++)
        {
            if (cooking_record_values[i] != 0)
            {
                menu_begin_close();
                sound_play(SOUND_MENU_CONFIRM);
                cooking_menu_step = 7;
                return;
            }
        }
    }
    else if (flags & PAD_TRIANGLE)
    {
        if (CURSOR_ID() != 0)
        {
            menu_begin_close();
            sound_play(SOUND_MENU_CONFIRM);
            cooking_menu_step = 6;
            return;
        }
    }
    else if (flags & PAD_CIRCLE)
    {
        sound_play(SOUND_MENU_CANCEL);
        if (cooking_item_chosen != 0)
        {
            for (i = 0; i < COOKING_INGREDIENT_SLOTS; i++)
            {
                cooking_record_values[i] = 0;
            }
        }
        else
        {
            menu_begin_close();
            farm_menu_handler = cooking_grid_input;
        }
        return;
    }
    else if (flags & PAD_CROSS)
    {
        if (CURSOR_ID() != 0)
        {
            if (cooking_item_selected(cooking_menu_page, cooking_menu_cursor) == 1)
            {
                for (i = 0; i < COOKING_INGREDIENT_SLOTS; i++)
                {
                    if (cooking_record_values[i] == CURSOR_ID())
                    {
                        sound_play(SOUND_MENU_CANCEL);
                        cooking_record_values[i] = 0;
                        return;
                    }
                }
                return;
            }
            if (CURSOR_ID() == 0)
            {
                goto empty;
            }
            if (first_empty < COOKING_INGREDIENT_SLOTS && item_category(CURSOR_ID()) != ITEM_CATEGORY_TOOL)
            {
                sound_play(SOUND_MENU_CONFIRM);
                cooking_record_values[first_empty] = CURSOR_ID();
                return;
            }
        }
    }
    else
    {
        return;
    }
    sound_play(SOUND_MENU_REFUSE);
    return;
empty:
    sound_play(SOUND_MENU_REFUSE);
}

/* Show the item under the cursor (a gift shows its contents' message); step 3 when it closes. */
static void cooking_item_prompt_tick(s32 first, s32 second)
{
    s8 first_index;
    s8 second_index;
    FarmStoredItem *record;
    s16 value;
    s32 state;

    first_index = first;
    second_index = second;
    record = farm_stored_item_at(second_index, first_index);
    value = record->id;
    if (value == ITEM_GIFT)
    {
        menu_request_open(3, stored_item_contents(record) + COOKING_GIFT_TEXT_BASE);
    }
    else
    {
        menu_request_open(3, value);
    }
    state = menu_get_phase();
    if (state == TEXT_WINDOW_CLOSED)
    {
        state = 3;
        cooking_menu_step = state;
    }
}

/* Ask whether to cook the picked items: yes fades to cooking_apply_result, no goes back to step 3. */
static void cooking_cook_confirm_tick(void)
{
    s32 result;

    menu_request_open(3, COOKING_MSG_COOK_CONFIRM);
    if (menu_get_phase() == TEXT_WINDOW_CLOSED)
    {
        result = (s16)menu_choice();
        if (result == 1)
        {
            fade_start_out(10, 0, 2);
            farm_menu_handler = cooking_apply_result;
        }
        else
        {
            cooking_menu_step = 3;
        }
    }
}

/* Draw the eight ids of cooking_record_values in a grid of rows 0x22 apart (wrapping at
 * x 0xEB). An id held in the rucksack (searched up to 2^(rucksack level + 1) slots, as
 * cooking_item_stored does) is drawn with picture_cell_build_prim; an empty or absent one
 * with item_entry_refresh. */
static void cooking_draw_staged_grid(void)
{
    u16 x;
    u16 y;
    s32 i;
    s32 slot;
    u16 id;
    u_long *head;

    x = 132;
    y = 20;
    head = &current_frame->otTags[0][26];
    for (i = 0; i < COOKING_INGREDIENT_SLOTS; i++)
    {
        id = cooking_record_values[i];
        slot = 0;
        while (1)
        {
            if (slot >= math_int_pow(2, game_state.player.rucksackLevel + 1))
            {
                break;
            }
            if (game_state.player.items[slot].id == id)
            {
                break;
            }
            slot++;
        }
        if (slot != math_int_pow(2, game_state.player.rucksackLevel + 1) && id != 0)
        {
            picture_cell_build_prim(id, 0, head, x, y, 8);
        }
        else
        {
            item_entry_refresh(head, id, x, y);
        }
        x += 34;
        if (x >= 235)
        {
            x = 132;
            y += 34;
        }
    }
}

/* Draw the eight result slots of the selected page in a grid of rows 0x22 apart (wrapping at
 * x 0xEB). Page 0 is the rucksack, bounded by 2^(rucksack level + 1); page n is the n-th block
 * of eight in the icebox. Slots past the bound are drawn empty; a running id gets its timer
 * gauge (item_anim_draw) and, when it is also in cooking_record_values, a mark. */
static void cooking_draw_timer_grid(void)
{
    u16 x;
    u16 y;
    s32 i;
    s32 j;
    s32 count;
    s32 status;
    FarmTimerRecord *record;
    u_long *ot;
    u_long *head_mark;
    u_long *head_gauge;
    u_long *head_icon;

    x = 132;
    y = 94;
    ot = current_frame->otTags[0];
    head_mark = &ot[24];
    head_icon = &ot[26];
    head_gauge = &ot[25];
    if (cooking_menu_page == 0)
    {
        record = (FarmTimerRecord *)game_state.player.items;
        count = math_int_pow(2, game_state.player.rucksackLevel + 1);
    }
    else
    {
        record = (FarmTimerRecord *)&game_state.progress.estate.iceboxItems[(cooking_menu_page - 1) * 8];
        count = 8;
    }
    ui_draw_icon_0d_phase(head_mark, 117, 144);
    ui_draw_icon_0c_phase(head_mark, 268, 144);
    for (i = 0; i < COOKING_INGREDIENT_SLOTS; i++, record++, x += 34)
    {
        if (x >= 235)
        {
            x = 132;
            y += 34;
        }
        if (i >= count)
        {
            item_empty_marker_draw(head_icon, x, y);
        }
        else
        {
            if (cooking_menu_page == 0)
            {
                if (record->id == 0)
                {
                    item_empty_marker_draw(head_icon, x, y);
                }
                else
                {
                    picture_cell_build_prim(record->id, 0, head_icon, x, y, 8);
                }
            }
            else
            {
                item_entry_refresh(head_icon, record->id, x, y);
            }
            if (record->id != 0)
            {
                status = item_category(record->id);
                if (cooking_menu_page == 0)
                {
                    if (status == ITEM_CATEGORY_SEED)
                    {
                        item_anim_draw(head_gauge, x, y, (s16)record->timer);
                    }
                }
                else if (status == ITEM_CATEGORY_CROP || status == ITEM_CATEGORY_SEED)
                {
                    item_anim_draw(head_gauge, x, y, (s16)record->timer);
                }
                for (j = 0; j < COOKING_INGREDIENT_SLOTS; j++)
                {
                    if (cooking_record_values[j] == record->id)
                    {
                        break;
                    }
                }
                if (j != COOKING_INGREDIENT_SLOTS && record->id != 0)
                {
                    item_effect_submit(head_mark, x, y);
                }
            }
        }
    }
}

/* Is the item at `page`/`cell` already one of the eight picked ingredients? */
static s32 cooking_item_selected(s8 page, s8 cell)
{
    u16 id = farm_stored_item_at(page, cell)->id;
    s32 i;

    for (i = 0; i < COOKING_INGREDIENT_SLOTS; i++)
    {
        if (cooking_record_values[i] == id)
        {
            break;
        }
    }
    return i != 8;
}

/* The stored item at `cell` of `page`: page 0 is the rucksack, page n the n-th icebox block of eight. */
FarmStoredItem *farm_stored_item_at(s8 a, s8 b)
{
    if (a == 0)
    {
        return &game_state.player.items[b];
    }
    return &game_state.progress.estate.iceboxItems[(a - 1) * 8 + b];
}

/* Once the fade has settled, apply the record the menu selected
 * (cooking_menu_selection), or with nothing selected the one cooking_find_record
 * finds, and choose the next handler by how its value compares with the stored one. */
static void cooking_apply_result(void)
{
    u16 id;
    s16 value;
    s32 slot;
    s32 selected;

    if (fade_is_settled() == 0)
    {
        return;
    }
    slot = cooking_menu_selection;
    selected = slot;
    cooking_menu_cursor = 0;
    cooking_clear_selected_timers();
    if (slot != COOKING_NO_SLOT)
    {
        id = game_state.progress.estate.recipeRecords[slot].id;
        value = game_state.progress.estate.recipeRecords[slot].value;
        cooking_hold_result(id, value);
        farm_menu_handler = cooking_result_equal_tick;
        cd_audio_play_track(CD_TRACK_COOKING_RESULT);
        cooking_menu_step = 8;
        return;
    }
    slot = cooking_find_record(&id, &value);
    cooking_hold_result(id, value);
    cd_audio_play_track(CD_TRACK_COOKING_RESULT);
    cooking_menu_step = 8;
    if (slot == COOKING_FIASCO_SLOT)
    {
        farm_menu_handler = cooking_result_fiasco_tick;
    }
    else if (slot == selected)
    {
        farm_menu_handler = cooking_result_repeat_tick;
    }
    else if (value == game_state.progress.estate.recipeRecords[slot].value)
    {
        farm_menu_handler = cooking_result_equal_tick;
    }
    else if (value < game_state.progress.estate.recipeRecords[slot].value)
    {
        farm_menu_handler = cooking_result_worse_tick;
    }
    else
    {
        farm_menu_handler = cooking_result_better_tick;
    }
}

/* Count down the running timers of the eight picked ingredients. */
static void cooking_clear_selected_timers(void)
{
    u16 *p;
    s32 i;
    p = cooking_record_values;
    i = 7;
    do
    {
        if (*p)
            cooking_timer_decrement(*p);
        p++;
        i--;
    } while (i >= 0);
}

/* Count the timer of item `id` down by one: a rucksack entry is released when it reaches
 * zero (and its picture freed), an icebox entry is cleared. */
static void cooking_timer_decrement(s32 id)
{
    FarmTimerRecord *record;
    s32 slot;
    u16 target_id;
    s32 timer;
    u16 timer_now;
    s32 value;
    slot = 0;
    record = (FarmTimerRecord *)game_state.player.items;
    target_id = id;
    while (1)
    {
        if (slot >= math_int_pow(2, game_state.player.rucksackLevel + 1))
        {
            break;
        }
        if (record->id == target_id)
        {
            slot++;
            timer_now = record->timer;
            timer = timer_now - 1;
            record->timer = timer;
            value = 16;
            if ((timer << value) > 0)
            {
                return;
            }
            record->id = 0;
            record->pad02 = 0;
            picture_release_item(target_id);
            return;
        }
        slot++;
        record++;
    }

    slot = 0;
    record = (FarmTimerRecord *)game_state.progress.estate.iceboxItems;
    do
    {
        if (record->id == target_id)
        {
            slot++;
            timer = record->timer - 1;
            record->timer = timer;
            value = timer << 16;
            if (value > 0)
            {
                return;
            }
            record->id = 0;
            record->pad02 = 0;
            return;
        }
        slot++;
        record++;
    } while (slot < ITEM_STORE_SLOTS);
}

/* Put the cooked result in the held slot and ask the player to refresh the held item. */
static void cooking_hold_result(u16 value, s16 modifier)
{
    u16 x = value;
    s16 y = modifier;
    s32 flags;

    picture_load_to_vram((s16)x);
    game_state.player.held[HELD_ITEM].id = x;
    game_state.player.held[HELD_ITEM].modifier = y;
    game_state.player.held[HELD_ITEM].count = 1;
    flags = game_state.player.stateFlags;
    flags |= PLAYER_FLAG_REFRESH_HELD;
    game_state.player.stateFlags = flags;
}

/* Find the first recipe whose checks pass against the pending header and values
 * (the option flags and the record values, restored before and after every try).
 * Report its id and value; return the farm record slot holding that id,
 * COOKING_NO_SLOT if none does, or COOKING_FIASCO_SLOT (id ITEM_FOOD_FIASCO,
 * value 1) when nothing passed. */
static u8 cooking_find_record(u16 *id, u16 *value)
{
    u16 saved[8];
    u16 found_value;
    u16 header;
    u16 found;
    Recipe *entry;
    s32 i;
    s32 n;
    s32 slot;

    for (i = 0; i < COOKING_INGREDIENT_SLOTS; i++)
    {
        saved[i] = cooking_record_values[i];
    }
    found_value = 0;
    header = cooking_option_flags;
    found = 0;
    for (n = 0; recipes[n].id != ITEM_FOOD_FIASCO; n++)
    {
        for (i = 0; i < COOKING_INGREDIENT_SLOTS; i++)
        {
            cooking_record_values[i] = saved[i];
        }
        cooking_option_flags = header;
        entry = &recipes[n];
        found_value = entry->quality;
        if (cooking_claim_recipe_tools(entry) && cooking_claim_entry_slots(entry) && cooking_sum_wanted_cues(entry, &found_value))
        {
            found = recipes[n].id;
            break;
        }
    }
    for (i = 0; i < COOKING_INGREDIENT_SLOTS; i++)
    {
        cooking_record_values[i] = saved[i];
    }
    cooking_option_flags = header;
    if (found == 0)
    {
        *id = ITEM_FOOD_FIASCO;
        *value = 1;
        return COOKING_FIASCO_SLOT;
    }
    *id = found;
    *value = found_value;
    for (slot = 0; slot < RECIPE_SLOTS; slot++)
    {
        if (game_state.progress.estate.recipeRecords[slot].id == found)
        {
            break;
        }
    }
    if (slot == RECIPE_SLOTS)
    {
        return COOKING_NO_SLOT;
    }
    return slot;
}

/* Use up the utensil bits the recipe needs from cooking_option_flags; 0 when one is missing. */
static s32 cooking_claim_recipe_tools(Recipe *recipe)
{
    s32 i;
    s32 mask;
    s32 value;
    u16 flags;
    i = 0;
    mask = 1;
    flags = recipe->tools;
    do
    {
        if (flags & mask)
        {
            value = cooking_option_flags;
            if (!(value & mask))
                return 0;
            cooking_option_flags = mask ^ value;
        }
        i++;
        mask = (mask << 1) & 0xFFFE;
    } while (i < COOKING_OPTION_BITS);
    return 1;
}

/* Claim a picked value for every non-zero ingredient of the recipe: a pattern code takes any
 * of its four items, any other code its own id. Fail if one cannot be claimed. */
static s32 cooking_claim_entry_slots(Recipe *recipe)
{
    s32 slot;
    s32 i;
    u16 value;
    s32 result;

    for (slot = 0; slot < COOKING_INGREDIENT_SLOTS; slot++)
    {
        value = recipe->ingredients[slot];
        if (value == 0)
        {
            continue;
        }
        result = cooking_clear_pattern_value(value);
        if (result == 1)
        {
            continue;
        }
        if (result > 1)
        {
            if (result == 2)
            {
                return 0;
            }
        }
        for (i = 0; i < COOKING_INGREDIENT_SLOTS; i++)
        {
            if (value == cooking_record_values[i])
            {
                break;
            }
        }
        if (i == COOKING_INGREDIENT_SLOTS)
        {
            return 0;
        }
        cooking_record_values[i] = 0;
    }
    return 1;
}

/* Sum the quality bonuses the recipe holds for the utensil and seasoning bits
 * set in cooking_option_flags and for the eight picked ingredients (farm_cue_add_delta
 * maps the grouped ones), and add the sum to *value. Returns 0, adding nothing,
 * as soon as a wanted entry is missing. */
static s32 cooking_sum_wanted_cues(Recipe *recipe, u16 *value)
{
    s16 total;
    u16 bit;
    u16 target;
    s32 i;
    s32 j;
    u16 code;

    total = 0;
    for (i = 0, bit = 1; i < COOKING_OPTION_BITS; i++, bit <<= 1)
    {
        target = 0;
        if (!(cooking_option_flags & bit))
        {
            continue;
        }
        switch (bit)
        {
        case UTENSIL_FLAG_KNIFE:
            target = ITEM_KNIFE;
            break;
        case UTENSIL_FLAG_FRYING_PAN:
            target = ITEM_FRYING_PAN;
            break;
        case UTENSIL_FLAG_POT:
            target = ITEM_POT;
            break;
        case UTENSIL_FLAG_MIXER:
            target = ITEM_MIXER;
            break;
        case UTENSIL_FLAG_WHISK:
            target = ITEM_WHISK;
            break;
        case UTENSIL_FLAG_ROLLING_PIN:
            target = ITEM_ROLLING_PIN;
            break;
        case UTENSIL_FLAG_OVEN:
            target = ITEM_OVEN;
            break;
        case SEASONING_FLAG_SUGAR:
            target = ITEM_SUGAR;
            break;
        case SEASONING_FLAG_SALT:
            target = ITEM_SALT;
            break;
        case SEASONING_FLAG_VINEGAR:
            target = ITEM_VINEGAR;
            break;
        case SEASONING_FLAG_SOY_SAUCE:
            target = ITEM_SOY_SAUCE;
            break;
        case SEASONING_FLAG_MISO:
            target = ITEM_MISO;
            break;
        }
        for (j = 0; recipe->bonuses[j].id != 0; j++)
        {
            if (recipe->bonuses[j].id == target)
            {
                total += recipe->bonuses[j].points;
                break;
            }
        }
        if (recipe->bonuses[j].id == 0)
        {
            return 0;
        }
    }
    for (i = 0; i < COOKING_INGREDIENT_SLOTS; i++)
    {
        code = cooking_record_values[i];
        if (code == 0)
        {
            continue;
        }
        switch (farm_cue_add_delta(recipe, code, &total))
        {
        case CUE_ADD_ADDED:
            continue;
        case CUE_ADD_MISSING:
            return 0;
        case CUE_ADD_NOT_GROUPED:
        default:
            break;
        }
        for (j = 0; recipe->bonuses[j].id != 0; j++)
        {
            if (recipe->bonuses[j].id == code)
            {
                total += recipe->bonuses[j].points;
                break;
            }
        }
        if (recipe->bonuses[j].id == 0)
        {
            return 0;
        }
    }
    *value += total;
    return 1;
}

/* Look the eight stored farm values up in the four-value pattern selected by
 * `code` (0x181..0x184) and clear the first one found: 1 if one was cleared,
 * 2 if none was, 0 for any other code. */
static s32 cooking_clear_pattern_value(u16 code)
{
    s32 pattern;
    s32 i;
    s32 j;

    switch (code)
    {
    case ITEM_ANY_EGG:
        pattern = 0;
        break;
    case ITEM_ANY_CHEESE:
        pattern = 1;
        break;
    case ITEM_ANY_MILK:
        pattern = 2;
        break;
    case ITEM_ANY_MAYONNAISE:
        pattern = 3;
        break;
    default:
        return 0;
    }

    for (i = 0; i < COOKING_INGREDIENT_SLOTS; i++)
    {
        for (j = 0; j < 4; j++)
        {
            if (cooking_record_values[i] == cooking_value_codes[pattern][j])
            {
                cooking_record_values[i] = 0;
                return 1;
            }
        }
    }
    return 2;
}

/* Map an ingredient `code` to its "any item of this kind" id, find that in the
 * recipe's zero-terminated bonus list and add its points to *value. Returns 0 for a code
 * outside the four groups, 1 when the list has the group and 2 when it ran out. */
static s32 farm_cue_add_delta(Recipe *recipe, u16 code, s16 *value)
{
    s32 target;
    s32 i;

    switch (code)
    {
    case ITEM_GOLDEN_EGG:
    case ITEM_NORMAL_EGG:
    case ITEM_GOOD_EGG:
    case ITEM_EXCELLENT_EGG:
        target = ITEM_ANY_EGG;
        break;
    case ITEM_CHEESE_S:
    case ITEM_CHEESE_M:
    case ITEM_CHEESE_L:
    case ITEM_CHEESE_G:
        target = ITEM_ANY_CHEESE;
        break;
    case ITEM_MILK_S:
    case ITEM_MILK_M:
    case ITEM_MILK_L:
    case ITEM_MILK_G:
        target = ITEM_ANY_MILK;
        break;
    case ITEM_MAYONNAISE_S:
    case ITEM_MAYONNAISE_M:
    case ITEM_MAYONNAISE_L:
    case ITEM_MAYONNAISE_G:
        target = ITEM_ANY_MAYONNAISE;
        break;
    default:
        return CUE_ADD_NOT_GROUPED;
    }

    for (i = 0; recipe->bonuses[i].id != 0; i++)
    {
        if (recipe->bonuses[i].id == target)
        {
            *value += recipe->bonuses[i].points;
            break;
        }
    }
    if (recipe->bonuses[i].id == 0)
    {
        return CUE_ADD_MISSING;
    }
    return CUE_ADD_ADDED;
}

static void cooking_result_equal_tick(void)
{
    if (fade_is_settled())
    {
        menu_request_open(3, COOKING_MSG_RESULT_EQUAL);
        if (menu_get_phase() == TEXT_WINDOW_CLOSED)
            farm_menu_handler = shop_page_close;
    }
}

static void cooking_result_repeat_tick(void)
{
    if (fade_is_settled() != 0)
    {
        menu_request_open(3, COOKING_MSG_RESULT_REPEAT);
        if (menu_get_phase() == TEXT_WINDOW_CLOSED)
        {
            cooking_record_store();
            farm_menu_handler = shop_page_close;
        }
    }
}

static void cooking_result_fiasco_tick(void)
{
    if (fade_is_settled())
    {
        menu_request_open(3, COOKING_MSG_RESULT_FIASCO);
        if (menu_get_phase() == TEXT_WINDOW_CLOSED)
        {
            farm_menu_handler = shop_page_close;
        }
    }
}

static void cooking_result_better_tick(void)
{
    if (fade_is_settled() != 0)
    {
        menu_request_open(3, COOKING_MSG_RESULT_BETTER);
        if (menu_get_phase() == TEXT_WINDOW_CLOSED)
        {
            if ((s16)menu_choice() == 1)
            {
                cooking_record_store();
            }
            farm_menu_handler = shop_page_close;
        }
    }
}

static void cooking_result_worse_tick(void)
{
    if (fade_is_settled())
    {
        menu_request_open(3, COOKING_MSG_RESULT_WORSE);
        if (menu_get_phase() == TEXT_WINDOW_CLOSED)
        {
            if ((s16)menu_choice() == 1)
            {
                cooking_record_store();
            }
            farm_menu_handler = shop_page_close;
        }
    }
}

/* Store the record cooking_find_record describes into its slot (or, if it names
 * none, the first free one), copy the pending header and values into it, and
 * raise the flag the record id asks for. */
static void cooking_record_store(void)
{
    s16 id;
    u16 value;
    s16 slot;
    s32 i;

    slot = cooking_find_record(&id, &value);
    if (slot == COOKING_NO_SLOT)
    {
        for (i = 0; i < RECIPE_SLOTS; i++)
        {
            if (game_state.progress.estate.recipeRecords[i].id == 0)
            {
                slot = i;
                break;
            }
        }
    }
    game_state.progress.estate.recipeRecords[slot].id = id;
    game_state.progress.estate.recipeRecords[slot].value = value;
    game_state.progress.estate.recipeRecords[slot].options = cooking_option_flags;
    for (i = 0; i < COOKING_INGREDIENT_SLOTS; i++)
    {
        game_state.progress.estate.recipeRecords[slot].values[i] = cooking_record_values[i];
    }
    switch (id)
    {
    case ITEM_SASHIMI:
        event_flag_set(EVENT_FLAG_COOKED_SASHIMI, 1);
        break;
    case ITEM_GRILLED_FISH:
        event_flag_set(EVENT_FLAG_COOKED_GRILLED_FISH, 1);
        break;
    case ITEM_SUSHI:
        event_flag_set(EVENT_FLAG_COOKED_SUSHI, 1);
        break;
    }
}

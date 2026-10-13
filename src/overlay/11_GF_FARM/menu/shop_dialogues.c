/* The farm shop dialogues: the barn, livestock, poultry, plot, feed and goods shops,
 * and the menus they open. */

#include <libapi.h>
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "actor.h"
#include "animals.h"
#include "chicken.h"
#include "clock.h"
#include "compiled_out.h"
#include "estate.h"
#include "event_commands.h"
#include "event_flags.h"
#include "event_request.h"
#include "farm_state.h"
#include "greenhouse.h"
#include "item_storage.h"
#include "items.h"
#include "livestock.h"
#include "main.h"
#include "math_util.h"
#include "menu_window.h"
#include "milestones.h"
#include "name_entry.h"
#include "pad.h"
#include "picture_cells.h"
#include "player.h"
#include "prim.h"
#include "screen_fade.h"
#include "shop_dialogues.h"
#include "sound_request.h"
#include "text.h"
#include "text_bank.h"
#include "ui_icons.h"
#include "window_frame.h"

/* One menu entry: store the four fields, then step the four cursors. */
#define ADD_ENTRY(name, code, on, id) \
    {                                 \
        *text = (name);               \
        *entryValue = (code);         \
        *entryFlag = (on);            \
        *label46 = (id);              \
        text++;                       \
        entryValue++;                 \
        entryFlag++;                  \
        label46++;                    \
    }
/* The tail of an entry whose text was stored by the caller. */
#define END_ENTRY(code, on, id) \
    {                           \
        text++;                 \
        *entryValue = (code);   \
        *entryFlag = (on);      \
        *label46 = (id);        \
        entryValue++;           \
        entryFlag++;            \
        label46++;              \
    }
/* The height of the choice panel: a row per entry, and a margin. */
#define SHOP_CHOICE_PANEL_HEIGHT() ((shop_menu_last_entry_index(shop_choice_menu) + 1) * SHOP_ROW_HEIGHT + 20)
#define ANIMAL(index) (game_state.animals.livestock[index])
#define CHICKEN(index) (game_state.animals.chickens[index])
/* A name's text, from the names' offset `base` (the snapshot taken with text_offset()). */
#define SHOP_NAME_AT(base, name) ((u16 *)((base) + NAME_OFFSET(name)))
/* The most feed a player can hold, and the days a sheep waits between shearings. */
#define SHOP_FEED_CAP 1000
#define SHEARING_INTERVAL_DAYS 7
/* A menu request's message pair with nothing to show, and a label that names no item. */
#define SHOP_NO_MESSAGE -1
#define SHOP_NO_LABEL -1
/* A FarmMenu holds ten entries. */
#define SHOP_MENU_ROWS 10
/* The messages shop_menu_message_select picks from. */
#define SHOP_MENU_MESSAGE_COUNT 5
/* The quantity caps: fish food, lumber, and a stock count. */
#define SHOP_FISH_FOOD_MAX 999
#define SHOP_LUMBER_MAX 999
#define SHOP_STOCK_COUNT_MAX 99
/* The directions shop_menu_next_entry steps a menu in. */
#define SHOP_MENU_BACKWARD 0
#define SHOP_MENU_FORWARD 1
/* The height of a row of text in the farm panels. */
#define SHOP_ROW_HEIGHT 14
/* The list shows four rows of SHOP_LIST_ROW_HEIGHT pixels. */
#define SHOP_LIST_ROWS 4
#define SHOP_LIST_ROW_HEIGHT 24
/* The player holds a shown item in carrying pose. */
#define SHOP_CARRY_READY_FLAGS (PLAYER_FLAG_SHOWING_ITEM | PLAYER_FLAG_IDLE_POSE | PLAYER_FLAG_CARRY_ITEM)
/* A slot of the stored items holds at most this many of one item. */
#define SHOP_ITEM_STACK_MAX 99

/* shop_record_list's results besides a record's index: the list was cancelled, or no record was picked. */
enum ShopRecordResult {
    SHOP_RECORD_CANCEL = -2,
    SHOP_RECORD_NONE = -1,
};

typedef struct {
    s32 value;
    u8 pad[0xB0];
} FarmPlotStatus;

typedef struct {
    s16 key;
    s16 value;
} FarmKeyValueEntry;

static void shop_fodder_order_commit(ShopMenuHeader *header);
static void shop_chicken_feed_order_commit(ShopMenuHeader *header);
static s16 shop_plot_find_marked(void);
static s32 shop_plot_check_marked(void);
static u16 shop_condition_message(void);
static s32 shop_medal_affordable(FarmMenu *menu);
static void shop_menu_entry_buy(FarmMenu *menu);
static void shop_draw_text_row(FarmActor *actor, u_long *ot, s16 x, s16 y);
static s16 shop_read_value(s16 *p);
static void shop_draw_livestock_row(s16 raw_count, u_long *ot, s16 raw_x, s16 raw_y);
static s16 shop_livestock_find(s16 kind, s16 filter, s16 wanted);
static s32 ending_offer_find(void);
static void shop_menu_disable_input(void);
static s32 ending_offer_answer(void);
static s32 ending_offer_buy(u32 option);
static void shop_menu_message_select(s32 arg);
static s32 shop_first_clear_flag(void);
static s32 shop_dialogue_finish(void);
static s32 shop_buy_stock_step(void);
static s32 shop_buy_item_step(void);
static s32 shop_grant_rice_ball(void);
static s32 shop_grant_bread(void);
static s32 shop_grant_oil(void);
static s32 shop_grant_flour(void);
static s32 shop_grant_curry_powder(void);
static s32 shop_grant_rucksack_upgrade(void);
static s32 shop_basket_step(void);
static s32 shop_result_request(void);
static s32 shop_result_request_ready(void);
static s32 shop_result_request_alt(void);
static s32 shop_result_request_std(void);
static s32 shop_offer_step(void);
static s32 shop_pay_step(void);
static s32 shop_stock_step(void);
static s32 shop_greenhouse_step(void);
static s32 shop_menu_count_draw(void);
static s32 shop_gold_panel_step(void);
static s32 shop_gold_panel_redraw_step(void);
static s32 shop_gold_choice_step(void);
static s32 shop_panel_submit(void);
static s32 shop_panel_submit_both(void);
static s32 shop_medals_choice_step(void);
static s32 shop_dialogue_0f_start(void);
static s32 shop_fish_food_menu_setup(void);
static s32 shop_dialogue_03_start(void);
static s32 shop_start_default(void);
static s32 shop_seed_menu_setup_a(void);
static s32 shop_seed_menu_setup_b(void);
static s32 shop_seed_menu_setup_c(void);
static s32 shop_seed_menu_setup_d(void);
static s32 shop_wine_menu_setup(void);
static s32 shop_tool_upgrade_menu_setup(void);
static s32 shop_greenhouse_start(void);
static s32 shop_item_carry_step(s32 *value, u32 position, s16 item);
static s32 shop_item_show_step(s32 *value, s32 amount, s16 modifier);
static s32 shop_menu_open_choose(s32 *step, s32 base, FarmMenuRequest request);
static s32 shop_item_menu_step(s32 *step, s32 base, FarmMenuRequest *request);
static s32 shop_item_grant(s16 item, s16 message0, s16 message1, s16 mode);
static void shop_draw_gold_panel(void);
static void shop_draw_medals_panel(void);
static void shop_draw_currency_panel(s32 medals);
static void shop_draw_count_total(ShopMenuHeader block, s16 count, s32 total, s16 x, s16 y);
static void shop_set_gold_unit(u16 *text);
static void shop_set_medals_unit(u16 *text);
static s32 shop_quantity_input(s16 limit, s16 *value);
static s32 shop_can_afford(s32 price);
static void shop_set_step(s32 start, s32 end);
static void shop_set_step_offset(s32 start, s32 end, s32 offset);
static void shop_restore_step(void);
static void shop_draw_short_panel(u_long *ot, s32 x, s32 y, s32 columns);
static void shop_draw_panel(s32 large, u_long *display, s16 left, s16 top, s16 columnCount);
static s32 shop_yes_no_input(s16 *value);
static s32 shop_yes_no_prompt(u_long *ot, s16 x, s16 y);
static void shop_menu_clear(FarmMenu *menu);
static void shop_menu_draw(FarmMenu *menu, s16 x, s16 y);
static s32 shop_menu_input(FarmMenu *menu);
static s16 shop_menu_next_entry(FarmMenu menu, s32 direction);
static s32 shop_menu_last_entry_index(FarmMenu menu);
static s16 shop_menu_selected_value(FarmMenu menu);
static s16 shop_menu_selected_label(FarmMenu menu);
static s16 shop_item_room(s16 item, s32 check);
static s16 shop_item_add(s16 tool, s16 amount, s32 mode);
static void shop_price_label_set(ShopMenuHeader *obj, u16 *name, s16 no_units, s16 amount, s16 count);
static s32 shop_tool_upgrade_state(void);

static s16 shop_list_cursor;
static s16 shop_list_top;
static s16 shop_buy_held_item;
/* The current farm screen id. */
static s32 shop_screen_id;
/* The running dialogue's step, and the step saved for returning to it. */
static s32 shop_step;
static s32 shop_return_step;

/* The paid-service dialogue's steps: the Saturday offer, its answer, the payment of 10 gold and the close. */
enum ShopFeeStep {
    SHOP_FEE_END = 0,
    SHOP_FEE_CHECK_DAY = 1,
    SHOP_FEE_OFFER_WAIT = 2,
    SHOP_FEE_OFFER_ANSWER = 3,
    SHOP_FEE_PAY_WAIT = 4,
    SHOP_FEE_PAY_OFFER = 5,
    SHOP_FEE_PAY_OFFER_END = 6,
    SHOP_FEE_BUY_ANSWER = 7,
    SHOP_FEE_MESSAGE_CLOSED = 8,
};


/* The goods dialogue's steps: the choose menu, a message for when the player cannot pay,
 * the purchase, and the leave with the item handed over. */
enum ShopGoodsStep {
    SHOP_GOODS_START = 0,
    SHOP_GOODS_SETUP = 1,
    SHOP_GOODS_CHOOSE = 2,
    SHOP_GOODS_NO_MONEY_RETURN = 3,
    SHOP_GOODS_PURCHASED = 4,
    SHOP_GOODS_MESSAGE_CLOSED = 5,
    SHOP_GOODS_LEAVE = 6,
};


/* The lumber dialogue's steps: the choose menu, the quantity, the yes-or-no order, and the
 * kind message. */
enum ShopLumberStep {
    SHOP_LUMBER_START = 0,
    SHOP_LUMBER_SETUP = 1,
    SHOP_LUMBER_CHOOSE = 2,
    SHOP_LUMBER_CONFIRM_SETUP = 3,
    SHOP_LUMBER_QUANTITY = 4,
    SHOP_LUMBER_ORDER_ASK = 5,
    SHOP_LUMBER_ORDER_MESSAGE_END = 6,
    SHOP_LUMBER_ORDER_ANSWER = 7,
    SHOP_LUMBER_MESSAGE_CLOSED = 8,
    SHOP_LUMBER_SHOW_MESSAGE = 9,
    SHOP_LUMBER_LEAVE = 10,
};


/* The steps every menu dialogue shares, counted from the dialogue's base step: open its
 * message (if it has one), open the prompt, take the menu or quantity input, and finish. */
enum ShopMenuStep {
    SHOP_MENU_OPEN_MESSAGE,
    SHOP_MENU_MESSAGE_CLOSE,
    SHOP_MENU_MESSAGE_END,
    SHOP_MENU_PROMPT,
    SHOP_MENU_INPUT,
    SHOP_MENU_DONE,
};


/* Shows a gift item, then waits until the player is idle again. */
enum ShopGiftStep {
    SHOP_GIFT_SHOW = 0,
    SHOP_GIFT_WAIT_IDLE = 1,
};

/* The animal kinds for the record lists, as `mode`: chickens, cows and sheep. */
enum ShopLivestockKind {
    SHOP_LIVESTOCK_CHICKEN = 0,
    SHOP_LIVESTOCK_COW = 1,
    SHOP_LIVESTOCK_SHEEP = 2,
};

/* The mode of shop_item_grant: 0 sets the item's flag only, 1 adds a rucksack level, 2 adds
 * the item to the rucksack and sets its flag. */
enum ShopGrantMode {
    SHOP_GRANT_FLAG_ONLY = 0,
    SHOP_GRANT_RUCKSACK = 1,
    SHOP_GRANT_ITEM = 2,
};

/* What shop_menu_input reads from a key: nothing, pick the highlighted row (cross), show
 * its info (triangle), or cancel (circle). */
enum ShopMenuInput {
    SHOP_MENU_INPUT_NONE = 0,
    SHOP_MENU_INPUT_SELECT = 1,
    SHOP_MENU_INPUT_INFO = 2,
    SHOP_MENU_INPUT_CANCEL = 3,
};

/* The kind of shop_menu_open_choose and shop_menu_choose_next: the row was chosen
 * (shop_choice_menu.selected), the dialogue was cancelled, or it is still running.
 * SHOP_MENU_RESULT_UNUSED is never returned; its case labels are dead. */
enum ShopMenuResult {
    SHOP_MENU_RESULT_NONE = 0,
    SHOP_MENU_RESULT_CHOSEN = 1,
    SHOP_MENU_RESULT_UNUSED = 2,
    SHOP_MENU_RESULT_CANCELLED = 3,
};

/* What shop_quantity_input reads from a key: nothing, confirm the count (cross), show the
 * item's info (triangle), or cancel (circle). */
enum ShopQuantityInput {
    SHOP_QUANTITY_NONE = 0,
    SHOP_QUANTITY_CONFIRM = 1,
    SHOP_QUANTITY_INFO = 2,
    SHOP_QUANTITY_CANCEL = 3,
};

/* The answer shop_result_update gives to a yes/no prompt: still waiting, yes (the first
 * row), or no. */
enum ShopAnswer {
    SHOP_ANSWER_PENDING = 0,
    SHOP_ANSWER_YES = 1,
    SHOP_ANSWER_NO = 2,
};

/* The kind of name_entry_step: cancelled, or a name entered. */
enum ShopNameResult {
    SHOP_NAME_CANCELLED = -1,
    SHOP_NAME_ENTERED = 1,
};
/* The panel the farm phase draws each frame: an index into shop_dialogue_tick_handlers. */
enum ShopDialogueTick {
    SHOP_TICK_NONE = 0,
    SHOP_TICK_COUNT = 1,
    SHOP_TICK_GOLD_PANEL = 2,
    SHOP_TICK_GOLD_PANEL_REDRAW = 3,
    SHOP_TICK_GOLD_CHOICE = 4,
    SHOP_TICK_PANEL_SUBMIT = 5,
    SHOP_TICK_PANEL_SUBMIT_BOTH = 6,
    SHOP_TICK_MEDALS_CHOICE = 7,
};
/* Reset to 0 when a screen starts. */
static s32 shop_dialogue_tick_index;
static u8 shop_dialogue_unused_8012D530[8];
static ShopMenuHeader shop_menu_header;
/* The two menus of a farm dialogue: the choice menu and the confirm menu shown beside it. */
static FarmMenu shop_choice_menu;
static FarmMenu shop_confirm_menu;
/* Scratch text for the shop screens. */
static u16 shop_text_buffer[24];
static FarmActor shop_livestock_name_actor;
static FarmActor shop_poultry_name_actor;
/* The paid-service dialogue's step (a ShopFeeStep) and the offer it is on. */
static s32 shop_fee_dialogue_step;
static s32 shop_fee_offer;
/* The goods dialogue's step (a ShopGoodsStep) and the step it returns to after a message. */
static s32 shop_goods_step;
static s32 shop_goods_return_step;
/* The farm menu's message ids. */
static s16 shop_menu_messages[8];
/* The request the shop menu runs. */
static FarmMenuRequest shop_menu_request;
/* The lumber dialogue's step (a ShopLumberStep) and the step it returns to after a message. */
static s32 shop_lumber_step;
static s32 shop_lumber_return_step;
static s32 shop_lumber_slot;
static FarmMenuRequest shop_lumber_request;

/* The current language's offset into the text tables. */
static inline s32 text_offset(void)
{
    return text_bank_starts.names;
}

/* The barn's choose menu rows, in the order shop_fodder_menu_setup adds them. */
enum ShopBarnMenuRow {
    SHOP_BARN_MENU_FODDER = 0,
    SHOP_BARN_MENU_CM_POTION = 1,
    SHOP_BARN_MENU_SM_POTION = 2,
    SHOP_BARN_MENU_COW = 3,
    SHOP_BARN_MENU_SHEEP = 4,
    SHOP_BARN_MENU_SELL_COW = 5,
    SHOP_BARN_MENU_SELL_SHEEP = 6,
    SHOP_BARN_MENU_MEDICINE = 7,
    SHOP_BARN_MENU_BELL = 8,
};

/* Set up the fodder menu. The potion, animal medicine and bell entries have no text
 * until their items are unlocked. */
static s32 shop_fodder_menu_setup(void)
{
    u32 flags;
    FarmMenu *menu;
    u16 **text;
    s16 *entryValue;
    u8 *entryFlag;
    s16 *label46;
    ShopMenuHeader *header;

    milestone_refresh_all();
    header = &shop_menu_header;
    header->w0 = LOCALIZED_NAME(text_bank.names.fodder);
    header->w4 = LOCALIZED_NAME(text_bank.names.units);
    flags = game_state.progress.estate.statusFlags;
    header->ha = item_buy_price(ITEM_FODDER);
    header->h8 = 1;

    menu = &shop_choice_menu;
    shop_menu_clear(menu);
    menu->columns[0] = 0xF;
    menu->columns[1] = 0x15;
    menu->selected = SHOP_BARN_MENU_FODDER;
    menu = &shop_choice_menu;
    text = menu->text;
    entryValue = menu->entryValue;
    entryFlag = menu->entryFlag;
    label46 = menu->label46;
    {
        s32 base = text_offset();

        ADD_ENTRY(SHOP_NAME_AT(base, text_bank.names.fodder), ITEM_FODDER, 0, 0x63);
        if (flags & ESTATE_STATUS_NO_COW_MIRACLE_POTION) {
            *text = SHOP_NAME_AT(base, text_bank.names.cm_potion);
        } else {
            *text = 0;
        }
        END_ENTRY(ITEM_COW_MIRACLE_POTION, 0, 0x65);
    }
    if (flags & ESTATE_STATUS_NO_SHEEP_MIRACLE_POTION) {
        *text = LOCALIZED_NAME(text_bank.names.sm_potion);
    } else {
        *text = 0;
    }
    END_ENTRY(ITEM_SHEEP_MIRACLE_POTION, 0, 0x66);
    {
        s32 base = text_offset();

        ADD_ENTRY(SHOP_NAME_AT(base, text_bank.names.buy_cow), ITEM_YOUNG_COW, 0, 0x67);
        ADD_ENTRY(SHOP_NAME_AT(base, text_bank.names.buy_sheep), ITEM_SHEEP, 0, 0x68);
        ADD_ENTRY(SHOP_NAME_AT(base, text_bank.names.sell_cow), 0, 3, 0x69);
        ADD_ENTRY(SHOP_NAME_AT(base, text_bank.names.sell_sheep), 0, 3, 0x6A);
        if (flags & ESTATE_STATUS_NO_ANIMAL_MEDICINE) {
            *text = SHOP_NAME_AT(base, text_bank.names.animal_medicine);
        } else {
            *text = 0;
        }
        END_ENTRY(ITEM_ANIMAL_MEDICINE, 0, 0x64);
    }
    if (event_item_can_take(ITEM_BELL, 0, 2, 0) != 1) {
        *text = LOCALIZED_NAME(text_bank.names.bell);
    } else {
        *text = 0;
    }
    *entryValue = ITEM_BELL;
    *entryFlag = 0;
    *label46 = 0x281;
    shop_dialogue_tick_index = 0;
    return 1;
}

/* The poultry's choose menu rows, in the order shop_chicken_menu_setup adds them. */
enum ShopPoultryMenuRow {
    SHOP_POULTRY_MENU_FEED = 0,
    SHOP_POULTRY_MENU_BUY_CHICKEN = 1,
    SHOP_POULTRY_MENU_SELL_CHICKEN = 2,
    SHOP_POULTRY_MENU_MEDICINE = 3,
};

/* Set up the chicken menu: the feed and buy and sell entries, then the medicine entry,
 * which has text only while the status flag ESTATE_STATUS_NO_ANIMAL_MEDICINE is set. */
static s32 shop_chicken_menu_setup(void)
{
    u32 flags;
    FarmMenu *menu;
    u16 **text;
    s16 *entryValue;
    u8 *entryFlag;
    s16 *label46;
    s32 base;
    ShopMenuHeader *header;

    milestone_refresh_all();
    header = &shop_menu_header;
    header->w0 = LOCALIZED_NAME(text_bank.names.chicken_feed);
    header->w4 = LOCALIZED_NAME(text_bank.names.units);
    flags = game_state.progress.estate.statusFlags;
    header->ha = item_buy_price(ITEM_CHICKEN_FEED);
    header->h8 = 1;

    menu = &shop_choice_menu;
    shop_menu_clear(menu);
    menu->columns[0] = 0xF;
    menu->columns[1] = 0x15;
    menu->selected = SHOP_POULTRY_MENU_FEED;
    menu = &shop_choice_menu;
    text = menu->text;
    entryValue = menu->entryValue;
    entryFlag = menu->entryFlag;
    label46 = menu->label46;
    base = text_offset();
    ADD_ENTRY(SHOP_NAME_AT(base, text_bank.names.chicken_feed), ITEM_CHICKEN_FEED, 0, 0x47);
    ADD_ENTRY(SHOP_NAME_AT(base, text_bank.names.buy_chicken), ITEM_CHICKEN, 0, 0x4A);
    ADD_ENTRY(SHOP_NAME_AT(base, text_bank.names.sell_chicken), 0, 3, 0x49);
    FARM_DEBUG_CHECK();
    if (flags & ESTATE_STATUS_NO_ANIMAL_MEDICINE) {
        *text = SHOP_NAME_AT(base, text_bank.names.animal_medicine);
    } else {
        *text = 0;
    }
    *entryValue = ITEM_ANIMAL_MEDICINE;
    *entryFlag = 0;
    *label46 = 0x48;
    shop_dialogue_tick_index = 0;
    return 1;
}

static s32 shop_plot_sale_start(void) {
    shop_dialogue_tick_index = 0;
    return 1;
}

/* Set up the medicine menus: two fixed entries in the first, and in the second two
 * fixed entries plus the two XL medicines, whose text shows only once unlocked. */
static s32 shop_medicine_menu_setup(void)
{
    FarmMenu *first;
    FarmMenu *second;
    u16 **text;
    s16 *entryValue;
    u8 *entryFlag;
    s16 *label46;
    u32 unlocked;

    milestone_refresh_all();
    first = &shop_choice_menu;
    unlocked = game_state.progress.estate.statusFlags;
    shop_menu_clear(first);
    first->columns[0] = 0xD;
    first->columns[1] = 0xD;
    first->selected = 0;
    text = first->text;
    entryValue = first->entryValue;
    entryFlag = first->entryFlag;
    label46 = first->label46;
    ADD_ENTRY(LOCALIZED_NAME(text_bank.names.exam), 0, 1, 0xDF);
    ADD_ENTRY(LOCALIZED_NAME(text_bank.names.get_medicine), 0, 1, 0xE0);

    second = &shop_confirm_menu;
    shop_menu_clear(second);
    second->columns[0] = 0xD;
    second->columns[1] = 0x14;
    second->selected = 0;
    text = second->text;
    entryValue = second->entryValue;
    entryFlag = second->entryFlag;
    label46 = second->label46;
    ADD_ENTRY(LOCALIZED_NAME(text_bank.names.bodigizer), ITEM_BODIGIZER, 0, 0xE1);
    ADD_ENTRY(LOCALIZED_NAME(text_bank.names.turbojolt), ITEM_TURBOJOLT, 0, 0xE3);
    if (unlocked & 1) {
        *text = LOCALIZED_NAME(text_bank.names.bodigizer_xl);
    } else {
        *text = 0;
    }
    END_ENTRY(ITEM_BODIGIZER_XL, 0, 0xE2);
    if (unlocked & 2) {

        *text = LOCALIZED_NAME(text_bank.names.turbojolt_xl);
    } else {
        *text = 0;
    }
    *entryValue = ITEM_TURBOJOLT_XL;
    *entryFlag = 0;
    *label46 = 0xE4;
    shop_dialogue_tick_index = 0;
    return 1;
}

/* Set up the goods menu. While eventBits bit 0x10 is set, the cursor starts on the
 * second entry (lumber_goods) and the first is blank; otherwise the first is power_fruit.
 * The other seven entries are fixed goods. */
static s32 shop_goods_menu_setup(void)
{
    FarmMenu *menu;
    u16 **text;
    s16 *entryValue;
    u8 *entryFlag;
    s16 *label46;

    milestone_refresh_all();
    menu = &shop_choice_menu;
    shop_menu_clear(menu);
    menu->columns[0] = 0xC;
    menu->columns[1] = 0x17;

    if (game_state.progress.estate.eventBits & 0x10) {
        menu->selected = 1;
    } else {
        menu->selected = 0;
    }
    menu = &shop_choice_menu;
    text = menu->text;
    entryValue = menu->entryValue;
    entryFlag = menu->entryFlag;
    label46 = menu->label46;

    if (game_state.progress.estate.eventBits & 0x10) {
        *text = 0;
    } else {

        *text = LOCALIZED_NAME(text_bank.names.power_fruit);
    }
    END_ENTRY(ITEM_POWER_BERRY, 2, SHOP_NO_LABEL);

    if (game_state.progress.estate.eventBits & 0x10) {

        *text = LOCALIZED_NAME(text_bank.names.lumber_goods);
    } else {
        *text = 0;
    }
    END_ENTRY(0x185, 2, SHOP_NO_LABEL);
    ADD_ENTRY(LOCALIZED_NAME(text_bank.names.bracelet), ITEM_BRACELET, 2, SHOP_NO_LABEL);
    ADD_ENTRY(LOCALIZED_NAME(text_bank.names.necklace), ITEM_NECKLACE, 2, SHOP_NO_LABEL);
    ADD_ENTRY(LOCALIZED_NAME(text_bank.names.earrings), ITEM_EARRINGS, 2, SHOP_NO_LABEL);
    ADD_ENTRY(LOCALIZED_NAME(text_bank.names.turbojolt_xl), ITEM_TURBOJOLT_XL, 2, SHOP_NO_LABEL);
    ADD_ENTRY(LOCALIZED_NAME(text_bank.names.bodigizer_xl), ITEM_BODIGIZER_XL, 2, SHOP_NO_LABEL);
    ADD_ENTRY(LOCALIZED_NAME(text_bank.names.truffle), ITEM_TRUFFLE, 2, SHOP_NO_LABEL);
    ADD_ENTRY(LOCALIZED_NAME(text_bank.names.mystrile), ITEM_MYSTRILE, 2, SHOP_NO_LABEL);
    shop_dialogue_tick_index = 0;
    return 1;
}

/* The steps the barn and poultry shops share (both sell feed and items, buy animals and
 * name them, and sell an animal back): start, setup, the choose menu (ShopMenuStep from
 * SHOP_ANIMAL_CHOOSE_MESSAGE), the count menu (from SHOP_ANIMAL_COUNT_MESSAGE), the name
 * entry, and the sell prompt, list and confirm. Each shop adds its own tail steps. */
enum ShopAnimalStep {
    SHOP_ANIMAL_START = 0,
    SHOP_ANIMAL_SETUP = 1,
    SHOP_ANIMAL_CHOOSE_MESSAGE = 2,
    SHOP_ANIMAL_CHOOSE_MESSAGE_CLOSE = 3,
    SHOP_ANIMAL_CHOOSE_MESSAGE_END = 4,
    SHOP_ANIMAL_CHOOSE_PROMPT = 5,
    SHOP_ANIMAL_CHOOSE_INPUT = 6,
    SHOP_ANIMAL_CHOOSE_DONE = 7,
    /* Never reached: no menu row leads here. */
    SHOP_ANIMAL_CHOOSE_AFTER_DONE = 8,
    SHOP_ANIMAL_COUNT_ASK = 9,
    SHOP_ANIMAL_COUNT_MESSAGE = 10,
    SHOP_ANIMAL_COUNT_MESSAGE_CLOSE = 11,
    SHOP_ANIMAL_COUNT_MESSAGE_END = 12,
    SHOP_ANIMAL_COUNT_PROMPT = 13,
    SHOP_ANIMAL_COUNT_INPUT = 14,
    SHOP_ANIMAL_COUNT_DONE = 15,
    SHOP_ANIMAL_NAME_START = 16,
    SHOP_ANIMAL_NAME_ENTRY = 17,
    SHOP_ANIMAL_SELL_PROMPT = 18,
    SHOP_ANIMAL_SELL_LIST = 19,
    SHOP_ANIMAL_SELL_CONFIRM = 20,
};

/* The barn's tail: it also sells sheep (21..23) and then returns to the menu. */
enum ShopBarnStep {
    SHOP_BARN_SHEEP_SELL_PROMPT = 21,
    SHOP_BARN_SHEEP_SELL_LIST = 22,
    SHOP_BARN_SHEEP_SELL_CONFIRM = 23,
    SHOP_BARN_REOPEN = 24,
    SHOP_BARN_MESSAGE_CLOSED = 25,
    SHOP_BARN_MESSAGE_END = 26,
    SHOP_BARN_LEAVE = 27,
};

/* The barn-shop dialogue, a state machine on shop_step: a choose menu (fodder, items,
 * animals, or the cow and sheep lists), a count menu for fodder, naming a bought
 * animal, and selling a cow or sheep at a price by its tier. Keeps FarmState.player.stateFlags
 * bit 0x800000 set while it runs; returns 1 when finished. */
static s32 shop_barn_step(void)
{
    FarmMenuRequest request;
    s32 result;
    s32 choice;
    s32 status;
    s32 slot;
    FarmActor *object;
    s32 i;

    result = 0;
    request.first[0] = 2;
    request.first[1] = 0x62;
    request.second[0] = 2;
    request.second[1] = 0x6B;
    request.third[0] = 2;
    request.third[1] = 0x6E;
    request.count = 4;
    game_state.player.stateFlags |= PLAYER_FLAG_SCRIPTED;
    request.menu = &shop_choice_menu;
    game_state.player.scriptHeld = 0;
    game_state.player.scriptPressed = 0;
    switch (shop_step) {
    case SHOP_ANIMAL_START:
        shop_set_step(SHOP_ANIMAL_CHOOSE_MESSAGE, SHOP_ANIMAL_START);
        break;
    case SHOP_ANIMAL_SETUP:
        shop_fodder_menu_setup();
        shop_dialogue_tick_index = SHOP_TICK_GOLD_CHOICE;
        request.first[0] = SHOP_NO_MESSAGE;
        shop_set_step(SHOP_ANIMAL_CHOOSE_MESSAGE, SHOP_ANIMAL_START);
        /* fall through */
    case SHOP_ANIMAL_CHOOSE_MESSAGE:
    case SHOP_ANIMAL_CHOOSE_MESSAGE_CLOSE:
    case SHOP_ANIMAL_CHOOSE_MESSAGE_END:
    case SHOP_ANIMAL_CHOOSE_PROMPT:
    case SHOP_ANIMAL_CHOOSE_INPUT:
    case SHOP_ANIMAL_CHOOSE_DONE:
    case SHOP_ANIMAL_CHOOSE_AFTER_DONE:
        switch (shop_menu_open_choose(&shop_step, SHOP_ANIMAL_CHOOSE_MESSAGE, request)) {
        case SHOP_MENU_RESULT_NONE:
            break;
        case SHOP_MENU_RESULT_CANCELLED:
            shop_set_step(SHOP_BARN_LEAVE, SHOP_ANIMAL_START);
            break;
        case SHOP_MENU_RESULT_CHOSEN:
            switch (shop_choice_menu.selected) {
            case SHOP_BARN_MENU_FODDER:
                if (game_state.progress.estate.fodder >= SHOP_FEED_CAP) {
                    menu_request_open(2, 0x6D);
                    shop_set_step(SHOP_BARN_MESSAGE_CLOSED, SHOP_ANIMAL_SETUP);
                } else {
                    menu_request_open(2, 0x6C);
                    shop_set_step(SHOP_BARN_MESSAGE_CLOSED, SHOP_ANIMAL_COUNT_ASK);
                }
                break;
            case SHOP_BARN_MENU_CM_POTION:
            case SHOP_BARN_MENU_SM_POTION:
            case SHOP_BARN_MENU_MEDICINE:
            case SHOP_BARN_MENU_BELL:
                if (shop_can_afford(item_buy_price(shop_menu_selected_value(shop_choice_menu))) == 1) {
                    if (shop_item_room(shop_menu_selected_value(shop_choice_menu), 1) == 0) {
                        menu_request_open(2, 0x54);
                        shop_set_step(SHOP_BARN_MESSAGE_CLOSED, SHOP_BARN_REOPEN);
                    } else {
                        shop_item_add(shop_menu_selected_value(shop_choice_menu), 1, 1);
                        estate_gold_change(-item_buy_price(shop_menu_selected_value(shop_choice_menu)), 1);
                        menu_request_open(2, 0x84);
                        shop_set_step(SHOP_BARN_MESSAGE_CLOSED, SHOP_BARN_REOPEN);
                    }
                } else {
                    menu_request_open(2, 0x4F);
                    shop_set_step(SHOP_BARN_MESSAGE_CLOSED, SHOP_BARN_REOPEN);
                }
                break;
            case SHOP_BARN_MENU_COW:
            case SHOP_BARN_MENU_SHEEP:
                if (livestock_find_free() == 0) {
                    menu_request_open(2, 0x72);
                    shop_set_step(SHOP_BARN_MESSAGE_CLOSED, SHOP_BARN_REOPEN);
                } else if (shop_can_afford(item_buy_price(shop_menu_selected_value(shop_choice_menu))) != 1) {
                    menu_request_open(2, 0x4F);
                    shop_set_step(SHOP_BARN_MESSAGE_CLOSED, SHOP_BARN_REOPEN);
                } else {
                    menu_request_open(2, 0x73);
                    shop_set_step(SHOP_BARN_MESSAGE_CLOSED, SHOP_ANIMAL_NAME_START);
                }
                break;
            case SHOP_BARN_MENU_SELL_COW:
                if (shop_livestock_find(SHOP_LIVESTOCK_COW, 0, -1) == 0) {
                    menu_request_open(2, 0x76);
                    shop_set_step(SHOP_BARN_MESSAGE_CLOSED, SHOP_BARN_REOPEN);
                } else {
                    menu_request_open(2, 0x75);
                    shop_dialogue_tick_index = 0;
                    shop_record_list(SHOP_LIVESTOCK_CHICKEN, 0, -1);
                    shop_set_step(SHOP_BARN_MESSAGE_CLOSED, SHOP_ANIMAL_SELL_PROMPT);
                }
                break;
            case SHOP_BARN_MENU_SELL_SHEEP:
                if (shop_livestock_find(SHOP_LIVESTOCK_SHEEP, 0, -1) == 0) {
                    menu_request_open(2, 0x78);
                    shop_set_step(SHOP_BARN_MESSAGE_CLOSED, SHOP_BARN_REOPEN);
                } else {
                    menu_request_open(2, 0x77);
                    shop_dialogue_tick_index = 0;
                    shop_record_list(SHOP_LIVESTOCK_CHICKEN, 0, -1);
                    shop_set_step(SHOP_BARN_MESSAGE_CLOSED, SHOP_BARN_SHEEP_SELL_PROMPT);
                }
                break;
            }
            break;
        case SHOP_MENU_RESULT_UNUSED:
            break;
        }
        break;
    case SHOP_ANIMAL_COUNT_ASK:
        menu_set_sound_enabled(0);
        text_set_input(0);
        menu_request_open(2, 0x4D);
        shop_set_step(SHOP_BARN_MESSAGE_END, SHOP_ANIMAL_COUNT_MESSAGE);
        break;
    case SHOP_ANIMAL_COUNT_MESSAGE:
    case SHOP_ANIMAL_COUNT_MESSAGE_CLOSE:
    case SHOP_ANIMAL_COUNT_MESSAGE_END:
    case SHOP_ANIMAL_COUNT_PROMPT:
    case SHOP_ANIMAL_COUNT_INPUT:
    case SHOP_ANIMAL_COUNT_DONE:
        choice = shop_menu_choose_next(&shop_step, SHOP_ANIMAL_COUNT_MESSAGE, SHOP_TICK_PANEL_SUBMIT, request, &shop_menu_header);
        switch (choice) {
        case SHOP_MENU_RESULT_NONE:
            break;
        case SHOP_MENU_RESULT_CANCELLED:
            shop_set_step(SHOP_BARN_MESSAGE_CLOSED, SHOP_BARN_REOPEN);
            break;
        case SHOP_MENU_RESULT_CHOSEN:
            menu_begin_close();
            if (shop_can_afford(shop_menu_header.h8 * shop_menu_header.ha) == choice) {
                if (shop_menu_header.h8 + game_state.progress.estate.fodder >= SHOP_FEED_CAP) {
                    menu_request_open(2, 0x6D);
                    shop_set_step(SHOP_BARN_MESSAGE_CLOSED, SHOP_ANIMAL_COUNT_ASK);
                } else {
                    shop_fodder_order_commit(&shop_menu_header);
                    menu_request_open(2, 0x70);
                    shop_set_step(SHOP_BARN_MESSAGE_CLOSED, SHOP_BARN_REOPEN);
                }
            } else {
                menu_request_open(2, 0x4F);
                shop_set_step(SHOP_BARN_MESSAGE_CLOSED, SHOP_ANIMAL_COUNT_ASK);
            }
            break;
        case SHOP_MENU_RESULT_UNUSED:
            break;
        }
        break;
    case SHOP_ANIMAL_NAME_START:
        name_entry_reset();
        name_entry_start_cancellable(shop_livestock_name_actor.name);
        shop_dialogue_tick_index = 0;
        shop_set_step(SHOP_ANIMAL_NAME_ENTRY, SHOP_ANIMAL_START);
        break;
    case SHOP_ANIMAL_NAME_ENTRY:
        status = name_entry_step();
        switch (status) {
        case SHOP_NAME_CANCELLED:
            menu_request_open(2, 0x6E);
            shop_set_step(SHOP_BARN_MESSAGE_CLOSED, SHOP_BARN_REOPEN);
            break;
        case SHOP_NAME_ENTERED:
            object = farm_action_create(shop_menu_selected_value(shop_choice_menu), -1);
            for (i = 0; i < NAME_LENGTH; i++) {
                object->name[i] = shop_livestock_name_actor.name[i];
            }
            estate_gold_change(-item_buy_price(shop_menu_selected_value(shop_choice_menu)), 1);
            menu_request_open(2, 0x74);
            shop_set_step(SHOP_BARN_MESSAGE_CLOSED, SHOP_BARN_REOPEN);
            break;
        }
        break;
    case SHOP_ANIMAL_SELL_PROMPT:
        menu_set_sound_enabled(0);
        text_set_input(0);
        menu_request_open(2, 0x5A);
        shop_set_step(SHOP_BARN_MESSAGE_END, SHOP_ANIMAL_SELL_LIST);
        break;
    case SHOP_ANIMAL_SELL_LIST:
        slot = shop_record_list(SHOP_LIVESTOCK_COW, 0, 0);
        if (slot == SHOP_RECORD_CANCEL) {
            menu_begin_close();
            menu_request_open(2, 0x79);
            shop_set_step(SHOP_BARN_MESSAGE_CLOSED, SHOP_BARN_REOPEN);
        } else if (slot != SHOP_RECORD_NONE) {
            menu_begin_close();
            menu_set_sound_enabled(0);
            if (ANIMAL(slot).actor.flags & ACTOR_FLAG_PRIZE_WINNER) {
                menu_request_open(2, 0x82);
            } else if (ANIMAL(slot).actor.affection < 101) {
                menu_request_open(2, 0x7D);
            } else if (ANIMAL(slot).actor.affection < 201) {
                menu_request_open(2, 0x7F);
            } else {
                menu_request_open(2, 0x80);
            }
            shop_set_step(SHOP_BARN_MESSAGE_END, SHOP_ANIMAL_SELL_CONFIRM);
        }
        break;
    case SHOP_ANIMAL_SELL_CONFIRM:
        slot = shop_record_list(SHOP_LIVESTOCK_COW, 0, 1);
        switch (shop_result_update(&current_frame->otTags[0][11], 200, 128)) {
        case SHOP_ANSWER_PENDING:
            break;
        case SHOP_ANSWER_YES:
            menu_begin_close();
            menu_request_open(2, 0x83);
            if (ANIMAL(slot).actor.flags & ACTOR_FLAG_PRIZE_WINNER) {
                estate_gold_change(8000, 1);
            } else if (ANIMAL(slot).actor.affection < 101) {
                estate_gold_change(3000, 1);
            } else if (ANIMAL(slot).actor.affection < 201) {
                estate_gold_change(4500, 1);
            } else {
                estate_gold_change(5000, 1);
            }
            livestock_clear_record(&ANIMAL(slot));
            shop_set_step(SHOP_BARN_MESSAGE_CLOSED, SHOP_BARN_REOPEN);
            break;
        case SHOP_ANSWER_NO:
            menu_begin_close();
            menu_request_open(2, 0x79);
            shop_set_step(SHOP_BARN_MESSAGE_CLOSED, SHOP_BARN_REOPEN);
            break;
        }
        break;
    case SHOP_BARN_SHEEP_SELL_PROMPT:
        menu_set_sound_enabled(0);
        text_set_input(0);
        menu_request_open(2, 0x5A);
        shop_set_step(SHOP_BARN_MESSAGE_END, SHOP_BARN_SHEEP_SELL_LIST);
        break;
    case SHOP_BARN_SHEEP_SELL_LIST:
        slot = shop_record_list(SHOP_LIVESTOCK_SHEEP, 0, 0);
        if (slot == SHOP_RECORD_CANCEL) {
            menu_begin_close();
            menu_request_open(2, 0x79);
            shop_set_step(SHOP_BARN_MESSAGE_CLOSED, SHOP_BARN_REOPEN);
        } else if (slot != SHOP_RECORD_NONE) {
            menu_begin_close();
            menu_set_sound_enabled(0);
            if (ANIMAL(slot).actor.flags & ACTOR_FLAG_PRIZE_WINNER) {
                menu_request_open(2, 0x81);
            } else if (ANIMAL(slot).actor.affection < 101) {
                menu_request_open(2, 0x7C);
            } else if (ANIMAL(slot).actor.affection < 201) {
                menu_request_open(2, 0x7D);
            } else {
                menu_request_open(2, 0x7E);
            }
            shop_set_step(SHOP_BARN_MESSAGE_END, SHOP_BARN_SHEEP_SELL_CONFIRM);
        }
        break;
    case SHOP_BARN_SHEEP_SELL_CONFIRM:
        slot = shop_record_list(SHOP_LIVESTOCK_SHEEP, 0, 1);
        switch (shop_result_update(&current_frame->otTags[0][11], 200, 128)) {
        case SHOP_ANSWER_PENDING:
            break;
        case SHOP_ANSWER_YES:
            menu_begin_close();
            menu_request_open(2, 0x83);
            if (ANIMAL(slot).actor.flags & ACTOR_FLAG_PRIZE_WINNER) {
                estate_gold_change(6000, 1);
            } else if (ANIMAL(slot).actor.affection < 101) {
                estate_gold_change(2000, 1);
            } else if (ANIMAL(slot).actor.affection < 201) {
                estate_gold_change(3000, 1);
            } else {
                estate_gold_change(3500, 1);
            }
            livestock_clear_record(&ANIMAL(slot));
            shop_set_step(SHOP_BARN_MESSAGE_CLOSED, SHOP_BARN_REOPEN);
            break;
        case SHOP_ANSWER_NO:
            menu_begin_close();
            menu_request_open(2, 0x79);
            shop_set_step(SHOP_BARN_MESSAGE_CLOSED, SHOP_BARN_REOPEN);
            break;
        }
        break;
    case SHOP_BARN_REOPEN:
        menu_request_open(2, 0x71);
        shop_set_step(SHOP_BARN_MESSAGE_CLOSED, SHOP_ANIMAL_SETUP);
        break;
    case SHOP_BARN_MESSAGE_CLOSED:
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            shop_restore_step();
        }
        break;
    case SHOP_BARN_MESSAGE_END:
        if (shop_return_step == SHOP_ANIMAL_SELL_CONFIRM) {
            shop_record_list(SHOP_LIVESTOCK_COW, 0, 1);
        }
        if (shop_return_step == SHOP_BARN_SHEEP_SELL_CONFIRM) {
            shop_record_list(SHOP_LIVESTOCK_SHEEP, 0, 1);
        }
        if (menu_get_phase() == TEXT_WINDOW_END_WAIT) {
            text_set_input(0);
            shop_restore_step();
        }
        break;
    case SHOP_BARN_LEAVE:
        game_state.player.stateFlags &= ~PLAYER_FLAG_SCRIPTED;
        result = 1;
        break;
    }
    return result;
}

static void shop_fodder_order_commit(ShopMenuHeader *header) {
    estate_gold_change(-(header->ha * header->h8), 1);
    estate_fodder_add(2, header->h8);
}

/* The poultry's tail: it has no sheep, so the reopen comes straight after the sell. */
enum ShopPoultryStep {
    SHOP_POULTRY_REOPEN = 21,
    SHOP_POULTRY_MESSAGE_CLOSED = 22,
    SHOP_POULTRY_MESSAGE_END = 23,
    SHOP_POULTRY_LEAVE = 24,
};

/* The poultry-shop dialogue, a state machine on shop_step: a choose menu (feed, a
 * chicken, the chicken list, or an item), a count menu for feed, naming a bought
 * chicken, and selling one for 2000 or 500. Keeps FarmState.player.stateFlags bit
 * PLAYER_FLAG_SCRIPTED set while it runs; returns 1 when finished. */
static s32 shop_poultry_step(void)
{
    FarmMenuRequest request;
    s32 result;
    s32 choice;
    s32 status;
    s32 slot;
    FarmActor *object;
    s32 i;

    result = 0;
    request.first[0] = 2;
    request.first[1] = 0x44;
    request.second[0] = 2;
    request.second[1] = 0x45;
    request.third[0] = 2;
    request.third[1] = 0x4E;
    request.count = 4;
    game_state.player.stateFlags |= PLAYER_FLAG_SCRIPTED;
    request.menu = &shop_choice_menu;
    game_state.player.scriptHeld = 0;
    game_state.player.scriptPressed = 0;
    switch (shop_step) {
    case SHOP_ANIMAL_START:
        shop_set_step(SHOP_ANIMAL_CHOOSE_MESSAGE, SHOP_ANIMAL_START);
        break;
    case SHOP_ANIMAL_SETUP:
        shop_chicken_menu_setup();
        shop_dialogue_tick_index = SHOP_TICK_GOLD_CHOICE;
        request.first[0] = SHOP_NO_MESSAGE;
        shop_set_step(SHOP_ANIMAL_CHOOSE_MESSAGE, SHOP_ANIMAL_START);
        /* fall through */
    case SHOP_ANIMAL_CHOOSE_MESSAGE:
    case SHOP_ANIMAL_CHOOSE_MESSAGE_CLOSE:
    case SHOP_ANIMAL_CHOOSE_MESSAGE_END:
    case SHOP_ANIMAL_CHOOSE_PROMPT:
    case SHOP_ANIMAL_CHOOSE_INPUT:
    case SHOP_ANIMAL_CHOOSE_DONE:
    case SHOP_ANIMAL_CHOOSE_AFTER_DONE:
        switch (shop_menu_open_choose(&shop_step, SHOP_ANIMAL_CHOOSE_MESSAGE, request)) {
        case SHOP_MENU_RESULT_NONE:
            break;
        case SHOP_MENU_RESULT_CANCELLED:
            shop_set_step(SHOP_POULTRY_LEAVE, SHOP_ANIMAL_START);
            break;
        case SHOP_MENU_RESULT_CHOSEN:
            switch (shop_choice_menu.selected) {
            case SHOP_POULTRY_MENU_FEED:
                if (game_state.progress.estate.chickenFeed >= SHOP_FEED_CAP) {
                    menu_request_open(2, 0x4C);
                    shop_set_step(SHOP_POULTRY_MESSAGE_CLOSED, SHOP_ANIMAL_SETUP);
                } else {
                    menu_request_open(2, 0x4B);
                    shop_set_step(SHOP_POULTRY_MESSAGE_CLOSED, SHOP_ANIMAL_COUNT_ASK);
                }
                break;
            case SHOP_POULTRY_MENU_BUY_CHICKEN:
                if (farm_chicken_find_free() == 0) {
                    menu_request_open(2, 0x55);
                    shop_set_step(SHOP_POULTRY_MESSAGE_CLOSED, SHOP_POULTRY_REOPEN);
                } else if (shop_can_afford(item_buy_price(shop_menu_selected_value(shop_choice_menu))) != 1) {
                    menu_request_open(2, 0x4F);
                    shop_set_step(SHOP_POULTRY_MESSAGE_CLOSED, SHOP_POULTRY_REOPEN);
                } else {
                    menu_request_open(2, 0x56);
                    shop_set_step(SHOP_POULTRY_MESSAGE_CLOSED, SHOP_ANIMAL_NAME_START);
                }
                break;
            case SHOP_POULTRY_MENU_SELL_CHICKEN:
                if (shop_livestock_find(SHOP_LIVESTOCK_CHICKEN, 0, -1) == 0) {
                    menu_request_open(2, 0x59);
                    shop_set_step(SHOP_POULTRY_MESSAGE_CLOSED, SHOP_POULTRY_REOPEN);
                } else {
                    menu_request_open(2, 0x58);
                    shop_dialogue_tick_index = 0;
                    shop_record_list(SHOP_LIVESTOCK_CHICKEN, 0, -1);
                    shop_set_step(SHOP_POULTRY_MESSAGE_CLOSED, SHOP_ANIMAL_SELL_PROMPT);
                }
                break;
            case SHOP_POULTRY_MENU_MEDICINE:
                if (shop_can_afford(item_buy_price(shop_menu_selected_value(shop_choice_menu))) == 1) {
                    if (shop_item_room(shop_menu_selected_value(shop_choice_menu), 1) == 0) {
                        menu_request_open(2, 0x54);
                        shop_set_step(SHOP_POULTRY_MESSAGE_CLOSED, SHOP_POULTRY_REOPEN);
                    } else {
                        shop_item_add(shop_menu_selected_value(shop_choice_menu), 1, 1);
                        estate_gold_change(-item_buy_price(shop_menu_selected_value(shop_choice_menu)), 1);
                        menu_request_open(2, 0x51);
                        shop_set_step(SHOP_POULTRY_MESSAGE_CLOSED, SHOP_POULTRY_REOPEN);
                    }
                } else {
                    menu_request_open(2, 0x4F);
                    shop_set_step(SHOP_POULTRY_MESSAGE_CLOSED, SHOP_POULTRY_REOPEN);
                }
                break;
            }
            break;
        case SHOP_MENU_RESULT_UNUSED:
            break;
        }
        break;
    case SHOP_ANIMAL_COUNT_ASK:
        menu_set_sound_enabled(0);
        text_set_input(0);
        menu_request_open(2, 0x4D);
        shop_set_step(SHOP_POULTRY_MESSAGE_END, SHOP_ANIMAL_COUNT_MESSAGE);
        break;
    case SHOP_ANIMAL_COUNT_MESSAGE:
    case SHOP_ANIMAL_COUNT_MESSAGE_CLOSE:
    case SHOP_ANIMAL_COUNT_MESSAGE_END:
    case SHOP_ANIMAL_COUNT_PROMPT:
    case SHOP_ANIMAL_COUNT_INPUT:
    case SHOP_ANIMAL_COUNT_DONE:
        choice = shop_menu_choose_next(&shop_step, SHOP_ANIMAL_COUNT_MESSAGE, SHOP_TICK_PANEL_SUBMIT, request, &shop_menu_header);
        switch (choice) {
        case SHOP_MENU_RESULT_NONE:
            break;
        case SHOP_MENU_RESULT_CANCELLED:
            shop_set_step(SHOP_POULTRY_MESSAGE_CLOSED, SHOP_POULTRY_REOPEN);
            break;
        case SHOP_MENU_RESULT_CHOSEN:
            menu_begin_close();
            if (shop_can_afford(shop_menu_header.h8 * shop_menu_header.ha) == choice) {
                if (shop_menu_header.h8 + game_state.progress.estate.chickenFeed >= SHOP_FEED_CAP) {
                    menu_request_open(2, 0x4C);
                    shop_set_step(SHOP_POULTRY_MESSAGE_CLOSED, SHOP_ANIMAL_COUNT_ASK);
                } else {
                    shop_chicken_feed_order_commit(&shop_menu_header);
                    menu_request_open(2, 0x52);
                    shop_set_step(SHOP_POULTRY_MESSAGE_CLOSED, SHOP_POULTRY_REOPEN);
                }
            } else {
                menu_request_open(2, 0x4F);
                shop_set_step(SHOP_POULTRY_MESSAGE_CLOSED, SHOP_ANIMAL_COUNT_ASK);
            }
            break;
        case SHOP_MENU_RESULT_UNUSED:
            break;
        }
        break;
    case SHOP_ANIMAL_NAME_START:
        name_entry_reset();
        name_entry_start_cancellable(shop_poultry_name_actor.name);
        shop_dialogue_tick_index = 0;
        shop_set_step(SHOP_ANIMAL_NAME_ENTRY, SHOP_ANIMAL_START);
        break;
    case SHOP_ANIMAL_NAME_ENTRY:
        status = name_entry_step();
        switch (status) {
        case SHOP_NAME_CANCELLED:
            menu_request_open(2, 0x4E);
            shop_set_step(SHOP_POULTRY_MESSAGE_CLOSED, SHOP_POULTRY_REOPEN);
            break;
        case SHOP_NAME_ENTERED:
            object = farm_action_create(ITEM_CHICKEN, -1);
            for (i = 0; i < NAME_LENGTH; i++) {
                object->name[i] = shop_poultry_name_actor.name[i];
            }
            estate_gold_change(-item_buy_price(shop_menu_selected_value(shop_choice_menu)), 1);
            menu_request_open(2, 0x57);
            shop_set_step(SHOP_POULTRY_MESSAGE_CLOSED, SHOP_POULTRY_REOPEN);
            break;
        }
        break;
    case SHOP_ANIMAL_SELL_PROMPT:
        menu_set_sound_enabled(0);
        text_set_input(0);
        menu_request_open(2, 0x5A);
        shop_set_step(SHOP_POULTRY_MESSAGE_END, SHOP_ANIMAL_SELL_LIST);
        break;
    case SHOP_ANIMAL_SELL_LIST:
        slot = shop_record_list(SHOP_LIVESTOCK_CHICKEN, 0, 0);
        if (slot == SHOP_RECORD_CANCEL) {
            menu_begin_close();
            menu_request_open(2, 0x5B);
            shop_set_step(SHOP_POULTRY_MESSAGE_CLOSED, SHOP_POULTRY_REOPEN);
        } else if (slot != SHOP_RECORD_NONE) {
            menu_begin_close();
            menu_set_sound_enabled(0);
            if (CHICKEN(slot).actor.flags & ACTOR_FLAG_PRIZE_WINNER) {
                menu_request_open(2, 0x61);
            } else {
                menu_request_open(2, 0x60);
            }
            shop_set_step(SHOP_POULTRY_MESSAGE_END, SHOP_ANIMAL_SELL_CONFIRM);
        }
        break;
    case SHOP_ANIMAL_SELL_CONFIRM:
        slot = shop_record_list(SHOP_LIVESTOCK_CHICKEN, 0, 1);
        switch (shop_result_update(&current_frame->otTags[0][11], 200, 128)) {
        case SHOP_ANSWER_PENDING:
            break;
        case SHOP_ANSWER_YES:
            menu_begin_close();
            menu_request_open(2, 0x5E);
            if (CHICKEN(slot).actor.flags & ACTOR_FLAG_PRIZE_WINNER) {
                estate_gold_change(2000, 1);
            } else {
                estate_gold_change(500, 1);
            }
            chicken_clear(&CHICKEN(slot));
            shop_set_step(SHOP_POULTRY_MESSAGE_CLOSED, SHOP_POULTRY_REOPEN);
            break;
        case SHOP_ANSWER_NO:
            menu_begin_close();
            menu_request_open(2, 0x5B);
            shop_set_step(SHOP_POULTRY_MESSAGE_CLOSED, SHOP_POULTRY_REOPEN);
            break;
        }
        break;
    case SHOP_POULTRY_REOPEN:
        menu_request_open(2, 0x53);
        shop_set_step(SHOP_POULTRY_MESSAGE_CLOSED, SHOP_ANIMAL_SETUP);
        break;
    case SHOP_POULTRY_MESSAGE_CLOSED:
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            shop_restore_step();
        }
        break;
    case SHOP_POULTRY_MESSAGE_END:
        if (shop_return_step == SHOP_ANIMAL_SELL_CONFIRM) {
            shop_record_list(SHOP_LIVESTOCK_CHICKEN, 0, 1);
        }
        if (menu_get_phase() == TEXT_WINDOW_END_WAIT) {
            text_set_input(0);
            shop_restore_step();
        }
        break;
    case SHOP_POULTRY_LEAVE:
        game_state.player.stateFlags &= ~PLAYER_FLAG_SCRIPTED;
        result = 1;
        break;
    }
    return result;
}

static void shop_chicken_feed_order_commit(ShopMenuHeader *header) {
    s32 product = header->ha * header->h8;
    estate_gold_change(-product, 1);
    estate_chicken_feed_add(2, header->h8);
}

/* The plot-sale dialogue's steps: start, offer the held chicken or the marked plot, take
 * the answer, hand the item over, wait for it to leave the player's hands, the message
 * waits, and the leave. */
enum ShopPlotSaleStep {
    SHOP_PLOT_SALE_START = 0,
    SHOP_PLOT_SALE_OFFER = 1,
    SHOP_PLOT_SALE_ANSWER = 2,
    SHOP_PLOT_SALE_TAKE_ITEM = 3,
    SHOP_PLOT_SALE_WAIT_ITEM = 4,
    SHOP_PLOT_SALE_MESSAGE_CLOSED = 5,
    SHOP_PLOT_SALE_MESSAGE_END = 6,
    SHOP_PLOT_SALE_LEAVE = 7,
};

/* The plot-sale dialogue: offer the held chicken or the marked plot, pay 2000 or 500,
 * clear the plot and close. */
static s32 shop_plot_sale_step(void)
{
    s32 done;
    s16 kind;
    s32 choice;

    done = 0;
    game_state.player.scriptHeld = 0;
    game_state.player.scriptPressed = 0;
    game_state.player.stateFlags |= PLAYER_FLAG_SCRIPTED;
    kind = game_state.player.held[HELD_ITEM].id;
    switch (shop_step) {
    case SHOP_PLOT_SALE_START:
        shop_set_step(SHOP_PLOT_SALE_OFFER, SHOP_PLOT_SALE_START);
        break;
    case SHOP_PLOT_SALE_OFFER:
        if (kind == ITEM_CHICK) {
            menu_request_open(2, 0x5C);
            shop_set_step(SHOP_PLOT_SALE_MESSAGE_CLOSED, SHOP_PLOT_SALE_LEAVE);
        } else if (kind == ITEM_CHICKEN) {
            menu_set_sound_enabled(0);
            if (shop_plot_check_marked() == 1) {
                menu_request_open(2, 0x61);
            } else {
                menu_request_open(2, 0x60);
            }
            shop_set_step(SHOP_PLOT_SALE_MESSAGE_END, SHOP_PLOT_SALE_ANSWER);
        }
        break;
    case SHOP_PLOT_SALE_ANSWER:
        choice = shop_result_update(&current_frame->otTags[0][11], 200, 128);
        switch (choice) {
        case SHOP_ANSWER_PENDING:
            break;
        case SHOP_ANSWER_YES:
            menu_begin_close();
            menu_request_open(2, 0x51);
            if (shop_plot_check_marked() == choice) {
                estate_gold_change(2000, 1);
            } else {
                estate_gold_change(500, 1);
            }
            chicken_clear(&CHICKEN(shop_plot_find_marked()));
            shop_set_step(SHOP_PLOT_SALE_MESSAGE_CLOSED, SHOP_PLOT_SALE_TAKE_ITEM);
            break;
        case SHOP_ANSWER_NO:
            menu_begin_close();
            menu_request_open(2, 0x5B);
            shop_set_step(SHOP_PLOT_SALE_MESSAGE_CLOSED, SHOP_PLOT_SALE_LEAVE);
            break;
        }
        break;
    case SHOP_PLOT_SALE_TAKE_ITEM:
        player_use_held_item(0);
        shop_set_step(SHOP_PLOT_SALE_WAIT_ITEM, SHOP_PLOT_SALE_START);
        break;
    case SHOP_PLOT_SALE_WAIT_ITEM:
        if (!(game_state.player.stateFlags & PLAYER_FLAG_BIT16)) {
            shop_set_step(SHOP_PLOT_SALE_LEAVE, SHOP_PLOT_SALE_START);
        }
        break;
    case SHOP_PLOT_SALE_MESSAGE_CLOSED:
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            shop_restore_step();
        }
        break;
    case SHOP_PLOT_SALE_MESSAGE_END:
        if (menu_get_phase() == TEXT_WINDOW_END_WAIT) {
            text_set_input(0);
            shop_restore_step();
        }
        break;
    case SHOP_PLOT_SALE_LEAVE:
        game_state.player.stateFlags &= ~PLAYER_FLAG_SCRIPTED;
        done = 1;
        break;
    }
    return done;
}

/* The index of the last of the ten plots flagged 0x20000, or -1 when none is. */
static s16 shop_plot_find_marked(void) {
    s16 result = -1;
    s32 i = 0;
    do {
        if (game_state.animals.chickens[i].actor.flags & 0x20000) result = i;
        i++;
    } while (i < 10);
    return result;
}

/* 1 when the marked plot (shop_plot_find_marked) has its 0x40 status bit, else 0. */
static s32 shop_plot_check_marked(void) {
    s32 i = shop_plot_find_marked();
    u8 *base = (u8 *)&game_state;
    FarmPlotStatus *records = (FarmPlotStatus *)(base + 0x7AE8);
    if (records[i].value & 0x40) return 1;
    return 0;
}

/* 1 unless the farm is on screen 0x14. */
static s32 shop_not_plot_upgrade(void) {
    return shop_screen_id != 0x14;
}

/* The doctor's steps: the exam and the medicine. */
enum ShopDoctorStep {
    SHOP_DOCTOR_START = 0,
    SHOP_DOCTOR_CHOOSE_NO_INTRO = 1,
    SHOP_DOCTOR_CHOOSE_MESSAGE = 2,
    SHOP_DOCTOR_CHOOSE_MESSAGE_CLOSE = 3,
    SHOP_DOCTOR_CHOOSE_MESSAGE_END = 4,
    SHOP_DOCTOR_CHOOSE_PROMPT = 5,
    SHOP_DOCTOR_CHOOSE_INPUT = 6,
    SHOP_DOCTOR_CHOOSE_DONE = 7,
    SHOP_DOCTOR_CHOOSE_AFTER_DONE = 8,
    SHOP_DOCTOR_EXAM_ASK = 9,
    SHOP_DOCTOR_EXAM_FADE_OUT = 10,
    SHOP_DOCTOR_EXAM_FADE_OUT_WAIT = 11,
    SHOP_DOCTOR_EXAM_FADE_IN = 12,
    SHOP_DOCTOR_EXAM_FADE_IN_WAIT = 13,
    SHOP_DOCTOR_EXAM_RESULT = 14,
    SHOP_DOCTOR_MEDICINE_SETUP = 15,
    SHOP_DOCTOR_MEDICINE_MESSAGE = 16,
    SHOP_DOCTOR_MEDICINE_MESSAGE_CLOSE = 17,
    SHOP_DOCTOR_MEDICINE_MESSAGE_END = 18,
    SHOP_DOCTOR_MEDICINE_PROMPT = 19,
    SHOP_DOCTOR_MEDICINE_INPUT = 20,
    SHOP_DOCTOR_MEDICINE_DONE = 21,
    SHOP_DOCTOR_MEDICINE_AFTER_DONE = 22,
    SHOP_DOCTOR_REOPEN = 23,
    SHOP_DOCTOR_MESSAGE_CLOSED = 24,
    SHOP_DOCTOR_MESSAGE_END = 25,
    SHOP_DOCTOR_LEAVE = 26,
    SHOP_DOCTOR_CARRY_CUE = 27,
    SHOP_DOCTOR_CARRY_WAIT = 28,
};

/* The doctor's choose menu rows, in the order shop_medicine_menu_setup adds them. */
enum ShopDoctorMenuRow {
    SHOP_DOCTOR_MENU_EXAM = 0,
    SHOP_DOCTOR_MENU_MEDICINE = 1,
};

/* The doctor's dialogue: a choose menu (the exam, or the medicine list), the exam (steps
 * 9-14: a yes or no, the fade, the result) and the medicine purchase on the confirm menu
 * (16-22), which hands the medicine over and leaves. Returns 1 at step 26. */
static s32 shop_doctor_step(void)
{
    FarmMenuRequest choose;
    FarmMenuRequest confirm;
    s32 done;
    s16 item;
    s16 price;
    s32 choice;

    choose.first[0] = 2;
    choose.first[1] = 0xDD;
    choose.second[0] = 2;
    choose.second[1] = 0xDE;
    choose.count = 4;
    choose.menu = &shop_choice_menu;
    confirm.first[0] = 2;
    confirm.first[1] = 0xE6;
    confirm.second[0] = 2;
    confirm.second[1] = 0xFC;
    confirm.count = 6;
    confirm.menu = &shop_confirm_menu;
    item = shop_menu_selected_value(shop_confirm_menu);
    done = 0;
    price = item_buy_price(item);
    game_state.player.stateFlags |= PLAYER_FLAG_SCRIPTED;
    game_state.player.scriptHeld = 0;
    game_state.player.scriptPressed = 0;
    switch (shop_step) {
    case SHOP_DOCTOR_START:
        shop_set_step(SHOP_DOCTOR_CHOOSE_MESSAGE, SHOP_DOCTOR_START);
        break;
    case SHOP_DOCTOR_CHOOSE_NO_INTRO:
        choose.first[0] = SHOP_NO_MESSAGE;
        shop_set_step(SHOP_DOCTOR_CHOOSE_MESSAGE, SHOP_DOCTOR_START);
        /* fall through */
    case SHOP_DOCTOR_CHOOSE_MESSAGE:
    case SHOP_DOCTOR_CHOOSE_MESSAGE_CLOSE:
    case SHOP_DOCTOR_CHOOSE_MESSAGE_END:
    case SHOP_DOCTOR_CHOOSE_PROMPT:
    case SHOP_DOCTOR_CHOOSE_INPUT:
    case SHOP_DOCTOR_CHOOSE_DONE:
    case SHOP_DOCTOR_CHOOSE_AFTER_DONE:
        switch (shop_menu_open_choose(&shop_step, SHOP_DOCTOR_CHOOSE_MESSAGE, choose)) {
        case SHOP_MENU_RESULT_NONE:
            break;
        case SHOP_MENU_RESULT_CANCELLED:
            shop_set_step(SHOP_DOCTOR_LEAVE, SHOP_DOCTOR_START);
            break;
        case SHOP_MENU_RESULT_CHOSEN:
            switch (shop_choice_menu.selected) {
            case SHOP_DOCTOR_MENU_EXAM:
                menu_set_sound_enabled(0);
                menu_request_open(2, 0xE5);
                shop_set_step(SHOP_DOCTOR_MESSAGE_END, SHOP_DOCTOR_EXAM_ASK);
                break;
            case SHOP_DOCTOR_MENU_MEDICINE:
                shop_set_step(SHOP_DOCTOR_MEDICINE_MESSAGE, SHOP_DOCTOR_START);
                break;
            }
            break;
        case SHOP_MENU_RESULT_UNUSED:
            break;
        }
        break;
    case SHOP_DOCTOR_EXAM_ASK:
        choice = shop_result_update(&current_frame->otTags[0][11], 200, 128);
        switch (choice) {
        case SHOP_ANSWER_PENDING:
            break;
        case SHOP_ANSWER_YES:
            menu_begin_close();
            if (shop_can_afford(10) == choice) {
                estate_gold_change(-10, 1);
                menu_request_open(2, 0xE7);
                shop_set_step(SHOP_DOCTOR_MESSAGE_CLOSED, SHOP_DOCTOR_EXAM_FADE_OUT);
            } else {
                menu_request_open(2, 0x4F);
                shop_set_step(SHOP_DOCTOR_MESSAGE_CLOSED, SHOP_DOCTOR_CHOOSE_NO_INTRO);
            }
            break;
        case SHOP_ANSWER_NO:
            menu_begin_close();
            menu_request_open(2, 0xE8);
            shop_set_step(SHOP_DOCTOR_MESSAGE_CLOSED, SHOP_DOCTOR_CHOOSE_NO_INTRO);
            break;
        }
        break;
    case SHOP_DOCTOR_EXAM_FADE_OUT:
        fade_start_out(20, 1, 2);
        shop_set_step(SHOP_DOCTOR_EXAM_FADE_OUT_WAIT, SHOP_DOCTOR_START);
        break;
    case SHOP_DOCTOR_EXAM_FADE_OUT_WAIT:
        if (fade_is_settled() == 1) {
            menu_request_open(2, 0xE9);
            shop_set_step(SHOP_DOCTOR_MESSAGE_CLOSED, SHOP_DOCTOR_EXAM_FADE_IN);
        }
        break;
    case SHOP_DOCTOR_EXAM_FADE_IN:
        fade_start_in(20, 1, 2);
        shop_set_step(SHOP_DOCTOR_EXAM_FADE_IN_WAIT, SHOP_DOCTOR_START);
        break;
    case SHOP_DOCTOR_EXAM_FADE_IN_WAIT:
        if (fade_is_settled() == 1) {
            menu_request_open(2, 0xEA);
            shop_set_step(SHOP_DOCTOR_MESSAGE_CLOSED, SHOP_DOCTOR_EXAM_RESULT);
        }
        break;
    case SHOP_DOCTOR_EXAM_RESULT:
        menu_request_open(2, shop_condition_message());
        shop_set_step(SHOP_DOCTOR_MESSAGE_CLOSED, SHOP_DOCTOR_LEAVE);
        break;
    case SHOP_DOCTOR_MEDICINE_SETUP:
        confirm.first[0] = SHOP_NO_MESSAGE;
        shop_set_step(SHOP_DOCTOR_MEDICINE_MESSAGE, SHOP_DOCTOR_START);
        /* fall through */
    case SHOP_DOCTOR_MEDICINE_MESSAGE:
    case SHOP_DOCTOR_MEDICINE_MESSAGE_CLOSE:
    case SHOP_DOCTOR_MEDICINE_MESSAGE_END:
    case SHOP_DOCTOR_MEDICINE_PROMPT:
    case SHOP_DOCTOR_MEDICINE_INPUT:
    case SHOP_DOCTOR_MEDICINE_DONE:
    case SHOP_DOCTOR_MEDICINE_AFTER_DONE:
        choice = shop_menu_open_choose(&shop_step, SHOP_DOCTOR_MEDICINE_MESSAGE, confirm);
        switch (choice) {
        case SHOP_MENU_RESULT_NONE:
            break;
        case SHOP_MENU_RESULT_CANCELLED:
            confirm.second[0] = SHOP_NO_MESSAGE;
            shop_set_step(SHOP_DOCTOR_CHOOSE_NO_INTRO, SHOP_DOCTOR_START);
            break;
        case SHOP_MENU_RESULT_CHOSEN:
            if (shop_can_afford(price) == choice) {
                estate_gold_change(-price, 1);
                menu_request_open(2, 0xFB);
                shop_set_step(SHOP_DOCTOR_MESSAGE_CLOSED, SHOP_DOCTOR_CARRY_CUE);
            } else {
                menu_request_open(2, 0x4F);
                shop_set_step(SHOP_DOCTOR_MESSAGE_CLOSED, SHOP_DOCTOR_REOPEN);
            }
            break;
        case SHOP_MENU_RESULT_UNUSED:
            break;
        }
        break;
    case SHOP_DOCTOR_REOPEN:
        menu_request_open(2, 0xFC);
        shop_set_step(SHOP_DOCTOR_MESSAGE_CLOSED, SHOP_DOCTOR_CHOOSE_NO_INTRO);
        break;
    case SHOP_DOCTOR_MESSAGE_END:
        if (menu_get_phase() == TEXT_WINDOW_END_WAIT) {
            text_set_input(0);
            shop_restore_step();
        }
        break;
    case SHOP_DOCTOR_MESSAGE_CLOSED:
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            shop_restore_step();
        }
        break;
    case SHOP_DOCTOR_LEAVE:
        done = 1;
        break;
    case SHOP_DOCTOR_CARRY_CUE:
    case SHOP_DOCTOR_CARRY_WAIT:
        if (shop_item_carry_step(&shop_step, SHOP_DOCTOR_CARRY_CUE, shop_confirm_menu.entryValue[shop_confirm_menu.selected]) != 0) {
            shop_set_step(SHOP_DOCTOR_LEAVE, SHOP_DOCTOR_START);
        }
        break;
    }
    return done;
}

static s32 shop_dialogue_done = 1;

/* The survey's condition messages, by stamina row and fatigue column. */
static u16 shop_condition_messages[5][4] = {
    {0xEB, 0xEC, 0xED, 0xEE}, {0xEF, 0xF0, 0xF1, 0xF2},
    {0xF3, 0xF4, 0xF5, 0xF6}, {0x1D9, 0x1DA, 0x1DB, 0x1DC},
    {0xF7, 0xF8, 0xF9, 0xFA},
};

/* The player's condition message: the row from how full stamina is (percent of
 * maxStamina: >= 70, 50, 20, 5, below), the column from fatigue (< 50, 70, 90, more). */
static u16 shop_condition_message(void)
{
    s16 percent;
    s16 fatigue;
    s32 row;
    s32 column;

    percent = game_state.player.stamina;
    percent = percent * 100 / game_state.player.maxStamina;
    fatigue = game_state.player.fatigue;
    row = 0;
    if (percent < 70) {
        if (percent >= 50) {
            row = 1;
        } else if (percent >= 20) {
            row = 2;
        } else if (percent >= 5) {
            row = 3;
        } else {
            row = 4;
        }
    }
    if (fatigue < 50) {
        column = 0;
    } else if (fatigue < 70) {
        column = 1;
    } else if (fatigue < 90) {
        column = 2;
    } else {
        column = 3;
    }
    return shop_condition_messages[row][column];
}

/* 1 once the farm has left the medal shop screen (0x1A). */
s32 shop_medal_exchange_done(void) {
    return shop_screen_id != 0x1A;
}

/* The medal-shop dialogue's steps: the choose menu of seven goods (from 1), the buy
 * answer, the message waits, the leave, and the carry of the bought good. */
enum ShopMedalStep {
    SHOP_MEDAL_START = 0,
    SHOP_MEDAL_CHOOSE_MESSAGE = 1,
    SHOP_MEDAL_CHOOSE_MESSAGE_CLOSE = 2,
    SHOP_MEDAL_CHOOSE_MESSAGE_END = 3,
    SHOP_MEDAL_CHOOSE_PROMPT = 4,
    SHOP_MEDAL_CHOOSE_INPUT = 5,
    SHOP_MEDAL_CHOOSE_DONE = 6,
    SHOP_MEDAL_CHOOSE_AFTER_DONE = 7,
    SHOP_MEDAL_ANSWER = 8,
    SHOP_MEDAL_MESSAGE_CLOSED = 9,
    SHOP_MEDAL_MESSAGE_END = 10,
    SHOP_MEDAL_LEAVE = 11,
    SHOP_MEDAL_CARRY_CUE = 12,
    SHOP_MEDAL_CARRY_WAIT = 13,
};

/* The rows shop_menu_entry_buy handles by number: row 0 gives the power berry and sets
 * event bit 0x10, row 1 adds 500 lumber, the rest are carried. */
enum ShopMedalRow {
    SHOP_MEDAL_ROW_POWER_BERRY = 0,
    SHOP_MEDAL_ROW_LUMBER = 1,
};

/* The medal-shop dialogue: a choose menu of seven goods, a confirmation, and the
 * purchase, which takes medals. Returns 1 at step 11. */
static s32 shop_medal_step(void)
{
    FarmMenuRequest request;
    s32 done;
    s32 choice;

    done = 0;
    request.first[0] = SHOP_NO_MESSAGE;
    request.second[0] = SHOP_NO_MESSAGE;
    request.count = 7;
    request.menu = &shop_choice_menu;
    game_state.player.scriptHeld = 0;
    game_state.player.scriptPressed = 0;
    game_state.player.stateFlags |= PLAYER_FLAG_SCRIPTED;
    switch (shop_step) {
    case SHOP_MEDAL_START:
        shop_dialogue_tick_index = SHOP_TICK_MEDALS_CHOICE;
        shop_set_step(SHOP_MEDAL_CHOOSE_MESSAGE, SHOP_MEDAL_START);
        request.first[0] = SHOP_NO_MESSAGE;
        break;
    case SHOP_MEDAL_CHOOSE_MESSAGE:
    case SHOP_MEDAL_CHOOSE_MESSAGE_CLOSE:
    case SHOP_MEDAL_CHOOSE_MESSAGE_END:
    case SHOP_MEDAL_CHOOSE_PROMPT:
    case SHOP_MEDAL_CHOOSE_INPUT:
    case SHOP_MEDAL_CHOOSE_DONE:
    case SHOP_MEDAL_CHOOSE_AFTER_DONE:
        choice = shop_menu_open_choose(&shop_step, SHOP_MEDAL_CHOOSE_MESSAGE, request);
        switch (choice) {
        case SHOP_MENU_RESULT_NONE:
            break;
        case SHOP_MENU_RESULT_CANCELLED:
            menu_request_open(1, 0x447);
            shop_set_step(SHOP_MEDAL_MESSAGE_CLOSED, SHOP_MEDAL_LEAVE);
            break;
        case SHOP_MENU_RESULT_CHOSEN:
            if (shop_medal_affordable(&shop_choice_menu) == choice) {
                menu_request_open(1, 0x449);
                shop_set_step(SHOP_MEDAL_MESSAGE_END, SHOP_MEDAL_ANSWER);
            } else {
                menu_request_open(1, 0x44A);
                shop_set_step(SHOP_MEDAL_MESSAGE_CLOSED, SHOP_MEDAL_START);
            }
            break;
        case SHOP_MENU_RESULT_UNUSED:
            break;
        }
        break;
    case SHOP_MEDAL_ANSWER:
        switch (shop_result_update(&current_frame->otTags[0][11], 200, 128)) {
        case SHOP_ANSWER_PENDING:
            break;
        case SHOP_ANSWER_YES:
            menu_begin_close();
            menu_request_open(1, 0x448);
            shop_menu_entry_buy(&shop_choice_menu);
            break;
        case SHOP_ANSWER_NO:
            menu_begin_close();
            menu_request_open(1, 0x447);
            shop_set_step(SHOP_MEDAL_MESSAGE_CLOSED, SHOP_MEDAL_START);
            break;
        }
        break;
    case SHOP_MEDAL_MESSAGE_END:
        if (menu_get_phase() == TEXT_WINDOW_END_WAIT) {
            text_set_input(0);
            shop_restore_step();
        }
        break;
    case SHOP_MEDAL_MESSAGE_CLOSED:
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            shop_restore_step();
        }
        break;
    case SHOP_MEDAL_LEAVE:
        done = 1;
        break;
    case SHOP_MEDAL_CARRY_CUE:
    case SHOP_MEDAL_CARRY_WAIT:
        if (shop_item_carry_step(&shop_step, SHOP_MEDAL_CARRY_CUE, shop_choice_menu.entryValue[shop_choice_menu.selected]) != 0) {
            shop_set_step(SHOP_MEDAL_LEAVE, SHOP_MEDAL_START);
        }
        break;
    }
    return done;
}

/* 1 when the player has the medals for the selected row's price. */
static s32 shop_medal_affordable(FarmMenu *menu) {
    s16 price;
    price = shop_medal_price((u16)menu->entryValue[menu->selected]);
    return game_state.player.medals >= price;
}

/* Buy the medal row picked: take its price in medals, then give the power berry (and set event
 * bit 0x10), add the lumber, or set up the carry of the item for the other rows. */
static void shop_menu_entry_buy(FarmMenu *menu) {
    s16 price;
    s16 index;

    shop_dialogue_tick_index = 0;
    price = shop_medal_price((u16)menu->entryValue[menu->selected]);
    game_state.player.medals = game_state.player.medals - price;
    index = menu->selected;
    if (index == SHOP_MEDAL_ROW_POWER_BERRY) {
        game_state.progress.estate.eventBits = game_state.progress.estate.eventBits | 0x10;
        player_receive_item(&game_state.player, ITEM_POWER_BERRY, 2);
        shop_set_step(SHOP_MEDAL_MESSAGE_CLOSED, SHOP_MEDAL_LEAVE);
    } else if (index == SHOP_MEDAL_ROW_LUMBER) {
        estate_lumber_add(3, 500);
        shop_set_step(SHOP_MEDAL_MESSAGE_CLOSED, SHOP_MEDAL_LEAVE);
    } else {
        shop_set_step(SHOP_MEDAL_MESSAGE_CLOSED, SHOP_MEDAL_CARRY_CUE);
    }
}

static void (*shop_livestock_icon_draws[4])(u_long *ot, s16 x, s16 y) = {
    ui_draw_icon_27_0, ui_draw_icon_27_2, ui_draw_icon_27_1, ui_draw_icon_27_3};

/* One frame of the record list for `kind` (0 chickens, 1 cows, 2 sheep): the title,
 * then with `action` 0 the cursor moves and the four-row window scrolls. action -1
 * resets the cursor and returns -2. Returns the record under the cursor (on confirm
 * when action is 0), -2 on cancel, else -1. */
s32 shop_record_list(s16 kind, s16 filter, s16 action)
{
    u_long *text;
    u_long *list;
    s16 mode;
    s32 result;
    s32 count;
    s32 i;
    s32 row;
    s32 next;
    s32 y;
    s32 top;
    s32 index;
    s32 icon;
    s32 triggered;
    s32 pressed;
    FarmActor *record;

    if (action == -1) {
        shop_list_cursor = 0;
        shop_list_top = 0;
        return -2;
    }
    text = &current_frame->otTags[0][14];
    list = &current_frame->otTags[0][16];
    mode = kind < 0 ? SHOP_LIVESTOCK_CHICKEN : (kind < 3 ? kind : SHOP_LIVESTOCK_SHEEP);
    result = -1;
    prim_add_tile(&current_frame->otTags[0][18], 0, 0, HWD0, VWD0, 0, 1);
    window_frame_draw_at(&current_frame->otTags[0][17], 1, 24, 32, 272, 116);
    window_frame_draw_at(&current_frame->otTags[0][15], 1, 80, 16, 160, 32);

    {
        u16 *suffix;
        /* Copy the farm name; at its terminator append the localized suffix
         * (four halfwords) and a new terminator. */
        for (i = 0;; i++) {
            if (game_state.progress.estate.farmName[i] == 0xFFFF) {
                suffix = LOCALIZED(text_bank.messages.farm);
                shop_text_buffer[i] = suffix[0];
                shop_text_buffer[i + 1] = suffix[1];
                shop_text_buffer[i + 2] = suffix[2];
                shop_text_buffer[i + 3] = suffix[3];
                shop_text_buffer[i + 4] = 0xFFFF;
                break;
            }
            shop_text_buffer[i] = game_state.progress.estate.farmName[i];
        }
    }
    text_draw_plain(text, shop_text_buffer, 82, 26);
    shop_livestock_icon_draws[mode](text, 176, 18);
    text_draw_plain(text, LOCALIZED(text_bank.messages.name_panel_title), 200, 26);

    count = shop_livestock_find(mode, filter, -1);
    if (action == 0) {
        triggered = input_pad1->triggered;
        if (triggered & PAD_UP) {
            sound_play(5);
            shop_list_cursor--;
        } else if (triggered & PAD_DOWN) {
            sound_play(5);
            shop_list_cursor++;
        }
        if (shop_list_cursor < 0) {
            shop_list_cursor = count - 1;
        }
        if (count - 1 < shop_list_cursor) {
            shop_list_cursor = 0;
        }
        if (count > SHOP_LIST_ROWS) {
            if (shop_list_top + SHOP_LIST_ROWS - 1 < shop_list_cursor) {
                shop_list_top = shop_list_cursor - (SHOP_LIST_ROWS - 1);
            }
            if (shop_list_cursor < shop_list_top) {
                shop_list_top = shop_list_cursor;
            }
        }
    }
    if (shop_list_top > 0) {
        ui_draw_icon_29_phase(list, 288, 48);
    }
    if (shop_list_top + SHOP_LIST_ROWS < count) {
        ui_draw_icon_28_phase(list, 288, 128);
    }
    ui_draw_icon_0b_1(list, 32, (shop_list_cursor - shop_list_top) * SHOP_LIST_ROW_HEIGHT + 60);

    for (i = 0; i < SHOP_LIST_ROWS; i++) {
        next = i + 1;
        if (count < shop_list_top + next) {
            break;
        }
        top = i * SHOP_LIST_ROW_HEIGHT;
        index = shop_livestock_find(mode, filter, shop_list_top + i + 1);
        row = i + 1;
        text_draw_number(shop_list_top + row, 2, list, 32, top + 54);
        y = top + 48;
        if (mode == SHOP_LIVESTOCK_CHICKEN) {
            record = &CHICKEN(index).actor;
        } else {
            record = &game_state.animals.livestock[index].actor;
        }
        icon = mode;
        if (mode == SHOP_LIVESTOCK_SHEEP && ANIMAL(index).daysSinceShearing < SHEARING_INTERVAL_DAYS) {
            icon = 3;
        }
        shop_livestock_icon_draws[icon](list, 64, y);
        shop_draw_text_row(record, list, 88, y);
    }

    if (action == 0) {
        pressed = input_pad1->pressed;
        if (pressed & PAD_CIRCLE) {
            sound_play(1);
            result = -2;
        } else if (pressed & PAD_CROSS) {
            sound_play(0);
            result = shop_livestock_find(mode, filter, shop_list_cursor + 1);
        }
    } else {
        result = shop_livestock_find(mode, filter, shop_list_cursor + 1);
    }
    return result;
}

/* Draw one record row at (x, y): the name, the affection icons on the row below, the date
 * (day and year) and the season icon. */
static void shop_draw_text_row(FarmActor *actor, u_long *ot, s16 x, s16 y) {
    s16 column;
    s16 row;
    u16 *work;
    u16 *digits;
    u32 index;
    s16 value;
    column = x;
    row = y;
    work = shop_text_buffer;
    index = 0;
    do {
        work[index] = 0xFFD;
        index++;
    } while (index < 24);
    if (actor->flags & 0x40) {
        work[0] = 0x90;
    }
    work[4] = 0x71;
    text_format_number(actor->date.day + 1, &work[5], 2);
    work[8] = 0;
    work[9] = 0x26;
    work[10] = 0x24;
    value = shop_read_value(&actor->date.year);
    digits = text_format_number(value, &work[11], 2);
    *digits = 0xFFFF;
    text_draw_plain(ot, actor->name, column + 2, row);
    shop_draw_livestock_row(actor->affection / 25, ot, column + 72, row + 12);
    value = column + 88;
    text_draw_plain(ot, work, value, row);
    ui_draw_icon_2b_sub(ot, value + 16, row, actor->date.season);
}

static s16 shop_read_value(s16 *p) {
    return *p;
}

/* Draw `raw_count` icons (the affection, one per 25 points) in a row from raw_x, each 8
 * pixels left of the last. */
static void shop_draw_livestock_row(s16 raw_count, u_long *ot, s16 raw_x, s16 raw_y) {
    s32 remaining;
    u_long *draw_ot;
    s32 pos;
    s32 step;
    s16 y;
    s32 val;
    s32 temp;
    s32 count;

    draw_ot = ot;
    val = raw_x;
    count = raw_count;
    y = raw_y;
    if (count <= 0) {
        return;
    }
    remaining = count;
    pos = (val << 16) + 0xFFF80000;
    step = 0xFFF80000;
    do {
        ui_draw_icon_2a(draw_ot, val, y);
        temp = pos >> 16;
        pos += step;
        val = temp;
        remaining--;
    } while (remaining != 0);
}

/* Count the animals of kind `mode` (0 chickens, 1 cows, 2 sheep). Filter 0 counts the cows that
 * are not pregnant and every sheep; filter 1 counts the cows that are healthy and not pregnant,
 * and the sheep due for shearing that are healthy and not pregnant. Chickens skip contest entrants.
 * With `wanted` == -1 return the count; otherwise return the index of the record at
 * which the count reached `wanted` (0 if it never did). */
static s16 shop_livestock_find(s16 kind, s16 filter, s16 wanted) {
    s16 count;
    s16 found;
    s32 i;

    count = 0;
    found = 0;
    switch (kind) {
    case SHOP_LIVESTOCK_CHICKEN:
        for (i = 0; i < MAX_CHICKENS; i++) {
            if (CHICKEN(i).actor.kind == ITEM_CHICKEN) {
                if (!(CHICKEN(i).actor.flags & ACTOR_FLAG_CONTEST_ENTRANT)) {
                    count++;
                }
                if (count == wanted) {
                    found = i;
                    break;
                }
            }
        }
        break;
    case SHOP_LIVESTOCK_COW:
        for (i = 0; i < MAX_LIVESTOCK; i++) {
            if (ANIMAL(i).actor.kind == ITEM_COW) {
                if (filter == 0) {
                    if (ANIMAL(i).pregnancy == 0) {
                        count++;
                    }
                } else if (filter == 1 && ANIMAL(i).daysSick == 0 && ANIMAL(i).pregnancy == 0) {
                    count++;
                }
                if (count == wanted) {
                    found = i;
                    break;
                }
            }
        }
        break;
    case SHOP_LIVESTOCK_SHEEP:
        for (i = 0; i < MAX_LIVESTOCK; i++) {
            if (ANIMAL(i).actor.kind == ITEM_SHEEP) {
                if (filter == 0) {
                    count++;
                } else if (filter == 1 && ANIMAL(i).daysSinceShearing >= SHEARING_INTERVAL_DAYS
                           && ANIMAL(i).daysSick == 0 && ANIMAL(i).pregnancy == 0) {
                    count++;
                }
                if (count == wanted) {
                    found = i;
                    break;
                }
            }
        }
        break;
    }
    if (wanted == -1) {
        return count;
    }
    return found;
}

/* Start the paid-service dialogue on its day check, with the first accepted offer. */
static s32 ending_offer_begin(void) {
    shop_fee_dialogue_step = SHOP_FEE_CHECK_DAY;
    shop_fee_offer = ending_offer_find();
    shop_dialogue_tick_index = 0;
    shop_result_update(0, -1, 0);
    return 1;
}

/* The farm's paid-service dialogue, a state machine on shop_fee_dialogue_step: offer it (the
 * Saturday message when an option is set, else the closed one), take the choice,
 * charge 10 when the player can pay, confirm and close. Returns 1 when done (state 0). */
static s32 shop_fee_step(void)
{
    s32 message;
    s32 choice;

    switch (shop_fee_dialogue_step) {
    case SHOP_FEE_END:
    default:
        shop_dialogue_tick_index = 0;
        menu_begin_close();
        return 1;
    case SHOP_FEE_CHECK_DAY:
        if (game_state.progress.clock.date.weekday != WEEKDAY_SATURDAY || shop_fee_offer == -1) {
            message = 0x11A;
            shop_fee_dialogue_step = SHOP_FEE_MESSAGE_CLOSED;
        } else {
            message = 0x11B;
            shop_fee_dialogue_step = SHOP_FEE_OFFER_WAIT;
            shop_menu_disable_input();
            shop_dialogue_tick_index = SHOP_TICK_GOLD_PANEL_REDRAW;
        }
        menu_request_open(2, message);
        return 0;
    case SHOP_FEE_OFFER_WAIT:
        if (menu_get_phase() == TEXT_WINDOW_END_WAIT) {
            shop_fee_dialogue_step = SHOP_FEE_OFFER_ANSWER;
        }
        return 0;
    case SHOP_FEE_OFFER_ANSWER:
        switch (ending_offer_answer()) {
        case SHOP_ANSWER_YES:
            if (game_state.player.gold >= 10) {
                estate_gold_change(-10, 1);
                shop_fee_dialogue_step = SHOP_FEE_PAY_WAIT;
                message = 0x11C;
            } else {
                shop_fee_dialogue_step = SHOP_FEE_MESSAGE_CLOSED;
                message = 0x4F;
            }
            menu_begin_close();
            menu_request_open(2, message);
            break;
        case SHOP_ANSWER_NO:
            shop_fee_dialogue_step = SHOP_FEE_END;
            break;
        }
        return 0;
    case SHOP_FEE_PAY_WAIT:
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            shop_fee_dialogue_step = SHOP_FEE_PAY_OFFER;
        }
        return 0;
    case SHOP_FEE_PAY_OFFER:
        message = shop_fee_offer + 0x11D;
        shop_fee_dialogue_step = SHOP_FEE_PAY_OFFER_END;
        shop_menu_disable_input();
        menu_request_open(2, message);
        return 0;
    case SHOP_FEE_PAY_OFFER_END:
        if (menu_get_phase() == TEXT_WINDOW_END_WAIT) {
            shop_fee_dialogue_step = SHOP_FEE_BUY_ANSWER;
        }
        return 0;
    case SHOP_FEE_BUY_ANSWER:
        choice = ending_offer_answer();
        switch (choice) {
        case SHOP_ANSWER_YES:
            message = 0x4F;
            if (ending_offer_buy(shop_fee_offer) == choice) {
                message = 0x126;
            }
            shop_fee_dialogue_step = SHOP_FEE_MESSAGE_CLOSED;
            menu_begin_close();
            menu_request_open(2, message);
            break;
        case SHOP_ANSWER_NO:
            shop_fee_dialogue_step = SHOP_FEE_MESSAGE_CLOSED;
            menu_begin_close();
            menu_request_open(2, 0x127);
            break;
        }
        return 0;
    case SHOP_FEE_MESSAGE_CLOSED:
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            shop_fee_dialogue_step = SHOP_FEE_END;
        }
        return 0;
    }
}

/* The index of the first offer the player accepted (ENDING_OFFER_ACCEPTED), or -1. */
static s32 ending_offer_find(void) {
    s32 i;
    u8 *p;
    i = 0;
    p = game_state.ending.offers;
    while (i < 9) {
        if (*p == ENDING_OFFER_ACCEPTED) return i;
        i++;
        p++;
    }
    return -1;
}

/* Mute the menu sound and stop the text input. */
static void shop_menu_disable_input(void) {
    menu_set_sound_enabled(0);
    text_set_input(0);
}

/* The yes/no answer to the ending offer, read once its message is at the end wait (0 before that). */
static s32 ending_offer_answer(void) {
    if (menu_get_phase() == TEXT_WINDOW_END_WAIT) {
        return shop_result_update(&current_frame->otTags[0][10], 131, 72);
    }
    return 0;
}

static unsigned short shop_option_items[10] = {
    0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x0067, 0x0000,
};

/* Buy ending offer `option` (0 to 8): pay its item price, mark it paid and set event flag
 * 0x67 + option. Returns 1 when bought, 0 when it is out of range or the player cannot pay. */
static s32 ending_offer_buy(u32 option) {
    s32 cost;
    FarmState *farm;

    if (option >= 9) return 0;
    cost = item_buy_price(shop_option_items[option]);
    farm = &game_state;
    if (farm->player.gold >= cost) {
        estate_gold_change(-cost, 1);
        farm->ending.offers[option] = ENDING_OFFER_PAID;
        event_flag_set((option + 0x67) & 0xFFFF, 1);
        return 1;
    }
    return 0;
}

/* Set up the shop menu: five food entries between 6:00 and 17:59, three drinks
 * otherwise, then the water entry, the request and the message list. */
static s32 shop_menu_setup_food_or_drinks(void)
{
    FarmMenu *menu;
    u16 **text;
    s16 *entryValue;
    u8 *entryFlag;
    s16 *label46;
    u8 inRange;
    FarmMenu *shown;

    inRange = (u32)((u16)game_state.progress.clock.time.hm.hour - 6) < 12;
    menu = &shop_choice_menu;
    shop_menu_clear(menu);
    text = menu->text;
    entryValue = menu->entryValue;
    entryFlag = menu->entryFlag;
    label46 = menu->label46;
    if (!inRange) {

        menu->columns[0] = 0xF;
        menu->columns[1] = 0x14;
        ADD_ENTRY(LOCALIZED_NAME(text_bank.names.grape_liquor), ITEM_WILD_GRAPE_WINE, 0, 0x112);
        ADD_ENTRY(LOCALIZED_NAME(text_bank.names.pineapple_juice), 0x134, 0, 0x113);
        ADD_ENTRY(LOCALIZED_NAME(text_bank.names.milk), ITEM_MILK, 0, 0x114);
    } else {

        menu->columns[0] = 0xA;
        menu->columns[1] = 0xF;
        ADD_ENTRY(LOCALIZED_NAME(text_bank.names.set_meal), ITEM_SET_MEAL, 0, 0x10D);
        ADD_ENTRY(LOCALIZED_NAME(text_bank.names.salad), ITEM_SALAD_130, 0, 0x10E);
        ADD_ENTRY(LOCALIZED_NAME(text_bank.names.apple_pie), ITEM_APPLE_PIE_131, 0, 0x10F);
        ADD_ENTRY(LOCALIZED_NAME(text_bank.names.cheesecake), ITEM_CHEESE_CAKE, 0, 0x110);
        ADD_ENTRY(LOCALIZED_NAME(text_bank.names.cookie), ITEM_COOKIE_133, 0, 0x111);
    }
    shop_dialogue_tick_index = 0;
    shown = &shop_choice_menu;
    shop_menu_request.first[0] = SHOP_NO_MESSAGE;
    shop_menu_request.first[1] = SHOP_NO_MESSAGE;
    shop_menu_request.second[0] = 2;
    shop_menu_request.second[1] = 0x116;
    shop_menu_request.count = 4;
    shop_menu_request.menu = shown;
    *text = LOCALIZED_NAME(text_bank.names.water);
    *entryValue = ITEM_WATER;
    *entryFlag = 0;
    *label46 = 0x115;
    shown->selected = 0;
    if (!inRange) {
        shop_menu_messages[0] = 0x10C;
    } else {
        shop_menu_messages[0] = 0x10B;
    }
    shop_menu_messages[1] = 0x4F;
    shop_menu_messages[2] = 0x117;
    shop_menu_messages[3] = 0x118;
    shop_menu_messages[4] = 0x119;
    shop_goods_step = SHOP_GOODS_START;
    shop_goods_return_step = SHOP_GOODS_LEAVE;
    return 1;
}

/* Set up the five-entry snack menu, with its request and message list. */
static s32 shop_menu_setup_snacks(void)
{
    FarmMenu *menu;
    u16 **text;
    s16 *entryValue;
    u8 *entryFlag;
    s16 *label46;

    menu = &shop_choice_menu;
    shop_menu_clear(menu);
    menu->columns[0] = 0xD;
    menu->columns[1] = 0x12;
    menu->selected = 0;
    text = menu->text;
    entryValue = menu->entryValue;
    entryFlag = menu->entryFlag;
    label46 = menu->label46;
    ADD_ENTRY(LOCALIZED_NAME(text_bank.names.snowcone), ITEM_SNOWCONE, 0, 0x12C);
    ADD_ENTRY(LOCALIZED_NAME(text_bank.names.roasted_corn), ITEM_ROASTED_CORN, 0, 0x129);
    ADD_ENTRY(LOCALIZED_NAME(text_bank.names.pasta), ITEM_PASTA_136, 0, 0x12A);
    ADD_ENTRY(LOCALIZED_NAME(text_bank.names.pizza), ITEM_PIZZA_137, 0, 0x12B);
    ADD_ENTRY(LOCALIZED_NAME(text_bank.names.water), ITEM_WATER, 0, 0x12D);
    shop_dialogue_tick_index = 0;
    shop_menu_request.first[0] = SHOP_NO_MESSAGE;
    shop_menu_request.first[1] = SHOP_NO_MESSAGE;
    shop_menu_request.second[0] = 2;
    shop_menu_request.second[1] = 0x12E;
    shop_menu_request.count = 4;
    shop_menu_request.menu = menu;
    shop_menu_messages[0] = 0x128;
    shop_menu_messages[1] = 0x4F;
    shop_menu_messages[2] = 0x12F;
    shop_menu_messages[3] = 0x130;
    shop_menu_messages[4] = 0x131;
    shop_goods_step = SHOP_GOODS_START;
    shop_goods_return_step = SHOP_GOODS_LEAVE;
    return 1;
}

/* The item chosen from the shop menu. */
static s32 shop_selected_item = 0;

/* The shop menu dialogue on shop_goods_step: run the menu, price the chosen item and take
 * payment, show the result message, then hand the item over on leaving. Keeps
 * FarmState.player.stateFlags bit PLAYER_FLAG_SCRIPTED set while it runs; returns 1 when done. */
static s32 shop_goods_run(void)
{
    s32 price;
    s32 message;

    game_state.player.scriptHeld = 0;
    game_state.player.scriptPressed = 0;
    game_state.player.stateFlags |= PLAYER_FLAG_SCRIPTED;
    switch (shop_goods_step) {
    case SHOP_GOODS_START:
        shop_menu_message_select(0);
        shop_goods_step = SHOP_GOODS_MESSAGE_CLOSED;
        shop_goods_return_step = SHOP_GOODS_SETUP;
        shop_selected_item = 0;
        return 0;
    case SHOP_GOODS_SETUP:
        shop_dialogue_tick_index = SHOP_TICK_GOLD_CHOICE;
        shop_goods_step = SHOP_GOODS_CHOOSE;
        shop_step = SHOP_MENU_OPEN_MESSAGE;
        shop_return_step = SHOP_MENU_OPEN_MESSAGE;
        return 0;
    case SHOP_GOODS_CHOOSE:
        switch (shop_menu_open_choose(&shop_step, SHOP_MENU_OPEN_MESSAGE, shop_menu_request)) {
        case SHOP_MENU_RESULT_NONE:
            break;
        case SHOP_MENU_RESULT_CHOSEN:
            shop_selected_item = shop_menu_selected_value(shop_choice_menu);
            price = item_buy_price(shop_selected_item);
            if (shop_can_afford(price) == 1) {
                estate_gold_change(-price, 1);
                shop_goods_step = SHOP_GOODS_MESSAGE_CLOSED;
                shop_goods_return_step = SHOP_GOODS_PURCHASED;
                shop_dialogue_tick_index = 0;
                message = 3;
            } else {
                shop_selected_item = 0;
                shop_goods_step = SHOP_GOODS_MESSAGE_CLOSED;
                shop_goods_return_step = SHOP_GOODS_NO_MONEY_RETURN;
                message = 1;
            }
            shop_menu_message_select(message);
            break;
        case SHOP_MENU_RESULT_UNUSED:
            break;
        case SHOP_MENU_RESULT_CANCELLED:
            shop_goods_step = SHOP_GOODS_MESSAGE_CLOSED;
            shop_goods_return_step = SHOP_GOODS_LEAVE;
            shop_dialogue_tick_index = 0;
            break;
        }
        return 0;
    case SHOP_GOODS_NO_MONEY_RETURN:
        shop_menu_message_select(2);
        shop_goods_step = SHOP_GOODS_MESSAGE_CLOSED;
        shop_goods_return_step = SHOP_GOODS_SETUP;
        return 0;
    case SHOP_GOODS_PURCHASED:
        if (shop_menu_messages[3] != 0x118) {
            if (shop_selected_item == ITEM_WATER) {
                shop_menu_messages[4] = 0x200;
            } else {
                shop_menu_messages[4] = 0x131;
            }
        }
        shop_menu_message_select(4);
        shop_goods_step = SHOP_GOODS_MESSAGE_CLOSED;
        shop_goods_return_step = SHOP_GOODS_LEAVE;
        return 0;
    case SHOP_GOODS_MESSAGE_CLOSED:
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            shop_goods_step = shop_goods_return_step;
        }
        return 0;
    case SHOP_GOODS_LEAVE:
    default:
        if (shop_selected_item != 0) {
            if (game_state.itemInfo[shop_selected_item].itemFlags & 0x400) {
                player_receive_item(&game_state.player, shop_selected_item, 3);
            } else {
                player_receive_item(&game_state.player, shop_selected_item, 2);
            }
        }
        game_state.player.stateFlags &= ~PLAYER_FLAG_SCRIPTED;
        return 1;
    }
}

static void shop_menu_message_select(s32 index) {
    s32 idx = index;
    if (idx < 0) {
        idx = 0;
    }
    if (idx >= SHOP_MENU_MESSAGE_COUNT) {
        idx = SHOP_MENU_MESSAGE_COUNT - 1;
    }
    menu_begin_close();
    menu_request_open(2, shop_menu_messages[idx]);
}

static s32 shop_enlarge_name_offsets[5] = {offsetof(TextNames, enlarge_hen_house), offsetof(TextNames, enlarge_own_house), offsetof(TextNames, enlarge_barn), offsetof(TextNames, enlarge_own_house), offsetof(TextNames, hothouse)};

/* Set up the two silo menus: the first lists the choices for the first free slot
 * (4 when none is free), the second is a single entry. */
static s32 shop_lumber_menu_setup(void)
{
    FarmMenu *choices;
    FarmMenu *single;
    s32 found;
    s32 slot;
    s32 base;

    found = shop_first_clear_flag();
    shop_lumber_slot = found;
    slot = 4;
    if (found != -1) {
        slot = found;
    }

    choices = &shop_choice_menu;
    shop_menu_clear(choices);
    choices->entryValue[0] = 0;
    choices->entryFlag[0] = 1;
    choices->label46[0] = 0x133;
    choices->entryFlag[1] = 1;
    choices->label46[1] = slot + 0x134;
    choices->columns[0] = 0x11;
    choices->columns[1] = 0x11;
    choices->entryValue[1] = 0;
    choices->selected = 0;

    shop_lumber_request.second[0] = 2;
    shop_lumber_request.second[1] = 0x139;
    shop_lumber_request.third[0] = 2;
    shop_lumber_request.first[0] = SHOP_NO_MESSAGE;
    shop_lumber_request.first[1] = SHOP_NO_MESSAGE;
    shop_lumber_request.third[1] = 0x141;
    shop_lumber_request.count = 4;
    shop_lumber_request.menu = choices;

    base = text_bank_starts.names;
    choices->text[0] = LOCALIZED_NAME(text_bank.names.buy_lumber);
    choices->text[1] = TEXT_AT_OFFSET(base + shop_enlarge_name_offsets[slot]);
    shop_menu_header.w4 = LOCALIZED_NAME(text_bank.names.unit);
    shop_menu_header.ha = item_buy_price(ITEM_LUMBER);
    shop_menu_header.h8 = 1;

    single = &shop_confirm_menu;
    shop_menu_clear(single);
    single->label46[0] = 0x133;
    single->entryValue[0] = ITEM_LUMBER;
    single->entryFlag[0] = 1;
    single->selected = 0;

    shop_dialogue_tick_index = 0;
    shop_lumber_step = SHOP_LUMBER_START;
    shop_lumber_return_step = SHOP_LUMBER_MESSAGE_CLOSED;
    return 1;
}

/* The price and amount of each silo slot. */
static s16 shop_silo_offers[5][2] = {
    {5000, 420}, {4700, 370}, {6800, 500}, {10000, 750}, {30000, 580},
};

/* The pending reply: message id (-1 for none) and the step after it. */
static s32 shop_pending_message = -1;

static s32 shop_pending_return_step = SHOP_LUMBER_LEAVE;

/* The lumber-silo dialogue: a choose menu, then either a count menu paid per unit into
 * the lumber store, or a confirm for the slot's deal. Returns 1 when the dialogue ends. */
static s32 shop_lumber_deal_step(void)
{
    s32 slot;

    switch (shop_lumber_step) {
    case SHOP_LUMBER_START:
        shop_lumber_step = SHOP_LUMBER_MESSAGE_CLOSED;
        shop_lumber_return_step = SHOP_LUMBER_SETUP;
        menu_request_open(2, 0x132);
        return 0;
    case SHOP_LUMBER_SETUP:
        shop_lumber_step = SHOP_LUMBER_CHOOSE;
        shop_step = SHOP_MENU_OPEN_MESSAGE;
        shop_return_step = SHOP_MENU_OPEN_MESSAGE;
        shop_lumber_request.menu = &shop_choice_menu;
        return 0;
    case SHOP_LUMBER_CHOOSE:
        switch (shop_menu_open_choose(&shop_step, SHOP_MENU_OPEN_MESSAGE, shop_lumber_request)) {
        case SHOP_MENU_RESULT_NONE:
            break;
        case SHOP_MENU_RESULT_CHOSEN:
            shop_lumber_step = SHOP_LUMBER_MESSAGE_CLOSED;
            shop_lumber_return_step = SHOP_LUMBER_SHOW_MESSAGE;
            if (shop_menu_last_entry_index(shop_choice_menu) == 0) {
                if (game_state.progress.estate.lumber >= SHOP_LUMBER_MAX) {
                    shop_pending_message = 0x13A;
                    shop_pending_return_step = SHOP_LUMBER_SETUP;
                } else {
                    shop_pending_message = 0x13B;
                    shop_pending_return_step = SHOP_LUMBER_CONFIRM_SETUP;
                }
            } else if (event_flag_get(EVENT_FLAG_BUILD_IN_PROGRESS) == 1) {
                shop_pending_message = 0x144;
                shop_pending_return_step = SHOP_LUMBER_SETUP;
            } else if (shop_lumber_slot == -1) {
                shop_pending_message = 0x146;
                shop_pending_return_step = SHOP_LUMBER_SETUP;
            } else {
                shop_pending_message = shop_lumber_slot + 0x13C;
                shop_pending_return_step = SHOP_LUMBER_ORDER_ASK;
            }
            menu_begin_close();
            break;
        case SHOP_MENU_RESULT_UNUSED:
            break;
        case SHOP_MENU_RESULT_CANCELLED:
            shop_lumber_step = SHOP_LUMBER_MESSAGE_CLOSED;
            shop_lumber_return_step = SHOP_LUMBER_LEAVE;
            shop_dialogue_tick_index = 0;
            break;
        }
        return 0;
    case SHOP_LUMBER_CONFIRM_SETUP:
        shop_lumber_step = SHOP_LUMBER_QUANTITY;
        shop_step = SHOP_MENU_OPEN_MESSAGE;
        shop_return_step = SHOP_MENU_OPEN_MESSAGE;
        shop_lumber_request.menu = &shop_confirm_menu;
        return 0;
    case SHOP_LUMBER_QUANTITY:
        switch (shop_menu_choose(&shop_step, SHOP_MENU_OPEN_MESSAGE, SHOP_TICK_PANEL_SUBMIT, shop_lumber_request, &shop_menu_header)) {
        case SHOP_MENU_RESULT_NONE:
            break;
        case SHOP_MENU_RESULT_CHOSEN:
            shop_lumber_step = SHOP_LUMBER_MESSAGE_CLOSED;
            shop_lumber_return_step = SHOP_LUMBER_SHOW_MESSAGE;
            if (shop_can_afford(shop_menu_header.h8 * shop_menu_header.ha) == 1) {
                if (shop_menu_header.h8 + game_state.progress.estate.lumber <= SHOP_LUMBER_MAX) {
                    estate_gold_change(-(shop_menu_header.h8 * shop_menu_header.ha), 1);
                    shop_pending_message = 0x143;
                    shop_pending_return_step = SHOP_LUMBER_LEAVE;
                    game_state.progress.estate.lumber += shop_menu_header.h8;
                } else {
                    shop_pending_message = 0x142;
                    shop_pending_return_step = SHOP_LUMBER_CONFIRM_SETUP;
                }
            } else {
                shop_pending_message = 0x4F;
                shop_pending_return_step = SHOP_LUMBER_CONFIRM_SETUP;
            }
            menu_begin_close();
            break;
        case SHOP_MENU_RESULT_UNUSED:
            break;
        case SHOP_MENU_RESULT_CANCELLED:
            shop_lumber_step = SHOP_LUMBER_MESSAGE_CLOSED;
            shop_lumber_return_step = SHOP_LUMBER_SETUP;
            break;
        }
        return 0;
    case SHOP_LUMBER_ORDER_ASK:
        shop_result_update(0, -1, 0);
        menu_set_sound_enabled(0);
        text_set_input(0);
        menu_request_open(2, 0x5F);
        shop_lumber_step = SHOP_LUMBER_ORDER_MESSAGE_END;
        return 0;
    case SHOP_LUMBER_ORDER_MESSAGE_END:
        if (menu_get_phase() == TEXT_WINDOW_END_WAIT) {
            shop_lumber_step = SHOP_LUMBER_ORDER_ANSWER;
        }
        return 0;
    case SHOP_LUMBER_ORDER_ANSWER:
        switch (shop_result_update(&current_frame->otTags[0][11], 224, 128)) {
        case SHOP_ANSWER_YES:
            slot = 4;
            if (shop_lumber_slot != -1) {
                slot = shop_lumber_slot;
            }
            if (shop_can_afford(shop_silo_offers[slot][0]) == 1) {
                if (game_state.progress.estate.lumber >= shop_silo_offers[slot][1]) {
                    shop_pending_message = 0x144;
                    shop_lumber_step = SHOP_LUMBER_MESSAGE_CLOSED;
                    shop_lumber_return_step = SHOP_LUMBER_SHOW_MESSAGE;
                    shop_pending_return_step = SHOP_LUMBER_LEAVE;
                    game_state.progress.estate.lumber -= shop_silo_offers[slot][1];
                    estate_gold_change(-shop_silo_offers[slot][0], 1);
                    event_flag_set(EVENT_FLAG_BUILD_IN_PROGRESS, 1);
                    milestone_arm_countdown();
                } else {
                    shop_pending_message = 0x145;
                    shop_lumber_step = SHOP_LUMBER_MESSAGE_CLOSED;
                    shop_lumber_return_step = SHOP_LUMBER_SHOW_MESSAGE;
                    shop_pending_return_step = SHOP_LUMBER_SETUP;
                }
            } else {
                shop_pending_message = 0x4F;
                shop_lumber_step = SHOP_LUMBER_MESSAGE_CLOSED;
                shop_lumber_return_step = SHOP_LUMBER_SHOW_MESSAGE;
                shop_pending_return_step = SHOP_LUMBER_SETUP;
            }
            menu_begin_close();
            break;
        case SHOP_ANSWER_NO:
            menu_begin_close();
            shop_pending_message = 0x141;
            shop_lumber_step = SHOP_LUMBER_MESSAGE_CLOSED;
            shop_lumber_return_step = SHOP_LUMBER_SHOW_MESSAGE;
            shop_pending_return_step = SHOP_LUMBER_SETUP;
            break;
        }
        return 0;
    case SHOP_LUMBER_MESSAGE_CLOSED:
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            shop_lumber_step = shop_lumber_return_step;
        }
        return 0;
    case SHOP_LUMBER_SHOW_MESSAGE:
        if (shop_pending_message != -1) {
            menu_request_open(2, shop_pending_message);
        }
        shop_lumber_step = SHOP_LUMBER_MESSAGE_CLOSED;
        shop_lumber_return_step = shop_pending_return_step;
        return 0;
    case SHOP_LUMBER_LEAVE:
    default:
        shop_dialogue_tick_index = 0;
        return 1;
    }
}

/* The first of the five building-level flags (0x3A to 0x3E: coop, house, barn, second house,
 * greenhouse) that is still clear, or -1. */
static s32 shop_first_clear_flag(void) {
    s32 i;
    for (i = 0; i < 5; i++) {
        if (event_flag_get(EVENT_FLAG_COOP_LEVEL_1 + i) == 0) return i;
    }
    return -1;
}

/* Run one step of the lumber quantity dialogue whose step is *step - base: open the
 * message window, show the lumber name and unit price, then take a quantity (up to
 * SHOP_FISH_FOOD_MAX, into header->h8) and close. Sets the dialogue tick to `next`.
 * Returns -1 when the step is not this dialogue's, 1 when the quantity was confirmed,
 * 3 when finished, else 0. */
s32 shop_menu_choose(s32 *step, s32 base, s32 next, FarmMenuRequest request, ShopMenuHeader *header) {
    s32 result;
    u32 index;
    s16 price;
    FarmMenu *menu;

    index = *step - base;
    if (index > SHOP_MENU_DONE) {
        return -1;
    }
    result = 0;
    menu = request.menu;
    switch (index) {
    case SHOP_MENU_OPEN_MESSAGE:
        shop_set_step_offset(SHOP_MENU_PROMPT, SHOP_MENU_OPEN_MESSAGE, base);
        break;
    case SHOP_MENU_MESSAGE_CLOSE:
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            shop_restore_step();
        }
        break;
    case SHOP_MENU_MESSAGE_END:
        if (menu_get_phase() == TEXT_WINDOW_END_WAIT) {
            shop_restore_step();
        }
        break;
    case SHOP_MENU_PROMPT:
        menu_set_sound_enabled(0);
        text_set_input(0);
        menu_request_open(2, 0x4D);
        price = item_buy_price(shop_menu_selected_value(*menu));
        shop_price_label_set(header, LOCALIZED_NAME(text_bank.names.lumber), 1, price, 0);
        shop_dialogue_tick_index = next;
        shop_set_step_offset(SHOP_MENU_MESSAGE_END, SHOP_MENU_INPUT, base);
        break;
    case SHOP_MENU_INPUT:
        switch (shop_quantity_input(SHOP_FISH_FOOD_MAX, &header->h8)) {
        case SHOP_QUANTITY_NONE:
            break;
        case SHOP_QUANTITY_CONFIRM:
            menu_begin_close();
            result = 1;
            break;
        case SHOP_QUANTITY_INFO:
            menu_begin_close();
            menu_request_open(2, shop_menu_selected_label(*menu));
            shop_set_step_offset(SHOP_MENU_MESSAGE_CLOSE, SHOP_MENU_PROMPT, base);
            break;
        case SHOP_QUANTITY_CANCEL:
            menu_begin_close();
            menu_request_open(request.third[0], request.third[1]);
            shop_set_step_offset(SHOP_MENU_MESSAGE_CLOSE, SHOP_MENU_DONE, base);
            break;
        }
        break;
    case SHOP_MENU_DONE:
        result = 3;
        break;
    }
    return result;
}

/* The callback tables the farm phase dispatches through: by screen id, and by request. */
s32 (*const shop_dialogue_tick_handlers[8])() = {
    0, shop_menu_count_draw, shop_gold_panel_step, shop_gold_panel_redraw_step,
    shop_gold_choice_step, shop_panel_submit, shop_panel_submit_both, shop_medals_choice_step,
};
s32 (*const shop_screen_run_handlers[29])() = {
    0, 0, shop_buy_stock_step, shop_buy_item_step,
    shop_grant_rice_ball, shop_grant_bread, shop_grant_oil, shop_grant_flour,
    shop_grant_curry_powder, shop_grant_rucksack_upgrade, shop_basket_step, shop_result_request,
    shop_result_request_ready, shop_result_request_alt, shop_result_request_std, shop_offer_step,
    shop_poultry_step, shop_barn_step, shop_stock_step, shop_pay_step,
    shop_doctor_step, 0, shop_goods_run, shop_goods_run,
    shop_lumber_deal_step, shop_greenhouse_step, shop_medal_step, shop_fee_step,
    shop_plot_sale_step,
};
void (*const shop_screen_start_handlers[1])(void) = {0};
s32 (*const shop_phase_complete_handlers[28])() = {
    shop_dialogue_finish, shop_fish_food_menu_setup, shop_dialogue_03_start, shop_start_default,
    shop_start_default, shop_start_default, shop_start_default, shop_start_default,
    shop_start_default, shop_start_default, shop_seed_menu_setup_a, shop_seed_menu_setup_b,
    shop_seed_menu_setup_c, shop_seed_menu_setup_d, shop_dialogue_0f_start, shop_chicken_menu_setup,
    shop_fodder_menu_setup, shop_tool_upgrade_menu_setup, shop_wine_menu_setup, shop_medicine_menu_setup,
    0, shop_menu_setup_food_or_drinks, shop_menu_setup_snacks, shop_lumber_menu_setup,
    shop_greenhouse_start, shop_goods_menu_setup, ending_offer_begin, shop_plot_sale_start,
};

/* Run the farm phase callbacks and report whether the phase completed. */
s32 shop_phase_run(void) {
    s32 completed;
    s32 result;

    completed = 1;
    if (shop_screen_id != 0) {
        if (shop_screen_run_handlers[shop_screen_id] != 0) {
            completed = 0;
            result = shop_screen_run_handlers[shop_screen_id]();
            if (result != 0) {
                completed = 1;
                shop_phase_complete_handlers[0]();
            }
        }
        if (shop_dialogue_tick_handlers[shop_dialogue_tick_index] != 0) {
            shop_dialogue_tick_handlers[shop_dialogue_tick_index](shop_dialogue_tick_handlers);
        }
    }
    return completed;
}

/* Dispatch a farm request. SHOP_DIALOGUE_ITEM_16 first sets event mode 0xA, runs the triggers
 * and steps the event request until it is idle. */
s32 shop_request_dispatch(s32 arg) {
    s32 result;

    result = 1;
    if (shop_screen_id == 0 || arg == 1) {
        if (arg == SHOP_DIALOGUE_ITEM_16 && event_flag_get(EVENT_FLAG_REQUEST_BUSY) == 0) {
            event_trigger_mode_set(0xA);
            event_triggers_run();
            event_request_step();
            if (event_flag_get(EVENT_FLAG_REQUEST_BUSY) == 1) {
                do {
                    if (game_state.requests.state == FARM_REQUEST_RUN) {
                        event_request_step();
                        break;
                    }
                    event_request_step();
                } while (event_flag_get(EVENT_FLAG_REQUEST_BUSY) != 0);
            }
            if (event_flag_get(EVENT_FLAG_REQUEST_BUSY) == 1) {
                return 0;
            }
        }
        result = 0;
        shop_screen_id = arg;
        if (shop_screen_start_handlers[arg] != 0) {
            shop_screen_start_handlers[arg]();
        }
    }
    return result;
}

static s16 shop_short_panel_state[2] = {0x0000, 0x0000};

/* Draw the short yes/no panel at (x, y) and report the answer: 0 still open, 1 yes, 2 no.
 * x -1 puts the cursor back on yes and x -2 on no; both draw nothing. */
s32 shop_result_update(u_long *ot, s16 x, s16 y) {
    if (x == -1) {
        shop_short_panel_state[0] = 0;
        return 0;
    }
    if (x == -2) {
        shop_short_panel_state[0] = 1;
        return 0;
    }
    shop_draw_short_panel(ot, x, y, shop_short_panel_state[0]);
    if (shop_yes_no_input(shop_short_panel_state) == 1) {
        if (shop_short_panel_state[0] == 0) {
            shop_short_panel_state[0] = 0;
            return 1;
        }
        shop_short_panel_state[0] = 0;
        return 2;
    }
    return 0;
}

static FarmKeyValueEntry shop_key_values[14] = {
    0x0128, 0x000B, 0x0129, 0x000C, 0x012A, 0x000D, 0x00A5, 0x0006, 0x00A6, 0x0007,
    0x00A7, 0x0008, 0x0099, 0x0004, 0x009A, 0x0005, 0x0021, 0x000F, 0x0177, 0x0003,
    0x006D, 0x0009, 0x0064, 0x000A, 0x0084, 0x0002, 0x006E, 0x0009,
};

/* The value paired with `key` in shop_key_values, or 0 when the key is not there. */
s32 shop_key_value_lookup(s16 key) {
    FarmKeyValueEntry *p = shop_key_values;
    u32 i = 0;
    do {
        i++;
        if (p->key != key) {
            p++;
            continue;
        }
        return p->value;
    } while (i < 14);
    return 0;
}

s32 shop_dialogue_current(void) {
    return shop_screen_id;
}

s16 shop_livestock_lookup(s16 mode, s16 filter, s16 wanted) {
    return shop_livestock_find(mode, filter, wanted);
}

/* Ends the farm dialogue: screen 0, step 0, tick 0, and the header count back to 1. */
static s32 shop_dialogue_finish(void) {
    shop_screen_id = 0;
    shop_dialogue_done = 1;
    shop_step = 0;
    shop_dialogue_tick_index = 0;
    shop_menu_header.h8 = 1;
    return 1;
}

/* The fish-food buy dialogue's steps: the start (which hands over the cue), the offer, the
 * count, the buy, the finish that records it and takes the item, and the leave. */
enum ShopFishFoodStep {
    SHOP_FISH_FOOD_START = 0,
    SHOP_FISH_FOOD_ASK = 1,
    SHOP_FISH_FOOD_MESSAGE_CLOSED = 2,
    SHOP_FISH_FOOD_MESSAGE_END = 3,
    SHOP_FISH_FOOD_COUNT_ASK = 4,
    SHOP_FISH_FOOD_COUNT = 5,
    SHOP_FISH_FOOD_BUY = 6,
    SHOP_FISH_FOOD_FINISH = 7,
    SHOP_FISH_FOOD_LEAVE = 8,
    SHOP_FISH_FOOD_CARRY_CUE = 9,
    SHOP_FISH_FOOD_CARRY_WAIT = 10,
};

/* A buy dialogue for fish food (stock capped at 999): offer it (the full-stock message
 * when the stock is full), take an amount, check the money and the room, then pay, add
 * the stock and set the item's flag. Returns 1 at step 8. */
static s32 shop_buy_stock_step(void)
{
    s32 done;
    s32 choice;

    done = 0;
    game_state.player.scriptHeld = 0;
    game_state.player.scriptPressed = 0;
    game_state.player.stateFlags |= PLAYER_FLAG_SCRIPTED;
    switch (shop_step) {
    case SHOP_FISH_FOOD_START:
        shop_set_step(SHOP_FISH_FOOD_CARRY_CUE, SHOP_FISH_FOOD_START);
        estate_record_publish(ITEM_FISH_FOOD, 0);
        break;
    case SHOP_FISH_FOOD_CARRY_CUE:
    case SHOP_FISH_FOOD_CARRY_WAIT:
        if (shop_item_carry_step(&shop_step, SHOP_FISH_FOOD_CARRY_CUE, ITEM_FISH_FOOD_06C) != 0) {
            shop_set_step(SHOP_FISH_FOOD_ASK, SHOP_FISH_FOOD_START);
        }
        break;
    case SHOP_FISH_FOOD_ASK:
        if (game_state.progress.estate.fishFood >= SHOP_FISH_FOOD_MAX) {
            menu_request_open(2, 0xD1);
            shop_set_step(SHOP_FISH_FOOD_MESSAGE_CLOSED, SHOP_FISH_FOOD_FINISH);
        } else {
            menu_request_open(2, 0xD0);
            shop_set_step(SHOP_FISH_FOOD_MESSAGE_CLOSED, SHOP_FISH_FOOD_COUNT_ASK);
        }
        break;
    case SHOP_FISH_FOOD_MESSAGE_CLOSED:
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            shop_restore_step();
        }
        break;
    case SHOP_FISH_FOOD_MESSAGE_END:
        if (menu_get_phase() == TEXT_WINDOW_END_WAIT) {
            shop_restore_step();
        }
        break;
    case SHOP_FISH_FOOD_COUNT_ASK:
        menu_set_sound_enabled(0);
        text_set_input(0);
        menu_request_open(2, 0x4D);
        shop_set_step(SHOP_FISH_FOOD_MESSAGE_END, SHOP_FISH_FOOD_COUNT);
        break;
    case SHOP_FISH_FOOD_COUNT:
        shop_dialogue_tick_index = SHOP_TICK_COUNT;
        choice = shop_quantity_input(SHOP_FISH_FOOD_MAX, &shop_menu_header.h8);
        switch (choice) {
        case SHOP_QUANTITY_NONE:
            break;
        case SHOP_QUANTITY_CONFIRM:
            if (shop_can_afford(shop_menu_header.h8 * shop_menu_header.ha) == choice) {
                if (shop_menu_header.h8 + game_state.progress.estate.fishFood > SHOP_FISH_FOOD_MAX) {
                    menu_begin_close();
                    menu_request_open(2, 0xD2);
                    shop_set_step(SHOP_FISH_FOOD_MESSAGE_CLOSED, SHOP_FISH_FOOD_COUNT_ASK);
                } else {
                    menu_begin_close();
                    menu_request_open(2, 0xD3);
                    shop_set_step(SHOP_FISH_FOOD_MESSAGE_CLOSED, SHOP_FISH_FOOD_BUY);
                }
            } else {
                menu_begin_close();
                menu_request_open(2, 0x4F);
                shop_set_step(SHOP_FISH_FOOD_MESSAGE_CLOSED, SHOP_FISH_FOOD_COUNT_ASK);
            }
            break;
        case SHOP_QUANTITY_INFO:
            menu_begin_close();
            menu_request_open(2, 0x25B);
            shop_set_step(SHOP_FISH_FOOD_MESSAGE_CLOSED, SHOP_FISH_FOOD_COUNT_ASK);
            break;
        case SHOP_QUANTITY_CANCEL:
            menu_begin_close();
            menu_request_open(2, 0xBA);
            shop_set_step(SHOP_FISH_FOOD_MESSAGE_CLOSED, SHOP_FISH_FOOD_FINISH);
            break;
        }
        break;
    case SHOP_FISH_FOOD_BUY:
        estate_fish_food_add(FISH_FOOD_ADD_AMOUNT, shop_menu_header.h8);
        estate_gold_change(-(shop_menu_header.h8 * shop_menu_header.ha), 1);
        /* fall through */
    case SHOP_FISH_FOOD_FINISH:
        estate_record_publish(ITEM_FISH_FOOD, 1);
        event_item_can_take(ITEM_FISH_FOOD_06C, 1, 0, 1);
        shop_set_step(SHOP_FISH_FOOD_LEAVE, SHOP_FISH_FOOD_START);
        break;
    case SHOP_FISH_FOOD_LEAVE:
        game_state.player.stateFlags &= ~PLAYER_FLAG_SCRIPTED;
        done = 1;
        break;
    }
    return done;
}

/* The buy dialogue for the held item's steps: check it (the refusal and the price ask), the
 * message waits, the answer, the payment, and the item's gift show (SHOW, SHOW_WAIT, the
 * shared shop_item_show_step) before the leave. */
enum ShopBuyItemStep {
    SHOP_BUY_ITEM_START = 0,
    SHOP_BUY_ITEM_CHECK_HELD = 1,
    SHOP_BUY_ITEM_MESSAGE_CLOSED = 2,
    SHOP_BUY_ITEM_MESSAGE_END = 3,
    SHOP_BUY_ITEM_LEAVE = 4,
    SHOP_BUY_ITEM_ASK = 5,
    SHOP_BUY_ITEM_ANSWER = 6,
    SHOP_BUY_ITEM_PAY = 7,
    SHOP_BUY_ITEM_WAIT_IDLE = 8,
    SHOP_BUY_ITEM_SHOW = 10,
    SHOP_BUY_ITEM_SHOW_WAIT = 11,
};

/* The buy dialogue for the held item (in shop_buy_held_item, priced by item_buy_price):
 * refuses the gift (ITEM_GIFT) and any item item_is_pickup_not_tool rejects, then pays,
 * gives the item and waits for the player to be idle. Returns 1 at step 4. */
static s32 shop_buy_item_step(void)
{
    s32 done;
    s16 price;
    s32 choice;

    game_state.player.scriptHeld = 0;
    game_state.player.scriptPressed = 0;
    game_state.player.stateFlags |= PLAYER_FLAG_SCRIPTED;
    price = item_buy_price(ITEM_GIFT);
    done = 0;
    switch (shop_step) {
    case SHOP_BUY_ITEM_START:
        shop_buy_held_item = game_state.player.held[HELD_ITEM].id;
        shop_set_step(SHOP_BUY_ITEM_CHECK_HELD, SHOP_BUY_ITEM_START);
        /* fall through */
    case SHOP_BUY_ITEM_CHECK_HELD:
        if (game_state.player.stateFlags & PLAYER_FLAG_CARRY_ITEM) {
            if (shop_buy_held_item == ITEM_GIFT) {
                menu_request_open(2, 0xBC);
                shop_set_step(SHOP_BUY_ITEM_MESSAGE_CLOSED, SHOP_BUY_ITEM_LEAVE);
            } else if (item_is_pickup_not_tool(shop_buy_held_item) == 1) {
                menu_request_open(2, 0xB6);
                menu_set_sound_enabled(0);
                shop_set_step(SHOP_BUY_ITEM_MESSAGE_END, SHOP_BUY_ITEM_ASK);
            } else {
                menu_request_open(2, 0xB9);
                shop_set_step(SHOP_BUY_ITEM_MESSAGE_CLOSED, SHOP_BUY_ITEM_LEAVE);
            }
        } else {
            menu_request_open(2, 0xB8);
            shop_set_step(SHOP_BUY_ITEM_MESSAGE_CLOSED, SHOP_BUY_ITEM_LEAVE);
        }
        break;
    case SHOP_BUY_ITEM_MESSAGE_CLOSED:
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            shop_restore_step();
        }
        break;
    case SHOP_BUY_ITEM_MESSAGE_END:
        if (menu_get_phase() == TEXT_WINDOW_END_WAIT) {
            text_set_input(0);
            shop_restore_step();
        }
        break;
    case SHOP_BUY_ITEM_ASK:
        shop_dialogue_tick_index = SHOP_TICK_GOLD_PANEL;
        shop_set_step(SHOP_BUY_ITEM_ANSWER, SHOP_BUY_ITEM_START);
        shop_result_update(0, -1, 0);
        /* fall through */
    case SHOP_BUY_ITEM_ANSWER:
        choice = shop_result_update(&current_frame->otTags[0][11], 224, 128);
        switch (choice) {
        case SHOP_ANSWER_PENDING:
            break;
        case SHOP_ANSWER_YES:
            if (shop_can_afford(price) == choice) {
                menu_begin_close();
                menu_request_open(2, 0xBB);
                shop_set_step(SHOP_BUY_ITEM_MESSAGE_CLOSED, SHOP_BUY_ITEM_PAY);
            } else {
                menu_begin_close();
                menu_request_open(2, 0x4F);
                shop_set_step(SHOP_BUY_ITEM_MESSAGE_CLOSED, SHOP_BUY_ITEM_LEAVE);
            }
            break;
        case SHOP_ANSWER_NO:
            menu_begin_close();
            menu_request_open(2, 0xB7);
            shop_set_step(SHOP_BUY_ITEM_MESSAGE_CLOSED, SHOP_BUY_ITEM_LEAVE);
            break;
        }
        break;
    case SHOP_BUY_ITEM_PAY:
        shop_dialogue_tick_index = 0;
        estate_gold_change(-price, 1);
        event_item_can_take(shop_buy_held_item, 1, 0, 1);
        shop_set_step(SHOP_BUY_ITEM_WAIT_IDLE, SHOP_BUY_ITEM_LEAVE);
        break;
    case SHOP_BUY_ITEM_WAIT_IDLE:
        if (game_state.player.stateFlags & PLAYER_FLAG_IDLE_POSE) {
            shop_set_step(SHOP_BUY_ITEM_SHOW, SHOP_BUY_ITEM_START);
        }
        break;
    case SHOP_BUY_ITEM_SHOW:
    case SHOP_BUY_ITEM_SHOW_WAIT:
        if (shop_item_show_step(&shop_step, SHOP_BUY_ITEM_SHOW, shop_buy_held_item) != 0) {
            shop_set_step(SHOP_BUY_ITEM_LEAVE, SHOP_BUY_ITEM_START);
        }
        break;
    case SHOP_BUY_ITEM_LEAVE:
        game_state.player.stateFlags &= ~PLAYER_FLAG_SCRIPTED;
        done = 1;
        break;
    }
    return done;
}

static s32 shop_grant_rice_ball(void) {
    return shop_item_grant(ITEM_RICE_BALL, 2, 0x1DD, SHOP_GRANT_FLAG_ONLY);
}

static s32 shop_grant_bread(void) {
    return shop_item_grant(ITEM_BREAD, 2, 0x1DE, SHOP_GRANT_FLAG_ONLY);
}

static s32 shop_grant_oil(void) {
    return shop_item_grant(ITEM_OIL, 2, 0xD9, SHOP_GRANT_FLAG_ONLY);
}

static s32 shop_grant_flour(void) {
    return shop_item_grant(ITEM_FLOUR, 2, 0xDA, SHOP_GRANT_FLAG_ONLY);
}

static s32 shop_grant_curry_powder(void) {
    return shop_item_grant(ITEM_CURRY_POWDER, 2, 0xDB, SHOP_GRANT_FLAG_ONLY);
}

static s32 shop_grant_rucksack_upgrade(void) {
    u32 flags = game_state.progress.estate.statusFlags;
    if (flags & ESTATE_STATUS_NO_RUCKSACK) {
        return shop_item_grant(ITEM_RUCKSACK_LEVEL_2, 2, 0xD4, SHOP_GRANT_RUCKSACK);
    }
    if (flags & ESTATE_STATUS_RUCKSACK_UPGRADE_OPEN) {
        return shop_item_grant(ITEM_RUCKSACK_LEVEL_3, 2, 0x248, SHOP_GRANT_RUCKSACK);
    }
    return 1;
}

/* The basket gift: done when the basket is not due or is already held (its message 0xD6 when
 * held); otherwise it is granted, and the function returns no value. */
static s32 shop_basket_step(void) {
    if ((game_state.progress.estate.statusFlags & ESTATE_STATUS_NO_BASKET) == 0) return 1;
    if ((game_state.progress.estate.estateFlags & ESTATE_FLAG_BASKET) != 0) {
        menu_request_open(2, 0xD6);
        return 1;
    }
    shop_item_grant(ITEM_BASKET, 2, 0xD5, SHOP_GRANT_ITEM);
}

/* The spring result: without a greenhouse it runs only in spring (done at once otherwise); a
 * greenhouse changes the first message to 0x275. Then the stored-item buy dialogue. */
static s32 shop_result_request(void) {
    FarmBuyDialogue request;
    request.chosen[0] = 2;
    request.chosen[1] = 0xC0;
    request.stored[0] = 2;
    request.stored[1] = 0xBB;
    request.cancelled[0] = 2;
    request.cancelled[1] = 0xC2;
    request.menu.first[0] = 2;
    request.menu.first[1] = 0xBE;
    request.menu.second[0] = 2;
    request.menu.second[1] = 0xBF;
    request.menu.third[0] = 2;
    request.menu.third[1] = 0xBA;
    request.menu.count = 4;
    request.menu.menu = &shop_choice_menu;
    if (game_state.progress.estate.greenhouseLevel == 0) {
        if (game_state.progress.clock.date.season != SEASON_SPRING) {
            return 1;
        }
    } else {
        request.menu.first[0] = 2;
        request.menu.first[1] = 0x275;
    }
    return shop_buy_stored_item(request.chosen, request.stored, request.cancelled, request.menu);
}

/* The summer result: as shop_result_request, for summer, with its own first message (0x273). */
static s32 shop_result_request_ready(void) {
    FarmBuyDialogue request;
    request.chosen[0] = 2;
    request.chosen[1] = 0xC0;
    request.stored[0] = 2;
    request.stored[1] = 0xBB;
    request.cancelled[0] = 2;
    request.cancelled[1] = 0xC2;
    request.menu.first[0] = 2;
    request.menu.first[1] = 0x273;
    request.menu.second[0] = 2;
    request.menu.second[1] = 0xBF;
    request.menu.third[0] = 2;
    request.menu.third[1] = 0xBA;
    request.menu.count = 4;
    request.menu.menu = &shop_choice_menu;
    if (game_state.progress.estate.greenhouseLevel == 0) {
        if (game_state.progress.clock.date.season != SEASON_SUMMER) {
            return 1;
        }
    } else {
        request.menu.first[0] = 2;
        request.menu.first[1] = 0x275;
    }
    return shop_buy_stored_item(request.chosen, request.stored, request.cancelled, request.menu);
}

/* The fall result: as shop_result_request, for fall, with its own first message (0x274). */
static s32 shop_result_request_alt(void) {
    FarmBuyDialogue request;
    request.chosen[0] = 2;
    request.chosen[1] = 0xC0;
    request.stored[0] = 2;
    request.stored[1] = 0xBB;
    request.cancelled[0] = 2;
    request.cancelled[1] = 0xC2;
    request.menu.first[0] = 2;
    request.menu.first[1] = 0x274;
    request.menu.second[0] = 2;
    request.menu.second[1] = 0xBF;
    request.menu.third[0] = 2;
    request.menu.third[1] = 0xBA;
    request.menu.count = 4;
    request.menu.menu = &shop_choice_menu;
    if (game_state.progress.estate.greenhouseLevel == 0) {
        if (game_state.progress.clock.date.season != SEASON_FALL) {
            return 1;
        }
    } else {
        request.menu.first[0] = 2;
        request.menu.first[1] = 0x275;
    }
    return shop_buy_stored_item(request.chosen, request.stored, request.cancelled, request.menu);
}

/* The result for any season and greenhouse: the stored-item buy dialogue with its own messages. */
static s32 shop_result_request_std(void) {
    FarmBuyDialogue request;

    request.chosen[0] = 2;
    request.chosen[1] = 0x16E;
    request.stored[0] = 2;
    request.stored[1] = 0x170;
    request.cancelled[0] = 2;
    request.cancelled[1] = 0x171;
    request.menu.first[0] = 2;
    request.menu.first[1] = 0x164;
    request.menu.second[0] = 2;
    request.menu.second[1] = 0x16D;
    request.menu.third[0] = 2;
    request.menu.third[1] = 0x16F;
    request.menu.count = 4;
    request.menu.menu = &shop_choice_menu;
    return shop_buy_stored_item(request.chosen, request.stored, request.cancelled, request.menu);
}

/* The blue feather offer's steps: the lead-in message, the yes or no to buy it, the pay and
 * the feather, and the leave. */
enum ShopOfferStep {
    SHOP_OFFER_START = 0,
    SHOP_OFFER_MESSAGE_CLOSED = 2,
    SHOP_OFFER_MESSAGE_END = 3,
    SHOP_OFFER_ASK = 4,
    SHOP_OFFER_BUY = 6,
    SHOP_OFFER_LEAVE = 7,
    SHOP_OFFER_ANSWER = 8,
    SHOP_OFFER_LEAD_IN = 11,
    SHOP_OFFER_LEAD_IN_ALT = 12,
};

/* The blue feather offer (only while the affection offer is pending; returns 1 otherwise):
 * take the choice, check the room and the money, then pay, add the feather and clear the
 * offer. Returns 1 when finished. */
static s32 shop_offer_step(void)
{
    s32 done;
    s16 price;
    s32 choice;

    if (!(game_state.progress.estate.statusFlags & ESTATE_STATUS_AFFECTION_OVER_50000)) {
        return 1;
    }
    game_state.player.scriptHeld = 0;
    game_state.player.scriptPressed = 0;
    game_state.player.stateFlags |= PLAYER_FLAG_SCRIPTED;
    price = item_buy_price(ITEM_BLUE_FEATHER);
    done = 0;
    switch (shop_step) {
    case SHOP_OFFER_START:
        shop_set_step(SHOP_OFFER_LEAD_IN, SHOP_OFFER_START);
        break;
    case SHOP_OFFER_LEAD_IN:
    case SHOP_OFFER_LEAD_IN_ALT:
        menu_request_open(2, 0xDC);
        menu_set_sound_enabled(0);
        shop_set_step(SHOP_OFFER_MESSAGE_END, SHOP_OFFER_ASK);
        break;
    case SHOP_OFFER_MESSAGE_CLOSED:
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            shop_restore_step();
        }
        break;
    case SHOP_OFFER_MESSAGE_END:
        if (menu_get_phase() == TEXT_WINDOW_END_WAIT) {
            text_set_input(0);
            shop_restore_step();
        }
        break;
    case SHOP_OFFER_ASK:
        shop_dialogue_tick_index = SHOP_TICK_GOLD_PANEL_REDRAW;
        shop_set_step(SHOP_OFFER_ANSWER, SHOP_OFFER_START);
        shop_yes_no_prompt(0, -1, 0);
        /* fall through */
    case SHOP_OFFER_ANSWER:
        choice = shop_yes_no_prompt(&current_frame->otTags[0][11], 200, 128);
        switch (choice) {
        case SHOP_ANSWER_PENDING:
            break;
        case SHOP_ANSWER_YES:
            if (shop_item_room(ITEM_BLUE_FEATHER, 1) == 0) {
                menu_begin_close();
                menu_request_open(2, 0x54);
                shop_set_step(SHOP_OFFER_MESSAGE_CLOSED, SHOP_OFFER_LEAVE);
            } else if (shop_can_afford(price) == choice) {
                menu_begin_close();
                menu_request_open(2, 0xBB);
                shop_set_step(SHOP_OFFER_MESSAGE_CLOSED, SHOP_OFFER_BUY);
            } else {
                menu_begin_close();
                menu_request_open(2, 0x4F);
                shop_set_step(SHOP_OFFER_MESSAGE_CLOSED, SHOP_OFFER_LEAVE);
            }
            break;
        case SHOP_ANSWER_NO:
            menu_begin_close();
            menu_request_open(2, 0xBA);
            shop_set_step(SHOP_OFFER_MESSAGE_CLOSED, SHOP_OFFER_LEAVE);
            break;
        }
        break;
    case SHOP_OFFER_BUY:
        estate_record_publish(ITEM_BLUE_FEATHER, 0);
        estate_gold_change(-price, 1);
        event_item_store(ITEM_BLUE_FEATHER, 0, 1, 1);
        game_state.progress.estate.estateFlags |= ESTATE_FLAG_BIT21;
        game_state.progress.estate.statusFlags &= ~ESTATE_STATUS_AFFECTION_OVER_50000;
        game_state.player.stateFlags &= ~PLAYER_FLAG_SCRIPTED;
        done = 1;
        break;
    case SHOP_OFFER_LEAVE:
        game_state.player.stateFlags &= ~PLAYER_FLAG_SCRIPTED;
        done = 1;
        break;
    }
    return done;
}

/* The pay dialogue's steps: the choose menu (from 7, shared with the menu sub-steps), the
 * message waits, the alternative intro, the carry of the bought item, and the leave. */
enum ShopPayStep {
    SHOP_PAY_START = 0,
    SHOP_PAY_MESSAGE_CLOSED = 1,
    SHOP_PAY_CHOOSE_ALT_INTRO = 2,
    SHOP_PAY_LEAVE = 3,
    SHOP_PAY_CARRY_CUE = 4,
    SHOP_PAY_CARRY_WAIT = 5,
    SHOP_PAY_CHOOSE_MESSAGE = 7,
    SHOP_PAY_CHOOSE_MESSAGE_CLOSE = 8,
    SHOP_PAY_CHOOSE_MESSAGE_END = 9,
    SHOP_PAY_CHOOSE_PROMPT = 10,
    SHOP_PAY_CHOOSE_INPUT = 11,
    SHOP_PAY_CHOOSE_DONE = 12,
    SHOP_PAY_CHOOSE_AFTER_DONE = 13,
};

/* A buy dialogue: pick an item from the request's menu, pay for it when it is affordable
 * (or refuse), and wait for the item to be carried. Returns 1 at step 3. */
static s32 shop_pay_step(void)
{
    FarmMenuRequest request;
    s32 done;

    done = 0;
    request.first[0] = 2;
    request.first[1] = 0xB0;
    request.second[0] = 2;
    request.second[1] = 0xB1;
    request.count = 4;
    request.menu = &shop_choice_menu;
    game_state.player.scriptHeld = 0;
    game_state.player.scriptPressed = 0;
    game_state.player.stateFlags |= PLAYER_FLAG_SCRIPTED;
    switch (shop_step) {
    case SHOP_PAY_START:
        shop_set_step(SHOP_PAY_CHOOSE_MESSAGE, SHOP_PAY_START);
        break;
    case SHOP_PAY_MESSAGE_CLOSED:
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            shop_restore_step();
        }
        break;
    case SHOP_PAY_CHOOSE_ALT_INTRO:
        request.first[0] = 2;
        request.first[1] = 0xB5;
        shop_set_step(SHOP_PAY_CHOOSE_MESSAGE, SHOP_PAY_START);
        /* fall through */
    case SHOP_PAY_CHOOSE_MESSAGE:
    case SHOP_PAY_CHOOSE_MESSAGE_CLOSE:
    case SHOP_PAY_CHOOSE_MESSAGE_END:
    case SHOP_PAY_CHOOSE_PROMPT:
    case SHOP_PAY_CHOOSE_INPUT:
    case SHOP_PAY_CHOOSE_DONE:
    case SHOP_PAY_CHOOSE_AFTER_DONE:
        switch (shop_menu_open_choose(&shop_step, SHOP_PAY_CHOOSE_MESSAGE, request)) {
        case SHOP_MENU_RESULT_NONE:
            break;
        case SHOP_MENU_RESULT_CHOSEN:
            if (shop_can_afford(item_buy_price(shop_menu_selected_value(shop_choice_menu))) == 1) {
                estate_gold_change(-item_buy_price(shop_menu_selected_value(shop_choice_menu)), 1);
                menu_begin_close();
                menu_request_open(2, 0xB4);
                shop_set_step(SHOP_PAY_MESSAGE_CLOSED, SHOP_PAY_CARRY_CUE);
            } else {
                menu_begin_close();
                menu_request_open(2, 0x4F);
                shop_set_step(SHOP_PAY_MESSAGE_CLOSED, SHOP_PAY_CHOOSE_ALT_INTRO);
            }
            break;
        case SHOP_MENU_RESULT_CANCELLED:
            shop_set_step(SHOP_PAY_LEAVE, SHOP_PAY_START);
            break;
        }
        break;
    case SHOP_PAY_CARRY_CUE:
    case SHOP_PAY_CARRY_WAIT:
        if (shop_item_carry_step(&shop_step, SHOP_PAY_CARRY_CUE, shop_menu_selected_value(shop_choice_menu)) != 0) {
            shop_set_step(SHOP_PAY_LEAVE, SHOP_PAY_START);
        }
        break;
    case SHOP_PAY_LEAVE:
        done = 1;
        break;
    }
    return done;
}

/* The tool-upgrade steps, run from a base step: check the held ore and the price, confirm
 * twice, take the ore, then pay and take the upgraded tool. */
enum ShopToolStep {
    SHOP_TOOL_CHECK = 0,
    SHOP_TOOL_MESSAGE_CLOSED = 1,
    SHOP_TOOL_MESSAGE_END = 2,
    SHOP_TOOL_CANCELLED = 3,
    SHOP_TOOL_ASK = 4,
    SHOP_TOOL_ANSWER = 5,
    SHOP_TOOL_ASK_AGAIN = 6,
    SHOP_TOOL_ANSWER_AGAIN = 7,
    SHOP_TOOL_TAKE = 8,
    SHOP_TOOL_DONE = 9,
    SHOP_TOOL_FADE_OUT_WAIT = 10,
    SHOP_TOOL_FADE_IN_WAIT = 11,
    SHOP_TOOL_FADE_OUT = 12,
    SHOP_TOOL_CARRY_WAIT = 13,
    SHOP_TOOL_PAY = 14,
};

/* The tool shop's choose menu rows, in the order shop_tool_upgrade_menu_setup adds them. */
enum ShopStockMenuRow {
    SHOP_STOCK_MENU_IMPROVE_TOOL = 0,
    SHOP_STOCK_MENU_GIRLS_PRESENT = 1,
    SHOP_STOCK_MENU_MAYONNAISE_MAKER = 2,
    SHOP_STOCK_MENU_CHEESE_MAKER = 3,
    SHOP_STOCK_MENU_YARN_MAKER = 4,
    SHOP_STOCK_MENU_BUY_TOOL = 5,
};

/* The tool-shop dialogue's steps: the choose menu, the confirm menu, the tool-upgrade
 * steps, and the pay and leave. */
enum ShopStockStep {
    SHOP_STOCK_START = 0,
    SHOP_STOCK_MESSAGE_CLOSED = 1,
    SHOP_STOCK_CHOOSE_ALT_INTRO = 2,
    SHOP_STOCK_CONFIRM_PROMPT = 3,
    SHOP_STOCK_LEAVE = 4,
    SHOP_STOCK_PAY = 5,
    SHOP_STOCK_CHOOSE_MESSAGE = 6,
    SHOP_STOCK_CHOOSE_MESSAGE_CLOSE = 7,
    SHOP_STOCK_CHOOSE_MESSAGE_END = 8,
    SHOP_STOCK_CHOOSE_PROMPT = 9,
    SHOP_STOCK_CHOOSE_INPUT = 10,
    SHOP_STOCK_CHOOSE_DONE = 11,
    SHOP_STOCK_CHOOSE_AFTER_DONE = 12,
    SHOP_STOCK_CONFIRM_MESSAGE = 13,
    SHOP_STOCK_CONFIRM_MESSAGE_CLOSE = 14,
    SHOP_STOCK_CONFIRM_MESSAGE_END = 15,
    SHOP_STOCK_CONFIRM_PROMPT_MENU = 16,
    SHOP_STOCK_CONFIRM_INPUT = 17,
    SHOP_STOCK_CONFIRM_DONE = 18,
    SHOP_STOCK_CONFIRM_AFTER_DONE = 19,
    SHOP_STOCK_TOOL_CHECK = 20,
    SHOP_STOCK_TOOL_MESSAGE_CLOSED = 21,
    SHOP_STOCK_TOOL_MESSAGE_END = 22,
    SHOP_STOCK_TOOL_CANCELLED = 23,
    SHOP_STOCK_TOOL_ASK = 24,
    SHOP_STOCK_TOOL_ANSWER = 25,
    SHOP_STOCK_TOOL_ASK_AGAIN = 26,
    SHOP_STOCK_TOOL_ANSWER_AGAIN = 27,
    SHOP_STOCK_TOOL_TAKE = 28,
    SHOP_STOCK_TOOL_DONE = 29,
    SHOP_STOCK_TOOL_FADE_OUT_WAIT = 30,
    SHOP_STOCK_TOOL_FADE_IN_WAIT = 31,
    SHOP_STOCK_TOOL_FADE_OUT = 32,
    SHOP_STOCK_TOOL_CARRY_WAIT = 33,
    SHOP_STOCK_TOOL_PAY = 34,
    SHOP_STOCK_TOOL_PAST_END_35 = 35,
    SHOP_STOCK_TOOL_PAST_END_36 = 36,
    SHOP_STOCK_TOOL_PAST_END_37 = 37,
    SHOP_STOCK_TOOL_PAST_END_38 = 38,
    SHOP_STOCK_TOOL_PAST_END_39 = 39,
    SHOP_STOCK_TOOL_PAST_END_40 = 40,
};

/* The tool-shop dialogue: a choose menu (improve tool, a maker or the girls' present to
 * upgrade a tool, or buy the tool), a confirm menu for the chosen tool, checked against the
 * room and the money, then paid and stored. Returns 1 at step 4. */
static s32 shop_stock_step(void)
{
    FarmMenuRequest choose;
    FarmMenuRequest confirm;
    s32 done;
    u32 flags;
    s16 item;
    s16 price;
    s32 choice;

    flags = game_state.progress.estate.statusFlags;
    choose.first[0] = SHOP_NO_MESSAGE;
    choose.first[1] = 0x85;
    choose.second[0] = 2;
    choose.second[1] = 0x86;
    choose.count = 4;
    choose.menu = &shop_choice_menu;
    confirm.first[0] = SHOP_NO_MESSAGE;
    confirm.first[1] = 0;
    confirm.second[0] = 2;
    confirm.second[1] = 0x91;
    confirm.count = 6;
    confirm.menu = &shop_confirm_menu;
    item = shop_menu_selected_value(shop_confirm_menu);
    done = 0;
    price = item_buy_price(item);
    game_state.player.stateFlags |= PLAYER_FLAG_SCRIPTED;
    game_state.player.scriptHeld = 0;
    game_state.player.scriptPressed = 0;
    switch (shop_step) {
    case SHOP_STOCK_START:
        shop_set_step(SHOP_STOCK_CHOOSE_MESSAGE, SHOP_STOCK_START);
        break;
    case SHOP_STOCK_MESSAGE_CLOSED:
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            shop_restore_step();
        }
        break;
    case SHOP_STOCK_CHOOSE_ALT_INTRO:
        choose.first[0] = SHOP_NO_MESSAGE;
        choose.first[1] = 0xB5;
        shop_set_step(SHOP_STOCK_CHOOSE_MESSAGE, SHOP_STOCK_START);
        /* fall through */
    case SHOP_STOCK_CHOOSE_MESSAGE:
    case SHOP_STOCK_CHOOSE_MESSAGE_CLOSE:
    case SHOP_STOCK_CHOOSE_MESSAGE_END:
    case SHOP_STOCK_CHOOSE_PROMPT:
    case SHOP_STOCK_CHOOSE_INPUT:
    case SHOP_STOCK_CHOOSE_DONE:
    case SHOP_STOCK_CHOOSE_AFTER_DONE:
        switch (shop_menu_open_choose(&shop_step, SHOP_STOCK_CHOOSE_MESSAGE, choose)) {
        case SHOP_MENU_RESULT_NONE:
            break;
        case SHOP_MENU_RESULT_CHOSEN:
            switch (shop_choice_menu.selected) {
            case SHOP_STOCK_MENU_IMPROVE_TOOL:
                menu_request_open(2, 0x87);
                shop_set_step(SHOP_STOCK_MESSAGE_CLOSED, SHOP_STOCK_CHOOSE_ALT_INTRO);
                break;
            case SHOP_STOCK_MENU_GIRLS_PRESENT:
            case SHOP_STOCK_MENU_MAYONNAISE_MAKER:
            case SHOP_STOCK_MENU_CHEESE_MAKER:
            case SHOP_STOCK_MENU_YARN_MAKER:
                shop_set_step(SHOP_STOCK_TOOL_CHECK, SHOP_STOCK_START);
                break;
            case SHOP_STOCK_MENU_BUY_TOOL:
                if (flags & (ESTATE_STATUS_NO_BRUSH | ESTATE_STATUS_NO_MILKER | ESTATE_STATUS_NO_CLIPPERS)) {
                    shop_set_step(SHOP_STOCK_CONFIRM_MESSAGE, SHOP_STOCK_START);
                } else {
                    menu_request_open(2, 0xAD);
                    shop_set_step(SHOP_STOCK_MESSAGE_CLOSED, SHOP_STOCK_CHOOSE_ALT_INTRO);
                }
                break;
            }
            break;
        case SHOP_MENU_RESULT_CANCELLED:
            shop_set_step(SHOP_STOCK_LEAVE, SHOP_STOCK_START);
            break;
        }
        break;
    case SHOP_STOCK_CONFIRM_PROMPT:
        menu_request_open(2, 0xAF);
        shop_set_step(SHOP_STOCK_MESSAGE_CLOSED, SHOP_STOCK_CONFIRM_MESSAGE);
        break;
    case SHOP_STOCK_CONFIRM_MESSAGE:
    case SHOP_STOCK_CONFIRM_MESSAGE_CLOSE:
    case SHOP_STOCK_CONFIRM_MESSAGE_END:
    case SHOP_STOCK_CONFIRM_PROMPT_MENU:
    case SHOP_STOCK_CONFIRM_INPUT:
    case SHOP_STOCK_CONFIRM_DONE:
    case SHOP_STOCK_CONFIRM_AFTER_DONE:
        choice = shop_menu_open_choose(&shop_step, SHOP_STOCK_CONFIRM_MESSAGE, confirm);
        switch (choice) {
        case SHOP_MENU_RESULT_NONE:
            break;
        case SHOP_MENU_RESULT_CHOSEN:
            if (shop_item_room(item, 1) == 0) {
                menu_begin_close();
                menu_request_open(2, 0x54);
                shop_set_step(SHOP_STOCK_MESSAGE_CLOSED, SHOP_STOCK_CONFIRM_PROMPT);
            } else if (shop_can_afford(price) == choice) {
                menu_begin_close();
                menu_request_open(2, 0xAE);
                shop_set_step(SHOP_STOCK_MESSAGE_CLOSED, SHOP_STOCK_PAY);
            } else {
                menu_begin_close();
                menu_request_open(2, 0x4F);
                shop_set_step(SHOP_STOCK_MESSAGE_CLOSED, SHOP_STOCK_CONFIRM_PROMPT);
            }
            break;
        case SHOP_MENU_RESULT_CANCELLED:
            shop_dialogue_tick_index = SHOP_TICK_GOLD_CHOICE;
            shop_set_step(SHOP_STOCK_CHOOSE_ALT_INTRO, SHOP_STOCK_START);
            break;
        }
        break;
    case SHOP_STOCK_TOOL_CHECK:
    case SHOP_STOCK_TOOL_MESSAGE_CLOSED:
    case SHOP_STOCK_TOOL_MESSAGE_END:
    case SHOP_STOCK_TOOL_CANCELLED:
    case SHOP_STOCK_TOOL_ASK:
    case SHOP_STOCK_TOOL_ANSWER:
    case SHOP_STOCK_TOOL_ASK_AGAIN:
    case SHOP_STOCK_TOOL_ANSWER_AGAIN:
    case SHOP_STOCK_TOOL_TAKE:
    case SHOP_STOCK_TOOL_DONE:
    case SHOP_STOCK_TOOL_FADE_OUT_WAIT:
    case SHOP_STOCK_TOOL_FADE_IN_WAIT:
    case SHOP_STOCK_TOOL_FADE_OUT:
    case SHOP_STOCK_TOOL_CARRY_WAIT:
    case SHOP_STOCK_TOOL_PAY:
    case SHOP_STOCK_TOOL_PAST_END_35:
    case SHOP_STOCK_TOOL_PAST_END_36:
    case SHOP_STOCK_TOOL_PAST_END_37:
    case SHOP_STOCK_TOOL_PAST_END_38:
    case SHOP_STOCK_TOOL_PAST_END_39:
    case SHOP_STOCK_TOOL_PAST_END_40:
        switch (shop_item_menu_step(&shop_step, SHOP_STOCK_TOOL_CHECK, &choose)) {
        case SHOP_MENU_RESULT_NONE:
            break;
        case SHOP_MENU_RESULT_CHOSEN:
            shop_set_step(SHOP_STOCK_LEAVE, SHOP_STOCK_START);
            break;
        case SHOP_MENU_RESULT_CANCELLED:
            shop_dialogue_tick_index = SHOP_TICK_GOLD_CHOICE;
            menu_request_open(2, 0x91);
            shop_set_step(SHOP_STOCK_MESSAGE_CLOSED, SHOP_STOCK_CHOOSE_ALT_INTRO);
            break;
        }
        break;
    case SHOP_STOCK_PAY:
        estate_gold_change(-price, 1);
        shop_item_add(item, 1, 1);
        milestone_refresh_all();
        shop_tool_upgrade_menu_setup();
        shop_dialogue_tick_index = SHOP_TICK_GOLD_CHOICE;
        menu_request_open(2, 0x91);
        shop_set_step(SHOP_STOCK_MESSAGE_CLOSED, SHOP_STOCK_CHOOSE_ALT_INTRO);
        break;
    case SHOP_STOCK_LEAVE:
        game_state.player.stateFlags &= ~PLAYER_FLAG_SCRIPTED;
        done = 1;
        break;
    }
    return done;
}

/* The refusal messages for shop_tool_upgrade_state's reasons 1 to 5. */
static s16 shop_reason_messages[6] = {0x0096, 0x008D, 0x0097, 0x0098, 0x0099, 0x0000};

/* The greenhouse purchase dialogue's steps: the check of the tool's state (CHECK, and the
 * refusal REFUSE), the offer and its two answers (ASK, ANSWER, ACCEPTED, ACCEPT_ANSWER), the
 * pay, the fade and the delivery scene, and the done step. */
enum ShopGreenhouseStep {
    SHOP_GREENHOUSE_START = 0,
    SHOP_GREENHOUSE_MESSAGE_CLOSED = 1,
    SHOP_GREENHOUSE_MESSAGE_END = 2,
    SHOP_GREENHOUSE_CHECK = 3,
    SHOP_GREENHOUSE_REFUSE = 4,
    SHOP_GREENHOUSE_ASK = 5,
    SHOP_GREENHOUSE_ANSWER = 6,
    SHOP_GREENHOUSE_DONE = 7,
    SHOP_GREENHOUSE_FADE_IN_WAIT = 8,
    SHOP_GREENHOUSE_FADE_OUT_WAIT = 9,
    SHOP_GREENHOUSE_TAKE = 10,
    SHOP_GREENHOUSE_PAY_START = 11,
    SHOP_GREENHOUSE_PAY = 12,
    SHOP_GREENHOUSE_ACCEPTED = 13,
    SHOP_GREENHOUSE_ACCEPT_ANSWER = 14,
    SHOP_GREENHOUSE_CARRY_WAIT = 15,
};

/* The greenhouse purchase dialogue: refuse with the reason for the tool's state, offer
 * the level's price, confirm twice, pay, set the milestone timer, then fade out and in
 * for the delivery scene. Returns 1 at step 7. */
static s32 shop_greenhouse_step(void)
{
    s32 done;
    s16 kind;
    s16 slot;
    s32 price;
    s32 choice;
    s16 reason;
    s32 row;

    done = 0;
    game_state.player.scriptHeld = 0;
    game_state.player.scriptPressed = 0;
    game_state.player.stateFlags |= PLAYER_FLAG_SCRIPTED;
    kind = game_state.player.held[HELD_ITEM].id;
    slot = game_state.player.held[HELD_TOOL].id;
    price = greenhouse_level_value(greenhouse_kind_index(kind), 1);
    switch (shop_step) {
    case SHOP_GREENHOUSE_START:
        shop_set_step(SHOP_GREENHOUSE_CHECK, SHOP_GREENHOUSE_START);
        break;
    case SHOP_GREENHOUSE_MESSAGE_CLOSED:
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            shop_restore_step();
        }
        break;
    case SHOP_GREENHOUSE_MESSAGE_END:
        if (menu_get_phase() == TEXT_WINDOW_END_WAIT) {
            text_set_input(0);
            shop_restore_step();
        }
        break;
    case SHOP_GREENHOUSE_CARRY_WAIT:
        if (game_state.player.stateFlags & PLAYER_FLAG_CARRY_ITEM) {
            shop_restore_step();
        }
        break;
    case SHOP_GREENHOUSE_CHECK:
        reason = shop_tool_upgrade_state();
        if (reason != 0) {
            menu_request_open(2, shop_reason_messages[reason - 1]);
            shop_set_step(SHOP_GREENHOUSE_MESSAGE_CLOSED, SHOP_GREENHOUSE_REFUSE);
        } else {
            shop_dialogue_tick_index = SHOP_TICK_GOLD_PANEL;
            menu_set_sound_enabled(0);
            menu_request_open(2, greenhouse_item_column(greenhouse_kind_index(kind)) + 0x9A);
            shop_set_step(SHOP_GREENHOUSE_MESSAGE_END, SHOP_GREENHOUSE_ASK);
        }
        break;
    case SHOP_GREENHOUSE_ASK:
        shop_set_step(SHOP_GREENHOUSE_ANSWER, SHOP_GREENHOUSE_START);
        shop_result_update(0, -1, 0);
        /* fall through */
    case SHOP_GREENHOUSE_ANSWER:
        choice = shop_result_update(&current_frame->otTags[0][11], 200, 128);
        switch (choice) {
        case SHOP_ANSWER_PENDING:
            break;
        case SHOP_ANSWER_YES:
            menu_begin_close();
            if (shop_can_afford(price) == choice) {
                menu_set_sound_enabled(0);
                menu_request_open(2, 0x9E);
                shop_set_step(SHOP_GREENHOUSE_MESSAGE_END, SHOP_GREENHOUSE_ACCEPTED);
            } else {
                menu_request_open(2, 0x4F);
                shop_set_step(SHOP_GREENHOUSE_MESSAGE_CLOSED, SHOP_GREENHOUSE_REFUSE);
            }
            break;
        case SHOP_ANSWER_NO:
            menu_begin_close();
            menu_request_open(2, 0x93);
            shop_set_step(SHOP_GREENHOUSE_MESSAGE_CLOSED, SHOP_GREENHOUSE_REFUSE);
            break;
        }
        break;
    case SHOP_GREENHOUSE_ACCEPTED:
        shop_set_step(SHOP_GREENHOUSE_ACCEPT_ANSWER, SHOP_GREENHOUSE_START);
        shop_result_update(0, -1, 0);
        /* fall through */
    case SHOP_GREENHOUSE_ACCEPT_ANSWER:
        switch (shop_result_update(&current_frame->otTags[0][11], 224, 128)) {
        case SHOP_ANSWER_PENDING:
            break;
        case SHOP_ANSWER_YES:
            menu_begin_close();
            shop_set_step(SHOP_GREENHOUSE_PAY_START, SHOP_GREENHOUSE_START);
            break;
        case SHOP_ANSWER_NO:
            menu_begin_close();
            menu_request_open(2, 0x93);
            shop_set_step(SHOP_GREENHOUSE_MESSAGE_CLOSED, SHOP_GREENHOUSE_REFUSE);
            break;
        }
        break;
    case SHOP_GREENHOUSE_PAY_START:
        shop_dialogue_tick_index = 0;
        /* fall through */
    case SHOP_GREENHOUSE_PAY:
        estate_gold_change(-price, 1);
        row = greenhouse_kind_index(kind);
        milestone_set_timer(((slot - 1) / GREENHOUSE_KINDS * GREENHOUSE_KINDS + 1 + row));
        menu_begin_close();
        menu_request_open(2, 0x9F);
        shop_set_step(SHOP_GREENHOUSE_MESSAGE_CLOSED, SHOP_GREENHOUSE_TAKE);
        break;
    case SHOP_GREENHOUSE_TAKE:
        event_item_can_take(kind, 1, 0, 1);
        event_item_can_take(slot, 0, 0, 1);
        fade_start_out(20, 0, 2);
        shop_set_step(SHOP_GREENHOUSE_FADE_OUT_WAIT, SHOP_GREENHOUSE_START);
        /* fall through */
    case SHOP_GREENHOUSE_FADE_OUT_WAIT:
        if (fade_is_settled() == 1) {
            event_cmd_set_target(0x20, 0, 0, 0);
            game_state.player.position.vx = 1000;
            game_state.player.position.vz = -2800;
            game_state.player.position.vy = 0;
            event_cmd_set_facing(3, 0, 0, 0);
            fade_start_in(20, 0, 2);
            shop_set_step(SHOP_GREENHOUSE_FADE_IN_WAIT, SHOP_GREENHOUSE_START);
        }
        break;
    case SHOP_GREENHOUSE_REFUSE:
        menu_request_open(2, 0x87);
        shop_set_step(SHOP_GREENHOUSE_MESSAGE_CLOSED, SHOP_GREENHOUSE_DONE);
        break;
    case SHOP_GREENHOUSE_FADE_IN_WAIT:
        if (fade_is_settled() != 1) {
            break;
        }
        event_cmd_reset_schedule(CHARACTER_SAIBARA, 0, 0, 0);
        shop_set_step(SHOP_GREENHOUSE_DONE, SHOP_GREENHOUSE_START);
        /* fall through */
    case SHOP_GREENHOUSE_DONE:
        game_state.player.stateFlags &= ~PLAYER_FLAG_SCRIPTED;
        done = 1;
        break;
    }
    return done;
}

/* Draw the gold panel and the count total (count times price) at (16, 16). */
static s32 shop_menu_count_draw(void) {
    s16 h8;
    s16 ha;

    shop_draw_gold_panel();
    h8 = shop_menu_header.h8;
    ha = shop_menu_header.ha;
    shop_draw_count_total(shop_menu_header, h8, h8 * ha, 16, 16);
    return 0;
}

static s32 shop_gold_panel_step(void) {
    shop_draw_gold_panel();
    return 0;
}

static s32 shop_gold_panel_redraw_step(void) {
    shop_draw_gold_panel();
    return 0;
}

static s32 shop_gold_choice_step(void) {
    shop_draw_gold_panel();
    shop_menu_draw(&shop_choice_menu, 16, 16);
    return 0;
}

/* Draw the gold panel and the choice menu, then the count total under them, the panel as tall
 * as the choice menu's rows. */
static s32 shop_panel_submit(void)
{
    s16 height;
    s16 h8;
    s16 ha;

    height = SHOP_CHOICE_PANEL_HEIGHT();
    shop_draw_gold_panel();
    shop_menu_draw(&shop_choice_menu, 16, 16);
    h8 = shop_menu_header.h8;
    ha = shop_menu_header.ha;
    shop_draw_count_total(shop_menu_header, h8, h8 * ha, 16, height);
    return 0;
}

/* Draw the gold panel, the confirm menu at the choice panel's height, and the choice menu. */
static s32 shop_panel_submit_both(void) {
    s16 height;

    height = SHOP_CHOICE_PANEL_HEIGHT();
    shop_draw_gold_panel();
    shop_menu_draw(&shop_confirm_menu, 16, height);
    shop_menu_draw(&shop_choice_menu, 16, 16);
    return 0;
}

static s32 shop_medals_choice_step(void) {
    shop_draw_medals_panel();
    shop_menu_draw(&shop_choice_menu, 16, 48);
    return 0;
}

static s32 shop_dialogue_0f_start(void) {
    shop_dialogue_tick_index = 0;
    return 1;
}

static s32 shop_fish_food_menu_setup(void) {
    ShopMenuHeader *block;
    s32 base;

    block = &shop_menu_header;
    base = text_bank_starts.names;
    block->w0 = LOCALIZED_NAME(text_bank.names.fish_food);
    block->w4 = LOCALIZED_NAME(text_bank.names.units);
    block->ha = item_buy_price(ITEM_FISH_FOOD_06C);
    block->h8 = 1;
    shop_dialogue_tick_index = 0;
    return 1;
}

static s32 shop_dialogue_03_start(void) {
    shop_dialogue_tick_index = 0;
    return 1;
}

static s32 shop_start_default(void) {
    shop_dialogue_tick_index = 0;
    return 1;
}

/* Set up the first seed menu: its value field ends at column 0x10 once the group A seeds
 * are unlocked (else 0xE), and the strawberry entry shows only then. */
static s32 shop_seed_menu_setup_a(void) {
    s32 base;
    FarmMenu *menu;
    FarmMenu *shown;
    u16 **text = 0;
    s16 *entryValue;
    u8 *entryFlag;
    s16 *label46;
    u32 unlocked;

    unlocked = game_state.progress.estate.statusFlags & ESTATE_STATUS_GROUP_A_DONE;
    shop_menu_header.w0 = 0;
    shop_menu_header.w4 = LOCALIZED_NAME(text_bank.names.units);
    shop_menu_header.ha = item_buy_price(0x6C);
    shop_menu_header.h8 = 1;

    menu = &shop_choice_menu;
    shop_menu_clear(menu);
    if (unlocked != 0) {
        menu->columns[0] = 0x10;
        menu->columns[1] = 0x10;
    } else {
        menu->columns[0] = 0xE;
        menu->columns[1] = 0xE;
    }
    shown = &shop_choice_menu;
    shown->selected = 0;
    text = shown->text;
    entryValue = shown->entryValue;
    entryFlag = shown->entryFlag;
    label46 = shown->label46;
    base = text_offset();
    ADD_ENTRY(SHOP_NAME_AT(base, text_bank.names.seeds.grass), 0x36, 1, 0xCF);
    ADD_ENTRY(SHOP_NAME_AT(base, text_bank.names.seeds.turnip), 0x22, 1, 0xC3);
    ADD_ENTRY(SHOP_NAME_AT(base, text_bank.names.seeds.potato), 0x23, 1, 0xC4);
    ADD_ENTRY(SHOP_NAME_AT(base, text_bank.names.seeds.cucumber), 0x24, 1, 0xC5);
    if (unlocked != 0) {
        *text = SHOP_NAME_AT(base, text_bank.names.seeds.strawberry);
    } else {
        *text = 0;
    }
    *entryValue = 0x25;
    *entryFlag = 1;
    *label46 = 0xC6;
    shop_dialogue_tick_index = 0;
    return 1;
}

/* Set up the second seed menu: its value field ends at column 0xD once the group B seeds
 * are unlocked (else 0xC), and the pumpkin entry shows only then. */
static s32 shop_seed_menu_setup_b(void) {
    s32 base;
    FarmMenu *menu;
    FarmMenu *shown;
    u16 **text = 0;
    s16 *entryValue;
    u8 *entryFlag;
    s16 *label46;
    u32 unlocked;

    shop_menu_header.w0 = 0;
    shop_menu_header.w4 = LOCALIZED_NAME(text_bank.names.units);
    shop_menu_header.ha = item_buy_price(0x6C);
    shop_menu_header.h8 = 1;
    unlocked = game_state.progress.estate.statusFlags & ESTATE_STATUS_GROUP_B_DONE;

    menu = &shop_choice_menu;
    shop_menu_clear(menu);
    if (unlocked != 0) {
        menu->columns[0] = 0xD;
        menu->columns[1] = 0xD;
    } else {
        menu->columns[0] = 0xC;
        menu->columns[1] = 0xC;
    }
    shown = &shop_choice_menu;
    shown->selected = 0;
    text = shown->text;
    entryValue = shown->entryValue;
    entryFlag = shown->entryFlag;
    label46 = shown->label46;
    base = text_offset();
    ADD_ENTRY(SHOP_NAME_AT(base, text_bank.names.seeds.grass), 0x36, 1, 0xCF);
    ADD_ENTRY(SHOP_NAME_AT(base, text_bank.names.seeds.tomato), 0x27, 1, 0xC7);
    ADD_ENTRY(SHOP_NAME_AT(base, text_bank.names.seeds.corn), 0x28, 1, 0xC8);
    ADD_ENTRY(SHOP_NAME_AT(base, text_bank.names.seeds.onion), 0x29, 1, 0xC9);
    if (unlocked != 0) {
        *text = SHOP_NAME_AT(base, text_bank.names.seeds.pumpkin);
    } else {
        *text = 0;
    }
    *entryValue = 0x2A;
    *entryFlag = 1;
    *label46 = 0xCA;
    shop_dialogue_tick_index = 0;
    return 1;
}

/* Set up the third seed menu: the spinach entry shows only while its status bit is set. */
static s32 shop_seed_menu_setup_c(void) {
    s32 base;
    FarmMenu *menu;
    u16 **text;
    s16 *entryValue;
    u8 *entryFlag;
    s16 *label46;
    u32 flags;

    shop_menu_header.w0 = 0;
    shop_menu_header.w4 = LOCALIZED_NAME(text_bank.names.units);
    shop_menu_header.ha = item_buy_price(0x6C);
    shop_menu_header.h8 = 1;

    menu = &shop_choice_menu;
    shop_menu_clear(menu);
    menu->columns[0] = 0x12;
    menu->columns[1] = 0x12;
    menu->selected = 0;
    menu = &shop_choice_menu;
    text = menu->text;
    entryValue = menu->entryValue;
    entryFlag = menu->entryFlag;
    label46 = menu->label46;
    base = text_offset();
    ADD_ENTRY(SHOP_NAME_AT(base, text_bank.names.seeds.grass), 0x36, 1, 0xCF);
    ADD_ENTRY(SHOP_NAME_AT(base, text_bank.names.seeds.eggplant), 0x2C, 1, 0xCB);
    ADD_ENTRY(SHOP_NAME_AT(base, text_bank.names.seeds.carrot), 0x2D, 1, 0xCC);
    ADD_ENTRY(SHOP_NAME_AT(base, text_bank.names.seeds.sweet_potato), 0x2E, 1, 0xCD);
    flags = game_state.progress.estate.statusFlags;
    FARM_DEBUG_CHECK();
    if (flags & ESTATE_STATUS_GROUP_C_DONE) {
        *text = SHOP_NAME_AT(base, text_bank.names.seeds.spinach);
    } else {
        *text = 0;
    }
    *entryValue = 0x2F;
    *entryFlag = 1;
    *label46 = 0xCE;
    shop_dialogue_tick_index = 0;
    return 1;
}

/* Set up the fourth seed menu: eight entries, the fourth shown only while its status bit
 * is set. */
static s32 shop_seed_menu_setup_d(void) {
    s32 base;
    FarmMenu *menu;
    u16 **text;
    s16 *entryValue;
    u8 *entryFlag;
    s16 *label46;
    u32 flags;

    shop_menu_header.w0 = 0;
    shop_menu_header.w4 = LOCALIZED_NAME(text_bank.names.units);
    flags = game_state.progress.estate.statusFlags;
    shop_menu_header.ha = item_buy_price(0x6C);
    shop_menu_header.h8 = 1;

    menu = &shop_choice_menu;
    shop_menu_clear(menu);
    menu->columns[0] = 0x12;
    menu->columns[1] = 0x12;
    menu->selected = 0;
    menu = &shop_choice_menu;
    text = menu->text;
    entryValue = menu->entryValue;
    entryFlag = menu->entryFlag;
    label46 = menu->label46;
    base = text_offset();
    ADD_ENTRY(SHOP_NAME_AT(base, text_bank.names.seeds.cabbage), 0x26, 1, 0x165);
    ADD_ENTRY(SHOP_NAME_AT(base, text_bank.names.seeds.pineapple), 0x2B, 1, 0x166);
    ADD_ENTRY(SHOP_NAME_AT(base, text_bank.names.seeds.green_pepper), 0x30, 1, 0x167);
    FARM_DEBUG_CHECK();
    ADD_ENTRY(flags & ESTATE_STATUS_BIT23 ? SHOP_NAME_AT(base, text_bank.names.seeds.moondrop) : 0, 0x31, 1, 0x168);
    {
        s32 offset = text_offset();

        ADD_ENTRY(SHOP_NAME_AT(offset, text_bank.names.seeds.pink_cat), 0x32, 1, 0x169);
        ADD_ENTRY(SHOP_NAME_AT(offset, text_bank.names.seeds.magic_red), 0x33, 1, 0x16A);
        ADD_ENTRY(SHOP_NAME_AT(offset, text_bank.names.seeds.toy_flower), 0x34, 1, 0x16B);
        ADD_ENTRY(SHOP_NAME_AT(offset, text_bank.names.seeds.orangecup), 0x35, 1, 0x16C);
    }
    shop_dialogue_tick_index = 0;
    return 1;
}

/* Set up the wine menu: wine and grape juice. */
static s32 shop_wine_menu_setup(void)
{
    FarmMenu *menu;
    u16 **text;
    s16 *entryValue;
    u8 *entryFlag;
    s16 *label46;

    menu = &shop_choice_menu;
    shop_menu_clear(menu);
    menu->columns[0] = 11;
    menu->columns[1] = 16;
    menu->selected = 0;
    text = menu->text;
    entryValue = menu->entryValue;
    entryFlag = menu->entryFlag;
    label46 = menu->label46;
    ADD_ENTRY(LOCALIZED_NAME(text_bank.names.wine), ITEM_WINE, 0, 0xB2);
    ADD_ENTRY(LOCALIZED_NAME(text_bank.names.grape_juice), ITEM_GRAPE_JUICE, 0, 0xB3);
    shop_dialogue_tick_index = 0;
    return 1;
}

/* The second entry's entryValue, picked at random from these three. */
static u16 girls_present_messages[3] = {
    0x00F7, 0x00F8, 0x00F9,
};

/* Set up the tool-upgrade menus. The first lists improve tool, the girls' present (its value
 * picked at random from girls_present_messages), the mayonnaise, cheese and yarn makers (each
 * shown once its order is open) and buy tool. The confirm menu lists the brush, milker and
 * clippers: each has its text only while that tool is missing, and widens the columns. When
 * the brush is held but another tool is missing, the cursor starts on the next entry. */
static s32 shop_tool_upgrade_menu_setup(void)
{
    FarmMenu *menu;
    FarmMenu *second;
    u16 **text;
    s16 *entryValue;
    u8 *entryFlag;
    s16 *label46;
    u32 flags;

    flags = game_state.progress.estate.statusFlags;
    menu = &shop_choice_menu;
    shop_menu_clear(menu);
    menu->columns[0] = 0xD;
    menu->columns[1] = 0xD;
    menu->selected = 0;
    text = menu->text;
    entryValue = menu->entryValue;
    entryFlag = menu->entryFlag;
    label46 = menu->label46;
    ADD_ENTRY(LOCALIZED_NAME(text_bank.names.improve_tool), 0, 1, 0x87);
    ADD_ENTRY(LOCALIZED_NAME(text_bank.names.girls_present), girls_present_messages[rand_range(sizeof girls_present_messages / sizeof girls_present_messages[0])], 1, 0x88);
    if (flags & ESTATE_STATUS_COOP_ORDER_OPEN) {
        menu->columns[0] = 0x10;
        menu->columns[1] = 0x10;
        *text = LOCALIZED_NAME(text_bank.names.mayonnaise_maker);
    } else {
        *text = 0;
    }
    END_ENTRY(ITEM_MAYONNAISE_MAKER, 1, 0x89);
    if (flags & ESTATE_STATUS_BARN_ORDER_OPEN) {

        *text = LOCALIZED_NAME(text_bank.names.cheese_maker);
    } else {
        *text = 0;
    }
    END_ENTRY(0x140, 1, 0x8A);
    if (flags & ESTATE_STATUS_BARN_FULL_ORDER_OPEN) {

        *text = LOCALIZED_NAME(text_bank.names.yarn_maker);
    } else {
        *text = 0;
    }
    END_ENTRY(0x141, 1, 0x8B);
    ADD_ENTRY(LOCALIZED_NAME(text_bank.names.buy_tool), 0, 1, 0x8C);

    second = &shop_confirm_menu;
    shop_menu_clear(second);
    second->columns[0] = 0;
    second->columns[1] = 0;
    second->selected = 0;
    text = second->text;
    entryValue = second->entryValue;
    entryFlag = second->entryFlag;
    label46 = second->label46;
    if (flags & ESTATE_STATUS_NO_BRUSH) {
        *text = LOCALIZED_NAME(text_bank.names.brush);
        second->columns[0] = 5;
        second->columns[1] = 0xA;
    } else {
        *text = 0;
    }
    END_ENTRY(ITEM_BRUSH, 0, 0xAA);
    if (flags & ESTATE_STATUS_NO_MILKER) {

        shop_confirm_menu.columns[0] = 6;
        shop_confirm_menu.columns[1] = 0xC;
        *text = LOCALIZED_NAME(text_bank.names.milker);
    } else {
        *text = 0;
    }
    END_ENTRY(ITEM_MILKER, 0, 0xAB);
    if (flags & ESTATE_STATUS_NO_CLIPPERS) {

        *text = LOCALIZED_NAME(text_bank.names.clippers);
        shop_confirm_menu.columns[0] = 8;
        shop_confirm_menu.columns[1] = 0xE;
    } else {
        *text = 0;
    }
    *entryValue = ITEM_CLIPPERS;
    *entryFlag = 0;
    *label46 = 0xAC;
    if ((flags & (ESTATE_STATUS_NO_BRUSH | ESTATE_STATUS_NO_MILKER | ESTATE_STATUS_NO_CLIPPERS)) && !(flags & ESTATE_STATUS_NO_BRUSH)) {
        shop_confirm_menu.selected =
            shop_menu_next_entry(shop_confirm_menu, SHOP_MENU_FORWARD);
    }
    shop_dialogue_tick_index = 0;
    return 1;
}

static s32 shop_greenhouse_start(void) {
    shop_dialogue_tick_index = 0;
    return 1;
}

/* Hand `item` over with its cue (at step `position`), then return 1 once the player holds it idle. */
static s32 shop_item_carry_step(s32 *value, u32 position, s16 item) {
    s32 result;
    u32 difference;

    result = 0;
    difference = *value - position;
    if (difference >= 2) return 0;
    switch (difference) {
    case 0:
        event_cue_issue(item, 0, 0, 0);
        difference = 1;
        break;
    case 1:
        if ((game_state.player.stateFlags & (SHOP_CARRY_READY_FLAGS | PLAYER_FLAG_MODE_ANIM)) == SHOP_CARRY_READY_FLAGS) {
            result = 1;
        }
        break;
    }
    *value = position + difference;
    return result;
}

/* Show the gift item (step SHOP_GIFT_SHOW), then return 1 once the player is idle again (SHOP_GIFT_WAIT_IDLE). */
static s32 shop_item_show_step(s32 *value, s32 amount, s16 modifier) {
    s32 completed;
    u32 difference;
    FarmStoredItem work;

    completed = 0;
    difference = *value - amount;
    if (difference >= 2U) {
        return 0;
    }
    switch (difference) {
    case SHOP_GIFT_SHOW:
        stored_item_set(&work, ITEM_GIFT, modifier, 1);
        player_show_item(&work);
        difference = SHOP_GIFT_WAIT_IDLE;
        break;
    case SHOP_GIFT_WAIT_IDLE:
        if ((game_state.player.stateFlags & PLAYER_FLAG_IDLE_POSE) != 0) {
            completed = 1;
        }
        break;
    }
    *value = amount + difference;
    return completed;
}

/* Run one step of a menu dialogue whose step is *step - base: show the
 * opening message, wait, open the menu, then take the choice. Returns -1
 * when the step is not this dialogue's, 1 when an entry was chosen, 3 when
 * finished, else 0. */
static s32 shop_menu_open_choose(s32 *step, s32 base, FarmMenuRequest request) {
    u32 index;
    s32 result;
    FarmMenu *menu;
    s16 open0;
    s16 open1;
    s16 cancel0;
    s16 cancel1;
    s32 next;

    index = *step - base;
    if (index > SHOP_MENU_DONE) {
        return -1;
    }
    result = 0;
    open0 = request.first[0];
    open1 = request.first[1];
    cancel0 = request.second[0];
    cancel1 = request.second[1];
    next = request.count;
    menu = request.menu;
    switch (index) {
    case SHOP_MENU_OPEN_MESSAGE:
        if (open0 != -1) {
            menu_request_open(open0, open1);
            shop_set_step_offset(SHOP_MENU_MESSAGE_CLOSE, SHOP_MENU_PROMPT, base);
        } else {
            shop_set_step_offset(SHOP_MENU_PROMPT, SHOP_MENU_OPEN_MESSAGE, base);
        }
        break;
    case SHOP_MENU_MESSAGE_CLOSE:
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            shop_restore_step();
        }
        break;
    case SHOP_MENU_MESSAGE_END:
        if (menu_get_phase() == TEXT_WINDOW_END_WAIT) {
            shop_restore_step();
        }
        break;
    case SHOP_MENU_PROMPT:
        menu_set_sound_enabled(0);
        text_set_input(0);
        if (shop_dialogue_tick_index == SHOP_TICK_MEDALS_CHOICE) {
            shop_dialogue_tick_index = next;
            if (menu_request_open(2, 0x5F) == 0) {
                shop_set_step_offset(SHOP_MENU_MESSAGE_END, SHOP_MENU_INPUT, base);
            }
        } else {
            shop_dialogue_tick_index = next;
            if (menu_request_open(2, 0x46) == 0) {
                shop_set_step_offset(SHOP_MENU_MESSAGE_END, SHOP_MENU_INPUT, base);
            }
        }
        break;
    case SHOP_MENU_INPUT:
        switch (shop_menu_input(menu)) {
        case SHOP_MENU_INPUT_NONE:
            break;
        case SHOP_MENU_INPUT_SELECT:
            menu_begin_close();
            result = 1;
            break;
        case SHOP_MENU_INPUT_INFO:
            if (shop_menu_selected_label(*menu) != -1) {
                menu_begin_close();
                menu_request_open(2, shop_menu_selected_label(*menu));
                shop_set_step_offset(SHOP_MENU_MESSAGE_CLOSE, SHOP_MENU_PROMPT, base);
            }
            break;
        case SHOP_MENU_INPUT_CANCEL:
            menu_begin_close();
            menu_request_open(cancel0, cancel1);
            shop_set_step_offset(SHOP_MENU_MESSAGE_CLOSE, SHOP_MENU_DONE, base);
            break;
        }
        break;
    case SHOP_MENU_DONE:
        result = 3;
        break;
    }
    return result;
}

/* Run one step of a count dialogue whose step is *step - base: open the message window,
 * show the selected item's price, then take a quantity (up to SHOP_STOCK_COUNT_MAX, into
 * header->h8) and close. Sets the dialogue tick to `next`. Returns -1 when the step is not
 * this dialogue's, 1 when the quantity was confirmed, 3 when finished, else 0. */
s32 shop_menu_choose_next(s32 *step, s32 base, s32 next, FarmMenuRequest request, ShopMenuHeader *header) {
    s32 result;
    u32 index;
    FarmMenu *menu;

    index = *step - base;
    if (index > SHOP_MENU_DONE) {
        return -1;
    }
    result = 0;
    menu = request.menu;
    switch (index) {
    case SHOP_MENU_OPEN_MESSAGE:
        shop_set_step_offset(SHOP_MENU_PROMPT, SHOP_MENU_OPEN_MESSAGE, base);
        break;
    case SHOP_MENU_MESSAGE_CLOSE:
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            shop_restore_step();
        }
        break;
    case SHOP_MENU_MESSAGE_END:
        if (menu_get_phase() == TEXT_WINDOW_END_WAIT) {
            shop_restore_step();
        }
        break;
    case SHOP_MENU_PROMPT:
        menu_set_sound_enabled(0);
        text_set_input(0);
        menu_request_open(2, 0x4D);
        shop_price_label_set(header, 0, 0, item_buy_price(shop_menu_selected_value(*menu)), 0);
        shop_dialogue_tick_index = next;
        shop_set_step_offset(SHOP_MENU_MESSAGE_END, SHOP_MENU_INPUT, base);
        break;
    case SHOP_MENU_INPUT:
        switch (shop_quantity_input(SHOP_STOCK_COUNT_MAX, &header->h8)) {
        case SHOP_QUANTITY_NONE:
            break;
        case SHOP_QUANTITY_CONFIRM:
            menu_begin_close();
            result = 1;
            break;
        case SHOP_QUANTITY_INFO:
            menu_begin_close();
            menu_request_open(2, shop_menu_selected_label(*menu));
            shop_set_step_offset(SHOP_MENU_MESSAGE_CLOSE, SHOP_MENU_PROMPT, base);
            break;
        case SHOP_QUANTITY_CANCEL:
            menu_begin_close();
            menu_request_open(request.third[0], request.third[1]);
            shop_set_step_offset(SHOP_MENU_MESSAGE_CLOSE, SHOP_MENU_DONE, base);
            break;
        }
        break;
    case SHOP_MENU_DONE:
        result = 3;
        break;
    }
    return result;
}

/* One step of the tool-upgrade dialogue, for the ore chosen from the request's menu: check
 * it is held and the price can be paid, confirm twice, hand it over, then pay and take the
 * upgraded tool. Returns -1 when the step is not this dialogue's, 1 when done, 3 when
 * cancelled, else 0. */
static s32 shop_item_menu_step(s32 *step, s32 base, FarmMenuRequest *request)
{
    u32 index;
    s16 item;
    s32 tool;
    s16 message[4][2];
    s32 held;
    s32 orichalcum;  /* the cursor is on the second entry */
    s32 result;

    index = *step - base;
    if (index > 14) {
        return -1;
    }
    item = shop_menu_selected_value(*request->menu);
    orichalcum = request->menu->selected == 1;
    if (!orichalcum) {
        tool = ITEM_ADAMANTITE;
        message[0][0] = 2;
        message[0][1] = 0x1E2;
        message[1][0] = 2;
        message[1][1] = 0xA4;
        message[2][0] = 2;
        message[2][1] = 0xA4;
        message[3][0] = 2;
        message[3][1] = 0xA5;
    } else {
        tool = ITEM_ORICHALCUM;
        message[0][0] = 2;
        message[0][1] = 0x1E1;
        message[1][0] = 2;
        message[1][1] = 0xA3;
        message[2][0] = 2;
        message[2][1] = 0xA3;
        message[3][0] = 2;
        message[3][1] = 0x9E;
    }
    result = 0;
    {
        switch (index) {
        case SHOP_TOOL_CHECK:
            held = event_item_can_take(tool, 1, 1, 0);
            if (held == 1) {
                if (shop_can_afford(item_buy_price(item)) == held) {
                    menu_set_sound_enabled(0);
                    menu_request_open(message[0][0], message[0][1]);
                    shop_set_step_offset(SHOP_TOOL_MESSAGE_END, SHOP_TOOL_ASK, base);
                } else {
                    menu_request_open(message[1][0], message[1][1]);
                    shop_set_step_offset(SHOP_TOOL_MESSAGE_CLOSED, SHOP_TOOL_CANCELLED, base);
                }
            } else {
                menu_request_open(message[2][0], message[2][1]);
                shop_set_step_offset(SHOP_TOOL_MESSAGE_CLOSED, SHOP_TOOL_CANCELLED, base);
            }
            break;
        case SHOP_TOOL_MESSAGE_CLOSED:
            if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
                shop_restore_step();
            }
            break;
        case SHOP_TOOL_MESSAGE_END:
            if (menu_get_phase() == TEXT_WINDOW_END_WAIT) {
                text_set_input(0);
                shop_restore_step();
            }
            break;
        case SHOP_TOOL_CARRY_WAIT:
            if (game_state.player.stateFlags & PLAYER_FLAG_CARRY_ITEM) {
                shop_restore_step();
            }
            break;
        case SHOP_TOOL_ASK:
            shop_set_step_offset(SHOP_TOOL_ANSWER, SHOP_TOOL_CHECK, base);
            shop_result_update(0, -1, 0);
            /* fall through */
        case SHOP_TOOL_ANSWER:
            switch (shop_result_update(&current_frame->otTags[0][11], 200, 128)) {
            case SHOP_ANSWER_PENDING:
                break;
            case SHOP_ANSWER_YES:
                menu_begin_close();
                menu_set_sound_enabled(0);
                menu_request_open(message[3][0], message[3][1]);
                shop_set_step_offset(SHOP_TOOL_MESSAGE_END, SHOP_TOOL_ASK_AGAIN, base);
                break;
            case SHOP_ANSWER_NO:
                menu_begin_close();
                menu_request_open(2, 0x93);
                shop_set_step_offset(SHOP_TOOL_MESSAGE_CLOSED, SHOP_TOOL_CANCELLED, base);
                break;
            }
            break;
        case SHOP_TOOL_ASK_AGAIN:
            shop_set_step_offset(SHOP_TOOL_ANSWER_AGAIN, SHOP_TOOL_CHECK, base);
            shop_result_update(0, -1, 0);
            /* fall through */
        case SHOP_TOOL_ANSWER_AGAIN:
            switch (shop_result_update(&current_frame->otTags[0][11], 224, 128)) {
            case SHOP_ANSWER_PENDING:
                break;
            case SHOP_ANSWER_YES:
                menu_begin_close();
                shop_set_step_offset(SHOP_TOOL_TAKE, SHOP_TOOL_CHECK, base);
                break;
            case SHOP_ANSWER_NO:
                menu_begin_close();
                menu_request_open(2, 0x93);
                shop_set_step_offset(SHOP_TOOL_MESSAGE_CLOSED, SHOP_TOOL_CANCELLED, base);
                break;
            }
            break;
        case SHOP_TOOL_TAKE:
            shop_dialogue_tick_index = 0;
            event_item_can_take(tool, 1, 1, 1);
            event_cue_issue(tool, 0, 0, 0);
            shop_set_step_offset(SHOP_TOOL_CARRY_WAIT, SHOP_TOOL_PAY, base);
            break;
        case SHOP_TOOL_PAY:
            estate_gold_change(-item_buy_price(item), 1);
            milestone_set_timer(item);
            menu_begin_close();
            menu_request_open(2, 0x9F);
            shop_set_step_offset(SHOP_TOOL_MESSAGE_CLOSED, SHOP_TOOL_FADE_OUT, base);
            break;
        case SHOP_TOOL_FADE_OUT:
            event_item_can_take(tool, 1, 0, 1);
            fade_start_out(20, 0, 2);
            shop_set_step_offset(SHOP_TOOL_FADE_OUT_WAIT, SHOP_TOOL_CHECK, base);
            /* fall through */
        case SHOP_TOOL_FADE_OUT_WAIT:
            if (fade_is_settled() == 1) {
                event_cmd_set_target(0x20, 0, 0, 0);
                game_state.player.position.vx = 1000;
                game_state.player.position.vz = -2800;
                game_state.player.position.vy = 0;
                event_cmd_set_facing(3, 0, 0, 0);
                fade_start_in(20, 0, 2);
                shop_set_step_offset(SHOP_TOOL_FADE_IN_WAIT, SHOP_TOOL_CHECK, base);
            }
            break;
        case SHOP_TOOL_FADE_IN_WAIT:
            if (fade_is_settled() != 1) {
                break;
            }
            shop_set_step_offset(SHOP_TOOL_DONE, SHOP_TOOL_CHECK, base);
            /* fall through */
        case SHOP_TOOL_DONE:
            event_cmd_reset_schedule(CHARACTER_SAIBARA, 0, 0, 0);
            result = 1;
            break;
        case SHOP_TOOL_CANCELLED:
            result = 3;
            break;
        }
    }
    return result;
}

/* The shared buy dialogue's steps: the publish and the carry cue (CARRY_CUE, CARRY_WAIT), the
 * opening message, the press to continue, the ask and its answer, the pay (PAY), the give
 * (FINISH), the leave, and the delivery booking (BOOK_DELIVERY). */
enum ShopGrantStep {
    SHOP_GRANT_START = 0,
    SHOP_GRANT_MESSAGE_CLOSED = 2,
    SHOP_GRANT_MESSAGE_END = 3,
    SHOP_GRANT_ASK = 4,
    SHOP_GRANT_PAY = 6,
    SHOP_GRANT_FINISH = 7,
    SHOP_GRANT_ANSWER = 8,
    SHOP_GRANT_LEAVE = 9,
    SHOP_GRANT_BOOK_DELIVERY = 10,
    SHOP_GRANT_PRESS_WAIT = 11,
    SHOP_GRANT_CARRY_CUE = 12,
    SHOP_GRANT_CARRY_WAIT = 13,
};

/* The messages a refused grant shows: for an item, and for a flag-only grant. */
#define SHOP_GRANT_REFUSE_ITEM 0xBA
#define SHOP_GRANT_REFUSE_FLAG 0xD8

/* The shared buy dialogue for `item` at price item_buy_price(item): opening
 * message (message0, message1), the choice, money check (shop_can_afford),
 * then by its mode: SHOP_GRANT_ITEM adds the item and sets its flag, SHOP_GRANT_RUCKSACK
 * adds one to FarmState.player.rucksackLevel (capped at 2), SHOP_GRANT_FLAG_ONLY sets just
 * the flag. Step 10 books a delivery (estate_marker_stamp). Returns 1 when finished. */
static s32 shop_item_grant(s16 item, s16 message0, s16 message1, s16 mode)
{
    s32 done;
    s32 refuse;
    s16 price;
    s32 choice;
    s32 i;
    VECTOR timing;

    game_state.player.scriptHeld = 0;
    game_state.player.scriptPressed = 0;
    game_state.player.stateFlags |= PLAYER_FLAG_SCRIPTED;
    price = item_buy_price(item);
    refuse = SHOP_GRANT_REFUSE_ITEM;
    if (mode == SHOP_GRANT_FLAG_ONLY) {
        refuse = SHOP_GRANT_REFUSE_FLAG;
    }
    done = 0;
    switch (shop_step) {
    case SHOP_GRANT_START:
        if (mode != SHOP_GRANT_FLAG_ONLY) {
            estate_record_publish(item, 0);
        }
        shop_set_step(SHOP_GRANT_CARRY_CUE, SHOP_GRANT_START);
        break;
    case SHOP_GRANT_CARRY_CUE:
    case SHOP_GRANT_CARRY_WAIT:
        if (shop_item_carry_step(&shop_step, SHOP_GRANT_CARRY_CUE, item) != 0) {
            menu_request_open(message0, message1);
            shop_set_step(SHOP_GRANT_MESSAGE_END, SHOP_GRANT_PRESS_WAIT);
        }
        break;
    case SHOP_GRANT_MESSAGE_CLOSED:
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            shop_restore_step();
        }
        break;
    case SHOP_GRANT_MESSAGE_END:
        if (menu_get_phase() == TEXT_WINDOW_END_WAIT) {
            text_set_input(0);
            shop_restore_step();
        }
        break;
    case SHOP_GRANT_PRESS_WAIT:
        if (input_pad1->pressed & (PAD_CIRCLE | PAD_CROSS)) {
            menu_set_sound_enabled(0);
            shop_set_step(SHOP_GRANT_ASK, SHOP_GRANT_ASK);
        }
        break;
    case SHOP_GRANT_ASK:
        shop_dialogue_tick_index = SHOP_TICK_GOLD_PANEL_REDRAW;
        shop_set_step(SHOP_GRANT_ANSWER, SHOP_GRANT_START);
        shop_result_update(0, -1, 0);
        /* fall through */
    case SHOP_GRANT_ANSWER:
        choice = shop_yes_no_prompt(&current_frame->otTags[0][11], 200, 128);
        switch (choice) {
        case SHOP_ANSWER_PENDING:
            break;
        case SHOP_ANSWER_YES:
            if (shop_can_afford(price) == choice) {
                menu_begin_close();
                menu_request_open(2, 0xBB);
                shop_set_step(SHOP_GRANT_MESSAGE_CLOSED, SHOP_GRANT_PAY);
            } else {
                menu_begin_close();
                menu_request_open(2, 0x4F);
                shop_set_step(SHOP_GRANT_MESSAGE_CLOSED, SHOP_GRANT_FINISH);
            }
            break;
        case SHOP_ANSWER_NO:
            menu_begin_close();
            menu_request_open(2, refuse);
            shop_set_step(SHOP_GRANT_MESSAGE_CLOSED, SHOP_GRANT_FINISH);
            break;
        }
        break;
    case SHOP_GRANT_PAY:
        estate_gold_change(-price, 1);
        shop_set_step(SHOP_GRANT_LEAVE, SHOP_GRANT_START);
        if (mode == SHOP_GRANT_ITEM) {
            estate_record_publish(item, 1);
            event_item_can_take(item, 1, 0, 1);
            shop_set_step(SHOP_GRANT_BOOK_DELIVERY, SHOP_GRANT_START);
        } else if (mode == SHOP_GRANT_RUCKSACK) {
            player_start_rucksack(&game_state.player);
            if (++game_state.player.rucksackLevel >= 3) {
                game_state.player.rucksackLevel = 2;
            }
            game_state.progress.estate.rucksackTimer = 10;
        }
        break;
    case SHOP_GRANT_FINISH:
        if (mode != SHOP_GRANT_FLAG_ONLY) {
            estate_record_publish(item, 1);
        }
        event_item_can_take(item, 1, 0, 1);
        shop_set_step(SHOP_GRANT_LEAVE, SHOP_GRANT_START);
        break;
    case SHOP_GRANT_LEAVE:
        game_state.player.stateFlags &= ~PLAYER_FLAG_SCRIPTED;
        done = 1;
        if (mode == SHOP_GRANT_RUCKSACK) {
            milestone_refresh_all();
        }
        break;
    case SHOP_GRANT_BOOK_DELIVERY:
        timing.vx = -5000;
        timing.vy = 0;
        timing.vz = 2300;
        game_state.progress.estate.basket.count = 0;
        game_state.progress.estate.basket.contentsValue = 0;
        game_state.progress.estate.estateFlags |= ESTATE_FLAG_BASKET;
        for (i = 24; i >= 0; i--) {
            game_state.progress.estate.basket.items[i] = 0;
        }
        estate_marker_stamp(100, timing, 1, 0);
        done = 1;
        game_state.player.stateFlags &= ~PLAYER_FLAG_SCRIPTED;
        break;
    }
    return done;
}

/* The stored-item buy dialogue's steps: the choose menu (from 4, shared with the menu
 * sub-steps), the note about the stored items (3), the count menu (from 11), and the leave. */
enum ShopStoredStep {
    SHOP_STORED_START = 0,
    SHOP_STORED_MESSAGE_CLOSED = 1,
    SHOP_STORED_LEAVE = 2,
    SHOP_STORED_NOTE = 3,
    SHOP_STORED_CHOOSE_MESSAGE = 4,
    SHOP_STORED_CHOOSE_MESSAGE_CLOSE = 5,
    SHOP_STORED_CHOOSE_MESSAGE_END = 6,
    SHOP_STORED_CHOOSE_PROMPT = 7,
    SHOP_STORED_CHOOSE_INPUT = 8,
    SHOP_STORED_CHOOSE_DONE = 9,
    SHOP_STORED_CHOOSE_AFTER_DONE = 10,
    SHOP_STORED_COUNT_MESSAGE = 11,
    SHOP_STORED_COUNT_MESSAGE_CLOSE = 12,
    SHOP_STORED_COUNT_MESSAGE_END = 13,
    SHOP_STORED_COUNT_PROMPT = 14,
    SHOP_STORED_COUNT_INPUT = 15,
    SHOP_STORED_COUNT_DONE = 16,
};

/* A buy dialogue: pick an item from the request's menu, check there is room, take a
 * count, check the money and the room for that many, then pay and store the items.
 * `chosen` is shown when an item is picked, `stored` after a purchase and `cancelled`
 * when the count is cancelled. Returns 1 at step 2. */
s32 shop_buy_stored_item(s16 *chosen, s16 *stored, s16 *cancelled, FarmMenuRequest request) {
    s32 result;
    s16 chosen0;
    s16 chosen1;
    s16 stored0;
    s16 stored1;
    s16 cancelled0;
    s16 cancelled1;
    s32 choice;
    ShopMenuHeader *order;
    ShopMenuHeader *again;

    result = 0;
    game_state.player.stateFlags |= PLAYER_FLAG_SCRIPTED;
    game_state.player.scriptHeld = 0;
    game_state.player.scriptPressed = 0;
    chosen0 = chosen[0];
    chosen1 = chosen[1];
    stored0 = stored[0];
    stored1 = stored[1];
    cancelled0 = cancelled[0];
    cancelled1 = cancelled[1];
    switch (shop_step) {
    case SHOP_STORED_START:
        shop_set_step(SHOP_STORED_CHOOSE_MESSAGE, SHOP_STORED_START);
        break;
    case SHOP_STORED_MESSAGE_CLOSED:
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            shop_restore_step();
        }
        break;
    case SHOP_STORED_CHOOSE_MESSAGE:
    case SHOP_STORED_CHOOSE_MESSAGE_CLOSE:
    case SHOP_STORED_CHOOSE_MESSAGE_END:
    case SHOP_STORED_CHOOSE_PROMPT:
    case SHOP_STORED_CHOOSE_INPUT:
    case SHOP_STORED_CHOOSE_DONE:
    case SHOP_STORED_CHOOSE_AFTER_DONE:
        switch (shop_menu_open_choose(&shop_step, SHOP_STORED_CHOOSE_MESSAGE, request)) {
        case SHOP_MENU_RESULT_NONE:
            break;
        case SHOP_MENU_RESULT_CHOSEN:
            if (shop_item_room(shop_menu_selected_value(shop_choice_menu), 0) == 0) {
                menu_begin_close();
                menu_request_open(2, 0x54);
                shop_set_step(SHOP_STORED_MESSAGE_CLOSED, SHOP_STORED_LEAVE);
            } else {
                shop_menu_header.h8 = 1;
                menu_request_open(chosen0, chosen1);
                shop_set_step(SHOP_STORED_MESSAGE_CLOSED, SHOP_STORED_COUNT_MESSAGE);
            }
            break;
        case SHOP_MENU_RESULT_CANCELLED:
            shop_set_step(SHOP_STORED_LEAVE, SHOP_STORED_START);
            break;
        }
        break;
    case SHOP_STORED_COUNT_MESSAGE:
    case SHOP_STORED_COUNT_MESSAGE_CLOSE:
    case SHOP_STORED_COUNT_MESSAGE_END:
    case SHOP_STORED_COUNT_PROMPT:
    case SHOP_STORED_COUNT_INPUT:
    case SHOP_STORED_COUNT_DONE:
        order = &shop_menu_header;
        choice = shop_menu_choose_next(&shop_step, SHOP_STORED_COUNT_MESSAGE, SHOP_TICK_PANEL_SUBMIT, request, order);
        switch (choice) {
        case SHOP_MENU_RESULT_NONE:
            break;
        case SHOP_MENU_RESULT_CHOSEN:
            if (shop_can_afford(order->h8 * order->ha) == choice) {
                if (shop_item_room(shop_menu_selected_value(shop_choice_menu), 0) < order->h8) {
                    menu_request_open(2, 0xC1);
                    shop_set_step(SHOP_STORED_MESSAGE_CLOSED, SHOP_STORED_COUNT_MESSAGE);
                } else {
                    again = &shop_menu_header;
                    if (shop_item_room(shop_menu_selected_value(shop_choice_menu), 1) < again->h8) {
                        menu_request_open(3, shop_screen_id == SHOP_DIALOGUE_SEEDS ? 0x261 : 0x207);
                        shop_set_step(SHOP_STORED_MESSAGE_CLOSED, SHOP_STORED_NOTE);
                    } else {
                        shop_dialogue_tick_index = SHOP_TICK_GOLD_CHOICE;
                        menu_request_open(stored0, stored1);
                        shop_set_step(SHOP_STORED_MESSAGE_CLOSED, SHOP_STORED_CHOOSE_PROMPT);
                    }
                    estate_gold_change(-(shop_menu_header.h8 * shop_menu_header.ha), 1);
                    shop_item_add(shop_menu_selected_value(shop_choice_menu), shop_menu_header.h8, 0);
                }
            } else {
                menu_request_open(2, 0x4F);
                shop_set_step(SHOP_STORED_MESSAGE_CLOSED, SHOP_STORED_COUNT_MESSAGE);
            }
            break;
        case SHOP_MENU_RESULT_CANCELLED:
            shop_dialogue_tick_index = SHOP_TICK_GOLD_CHOICE;
            menu_request_open(cancelled0, cancelled1);
            shop_set_step(SHOP_STORED_MESSAGE_CLOSED, SHOP_STORED_CHOOSE_PROMPT);
            break;
        }
        break;
    case SHOP_STORED_NOTE:
        menu_request_open(stored0, stored1);
        shop_set_step(SHOP_STORED_MESSAGE_CLOSED, SHOP_STORED_CHOOSE_PROMPT);
        break;
    case SHOP_STORED_LEAVE:
        game_state.player.stateFlags &= ~PLAYER_FLAG_SCRIPTED;
        result = 1;
        break;
    }
    return result;
}

static void shop_draw_gold_panel(void) {
    shop_draw_currency_panel(0);
}

static void shop_draw_medals_panel(void) {
    shop_draw_currency_panel(1);
}

/* Draw the gold panel, or the medals panel when `medals` is set: the amount with its unit,
 * the title and the frame. */
static void shop_draw_currency_panel(s32 medals) {
    s32 amount;
    u_long *panel;
    s32 number;
    s32 titleOffset;
    s32 height;
    s32 layer;
    s32 mask;
    void (*format)(u16 *);
    if (medals != 0) {
        amount = game_state.player.medals;
        titleOffset = offsetof(TextNames, medals_title);
        format = shop_set_medals_unit;
        number = 0xBB;
        height = 119;
        layer = 0x22;
    } else {
        amount = game_state.player.gold;
        titleOffset = offsetof(TextNames, money_title);
        format = shop_set_gold_unit;
        number = 0xE3;
        height = 79;
        layer = 0x22;
    }
    panel = &current_frame->otTags[0][16];
    format(text_format_number(amount, shop_text_buffer, 8));
    window_frame_draw(&current_frame->otTags[0][17], number, 16, height, layer);
    mask = number | 4;
    text_draw_plain(panel, TEXT_AT_OFFSET(text_bank_starts.names + titleOffset), mask, 20);
    text_draw_plain(panel, shop_text_buffer, mask, 34);
}

/* Draw the farm block's panel at (x, y): its title (block.w0, if any) on the
 * first row, then `count` as three digits followed by the text at block.w4,
 * and on the next row `total` in five digits. The frame is one row shorter
 * without a title. */
static void shop_draw_count_total(ShopMenuHeader block, s16 count, s32 total, s16 x, s16 y) {
    s16 row;
    u_long *panel;
    s32 shrink;

    row = y + 4;
    panel = &current_frame->otTags[0][12];
    if (block.w0 == 0) {
        shrink = -SHOP_ROW_HEIGHT;
    } else {
        text_draw_plain(panel, block.w0, x + 4, row);
        shrink = 0;
        row += SHOP_ROW_HEIGHT;
    }
    window_frame_draw(&current_frame->otTags[0][13], x, y, 116, shrink + 48);
    *text_format_number(count, shop_text_buffer, 3) = 0xFFFF;
    text_list_append(shop_text_buffer, block.w4);
    text_draw_plain(panel, shop_text_buffer, x + 32, row);
    ui_draw_icon_0d_phase(panel, x + 4, row);
    ui_draw_icon_0c_phase(panel, x + 96, row);
    row += SHOP_ROW_HEIGHT;
    shop_set_gold_unit(text_format_number(total, shop_text_buffer, 5));
    text_draw_plain(&current_frame->otTags[0][12], shop_text_buffer, x + 16, row);
}

static void shop_set_gold_unit(u16 *text) {
    text_copy_string(text, LOCALIZED_NAME(text_bank.names.gold_unit));
}

static void shop_set_medals_unit(u16 *text) {
    text_copy_string(text, LOCALIZED_NAME(text_bank.names.medals_unit));
}

/* Read the pad for a quantity: left and right change it by 1, up and down by 10, wrapping between
 * 1 and limit. Returns the ShopQuantityInput (confirm, info, cancel) or none. */
static s32 shop_quantity_input(s16 limit, s16 *value) {
    s32 action;
    s32 triggered;
    s32 flags;

    action = 0;
    triggered = input_pad1->triggered;
    flags = input_pad1->pressed;
    if (triggered & PAD_RIGHT) {
        *value = *value + 1;
        sound_play(5);
    } else if (triggered & PAD_LEFT) {
        *value = *value - 1;
        sound_play(5);
    } else if (triggered & PAD_UP) {
        *value = *value + 10;
        sound_play(5);
    } else if (triggered & PAD_DOWN) {
        *value = *value - 10;
        sound_play(5);
    } else if (flags & PAD_CROSS) {
        action = 1;
        sound_play(0);
    } else if (flags & PAD_TRIANGLE) {
        action = 2;
        sound_play(0);
    } else if (flags & PAD_CIRCLE) {
        action = 3;
        sound_play(1);
    }
    if (limit < *value) {
        *value = 1;
    }
    if (*value <= 0) {
        *value = limit;
    }
    return action;
}

static s32 shop_can_afford(s32 price) {
    return !(game_state.player.gold < price);
}

static void shop_set_step(s32 start, s32 end) {
    shop_step = start;
    shop_return_step = end;
}

static void shop_set_step_offset(s32 start, s32 end, s32 offset) {
    shop_step = start + offset;
    shop_return_step = end + offset;
}

static void shop_restore_step(void) {
    shop_step = shop_return_step;
}

static void shop_draw_short_panel(u_long *ot, s32 x, s32 y, s32 columns) {
    shop_draw_panel(0, ot, x, y, columns);
}

static void shop_draw_long_panel(void *display, s16 x, s16 y, s16 columns) {
    shop_draw_panel(1, display, x, y, columns);
}

/* Draw a framed text panel at (x, y): the long text (9 rows) when `large`
 * is set, the short one (4 rows) otherwise. */
static void shop_draw_panel(s32 large, u_long *display, s16 left, s16 top, s16 columnCount)
{
    s16 x = left;
    s16 y = top;
    s16 columns = columnCount;
    s32 rows;
    s32 indent;
    u16 *text;

    if (large) {
        rows = 9;
        text = LOCALIZED_NAME(text_bank.names.buy_dont_buy);
    } else {
        rows = 4;
        text = LOCALIZED_NAME(text_bank.names.shop_yes_no);
    }
    indent = columns * 14 + 4;
    text_draw_cursor(display, x + 4, y + indent);
    text_set_spacing(0, 2);
    text_draw_plain(display, text, x + 12, y + 4);
    text_set_spacing(0, 0);
    window_frame_draw(display, x, y, rows * 8 + 8, 36);
}

/* Move the yes/no cursor (0 yes, 1 no) with up and down; cross picks it, circle picks no. Returns 1
 * when a choice was made. */
static s32 shop_yes_no_input(s16 *value) {
    s32 flags;
    s32 changed;
    s32 next;
    u16 current;

    changed = 0;
    flags = input_pad1->pressed;
    if (flags & PAD_UP) {
        *value = *value - 1;
        sound_play(5);
    } else if (flags & PAD_DOWN) {
        *value = *value + 1;
        sound_play(5);
    } else if (flags & PAD_CROSS) {
        changed = 1;
        sound_play(*value == 1);
    } else if (flags & PAD_CIRCLE) {
        sound_play(1);
        *value = 1;
        changed = 1;
    }
    current = *value;
    if (*value < 0) {
        current = 1;
    }
    *value = current;
    next = 0;
    if ((s16)current < 2) {
        next = current;
    }
    *value = next;
    return changed;
}

static s16 shop_long_panel_state = 0;

/* Draw the long yes/no panel at (x, y) and report the answer: 0 still open, 1 yes, 2 no.
 * x -1 puts the cursor back on yes and x -2 on no; both draw nothing. */
static s32 shop_yes_no_prompt(u_long *ot, s16 x, s16 y) {
    if (x == -1) {
        shop_long_panel_state = 0;
        return 0;
    }
    if (x == -2) {
        shop_long_panel_state = 1;
        return 0;
    }
    shop_draw_long_panel(ot, x, y, shop_long_panel_state);
    if (shop_yes_no_input(&shop_long_panel_state) == 1) {
        if (shop_long_panel_state == 0) {
            shop_long_panel_state = 0;
            return 1;
        }
        shop_long_panel_state = 0;
        return 2;
    }
    return 0;
}

static void shop_menu_clear(FarmMenu *menu) {
    s32 i;
    for (i = 0; i < SHOP_MENU_ROWS; i++) {
        menu->text[i] = 0;
        menu->entryValue[i] = 0;
        menu->entryFlag[i] = 0;
    }
}

/* Draw `menu` at (x, y): up to ten rows of label, value and cursor, then the
 * frame around them. A value is drawn by its row's flag: 0 a price in gold, 2 a
 * price in medals, 3 a row of dashes as wide as the value column, anything else
 * nothing. */
static void shop_menu_draw(FarmMenu *menu, s16 x, s16 y) {
    u_long *display;
    s16 valueX;
    s32 span;
    s16 width; /* the value column's width in characters */
    s32 row;
    s32 k;
    u16 *text;
    u16 **labels;
    s16 *values;
    u8 *flags;
    s32 i;

    /* Whether any row's value is a medal price (flag 2). */
    s32 has_medal_price(u8 *flags) {
        s32 j;

        for (j = 0; j < 10; j++, flags++) {
            if (*flags == 2) {
                return 1;
            }
        }
        return 0;
    }

    display = &current_frame->otTags[0][15];
    valueX = menu->columns[0] * 8 + 28;
    span = menu->columns[1] - menu->columns[0];
    if (has_medal_price(menu->entryFlag)) {
        width = span - 7;
    } else {
        width = span - 2;
    }
    labels = menu->text;
    values = menu->entryValue;
    flags = menu->entryFlag;
    row = 0;
    for (i = 0; i < 10; i++, labels++, values++, flags++) {
        if (*labels != 0) {
            if (i == menu->selected) {
                text_draw_cursor(display, x + 4, y + 4 + row * 14);
            }
            text_draw_plain(display, *labels, x + 12, y + 4 + row * 14);
            text = shop_text_buffer;
            if (*flags == 0) {
                shop_set_gold_unit(text_format_number(item_buy_price(*values), text, width));
            } else if (*flags == 2) {
                shop_set_medals_unit(text_format_number(shop_medal_price((u16)*values), text, width));
            } else if (*flags == 3) {
                for (k = 0; k < width + 1; k++) {
                    text[k] = 0x60;
                }
                shop_text_buffer[k] = 0xFFFF;
            } else {
                text = 0;
            }
            if (text != 0) {
                text_draw_plain(display, text, valueX, y + 4 + row * 14);
            }
            row++;
        }
    }
    window_frame_draw(display, x, y, menu->columns[1] * 8 + 15, row * 14 + 6);
}

/* Move the menu's selection with the d-pad (up: back, down: forward, to the
 * nearest entry with text) and report the button that closes it: 1 cross, 2 triangle
 * (not on the medal menu), 3 circle, else 0. */
static s32 shop_menu_input(FarmMenu *menu) {
    s32 result;
    InputRecord *object;
    s32 triggered;
    s32 flags;
    s16 *selected;

    object = input_pad1;
    result = 0;
    triggered = object->triggered;
    flags = object->pressed;
    selected = &menu->selected;
    if (triggered & PAD_UP) {
        *selected = shop_menu_next_entry(*menu, SHOP_MENU_BACKWARD);
        sound_play(5);
    } else if (triggered & PAD_DOWN) {
        *selected = shop_menu_next_entry(*menu, SHOP_MENU_FORWARD);
        sound_play(5);
    } else if (flags & PAD_CROSS) {
        result = 1;
        sound_play(0);
    } else if (flags & PAD_TRIANGLE) {
        if (shop_dialogue_tick_index != SHOP_TICK_MEDALS_CHOICE) {
            result = 2;
            sound_play(0);
        }
    } else if (flags & PAD_CIRCLE) {
        result = 3;
        sound_play(1);
    }
    return result;
}

/* Search outward from the selected entry, forward when direction is 1 and backward
 * otherwise, for the nearest entry with text, wrapping around the ten. Returns the
 * selected entry if none is found. */
static s16 shop_menu_next_entry(FarmMenu menu, s32 direction) {
    s32 i;
    s16 index;

    for (i = 1; i < 10; i++) {
        if (direction == 1) {
            index = menu.selected + i;
        } else {
            index = menu.selected - i;
        }
        index = (index >= 10) ? index - 10 : index;
        index = (index < 0) ? index + 10 : index;
        if (menu.text[index] != 0) return index;
    }
    return menu.selected;
}

/* The number of entries with text above the selected row (0 when none is above it). The choice
 * panel's height counts this plus one. */
static s32 shop_menu_last_entry_index(FarmMenu menu) {
    s16 count;
    u16 **value;
    s32 remaining;
    s32 ordinal;
    s32 step;
    s32 prev;
    s32 result;

    count = menu.selected;
    result = 0;
    if (count > 0) {
        value = menu.text;
        ordinal = 0x10000;
        step = 0x10000;
        remaining = count;
        do {
            if (*value != 0) {
                prev = ordinal;
                ordinal += step;
                result = prev >> 16;
            }
            remaining--;
            value++;
        } while (remaining != 0);
    }
    return result;
}

/* The selected entry's value: entryValue[selected]. */
static s16 shop_menu_selected_value(FarmMenu menu) {
    return menu.entryValue[menu.selected];
}

/* The selected entry's label: label46[selected]. */
static s16 shop_menu_selected_label(FarmMenu menu) {
    return menu.label46[menu.selected];
}

/* Room left for item `item` in the held slot and the rucksack: 99 per empty
 * slot, 99 less the count for a slot already holding it. 99 when `check` is
 * 0 and event_cmd_has_room(0, 2) reports 1. */
static s16 shop_item_room(s16 item, s32 check) {
    s32 full;
    s32 room;
    s32 n;
    s32 i;
    FarmStoredItem *list;

    full = SHOP_ITEM_STACK_MAX;
    if (check == 0 && event_cmd_has_room(0, 2, 0, 0) == 1) {
        return full;
    }
    room = 0;
    n = math_int_pow(2, game_state.player.rucksackLevel + 1);
    list = &game_state.player.held[HELD_TOOL];
    if (list->id == 0) {
        room = full;
    } else if (list->id == item) {
        room = full - list->count;
    }
    list = game_state.player.tools;
    for (i = 0; i < n; i++) {
        if (list->id == 0) {
            room += full;
        } else if (list->id == item) {
            room = (room + full) - list->count;
        }
        list++;
    }
    return room;
}

/* Store `amount` items of `category`: into the held slot, then the tool bag, 99 per
 * slot, opening free slots as needed. When they cannot take it all (unless mode == 1),
 * the lot goes into the first free one of the 64 spare slots instead. Returns the
 * amount left over. */
static s16 shop_item_add(s16 category, s16 amount, s32 mode) {
    s32 overflow;
    s32 opened;
    s32 count;
    s32 i;
    s32 space;
    s32 fill;
    FarmStoredItem *slot;
    FarmStoredItem *spare;

    if (mode == 1) {
        overflow = 0;
    } else {
        overflow = shop_item_room(category, 1) < amount;
    }
    if (!overflow) {
        count = math_int_pow(2, game_state.player.rucksackLevel + 1);
        opened = 0;
        slot = &game_state.player.held[HELD_TOOL];
        space = 0;
        if (slot->id == 0) {
            space = SHOP_ITEM_STACK_MAX;
        } else if (slot->id == category) {
            space = SHOP_ITEM_STACK_MAX - slot->count;
        }
        if (slot->id == category || slot->id == 0) {
            if (space >= amount) {
                if (slot->id == 0) {
                    slot->count = 0;
                    opened = 1;
                }
                fill = slot->count + amount;
                amount = 0;
            } else {
                fill = SHOP_ITEM_STACK_MAX;
                amount -= space;
            }
            if (fill != 0) {
                if (opened == 1) {
                    picture_load_to_vram(category);
                }
                stored_item_set(slot, category, 0, fill);
            }
        }

        slot = game_state.player.tools;
        for (i = 0; i < count; i++, slot++) {
            space = 0;
            if (slot->id == 0) {
                space = SHOP_ITEM_STACK_MAX;
                opened = 1;
            } else if (slot->id == category) {
                space = SHOP_ITEM_STACK_MAX - slot->count;
            }
            if (slot->id == category || slot->id == 0) {
                if (space >= amount) {
                    if (slot->id == 0) {
                        slot->count = 0;
                    }
                    fill = slot->count + amount;
                    amount = 0;
                } else {
                    fill = SHOP_ITEM_STACK_MAX;
                    amount -= space;
                }
                if (fill != 0) {
                    if (opened == 1) {
                        picture_load_to_vram(category);
                    }
                    stored_item_set(slot, category, 0, fill);
                }
            }
            if (amount <= 0) {
                break;
            }
        }
    } else {
        spare = game_state.progress.estate.toolboxItems;
        for (i = 0; i < ITEM_STORE_SLOTS; i++, spare++) {
            if (spare->id == 0) {
                stored_item_set(spare, category, 0, amount);
                amount = 0;
                break;
            }
        }
    }
    return amount;
}

/* Set a menu header's name, price and count (when non-zero); the unit is "units" unless
 * no_units is set. */
static void shop_price_label_set(ShopMenuHeader *obj, u16 *name, s16 no_units, s16 amount, s16 count) {
    obj->w0 = name;
    obj->ha = amount;
    if (count != 0) obj->h8 = count;
    if (no_units == 0) obj->w4 = LOCALIZED_NAME(text_bank.names.units);
}

/* Why the greenhouse purchase is refused for the held tool and item: 0 when it may go ahead,
 * else 1 to 5, the row of shop_reason_messages to show. */
static s32 shop_tool_upgrade_state(void) {
    s16 tool;
    s16 item;
    s32 adjusted;
    s32 quotient;
    s16 remainder;
    s16 kind;
    u16 limit;

    tool = game_state.player.held[HELD_TOOL].id;
    item = game_state.player.held[HELD_ITEM].id;
    adjusted = tool - 1;
    if ((u32)(adjusted & 0xFFFF) >= GREENHOUSE_TOOLS) {
        return 1;
    }
    quotient = adjusted / GREENHOUSE_KINDS;
    remainder = adjusted - quotient * GREENHOUSE_KINDS;
    kind = greenhouse_kind_index(item);
    if (remainder == 4) {
        return 2;
    }
    if (kind < remainder) {
        return 3;
    }
    if (remainder == kind) {
        return 4;
    }
    limit = game_state.player.toolExp[(s16)quotient];
    if (limit < greenhouse_level_value(kind, 0)) {
        return 5;
    }
    return 0;
}

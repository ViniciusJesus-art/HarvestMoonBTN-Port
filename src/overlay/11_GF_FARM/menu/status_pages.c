/* The farm status pages: the summary, the shipment list, the animal and helper
 * lists, the ledger and the tool levels, the page arrows and the animal counts. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "types.h"
#include "actor.h"
#include "afile.h"
#include "anim.h"
#include "animals.h"
#include "bg_tile.h"
#include "chicken.h"
#include "clock.h"
#include "estate.h"
#include "event_commands.h"
#include "family.h"
#include "farm_main.h"
#include "farm_state.h"
#include "harvest_sprites.h"
#include "item_storage.h"
#include "items.h"
#include "livestock.h"
#include "main.h"
#include "menu_window.h"
#include "pad.h"
#include "player.h"
#include "pond.h"
#include "prim.h"
#include "screen_fade.h"
#include "sound_request.h"
#include "status_pages.h"
#include "text.h"
#include "text_bank.h"
#include "texture_pack.h"
#include "ui_icons.h"

/* The text terminator and the blank glyph of the status pages' message strings. */
#define TEXT_END 0xFFFF
#define TEXT_BLANK 0xFFD
#define STATUS_FADE_FRAMES 10
#define STATUS_CYCLE_FRAMES 120
#define STATUS_PACK_FILE 0xE
#define STATUS_PACK_BYTES 0x8E90
#define STATUS_DIGIT_COUNT 14
#define TOOL_LEVEL_COUNT 5
#define BAR_WIDTH 130
#define BAR_RIGHT 131
#define STATUS_LIST_ROWS 7
#define STATUS_LIST_ORIGIN_Y 48
#define STATUS_LIST_ROW_HEIGHT 24
#define STATUS_AFFECTION_BONUS_STEP 6000    /* a villager's affection: one bonus point per 6000 */
#define STATUS_FRIENDSHIP_BONUS_STEP 25     /* friendship and animal affection: one point per 25 */
/* The tool-level rows are 34 pixels apart and the helper rows 24, in 16.16 fixed point. */
#define STATUS_TOOL_ROW_STEP 0x220000
#define STATUS_HELPER_ROW_STEP 0x180000
#define LEDGER_HIGH_MONEY 5000
#define LEDGER_MID_MONEY 2000
#define LEDGER_BAR_UNIT 10000               /* one bar's worth; the bar fills by the rest */
#define LEDGER_BAR_PIXELS 64

typedef struct FarmPack {
    u8 bytes[0x2D74];
} FarmPack;

typedef struct FarmListRow {
    u8 sprite;
    u8 animation;
    s16 count;
} FarmListRow;

typedef struct {
    u16 id;
    s16 count;
    u8 icon;
    u8 pad5;
} FarmShipmentRow;

typedef void (*DrawRowFn)(s16 row, s16 y);

static void status_draw_clock(void);
static void status_page_tick(void);
static void status_page_close(void);
static void status_draw_summary(void);
static void status_draw_sprite_label(s32 sprite, s32 anim, u16 *text, s16 x, s16 y);
static void status_draw_messages(void);
static void status_draw_list(void);
static void status_draw_entries(void);
static void status_draw_entry(s32 value, s32 sprite, s32 animation, s32 frame, u16 *text, s16 x, s16 y);
static void status_draw_marks(void);
static void status_draw_shipments(void);
static void status_draw_count(s16 count, s16 x, s16 y);
static void status_page_chickens(void);
static void status_draw_chicken_row(s16 n, s16 y);
static void status_page_cows(void);
static void status_draw_cow_row(s16 n, s16 y);
static void status_page_sheep(void);
static void status_draw_sheep_row(s16 n, s16 y);
static void status_list_draw(s16 count, DrawRowFn draw_row);
static void status_page_heading(s32 arg);
static void status_draw_item_values(s32 prizeWinner, s16 season, s16 day, s16 year, s16 x, s16 y);
static void status_page_helpers(void);
static void status_draw_helper(s16 stage, s16 y);
static void status_page_ledger(void);
static void status_draw_ledger_row(s16 slot, s16 y);
static void status_draw_ledger_bars(FarmLedger *entry, s16 y);
static void status_page_tool_levels(void);
static s16 status_tool_level(u16 index);
static s16 status_find_level(s16 arg);
static s32 status_event_item_can_take(s16 arg);
static s32 status_level_matches(s16 arg);
static void status_draw_mark(u16 index, s16 x, s16 y);
static void status_draw_threshold_marks(u16 value, s16 x, s16 y);
static void status_draw_gauge(s16 level, u16 value, s16 x, s16 y);
static void status_draw_level(s16 level, u16 value, s16 x, s16 y);
static s16 status_actor_year(s16 *arg);
static void status_draw_number(s16 number, s16 x, s16 y);
static void status_number_set_suffix(u16 *out, u32 offset);

/* Text for the clock panel's day number and weekday, TEXT_END-terminated. */
static u16 status_day_text[16];
/* Scratch text for the number formatter, shared by the summary and count draws. */
static u16 status_number_scratch[16];
static FarmPack status_pack;
static u16 status_text_line[14];
/* The farm score percentage shown on the summary page. */
static s16 status_farm_percentage;
static s16 status_building_score;
static s16 status_recipe_count;
/* Page sprites, bound from the sprite pack by status_load_sprites. The cursor and mark sprites are shared with the options menu. */
static AnimState status_page_sprites[3];
AnimState status_cursor_sprite;
AnimState status_mark_sprite;

/* A one-shot latch: set once, drawn while set, cleared to reset. */
static s32 status_latch = 0;

static void status_latch_clear(void) {
    status_latch = 0;
}

void status_latch_set(void) {
    if (status_latch == 0) {
        status_latch = 1;
    }
}

void status_latch_reset(void) {
    s32 *p = &status_latch;
    if (*p != 0) *p = 0;
}

void status_latch_draw_clock(void) {
    if (status_latch != 0) {
        status_draw_clock();
    }
}

/* Draw the date and clock panel: the season icon and day number, the weekday (in its
 * own text colour on Saturday and Sunday), then AM/PM, the hour and the minutes (tens
 * digit, a 0, and the half-ten marker) from the game clock. */
static void status_draw_clock(void)
{
    u_long *ot;
    u_long *text;
    s32 morning;
    s32 saved;
    s32 color;
    u16 *glyphs;
    s32 x;
    s32 y;

    ot = current_frame->otTags[0];
    text = &ot[35];
    morning = game_state.progress.clock.time.hm.hour < 12;
    window_frame_draw_at(&ot[36], 1, 228, 16, 78, 32);
    y = 19;
    x = 276;
    ui_draw_icon_2b_sub(text, 232, 19, game_state.progress.clock.date.season);
    *text_format_number(game_state.progress.clock.date.day + 1, status_day_text, 2) = TEXT_END;
    text_draw_plain(text, status_day_text, 252, 19);
    *text_write_weekday(game_state.progress.clock.date.weekday, status_day_text) = TEXT_END;
    saved = text_get_palette();
    switch (game_state.progress.clock.date.weekday) {
    default:
        color = saved;
        break;
    case WEEKDAY_SATURDAY:
        color = 5;
        break;
    case WEEKDAY_SUNDAY:
        color = 4;
        break;
    }
    text_set_palette(color);
    text_draw_plain(text, status_day_text, x, y);
    text_set_palette(saved);
    x = 232;
    y = 33;
    glyphs = status_day_text;
    if (!morning) {
        glyphs[0] = 0xF;
    } else {
        glyphs[0] = 0;
    }
    glyphs[1] = 0xC;
    glyphs[2] = TEXT_BLANK;
    glyphs += 3;
    glyphs = text_format_number(morning ? game_state.progress.clock.time.hm.hour : game_state.progress.clock.time.hm.hour - 12, glyphs, 2);
    *glyphs = TEXT_BLANK;
    glyphs = text_format_number(game_state.progress.clock.time.hm.minute / 10, glyphs + 1, 1);
    glyphs = text_format_number(0, glyphs, 1);
    *glyphs = TEXT_END;
    text_draw_plain(text, status_day_text, x, y);
    status_day_text[0] = 0x66;
    status_day_text[1] = TEXT_END;
    text_draw_semi(text, status_day_text, 272, y,
                  (game_state.progress.clock.time.hm.minute - game_state.progress.clock.time.hm.minute / 10 * 10) / 5);
}

/* The steps of the status page sequence. Each step waits for the fade it started. */
enum StatusStep {
    STATUS_STEP_IDLE = 0,
    STATUS_STEP_FADE_OUT = 1,           /* the fade out when the pages open */
    STATUS_STEP_LOAD = 2,               /* after the fade: the sprites load, the fade in starts */
    STATUS_STEP_FADE_IN = 3,            /* waits for the fade in */
    STATUS_STEP_PAGE = 4,               /* the page runs */
    STATUS_STEP_CLOSE_FADE_OUT = 5,     /* the fade out when the pages close */
    STATUS_STEP_CLOSE_FADE_IN = 6,      /* after that fade the fade in starts; then the page unpauses */
};

static s16 status_sequence_step = STATUS_STEP_IDLE;

u16 status_page = 0;

s16 status_list_top = 0;

u16 status_cycle_frames = 0;

static u16 status_band_thresholds[5] = {
    0, 6000, 18000, 36000, 0xFFFF,
};

static FarmListRow status_list_rows[5] = {{1, 4, 0}, {1, 2, 0}, {1, 3, 0}, {1, 5, 0}, {3, 0x16, 0}};

typedef struct {
    u32 bonus;
    u16 *name;
    u8 sprite;
    u8 animation;
    u8 padA[2];
} FarmSummaryEntry;

/* The four summary entries: filled when the summary is built, then drawn. */
static FarmSummaryEntry status_summary_entries[4] = {
    {0, 0, 0, 0},
    {0, 0, 0, 6},
    {0, 0, 1, 0},
    {0, 0, 1, 1},
};

/* Four {message, value} pairs; the values are filled in when the summary is built. */
typedef struct {
    s16 message;
    s16 value;
} FarmLowFeed;

static FarmLowFeed status_low_feed[4] = {
    {8, 0},
    {7, 0},
    {6, 0},
    {9, 0},
};

static FarmShipmentRow status_shipment_rows[21] = {
    {65, 0, 1, 0},
    {68, 0, 0, 0},
    {71, 0, 8, 0},
    {74, 0, 0x0C, 0},
    {77, 0, 5, 0},
    {0x0122, 0, 0x0F, 0},
    {136, 0, 0x11, 0},
    {66, 0, 6, 0},
    {69, 0, 3, 0},
    {72, 0, 7, 0},
    {75, 0, 0x0A, 0},
    {78, 0, 0x0E, 0},
    {139, 0, 0x12, 0},
    {142, 0, 0x10, 0},
    {67, 0, 4, 0},
    {70, 0, 9, 0},
    {73, 0, 2, 0},
    {76, 0, 0x0B, 0},
    {79, 0, 0x0D, 0},
    {145, 0, 0x14, 0},
    {148, 0, 0x13, 0},
};

/* Reset the page state, then rebuild the farm summary: name line, score inputs, list and shipment counts. */
static void status_summary_build(void) {
    s32 i;
    s32 k;
    u16 *src;
    s32 n;
    s32 bonus0;
    s32 bonus1;
    s32 bonus2;
    s32 bonus3;

    status_sequence_step = STATUS_STEP_IDLE;
    status_page = 0;
    status_list_top = 0;
    status_cycle_frames = 0;
    for (i = 0; i < 14; i++) {
        u16 c = (game_state.progress.estate.farmName)[i];

        if (c != TEXT_END) {
            status_text_line[i] = c;
        } else {
            status_text_line[i] = TEXT_BLANK;
            src = LOCALIZED(text_bank.messages.farm);
            for (k = 0, i++; i < 14; k++, i++) {
                status_text_line[i] = src[k];
                if (src[k] == TEXT_END) {
                    goto named;
                }
            }
        }
    }
named:
    status_text_line[13] = TEXT_END;

    n = (game_state.progress.estate.houseLevel < 3) ? game_state.progress.estate.houseLevel : 2;
    n += (game_state.progress.estate.barnLevel < 2) ? game_state.progress.estate.barnLevel : 1;
    n += (game_state.progress.estate.coopLevel < 2) ? game_state.progress.estate.coopLevel : 1;
    n += (game_state.progress.estate.greenhouseLevel < 2) ? game_state.progress.estate.greenhouseLevel : 1;
    n *= 20;
    status_building_score = n;

    status_recipe_count = 0;
    for (i = 0; i < RECIPE_SLOTS; i++) {
        if (game_state.progress.estate.recipeRecords[i].id != 0)
            status_recipe_count++;
    }

    if (game_state.family.wifeIndex >= 0) {
        bonus0 = game_state.villagers[game_state.family.wifeVillager].affection / STATUS_AFFECTION_BONUS_STEP;
        status_summary_entries[0].bonus = bonus0;
        status_summary_entries[0].name = (u16 *)family_table_lookup();
        status_summary_entries[0].animation = game_state.family.wifeIndex + 1;
    } else {
        bonus0 = 0;
        status_summary_entries[0].name = 0;
    }
    if (game_state.family.childNamed == 1) {
        bonus1 = game_state.villagers[CHARACTER_CHILD].friendship / STATUS_FRIENDSHIP_BONUS_STEP;
        status_summary_entries[1].name = game_state.family.childName;
        status_summary_entries[1].bonus = bonus1;
    } else {
        bonus1 = 0;
        status_summary_entries[1].name = 0;
    }
    if (game_state.animals.dog.actor.kind != 0) {
        bonus2 = game_state.animals.dog.actor.affection / STATUS_FRIENDSHIP_BONUS_STEP;
        status_summary_entries[2].name = game_state.animals.dog.actor.name;
        status_summary_entries[2].bonus = bonus2;
    } else {
        bonus2 = 0;
        status_summary_entries[2].name = 0;
    }
    if (game_state.animals.horse.actor.kind != 0) {
        bonus3 = game_state.animals.horse.actor.affection / STATUS_FRIENDSHIP_BONUS_STEP;
        status_summary_entries[3].name = game_state.animals.horse.actor.name;
        status_summary_entries[3].bonus = bonus3;
    } else {
        bonus3 = 0;
        status_summary_entries[3].name = 0;
    }

    status_list_rows[0].count = chicken_count_unflagged();
    status_list_rows[1].count = livestock_count_cattle();
    status_list_rows[2].count = livestock_count_sheep();
    status_list_rows[3].count = status_count_pond_fish();
    status_list_rows[4].count = (game_state.progress.estate.discardCount < 100) ? game_state.progress.estate.discardCount : 99;

    status_low_feed[0].value = (game_state.progress.estate.chickenFeed < 1000) ? game_state.progress.estate.chickenFeed : 999;
    status_low_feed[1].value = (game_state.progress.estate.fodder < 1000) ? game_state.progress.estate.fodder : 999;
    status_low_feed[2].value = (game_state.progress.estate.fishFood < 1000) ? game_state.progress.estate.fishFood : 999;
    status_low_feed[3].value = (game_state.progress.estate.lumber < 1000) ? game_state.progress.estate.lumber : 999;

    status_farm_percentage = status_building_score / 10 + status_recipe_count / 8 + bonus0 + bonus1 + bonus2 + bonus3 +
                 status_list_rows[0].count + status_list_rows[1].count + status_list_rows[2].count;

    for (i = 0; (u32)i < 21; i++) {
        status_shipment_rows[i].count = game_state.progress.estate.shipment.counts[estate_item_slot_index(status_shipment_rows[i].id)];
    }
}

/* Start the status pages: fade out, then the page sequence loads them. */
void status_page_open(void) {
    if (status_sequence_step == STATUS_STEP_IDLE) {
        status_sequence_step = STATUS_STEP_FADE_OUT;
        fade_start_out(STATUS_FADE_FRAMES, 0, 2);
    }
}

static u32 status_sprite_offsets[5] = {
    0x00000000, 0x00000A88, 0x00001500, 0x00001F64, 0x00002BB0};

/* Build the summary, read the sprite pack and bind the page sprites from it. */
static void status_load_sprites(void) {
    u32 i;

    sound_play(SOUND_MENU_OPEN_CLOSE);
    status_summary_build();
    status_sequence_step = STATUS_STEP_FADE_OUT;
    DrawSync(0);
    ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
    afile_read_bytes(STATUS_PACK_FILE, 0, STATUS_PACK_BYTES, STAGING_BUFFER, 1, 1);
    while (afile_read_bytes(STATUS_PACK_FILE, 0, STATUS_PACK_BYTES, STAGING_BUFFER, 1, 0) == 0) {
    }
    texture_pack_load(STAGING_BUFFER + sizeof(FarmPack));
    status_pack = *(FarmPack *)STAGING_BUFFER;
    for (i = 0; i < 5; i++) {
        anim_bind_file(&status_page_sprites[i], (AnimFile *)(status_pack.bytes + status_sprite_offsets[i]));
    }
}

/* Advance the page sequence (steps 1 to 6) as the screen fades out and back in.
 * The background cursor animates during steps 3 to 5. */
void status_page_step(void)
{
    s32 ready;

    ready = fade_is_settled();
    if (status_sequence_step >= STATUS_STEP_FADE_IN && status_sequence_step <= STATUS_STEP_CLOSE_FADE_OUT) {
        bg_tile_mode_cursor(&current_frame->otTags[0][29]);
    }
    switch (status_sequence_step) {
    case STATUS_STEP_FADE_OUT:
        if (ready == 1) {
            status_sequence_step = STATUS_STEP_LOAD;
            game_state.globalFlags |= GLOBAL_FLAG_TRANSITION_BEGIN;
        }
        break;
    case STATUS_STEP_LOAD:
        status_load_sprites();
        fade_start_in(STATUS_FADE_FRAMES, 0, 2);
        status_sequence_step = STATUS_STEP_FADE_IN;
        break;
    case STATUS_STEP_FADE_IN:
        if (ready == 1) {
            status_sequence_step = STATUS_STEP_PAGE;
        }
        break;
    case STATUS_STEP_PAGE:
        status_page_tick();
        break;
    case STATUS_STEP_CLOSE_FADE_OUT:
        if (ready == 1) {
            fade_start_in(STATUS_FADE_FRAMES, 0, 2);
            status_sequence_step = STATUS_STEP_CLOSE_FADE_IN;
            game_state.globalFlags |= GLOBAL_FLAG_TRANSITION_END;
        }
        break;
    case STATUS_STEP_CLOSE_FADE_IN:
        if (ready == 1) {
            status_sequence_step = STATUS_STEP_IDLE;
            farm_unpause();
        }
        break;
    }
}

static void (*status_page_handlers[8])(void) = {
    status_draw_summary, status_draw_shipments, status_page_chickens, status_page_cows, status_page_sheep,
    status_page_helpers, status_page_ledger, status_page_tool_levels,
};

/* Draw the page frame, the title and the side arrows, handle the page and close buttons,
 * draw the current page, then advance the cycle timer. */
static void status_page_tick(void)
{
    s32 flags;
    s32 timer;

    window_frame_draw_at(&current_frame->otTags[0][28], 1, 16, 32, 288, 192);
    window_frame_draw_at(&current_frame->otTags[0][27], 1, 60, 16, 200, 32);
    status_draw_side_arrows();
    text_draw_plain(&current_frame->otTags[0][21], status_text_line, 62, 26);

    flags = input_pad1->triggered;
    if (flags & PAD_RIGHT) {
        sound_play(SOUND_MENU_CURSOR);
        status_list_top = 0;
        options_page_next();
    } else if (flags & PAD_LEFT) {
        sound_play(SOUND_MENU_CURSOR);
        status_list_top = 0;
        options_page_prev();
    }
    if (input_pad1->pressed & (PAD_CIRCLE | PAD_CROSS | PAD_START)) {
        status_page_close();
    }

    (*status_page_handlers[(s16)status_page])();

    timer = status_cycle_frames + 1;
    status_cycle_frames = timer;
    if (((s16) timer) >= STATUS_CYCLE_FRAMES) {
        status_cycle_frames = 0;
    }
}

static void status_page_close(void) {
    sound_play(SOUND_MENU_OPEN_CLOSE);
    fade_start_out(STATUS_FADE_FRAMES, 0, 2);
    status_sequence_step = STATUS_STEP_CLOSE_FADE_OUT;
}

/* Draw the summary page: the score, the player's gold, the named entries, the list, the marks and the low-stock messages. */
static void status_draw_summary(void) {
    s32 value;

    anim_draw_sprt_cursor(&current_frame->otTags[0][26], status_page_sprites, 0, 0, 24, 48);
    status_draw_glyph_run_5(game_state.progress.estate.powerBerries, 120, 60);
    text_draw_plain(&current_frame->otTags[0][21], game_state.player.name, 48, 50);

    status_number_set_suffix(text_format_number(status_building_score, status_number_scratch, 3), offsetof(TextMessages, glyph_8f));
    status_draw_sprite_label(3, 0xB, status_number_scratch, 40, 72);
    status_number_set_suffix(text_format_number(status_recipe_count, status_number_scratch, 3), offsetof(TextMessages, pieces));
    status_draw_sprite_label(3, 0xC, status_number_scratch, 40, 96);

    status_draw_entries();

    value = game_state.player.gold;
    if (value >= -FARM_MONEY_MAX) {
        if (value > FARM_MONEY_MAX) value = FARM_MONEY_MAX;
    } else {
        value = -FARM_MONEY_MAX;
    }
    status_number_set_suffix(text_format_number(value, status_number_scratch, 8), offsetof(TextMessages, gold));
    status_draw_sprite_label(3, 0xA, status_number_scratch, 140, 48);

    status_draw_list();
    status_draw_marks();
    status_draw_messages();

    status_number_set_suffix(text_format_number(status_farm_percentage, status_number_scratch, 3), offsetof(TextMessages, glyph_8f));
    text_draw_plain(&current_frame->otTags[0][21], status_number_scratch, 186, 26);
}

static void status_draw_sprite_label(s32 sprite, s32 anim, u16 *text, s16 x, s16 y) {
    anim_draw_sprt_cursor(&current_frame->otTags[0][26], &status_page_sprites[sprite], anim & 0xFF, 0,
                  x, y);
    text_draw_plain(&current_frame->otTags[0][21], text, x + 24, y + 4);
}

/* Draw the four low-stock messages with their values. */
static void status_draw_messages(void) {
    u32 count;
    s32 rowOffset;
    s32 rowTop;
    s16 y;
    s16 message;
    FarmLowFeed *feed;
    u16 *text;

    count = 0;
    feed = status_low_feed;
    text = status_number_scratch;
    do {
        *text_format_number(feed->value, text, 3) = TEXT_END;
        rowOffset = (count + 1) * 24;
        rowTop = rowOffset + 48;
        message = feed->message;
        if (count & 2)
            y = rowOffset + 72;
        else
            y = rowTop;
        status_draw_sprite_label(3, message, text, 212, y);
        count = count + 1;
        feed++;
    } while (count < 4U);
}

/* Format and draw the five rows of status_list_rows, 24 pixels apart from y = 72. */
static void status_draw_list(void)
{
    u32 count;
    FarmListRow *row;
    u16 *text;

    count = 0;
    row = status_list_rows;
    text = status_number_scratch;
    do {
        *text_format_number(row->count, text, 2) = TEXT_END;
        status_draw_sprite_label(row->sprite, row->animation, text, 140, (count + 1) * 24 + 48);
        count++;
        row++;
    } while (count < sizeof(status_list_rows) / sizeof(status_list_rows[0]));
}

/* Draw the four summary entries that have a name. */
static void status_draw_entries(void) {
    u32 slot;
    s16 x;
    s32 yFixed;
    FarmSummaryEntry *entry;

    slot = 0;
    entry = status_summary_entries;
    x = 24;
    yFixed = 0x780000;
    do {
        if (entry->name != 0) {
            status_draw_entry(entry->bonus, entry->sprite, entry->animation, 0, entry->name, x,
                          yFixed >> 16);
        }
        yFixed += 0x180000;
        slot++;
        entry++;
    } while (slot < 4);
}

typedef struct FarmStatusMark {
    u32 mask;
    u8 frame;
    u8 pad05[3];
} FarmStatusMark;

static void status_draw_entry(s32 value, s32 sprite, s32 animation, s32 frame, u16 *text, s16 x, s16 y) {
    anim_draw_sprt_cursor(&current_frame->otTags[0][26], &status_page_sprites[sprite], animation & 0xFF,
                  frame & 0xFF, x, y);
    text_draw_plain(&current_frame->otTags[0][21], text, x + 24, y + 2);
    status_draw_glyph_run_4(value, x + 96, y + 16);
}

/* Estate flag bits that each put a mark on the summary, with the mark's frame. */
static FarmStatusMark status_mark_table[6] = {
    {ESTATE_FLAG_CATCH_ED, 0x13}, {ESTATE_FLAG_CATCH_EE, 0x12}, {ESTATE_FLAG_CATCH_EF, 0x15},
    {ESTATE_FLAG_CATCH_F0, 0x11}, {ESTATE_FLAG_CATCH_F1, 0x10}, {ESTATE_FLAG_CATCH_F2, 0x14},
};

/* Draw a mark at the bottom of the summary for each estate flag that is set, left to right:
 * the first at x 140, each next one 24 pixels further on. */
static void status_draw_marks(void) {
    s32 count;
    s32 x;
    s32 markX;
    s32 markPixel;
    short height;
    FarmStatusMark *entry;
    FarmState *farm;

    count = 0;
    x = 140;
    height = 192;
    farm = &game_state;
    entry = status_mark_table;
    markX = 0xA40000;
    do {
        if ((farm->progress.estate.estateFlags & entry->mask) != 0) {
            anim_draw_sprt_cursor(&current_frame->otTags[0][26], &status_cursor_sprite, entry->frame, 0, x,
                          height);
            markPixel = 16;
            markPixel = markX >> markPixel;
            markX += 0x180000;
            x = markPixel;
        }
        count++;
        entry++;
    } while (count < 6U);
}

/* Draw the heading, then a 3x7 grid of 21 entries: each entry's icon and,
 * beside it, its count. */
static void status_draw_shipments(void)
{
    u32 i;
    s16 x;
    s16 y;
    const FarmShipmentRow *entry;

    text_draw_plain(&current_frame->otTags[0][21], LOCALIZED(text_bank.messages.amount_shipped), 186, 20);
    for (i = 0; i < 21; i++) {
        entry = &status_shipment_rows[i];
        x = ((s32)i / 7) * 88 + 36;
        y = ((s32)i % 7) * 24 + 50;
        anim_draw_sprt_cursor(&current_frame->otTags[0][26], &status_page_sprites[2], entry->icon, 0, x, y);
        x += 30;
        y += 6;
        status_draw_count(entry->count, x, y);
    }
}

static void status_draw_count(s16 count, s16 x, s16 y) {
    if (count >= 1000) count = 999;
    *text_format_number(count, status_number_scratch, 3) = TEXT_END;
    text_draw_plain(&current_frame->otTags[0][21], status_number_scratch, x, y);
}

static void status_page_chickens(void) {
    status_page_heading(0);
    status_list_draw(status_list_rows[0].count, status_draw_chicken_row);
}

/* Draw the n-th chicken that is not claimed, at row y. */
static void status_draw_chicken_row(s16 n, s16 y) {
    FarmChicken *chicken;
    s32 i;
    s16 ordinal;
    s32 icon;
    s32 sick;

    ordinal = 0;
    chicken = game_state.animals.chickens;
    for (i = 0; i < MAX_CHICKENS; i++, chicken++) {
        if (chicken->actor.kind != 0 && !(chicken->actor.flags & ACTOR_FLAG_CLAIMED)) {
            ordinal++;
        }
        if (ordinal == n) {
            break;
        }
    }
    if (i == MAX_CHICKENS) {
        return;
    }
    status_draw_number(n, 24, y + 4);
    sick = 0;
    if (chicken->actor.kind == ITEM_CHICK) {
        icon = 0xE;
    } else {
        icon = 0xF;
        sick = chicken->daysSick != 0;
    }
    status_draw_entry(chicken->actor.affection / STATUS_FRIENDSHIP_BONUS_STEP, 1, icon, sick, chicken->actor.name, 48, y);
    status_draw_item_values(chicken->actor.flags & ACTOR_FLAG_PRIZE_WINNER, chicken->actor.date.season, chicken->actor.date.day,
                  status_actor_year(&chicken->actor.date.year),
                  176, y + 4);
}

static void status_page_cows(void) {
    status_page_heading(1);
    status_list_draw(status_list_rows[1].count, status_draw_cow_row);
}

/* Draw the n-th head of cattle (1-based) at row y. */
static void status_draw_cow_row(s16 n, s16 y) {
    FarmLivestock *cow;
    s32 i;
    s16 ordinal;
    s32 icon;

    ordinal = 0;
    cow = game_state.animals.livestock;
    for (i = 0; i < MAX_LIVESTOCK; i++, cow++) {
        if ((u32)(cow->actor.kind - ITEM_CALF) < 4) {
            ordinal++;
        }
        if (ordinal == n) {
            break;
        }
    }
    if (i == MAX_LIVESTOCK) {
        return;
    }
    status_draw_number(n, 24, y + 4);
    if (cow->pregnancy != 0) {
        icon = 9;
    } else if (cow->actor.kind == ITEM_CALF) {
        icon = 6;
    } else if (cow->actor.kind == ITEM_YOUNG_COW) {
        icon = 7;
    } else {
        icon = 8;
    }
    status_draw_entry(cow->actor.affection / STATUS_FRIENDSHIP_BONUS_STEP, 1, icon, 0, cow->actor.name, 48, y);
    status_draw_item_values(cow->actor.flags & ACTOR_FLAG_PRIZE_WINNER, cow->actor.date.season, cow->actor.date.day,
                  status_actor_year(&cow->actor.date.year),
                  176, y + 4);
}

static void status_page_sheep(void) {
    status_page_heading(2);
    status_list_draw(status_list_rows[2].count, status_draw_sheep_row);
}

/* Draw the n-th sheep (1-based) at row y. */
static void status_draw_sheep_row(s16 n, s16 y) {
    FarmLivestock *item;
    s32 i;
    s16 ordinal;
    s32 icon;

    ordinal = 0;
    item = game_state.animals.livestock;
    for (i = 0; i < 20; i++, item++) {
        if ((u32)(item->actor.kind - ITEM_LAMB) < 3) {
            ordinal++;
        }
        if (ordinal == n) {
            break;
        }
    }
    if (i == 20) {
        return;
    }
    status_draw_number(n, 24, y + 4);
    if (item->pregnancy != 0) {
        icon = 0xD;
    } else if (item->actor.kind == ITEM_LAMB) {
        icon = 0xA;
    } else {
        icon = 0xC;
    }
    status_draw_entry(item->actor.affection / STATUS_FRIENDSHIP_BONUS_STEP, 1, icon, 0, item->actor.name, 48, y);
    status_draw_item_values(item->actor.flags & ACTOR_FLAG_PRIZE_WINNER, item->actor.date.season, item->actor.date.day + 1,
                  status_actor_year(&item->actor.date.year), 176, y + 4);
}

/* Scroll a list of `count` rows seven at a time with up/down, draw the
 * scroll arrows, then hand each visible row (1-based) and its screen y
 * to `draw_row`. */
static void status_list_draw(s16 count, DrawRowFn draw_row)
{
    DrawRowFn draw = draw_row;
    s16 row;
    s16 y;

    if (count > STATUS_LIST_ROWS) {
        if (input_pad1->triggered & PAD_UP) {
            sound_play(SOUND_MENU_CURSOR);
            status_list_top--;
        } else if (input_pad1->triggered & PAD_DOWN) {
            sound_play(SOUND_MENU_CURSOR);
            status_list_top++;
        }
    }
    if (count < status_list_top + STATUS_LIST_ROWS) {
        status_list_top = 0;
    }
    if (status_list_top < 0) {
        status_list_top = count - STATUS_LIST_ROWS;
    }
    if (status_list_top > 0) {
        status_draw_arrow_up();
    }
    if (status_list_top + STATUS_LIST_ROWS < count) {
        status_draw_arrow_down();
    }

    y = STATUS_LIST_ORIGIN_Y;
    for (row = status_list_top; row < status_list_top + STATUS_LIST_ROWS; row++, y += STATUS_LIST_ROW_HEIGHT) {
        draw(row + 1, y);
    }
}

static u8 status_heading_xs[4] = {
    0x0F, 0x07, 0x0C, 0x00,
};

static void status_page_heading(s32 page) {
    FrameState **q = &current_frame;
    u8 x = status_heading_xs[page];

    anim_draw_sprt_cursor(&(*q)->otTags[0][26], &status_page_sprites[1], x, 0, 186, 20);
    text_draw_plain(&(*q)->otTags[0][21], LOCALIZED(text_bank.messages.name_panel_title), 210, 26);
}

/* Build a date label in the number scratch: fifteen blank glyphs (TEXT_BLANK), an optional
 * 0x90 lead glyph for a prize winner, then the day and the year as two-digit numbers with
 * fixed glyphs between them. Draw it at (x, y) and the season icon 16 pixels to its right. */
static void status_draw_item_values(s32 prizeWinner, s16 season, s16 day, s16 year, s16 x, s16 y)
{
    u16 *text;
    s32 i;

    for (i = 0; i < 15; i++) {
        status_number_scratch[i] = TEXT_BLANK;
    }
    if (prizeWinner) {
        status_number_scratch[0] = 0x90;
    }
    text = status_number_scratch;
    text[4] = 0x71;
    text_format_number(day, &text[5], 2);
    text[8] = 0;
    text[9] = 0x26;
    text[10] = 0x24;
    *text_format_number(year, &text[11], 2) = TEXT_END;
    text_draw_plain(&current_frame->otTags[0][21], text, x, y);
    ui_draw_icon_2b_sub(&current_frame->otTags[0][26], x + 16, y, season);
}

/* Draw the seven helper rows, each 24 pixels below the last. */
static void status_page_helpers(void) {
    s32 stage;
    s32 xFixed;
    s32 nextY;
    s32 y;

    stage = 0;
    xFixed = 0x480000;
    text_draw_plain(&current_frame->otTags[0][21], LOCALIZED(text_bank.messages.help), 186, 26);
    y = 0x30;
    do {
        nextY = xFixed >> 16;
        status_draw_helper(stage, y);
        y = nextY;
        stage++;
        xFixed += STATUS_HELPER_ROW_STEP;
    } while (stage < HELPER_COUNT);
}

/* Draw helper `stage`'s row at height y: its marker, its name and affection / 25,
 * a badge if it is flagged, then its job icon with the days left, or the empty label. */
static void status_draw_helper(s16 stage, s16 y)
{
    FarmHelper *helper;
    u_long *ot;
    s32 icon;
    u16 *src;
    u16 *text;

    helper = &game_state.helpers[stage];
    ot = &current_frame->otTags[0][26];
    status_draw_number(stage + 1, 24, y + 4);
    status_draw_entry(helper->actor.affection / STATUS_FRIENDSHIP_BONUS_STEP, 0, 7, stage, helper->actor.name, 40, y);
    if (helper->flags & HELPER_BIT5) {
        anim_draw_sprt_cursor(ot, status_page_sprites, 8, 0, 150, y);
    }
    if (helper->flags & HELPER_WATER) {
        icon = 0xD;
    } else if (helper->flags & HELPER_ANIMALS) {
        icon = 0xE;
    } else if (helper->flags & HELPER_HARVEST) {
        icon = 0xF;
    } else {
        icon = -1;
    }
    if (icon >= 0) {
        src = LOCALIZED(text_bank.messages.status_days_left);
        anim_draw_sprt_cursor(ot, &status_cursor_sprite, icon, 0, 150, y);
        text = text_format_number(helper->jobDays, status_number_scratch, 2);
        *text++ = TEXT_BLANK;
        while ((*text = *src) != TEXT_END) {
            src++;
            text++;
        }
        text = status_number_scratch;
    } else {
        text = LOCALIZED(text_bank.messages.status_free);
    }
    text_draw_plain(&current_frame->otTags[0][21], text, 198, y + 4);
}

/* Draw the ledger page: the two headings, the first season's earnings and
 * expenses as seven-digit numbers, then the four season rows. */
static void status_page_ledger(void) {
    u_long *ot;
    u16 *text;
    s32 amount;
    s32 value;
    s32 i;
    s16 y;

    ot = &current_frame->otTags[0][21];
    text = status_number_scratch;
    text[1] = TEXT_BLANK;
    text_draw_plain(ot, LOCALIZED(text_bank.messages.earnings_expenses), 178, 20);
    text_draw_plain(ot, LOCALIZED(text_bank.messages.this_months_accounts), 32, 64);

    amount = game_state.progress.estate.ledger[0].earnings;
    if (amount >= -FARM_MONEY_MAX) {
        value = amount;
        if (value > FARM_MONEY_MAX) {
            value = FARM_MONEY_MAX;
        }
    } else {
        value = -FARM_MONEY_MAX;
    }
    status_number_scratch[0] = 0x68;
    status_number_set_suffix(text_format_number(value, &status_number_scratch[2], 7), offsetof(TextMessages, gold));
    text_draw_plain(ot, status_number_scratch, 172, 64);

    amount = game_state.progress.estate.ledger[0].expenses;
    if (amount >= -FARM_MONEY_MAX) {
        value = amount;
        if (value > FARM_MONEY_MAX) {
            value = FARM_MONEY_MAX;
        }
    } else {
        value = -FARM_MONEY_MAX;
    }
    status_number_scratch[0] = 0x69;
    status_number_set_suffix(text_format_number(value, &status_number_scratch[2], 7), offsetof(TextMessages, gold));
    text_draw_plain(ot, status_number_scratch, 172, 78);

    y = 104;
    for (i = 0; i < 4; i++) {
        status_draw_ledger_row(i, y);
        y += 32;
    }
}

#define TEXT_OFFSET(name) offsetof(TextMessages, name)
static s16 status_ledger_offsets[10] = {
    TEXT_OFFSET(go_getter), TEXT_OFFSET(spend_too_much), TEXT_OFFSET(idler),
    TEXT_OFFSET(satisfactory), TEXT_OFFSET(you_should_save), TEXT_OFFSET(play_too_much),
    TEXT_OFFSET(hard_worker), TEXT_OFFSET(keep_working), TEXT_OFFSET(try_harder),
    0,
};

/* Draw ledger row `slot` at height y: the season icon, the two bars, then a
 * label picked from the row's expenses and earnings (both zero: the empty label). */
static void status_draw_ledger_row(s16 slot, s16 y) {
    s16 season;
    s32 expenseGrade;
    s32 earningsGrade;
    u16 *text;
    u_long *dst;

    dst = &current_frame->otTags[0][21];
    season = game_state.progress.clock.date.season;
    season += 4;
    season -= slot;
    ui_draw_icon_2b_sub(dst, 24, y, season % 4);
    status_draw_ledger_bars(&game_state.progress.estate.ledger[slot], y + 2);

    expenseGrade = 0;
    if (game_state.progress.estate.ledger[slot].expenses < LEDGER_HIGH_MONEY) {
        expenseGrade = 2;
        if (game_state.progress.estate.ledger[slot].expenses >= LEDGER_MID_MONEY) {
            expenseGrade = 1;
        }
    }
    if (game_state.progress.estate.ledger[slot].earnings >= LEDGER_HIGH_MONEY) {
        earningsGrade = 0;
    } else if (game_state.progress.estate.ledger[slot].earnings >= LEDGER_MID_MONEY) {
        earningsGrade = 1;
    } else {
        earningsGrade = 2;
    }
    if (game_state.progress.estate.ledger[slot].earnings == 0 && game_state.progress.estate.ledger[slot].expenses == 0) {
        text = LOCALIZED(text_bank.messages.nothing);
    } else {
        text = TEXT_AT_OFFSET(text_bank_starts.messages + status_ledger_offsets[expenseGrade * 3 + earningsGrade]);
    }
    text_draw_plain(&current_frame->otTags[0][21], text, 172, y);
}

static inline s32 pick(s32 cond, s32 a, s32 b) {
    return cond ? a : b;
}

/* Draw a ledger entry's two bars at height y: a frame of four lines, a background
 * bar, then one bar each for expenses and earnings, filled by the value mod 10000
 * (64 pixels full). A figure that reaches 10000 gets an icon and the count of
 * ten-thousands (mod 100) beside it. */
static void status_draw_ledger_bars(FarmLedger *entry, s16 y) {
    u_long *line_ot;
    u_long *bar_ot;
    u_long *icon_ot;
    s32 i;
    s16 bar_y;
    s16 fill;

    line_ot = &current_frame->otTags[0][24];
    bar_ot = &current_frame->otTags[0][23];
    icon_ot = &current_frame->otTags[0][22];
    i = 0;
line:
    prim_add_line(line_ot, pick(i % 3, 168, 40), pick(i & 2, y + 8, y),
                  pick(i & 2, 40, 168), pick(i % 3, y + 8, y), 0xE0B888, 0);
    i++;
    if (i < 4) {
        goto line;
    }
    bar_y = y + 1;
    prim_add_rect(line_ot, 41, bar_y, 128, 7, 0xC8A060, 0);
    prim_add_line(bar_ot, 104, y, 104, y + 8, 0xE0B888, 0);

    fill = entry->expenses % LEDGER_BAR_UNIT * LEDGER_BAR_PIXELS / LEDGER_BAR_UNIT;
    prim_add_gouraud_rect(bar_ot, 105 - fill, bar_y, fill, 7,
                  0xE8A0B8, 0xD14070, 0xE8A0B8, 0xD14070, 0);
    if (entry->expenses / LEDGER_BAR_UNIT != 0) {
        anim_draw_sprt_cursor(icon_ot, &status_cursor_sprite, 0xA, 0, 40, y - 24);
        item_anim_draw(icon_ot, 64, y - 32,
                      entry->expenses / LEDGER_BAR_UNIT - entry->expenses / (LEDGER_BAR_UNIT * 100) * 100);
    }

    fill = entry->earnings % LEDGER_BAR_UNIT * LEDGER_BAR_PIXELS / LEDGER_BAR_UNIT;
    prim_add_gouraud_rect(bar_ot, 105, bar_y, fill, 7,
                  0x4D70A0, 0xA6B8D0, 0x4D70A0, 0xA6B8D0, 0);
    if (entry->earnings / LEDGER_BAR_UNIT != 0) {
        anim_draw_sprt_cursor(icon_ot, &status_cursor_sprite, 0xA, 0, 104, y - 24);
        item_anim_draw(icon_ot, 128, y - 32,
                      entry->earnings / LEDGER_BAR_UNIT - entry->earnings / (LEDGER_BAR_UNIT * 100) * 100);
    }
}

/* Draw the five tool-level rows: a mark, its threshold marks, a gauge and the level. */
static void status_page_tool_levels(void) {
    s32 slot;
    s32 markY;
    s32 gaugeY;
    s32 thresholdY;
    s32 levelY;
    u16 *value;
    s16 result;
    FarmState *farm;

    text_draw_plain(&current_frame->otTags[0][21], LOCALIZED(text_bank.messages.level), 186, 26);
    slot = 0;
    levelY = 0x3C0000;
    gaugeY = 0x380000;
    thresholdY = 0x440000;
    markY = 0x300000;
    farm = &game_state;
    value = farm->player.toolExp;
    do {
        status_draw_mark(slot, 40, markY >> 16);
        status_draw_threshold_marks(*value, 72, thresholdY >> 16);
        thresholdY += STATUS_TOOL_ROW_STEP;
        markY += STATUS_TOOL_ROW_STEP;
        result = status_tool_level(slot);
        status_draw_gauge(result, *value, 80, gaugeY >> 16);
        gaugeY += STATUS_TOOL_ROW_STEP;
        status_draw_level(result, *value, 215, levelY >> 16);
        levelY += STATUS_TOOL_ROW_STEP;
        value++;
        slot++;
    } while (slot < 5);
}

static s16 status_level_indices[6] = {0x0001, 0x0006, 0x000B, 0x0010, 0x0015, 0x0000};

/* The level of tool `index` (1 to 5, negative while it is being rebuilt), from its order ladder. */
static s16 status_tool_level(u16 index) {
    if (index >= TOOL_LEVEL_COUNT) {
        index = TOOL_LEVEL_COUNT - 1;
    }
    return status_find_level(status_level_indices[index]);
}

/* The highest of the five orders from `firstOrder` that is reached, as 1 to 5; negated while
 * the carpenter is rebuilding one of them. */
static s16 status_find_level(s16 firstOrder) {
    s16 start;
    s32 slot;
    s16 result;
    s32 rebuilding;
    s32 (*check)(s16);

    result = 1;
    start = firstOrder;
    if ((u32)(game_state.progress.estate.carpenterOrder - start) < TOOL_LEVEL_COUNT) {
        check = status_level_matches;
        rebuilding = 1;
    } else {
        check = status_event_item_can_take;
        rebuilding = 0;
    }
    slot = 0;
    do {
        if (check(start + slot) == 1) {
            result = slot + 1;
        }
        slot++;
    } while (slot < TOOL_LEVEL_COUNT);
    if (rebuilding != 0) {
        result = -result;
    }
    return result;
}

static s32 status_event_item_can_take(s16 arg) {
    return event_item_can_take(arg, 0, 2, 0);
}

static s32 status_level_matches(s16 arg) {
    s32 a = arg;
    s16 x = game_state.progress.estate.carpenterOrder;
    a <<= 16;
    a >>= 16;
    return (x ^ a) == 0;
}

static u8 status_mark_frames[12] = {
    0x05, 0x00, 0x04, 0x00, 0x06, 0x00, 0x07, 0x00, 0x08, 0x00, 0x00, 0x00,
};

/* Draw the mark of tool `index` (clamped to the last tool) at (x, y). */
static void status_draw_mark(u16 index, s16 x, s16 y) {
    u32 i = index;
    if (i >= TOOL_LEVEL_COUNT) i = TOOL_LEVEL_COUNT - 1;
    anim_draw_sprt_cursor(&current_frame->otTags[0][26], &status_mark_sprite, status_mark_frames[i * 2], 0, x, y);
}

/* Draw a mark for each band threshold from the second on that `value` has reached,
 * 8 pixels apart from x. */
static void status_draw_threshold_marks(u16 value, s16 x, s16 y)
{
    s32 i;

    for (i = 0; i < 4; i++) {
        if (value >= status_band_thresholds[i + 1]) {
            anim_draw_sprt_cursor(&current_frame->otTags[0][26], &status_mark_sprite, i, 0, x, y);
            x += 8;
        }
    }
}

static s32 status_gauge_colors_a[5] = {
    0x00B89000, 0x00B8B0B8, 0x00E0E000, 0x000088E0, 0x00FFFFFF,
};

static s32 status_gauge_colors_b[5] = {
    0x008A6C00, 0x008A848A, 0x00A8A800, 0x000066A8, 0x00BFBFBF,
};

/* Draw the gauge for `level` at (x, y): a frame of four lines, a background,
 * and the bar filled as far as `value` has come through the level's threshold band,
 * in the level's two colours (full for the maximum). */
static void status_draw_gauge(s16 level, u16 value, s16 x, s16 y) {
    s32 i;
    s32 right;
    s32 bottom;
    s16 sx0;
    s16 sy0;
    s16 sx1;
    s16 sy1;
    s32 x0;
    s32 y0;
    s32 x1;
    s32 y1;
    s32 colorA;
    s32 colorB;
    s16 width;
    u_long *ot;

    i = 0;
    ot = &current_frame->otTags[0][24];
    for (; i < 4; i++) {
        right = x + BAR_RIGHT;
        bottom = y + 8;
        x0 = x;
        if (i % 3) {
            x0 = right;
        }
        sx0 = x0;
        y0 = y;
        if (i >= 2) {
            y0 = bottom;
        }
        sy0 = y0;
        x1 = x;
        if (i < 2) {
            x1 = right;
        }
        sx1 = x1;
        y1 = y;
        if (i % 3) {
            y1 = bottom;
        }
        sy1 = y1;
        prim_add_line(ot, sx0, sy0, sx1, sy1, 0xE0B888, 0);
    }
    prim_add_rect(ot, x + 1, y + 1, BAR_WIDTH, 7, 0xC8A060, 0);

    if (level < 0) {
        level = -level;
    }
    colorA = status_gauge_colors_a[level - 1];
    colorB = status_gauge_colors_b[level - 1];
    if (level == TOOL_LEVEL_COUNT) {
        width = BAR_WIDTH;
    } else {
        width = (value - status_band_thresholds[level - 1]) * BAR_WIDTH / (status_band_thresholds[level] - status_band_thresholds[level - 1]);
        if (width > BAR_WIDTH) {
            width = BAR_WIDTH;
        }
    }
    prim_add_gouraud_rect(&current_frame->otTags[0][23], x + 1, y + 1, width, 7, colorB, colorA, colorB, colorA, 0);
}

/* Draw the level label at (x, y): a fixed word for a negative level or the maximum,
 * otherwise the percentage of the way through the current level's threshold band,
 * plus whole levels. */
static void status_draw_level(s16 level, u16 value, s16 x, s16 y) {
    s16 band;
    u16 percent;
    u16 bandStart;
    u16 *text;

    if (level < 0) {
        text = LOCALIZED(text_bank.messages.rebuilding);
    } else if (level == TOOL_LEVEL_COUNT) {
        text = LOCALIZED(text_bank.messages.maximum);
    } else {
        for (band = 1; band < 5; band++) {
            if (value < status_band_thresholds[band]) {
                break;
            }
        }
        bandStart = status_band_thresholds[band - 1];
        percent = (value - bandStart) * 100 / (status_band_thresholds[band] - bandStart) + (band - level) * 100;
        text_copy_string(text_format_number(percent, status_number_scratch, 4),
                      LOCALIZED(text_bank.messages.glyph_8f));
        text = status_number_scratch;
    }
    text_draw_plain(&current_frame->otTags[0][21], text, x, y);
}

static s16 status_actor_year(s16 *arg) {
    return *arg;
}

/* Draw a number with its leading blank glyphs left out. */
static void status_draw_number(s16 number, s16 x, s16 y) {
    s32 count;
    u16 *cursor;

    *text_format_number(number, status_number_scratch, STATUS_DIGIT_COUNT) = TEXT_END;
    cursor = status_number_scratch;
    count = 0;
    if (*cursor == TEXT_BLANK) {
        do {
            count++;
            cursor++;
            if (count >= STATUS_DIGIT_COUNT) break;
        } while (*cursor == TEXT_BLANK);
    }
    text_draw_plain(&current_frame->otTags[0][21], cursor, x, y);
}

/* Write the first glyph of the message at `offset` to out[0], then TEXT_END to out[1]. */
static void status_number_set_suffix(u16 *out, u32 offset) {
    s32 messageAddress = text_bank_starts.messages + offset;
    u16 value = *TEXT_AT_OFFSET(messageAddress);

    out[1] = TEXT_END;
    out[0] = value;
}

static void options_draw_glyph_run(s32 count, s32 glyph_arg, s32 y_arg, s32 height_arg);

/* Draw the left and right page arrows, pulsing with the cycle timer. */
void status_draw_side_arrows(void) {
    s32 phase;
    u_long *display_list;

    phase = (s16)status_cycle_frames / 30;
    display_list = &current_frame->otTags[0][26];
    anim_draw_sprt_cursor(display_list, &status_cursor_sprite, 0, phase & 0xFF, 32, 24);
    anim_draw_sprt_cursor(display_list, &status_cursor_sprite, 2, phase & 0xFF, 264, 24);
}

/* Draw the list's up arrow, pulsing with the cycle timer. */
void status_draw_arrow_up(void) {
    s32 x = (s16)status_cycle_frames;
    s32 q = x / 30;
    anim_draw_sprt_cursor(&current_frame->otTags[0][26], &status_cursor_sprite, 1, q & 0xFF, 288, 48);
}

/* Draw the list's down arrow, pulsing with the cycle timer. */
void status_draw_arrow_down(void) {
    s32 value = (s16)status_cycle_frames;
    s32 calc = value / 30;
    s32 arg = calc & 0xFF;
    u_long *base = current_frame->otTags[0];
    anim_draw_sprt_cursor(&base[26], &status_cursor_sprite, 3, arg, 288, 200);
}

void status_draw_glyph_run_5(s16 count, s16 y, s16 height) {
    options_draw_glyph_run(count, 5, y, height);
}

void status_draw_glyph_run_4(s16 a, s16 b, s16 c) {
    options_draw_glyph_run(a, 4, b, c);
}

/* Draw the glyph `count` times, stepping 8 pixels up each time. */
static void options_draw_glyph_run(s32 count, s32 glyph_arg, s32 y_arg, s32 height_arg)
{
    s16 glyph = glyph_arg;
    s16 y = y_arg;
    s16 height = height_arg;
    s32 i;

    for (i = 0; i < count; i++) {
        anim_draw_sprt_cursor(&current_frame->otTags[0][26], &status_cursor_sprite, glyph, 0, y, height);
        y -= 8;
    }
}

/* Step to the next status page, skipping the chicken, cattle and sheep pages when there are
 * no animals for them, and wrap to the first page after the last. */
void options_page_next(void) {
    s32 counter;

    counter = status_page + 1;
    status_page = counter;
    if ((s16)counter == 2) {
        if (chicken_count_unflagged() == 0) {
            status_page = status_page + 1;
        }
    }
    if ((s16)status_page == 3) {
        if (livestock_count_cattle() == 0) {
            status_page = status_page + 1;
        }
    }
    if ((s16)status_page == 4) {
        if (livestock_count_sheep() == 0) {
            status_page = status_page + 1;
        }
    }
    if ((s16)status_page >= 8) {
        status_page = 0;
    }
}

/* Step to the previous status page, skipping the chicken, cattle and sheep pages when there are
 * no animals for them, and wrap to the last page before the first. */
void options_page_prev(void) {
    s32 counter;

    counter = status_page - 1;
    status_page = counter;
    if ((s16)counter == 4) {
        if (livestock_count_sheep() == 0) {
            status_page = status_page - 1;
        }
    }
    if ((s16)status_page == 3) {
        if (livestock_count_cattle() == 0) {
            status_page = status_page - 1;
        }
    }
    if ((s16)status_page == 2) {
        if (chicken_count_unflagged() == 0) {
            status_page = status_page - 1;
        }
    }
    if ((s16)status_page < 0) {
        status_page = 7;
    }
}

s32 status_count_pond_fish(void) {
    s32 count = 0;
    s32 i;
    for (i = 0; i < POND_FISH; i++) {
        if (game_state.animals.pond.fish[i].item != ITEM_NOTHING) count++;
    }
    return count;
}

/* Count the live chickens that are not ACTOR_FLAG_CLAIMED. */
s32 chicken_count_unflagged(void) {
    s32 count = 0;
    s32 i;
    for (i = 0; i < MAX_CHICKENS; i++) {
        if (game_state.animals.chickens[i].actor.kind != 0 && (game_state.animals.chickens[i].actor.flags & ACTOR_FLAG_CLAIMED) == 0) count++;
    }
    return count;
}

s32 livestock_count_cattle(void) {
    s32 count = 0;
    s32 i;
    for (i = 0; i < MAX_LIVESTOCK; i++) {
        if ((u32)(game_state.animals.livestock[i].actor.kind - ITEM_CALF) < 4U) count++;
    }
    return count;
}

s32 livestock_count_sheep(void) {
    s32 count = 0;
    s32 i;
    for (i = 0; i < MAX_LIVESTOCK; i++) {
        if ((u32)(game_state.animals.livestock[i].actor.kind - ITEM_LAMB) < 3U) count++;
    }
    return count;
}

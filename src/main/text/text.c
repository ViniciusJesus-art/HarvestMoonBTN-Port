/* Text: the message window, glyph sprites, text measuring and the name and number fill-ins. */

#include <stdio.h>
#include <ctype.h>
#include "stdarg.h"
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libapi.h>
#include <memory.h>
#include "types.h"
#include "actor.h"
#include "afile.h"
#include "chicken.h"
#include "family.h"
#include "farm_state.h"
#include "farm_tables.h"
#include "items.h"
#include "livestock.h"
#include "loading_screen.h"
#include "main.h"
#include "math_util.h"
#include "menu_window.h"
#include "pad.h"
#include "prim.h"
#include "screen_fade.h"
#include "sound_request.h"
#include "text.h"
#include "text_bank.h"
#include "texture_pack.h"
#include "ui_icons.h"
#include "villager.h"

/* Textured sprite primitive. */
typedef struct Sprt
{
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    s16 w, h;
} Sprt;

typedef struct DrTPage
{
    u32 tag;
    u32 code[1];
} DrTPage;

/* One line of text: up to 40 characters and the terminator. */
typedef struct
{
    u16 chars[41];
} TextLine;

/* A sprite and a draw-mode primitive; used by text_draw_face. */
typedef struct
{
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    s16 w, h;
} PrimSprite;

typedef struct
{
    u32 tag;
    u32 code[1];
} PrimDrTPage;

/* Text generators: each writes its text to text_list_buffer and returns its length
 * in halfwords (text_list_livestock, text_list_flagged_livestock, text_list_chickens). */
typedef s32 (*TextFill)(s32 arg);

static TextWindowResult text_window_frame(u_long *ot, u16 *text, s16 x, s16 y, s16 unused, s16 height);
static void text_window_clear(void);
static u8 *text_draw_string(u_long *ot, u8 *prim, u16 *text, s16 x, s16 y, s32 scale, s32 mode, s32 semiTrans,
                            s32 nested, s32 toOrigin);
static s16 text_end_y(void);
static u8 *text_glyph_sprite(u_long *ot, struct Sprt *sprt, u16 glyph, s16 x, s16 y, u8 r, u8 g, u8 b, u32 palette,
                                         s32 semiTrans);
static u16 *text_find_nth(u16 *entry, u16 code, s32 nth, s32 limit);
static u16 text_count_matches(u16 *entries, u16 match, s16 limit, u16 *out_count);
static u16 *text_advance_line(u16 *text, s32 line);
static u16 text_count_entries(u16 *entries, u16 stop, s16 limit);
static void text_draw_typed(u_long *ot, u16 *text, s16 x, s16 y);
static u16 text_get_state(void);
static void text_reset_state(void);
static void *text_draw_lines_centered(u_long *target, void *handle, u16 *text, s16 x, s16 y, s16 width, s32 scale,
                                      s16 lineStep, s16 spacing);
static void *text_draw_line_centered(u_long *ot, void *handle, u16 *text, s16 x, s16 y, s16 width,
                                     s32 scale, s16 spacing);
static s32 text_measure(u16 *entries, s32 size, s16 spacing);
static void *text_draw_lines_right(u_long *target, void *handle, u16 *text, s16 x, s16 y, s16 width, s32 scale,
                                   s16 lineHeight, s16 spacing);
static s32 text_choice_menu(s16 x, s16 y, s16 *cursor, s16 count);
static s32 text_choice_face_bank(u16 choice);
static void text_draw_face(u_long *ot, s16 x, s16 y, s16 id);
static s16 text_code_to_bank(s32 code);
static void text_switch_bank(s16 bank);
static void text_load_bank(s32 bank, s32 wait);
static void text_next_page(void);
static void text_start_choice(void);
static void *text_insert_source(u16 code);
static void text_expand_request(s32 selector);
static s32 text_list_livestock(int mode);
static s32 text_list_flagged_livestock(s32 mode);
static s32 text_list_chickens(int mode);
static void *text_append_name(void *end, void *item, int mode);
static u16 *text_write_number(u32 handle, u16 *text, s32 width);
static s32 text_char_code(s32 input);
static s32 text_find_staged_value(void);

/* Small variables of this file; they are zeroed at start-up. */
s32 text_bank_loading;
u16 text_index;
u16 text_phase;
s32 text_glyph_timer;
/* Vsync ticks between glyphs, from text_speed_intervals. */
s16 text_glyph_interval;
u16 text_state;
s32 text_palette;
s16 text_face_bank;
s32 text_face_reload;
/* 1 while a text bank read is in flight. */
s32 text_bank_reading;
/* The text bank in memory, -1 when none. */
s16 text_bank_loaded;
s32 text_face_shown;
/* The text bank asked for: set by text_set_bank and text_switch_bank. */
s16 text_bank_id;
/* The staged text bank block's handle: the afile read result. */
u8 *text_bank_result;
s16 text_wait_frames;
s32 text_no_scroll;
s16 text_pen_x;
s16 text_pen_y;
s16 text_margin_x;
s16 text_line_glyphs;
s16 text_wrap_index;
u16 text_last_code;
u16 text_last_glyph;
s32 text_scroll_hold;
s32 text_blip_toggle;
u16 text_number_buffer[4];
s16 text_origin_x;
s16 text_origin_y;
s16 text_offset_x;
s16 text_offset_y;
s16 text_scroll_state;
s16 text_scroll_y;
s16 text_choice;
u16 *text_current;
s32 text_scroll_timer;
u16 *text_source;
s32 text_scrolling;
s32 text_scroll_started;

/* Text state: the bank being read, the typing and scroll positions, the pen and the cursor. */
static u16 text_glyphs[64];
/* Digit buffer: text_format_number writes one glyph per digit into it. */
static u16 text_digits[16];
static u16 *text_sources[12];
/* Name list the expanded text requests are built in. */
static u16 text_list_buffer[224];

/* The height of a line of text, in pixels. */
#define TEXT_LINE_HEIGHT 12

/* One text bank is 0x1800 bytes; bank n starts n banks into the file. */
#define TEXT_BANK_STRIDE 0x1800

/* The text bank id of no bank: it matches no real bank. */
#define TEXT_BANK_NONE 0x3E8

/* A text-insert halfword has TEXT_INSERT_FLAG set; its low 12 bits are the insert id. */
#define TEXT_INSERT_FLAG 0x4000
#define TEXT_INSERT_ID_MASK 0xFFF

/* The glyphs of the marks and digits text_char_code and text_format_number write. Letters
 * are not listed: glyph n is letter 'A' + n. */
enum TextGlyph
{
    TEXT_GLYPH_DIGIT_0 = 0x40,
    TEXT_GLYPH_QUESTION = 0x62,
    TEXT_GLYPH_EXCLAIM = 0x63,
    TEXT_GLYPH_MINUS = 0x69,
    TEXT_GLYPH_LPAREN = 0x6A,
    TEXT_GLYPH_RPAREN = 0x6B,
};

s16 text_spacing_x = 0;
u16 text_spacing_y = 0;
s32 text_input = 1;

/* VRAM row of each of the six text CLUTs (all at x = 896). */
static u16 text_clut_rows[6] = {0x00FE, 0x00FA, 0x00FA, 0x00FC, 0x00FD, 0x00FB};

void text_reset(void)
{
    text_window_clear();
}

void text_set_source(int index, u16 *source)
{
    unsigned int slot;
    slot = index & 0xFFFF;
    if (slot >= 11)
    {
        index = 10;
    }
    slot = index & 0xFFFF;
    text_sources[slot] = source;
}

void text_set_spacing(s16 x, s16 y)
{
    text_spacing_x = x;
    text_spacing_y = y;
}

void text_set_palette(unsigned int palette)
{
    if (palette >= 7)
    {
        palette = 6;
    }
    text_palette = palette;
}

s32 text_get_palette(void)
{
    return text_palette;
}

static u16 text_speed_intervals[] = {0, 3, 5, 0};

/* Set the glyph interval from text_speed_intervals; an index past the table uses entry 1. */
void text_set_speed(s32 index)
{
    if (index >= 3U)
    {
        index = 1;
    }
    text_glyph_interval = text_speed_intervals[index];
}

/* The text_speed_intervals index of text_glyph_interval: 0 instant, 1 three ticks, 2 five. */
static s32 text_get_speed(void)
{
    if (text_glyph_interval == 0)
    {
        return 0;
    }
    if (text_glyph_interval == 5)
    {
        return 2;
    }
    return 1;
}

/* A printing window starts scrolling; any other state is left alone. */
static void text_begin_typing(void)
{
    if (text_phase == TEXT_STATE_PRINTING)
    {
        text_phase = TEXT_STATE_SCROLLING;
    }
}

/* Leave a scroll or page break for printing, one glyph further on. */
static void text_advance_glyph(void)
{
    if (text_phase == TEXT_STATE_SCROLLING || text_phase == TEXT_STATE_PAGE_BREAK)
    {
        text_phase = TEXT_STATE_PRINTING;
        text_index++;
    }
}

/* 1 reads the pad in text_window_frame; 0 ignores it. */
void text_set_input(s32 value)
{
    text_input = value;
}

static s32 text_get_input(void)
{
    return text_input;
}

s16 text_get_wait_frames(void)
{
    return text_wait_frames;
}

void text_draw_plain(u_long *ot, u16 *text, s16 x, s16 y)
{
    u8 *packet;

    packet = gs_work_alloc(0);
    packet = text_draw_string(ot, packet, text, x, y, FIXED_ONE, 0, 0, 0, 0);
    gs_work_release(packet);
}

TextWindowResult text_window_draw(u_long *ot, u16 *text, s16 x, s16 y, s16 unused, s16 height)
{
    return text_window_frame(ot, text, x, y, unused, height);
}

static void text_end_y_discard(void)
{
    text_end_y();
}

static u16 text_get_last_glyph(void)
{
    return text_last_glyph;
}

u16 text_get_bank(void)
{
    return (u16)text_bank_id;
}

/* Make `value` the bank the window asks for and the one in memory. */
void text_set_bank(s32 value)
{
    text_bank_id = value;
    text_bank_loaded = value;
}

static void text_draw_centered(u_long *ot, u16 *text, s16 x, s16 y, s16 width)
{
    u8 *packet;
    s16 limit;

    packet = gs_work_alloc(0);
    limit = text_spacing_y + 16;
    packet =
        text_draw_lines_centered(ot, packet, text, x, y, width, FIXED_ONE, limit, text_spacing_x);
    gs_work_release(packet);
}

static void text_draw_right(u_long *ot, u16 *text, s16 x, s16 y, s16 width)
{
    u8 *packet;
    s16 limit;

    packet = gs_work_alloc(0);
    limit = text_spacing_y + 16;
    packet = text_draw_lines_right(ot, packet, text, x, y, width, FIXED_ONE, limit, text_spacing_x);
    gs_work_release(packet);
}

void text_draw_semi(u_long *ot, u16 *text, s16 x, s16 y,
                    unsigned int semiTrans)
{
    u8 *packet;

    packet = gs_work_alloc(0);
    packet = text_draw_string(ot, packet, text, x, y, FIXED_ONE, 0, semiTrans, 0, 0);
    gs_work_release(packet);
}

/* Append the TEXT_END-terminated string `src` to the list `dst` over its terminator;
 * returns the new terminator. */
u16 *text_list_append(u16 *dst, u16 *src)
{
    u16 *p;
    u16 *q;
    u16 sentinel;
    u16 value;
    u16 check;
    p = dst;
    while (*p != TEXT_END)
        p++;
    *p = *src;
    if (*src != TEXT_END)
    {
        sentinel = TEXT_END;
        q = src + 1;
        do
        {
            value = *q;
            p++;
            *p = value;
            check = *q;
            if (check == sentinel)
                break;
            q++;
        } while (1);
    }
    return p;
}

/* Copy the TEXT_END-terminated string `src` to `dst`, terminator included. */
static void text_list_copy(u16 *dst, u16 *src)
{
    *dst = *src;
    while (*src != TEXT_END)
    {
        src++;
        dst++;
        *dst = *src;
    }
}

/* Compare two TEXT_END-terminated lists halfword by halfword: 0 when equal, otherwise
 * -1 or 1 at the first difference. */
static s32 text_list_compare(u16 *a, u16 *b)
{
    u16 x, y;
    u16 end = TEXT_END;
    goto first;
cont:
    b++;
first:
    x = *a;
    y = *b;
    if (x < y)
        return -1;
    if (y < x)
        goto greater;
    if (y != end)
    {
        a++;
        goto cont;
    }
    return 0;
greater:
    return 1;
}

static inline s32 char_at(u16 *text)
{
    return *text;
}

/* Compare the TEXT_END-terminated string `text` with two alternatives a
 * character at a time: a character matches if it equals the one at the same
 * place in `first` or in `second`. 0 when the whole string matches, otherwise
 * -1 / 1 by comparing the first mismatch with `first`. */
s32 text_match_either(u16 *text, u16 *first, u16 *second)
{
    u16 ch;
    u16 want;
    s32 result;

    while (1)
    {
        ch = char_at(text);
        want = *first;
        if (ch == want || ch == *second)
        {
            result = 0;
        }
        else
        {
            break;
        }
        if (char_at(text) == TEXT_END)
        {
            goto done;
        }
        text++;
        first++;
        second++;
    }
    result = (ch < want) ? -1 : 1;
done:
    return result;
}

static char text_fmt_dec[4] = "%d";
static char text_fmt_hex[4] = "%x";
static char text_fmt_int[4] = "%i";

/* Debug print: formats `fmt` (%d, %x, %i, %s) into text_glyphs and draws it at
 * the text origin plus the cursor offset, with a backing box per line. Does
 * nothing during a transition, outside the normal farm view or event mode, or
 * while the loading screen is up. */
static void text_printf(u8 *fmt, ...)
{
    va_list args;
    u8 number[16];
    s16 line_length[5];
    u8 *p;
    u8 *str;
    s16 lines;
    s32 i;
    s16 saved_a;
    s16 saved_b;
    u_long *ot;

    if (game_state.globalFlags & GLOBAL_FLAG_TRANSITION)
    {
        return;
    }
    if (farm_area.mode.view != FARM_VIEW_NORMAL)
    {
        return;
    }
    if (farm_area.mode.eventMode != FARM_MODE_IDLE)
    {
        return;
    }
    if (loading_screen_is_active() == 1)
    {
        return;
    }

    for (i = 0; i < 5; i++)
    {
        line_length[i] = 0;
    }
    lines = 0;
    va_start(args, fmt);
    p = fmt;
    i = 0;
    while (1)
    {
        if (*p == '\0')
        {
            text_glyphs[i] = TEXT_END;
            break;
        }
        if (*p != '%')
        {
            text_glyphs[i] = text_char_code(*p);
            i++;
            if (*p == '\n')
            {
                lines++;
            }
            else
            {
                line_length[lines]++;
            }
        }
        else
        {
            p++;
            str = number;
            switch (*p)
            {
            case 'd':
                sprintf((char *)str, text_fmt_dec, va_arg(args, int));
                goto copy;
            case 'x':
                sprintf((char *)str, text_fmt_hex, va_arg(args, int));
                goto copy;
            case 'i':
                sprintf((char *)str, text_fmt_int, va_arg(args, int));
                goto copy;
            case 's':
                str = va_arg(args, u8 *);
            copy:
                while (*str != '\0')
                {
                    text_glyphs[i] = text_char_code(*str);
                    i++;
                    line_length[lines]++;
                    str++;
                }
                break;
            }
        }
        p++;
        if (i >= 64)
        {
            break;
        }
    }
    saved_a = text_spacing_x;
    saved_b = text_spacing_y;
    text_glyphs[63] = TEXT_END;
    text_spacing_x = 0;
    text_spacing_y = 0;
    i = text_get_palette();
    text_set_palette(0);
    if (text_offset_x < 321 && text_offset_y < 121)
    {
        ot = current_frame->ot;
        current_frame->primCursor.bytes =
            text_draw_string(ot, current_frame->primCursor.bytes, text_glyphs, text_origin_x + text_offset_x,
                             text_origin_y + text_offset_y, FIXED_ONE, 0, 0, 0, 1);
        text_set_palette(i);
        for (i = 0; i <= lines; i++)
        {
            if (line_length[i] != 0)
            {
                prim_add_tile(ot, (i == 0) ? text_origin_x + text_offset_x : text_origin_x,
                              text_origin_y + text_offset_y + i * TEXT_LINE_HEIGHT, line_length[i] * 8, TEXT_LINE_HEIGHT, 0xFFFFFF,
                              0);
            }
        }
        text_spacing_x = saved_a;
        text_spacing_y = saved_b;
        text_offset_x = text_pen_x - text_origin_x;
        text_offset_y = text_pen_y - text_origin_y;
    }
}

void text_set_origin(s16 x, s16 y)
{
    text_origin_x = x;
    text_origin_y = y;
    text_offset_x = 0;
    text_offset_y = 0;
}

static void text_reset_cursor(void)
{
    text_offset_x = 0;
    text_offset_y = 0;
}

/* One frame of the message window: (re)start `text` when it changes, run the
 * TextWindowState (text_get_state), scroll a full window up line by line (or jump a page),
 * then draw the text clipped to the window and the face. Returns the frame's
 * TextWindowStatus; a picked choice comes back as its face bank (10..13) and a
 * closed window as TEXT_WINDOW_CLOSED. */
static TextWindowResult text_window_frame(u_long *ot, u16 *text, s16 x, s16 y, s16 unused, s16 height)
{
    u16 *p;
    s32 step;
    s32 result;
    TextWindowResult ret;
    s32 held;
    s32 pressed;
    enum TextWindowState state;
    u16 *mark;
    u16 *line;
    s32 i;
    s32 total;
    s16 lineY;
    s32 savedMode;
    DR_AREA *restore;
    DR_AREA *clip;
    RECT area;

    if (text_source != text)
    {
        text_source = text;
        text_current = text;
        text_index = 0;
        p = text;
        if (*p != TEXT_END)
        {
            do
            {
                if (*p & TEXT_INSERT_FLAG)
                {
                    text_insert_source(*p);
                }
                text_index++;
                p++;
            } while (*p != TEXT_END);
        }
        text_scroll_timer = 0;
        text_scroll_y = 0;
        text_scroll_state = 3;
        text_scrolling = 1;
        text_reset_state();
        text_scroll_started = 0;
        text_face_reload = 0;
        text_bank_result = 0;
        text_face_bank = -1;
        text_face_shown = 0;
        text_bank_reading = 0;
        text_wait_frames = 0;
        text_no_scroll = 0;
        text_scroll_hold = 0;
        text_blip_toggle = 0;
    }
    step = 1;
    savedMode = text_get_speed();
    result = TEXT_WINDOW_PRINTING;
    if (text_input != 0)
    {
        held = input_pad1->held;
        pressed = input_pad1->pressed;
    }
    else
    {
        held = 0;
        pressed = 0;
    }
    state = text_get_state();
    switch (state)
    {
    case TEXT_STATE_PRINTING:
        result = TEXT_WINDOW_PRINTING;
        if (held & (PAD_CIRCLE | PAD_CROSS))
        {
            text_set_speed(0);
        }
        break;
    case TEXT_STATE_SCROLLING:
        if (text_scrolling == 0)
        {
            result = TEXT_WINDOW_SCROLL_WAIT;
            if ((pressed & (PAD_CIRCLE | PAD_CROSS)) && text_scroll_state >= 4)
            {
                text_scrolling = 1;
                text_scroll_state = 0;
                result = TEXT_WINDOW_PRINTING;
            }
        }
        else
        {
            result = TEXT_WINDOW_SCROLLING;
            if (held & (PAD_CIRCLE | PAD_CROSS))
            {
                step = 2;
            }
        }
        break;
    case TEXT_STATE_END:
        farm_work_sector_release();
        if ((pressed & (PAD_CIRCLE | PAD_CROSS)) && text_state == TEXT_STATE_END)
        {
            text_source = 0;
            return TEXT_WINDOW_CLOSED;
        }
        result = TEXT_WINDOW_END_WAIT;
        break;
    case TEXT_STATE_PAGE_BREAK:
        result = TEXT_WINDOW_PAGE_WAIT;
        if (pressed & (PAD_CIRCLE | PAD_CROSS))
        {
            result = TEXT_WINDOW_PRINTING;
            text_advance_glyph();
            if (text_current[text_index - 1] == TEXT_LINE_BREAK)
            {
                if (text_scroll_started == 0)
                {
                    text_scroll_state -= text_count_matches(text_current, TEXT_LINE_BREAK,
                                                            text_index != 0 ? text_index : 1, 0);
                }
                else
                {
                    text_scroll_state = -1;
                }
            }
        }
        break;
    case TEXT_STATE_CHOICE_2:
    case TEXT_STATE_CHOICE_3:
    case TEXT_STATE_CHOICE_4:
        result = text_state;
        if (text_choice_menu(x, y, &text_choice, text_state - TEXT_STATE_CHOICE_2 + 2))
        {
            text_source = 0;
            return text_choice_face_bank(text_choice + 1);
        }
        break;
    }
    text_state = state;
    text_next_page();
    text_start_choice();
    lineY = y;
    if (text_no_scroll == 0)
    {
        if (text_scrolling == 1 &&
            text_count_matches(text_current, TEXT_LINE_BREAK, text_index != 0 ? text_index : 1, 0) >= 4)
        {
            text_scroll_started = 1;
            if (text_scroll_timer == 0)
            {
                text_begin_typing();
                text_scroll_state++;
            }
            if (vsync_elapsed(1, &text_scroll_timer, 1) == 1)
            {
                text_scroll_y += step;
            }
            if (text_scroll_y >= 14)
            {
                text_advance_glyph();
                text_current = text_advance_line(text_current, 1);
                text_scroll_timer = 0;
                text_scroll_y = 0;
            }
            else
            {
                lineY = y - text_scroll_y;
            }
        }
        else
        {
            text_scroll_y = 0;
        }
    }
    else if (text_scrolling == 1)
    {
        text_scroll_started = 1;
        text_scroll_state = 4;
        mark = text_find_nth(text_current, TEXT_CODE_PAGE_MARK, 1, 0);
        if (text_scroll_timer != 0)
        {
            line = text_find_nth(text_current, TEXT_LINE_BREAK, 4, 0);
            if (line != 0)
            {
                if (line[1] != TEXT_END)
                {
                    if (mark != 0 && mark < line)
                    {
                        line = text_find_nth(mark, TEXT_LINE_BREAK, 1, 0);
                    }
                    text_current = line + 1;
                }
            }
            else if (mark != 0)
            {
                text_current = mark + 1;
                if (*text_current == TEXT_LINE_BREAK)
                {
                    text_current++;
                }
            }
        }
        text_scroll_timer = 1;
        mark = text_find_nth(text_current, TEXT_CODE_PAGE_MARK, 1, 0);
        line = text_find_nth(text_current, TEXT_LINE_BREAK, 4, 0);
        if (line != 0)
        {
            if (mark != 0 && mark < line)
            {
                text_index = text_count_entries(text_current, TEXT_CODE_PAGE_MARK, 0);
            }
            else
            {
                for (i = 0, total = 0, p = text_current; i < 4; i++)
                {
                    total += text_count_entries(p, TEXT_LINE_BREAK, 0);
                    if (i < 3)
                    {
                        while (*p != TEXT_LINE_BREAK)
                        {
                            p++;
                        }
                    }
                    p++;
                }
                text_index = total;
            }
        }
        else
        {
            line = text_find_nth(text_current, TEXT_END, 1, 0);
            text_index = text_count_entries(text_current, (mark != 0 && mark < line) ? TEXT_CODE_PAGE_MARK : TEXT_END, 0);
        }
    }
    if (text_scroll_hold == 0 && text_scroll_state >= 4)
    {
        text_scrolling = 0;
        text_begin_typing();
    }
    ret = result;
    restore = gs_work_alloc(12);
    clip = gs_work_alloc(12);
    area.x = x;
    area.y = y + current_frame->drawEnv.clip.y;
    area.w = 320 - x;
    area.h = height;
    SetDrawArea(clip, &area);
    SetDrawArea(restore, &current_frame->drawEnv.clip);
    addPrim(ot, restore);
    text_draw_typed(ot, text_current, x, lineY);
    addPrim(ot, clip);
    text_set_speed(savedMode);
    text_load_bank(text_face_bank, text_face_reload);
    text_draw_face(current_frame->otTags[0], x - 4, y - 84, text_face_shown ? text_face_bank : -1);
    text_last_glyph = text_last_code;
    return ret;
}

static void text_window_clear(void)
{
    text_source = 0;
}

/* Draw a u16 glyph string at (x, y) into `ot`, scaled about the origin by
 * `scale` (20.12), and return the next free primitive address. Codes:
 * TEXT_END ends the text, TEXT_LINE_BREAK breaks the line (to the margin, or to
 * text_origin_x when `toOrigin`), 0xE25..0xE2F / 0xE38..0xFEF / 0xFF1..0xFFC set
 * text state, 0xFFD is a space, and a code with bit 14 set expands to a
 * nested string (drawn recursively). With `mode` 1 the text types out: one
 * more glyph per tick of the text_glyph_interval timer, counted in text_index. */
static u8 *text_draw_string(u_long *ot, u8 *prim, u16 *text, s16 x, s16 y, s32 scale, s32 mode, s32 semiTrans,
                            s32 nested, s32 toOrigin)
{
    s32 fast;
    u16 *child;
    s16 glyphX;
    s16 glyphY;

    if (nested == 0)
    {
        text_margin_x = x;
        if (mode == 1)
        {
            text_line_glyphs = 0;
            if (text_phase == TEXT_STATE_PRINTING)
            {
                if (vsync_elapsed(text_glyph_interval, &text_glyph_timer, 1) == mode)
                {
                    if (text_blip_toggle != 0)
                    {
                        sound_play(SOUND_TEXT_BLIP);
                        text_blip_toggle = 0;
                    }
                    else
                    {
                        text_blip_toggle = mode;
                    }
                    text_index++;
                }
                else if (text[text_index] > 0xE00 || (text[0] & TEXT_INSERT_FLAG))
                {
                    text_index++;
                }
            }
        }
    }
    text_pen_x = x;
    text_pen_y = y;
    fast = text_phase >= TEXT_STATE_CHOICE_2 && text_phase <= TEXT_STATE_CHOICE_4;
    while (1)
    {
        if (mode == 1)
        {
            if (text_index <= text_line_glyphs)
            {
                break;
            }
            text_line_glyphs++;
            if (text_phase != TEXT_STATE_SCROLLING && text_wrap_index != text_line_glyphs && !fast)
            {
                text_phase = TEXT_STATE_PRINTING;
            }
        }
        if (*text == TEXT_END)
        {
            if (mode != 0 && nested == 0 && !fast)
            {
                text_phase = TEXT_STATE_END;
            }
            break;
        }
        if (*text == TEXT_LINE_BREAK)
        {
            text_pen_y += TEXT_LINE_HEIGHT + (s16)text_spacing_y;
            text_pen_x = text_margin_x;
            if (toOrigin != 0)
            {
                text_pen_x = text_origin_x;
            }
        }
        else if (*text == TEXT_CODE_PAGE_MARK)
        {
            if (text_phase != TEXT_STATE_SCROLLING)
            {
                text_phase = TEXT_STATE_PAGE_BREAK;
                text_wrap_index = text_line_glyphs;
            }
        }
        else if (*text == TEXT_CODE_PAGE_END)
        {
        }
        else if (*text >= TEXT_CODE_CHOICE_4 && *text <= TEXT_CODE_CHOICE_2)
        {
        }
        else if (*text >= TEXT_CODE_FACE_FIRST && *text <= TEXT_CODE_FACE_LAST)
        {
            text_face_bank = text_code_to_bank(*text);
        }
        else if (*text == TEXT_CODE_FACE_BANK)
        {
            text_switch_bank(text_face_bank);
        }
        else if (*text == TEXT_CODE_FACE_HIDE)
        {
            text_face_shown = 0;
        }
        else if (*text == TEXT_CODE_FACE_RELOAD)
        {
            text_face_reload = 1;
        }
        else if (*text == TEXT_CODE_FACE_SHOW)
        {
            text_face_shown = 1;
        }
        else if (*text >= TEXT_CODE_WAIT_FIRST && *text <= TEXT_CODE_WAIT_LAST)
        {
            text_wait_frames = TEXT_CODE_WAIT_LAST - *text;
        }
        else if (*text == TEXT_CODE_NO_SCROLL)
        {
            text_no_scroll = 1;
        }
        else if (*text == TEXT_CODE_FLAG_FF2)
        {
        }
        else if (*text == TEXT_CODE_SCROLL_HOLD)
        {
            text_scroll_hold = 1;
        }
        else if (*text & TEXT_INSERT_FLAG)
        {
            child = text_insert_source(*text);
            if (child == 0)
            {
                child = text_sources[*text & TEXT_INSERT_ID_MASK];
            }
            if (child != 0)
            {
                prim = text_draw_string(ot, prim, child, text_pen_x, text_pen_y, scale, mode,
                                        semiTrans, 1, 0);
            }
        }
        else
        {
            if ((*text & 0xFFF) != TEXT_CODE_SPACE)
            {
                glyphX = x + (text_pen_x - x) * scale / FIXED_ONE;
                glyphY = y + (text_pen_y - y) * scale / FIXED_ONE;
                prim = text_glyph_sprite(ot, (struct Sprt *)prim, *text, glyphX, glyphY, 0x80, 0x80, 0x80, text_palette,
                                         semiTrans);
            }
            text_pen_x += 8 + text_spacing_x;
        }
        text++;
    }
    text_last_code = text[-1];
    return prim;
}

/* The y of the last line written: when the text so far ends in a line break the pen is
 * already on the next line, so this steps back one line. */
static s16 text_end_y(void)
{
    s16 y = text_pen_y;
    u16 index = text_index;
    u16 *p = text_current;
    if (p[index - 1] == TEXT_LINE_BREAK)
    {
        s32 next = y - TEXT_LINE_HEIGHT;
        next -= text_spacing_y;
        y = next;
    }
    return y;
}

/* One 8x12 font glyph at (x, y) into `ot`: a sprite cut from the 16-column
 * glyph sheet (`glyph` low nibble = column, next nibble = row), tinted (r, g, b)
 * with text CLUT `palette` (0 if out of range) and optionally semi-transparent,
 * followed by the draw-mode packet that selects the sheet's texture page.
 * Returns the next free packet address. */
static u8 *text_glyph_sprite(u_long *ot, struct Sprt *sprt, u16 glyph, s16 x, s16 y, u8 r, u8 g, u8 b, u32 palette,
                                         s32 semiTrans)
{
    DrTPage *tpage;
    s32 column = glyph & 0xF;
    s32 row = (glyph & 0xF0) >> 4;

    setlen(sprt, 4);
    sprt->code = 0x64;
    if (palette >= 6)
    {
        palette = 0;
    }
    sprt->clut = getClut(896, text_clut_rows[palette]);
    sprt->r0 = r;
    sprt->g0 = g;
    sprt->b0 = b;
    sprt->code = semiTrans ? sprt->code | 2 : sprt->code & ~2;
    sprt->x0 = x;
    sprt->y0 = y;
    sprt->u0 = column * 8;
    sprt->v0 = row * 12;
    sprt->w = 8;
    sprt->h = 12;
    addPrim(ot, sprt);

    sprt++;
    tpage = (DrTPage *)sprt;
    /* Dither on, draw to the display area on. */
    setlen(tpage, 1);
    tpage->code[0] = 0xE100060E;
    addPrim(ot, tpage);
    return (u8 *)(tpage + 1);
}

/* Find the `nth` occurrence of `code` in a TEXT_END-terminated list, walking
 * from `entry`. The terminator itself can be searched for. A nonzero `limit`
 * stops the search once the list up to the current entry counts `limit`
 * entries (see text_count_entries). Returns NULL when there is no such entry. */
static u16 *text_find_nth(u16 *entry, u16 code, s32 nth, s32 limit)
{
    s32 found;
    u16 *list;
    u16 *result;
    s32 length;

    found = 0;
    result = 0;
    list = entry;
    if (nth == 0)
    {
        return 0;
    }
    while (1)
    {
        length = text_count_entries(list, *entry, 0);
        if (limit != 0 && length >= limit)
        {
            break;
        }
        if (*entry == TEXT_END)
        {
            if (code == *entry)
            {
                result = entry;
            }
            break;
        }
        if (*entry == code)
        {
            found++;
            if (found >= nth)
            {
                result = entry;
                break;
            }
        }
        entry++;
    }
    return result;
}

/* Count the `match` halfwords in a TEXT_END-terminated list, expanding inserts and
 * stopping after `limit` entries when nonzero. Returns the match count; *out_count, when
 * given, gets the number of entries walked. */
static u16 text_count_matches(u16 *entries, u16 match, s16 limit, u16 *out_count)
{
    s32 result;
    s32 count;
    u16 *child;
    u16 **table;
    u16 recursive_count;

    result = 0;
    count = 0;
    match &= 0xFFFF;
    table = text_sources;
    while (limit == 0 || count < limit)
    {
        if (*entries == TEXT_END)
        {
            count++;
            break;
        }
        if (*entries == match)
        {
            result = (result + 1) & 0xFFFF;
        }
        else if (*entries != TEXT_LINE_BREAK && (*entries & TEXT_INSERT_FLAG))
        {
            child = text_insert_source(*entries);
            if (child == 0)
                child = table[*entries & TEXT_INSERT_ID_MASK];
            if (child != 0)
            {
                if (limit != 0)
                {
                    result = (result + text_count_matches(child, match, limit - count,
                                                          &recursive_count)) &
                             0xFFFF;
                    count += recursive_count;
                }
                else
                {
                    result = (result + text_count_matches(child, match, 0, 0)) & 0xFFFF;
                }
            }
        }
        count++;
        entries++;
    }
    if (out_count != 0)
        *out_count = count;
    return result;
}

/* Step `text` past its `line`th line break and take the entries passed off text_index;
 * NULL when there is no such break. */
static u16 *text_advance_line(u16 *text, s32 line)
{
    u16 *result;
    unsigned int count;

    result = text_find_nth(text, TEXT_LINE_BREAK, line, 0);
    count = (text_count_entries(text, TEXT_LINE_BREAK, 0) + 1) & 0xFFFF;
    if (result != 0)
    {
        result += 1;
        text_index -= count;
    }
    return result;
}

/* Count the entries of a list up to and including its terminator (TEXT_END or
 * `stop`), expanding every 0x4000 reference to its sub-list recursively. A
 * nonzero `limit` caps the count. */
static u16 text_count_entries(u16 *entries, u16 stop, s16 limit)
{
    s32 count;
    u16 *child;
    u16 *listed;
    u16 **table;

    count = 0;
    stop &= 0xFFFF;
    table = text_sources;
    for (; limit == 0 || count < limit; count++, entries++)
    {
        if (*entries == TEXT_END || *entries == stop)
        {
            count++;
            break;
        }
        if (*entries != TEXT_LINE_BREAK && (*entries & TEXT_INSERT_FLAG))
        {
            child = text_insert_source(*entries);
            if (child == 0)
            {
                listed = table[*entries & TEXT_INSERT_ID_MASK];
                if (listed == 0)
                {
                    continue;
                }
                child = listed;
            }
            count += text_count_entries(child, stop, limit != 0 ? limit - count : 0);
        }
    }
    return count;
}

/* Draw `text` typed out at (x, y): the glyphs up to text_index, as the typing state allows. */
static void text_draw_typed(u_long *ot, u16 *text, s16 x, s16 y)
{
    u8 *packet;

    packet = gs_work_alloc(0);
    packet = text_draw_string(ot, packet, text, x, y, FIXED_ONE, 1, 0, 0, 0);
    gs_work_release(packet);
}

static u16 text_get_state(void)
{
    return text_phase;
}

/* Restart typing from the first glyph of the current text. */
static void text_reset_state(void)
{
    text_index = 0;
    text_phase = TEXT_STATE_PRINTING;
}

/* Draw `text` (u16 characters; TEXT_LINE_BREAK ends a line, TEXT_END the text) one line
 * at a time with text_draw_line_centered, stepping y by `lineStep` per line. Each line is
 * copied to a buffer and terminated with TEXT_END there. Returns what the last
 * text_draw_line_centered call returned; `handle` is threaded through the calls. */
static void *text_draw_lines_centered(u_long *target, void *handle, u16 *text, s16 x, s16 y, s16 width, s32 scale,
                                      s16 lineStep, s16 spacing)
{
    TextLine line;
    u32 i;
    s32 done;
    u16 *cursor;

    cursor = text;
    done = 0;
    do
    {
        line = *(TextLine *)cursor;
        line.chars[40] = TEXT_END;
        for (i = 0; i < 41; i++)
        {
            if (line.chars[i] == TEXT_LINE_BREAK)
            {
                line.chars[i] = TEXT_END;
                i++;
                break;
            }
            if (line.chars[i] == TEXT_END)
            {
                done = 1;
                i++;
                break;
            }
        }
        cursor += i;
        handle = text_draw_line_centered(target, handle, line.chars, x, y, width, scale, spacing);
        y += lineStep;
    } while (done != 1);
    return handle;
}

static void *text_draw_line_centered(u_long *ot, void *handle, u16 *text, s16 x, s16 y, s16 width,
                                     s32 scale, s16 spacing)
{
    s32 value;
    /* Sets the global spacing for this line; the y spacing is cleared as a side effect. */
    text_set_spacing(spacing, 0);
    value = x + (width - text_measure(text, scale, spacing)) / 2;
    gs_work_release(
        text_draw_string(ot, gs_work_alloc(0), text, value, y, scale, 0, 0, 0, 0));
    return gs_work_alloc(0);
}

/* Width of a text list up to its terminator (TEXT_LINE_BREAK or TEXT_END): every glyph
 * adds `size / 512 + spacing`, every 0x4000 reference adds its sub-list's
 * width. A list ending on glyph 0x6C is narrowed by 3/4096 of `size`, and the
 * spacing after the last glyph is not counted. */
static s32 text_measure(u16 *entries, s32 size, s16 spacing)
{
    s32 width;
    u16 *child;
    u16 entry;

    width = 0;
    while (*entries != TEXT_LINE_BREAK && *entries != TEXT_END)
    {
        entry = *entries;
        if ((entry & 0xF000) == 0x4000)
        {
            child = text_sources[entry & 0xFFF];
            if (child != 0)
            {
                width += text_measure(child, size, spacing);
            }
        }
        else
        {
            width += size / 512;
            width += spacing;
        }
        entries++;
    }
    if ((entries[-1] & 0xFFF) == 0x6C)
    {
        width -= size * 3 / FIXED_ONE;
    }
    return width - spacing;
}

static void *text_draw_line_right(u_long *ot, void *handle, u16 *text, s16 x, s16 y, s16 width,
                                  s32 scale, s16 spacing)
{
    s32 value;
    text_set_spacing(spacing, 0);
    value = x + width - text_measure(text, scale, spacing);
    gs_work_release(text_draw_string(ot, gs_work_alloc(0), text, value, y, scale, 0, 0, 0, 0));
    return gs_work_alloc(0);
}

/* Lay out `text` line by line: each line (up to a TEXT_LINE_BREAK or TEXT_END, the end
 * of the text) is copied out, terminated with TEXT_END and
 * drawn by text_draw_line_right at (x, y); y advances by `lineHeight` per line.
 * Returns the running value text_draw_line_right hands back. */
static void *text_draw_lines_right(u_long *target, void *handle, u16 *text, s16 x, s16 y, s16 width, s32 scale,
                                   s16 lineHeight, s16 spacing)
{
    TextLine line;
    u32 i;
    s32 done;
    u16 *cursor;

    cursor = text;
    done = 0;
    do
    {
        line = *(TextLine *)cursor;
        line.chars[40] = TEXT_END;
        for (i = 0; i < 41; i++)
        {
            if (line.chars[i] == TEXT_LINE_BREAK)
            {
                line.chars[i] = TEXT_END;
                i++;
                break;
            }
            if (line.chars[i] == TEXT_END)
            {
                done = 1;
                i++;
                break;
            }
        }
        cursor += i;
        handle = text_draw_line_right(target, handle, line.chars, x, y, width, scale, spacing);
        y += lineHeight;
    } while (done != 1);
    return handle;
}

/* Vertical menu cursor over `count` rows: up/down wrap it and play sound 5,
 * confirm plays sound 0 and returns the 1-based row. While nothing is
 * chosen, draw the pointer beside row *cursor and return 0. */
static s32 text_choice_menu(s16 x, s16 y, s16 *cursor, s16 count)
{
    s32 chosen = 0;

    if (input_pad1->triggered & PAD_DOWN)
    {
        sound_play(SOUND_MENU_CURSOR);
        *cursor = *cursor + 1;
        *cursor = (*cursor < count) ? *cursor : 0;
    }
    else if (input_pad1->triggered & PAD_UP)
    {
        sound_play(SOUND_MENU_CURSOR);
        *cursor = *cursor - 1;
        *cursor = (*cursor < 0) ? count - 1 : *cursor;
    }
    else if (input_pad1->pressed & PAD_CROSS)
    {
        sound_play(SOUND_MENU_CONFIRM);
        chosen = *cursor + 1;
    }
    if (chosen == 0)
    {
        text_draw_cursor(current_frame->otTags[0], x - 2, y + ((s16)text_spacing_y + TEXT_LINE_HEIGHT) * *cursor);
    }
    return chosen;
}

void text_draw_cursor(u_long *ot, s16 x, s16 y)
{
    ui_draw_icon_0b_1(ot, x + 8, y + 6);
}

static s32 text_choice_face_banks[4] = {10, 11, 12, 13};

/* Map a 1-based choice (1..4) to its face bank, 10..13; any other choice gives bank 10. */
static s32 text_choice_face_bank(u16 choice)
{
    unsigned int index;

    index = (choice - 1) & 0xFFFF;
    if (index >= 4U)
    {
        index = 0;
    }
    return text_choice_face_banks[index];
}

/* Queue an 80x80 sprite at (x, y) from texture row v=0xAF, then the texture
 * page it samples, into `ot`; nothing when id is -1. */
static void text_draw_face(u_long *ot, s16 x, s16 y, s16 id)
{
    PrimSprite *sprite;
    PrimDrTPage *tpage;

    if (id == -1)
    {
        return;
    }
    sprite = gs_work_alloc(sizeof(PrimSprite));
    tpage = gs_work_alloc(sizeof(PrimDrTPage));
    setlen(sprite, 4);
    sprite->code = 0x64;
    sprite->clut = 0x7A80;
    sprite->r0 = 0x80;
    sprite->g0 = 0x80;
    sprite->b0 = 0x80;
    sprite->x0 = x;
    sprite->y0 = y;
    sprite->w = 80;
    sprite->h = 80;
    sprite->u0 = 0;
    sprite->v0 = 0xAF;
    addPrim(ot, sprite);
    setlen(tpage, 1);
    tpage->code[0] = 0xE100068F;
    addPrim(ot, tpage);
}

/* The text bank for face code `code`: codes count down from TEXT_CODE_FACE_LAST. */
static s16 text_code_to_bank(s32 code)
{
    code &= 0xFFFF;
    return TEXT_CODE_FACE_LAST - code;
}

/* Once the bank being read has landed (text_bank_result), load it as a texture pack and
 * make `bank` the current bank. */
static void text_switch_bank(s16 bank)
{
    if ((text_bank_id != bank) && (text_bank_result != 0))
    {
        texture_pack_load(text_bank_result);
        farm_work_sector_release();
        text_bank_id = bank;
        text_bank_reading = 0;
    }
}

/* Read text bank `bank` into the work sector. `wait` 0 starts a read and blocks until it
 * lands; otherwise a read is started only when none is in flight, and it is polled once. */
static void text_load_bank(s32 bank, s32 wait)
{
    s16 index;
    s32 offset;
    u8 *loaded;

    index = bank;
    if (index == text_bank_loaded)
        return;
    if (index < 0)
        return;
    offset = index * TEXT_BANK_STRIDE;
    if (wait == 0 || text_bank_reading == 0)
    {
        if (farm_work_sector_get() == 0)
        {
            SystemError(0x54, 0);
        }
        afile_read_bytes(1, offset, TEXT_BANK_STRIDE, game_state.work.sector, 1, 1);
        text_bank_reading = 1;
        text_bank_loading = index;
    }
    do
    {
        loaded = afile_read_bytes(1, offset, TEXT_BANK_STRIDE, game_state.work.sector, 1, 0);
        text_bank_result = loaded;
        if (loaded != 0)
        {
            text_bank_loaded = text_bank_loading;
            return;
        }
    } while (wait == 0);
}

/* Move past the end of the current page (TEXT_CODE_PAGE_END) and restart typing there. */
static void text_next_page(void)
{
    u16 *current = text_current;
    u16 index = text_index;
    int level = 1;
    u16 *p;

    if (index)
    {
        level = index;
    }
    p = text_find_nth(current, TEXT_CODE_PAGE_END, 1, level);
    if (p)
    {
        text_current = p + 1;
        text_scroll_timer = 0;
        text_scroll_y = 0;
        text_scroll_state = 3;
        text_scrolling = 1;
        text_reset_state();
        text_scroll_started = 0;
        if (*text_current == TEXT_LINE_BREAK)
        {
            text_current++;
        }
    }
}

static u16 text_page_breaks[] = {TEXT_CODE_CHOICE_2, TEXT_CODE_CHOICE_3, TEXT_CODE_CHOICE_4, 0x0000};

static u16 text_page_phases[] = {TEXT_STATE_CHOICE_2, TEXT_STATE_CHOICE_3, TEXT_STATE_CHOICE_4, 0x0000};

/* Start a choice list at the first 2-, 3- or 4-row marker found: the text after it becomes
 * the page and the phase becomes the matching TEXT_STATE_CHOICE_n. */
static void text_start_choice(void)
{
    u16 index = text_index;
    s32 level = 1;
    u32 i;
    u16 *p;

    if (index)
    {
        level = index;
    }
    for (i = 0; i < 3; i++)
    {
        p = text_find_nth(text_current, text_page_breaks[i], 1, level);
        if (p)
        {
            text_current = p + 1;
            if (*text_current == TEXT_LINE_BREAK)
            {
                text_current = p + 2;
            }
            text_scroll_state = 3;
            text_scroll_timer = 0;
            text_scroll_y = 0;
            text_scrolling = 1;
            text_scroll_started = 0;
            text_choice = 0;
            text_phase = text_page_phases[i];
            text_index = text_count_entries(text_current, TEXT_END, 0);
            return;
        }
    }
}

/* The text inserts: a 0x4000 halfword whose low 12 bits name one of these. Each
 * stands for a name, a number or a generated list (text_insert_source). */
enum TextInsert
{
    TEXT_INSERT_PLAYER_NAME = 10,
    TEXT_INSERT_HORSE_NAME = 11,
    TEXT_INSERT_DOG_NAME = 12,
    TEXT_INSERT_FARM_NAME = 13,
    TEXT_INSERT_FAMILY_NAME = 14,      /* the child's name */
    TEXT_INSERT_FAMILY_MODE_NAME = 15, /* the name family.addressMode selects */
    TEXT_INSERT_STAGED_VALUE = 16,
    TEXT_INSERT_BASKET_COUNT = 17,
    TEXT_INSERT_FODDER = 18,
    TEXT_INSERT_LUMBER = 19,
    TEXT_INSERT_CHICKEN_FEED = 20,
    TEXT_INSERT_FISH_FOOD = 21,
    TEXT_INSERT_SLOT0_LIVESTOCK = 22,     /* the animal in mode slot 0 */
    TEXT_INSERT_SLOT1_LIVESTOCK = 23,     /* the animal in mode slot 1 */
    TEXT_INSERT_MINIGAME_SOURCE = 24,     /* minigames.textSource */
    TEXT_INSERT_EVENT_LIVESTOCK = 25,     /* the animal an event shows (ACTOR_FLAG_IN_EVENT) */
    TEXT_INSERT_EVENT_LIVESTOCK_ALT = 26, /* the same lookup as 25 */
    TEXT_INSERT_LIST_LIVESTOCK_AGED = 27, /* the livestock list, mode 0: died of age */
    TEXT_INSERT_LIST_FLAGGED_LIVESTOCK_AGED = 28,
    TEXT_INSERT_LIST_CHICKENS_AGED = 29,
    TEXT_INSERT_LIST_LIVESTOCK_OTHER = 30, /* the same lists, mode 1: died, not of age */
    TEXT_INSERT_LIST_FLAGGED_LIVESTOCK_OTHER = 31,
    TEXT_INSERT_LIST_CHICKENS_OTHER = 32,
    TEXT_INSERT_ANN_BIRTHDAY_DAY = 33,
    TEXT_INSERT_HELD_CHICKEN = 34, /* the chicken the player holds (ACTOR_FLAG_HELD) */
};

/* Resolve a text-insert code (low 12 bits, 10..34) to the object or text it
 * stands for; 0 when there is none. */
static void *text_insert_source(u16 code)
{
    void *result;
    s32 i;
    s16 index;

    result = 0;
    switch (code & TEXT_INSERT_ID_MASK)
    {
    case TEXT_INSERT_PLAYER_NAME:
        result = game_state.player.name;
        break;
    case TEXT_INSERT_HORSE_NAME:
        result = game_state.animals.horse.actor.name;
        break;
    case TEXT_INSERT_DOG_NAME:
        result = game_state.animals.dog.actor.name;
        break;
    case TEXT_INSERT_FARM_NAME:
        result = game_state.progress.estate.farmName;
        break;
    case TEXT_INSERT_FAMILY_NAME:
        result = game_state.family.childName;
        break;
    case TEXT_INSERT_FAMILY_MODE_NAME:
        switch (game_state.family.addressMode)
        {
        case FAMILY_MODE_PLAYER_NAME:
            result = game_state.player.name;
            break;
        case FAMILY_MODE_HONEY:
            result = LOCALIZED_NAME(text_bank.names.honey); /* the first name, "Honey" */
            break;
        case FAMILY_MODE_DARLING:
            result = LOCALIZED_NAME(text_bank.names.darling); /* the second name, "Darling" */
            break;
        default:
            result = 0;
            break;
        }
        break;
    case TEXT_INSERT_STAGED_VALUE:
        result = text_write_number(text_find_staged_value(), text_number_buffer, 4);
        break;
    case TEXT_INSERT_BASKET_COUNT:
        result = text_write_number(game_state.progress.estate.basket.count, text_number_buffer, 4);
        break;
    case TEXT_INSERT_FODDER:
        result = text_write_number(game_state.progress.estate.fodder, text_number_buffer, 4);
        break;
    case TEXT_INSERT_LUMBER:
        result = text_write_number(game_state.progress.estate.lumber, text_number_buffer, 4);
        break;
    case TEXT_INSERT_CHICKEN_FEED:
        result = text_write_number(game_state.progress.estate.chickenFeed, text_number_buffer, 4);
        break;
    case TEXT_INSERT_FISH_FOOD:
        result = text_write_number(game_state.progress.estate.fishFood, text_number_buffer, 4);
        break;
    case TEXT_INSERT_SLOT0_LIVESTOCK:
        index = game_state.animals.slots[0].animal;
        if (index != -1)
        {
            result = game_state.animals.livestock[index].actor.name;
        }
        break;
    case TEXT_INSERT_SLOT1_LIVESTOCK:
        index = game_state.animals.slots[1].animal;
        if (index != -1)
        {
            result = game_state.animals.livestock[index].actor.name;
        }
        break;
    case TEXT_INSERT_MINIGAME_SOURCE:
        result = game_state.minigames.textSource;
        break;
    case TEXT_INSERT_EVENT_LIVESTOCK:
    case TEXT_INSERT_EVENT_LIVESTOCK_ALT:
        for (i = 0; i < MAX_LIVESTOCK; i++)
        {
            if (game_state.animals.livestock[i].actor.flags & ACTOR_FLAG_IN_EVENT)
            {
                result = game_state.animals.livestock[i].actor.name;
            }
        }
        break;
    case TEXT_INSERT_ANN_BIRTHDAY_DAY:
        result = text_write_number(game_state.villagers[CHARACTER_ANN].birthday.day + 1, text_number_buffer, 4);
        break;
    case TEXT_INSERT_LIST_LIVESTOCK_AGED:
        text_expand_request(0);
        result = text_list_buffer;
        break;
    case TEXT_INSERT_LIST_FLAGGED_LIVESTOCK_AGED:
        text_expand_request(1);
        result = text_list_buffer;
        break;
    case TEXT_INSERT_LIST_CHICKENS_AGED:
        text_expand_request(2);
        result = text_list_buffer;
        break;
    case TEXT_INSERT_LIST_LIVESTOCK_OTHER:
        text_expand_request(3);
        result = text_list_buffer;
        break;
    case TEXT_INSERT_LIST_FLAGGED_LIVESTOCK_OTHER:
        text_expand_request(4);
        result = text_list_buffer;
        break;
    case TEXT_INSERT_LIST_CHICKENS_OTHER:
        text_expand_request(5);
        result = text_list_buffer;
        break;
    case TEXT_INSERT_HELD_CHICKEN:
        for (i = 0; i < MAX_CHICKENS; i++)
        {
            if (game_state.animals.chickens[i].actor.flags & ACTOR_FLAG_HELD)
            {
                result = game_state.animals.chickens[i].actor.name;
            }
        }
        break;
    }
    return result;
}

static TextFill text_list_generators[3] = {text_list_livestock, text_list_flagged_livestock, text_list_chickens};

/* Expand an inline text request at the script cursor. The selector picks the
 * generator (selector % 3) and its argument (selector / 3); the generated
 * text replaces the one-halfword request code at the cursor, and the rest of
 * the script is shifted up behind it. */
static void text_expand_request(s32 selector)
{
    u16 *cursor;
    s32 inserted;
    s32 tail;

    text_list_buffer[0] = TEXT_END;
    cursor = &text_current[text_index];
    inserted = text_list_generators[selector % 3](selector / 3);
    for (tail = 0; cursor[tail] != TEXT_END; tail++)
    {
    }
    memmove((u8 *)(cursor + inserted), (u8 *)(cursor + 1), tail * sizeof(cursor[0]));
    memcpy(cursor, text_list_buffer, inserted * sizeof(cursor[0]));
    text_list_buffer[0] = TEXT_END;
}

/* Append the name of each livestock not flagged ACTOR_FLAG_BIT16 to text_list_buffer,
 * subject to text_append_name's death test for `mode`; return the length in halfwords. */
static s32 text_list_livestock(int mode)
{
    u16 *end;
    FarmLivestock *item;
    s32 i;

    end = text_list_buffer;
    item = game_state.animals.livestock;
    for (i = 0; i < MAX_LIVESTOCK; i++)
    {
        if (!(item->actor.flags & ACTOR_FLAG_BIT16))
        {
            end = text_append_name(end, item, mode);
        }
        item++;
    }
    return end - text_list_buffer;
}

/* Append each livestock flagged ACTOR_FLAG_BIT16 to text_list_buffer, subject to
 * text_append_name's death test for `mode`; return the length in halfwords. */
static s32 text_list_flagged_livestock(s32 mode)
{
    u16 *out;
    FarmLivestock *item;
    s32 i;

    out = text_list_buffer;
    item = game_state.animals.livestock;
    for (i = 0; i < MAX_LIVESTOCK; i++)
    {
        if (item->actor.flags & ACTOR_FLAG_BIT16)
        {
            out = text_append_name(out, item, mode);
        }
        item++;
    }
    return out - text_list_buffer;
}

/* Append, for each of the chickens, its name to text_list_buffer (see
 * text_append_name); return the length written in halfwords. */
static s32 text_list_chickens(int mode)
{
    u16 *end;
    FarmChicken *item;
    s32 i;

    end = text_list_buffer;
    item = game_state.animals.chickens;
    for (i = 0; i < MAX_CHICKENS; i++)
    {
        end = text_append_name(end, item, mode);
        item++;
    }
    return end - text_list_buffer;
}

/* The line added after the name of each dead animal (text_append_name). */
static u16 text_died_suffix[] = {
    0x0051,
    TEXT_LINE_BREAK,
    TEXT_END,
    0x0000,
};

/* Append `item`'s name and the died line to the list at `end` when it died: mode 0 takes
 * the animals that died of age, mode 1 the rest. Returns the new end. */
static void *text_append_name(void *end, void *item, int mode)
{
    FarmActor *actor = item;
    u32 flags = actor->flags;
    int died = flags & ACTOR_FLAG_DIED;
    int accepted;
    if (mode)
    {
        accepted = !(flags & ACTOR_FLAG_DIED_OF_AGE);
    }
    else
    {
        accepted = flags & ACTOR_FLAG_DIED_OF_AGE;
    }
    if (died && accepted)
    {
        end = text_list_append(end, actor->name);
        end = text_list_append(end, text_died_suffix);
    }
    return end;
}

/* Write `value` into `text` as width-1 glyphs and terminate it; the return is the first
 * glyph after any leading blanks. */
static u16 *text_write_number(u32 value, u16 *text, s32 width)
{
    u16 *p = text;
    u16 *r = text_format_number(value, text, width - 1);
    *r = TEXT_END;
    if (*p == TEXT_CODE_SPACE)
    {
        do
        {
            p++;
        } while (*p == TEXT_CODE_SPACE);
    }
    return p;
}

/* Map a character to its glyph code: digits, letters, a few marks, and a space for anything else. */
static s32 text_char_code(s32 input)
{
    input &= 0xFF;
    if (isdigit(input))
        return input - '0' + TEXT_GLYPH_DIGIT_0;
    if (isalpha(input))
        return (input - 'A') & 0xFFFF;
    switch (input)
    {
    case '!':
        return TEXT_GLYPH_EXCLAIM;
    case '?':
        return TEXT_GLYPH_QUESTION;
    case '-':
        return TEXT_GLYPH_MINUS;
    case '(':
        return TEXT_GLYPH_LPAREN;
    case ')':
        return TEXT_GLYPH_RPAREN;
    case '\n':
        return TEXT_LINE_BREAK;
    default:
        return TEXT_CODE_SPACE;
    }
}

/* Write `value` as `width` glyphs (at most 10): digits from TEXT_GLYPH_DIGIT_0 with leading
 * zeros as blanks, and a minus glyph in the leftmost place of a negative value. Returns the
 * address after the last glyph. */
u16 *text_format_number(s32 value, u16 *out, s32 width)
{
    u8 digits[10];
    s32 count;
    s32 negative;
    s32 index;
    s32 sum = 0;
    u8 digit;
    if (width >= 10)
    {
        width = 10;
    }
    negative = (value & 0x80000000) != 0;
    if (negative != 0)
    {
        value = -value;
    }
    count = 0;
    index = width - 1;
    if (width > sum)
    {
        do
        {
            digits[count] = value % 10;
            value /= 10;
            count++;
        } while (count < width);
    }
    if (negative != 0)
    {
        digits[index] = 10;
    }
    count = index;
    sum = 0;
    if (count >= 0)
    {
        do
        {
            digit = digits[count];
            if (digit == 10)
            {
                *out = TEXT_GLYPH_MINUS;
            }
            else if (((sum == 0) && (digit == 0)) && (count != 0))
            {
                *out = TEXT_CODE_SPACE;
            }
            else
            {
                sum += digits[count];
                *out = digits[count] + TEXT_GLYPH_DIGIT_0;
            }
            out++;
            count--;
        } while (count >= 0);
    }
    return out;
}

static u16 season_text_codes[24] = {
    0x0012,
    0x002F,
    0x0031,
    0x0028,
    0x002D,
    0x0026,
    0x0012,
    0x0034,
    0x002C,
    0x002C,
    0x0024,
    0x0031,
    0x0005,
    0x0020,
    0x002B,
    0x002B,
    TEXT_CODE_SPACE,
    TEXT_CODE_SPACE,
    0x0016,
    0x0028,
    0x002D,
    0x0033,
    0x0024,
    0x0031,
};

/* Write the six-glyph name of `season` to `text`; returns the next free halfword. */
u16 *text_write_season(s32 season, u16 *text)
{
    u16 *src;
    s32 count;
    src = season_text_codes + season * 6;
    count = 5;
    do
    {
        *text = *src;
        src++;
        count--;
        text++;
    } while (count >= 0);
    return text;
}

static u16 weekday_text_codes[22] = {
    0x0012,
    0x0034,
    0x002D,
    0x000C,
    0x002E,
    0x002D,
    0x0013,
    0x0034,
    0x0024,
    0x0016,
    0x0024,
    0x0023,
    0x0013,
    0x0027,
    0x0034,
    0x0005,
    0x0031,
    0x0028,
    0x0012,
    0x0020,
    0x0033,
    0x0000,
};

/* Write the three-glyph name of `weekday` to `text`; returns the next free halfword. */
u16 *text_write_weekday(s32 weekday, u16 *text)
{
    u16 *src = weekday_text_codes + weekday * 3;
    s32 count = 2;
    do
    {
        *text++ = *src++;
        count--;
    } while (count >= 0);
    return text;
}

/* Draw `value` as `width` digits (at most 9) at (x, y). A width of -1 draws it
 * nine digits wide with the leading blanks (glyph 0xFFD) skipped. */
void text_draw_number(s16 value, s16 width, u_long *display, s16 x, s16 y)
{
    u16 *digits;
    s32 count;
    s32 i;

    if (width != -1)
    {
        if (width >= 10)
        {
            return;
        }
        count = width;
    }
    else
    {
        count = 9;
    }
    *text_format_number(value, text_digits, count) = TEXT_END;
    digits = text_digits;
    if (width == -1)
    {
        for (i = 0; i < count && *digits == TEXT_CODE_SPACE; i++)
        {
            digits++;
        }
    }
    text_draw_plain(display, digits, x, y);
}

/* Copy the TEXT_END-terminated string `source` to `destination`, terminator included. */
void text_copy_string(u16 *destination, u16 *source)
{
    u16 value;

    do
    {
        value = *source++;
        *destination++ = value;
    } while (value != TEXT_END);
}

/* Draw the message window frame, then `destination` inside it, in text palette 3. */
void text_draw_in_frame(u_long *frame_slot, u_long *text_slot, u16 *destination)
{
    s32 previous;

    previous = text_get_palette();
    text_set_palette(3);
    window_frame_draw_at(frame_slot, 0, 26, 164, 267, 62);
    text_draw_plain(text_slot, destination, 30, 168);
    text_set_palette(previous);
}

/* Forget the face and the loaded bank (-1), and make the requested bank -3, which no real
 * bank matches, so the next switch always reloads. */
static void text_reset_bank_state(void)
{
    text_face_bank = -1;
    text_bank_loaded = -1;
    text_bank_id = -3;
}

/* Reset the bank state, then read and switch to the bank for face code `code` at once. */
static void text_select_bank(u16 code)
{
    s16 bank;
    text_reset_bank_state();
    bank = text_code_to_bank(code);
    text_load_bank(bank, 0);
    text_switch_bank(bank);
}

/* Draw the face for `code` above the message box at (x, y). */
static void text_draw_face_above(u_long *ot, s32 code, s16 x, s16 y)
{
    s32 x0 = x;
    s32 y0 = y;
    s16 bank = text_code_to_bank(code & 0xFFFF);
    text_draw_face(ot, x0 - 4, y0 - 84, bank);
}

/* Start loading the bank for face code `code`. Returns 1 once it is loaded and switched to,
 * 0 while it is still loading. */
s32 text_bank_ready(s32 code)
{
    s32 id;
    s32 current;
    s16 wanted;
    id = code & 0xFFFF;
    current = (s16)text_get_bank();
    wanted = text_code_to_bank(id);
    if (current != wanted)
    {
        text_load_bank(wanted, 1);
    }
    if (text_bank_loaded == wanted)
    {
        text_switch_bank(wanted);
        return 1;
    }
    return 0;
}

/* Draw the face for `code` at (x, y), with no offset. */
void text_draw_face_by_code(u_long *ot, s32 code, s16 x, s16 y)
{
    text_draw_face(ot, x, y, text_code_to_bank(code & 0xFFFF));
}

/* The three tables text_find_staged_value searches, in order. */
enum TextStagedSource
{
    TEXT_STAGED_HELD = 0,  /* player.held */
    TEXT_STAGED_TOOLS = 1, /* player.tools, 2 << rucksackLevel records */
    TEXT_STAGED_STORE = 2, /* the estate's toolbox store */
};

/* Search the three staged tables for the first watering can (ITEM_WATERING_CAN ..
 * ITEM_MYSTRILE_WATERING_CAN) and return its count. */
static s32 text_find_staged_value(void)
{
    FarmStoredItem *table = 0;
    s32 limit = 0;
    s32 table_index;
    s32 i;
    s16 id;

    for (table_index = TEXT_STAGED_HELD; table_index < 3; table_index++)
    {
        switch (table_index)
        {
        case TEXT_STAGED_HELD:
            table = game_state.player.held;
            limit = 1;
            break;
        case TEXT_STAGED_TOOLS:
            table = game_state.player.tools;
            limit = math_int_pow(2, game_state.player.rucksackLevel + 1);
            break;
        case TEXT_STAGED_STORE:
            table = game_state.progress.estate.toolboxItems;
            limit = ITEM_STORE_SLOTS;
            break;
        }
        for (i = 0; i < limit; i++)
        {
            id = table[i].id;
            if (id > ITEM_MYSTRILE_WATERING_CAN)
                continue;
            if (id < ITEM_WATERING_CAN)
                continue;
            return table[i].count;
        }
    }
    return 0;
}

void text_clear_bank(void)
{
    text_bank_id = TEXT_BANK_NONE;
}

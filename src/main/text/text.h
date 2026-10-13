#ifndef TEXT_H
#define TEXT_H


/* The message text system: glyphs, the message window and formatting. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

struct DrTPage;
struct Sprt;

/* An {a, b} message pair. */
typedef struct MessagePair { s16 a; s16 b; } MessagePair;

/* A message window's ordering-table slots: the frame, then the text drawn over it. */
#define MESSAGE_FRAME_OT 6
#define MESSAGE_TEXT_OT  5

/* Draws a message window into its ordering-table slots. */
#define SHOW_MESSAGE(ot, text) text_draw_in_frame(&(ot)[MESSAGE_FRAME_OT], &(ot)[MESSAGE_TEXT_OT], (text))

/* What text_window_frame returns for one frame. Its typing state is text_phase:
 * printing, waiting at a page break, at the end, or scrolling; the closed value
 * is returned when a button closes the window. 7..9 is a choice list of 2..4
 * rows; 10..13 is the choice the player picked. TEXT_WINDOW_DONE is not returned
 * by the text code: the menu sets it when it closes. */
enum TextWindowStatus {
    TEXT_WINDOW_PRINTING = 0,
    TEXT_WINDOW_SCROLL_WAIT = 1,
    TEXT_WINDOW_CLOSED = 2,
    TEXT_WINDOW_PAGE_WAIT = 3,
    TEXT_WINDOW_RESERVED_4 = 4,         /* never returned; the menu counts it as a confirm wait */
    TEXT_WINDOW_END_WAIT = 5,
    TEXT_WINDOW_SCROLLING = 6,
    TEXT_WINDOW_CHOICE_LIST_2 = 7,
    TEXT_WINDOW_CHOICE_LIST_3 = 8,
    TEXT_WINDOW_CHOICE_LIST_4 = 9,
    TEXT_WINDOW_CHOICE_0 = 10,
    TEXT_WINDOW_CHOICE_1 = 11,
    TEXT_WINDOW_CHOICE_2 = 12,
    TEXT_WINDOW_CHOICE_3 = 13,
    TEXT_WINDOW_DONE = 15,
};

/* The message window's typing state (text_phase): printing glyphs, scrolling a
 * full window, waiting at the end of the text or at a page break, or showing a
 * choice list of 2..4 rows. */
enum TextWindowState {
    TEXT_STATE_PRINTING = 0,
    TEXT_STATE_SCROLLING = 1,
    TEXT_STATE_END = 2,
    TEXT_STATE_PAGE_BREAK = 3,
    TEXT_STATE_CHOICE_2 = 7,
    TEXT_STATE_CHOICE_3 = 8,
    TEXT_STATE_CHOICE_4 = 9,
};

/* A TextWindowStatus value, returned as the window's 16-bit result. */
typedef u16 TextWindowResult;

/* The halfwords that end a line of a text list, and end the text. */
#define TEXT_LINE_BREAK 0xFFFE
#define TEXT_END 0xFFFF

/* The control codes of a text list: halfwords that text_draw_string reads as
 * commands rather than glyphs. TEXT_LINE_BREAK and TEXT_END (text.h) are not in it. */
enum TextControlCode {
    TEXT_CODE_PAGE_END = 0xFFB,         /* the end of a page: text_next_page steps past it */
    TEXT_CODE_PAGE_MARK = 0xFFC,        /* a page break: the window waits (TEXT_STATE_PAGE_BREAK) */
    TEXT_CODE_CHOICE_4 = 0xFF8,         /* the choice markers: 0xFFA starts a 2-row list, 0xFF9 3 rows, 0xFF8 4 */
    TEXT_CODE_CHOICE_3 = 0xFF9,
    TEXT_CODE_CHOICE_2 = 0xFFA,
    TEXT_CODE_FACE_BANK = 0xFF7,        /* switches to the bank of the last face code */
    TEXT_CODE_FACE_HIDE = 0xFF6,         /* clears text_face_shown */
    TEXT_CODE_FACE_RELOAD = 0xFF5,         /* sets text_face_reload */
    TEXT_CODE_FACE_SHOW = 0xFF4,         /* sets text_face_shown */
    TEXT_CODE_NO_SCROLL = 0xFF3,         /* sets text_no_scroll */
    TEXT_CODE_FLAG_FF2 = 0xFF2,         /* no effect */
    TEXT_CODE_SCROLL_HOLD = 0xFF1,         /* sets text_scroll_hold */
    TEXT_CODE_WAIT_FIRST = 0xE25,       /* 0xE25..0xE2F: wait 0xE2F - code frames */
    TEXT_CODE_WAIT_LAST = 0xE2F,
    TEXT_CODE_FACE_FIRST = 0xE38,       /* 0xE38..0xFEF: a face code, its bank is text_code_to_bank */
    TEXT_CODE_FACE_LAST = 0xFEF,
    TEXT_CODE_SPACE = 0xFFD,            /* a blank glyph */
};

void text_reset(void);
void text_set_source(s32 index, u16 *source);
void text_set_spacing(s16 x, s16 y);
void text_set_palette(u32 palette);
s32 text_get_palette(void);
void text_set_speed(s32 index);
void text_set_input(s32 value);
s16 text_get_wait_frames(void);
void text_draw_plain(u_long *ot, u16 *text, s16 x, s16 y);
TextWindowResult text_window_draw(u_long *ot, u16 *text, s16 x, s16 y, s16 unused, s16 height);
u16 text_get_bank(void);
void text_set_bank(s32 value);
void text_draw_semi(u_long *ot, u16 *text, s16 x, s16 y, u32 semiTrans);
u16 *text_list_append(u16 *dst, u16 *src);
s32 text_match_either(u16 *text, u16 *first, u16 *second);
void text_set_origin(s16 x, s16 y);
void text_draw_cursor(u_long *ot, s16 x, s16 y);
u16 *text_format_number(s32 value, u16 *out, s32 width);
u16 *text_write_season(s32 season, u16 *text);
u16 *text_write_weekday(s32 weekday, u16 *text);
void text_draw_number(s16 value, s16 width, u_long *display, s16 x, s16 y);
void text_copy_string(u16 *destination, u16 *source);
void text_draw_in_frame(u_long *display, u_long *source, u16 *destination);
s32 text_bank_ready(s32 code);
void text_draw_face_by_code(u_long *ot, s32 code, s16 x, s16 y);
void text_clear_bank(void);

extern char text_fmt_dec[];
extern char text_fmt_hex[];
extern char text_fmt_int[];

#endif /* TEXT_H */

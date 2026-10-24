#ifndef HMBTN_NEW_GAME_H
#define HMBTN_NEW_GAME_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "pad_buttons.h"

enum { HMBTN_MESSAGE_WORDS = 1024, HMBTN_NAME_CHARS = 8 };
typedef enum HmbtnMessagePhase {
    HMBTN_MESSAGE_CLOSED, HMBTN_MESSAGE_OPENING, HMBTN_MESSAGE_PRINTING,
    HMBTN_MESSAGE_WAIT_PAGE, HMBTN_MESSAGE_WAIT_END, HMBTN_MESSAGE_CLOSING,
    HMBTN_MESSAGE_SCROLLING
} HmbtnMessagePhase;
typedef struct HmbtnMessage {
    uint16_t words[HMBTN_MESSAGE_WORDS];
    unsigned count, page, cursor, box_frame, timer, wait_frames;
    HmbtnMessagePhase phase;
    bool face, no_scroll, blip;
    unsigned choices;
    unsigned lines, scroll_y;
    bool scroll_hold, suppress_cursor;
    unsigned face_bank, bank; /* Last selected face and active text/portrait bank. */
} HmbtnMessage;
typedef struct HmbtnNewGame {
    unsigned step; /* Original title steps 11..28; 100 is the host scene boundary. */
    unsigned request, queued, loaded;
    HmbtnMessage message;
    uint16_t name[HMBTN_NAME_CHARS + 1]; /* Current editor buffer. */
    uint16_t player_name[HMBTN_NAME_CHARS + 1], farm_name[HMBTN_NAME_CHARS + 1], dog_name[HMBTN_NAME_CHARS + 1];
    unsigned birthday_phase, birthday_value, season, day, fade;
    bool finished;
    unsigned length, cell, grid, choice, repeat_frames;
    uint32_t previous_held;
    bool confirming, dirty, repeating;
    int sound; /* Original bank-0 tone; -1 = no request. */
} HmbtnNewGame;
void hmbtn_new_game_init(HmbtnNewGame *game);
/* Reuse the original eight-glyph editor for an animal; step 102 holds its result. */
void hmbtn_name_editor_begin(HmbtnNewGame *game);
bool hmbtn_message_open(HmbtnMessage *message, const uint8_t *record, size_t size,
    char *error, size_t error_size);
int hmbtn_message_tick(HmbtnMessage *message, uint32_t pressed, uint32_t held);
void hmbtn_new_game_tick(HmbtnNewGame *game, uint32_t pressed, uint32_t held);
bool hmbtn_new_game_name_editor(const HmbtnNewGame *game);
/* menu_box_animate coordinates; false means the box has no pixels this tick. */
bool hmbtn_message_box(const HmbtnMessage *message, int *x, int *y, int *w, int *h);
#endif

#ifndef HMBTN_TITLE_FLOW_H
#define HMBTN_TITLE_FLOW_H
#include <stdint.h>
#include "pad_buttons.h"

/* Original TitleSequenceStep numbers. Negative states adapt publisher loading
 * to host I/O; registration is in new_game.c; saves and farm remain outside. */
typedef enum HmbtnTitleStep {
    HMBTN_TITLE_PUBLISHER_IN = -2, HMBTN_TITLE_PUBLISHER_OUT = -1,
    HMBTN_TITLE_INTRO_FADE_IN = 0, HMBTN_TITLE_INTRO_FADE_WAIT = 1,
    HMBTN_TITLE_INTRO = 2, HMBTN_TITLE_INTRO_FADE_OUT = 3,
    HMBTN_TITLE_SCENE_FADE_IN = 4, HMBTN_TITLE_SCENE_FADE_WAIT = 5,
    HMBTN_TITLE_START_WAIT = 6, HMBTN_TITLE_MENU = 7,
    HMBTN_TITLE_NEW_GAME = 8, HMBTN_TITLE_CONTINUE_FADE_WAIT = 9,
    HMBTN_TITLE_NEW_GAME_TEXT_OPEN = 11, HMBTN_TITLE_NEW_GAME_TEXT_WAIT = 12,
    HMBTN_TITLE_PLAYER_NAME_OPEN = 13, HMBTN_TITLE_PLAYER_NAME_START = 14,
    HMBTN_TITLE_PLAYER_NAME_ENTRY = 15,
    HMBTN_TITLE_BIRTHDAY_OPEN = 16, HMBTN_TITLE_BIRTHDAY_WAIT = 17, HMBTN_TITLE_BIRTHDAY_PICKER = 18,
    HMBTN_TITLE_FARM_NAME_OPEN = 19, HMBTN_TITLE_FARM_NAME_WAIT = 20, HMBTN_TITLE_FARM_NAME_ENTRY = 21,
    HMBTN_TITLE_DOG_NAME_OPEN = 22, HMBTN_TITLE_DOG_NAME_WAIT = 23, HMBTN_TITLE_DOG_NAME_ENTRY = 24,
    HMBTN_TITLE_SUMMARY_OPEN = 25, HMBTN_TITLE_SUMMARY = 26,
    HMBTN_TITLE_NEW_GAME_FADE_WAIT = 27, HMBTN_TITLE_NEW_GAME_LEAVE = 28,
    HMBTN_TITLE_UNAVAILABLE = 100
} HmbtnTitleStep;

typedef enum HmbtnTitleDestination {
    HMBTN_TITLE_DESTINATION_NONE, HMBTN_TITLE_DESTINATION_NEW_GAME,
    HMBTN_TITLE_DESTINATION_CONTINUE, HMBTN_TITLE_DESTINATION_DEMO
} HmbtnTitleDestination;

enum { HMBTN_TITLE_FADE_TICKS = 10, HMBTN_TITLE_IDLE_TICKS = 1800,
    HMBTN_TITLE_PUBLISHER_FADE_TICKS = 30 };

typedef enum HmbtnTitleSound {
    HMBTN_TITLE_SOUND_NONE, HMBTN_TITLE_SOUND_CONFIRM, HMBTN_TITLE_SOUND_CURSOR
} HmbtnTitleSound;

typedef enum HmbtnTitleLogoStep {
    HMBTN_LOGO_BEGIN, HMBTN_LOGO_WAIT, HMBTN_LOGO_SLIDE, HMBTN_LOGO_DROP,
    HMBTN_LOGO_WIPE_IN, HMBTN_LOGO_BLINK, HMBTN_LOGO_WIPE_OUT, HMBTN_LOGO_RESET
} HmbtnTitleLogoStep;

typedef struct HmbtnTitleFlow {
    HmbtnTitleStep step;
    HmbtnTitleDestination destination;
    uint32_t logo_frames, idle_frames;
    uint16_t menu_row; /* 0 new game, 1 continue (original default). */
    uint16_t brightness; /* 0..10 scene, 0..30 publisher. */
    HmbtnTitleLogoStep logo_step;
    int16_t slide, drop, wipe, draw_slide, draw_drop, draw_wipe;
    uint8_t visible; /* bit 0 logo, bit 1 copyright, bit 2 prompt */
    HmbtnTitleSound sound; /* One original menu sound request per tick. */
} HmbtnTitleFlow;

void hmbtn_title_flow_init(HmbtnTitleFlow *flow);
void hmbtn_title_flow_tick(HmbtnTitleFlow *flow, uint32_t pressed);
#endif

#include "title_flow.h"
#include <stdio.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition); return 1; } } while (0)

int main(void)
{
    HmbtnTitleFlow flow;
    hmbtn_title_flow_init(&flow);
    CHECK(flow.step == HMBTN_TITLE_PUBLISHER_IN && flow.menu_row == 1 && flow.brightness == 0);
    /* Publisher ignores input, fades on white before any game options. */
    for (unsigned i = 0; i < 60; ++i) hmbtn_title_flow_tick(&flow, HMBTN_PAD_START);
    CHECK(flow.step == 0 && flow.brightness == 0 && flow.visible == 0);
    for (unsigned i = 0; i < 11; ++i) hmbtn_title_flow_tick(&flow, 0);
    CHECK(flow.step == 2 && flow.brightness == 10 && flow.visible == 0);
    while (flow.logo_step == HMBTN_LOGO_WAIT) hmbtn_title_flow_tick(&flow, 0);
    CHECK(flow.logo_step == HMBTN_LOGO_SLIDE && flow.visible == 0);
    hmbtn_title_flow_tick(&flow, 0);
    CHECK(flow.draw_slide == -160 && flow.visible == 1);
    for (unsigned i = 1; i < 160; ++i) hmbtn_title_flow_tick(&flow, 0);
    CHECK(flow.logo_step == HMBTN_LOGO_DROP && flow.draw_slide == -1);
    hmbtn_title_flow_tick(&flow, 0);
    CHECK(flow.visible == 3 && flow.draw_drop == 48);
    for (unsigned i = 1; i < 48; ++i) hmbtn_title_flow_tick(&flow, 0);
    CHECK(flow.logo_step == HMBTN_LOGO_WIPE_IN);
    hmbtn_title_flow_tick(&flow, 0);
    CHECK(flow.visible == 7 && flow.draw_wipe == 320);
    for (unsigned i = 1; i < 160; ++i) hmbtn_title_flow_tick(&flow, 0);
    CHECK(flow.logo_step == HMBTN_LOGO_BLINK && flow.logo_frames == 0);
    for (unsigned i = 0; i < 15; ++i) hmbtn_title_flow_tick(&flow, 0);
    CHECK(flow.visible == 3);
    for (unsigned i = 15; i < 90; ++i) hmbtn_title_flow_tick(&flow, 0);
    CHECK(flow.logo_step == HMBTN_LOGO_WIPE_OUT);
    for (unsigned i = 0; i < 80; ++i) hmbtn_title_flow_tick(&flow, 0);
    CHECK(flow.logo_step == HMBTN_LOGO_RESET && flow.draw_wipe == -158);
    hmbtn_title_flow_tick(&flow, 0);
    CHECK(flow.logo_step == HMBTN_LOGO_WIPE_IN && flow.wipe == 320);
    hmbtn_title_flow_tick(&flow, HMBTN_PAD_CROSS);
    CHECK(flow.step == 3 && flow.sound == HMBTN_TITLE_SOUND_CONFIRM);
    for (unsigned i = 0; i < 10; ++i) hmbtn_title_flow_tick(&flow, 0);
    CHECK(flow.step == 4 && flow.brightness == 0);
    for (unsigned i = 0; i <= HMBTN_TITLE_FADE_TICKS; ++i) hmbtn_title_flow_tick(&flow, 0);
    CHECK(flow.step == 6 && flow.brightness == 10);
    hmbtn_title_flow_tick(&flow, HMBTN_PAD_CROSS);
    CHECK(flow.step == 6); /* Original gate requires Start, not Cross. */
    hmbtn_title_flow_tick(&flow, HMBTN_PAD_START);
    CHECK(flow.step == 7 && flow.menu_row == 1 && flow.sound == HMBTN_TITLE_SOUND_CONFIRM);
    hmbtn_title_flow_tick(&flow, HMBTN_PAD_UP | HMBTN_PAD_START);
    CHECK(flow.step == 7 && flow.menu_row == 0 && flow.sound == HMBTN_TITLE_SOUND_CURSOR); /* Navigation takes priority. */
    hmbtn_title_flow_tick(&flow, HMBTN_PAD_SELECT);
    CHECK(flow.menu_row == 1);
    hmbtn_title_flow_tick(&flow, HMBTN_PAD_DOWN);
    CHECK(flow.menu_row == 0);
    hmbtn_title_flow_tick(&flow, HMBTN_PAD_CROSS);
    CHECK(flow.step == 8);
    hmbtn_title_flow_tick(&flow, 0);
    CHECK(flow.step == HMBTN_TITLE_NEW_GAME_TEXT_OPEN);
    /* new_game.c owns states 11..16 and signals the next unsupported boundary. */
    flow.step = HMBTN_TITLE_UNAVAILABLE;
    flow.destination = HMBTN_TITLE_DESTINATION_NEW_GAME;
    hmbtn_title_flow_tick(&flow, HMBTN_PAD_CIRCLE);
    CHECK(flow.step == 7 && flow.destination == HMBTN_TITLE_DESTINATION_NONE);
    hmbtn_title_flow_tick(&flow, HMBTN_PAD_UP);
    hmbtn_title_flow_tick(&flow, HMBTN_PAD_START);
    CHECK(flow.step == 9);
    for (unsigned i = 0; i < HMBTN_TITLE_FADE_TICKS; ++i) hmbtn_title_flow_tick(&flow, 0);
    CHECK(flow.step == HMBTN_TITLE_UNAVAILABLE && flow.destination == HMBTN_TITLE_DESTINATION_CONTINUE);
    hmbtn_title_flow_tick(&flow, HMBTN_PAD_START);
    CHECK(flow.step == 7 && flow.brightness == 10);
    flow.step = HMBTN_TITLE_START_WAIT;
    flow.logo_frames = HMBTN_TITLE_IDLE_TICKS - 1;
    hmbtn_title_flow_tick(&flow, HMBTN_PAD_START);
    CHECK(flow.step == 7 && flow.destination == HMBTN_TITLE_DESTINATION_NONE);
    flow.step = HMBTN_TITLE_START_WAIT;
    flow.logo_frames = HMBTN_TITLE_IDLE_TICKS - 1;
    hmbtn_title_flow_tick(&flow, 0);
    CHECK(flow.step == HMBTN_TITLE_UNAVAILABLE && flow.destination == HMBTN_TITLE_DESTINATION_DEMO);
    return 0;
}

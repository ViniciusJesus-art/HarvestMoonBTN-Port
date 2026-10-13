/* The farm's menu screens, run every frame in order, and their open and close hooks. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <memory.h>
#include "calendar.h"
#include "farm_main.h"
#include "house.h"
#include "item_storage.h"
#include "menus.h"
#include "options_menu.h"
#include "photo_album.h"
#include "shop_page.h"
#include "status_pages.h"
#include "storage_swap.h"
#include "ui_icons.h"

/* Run each menu screen's per-frame step, in order. */
void item_storage_reset(void) {
    ui_blink_tick(0);
    item_result_step();
    farm_calendar_frame();
    storage_swap_tick();
    status_latch_draw_clock();
    options_menu_close();
    cooking_menu_tick();
    options_pick_tick();
    status_page_step();
    shop_menu_tick();
    photo_viewer_step();
    photo_sequence_tick();
}

/* Empty hook, called by the farm reset. */
void farm_reset_hook(void) {
}

/* Empty hook: called when the calendar opens. */
void calendar_open_hook(void) {
}

/* Empty hook: called when the calendar closes. */
void calendar_close_hook(void) {
}

/* The calendar's close hook runs when this is 1 (it always is). */
int calendar_close_hook_enabled(void) {
    return 1;
}

/* Unused: unpauses the farm. */
static void item_storage_unpause(void) {
    farm_unpause();
}

/* Unused empty stub. */
static void item_storage_stub_1(void) {
}

/* Unused: always 1. */
static int item_storage_stub_2(void) {
    return 1;
}

/* The villagers' hearts: the heart level from affection, and the heart sprite drawn for the selected villager. */
#include "types.h"
#include "anim.h"
#include "farm_state.h"
#include "heart.h"
#include "main.h"
#include "menu_window.h"
#include "text.h"

static s32 heart_level_for_villager(s32 slot);

/* The sprite bank that means no villager is selected. */
#define HEART_BANK_NONE 7

/* Pick the sprite bank for the current selection (7 when nothing is
 * selected), switch the sprite to it if it changed, and draw it. */
void heart_icon_draw(void) {
    s16 selection;
    s32 bank;

    selection = text_get_wait_frames();
    bank = HEART_BANK_NONE;
    if (selection != 0) {
        bank = heart_level_for_villager(selection);
    }
    if (menu_anim.animation != bank) {
        anim_select_range(&menu_anim, bank & 0xFF);
    }
    anim_draw_sprt_advance(&current_frame->otTags[0][1], &menu_anim, 285, 217);
}

/* The villager record each selection slot (0..5) reads its heart level from. */
short heart_index_table[6] = {0x0000, 0x0003, 0x0007, 0x0009, 0x000E, 0x0014};

/* The heart level, 0 to 6, of a villager from its affection: 1 from 5000, 2 from 20000,
 * then one more heart per 10000 up to 6. */
static s32 heart_level_for_villager(s32 slot) {
    FarmState *farm;
    u16 value;
    s16 index;

    index = slot;
    farm = &game_state;
    index = heart_index_table[index];
    value = farm->villagers[index].affection;
    if (value < 5000U) {
        return 0;
    }
    if (value < 20000U) {
        return 1;
    }
    if (value < 30000U) {
        return 2;
    }
    if (value < 40000U) {
        return 3;
    }
    if (value < 50000U) {
        return 4;
    }
    if (value < 60000U) {
        return 5;
    }
    return 6;
}

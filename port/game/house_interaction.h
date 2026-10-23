#ifndef HMBTN_HOUSE_INTERACTION_H
#define HMBTN_HOUSE_INTERACTION_H
#include "house_control.h"
#include "new_game.h"
typedef enum HmbtnHouseMode {
    HMBTN_HOUSE_FREE, HMBTN_HOUSE_TV_SELECT, HMBTN_HOUSE_TV,
    HMBTN_HOUSE_CALENDAR, HMBTN_HOUSE_TOOLBOX_OPEN, HMBTN_HOUSE_TOOLBOX,
    HMBTN_HOUSE_BOOK, HMBTN_HOUSE_OPTIONS, HMBTN_HOUSE_BED,
    HMBTN_HOUSE_SLEEP
} HmbtnHouseMode;
typedef struct HmbtnHouseOptions {
    unsigned layout, page, row, column, opening_cursor, manual_row, manual_column;
    unsigned idle_frames;
    bool vibration, stereo, clock, rotated, rumble;
} HmbtnHouseOptions;
typedef struct HmbtnStoredTool { uint16_t id, modifier, count; } HmbtnStoredTool;
typedef struct HmbtnHouseInteraction {
    HmbtnHouseMode mode;
    HmbtnMessage message;
    unsigned request, loaded, message_bank, tv_channel, tv_group, tv_ticks;
    unsigned year, season, day, weekday, hour, weather, tomorrow;
    unsigned calendar_day, cursor, page, picked, picked_page, pan;
    unsigned steps;
    HmbtnStoredTool equipped, bag[8], toolbox[64];
    HmbtnHouseOptions options;
    unsigned bed_choice, boundary; /* 1 overnight, 2 diary/load, 3 livestock. */
    bool save_requested;
    int sound;
    bool detail, closing, tv_dirty;
} HmbtnHouseInteraction;
void hmbtn_house_interaction_init(HmbtnHouseInteraction *house);
bool hmbtn_house_interact(HmbtnHouseInteraction *house, const uint8_t *section,
    size_t size, const HmbtnIntroActor *player);
void hmbtn_house_interaction_tick(HmbtnHouseInteraction *house, uint32_t pressed, uint32_t held);
uint32_t hmbtn_house_world_buttons(const HmbtnHouseOptions *options, uint32_t held);
void hmbtn_house_options_idle(HmbtnHouseOptions *options, bool moving, unsigned facing);
unsigned hmbtn_house_calendar_message(unsigned season, unsigned day);
void hmbtn_house_tv_program(const HmbtnHouseInteraction *house, unsigned channel,
    unsigned *message, unsigned *group);
#endif

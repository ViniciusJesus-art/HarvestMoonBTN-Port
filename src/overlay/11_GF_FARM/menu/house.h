#ifndef HOUSE_H
#define HOUSE_H

/* The kitchen: the cooking menu, the utensils and the recipes. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "item_storage.h"

/* One of the 0x50 cooking records; id 0 is a free record. */
typedef struct FarmRecipe {
    u16 id;
    s16 value;
    u16 options;        /* option flags when stored */
    u16 values[8];      /* record values when stored */
} FarmRecipe;

/* FarmEstate.utensils: the kitchen tools owned, one bit each, in item order. */
enum Utensil {
    UTENSIL_KNIFE = 0,
    UTENSIL_FRYING_PAN = 1,
    UTENSIL_POT = 2,
    UTENSIL_MIXER = 3,
    UTENSIL_WHISK = 4,
    UTENSIL_ROLLING_PIN = 5,
    UTENSIL_OVEN = 6,
    UTENSIL_SEASONING_SET = 7,
};

enum UtensilFlag {
    UTENSIL_FLAG_KNIFE = (1 << UTENSIL_KNIFE),
    UTENSIL_FLAG_FRYING_PAN = (1 << UTENSIL_FRYING_PAN),
    UTENSIL_FLAG_POT = (1 << UTENSIL_POT),
    UTENSIL_FLAG_MIXER = (1 << UTENSIL_MIXER),
    UTENSIL_FLAG_WHISK = (1 << UTENSIL_WHISK),
    UTENSIL_FLAG_ROLLING_PIN = (1 << UTENSIL_ROLLING_PIN),
    UTENSIL_FLAG_OVEN = (1 << UTENSIL_OVEN),
    UTENSIL_FLAG_SEASONING_SET = (1 << UTENSIL_SEASONING_SET),
};

void cooking_menu_open_choose(void);
void cooking_menu_open_records(void);
void cooking_menu_tick(void);
s32 cooking_table_has_id(s16 key);

FarmStoredItem *farm_stored_item_at(s8 a, s8 b);

void options_pick_reset(void);
void options_pick_clear(void);
void options_pick_tick(void);

extern s32 options_pick_result;
extern u8 cooking_menu_step;

/* The handler the menu runs now. */
extern void (*farm_menu_handler)(void);

/* The handler started last, to tell when the current one changes. */
extern void (*farm_menu_handler_last)(void);

#endif /* HOUSE_H */

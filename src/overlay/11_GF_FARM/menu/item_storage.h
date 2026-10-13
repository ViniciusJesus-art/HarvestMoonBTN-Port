#ifndef ITEM_STORAGE_H
#define ITEM_STORAGE_H

/* The farm's item stores. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

/* One record of the farm's stores: the player's held tool and item, tool bag
 * and rucksack, the boxes, the fridge. id 0 is an empty slot. */
typedef struct FarmStoredItem {
    s16 id;
    s16 modifier;       /* the item's modifier: a dish's tastiness, a gift's contents */
    s16 count;
} FarmStoredItem;
void item_result_step(void);
void item_result_begin_state6(void);
void item_result_begin_state7(void);
void item_result_begin_state4(void);
void item_picture_slots_clear(void);
void item_picture_slots_unmark(void);
void item_entry_refresh(u_long *object, u16 id, s16 x, s16 y);
void item_effect_submit(u_long *ot, s16 posX, s16 posY);
void item_empty_marker_draw(u_long *ot, s16 x, s16 y);
void item_anim_draw(u_long *ot, s16 posX, s16 posY, s32 value);
s16 item_pictures_load_poll(s16 arg);


#endif /* ITEM_STORAGE_H */

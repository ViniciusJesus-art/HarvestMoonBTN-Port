#ifndef FARM_DATA_H
#define FARM_DATA_H

/* The farm's animation and item art tables of contents, and the weather and
 * resource tables. */

#include "types.h"
#include "anim.h"

typedef struct {
    s16 x;
    s16 y;
} HouseDoorPosition;


/* Tables of contents of the seven slot packs. Slots 2 to 5 hold one pack per season. */
extern AnimPackToc *farm_slot_tocs[7];
/* Tables of contents of the item art packs. */
extern AnimPackToc *farm_item_tocs[];
/* Table of contents of each farm resource's pack. */
extern AnimPackToc *farm_pack_tocs[];
extern s32 drift_by_hour[4][3];
extern s32 drift_by_weather[8][3];
extern HouseDoorPosition house_door_positions[3];
extern s16 place_entries[];
extern s16 *resource_lists[10];

#endif /* FARM_DATA_H */

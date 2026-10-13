#ifndef FARM_VARS_H
#define FARM_VARS_H


/* The farm's loose variables: the section and zone being loaded and entered. */
#include "types.h"
#include "scene.h"

extern s32 farm_section_id;
extern s32 zone_event_pending;
/* The zone the player walked into, whose event runs. */
extern FarmZone *current_zone;
/* Steps 11 to 15 repeat steps 3 to 7; there is no step 8. */
extern u32 farm_startup_phase;
extern u16 *farm_store_source;
extern s32 farm_loaded_slot;
extern s16 farm_sector_row;

#endif /* FARM_VARS_H */

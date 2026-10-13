#include "types.h"

/* The farm overlay's globals. */
s32 farm_section_id = 0;
s32 zone_event_pending = 0;
struct FarmZone *current_zone = 0;
u32 farm_startup_phase = 0;
u16 *farm_store_source = 0;
s32 farm_loaded_slot = 0;
s16 farm_sector_row = 0;

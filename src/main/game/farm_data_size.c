#include "types.h"

/* Size of the farm section, set by farm_load and read by the free-memory check. farm_swind_size
 * is the swind model index saved when the windmill changes; farm_main reads it back as the area. */
s32 farm_section_size = 0;
s32 farm_swind_size = 0;

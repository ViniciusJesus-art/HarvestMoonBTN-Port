#include <memory.h>
#include "types.h"
#include "afile.h"
#include "farm_state.h"
#include "farm_tables.h"
#include "main.h"
#include "overlay_regions.h"
#include "pad.h"

FarmState game_state;
FarmLoadArea farm_area;

/* Clear both farm-state objects, register the tables, lay out the sector-aligned load
 * window, and put the input demo buffer just below the region base. */
void farm_state_init(void) {
    u32 size;
    u32 rounded;

    memset(&game_state, 0, sizeof(game_state));
    memset(&farm_area, 0, sizeof(farm_area));
    farm_tables_register(0);
    size = overlay_region_table[OVERLAY_HIGH] - overlay_region_table[OVERLAY_LOW];
    /* whole CD sectors, and one more for a partial sector */
    rounded = size / DISC_SECTOR_SIZE * DISC_SECTOR_SIZE;
    if (size % DISC_SECTOR_SIZE) {
        rounded += DISC_SECTOR_SIZE;
    }
    farm_area.loadWindow = overlay_region_table[OVERLAY_LOW] + rounded;
    farm_area.sectionBuffer = farm_area.loadWindow + FARM_IMAGE_SIZE;
    game_state.demo = (InputDemo *)(main_get_region_base() - sizeof(InputDemo));
}

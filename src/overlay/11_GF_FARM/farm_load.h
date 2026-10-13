#ifndef FARM_LOAD_H
#define FARM_LOAD_H

/* Loading the farm's resources from the CD: the sections, the season and item art, the
 * sprite and texture blocks. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

/* Resource ids farm_load_resource_id picks a pack for. Id 14 is the day's pack: a part per
 * six hours, three parts a weekday. 0x3E6 is the wife's pack: three a wife, one a child level. */
enum FarmResourceId {
    FARM_RESOURCE_DAY = 14,
    FARM_RESOURCE_WIFE = 0x3E6,
    FARM_RESOURCE_WIFE_BASE = 0x23,
    FARM_RESOURCE_PARTS = 3,
    FARM_RESOURCE_HOURS_PER_PART = 6,
};

void farm_load_section(s32 index, s32 cached);
void farm_startup_pause_hook(void);
void farm_load_player_sprite(void);
void farm_load_span(s32 index);
void farm_load_anim8(s32 index);
void farm_clear_loaded_slot(void);
s32 farm_section_load_async(s32 index, s32 restart);
void farm_load_resource(s32 slot);
void farm_load_season_art(s32 index);
void farm_load_slot_six(void);
void farm_load_hook(void);
void farm_load_item_art(s32 id);
void farm_load_fixed_block(void);
void farm_load_resource_id(s16 id, s16 sync);
void farm_load_sector_row(s16 row);
void farm_load_sector_group(s32 group);
void farm_load_texture_block(void);
void farm_load_framebuffer_header(void);

#endif /* FARM_LOAD_H */

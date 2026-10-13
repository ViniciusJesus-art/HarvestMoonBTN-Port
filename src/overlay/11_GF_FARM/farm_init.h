#ifndef FARM_INIT_H
#define FARM_INIT_H

/* Start-up of the farm overlay. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "scene.h"

void farm_bring_up(void);
void farm_apply_mode_flags(void);
void farm_clear_resource_buffer(void);
void farm_startup(void);
void farm_reenter(void);
void farm_enter_startup(void);
s32 farm_event_available(s16 id);
void farm_zones_reset(void);
void farm_zones_register_script(FarmModeScript *data);
s32 farm_zone_trigger(s16 event, u16 value);

extern FarmZoneEntry farm_zone_entries[];

#endif /* FARM_INIT_H */

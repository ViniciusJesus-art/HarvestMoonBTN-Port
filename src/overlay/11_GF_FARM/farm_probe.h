#ifndef FARM_PROBE_H
#define FARM_PROBE_H

/* Probes of the farm: the ground under an actor, the zones, the tile and the villager or
 * record at a point, the frame's callback count and the status cache. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "farm_main.h"
#include "game_types.h"
#include "model.h"
#include "scene.h"
#include "villager.h"

/* The mode of a plain ground settle: no callback is tried, the ground lookup alone runs. */
#define FARM_SETTLE_GROUND_ONLY 0xC000

s32 farm_settle_on_ground(VECTOR *position, s32 mode);
s32 farm_callback_count(VECTOR *pos, struct LimitPair limits, s32 width, s32 depth, void **out_tile,
                         s32 *out_flags, void *walker, s32 mask);
/* The zone record the position falls in, or NULL. */
FarmZone *farm_zone_find(FarmModeScript *data, VECTOR *pos, s32 interact);
FarmZone *farm_zone_at_probe(FarmModeScript *data, ZonePoint *probe);
s32 farm_probe_tile(VECTOR pos, struct LimitPair limits, s16 mode, s32 flag);
s32 farm_zone_row_lookup(VECTOR pos, s16 kind, s32 *result, u16 *result_kind, u32 row);
s32 farm_callback_capped(VECTOR *pos, struct LimitPair limits, s32 width, s32 depth, void **out_tile,
                         s32 *out_flags, void *walker, s32 mask);
s32 farm_tile_occupant(VECTOR *pos, struct LimitPair limits, void **out_tile, s32 *out_flags,
                         void *walker, s32 mask);
void farm_callback_reset(void);
FarmVillager *farm_villager_at_point(ZonePoint *point, s16 *unused1);
s32 farm_record_find_key(ZonePoint *point, LimitPair *arg);
u32 farm_status_upper_bits(void);
s32 farm_status_kind(void);
s32 farm_status_cache(void);
s32 farm_probe_steps(FarmProbePosition *request, s32 index);

extern struct Model farm_model;  /* the farm's TMD model */

#endif /* FARM_PROBE_H */

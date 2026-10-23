#ifndef HMBTN_WORLD_MAP_H
#define HMBTN_WORLD_MAP_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
/* Stable host ids for the initial farm/house, three farm buildings and outdoor regions.
 * Serialized exits already contain a seasonal id; resolve against today's season. */
bool hmbtn_world_area(unsigned selection,unsigned season,unsigned *logical,unsigned *resource);
/* farm_track_id / farm_set_message_timing, sunny weather without event overrides. */
unsigned hmbtn_world_song(const uint8_t *section,size_t size,unsigned season,unsigned hour);
unsigned hmbtn_world_music_volume(const uint8_t *section,size_t size,unsigned season,unsigned hour);
/* scene_clamp_result with the section's original signed X/Z bounds. */
void hmbtn_world_camera(const uint8_t *section,size_t size,int32_t centre[3]);
#endif

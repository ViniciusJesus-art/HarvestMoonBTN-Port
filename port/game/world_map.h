#ifndef HMBTN_WORLD_MAP_H
#define HMBTN_WORLD_MAP_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
bool hmbtn_world_outdoors(const uint8_t *section,size_t size);
/* Stable host ids for the initial farm/house, three farm buildings and outdoor regions.
 * Serialized exits already contain a seasonal id; resolve against today's season. */
bool hmbtn_world_area(unsigned selection,unsigned season,unsigned *logical,unsigned *resource);
/* scene_resolve_selection/farm_layout_index: logical ids survive an upgrade. */
bool hmbtn_world_area_buildings(unsigned selection,unsigned season,bool coop_upgraded,unsigned house_level,unsigned barn_level,bool greenhouse,unsigned *logical,unsigned *resource);
bool hmbtn_world_area_coop(unsigned selection,unsigned season,bool upgraded,unsigned *logical,unsigned *resource);
/* farm_track_id / farm_set_message_timing, without scripted event overrides. */
unsigned hmbtn_world_song(const uint8_t *section,size_t size,unsigned season,unsigned hour);
unsigned hmbtn_world_music_volume(const uint8_t *section,size_t size,unsigned season,unsigned hour);
unsigned hmbtn_world_song_weather(const uint8_t *section,size_t size,unsigned season,unsigned hour,unsigned weather);
unsigned hmbtn_world_music_volume_weather(const uint8_t *section,size_t size,unsigned season,unsigned hour,unsigned weather);
/* scene_clamp_result with the section's original signed X/Z bounds. */
void hmbtn_world_camera(const uint8_t *section,size_t size,int32_t centre[3]);
#endif

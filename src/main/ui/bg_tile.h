#ifndef BG_TILE_H
#define BG_TILE_H


/* Tiling the background with a sprite or a cursor. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>

void bg_tile_mode_sprite(u_long *arg);
void bg_tile_mode_cursor(u_long *arg);

#endif /* BG_TILE_H */

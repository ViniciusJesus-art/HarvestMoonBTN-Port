#ifndef MINE_H
#define MINE_H

/* The mine: its layers and the returns from it. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "farm.h"
#include "player.h"

void mine_clear_grid(void);
s32 mine_check_player(FarmPlayer *player);
s32 mine_probe_grid(s16 restart);
s32 mine_dig(FarmPlayer *player);
void mine_update_transition(void);
void mine_next_layer_check(void);

extern FarmStep facing_steps[];

#endif /* MINE_H */

#ifndef STORAGE_SWAP_H
#define STORAGE_SWAP_H

/* Swapping items between the stores and the player. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>

void storage_swap_begin(void);
void storage_swap_tick(void);


#endif /* STORAGE_SWAP_H */

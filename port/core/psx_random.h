#ifndef HMBTN_PSX_RANDOM_H
#define HMBTN_PSX_RANDOM_H
#include <stdint.h>
/* screen_fade.c: explicit 32-bit state, including wrap and low-16-bit range. */
uint32_t hmbtn_psx_random_range(uint32_t *state,uint32_t range);
#endif

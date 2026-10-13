#ifndef PICTURE_CODES_H
#define PICTURE_CODES_H

/* The picture codes: one row per item picture, each four words. A word packs
 * ten 3-bit codes, slot 0 in the lowest bits, so in octal every digit is one
 * code and slot 0 is the rightmost. */

#include <sys/types.h>
#include <libgte.h>
#include "types.h"

/* One row of four words per ItemInfo.picture, ten 3-bit codes in each word. */
extern u32 picture_codes[174][4];

#endif /* PICTURE_CODES_H */

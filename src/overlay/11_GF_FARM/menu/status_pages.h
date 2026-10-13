#ifndef STATUS_PAGES_H
#define STATUS_PAGES_H

/* The status pages. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "anim.h"

void status_latch_set(void);
void status_latch_reset(void);
void status_latch_draw_clock(void);
void status_page_open(void);
void status_page_step(void);

s32 chicken_count_unflagged(void);
s32 livestock_count_cattle(void);
s32 livestock_count_sheep(void);
s32 status_count_pond_fish(void);
void status_draw_side_arrows(void);
void options_page_next(void);
void options_page_prev(void);
void status_draw_glyph_run_5(s16 count, s16 y, s16 height);
void status_draw_glyph_run_4(s16 a, s16 b, s16 c);
void status_draw_arrow_up(void);
void status_draw_arrow_down(void);

extern struct AnimState status_cursor_sprite; /* the cursor sprite of the status screen */
extern u16 status_page;
extern u16 status_cycle_frames;

#endif /* STATUS_PAGES_H */

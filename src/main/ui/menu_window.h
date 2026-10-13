#ifndef MENU_WINDOW_H
#define MENU_WINDOW_H


/* The menu box that opens from its centre, and the menu dispatcher. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "anim.h"
#include "text.h"

s32 menu_choice(void);
s32 menu_run_state(void);
s32 menu_request_open(s32 id, s32 dataIndex);
enum TextWindowStatus menu_get_phase(void);
s32 menu_phase_is_result(void);
void menu_set_sound_enabled(s32 arg);
void menu_reset(void);
void menu_begin_close(void);
s32 menu_box_animate(u_long *ot, s32 unused, s16 x, s16 y, s16 w, s16 h, s32 *frame, s32 duration,
                         s32 mode);

void window_frame_draw_at(u_long *ot, s32 unused, s16 x, s16 y, s16 w, s16 h);

/* The menu's animation sprite. */
extern AnimState menu_anim;

#endif /* MENU_WINDOW_H */

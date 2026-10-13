#ifndef MENUS_H
#define MENUS_H

/* The farm's menu screens, run every frame in order, and their open and close hooks. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

void item_storage_reset(void);
void farm_reset_hook(void);
void calendar_open_hook(void);
void calendar_close_hook(void);
s32 calendar_close_hook_enabled(void);

#endif /* MENUS_H */

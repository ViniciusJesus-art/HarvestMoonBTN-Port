#ifndef SHOP_PAGE_H
#define SHOP_PAGE_H

/* Shop page: the row being edited, the gold display, and the item name table. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

void shop_page_close(void);
s32 shop_row_all_zero(void);
s32 shop_menu_idle(void);
void shop_edit_begin(void);
void shop_menu_tick(void);

#endif /* SHOP_PAGE_H */

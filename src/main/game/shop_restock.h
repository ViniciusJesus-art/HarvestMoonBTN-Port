#ifndef SHOP_RESTOCK_H
#define SHOP_RESTOCK_H


/* Restocking the shop tables of FarmState. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>

/* Restocks the shops for the day: the ids and chances of each group. */
void shop_restock(void);

#endif /* SHOP_RESTOCK_H */

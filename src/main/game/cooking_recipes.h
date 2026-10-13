#ifndef COOKING_RECIPES_H
#define COOKING_RECIPES_H

/* The kitchen's recipes: one entry per way of making a dish, and the lists of
 * quality bonuses they share. The cooking menu's cooking_find_record (11_GF_FARM cooking.c)
 * walks `recipes` in order for the first entry the player's pick fits, so the order is the priority; the
 * final entry, the failed dish, always fits. */

#include <sys/types.h>
#include <libgte.h>
#include "farm_tables.h"

extern Recipe recipes[RECIPE_COUNT];

#endif /* COOKING_RECIPES_H */

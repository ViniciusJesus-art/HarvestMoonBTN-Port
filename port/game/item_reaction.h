#ifndef HMBTN_ITEM_REACTION_H
#define HMBTN_ITEM_REACTION_H
/* Original picture code: 0..6, 7 (no reaction), -1 (no picture), -2 (tool).
 * Wrapped gifts are resolved by the caller from their stored modifier. */
int hmbtn_item_villager_reaction(unsigned item,unsigned villager);
#endif

#include <memory.h>
#include "types.h"
#include "farm_state.h"
#include "screen_fade.h"
#include "shop_dialogues.h"
#include "shop_restock.h"

/* The horses that can run, ids 0..SHOP_RESTOCK_HORSES - 1. */
#define SHOP_RESTOCK_HORSES 40

/* Whether any runner of the bet table already is horse `id`. */
static int shop_has_item(FarmRaceBets *bets, s16 id) {
    unsigned int group;
    unsigned int row;
    for (group = 0; group < BET_RACES; group++) {
        for (row = 0; row < BET_RUNNERS; row++) {
            if (bets->rows[group][row].runner == id) return 1;
        }
    }
    return 0;
}

/* Two halfwords per horse id: its chance, then its rival chance. */
static u16 shop_restock_values[80] = {
    0x007F, 0x007F, 0x0040, 0x0000, 0x0000, 0x0000, 0x0060, 0x0020, 0x0000, 0x007F, 0x0040,
    0x0040, 0x007F, 0x007F, 0x0040, 0x0060, 0x0000, 0x0040, 0x0040, 0x007F, 0x007F, 0x007F,
    0x0060, 0x0060, 0x007F, 0x0000, 0x0020, 0x007F, 0x0020, 0x0000, 0x0020, 0x0040, 0x007F,
    0x0040, 0x0000, 0x007F, 0x0040, 0x007F, 0x007F, 0x0040, 0x0000, 0x0000, 0x0040, 0x0020,
    0x0040, 0x0000, 0x0060, 0x0040, 0x0000, 0x0020, 0x0000, 0x0060, 0x0040, 0x007F, 0x0060,
    0x007F, 0x0000, 0x007F, 0x0020, 0x0060, 0x007F, 0x0020, 0x0060, 0x0000, 0x007F, 0x0000,
    0x007F, 0x0060, 0x0040, 0x0040, 0x0000, 0x007F, 0x0020, 0x0020, 0x0000, 0x0040, 0x0040,
    0x007F, 0x007F, 0x007F,
};

/* Per runner slot: training min, training max, affection min, affection max. */
static u16 shop_restock_ranges[24] = {
    0, 150, 0, 255, 0, 99, 0, 255, 100, 199, 0,
    255, 100, 149, 0, 255, 0, 255, 0, 255, 150, 255,
    0, 255,
};

/* Restock the three six-runner races with distinct random horses (ids 0..39),
 * each with random training and affection from its slot's range; in race 3 row [2][2]
 * is the player's horse. */
void shop_restock(void) {
    FarmRaceBets *bets = &game_state.raceBets;
    s16 mode;
    u32 group;
    u32 row;
    s16 id;
    u32 highest;

    mode = bets->race;
    memset(bets, 0, sizeof(FarmRaceBets));
    for (group = 0; group < BET_RACES; group++) {
        for (row = 0; row < BET_RUNNERS; row++) {
            bets->rows[group][row].runner = -1;
        }
    }
    for (group = 0; group < BET_RACES; group++) {
        for (row = 0; row < BET_RUNNERS; row++) {
            do {
                id = rand_range(SHOP_RESTOCK_HORSES);
            } while (shop_has_item(bets, id) != 0);
            bets->rows[group][row].runner = id;
            bets->rows[group][row].chance = shop_restock_values[id * 2];
            bets->rows[group][row].rivalChance = shop_restock_values[id * 2 + 1];
            bets->rows[group][row].training =
                shop_restock_ranges[row * 4] +
                rand_range((u16)(shop_restock_ranges[row * 4 + 1] - shop_restock_ranges[row * 4]));
            bets->rows[group][row].affection =
                shop_restock_ranges[row * 4 + 2] +
                rand_range((u16)(shop_restock_ranges[row * 4 + 3] - shop_restock_ranges[row * 4 + 2]));
        }
    }
    if (mode == BET_RACE_HORSE) {
        bets->rows[2][2].runner = -1;
        bets->rows[2][2].chance = 0;
        bets->rows[2][2].rivalChance = 0;
        bets->rows[2][2].training = game_state.animals.horse.training;
        bets->rows[2][2].affection = game_state.animals.horse.actor.affection;
    }
    for (group = 0; group < BET_RACES; group++) {
        highest = 0;
        for (row = 0; row < BET_RUNNERS; row++) {
            if (highest < bets->rows[group][row].training) {
                highest = bets->rows[group][row].training;
            }
        }
        for (row = 0; row < BET_RUNNERS; row++) {
            bets->rows[group][row].odds =
                rand_range(4) + ((highest - bets->rows[group][row].training) *
                                        (highest - bets->rows[group][row].training) / 1000 +
                                    1);
        }
    }
}

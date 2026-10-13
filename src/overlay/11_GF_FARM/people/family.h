#ifndef FAMILY_H
#define FAMILY_H

/* The player's family, and the weddings of the rival couples. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "actor.h"
#include "clock.h"
#include "villager.h"

/* The five rival couples: the index into FarmWorld.rivalWeddings. */
enum RivalCouple {
    RIVAL_COUPLE_KAREN_RICK = 0,
    RIVAL_COUPLE_ANN_CLIFF = 1,
    RIVAL_COUPLE_ELLI_DOCTOR = 2,
    RIVAL_COUPLE_MARY_GRAY = 3,
    RIVAL_COUPLE_POPURI_KAI = 4,
    RIVAL_COUPLE_COUNT = 5,
};

/* A wedding: the day and the two villager records married; -1 is no record.
 * The player's own is player.wedding. */
typedef struct FarmWedding {
    FarmDate date;
    CharacterId groom;
    CharacterId bride;
} FarmWedding;

/* The name text insert 15 stands for (text_insert_source): the player's own name, or
 * the first or second name of the wedding ("Honey", "Darling") in the localized table. */
enum FamilyAddressMode {
    FAMILY_MODE_PLAYER_NAME = 0,
    FAMILY_MODE_HONEY = 1,
    FAMILY_MODE_DARLING = 2,
};

/* The player's family. */
typedef struct FarmFamily {
    u16 addressMode;        /* a FamilyAddressMode: which name text insert 15 stands for */
    u16 childGrowthDays;
    u16 pregnancyDays;
    u16 wifeRequests;       /* counts the 0x2B request, capped at 3 */
    s16 pregnancyTicks;     /* counts to 0x1E before pregnancyDays starts to rise */
    s16 wifeIndex;          /* which wife */
    s16 wifeVillager;
    s16 wifeIgnoredDays;    /* counts up while the 0x4B request is pending */
    s16 childLevel;
    s32 childNamed;
    u16 childName[NAME_LENGTH];  /* the text insert for code 14: the child's name */
    FarmDate selection;     /* a copy of the clock's date, taken when the bride is set */
    u8  pad32[0x34 - 0x32];
} FarmFamily;

void family_reset(void);
void family_set_bride(s16 mode);
void family_child_born(void);
void family_daily_update(void);
s32 family_table_lookup(void);


#endif /* FAMILY_H */

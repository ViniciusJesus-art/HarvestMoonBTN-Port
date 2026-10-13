#ifndef LIVESTOCK_H
#define LIVESTOCK_H

/* Cows and sheep: the overnight update, the handlers and their cues. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "actor.h"

/* Days of pregnancy before the birth. */
#define LIVESTOCK_GESTATION_DAYS 21

/* The states of a cow or sheep: the number is its taskNo, which is also its handler index. */
enum LivestockState {
    LIVESTOCK_STATE_SLEEP = 0x1,
    LIVESTOCK_STATE_WAKE = 0x2,
    LIVESTOCK_STATE_NEGLECT = 0x3,
    LIVESTOCK_STATE_SCOLD = 0x4,
    LIVESTOCK_STATE_TALK = 0x5,
    LIVESTOCK_STATE_PRAISE = 0x6,
    LIVESTOCK_STATE_PRODUCE = 0x7,      /* a cow gives its product */
    LIVESTOCK_STATE_SHEAR = 0x8,        /* a sheep is sheared */
    LIVESTOCK_STATE_CURE = 0x9,
    LIVESTOCK_STATE_BREED = 0xA,
    LIVESTOCK_STATE_APPROACH_CHANCE = 0xB,
    LIVESTOCK_STATE_APPROACH_PLAYER = 0xC,
    LIVESTOCK_STATE_APPROACH_WALK = 0xD,    /* walks up to the player */
    LIVESTOCK_STATE_BED = 0xF,              /* lies down outside the day hours */
    LIVESTOCK_STATE_GET_UP = 0x10,
    LIVESTOCK_STATE_WALK = 0x12,
};

/* One of the twenty barn animals (cows and sheep). */
typedef struct FarmLivestock {
    FarmActor actor;    /* kind ITEM_COW or ITEM_SHEEP */
    u16 grazeCount;     /* counts up while the animal is out in the pasture; 5 or more, with a ready crop near, means fed */
    u16 daysSick;       /* 病気期間: idle while this and pregnancy are 0 */
    u16 growthDays;     /* 成長期間 */
    u16 pregnancy;      /* 妊娠期間 */
    u16 daysSinceShearing;  /* 刈後日数 */
    s16 helperNumber;   /* the helper's number (FarmHelper.number); -1: none */
} FarmLivestock;
void livestock_sprite_refresh(FarmActor *object);
void livestock_pair_at_38(FarmLivestock *obj);
FarmLivestock *livestock_spawn(s32 slot, s32 unused1, s32 unused2);
FarmLivestock *livestock_copy_placement(s32 source, s32 destination);
s32 livestock_action_timer(FarmLivestock *obj);
s32 livestock_overnight_update(FarmLivestock *animal);
s32 livestock_dispatch_tick(FarmLivestock *object);
s32 livestock_action_dispatch(FarmLivestock *action, s32 state);
void livestock_clear_record(FarmLivestock *p);
s32 cow_idle_cue(FarmActor *self);
s32 cow_sleep_cue(FarmActor *self);
s32 cow_scold_cue(FarmActor *self);
s32 cow_happy_cue(FarmActor *self);
s32 cow_cue_14(FarmActor *self);
s32 cow_cue_17(FarmActor *self);
void livestock_sprite_refresh_b(FarmActor *object);
void livestock_pair_at_38_b(FarmLivestock *obj);
FarmLivestock *livestock_spawn_b(s32 slot, s32 unused1, s32 unused2);
FarmLivestock *livestock_copy_placement_b(s32 source, s32 destination);
s32 livestock_action_timer_b(FarmLivestock *obj);
s32 livestock_overnight_update_alt(FarmLivestock *animal);
s32 livestock_dispatch_tick_b(FarmLivestock *object);
s32 livestock_action_dispatch_b(FarmLivestock *action, s32 state);
s32 sheep_idle_cue(FarmActor *self);
s32 sheep_sleep_cue(FarmActor *self);
s32 sheep_scold_cue(FarmActor *self);
s32 sheep_happy_cue(FarmActor *self);
s32 sheep_cue_14(FarmActor *self);
void livestock_frame_refresh(FarmActor *object);


#endif /* LIVESTOCK_H */

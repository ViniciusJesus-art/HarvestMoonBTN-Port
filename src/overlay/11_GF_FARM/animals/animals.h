#ifndef ANIMALS_H
#define ANIMALS_H

/* The farm animal tables: the daily wild visitors, placement and the walkers. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "actor.h"
#include "anim.h"
#include "animal_actions.h"
#include "chicken.h"
#include "cue.h"
#include "dog.h"
#include "livestock.h"
#include "pond.h"

struct FarmActionSlot;
struct FarmHelper;

#define MAX_CHICKENS    10
#define EXTRA_CHICKENS  3
#define MAX_LIVESTOCK   20
/* The chicken and livestock slots there are before the coop and the barn are built. */
#define CHICKENS_NO_COOP 5
#define LIVESTOCK_NO_BARN 10
#define FARM_ACTIONS    6
#define FARM_MODES      4

/* The two extra row spots, passed to place_row_spot as a slot: the mode slot 0 and slot 1
 * animals. */
enum FarmRowSpotSlot {
    ROW_SPOT_MODE_SLOT_0 = -1,
    ROW_SPOT_MODE_SLOT_1 = -2,
};

/* The farm_walker_move flags of a walk: the walker keeps off the player's cell, or (the
 * approach to the player) leaves that cell open so it can reach him. */
enum FarmWalkFlags {
    FARM_WALK_AROUND_PLAYER = 0x8031,
    FARM_WALK_TO_PLAYER = 0x8030,
};

/* The affection from which a livestock reads as fond, and as devoted. */
#define LIVESTOCK_FOND_AFFECTION    9
#define LIVESTOCK_DEVOTED_AFFECTION 201

/* The message farm_message_request opens for a livestock animal. */
enum FarmLivestockMessage {
    LIVESTOCK_MSG_SICK_RECENT = 0x6C0,      /* sick for 1 to 3 days */
    LIVESTOCK_MSG_SICK_LONG = 0x6C1,        /* sick for 4 days or more */
    LIVESTOCK_MSG_PREGNANT = 0x6C2,         /* pregnant, days left to go */
    LIVESTOCK_MSG_PREGNANT_START = 0x6C3,   /* pregnant for one day */
    LIVESTOCK_MSG_PREGNANT_OVERDUE = 0x6C4, /* pregnant past the gestation */
    LIVESTOCK_MSG_HEALTHY = 0x6BE,          /* healthy, affection below 9 */
    LIVESTOCK_MSG_FOND = 0x6BD,             /* affection 9 to 200 */
    LIVESTOCK_MSG_DEVOTED = 0x6BF,          /* affection 201 or more */
};

/* What farm_mode_slot_claim returns: 0 is a claim, the others say why it failed. */
enum FarmClaimResult {
    FARM_CLAIM_OK = 0,
    FARM_CLAIM_SLOT_TAKEN = 0x1E6,      /* the slot is unavailable or already holds an item */
    FARM_CLAIM_NO_ITEM = 0x1A1,         /* no item is free to take */
};

/* One slot of the mode table. */
typedef struct FarmModeSlot {
    s16 value;          /* -1 is the empty sentinel */
    s16 animal;         /* the livestock record the slot holds, -1 none */
} FarmModeSlot;

/* The animals and the other actors of the farm. */
typedef struct FarmAnimals {
    FarmChicken chickens[MAX_CHICKENS];
    FarmLivestock livestock[MAX_LIVESTOCK];
    FarmPond pond;
    u8  pad16F6[0x16F8 - 0x16F6];
    FarmPet dog;
    FarmPet horse;
    FarmAction actions[FARM_ACTIONS];
    FarmChicken extraChickens[EXTRA_CHICKENS];
    FarmModeSlot slots[FARM_MODES];         /* one per mode, 0..3 */
    u16 spawnGate;
    s16 moveSpeeds[5];
    s16 fishCount;          /* the pond's live fish */
    s16 pendingTrack;
} FarmAnimals;

void *farm_action_create(s32 kind, s32 slot);
void farm_animals_tick(void);
void farm_reset_tables(void);
void farm_cleanup_records(void);
void farm_end_of_day(void);
s32 farm_action_begin(struct FarmActionSlot *self);
s32 farm_place_held(s16 kind, void *position, void *block);
void farm_actor_release_held(FarmActor *obj);
void farm_mode_check_start(s16 handle);
void farm_horse_place(FarmPet *horse, u16 *cell, VECTOR *position);
void farm_actions_clear(s32 force);
void farm_spawn_daily_visitors(void);
void farm_create_object(VECTOR pos, s32 mode);
s32 farm_kind_selected(s32 mode, u32 kind);
void farm_clear_flagged_items(void);
void farm_spawn_chicken_sources(void);
void farm_draw_actor(FarmActor *object);
void farm_draw_object(struct FarmHelper *helper);
void farm_request_project(FarmActor *request, VramPlace *places, s32 base);
void farm_vec3_accumulate(VECTOR *a, VECTOR *b, s32 clear);
void farm_vector_adjust(s32 mode, VECTOR *result, s32 amount);
void farm_cue_praise(FarmActor *p);
s32 farm_cue_step_run(FarmActor *self, FarmCueStep *table, s16 *step, s16 offset, s16 *timer);
void farm_set_destination(FarmActor *actor, VECTOR v);
void farm_mover_set_facing(FarmActor *mover);
void farm_place_spot(void *spot, s16 index, s32 mode);
void place_row_spot(FarmActor *object, s16 slot);
void place_grid_spot(FarmActor *object, s16 slot);
void farm_items_apply_kind(s32 mode);
FarmModeSlot *farm_mode_slot_find(s32 mode);
void farm_mode_slot_respawn(s32 arg);
s32 farm_mode_available(s32 mode);
FarmLivestock *livestock_find_free(void);
FarmChicken *farm_chicken_find_free(void);
s32 livestock_index(FarmLivestock *animal);
void farm_mode_slot_set(FarmModeSlot *slot, s16 value, s16 animal);
void animal_slot_clear(s16 *pair);
void farm_message_request(FarmLivestock *state);
void farm_feed_bin_set(s32 slot, s32 sel);
s32 farm_feed_bin_full(s32 index, s32 barn);
s32 farm_mode_slot_claim(s32 mode, s32 value);
void farm_put_back_blocked_animals(void);
void farm_request_track(u16 value);
void farm_walker_move(FarmActor *walker, s32 speed, s32 flags);
void farm_set_proximity(s16 threshold);

#endif /* ANIMALS_H */

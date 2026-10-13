/* Chickens on the farm: their spawn, daily tick, state and timers, their sprites and the
 * animals' sprite table, their next state, following the player, their cue scripts and
 * their walking. */
#include <memory.h>
#include "types.h"
#include "actor.h"
#include "anim.h"
#include "animals.h"
#include "chicken.h"
#include "clock.h"
#include "cue.h"
#include "estate.h"
#include "farm.h"
#include "farm_data.h"
#include "farm_shared.h"
#include "farm_state.h"
#include "items.h"
#include "scene.h"
#include "screen_fade.h"
#include "sound_request.h"

static s32 farm_position_normalize(VECTOR *source, VECTOR *result);
static s32 chicken_init(FarmChicken *chicken, s32 kind);
static s32 chicken_load_sprite(FarmChicken *chicken);

void chicken_pair_at_38(FarmChicken *obj) {
    farm_pair_at_38(&obj->actor);
}

/* Add a chick or chicken of `kind` and return its slot, or 0 if there is none. Mode 1 puts it
 * in the coop's slots (`requested`, or the first free one when -1) and places it there; any
 * other mode uses the first free of the three extra chicken slots. */
FarmChicken *chicken_spawn(s32 kind, s32 requested, s32 mode) {
    s32 slot_kind;
    FarmChicken *slot;
    s32 index;
    FarmChicken *scan;
    FarmChicken *entry;
    FarmChicken *next;
    index = 0;
    if ((kind - ITEM_CHICKEN) >= 2U) {
        return 0;
    }
    if (mode == 1) {
        if (requested != (-1)) {
            slot = &game_state.animals.chickens[requested];
        } else {
            slot = farm_chicken_find_free();
        }
        if (slot != 0) {
            slot_kind = (index = chicken_index(slot));
            chicken_init(slot, kind);
            farm_place_spot(slot, slot_kind, 0);
            return slot;
        }
        return 0;
    }
    entry = game_state.animals.extraChickens;
    if (entry->actor.kind != ITEM_NOTHING) {
        scan = entry;
        next = entry;
        do {
            scan++;
            index++;
            next++;
            if (index >= 3) {
                break;
            }
            entry = next;
        } while (scan->actor.kind != ITEM_NOTHING);
    }
    if (index != 3) {
        chicken_init(entry, kind);
        return entry;
    }
    return 0;
}

/* Count the coop chicken slots that hold an animal. */
static s32 farm_chicken_count(void) {
    s32 count = 0;
    FarmChicken *p = game_state.animals.chickens;
    s32 n = 9;

    do {
        if (p->actor.kind != ITEM_NOTHING) count++;
        n--;
        p++;
    } while (n >= 0);
    return count;
}

/* On the farm outside winter, a chicken counts a grazing day while the world weather differs
 * from its place (PLACE_FARM); five of those make it fed today. Otherwise it loses 5
 * affection. Outside the day hours its interrupt time is set to 300 and its cue stops. */
s32 chicken_weather_tick(FarmChicken *object) {
    if (object->actor.kind == ITEM_NOTHING) {
        return -1;
    }
    if (object->actor.kind != ITEM_CHICKEN) {
        return 0;
    }
    if (object->actor.place.location == PLACE_FARM && game_state.progress.clock.date.season != SEASON_WINTER) {
        if (game_state.world.weather != object->actor.place.location) {
            if (++object->grazingTime >= 5) {
                object->actor.flags |= ACTOR_FLAG_FED_TODAY;
            }
        } else {
            object->actor.affection -= 5;
            object->actor.affection = (object->actor.affection < 0) ? 0 : object->actor.affection;
        }
    }
    if ((u16)(game_state.progress.clock.time.hm.hour - FARM_DAY_HOUR_FIRST) >= FARM_DAY_HOUR_COUNT) {
        object->actor.interruptTime = 300;
        object->actor.flags &= ~ACTOR_FLAG_CUE_RUNNING;
    }
    return 0;
}

/* A chicken's day. Free slots return -1. Eggs and unclaimed chicks age; a claimed chick is
 * left alone. In the coop a chicken eats from the first filled feed bin: fed gives +10
 * affection (up to 255) and a marker the next day, unfed gives -20 (down to 0). A sick
 * chicken worsens daily and may die; one with no affection may fall sick (11% a day). An
 * adult aged 3, 4 or 5+ years may die of age (25, 75 or 100%); contest entrants never do. */
s32 chicken_daily_tick(FarmChicken *chicken) {
    VECTOR position;
    s32 index;
    s32 cue;
    s32 chance;
    s32 i;

    if (chicken->actor.kind == ITEM_NOTHING && !(chicken->actor.flags & ACTOR_FLAG_CLAIMED)) {
        return -1;
    }
    if (chicken->actor.kind != ITEM_CHICKEN) {
        if (chicken->actor.kind == ITEM_CHICK && (chicken->actor.flags & ACTOR_FLAG_CLAIMED)) {
            return 0;
        }
        chicken->pregnancy++;
        chicken_load_sprite(chicken);
        chicken_place(chicken);
        return 0;
    }
    index = chicken_index(chicken);
    if (chicken->actor.place.location == PLACE_4 && !(chicken->actor.flags & ACTOR_FLAG_FED_TODAY)) {
        for (i = 0; i < COOP_FEED_BINS; i++) {
            if (game_state.progress.estate.coopFeedBins[i] != 0) {
                chicken->actor.flags |= ACTOR_FLAG_FED_TODAY;
                game_state.progress.estate.coopFeedBins[i] = 0;
                break;
            }
        }
    }
    chicken_place(chicken);
    if (chicken->actor.flags & ACTOR_FLAG_FED_YESTERDAY) {
        if (chicken->actor.affection < 100) {
            cue = 0x122;
        } else if (chicken->actor.affection < 200) {
            cue = 0x123;
        } else if (chicken->actor.flags & ACTOR_FLAG_PRIZE_WINNER) {
            cue = 0xE8;
        } else {
            cue = 0x124;
        }
        if (chicken->actor.place.location == PLACE_4) {
            farm_position_normalize(&chicken->actor.position, &position);
        } else {
            position = chicken->actor.position;
        }
        estate_marker_stamp(cue, position, chicken->actor.place.location, index);
    }
    if (chicken->actor.flags & ACTOR_FLAG_FED_TODAY) {
        chicken->actor.affection += 10;
        chicken->actor.affection = chicken->actor.affection > 255 ? 255 : chicken->actor.affection;
        chicken->actor.flags |= ACTOR_FLAG_FED_YESTERDAY;
    } else {
        chicken->actor.affection -= 20;
        chicken->actor.affection = chicken->actor.affection < 0 ? 0 : chicken->actor.affection;
        chicken->actor.flags &= ~ACTOR_FLAG_FED_YESTERDAY;
    }
    if (!(chicken->actor.flags & ACTOR_FLAG_DIED)) {
        if (chicken->daysSick != 0) {
            chicken->daysSick++;
            chicken->daysSick = chicken->daysSick >= 32 ? 31 : chicken->daysSick;
            if (chicken->daysSick >= 3 && !(chicken->actor.flags & ACTOR_FLAG_CONTEST_ENTRANT) &&
                rand_range(100) <= chicken->daysSick * 10) {
                chicken->actor.flags |= ACTOR_FLAG_DIED;
            }
        } else if (chicken->actor.affection <= 0 && (u32)rand_range(100) < 11) {
            chicken->daysSick++;
        }
    }
    if (!(chicken->actor.flags & ACTOR_FLAG_DIED)) {
        if (!(chicken->actor.flags & ACTOR_FLAG_CONTEST_ENTRANT)) {
            if (chicken->actor.date.year < 3) {
                chance = 0;
            } else if (chicken->actor.date.year == 3) {
                chance = 25;
            } else if (chicken->actor.date.year == 4) {
                chance = 75;
            } else {
                chance = 100;
            }
            if (chance != 0 && (u32)rand_range(100) <= chance) {
                chicken->actor.flags |= (ACTOR_FLAG_DIED | ACTOR_FLAG_DIED_OF_AGE);
            }
        }
    }
    chicken->pregnancy++;
    chicken->pregnancy = chicken->pregnancy >= 32 ? 31 : chicken->pregnancy;
    chicken_load_sprite(chicken);
    if (chicken->actor.date.day == game_state.progress.clock.date.day && chicken->actor.date.season == game_state.progress.clock.date.season) {
        chicken->actor.date.year++;
    }
    if (!(chicken->actor.flags & ACTOR_FLAG_FED_TODAY)) {
        game_state.progress.estate.estateFlags |= ESTATE_FLAG_ANIMAL_AGED;
    }
    farm_actor_release_held(&chicken->actor);
    chicken->grazingTime = 0;
    chicken->actor.flags &= ~ACTOR_FLAG_FED_TODAY;
    chicken->actor.flags &= ~ACTOR_FLAG_APPROACHING_PLAYER;
    cue_reset(&chicken->actor.cue);
    return 0;
}

/* The horizontal range a chicken's position is kept to; outside it a new one is rolled. */
#define CHICKEN_X_MIN (-1000)
#define CHICKEN_X_MAX 1200

/* Copy a farm position to `result`. An x outside CHICKEN_X_MIN..CHICKEN_X_MAX and a z outside
 * its band are rolled again at random; returns 1 when either was. */
static s32 farm_position_normalize(VECTOR *source, VECTOR *result) {
    s32 value;
    s32 random_value;
    s32 lower;
    s32 upper;
    s32 replacement;
    s32 changed;

    result->vy = source->vy;
    value = source->vx;
    changed = 0;
    if ((u32)(value - CHICKEN_X_MIN) > (CHICKEN_X_MAX - CHICKEN_X_MIN)) {
        result->vx = rand_range(CHICKEN_X_MAX - CHICKEN_X_MIN) + CHICKEN_X_MIN;
        changed = 1;
    } else {
        result->vx = value;
    }
    random_value = source->vz;
    replacement = 1700;
    if (random_value >= 0) {
        lower = 400;
        upper = 1300;
    } else {
        replacement = -500;
        lower = -2000;
        upper = 1500;
    }
    if (replacement < random_value || random_value < lower) {
        result->vz = lower + rand_range(upper);
        changed = 1;
    } else {
        result->vz = random_value;
    }
    return changed;
}

/* The kind a chicken's age (its pregnancy counter) makes: an egg under 3, a chick under 10,
 * otherwise a chicken. A free slot gives -1. */
static s32 chicken_kind_by_age(FarmChicken *chicken) {
    u16 age;
    if (chicken->actor.kind == ITEM_NOTHING && (chicken->actor.flags & ACTOR_FLAG_CLAIMED) == 0) return -1;
    age = chicken->pregnancy;
    if (age < 3) return ITEM_EGG;
    if (age < 10) {
        return ITEM_CHICK;
    } else {
        return ITEM_CHICKEN;
    }
}

/* Run the cue handler for a farm object's state, loading its sprite if it has none. Once no
 * cue is running and the interrupt timer has reached 300, pick its next state; then the
 * timer advances by one. */
s32 chicken_timer_tick(FarmChicken *object) {
    s32 (*callback)(FarmActor *);

    if (object->actor.kind == ITEM_NOTHING) {
        return -1;
    }
    if (object->actor.sprite.source == 0) {
        chicken_load_sprite(object);
    }
    callback = farm_actor_cue_handlers[(u16)object->actor.taskNo];
    if (callback != 0) {
        callback(&object->actor);
    }
    if ((object->actor.flags & ACTOR_FLAG_CUE_RUNNING) == 0 && object->actor.interruptTime >= 300) {
        chicken_pick_next_state(object);
        object->actor.interruptTime = 0;
    }
    object->actor.interruptTime++;
    return 0;
}

static s32 (*const farm_actor_enter_handlers[20])() = {
    0, 0, 0, farm_chicken_follow_player, farm_chicken_tick_offset, 0, 0, 0, 0, farm_chicken_clear_sick, 0, 0, 0, 0,
    farm_chicken_action_none, 0, 0, 0, 0, 0,
};

s32 (*const farm_actor_cue_handlers[20])(FarmActor *) = {
    farm_chicken_cue_run, chicken_cue_1, chicken_cue_2, chicken_cue_walk, chicken_cue_walk_quick, 0, 0, 0, 0,
    0, 0, 0, 0, 0, chicken_cue_sound, chicken_cue_1, chicken_cue_2, 0, chicken_walk_state,
    chicken_sick_cue,
};

/* Run the handler for `state` on a farm object, and start that state's cue when it changes. */
s32 farm_actor_dispatch_state(FarmActor *object, s32 state) {
    s32 result;

    result = 0;
    if (farm_actor_enter_handlers[state] != 0) {
        if (object->taskNo == state && (object->flags & ACTOR_FLAG_CUE_RUNNING)) {
            result = -1;
        } else {
            result = farm_actor_enter_handlers[state](object);
        }
    }
    if (farm_actor_cue_handlers[state] != 0 && result != -1) {
        object->taskNo = state;
        object->taskId = 0;
        object->taskTime = 0;
        object->interruptTime = 0;
        farm_vector_adjust(4, &object->speed, 0);
    }
    return result;
}

void chicken_clear(FarmChicken *p) {
    memset(p, 0, 0xB4);
    p->actor.kind = 0;
}

/* The slot index of `p` in game_state.animals.chickens, or -1 if it is not one of them. */
s32 chicken_index(FarmChicken *p) {
    s32 i = 0;
    FarmChicken *q = game_state.animals.chickens;
    while (i < MAX_CHICKENS) {
        if (p == q) return i;
        i++;
        q++;
    }
    return -1;
}

/* Set up a new chick or chicken (`kind`) in a free slot: ten affection, born
 * today, no name. */
static s32 chicken_init(FarmChicken *chicken, s32 kind) {
    if (chicken->actor.kind != ITEM_NOTHING) return -1;
    memset(chicken, 0, sizeof(FarmChicken));
    switch (kind) {
    case ITEM_CHICK:
        chicken->pregnancy = 3;
        break;
    case ITEM_CHICKEN:
        chicken->pregnancy = 10;
        break;
    }
    chicken->actor.kind = kind;
    chicken->actor.affection = 10;
    chicken->actor.date = game_state.progress.clock.date;
    chicken->actor.date.year = 0;
    chicken->actor.name[0] = 0xFFFF;
    chicken_load_sprite(chicken);
    cue_reset(&chicken->actor.cue);
    return 0;
}

/* Load the sprite file for the kind a chicken's age makes (chicken_kind_by_age) and set that
 * kind; an egg changes nothing. */
static s32 chicken_load_sprite(FarmChicken *chicken) {
    s32 kind;
    s32 slot;
    AnimPackToc *toc;

    kind = chicken_kind_by_age(chicken);
    slot = 1;
    switch (kind) {
    case ITEM_CHICK:
        chicken->actor.kind = kind;
        break;
    case ITEM_CHICKEN:
        chicken->actor.kind = kind;
        slot = 2;
        break;
    default:
        return -1;
    }
    toc = farm_slot_tocs[6];
    anim_bind_file(&chicken->actor.sprite, (AnimFile *)(farm_area.slotSixArt + ANIM_PACK_START(toc, slot - 1)));
    return 0;
}

typedef struct {
    u16 first;
    u16 second;
} FarmAnimTableEntry;

static FarmAnimTableEntry farm_anim_table[7][4] = {
    {{0, 0}, {0, 1}, {0, 2}, {0, 3}},
    {{1, 0}, {2, 0}, {3, 0}, {4, 0}},
    {{5, 0}, {6, 0}, {7, 0}, {8, 0}},
    {{9, 0}, {10, 0}, {11, 0}, {12, 0}},
    {{13, 0}, {14, 0}, {15, 0}, {16, 0}},
    {{17, 0}, {17, 1}, {17, 2}, {17, 3}},
    {{18, 0}, {18, 1}, {18, 2}, {18, 3}},
};

s32 farm_anim_lookup(u16 row, u16 column, u16 *first, u16 *second) {
    *first = farm_anim_table[row][column].first;
    if (second != 0) {
        *second = farm_anim_table[row][column].second;
    }
    return 0;
}

/* How far a chicken moves along the player's facing in farm_chicken_tick_offset. */
#define CHICKEN_HOP_DISTANCE 2000

/* The chicken states (farm actor taskNo) the chicken code names: a cluck, a walk and the
 * sick cue. */
enum FarmChickenState {
    CHICKEN_STATE_CLUCK = 0x0E,
    CHICKEN_STATE_WALK = 0x12,
    CHICKEN_STATE_SICK = 0x13,
};

/* Pick a chicken's next state: a sick adult takes CHICKEN_STATE_SICK; outside the day hours
 * it takes 0xF, and 0xF is followed by 0x10; otherwise a random roll (wider when it is
 * approaching the player) gives CHICKEN_STATE_CLUCK, 0xF or CHICKEN_STATE_WALK. Switch to
 * it if it differs from the current one. */
s32 chicken_pick_next_state(FarmChicken *object) {
    s32 next;

    if (object->daysSick != 0 && object->actor.kind == ITEM_CHICKEN) {
        next = CHICKEN_STATE_SICK;
    } else if ((u32)((u16)game_state.progress.clock.time.hm.hour - FARM_DAY_HOUR_FIRST) >= FARM_DAY_HOUR_COUNT) {
        next = 0xF;
    } else if (object->actor.taskNo == 0xF) {
        next = 0x10;
    } else {
        switch (rand_range((object->actor.flags & ACTOR_FLAG_APPROACHING_PLAYER) ? 9 : 4)) {
        case 1:
            next = CHICKEN_STATE_CLUCK;
            break;
        case 2:
            next = 0xF;
            break;
        case 0:
        default:
            next = CHICKEN_STATE_WALK;
            break;
        }
    }
    if (next != object->actor.taskNo) {
        farm_actor_dispatch_state(&object->actor, next);
    }
    return 0;
}

/* Place a chicken that is neither in the coop nor on the farm. */
void chicken_place(FarmChicken *chicken) {
    s32 i;
    for (i = 0; i < MAX_CHICKENS; i++) {
        if (chicken->actor.place.location != PLACE_4 && chicken->actor.place.location != PLACE_FARM) {
            farm_place_spot(chicken, i, 0);
        }
    }
}

/* A chicken follows the player: it heads for the player's position, loses ten affection and
 * starts its cue, playing its startled call when the player is in its place. */
s32 farm_chicken_follow_player(FarmActor *object)
{
    if (object->kind == ITEM_NOTHING) {
        return -1;
    }
    if (object->kind != ITEM_CHICKEN) {
        return -1;
    }
    if (farm_area.mode.place.location == object->place.location) {
        sound_play(SOUND_CHICKEN_STARTLED);
    }
    object->destination.vx = game_state.player.position.vx;
    object->destination.vy = game_state.player.position.vy;
    object->destination.vz = game_state.player.position.vz;
    object->destination.pad = game_state.player.position.pad;
    object->flags = (object->flags | ACTOR_FLAG_APPROACHING_PLAYER) & ~ACTOR_FLAG_BIT14;
    object->affection -= 10;
    object->affection = (object->affection < 0) ? 0 : object->affection;
    cue_set(&object->cue, 0x24, 0x2D);
    return 0;
}

/* Move a chicken CHICKEN_HOP_DISTANCE units along the player's facing from where it stands,
 * loses ten affection and turns to face the player. */
s32 farm_chicken_tick_offset(FarmActor *object) {
    VECTOR offset;

    if (object->kind == ITEM_NOTHING) {
        return -1;
    }
    if (object->kind != ITEM_CHICKEN) {
        return -1;
    }
    if (farm_area.mode.place.location == object->place.location) {
        sound_play(SOUND_CHICKEN_STARTLED);
    }
    object->affection -= 10;
    object->affection = (object->affection < 0) ? 0 : object->affection;
    farm_vector_adjust(game_state.player.facing38, &offset, CHICKEN_HOP_DISTANCE);
    offset.vx += object->position.vx;
    offset.vy += object->position.vy;
    offset.vz += object->position.vz;
    farm_set_destination(object, offset);
    object->facing = game_state.player.facing38;
    cue_set(&object->cue, 0x24, 0x2D);
    return 0;
}

/* Cure a sick chicken and give it the praise cue. */
s32 farm_chicken_clear_sick(FarmChicken *chicken) {
    if (chicken->actor.kind == ITEM_NOTHING) {
        return -1;
    }
    if (chicken->actor.kind != ITEM_CHICKEN) {
        return -1;
    }
    if (chicken->daysSick == 0) {
        return -1;
    }
    chicken->daysSick = 0;
    farm_cue_praise(&chicken->actor);
    return 0;
}

s32 farm_chicken_action_none(void) {
    return 0;
}

FARM_CUE_RUNNER(farm_chicken_cue_run, {0, 30}, {-1, 0})

static FarmCueStep chicken_cue_steps_adult[4] = {
    {0x00, 0x0A}, {0x05, 0x0A}, {0x02, 0x00}, {-1, 0x00},
};

static FarmCueStep chicken_cue_steps_chick[3] = {
    {0x00, 0x0A}, {0x02, 0x00}, {-1, 0x00},
};

/* Run the chick's or the adult's step table, then stop. */
s32 chicken_cue_1(FarmActor *self) {
    s16 step;
    s32 finished;
    FarmCueStep *table;

    step = self->taskId;
    finished = 0;
    switch (step) {
    case CUE_STEP_START:
        self->taskId = CUE_STEP_PLAY;
        self->flags |= ACTOR_FLAG_CUE_RUNNING;
        break;
    case CUE_STEP_STOP:
        finished = 1;
        self->flags &= ~ACTOR_FLAG_CUE_RUNNING;
        break;
    case CUE_STEP_PLAY:
    default:
        if (self->kind == ITEM_CHICK) {
            table = chicken_cue_steps_chick;
        } else {
            table = chicken_cue_steps_adult;
        }
        if (farm_cue_step_run(self, table, &self->taskId, -1, &self->taskTime) != 0) {
            self->taskId = CUE_STEP_STOP;
        }
        break;
    }
    return finished;
}

/* The taskId chicken_cue_2 moves to once its table runs out (not CUE_STEP_STOP). */
#define CHICKEN_CUE_2_DONE 4

static FarmCueStep chicken_cue_steps_adult_2[3] = {
    {0x05, 0x0A}, {0x00, 0x0A}, {-1, 0x00},
};

static FarmCueStep chicken_cue_steps_chick_2[2] = {
    {0x00, 0x0A}, {-1, 0x00},
};

s32 chicken_cue_2(FarmActor *self) {
    s16 step;
    s32 finished;
    FarmCueStep *table;

    step = self->taskId;
    finished = 0;
    switch (step) {
    case CUE_STEP_START:
        self->taskId = CUE_STEP_PLAY;
        self->flags |= ACTOR_FLAG_CUE_RUNNING;
        break;
    case CHICKEN_CUE_2_DONE:
        finished = 1;
        self->interruptTime = 300;
        self->flags &= ~ACTOR_FLAG_CUE_RUNNING;
        break;
    case CUE_STEP_PLAY:
    default:
        if (self->kind == ITEM_CHICK) {
            table = chicken_cue_steps_chick_2;
        } else {
            table = chicken_cue_steps_adult_2;
        }
        if (farm_cue_step_run(self, table, &self->taskId, -1, &self->taskTime) != 0) {
            self->taskId = CHICKEN_CUE_2_DONE;
        }
        break;
    }
    return finished;
}

/* Walk the chicken through its steps at walk speed, then stop. */
s32 chicken_cue_walk(FarmActor *self) {
    static FarmCueStep steps[3] = {{4, 60}, {1, 0}, {-1, 0}};
    s16 step;
    s16 saved_parameter;
    s32 finished;

    step = self->taskId;
    finished = 0;
    switch (step) {
    case CUE_STEP_START:
        self->taskId = CUE_STEP_PLAY;
        self->interruptTime = 0;
        self->flags = (self->flags | ACTOR_FLAG_CUE_RUNNING) & ~ACTOR_FLAG_APPROACHING_PLAYER;
        farm_walker_move(self, 18, 0x31);
        break;
    case CUE_STEP_STOP:
        finished = 1;
        self->interruptTime = 300;
        self->flags &= ~ACTOR_FLAG_BIT14;
        self->flags &= ~ACTOR_FLAG_CUE_RUNNING;
        break;
    case CUE_STEP_PLAY:
    default:
        if (farm_cue_step_run(self, steps, &self->taskId, -1, &self->interruptTime) != 0) {
            self->taskId = CUE_STEP_STOP;
        } else {
            saved_parameter = self->taskTime;
            farm_walker_move(self, 18, 0x31);
            self->taskTime = saved_parameter;
        }
        break;
    }
    return finished;
}

/* As chicken_cue_walk, with a shorter first step; stopping also clears the approach flag. */
s32 chicken_cue_walk_quick(FarmActor *self) {
    static FarmCueStep steps[3] = {{4, 30}, {0, 0}, {-1, 0}};
    s16 step;
    s16 saved_parameter;
    s32 finished;

    step = self->taskId;
    finished = 0;
    switch (step) {
    case CUE_STEP_START:
        self->taskId = CUE_STEP_PLAY;
        self->flags |= ACTOR_FLAG_CUE_RUNNING;
        break;
    case CUE_STEP_STOP:
        finished = 1;
        self->interruptTime = 300;
        self->flags &= ~ACTOR_FLAG_CUE_RUNNING;
        self->flags &= ~ACTOR_FLAG_APPROACHING_PLAYER;
        break;
    case CUE_STEP_PLAY:
    default:
        if (farm_cue_step_run(self, steps, &self->taskId, -1, &self->taskTime) != 0) {
            self->taskId = CUE_STEP_STOP;
        } else {
            saved_parameter = self->taskTime;
            farm_walker_move(self, 18, 0x31);
            self->taskTime = saved_parameter;
        }
        break;
    }
    return finished;
}

/* When the cue starts, a hen calls (or a chick peeps) if the player is in its place; then it
 * runs out its steps. */
s32 chicken_cue_sound(FarmActor *self) {
    static FarmCueStep steps[2] = {{3, 0}, {-1, 0}};
    s16 step;
    s32 finished;

    step = self->taskId;
    finished = 0;
    switch (step) {
    case CUE_STEP_START:
        if (farm_area.mode.place.location == self->place.location) {
            if (self->kind == ITEM_CHICKEN) {
                sound_play(SOUND_CHICKEN_VOICE);
            } else if (self->kind == ITEM_CHICK) {
                sound_play(SOUND_CHICK_VOICE);
            }
        }
        self->taskId = CUE_STEP_PLAY;
        self->flags |= ACTOR_FLAG_CUE_RUNNING;
        break;
    case CUE_STEP_STOP:
        finished = 1;
        self->flags &= ~ACTOR_FLAG_CUE_RUNNING;
        break;
    case CUE_STEP_PLAY:
    default:
        if (farm_cue_step_run(self, steps, &self->taskId, -1, &self->taskTime) != 0) {
            self->taskId = CUE_STEP_STOP;
        }
        break;
    }
    return finished;
}

/* Walk one step, then set the action to rest or moving by its speed and clear the cue flag. */
s32 chicken_walk_state(FarmActor *object) {
    farm_walker_move(object, 6, 0x31);
    if (object->speed.vx == 0 && object->speed.vy == 0 && object->speed.vz == 0)
        object->action = ACTOR_ACTION_REST;
    else
        object->action = ACTOR_ACTION_MOVING;
    object->flags &= ~ACTOR_FLAG_CUE_RUNNING;
    return 1;
}

FARM_CUE_RUNNER(chicken_sick_cue, {6, 30}, {-1, 0})

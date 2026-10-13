/* Livestock on the farm: the overnight update of each cow and sheep, their cue scripts and their walking. */
#include <memory.h>
#include <sys/types.h>
#include "types.h"
#include "actor.h"
#include "anim.h"
#include "animals.h"
#include "cd_audio.h"
#include "chicken.h"
#include "clock.h"
#include "cue.h"
#include "estate.h"
#include "farm.h"
#include "farm_data.h"
#include "farm_shared.h"
#include "farm_state.h"
#include "field.h"
#include "item_storage.h"
#include "items.h"
#include "livestock.h"
#include "picture_cells.h"
#include "player.h"
#include "scene.h"
#include "screen_fade.h"

typedef void (*DefHandler)(FarmLivestock *);

/* The taskId of a cow or sheep wake cue once its table runs out (not CUE_STEP_STOP). */
#define LIVESTOCK_WAKE_DONE 4

/* The grazing count at which an animal is fed: five or more, with a ready crop near. */
#define LIVESTOCK_FED_GRAZE 5

/* Days from one shearing to the next wool cue. */
#define LIVESTOCK_SHEAR_DAYS 7

/* The growth days at which a calf becomes a young cow, and a lamb a sheep. */
#define LIVESTOCK_YOUNG_DAYS 14

/* The growth days at which a young cow becomes a cow. */
#define LIVESTOCK_ADULT_DAYS 28

/* How far behind its mother a newborn is placed, along z. */
#define LIVESTOCK_OFFSPRING_Z_OFFSET 200

/* The most days a cow or sheep stays sick, and the most growth days counted. */
#define LIVESTOCK_MAX_DAYS_SICK 31
#define LIVESTOCK_MAX_GROWTH_DAYS 31

/* The most affection an animal keeps. */
#define LIVESTOCK_MAX_AFFECTION 255

/* The percent chance each day that an animal at zero affection falls sick. */
#define LIVESTOCK_SICK_CHANCE 11

/* A sick animal may die from its third sick day on, at the sick-day count times this percent. */
#define LIVESTOCK_SICK_DEATH_FROM 3
#define LIVESTOCK_SICK_DEATH_PER_DAY 10

/* The year of age in which a cow or sheep may die of old age: the percent chance in that
 * year, a higher one the year after, and certain death from the year after that. */
#define LIVESTOCK_COW_OLD_YEAR 8
#define LIVESTOCK_SHEEP_OLD_YEAR 3
#define LIVESTOCK_OLD_CHANCE_FIRST 26
#define LIVESTOCK_OLD_CHANCE_SECOND 76

/* The affection an animal gains or loses in a day. */
#define LIVESTOCK_TALK_AFFECTION 3
#define LIVESTOCK_FED_AFFECTION 1
#define LIVESTOCK_GRAZE_AFFECTION 1
#define LIVESTOCK_UNFED_AFFECTION 10
#define LIVESTOCK_BAD_WEATHER_AFFECTION 5
#define LIVESTOCK_NEGLECT_AFFECTION 10

/* The sprite scale of a young cow. */
#define LIVESTOCK_YOUNG_SCALE 0xCC8

/* Walk speeds: a cow or sheep walks at 6, a pregnant cow at 2, and a neglected one at 18. */
#define LIVESTOCK_WALK_SPEED 6
#define LIVESTOCK_WALK_SPEED_PREGNANT 2
#define LIVESTOCK_WALK_SPEED_NEGLECT 18

/* The sprite packs of a cow or sheep, as the resource number: the sprite loads pack resource - 1. */
enum LivestockPack {
    LIVESTOCK_PACK_CALF = 1,
    LIVESTOCK_PACK_COW = 2,             /* a young cow uses the cow pack too */
    LIVESTOCK_PACK_PREGNANT_COW = 4,
    LIVESTOCK_PACK_LAMB = 5,
    LIVESTOCK_PACK_SHEEP = 6,
    LIVESTOCK_PACK_PREGNANT_SHEEP = 8,
    LIVESTOCK_PACK_SHORN_SHEEP = 9,     /* shorn within LIVESTOCK_SHEAR_DAYS: the wool has not grown back */
};

typedef struct {
    u16 value;
    u16 frame;
} FrameEntry;

typedef struct {
    FrameEntry entry[4];
} FrameRow;

typedef struct {
    u16 x;
    u16 y;
} LivestockFrameEntry;

static s32 livestock_record_init(FarmLivestock *item, s32 kind);
static s32 livestock_load_mode_sprite(FarmLivestock *object);
static s32 livestock_frame_lookup(u32 group, u32 index, u16 *out_value, u16 *out_frame);
static s32 livestock_pick_state(FarmLivestock *animal);
static s32 livestock_is_live(FarmActor *animal);
static s32 livestock_start_neglect_cue(FarmActor *obj);
static s32 livestock_start_scold_cue(FarmActor *obj);
static s32 livestock_cure_sickness(FarmLivestock *obj);
static s32 livestock_set_bit2_cue(FarmActor *actor);
static s32 livestock_start_pregnancy(FarmLivestock *object);
static s32 livestock_cow_cue(FarmLivestock *object);
static s32 livestock_talk(FarmLivestock *animal);
static s32 livestock_approach_player_chance(FarmLivestock *request);
static s32 livestock_approach_player(FarmLivestock *animal);
static s32 livestock_position_update(FarmActor *obj);
static s32 livestock_action_none(void);
static s32 cow_wake_cue(FarmActor *self);
static s32 cow_neglect_cue(FarmActor *self);
static s32 cow_cue_13(FarmLivestock *p);
static s32 livestock_walk_cue(FarmLivestock *obj);
static s32 livestock_record_init_b(FarmLivestock *item, s32 kind);
static s32 livestock_load_mode_sprite_b(FarmLivestock *arg);
static s32 livestock_frame_lookup_b(u16 row, u16 col, u16 *out_x, u16 *out_y);
static s32 livestock_pick_state_b(FarmLivestock *action);
static s32 livestock_is_live_b(FarmActor *arg);
static s32 livestock_start_neglect_cue_b(FarmActor *obj);
static s32 livestock_start_scold_cue_b(FarmActor *object);
static s32 livestock_cure_sickness_b(FarmLivestock *obj);
static s32 livestock_set_bit2_cue_b(FarmActor *obj);
static s32 livestock_start_pregnancy_b(FarmLivestock *entry);
static s32 livestock_sheep_harvest(FarmLivestock *obj);
static s32 livestock_talk_b(FarmLivestock *obj);
static s32 livestock_approach_player_chance_b(FarmLivestock *object);
static s32 livestock_approach_player_b(FarmLivestock *animal);
static s32 livestock_position_update_b(FarmActor *o);
static s32 livestock_action_none_b(void);
static s32 sheep_wake_cue(FarmActor *self);
static s32 sheep_neglect_cue(FarmActor *self);
static s32 livestock_action_check(FarmLivestock *obj);
static s32 livestock_walk_start(FarmLivestock *obj);

/* Show the animal's sprite for its current action and facing. */
void livestock_sprite_refresh(FarmActor *object) {
    u16 animation;
    u16 frame;

    livestock_frame_lookup(object->action, object->facing, &animation, &frame);
    if (animation != object->sprite.animation) {
        anim_select_range(&object->sprite, animation);
    }
    if ((frame != 0) && (frame != object->sprite.frame)) {
        anim_set_frame(&object->sprite, frame);
    }
    farm_draw_actor(object);
    if ((object->action == ACTOR_ACTION_REST) || (object->action == ACTOR_ACTION_5)) {
        object->sprite.flags = object->sprite.flags | 0x100;
    }
}

void livestock_pair_at_38(FarmLivestock *obj) {
    farm_pair_at_38(&obj->actor);
}

/* Place a new calf, young cow or cow of the given kind in a free record and dispatch it to its
 * slot; returns the record, or 0 when the kind is not a cow or no record is free. */
FarmLivestock *livestock_spawn(s32 slot, s32 unused1, s32 unused2) {
    FarmLivestock *item;
    s32 value;

    if ((slot - ITEM_CALF) < 3U) {
        item = livestock_find_free();
        if (item != 0) {
            value = livestock_index(item);
            livestock_record_init(item, slot);
            farm_place_spot(item, value, 1);
            return item;
        }
    }
    return 0;
}

/* Place a newborn calf in the destination record, a step behind its mother at source along z,
 * with the mother's place and facing. */
FarmLivestock *livestock_copy_placement(s32 source, s32 destination) {
    FarmLivestock *source_record;
    FarmLivestock *destination_record;

    destination_record = game_state.animals.livestock + destination;
    source_record = game_state.animals.livestock + source;
    livestock_record_init(destination_record, ITEM_CALF);
    destination_record->actor.position.vx = source_record->actor.position.vx;
    destination_record->actor.position.vy = source_record->actor.position.vy;
    destination_record->actor.position.vz = source_record->actor.position.vz - LIVESTOCK_OFFSPRING_Z_OFFSET;
    destination_record->actor.place.location = source_record->actor.place.location;
    destination_record->actor.facing = source_record->actor.facing;
    return destination_record;
}

/* Advance a cow's daily timer. In the pasture outside winter it grazes, and is fed once it has
 * grazed LIVESTOCK_FED_GRAZE times with a ready crop near; in bad weather it loses affection
 * instead. Outside hours 6 to 20 its timer is set to 300 and its cue stops. */
s32 livestock_action_timer(FarmLivestock *obj) {
    u32 flags;
    s32 value;
    s32 clamped;
    s16 mode;
    s32 shifted;
    if (livestock_is_live(&obj->actor) == 0) return -1;
    flags = obj->actor.flags;
    if (flags & ACTOR_FLAG_IN_EVENT) return -1;
    mode = obj->actor.place.location;
    if (mode == PLACE_FARM && game_state.progress.clock.date.season != SEASON_WINTER) {
        if (game_state.world.weather == 0) {
            value = obj->grazeCount + 1;
            obj->grazeCount = value;
            if (obj->grazeCount >= LIVESTOCK_FED_GRAZE && (flags & ACTOR_FLAG_FED_TODAY) == 0 && field_find_ready_grown(&obj->actor.position) == mode)
                obj->actor.flags |= ACTOR_FLAG_FED_TODAY;
        } else {
            value = (u16)obj->actor.affection - LIVESTOCK_BAD_WEATHER_AFFECTION;
            obj->actor.affection = value;
            shifted = value << 16;
            clamped = value;
            if (shifted < 0) clamped = 0;
            obj->actor.affection = clamped;
        }
    }
    if ((u32)((u16)game_state.progress.clock.time.hm.hour - FARM_DAY_HOUR_FIRST) >= FARM_DAY_HOUR_COUNT) {
        obj->actor.interruptTime = 300;
        obj->actor.flags &= ~ACTOR_FLAG_CUE_RUNNING;
    }
    return 0;
}

/* The overnight update of one cow: fed from the barn feed bins, affection,
 * sickness, ageing and the birthday, then the day's flags are cleared. */
s32 livestock_overnight_update(FarmLivestock *animal) {
    ItemCue *cue;
    s32 i;
    s32 value;
    s32 capped;

    if (livestock_is_live(&animal->actor) == 0) {
        return -1;
    }
    if (animal->actor.flags & ACTOR_FLAG_IN_EVENT) {
        return -1;
    }
    livestock_index(animal);
    cue = &animal->actor.cue;
    if (animal->actor.place.location == PLACE_5 && !(animal->actor.flags & ACTOR_FLAG_FED_TODAY)) {
        for (i = 0; i < BARN_FEED_BINS; i++) {
            if (game_state.progress.estate.barnFeedBins[i] != 0) {
                animal->actor.flags |= ACTOR_FLAG_FED_TODAY;
                game_state.progress.estate.barnFeedBins[i] = 0;
                break;
            }
        }
    }
    if (animal->actor.flags & ACTOR_FLAG_TALKED_TODAY) {
        animal->actor.affection += LIVESTOCK_TALK_AFFECTION;
    }
    if (animal->actor.flags & ACTOR_FLAG_FED_TODAY) {
        animal->actor.flags |= ACTOR_FLAG_FED_YESTERDAY;
        animal->actor.affection += LIVESTOCK_FED_AFFECTION;
    } else {
        animal->actor.flags &= ~ACTOR_FLAG_FED_YESTERDAY;
        animal->actor.affection -= LIVESTOCK_UNFED_AFFECTION;
    }
    if (animal->grazeCount >= LIVESTOCK_FED_GRAZE) {
        animal->actor.affection += LIVESTOCK_GRAZE_AFFECTION;
    }
    value = (u16)animal->actor.affection;
    if (animal->actor.affection < 0) {
        value = 0;
    }
    animal->actor.affection = value;
    capped = value;
    if ((s16)capped > LIVESTOCK_MAX_AFFECTION) {
        capped = LIVESTOCK_MAX_AFFECTION;
    }
    animal->actor.affection = capped;
    if (!(animal->actor.flags & ACTOR_FLAG_DIED)) {
        value = animal->daysSick;
        if (value != 0) {
            value++;
            animal->daysSick = value;
            if ((u16)value > LIVESTOCK_MAX_DAYS_SICK) {
                value = LIVESTOCK_MAX_DAYS_SICK;
            }
            animal->daysSick = value;
            if ((u16)value >= LIVESTOCK_SICK_DEATH_FROM && rand_range(100) <= animal->daysSick * LIVESTOCK_SICK_DEATH_PER_DAY) {
                animal->actor.flags |= ACTOR_FLAG_DIED;
            }
        } else if (animal->actor.affection <= 0) {
            if ((u32)rand_range(100) < LIVESTOCK_SICK_CHANCE) {
                animal->daysSick++;
            }
        }
    }
    if (animal->pregnancy != 0) {
        animal->pregnancy++;
        if (animal->pregnancy >= LIVESTOCK_GESTATION_DAYS) {
            animal->pregnancy = 0;
            animal->actor.flags |= ACTOR_FLAG_NAMING_PENDING;
        }
    }
    if (!(animal->actor.flags & ACTOR_FLAG_DIED)) {
        if (animal->actor.date.year == LIVESTOCK_COW_OLD_YEAR) {
            if ((u32)rand_range(100) < LIVESTOCK_OLD_CHANCE_FIRST) {
                animal->actor.flags |= (ACTOR_FLAG_DIED | ACTOR_FLAG_DIED_OF_AGE);
            }
        } else if (animal->actor.date.year == LIVESTOCK_COW_OLD_YEAR + 1) {
            if ((u32)rand_range(100) < LIVESTOCK_OLD_CHANCE_SECOND) {
                animal->actor.flags |= (ACTOR_FLAG_DIED | ACTOR_FLAG_DIED_OF_AGE);
            }
        }
        if (animal->actor.date.year >= LIVESTOCK_COW_OLD_YEAR + 2) {
            animal->actor.flags |= (ACTOR_FLAG_DIED | ACTOR_FLAG_DIED_OF_AGE);
        }
    }
    animal->growthDays++;
    animal->growthDays = (animal->growthDays > LIVESTOCK_MAX_GROWTH_DAYS) ? LIVESTOCK_MAX_GROWTH_DAYS : animal->growthDays;
    animal->actor.sprite.source = 0;
    if (animal->actor.date.day == game_state.progress.clock.date.day && animal->actor.date.season == game_state.progress.clock.date.season) {
        animal->actor.date.year++;
    }
    animal->actor.interruptTime = rand_range(300);
    if (!(animal->actor.flags & ACTOR_FLAG_FED_TODAY)) {
        game_state.progress.estate.estateFlags |= ESTATE_FLAG_ANIMAL_AGED;
    }
    animal->grazeCount = 0;
    animal->actor.flags &= ~ACTOR_FLAG_FED_TODAY;
    animal->actor.flags &= ~ACTOR_FLAG_TALKED_TODAY;
    animal->actor.flags &= ~ACTOR_FLAG_BIT2;
    animal->actor.flags &= ~ACTOR_FLAG_COW_CUE_TODAY;
    animal->actor.flags &= ~ACTOR_FLAG_APPROACHING_PLAYER;
    cue_reset(cue);
    return 0;
}

/* The cow's growth stage kind (calf, young cow or cow), or -1 when the record is not live. */
static int livestock_growth_kind(FarmLivestock *obj) {
    if (!livestock_is_live(&obj->actor)) return -1;
    if (obj->growthDays < LIVESTOCK_YOUNG_DAYS) return ITEM_CALF;
    if (obj->growthDays < LIVESTOCK_ADULT_DAYS) return ITEM_YOUNG_COW;
    return ITEM_COW;
}

/* The start handler of each state, by state number; 0 where dispatch does not start the state. */
void *const livestock_action_start_handlers[20] = {
    0, 0, 0, livestock_start_neglect_cue, livestock_start_scold_cue, livestock_talk, livestock_set_bit2_cue, livestock_cow_cue, 0,
    livestock_cure_sickness, livestock_start_pregnancy, livestock_approach_player_chance, livestock_approach_player, livestock_position_update,
    livestock_action_none, 0, 0, 0, 0, 0,
};

/* The tick of each state, by state number: it runs the state's cue or walk every frame. */
void *const livestock_action_tick_handlers[20] = {
    cow_idle_cue, cow_sleep_cue, cow_wake_cue, cow_neglect_cue, cow_scold_cue,
    cow_happy_cue, cow_happy_cue, cow_happy_cue, 0, cow_happy_cue, cow_happy_cue, 0, 0,
    cow_cue_13, cow_cue_14, cow_sleep_cue, cow_wake_cue, cow_cue_17,
    livestock_walk_cue, 0,
};

/* Advance a cow: load its sprite in the pasture or barn, run its current action's
 * tick, and pick a new state when its timer runs out. */
s32 livestock_dispatch_tick(FarmLivestock *object) {
    DefHandler handler;
    if (!livestock_is_live(&object->actor)) return -1;
    if (farm_area.mode.place.location == PLACE_FARM || farm_area.mode.place.location == PLACE_5) {
        if (object->actor.sprite.source == 0) livestock_load_mode_sprite(object);
    }
    handler = (DefHandler)livestock_action_tick_handlers[(u16)object->actor.taskNo];
    if (handler != 0) handler(object);
    if ((object->actor.flags & ACTOR_FLAG_CUE_RUNNING) == 0 && object->actor.interruptTime >= 300) {
        livestock_pick_state(object);
        object->actor.interruptTime = 0;
    }
    object->actor.interruptTime++;
    return 0;
}

/* Start a state on a cow if its start handler allows it, then reset its timing and speed and
 * clear its helper. Returns -1 when the state is blocked. */
s32 livestock_action_dispatch(FarmLivestock *action, s32 state) {
    s32 result;

    result = 0;
    if (livestock_action_start_handlers[state] != 0) {
        if (action->actor.taskNo == state && (action->actor.flags & ACTOR_FLAG_CUE_RUNNING)) {
            result = -1;
        } else {
            result = ((s32(*)(FarmLivestock *))livestock_action_start_handlers[state])(action);
        }
    }
    if (livestock_action_tick_handlers[state] != 0 && result != -1) {
        action->actor.taskNo = state;
        action->actor.taskId = 0;
        action->actor.taskTime = 0;
        action->actor.interruptTime = 0;
        farm_vector_adjust(4, &action->actor.speed, 0);
    }
    action->helperNumber = -1;
    return result;
}

/* Zero a cow record, leaving it with no kind. */
void livestock_clear_record(FarmLivestock *p) {
    memset(p, 0, sizeof(FarmLivestock));
    p->actor.kind = 0;
}

/* Start a fresh cow, young cow or calf record, aged 0 years, with the growth days its kind
 * implies. */
static s32 livestock_record_init(FarmLivestock *item, s32 kind) {
    s16 mode;
    FarmActor *actor;
    if (livestock_is_live(&item->actor) != 0) return -1;
    memset(item, 0, sizeof(FarmLivestock));
    actor = &item->actor;
    switch (kind) {
    case ITEM_YOUNG_COW:
        item->growthDays = LIVESTOCK_YOUNG_DAYS;
        break;
    case ITEM_COW:
        item->growthDays = LIVESTOCK_ADULT_DAYS;
        break;
    }
    actor->kind = kind;
    actor->affection = 10;
    actor->date = game_state.progress.clock.date;
    actor->name[0] = 0xFFFF;
    actor->date.year = 0;
    mode = farm_area.mode.place.location;
    if (mode == PLACE_FARM || mode == PLACE_5)
        livestock_load_mode_sprite(item);
    else
        actor->sprite.source = 0;
    cue_reset(&actor->cue);
    return 0;
}

/* Set the cow's kind from its growth stage and load its sprite: a pregnant cow gets its own
 * sprite, and a sick one its animation offsets. */
static s32 livestock_load_mode_sprite(FarmLivestock *object) {
    s32 kind;
    s32 resource;

    resource = 0;
    kind = livestock_growth_kind(object);
    switch (kind) {
    case ITEM_CALF:
        object->actor.kind = kind;
        resource = LIVESTOCK_PACK_CALF;
        break;
    case ITEM_YOUNG_COW:
        object->actor.kind = kind;
        resource = LIVESTOCK_PACK_COW;
        break;
    case ITEM_COW:
        object->actor.kind = kind;
        resource = LIVESTOCK_PACK_COW;
        break;
    }
    object->actor.flags &= ~ACTOR_FLAG_BIT26;
    if (object->pregnancy != 0) resource = LIVESTOCK_PACK_PREGNANT_COW;
    anim_bind_file(&object->actor.sprite, (AnimFile *)&farm_area.slotAnims[ANIM_PACK_START(farm_slot_tocs[0], resource - 1)]);
    if (kind == ITEM_YOUNG_COW) object->actor.sprite.scale = LIVESTOCK_YOUNG_SCALE;

    if (object->daysSick != 0) {
        switch (kind) {
        case ITEM_CALF:
            anim_set_offsets(&object->actor.sprite, 0, 0, 0, 3);
            break;
        case ITEM_YOUNG_COW:
            anim_set_offsets(&object->actor.sprite, 0, 0, 0, 1);
            break;
        case ITEM_COW:
            anim_set_offsets(&object->actor.sprite, 0, 0, 0, 1);
            break;
        }
        if (object->pregnancy != 0) {
            anim_set_offsets(&object->actor.sprite, 0, 0, 0x20, 0);
        }
    }
    return 0;
}

static FrameRow livestock_frame_table[9] = {
    {{{0, 0}, {0, 1}, {0, 2}, {0, 3}}},
    {{{1, 0}, {2, 0}, {3, 0}, {4, 0}}},
    {{{5, 0}, {6, 0}, {7, 0}, {8, 0}}},
    {{{9, 0}, {10, 0}, {11, 0}, {12, 0}}},
    {{{19, 0}, {20, 0}, {21, 0}, {22, 0}}},
    {{{13, 0}, {13, 1}, {13, 2}, {13, 3}}},
    {{{14, 0}, {0, 1}, {0, 2}, {15, 0}}},
    {{{16, 0}, {0, 1}, {0, 2}, {16, 1}}},
    {{{9, 0}, {17, 0}, {18, 0}, {12, 0}}},
};

/* The animation and frame of an action (row) and a facing (column) in the frame table; the
 * frame is only written when out_frame is given. */
static s32 livestock_frame_lookup(u32 group, u32 index, u16 *out_value, u16 *out_frame) {
    u16 value;
    group &= 0xffff;
    index &= 0xffff;
    value = livestock_frame_table[group].entry[index].value;
    *out_value = value;
    if (out_frame != 0) {
        *out_frame = livestock_frame_table[group].entry[index].frame;
    }
    return 0;
}

/* Pick a cow's next state: bed (LIVESTOCK_STATE_BED) outside hours 6 to 20, GET_UP when it is
 * already in BED, otherwise a random pick (out of 9 when it is approaching the player, else 4).
 * The state is only set on a change. */
static s32 livestock_pick_state(FarmLivestock *animal) {
    s32 next;

    if ((u32)((u16)game_state.progress.clock.time.hm.hour - FARM_DAY_HOUR_FIRST) >= FARM_DAY_HOUR_COUNT) {
        next = LIVESTOCK_STATE_BED;
    } else if (animal->actor.taskNo == LIVESTOCK_STATE_BED) {
        next = LIVESTOCK_STATE_GET_UP;
    } else {
        switch (rand_range((animal->actor.flags & ACTOR_FLAG_APPROACHING_PLAYER) ? 9 : 4)) {
        case 1:
            next = 0xE;
            break;
        case 2:
            next = LIVESTOCK_STATE_BED;
            break;
        case 3:
            next = 0x11;
            if (animal->pregnancy != 0)
                next = 0xE;
            break;
        case 0:
        default:
            next = LIVESTOCK_STATE_WALK;
            break;
        }
    }
    if (next != animal->actor.taskNo)
        livestock_action_dispatch(animal, next);
    return 0;
}

static s32 livestock_is_live(FarmActor *animal) {
    u32 mask = ACTOR_FLAG_BIT16;
    if (animal->kind == ITEM_NOTHING) return 0;
    return (animal->flags & mask) == 0;
}

/* Neglect: affection falls and the neglect icon shows; the neglect track plays here. */
static s32 livestock_start_neglect_cue(FarmActor *obj) {
    if (livestock_is_live(obj) == 0) {
        return -1;
    }
    obj->affection -= LIVESTOCK_NEGLECT_AFFECTION;
    obj->affection = (obj->affection < 0) ? 0 : obj->affection;
    cue_set(&obj->cue, CUE_ICON_NEGLECT, CUE_PHASE_LONG);
    if (farm_area.mode.place.location != obj->place.location) {
        return 0;
    }
    farm_request_track(CD_TRACK_COW_NEGLECT);
    return 0;
}

/* Scold: affection falls and the scold icon shows, for the short phase. */
static s32 livestock_start_scold_cue(FarmActor *obj) {
    if (livestock_is_live(obj) == 0) return -1;
    obj->affection -= 10;
    obj->affection = obj->affection < 0 ? 0 : obj->affection;
    cue_set(&obj->cue, CUE_ICON_SCOLD, CUE_PHASE_SHORT);
    return 0;
}

/* Cure: a sick cow loses its sickness, and is praised; the praise track plays here. */
static s32 livestock_cure_sickness(FarmLivestock *obj) {
    if (livestock_is_live(&obj->actor) == 0) return -1;
    if (obj->daysSick == 0) return -1;
    obj->daysSick = 0;
    anim_set_offsets(&obj->actor.sprite, 0, 0, 0, 0);
    farm_cue_praise(&obj->actor);
    if (farm_area.mode.place.location != obj->actor.place.location) return 0;
    farm_request_track(CD_TRACK_COW_PRAISE);
    return 0;
}

/* Praise: the cow is praised once a day (BIT2 marks it done); the praise track plays here. */
static s32 livestock_set_bit2_cue(FarmActor *actor) {
    if (livestock_is_live(actor) == 0) return -1;
    if (!(actor->flags & ACTOR_FLAG_BIT2)) farm_cue_praise(actor);
    actor->flags |= ACTOR_FLAG_BIT2;
    if (farm_area.mode.place.location != actor->place.location) return 0;
    farm_request_track(CD_TRACK_COW_PRAISE);
    return 0;
}

/* Breed: a healthy cow not yet pregnant reserves a free record for the calf and is praised. */
static s32 livestock_start_pregnancy(FarmLivestock *object) {
    FarmLivestock *item;
    FarmModeSlot *source;
    s32 object_index;
    s16 item_slot;
    s32 result;
    result = -1;
    if (livestock_is_live(&object->actor) == 0) return result;
    if (object->actor.kind != ITEM_COW) return result;
    if (object->pregnancy != 0) return result;
    if (object->daysSick != 0) return result;
    source = farm_mode_slot_find(1);
    if (source == 0) return result;
    item = livestock_find_free();
    if (item == 0) return result;
    item_slot = livestock_index(item);
    object_index = livestock_index(object);
    farm_mode_slot_set(source, item_slot, object_index);
    item->actor.flags |= ACTOR_FLAG_CLAIMED;
    object->pregnancy = 1;
    farm_cue_praise(&object->actor);
    if (farm_area.mode.place.location != object->actor.place.location) return 0;
    farm_request_track(CD_TRACK_COW_PRAISE);
    return 0;
}

/* Cow product cue: once a day, a cow fed yesterday and not pregnant gives a product picked by
 * affection (above 200 the best; a prize winner above 200 gives 0xE3). The product goes to the
 * player when no helper carries the cow, or to the helper if his hands are free. Returns 0
 * when a product was issued, else -1. */
static s32 livestock_cow_cue(FarmLivestock *object) {
    FarmStoredItem cue;
    s16 level;
    s16 slot;
    s32 cueId;

    if (livestock_is_live(&object->actor) == 0 || object->actor.kind != ITEM_COW) {
    fail:
        return -1;
    }
    if (object->pregnancy != 0) {
        goto fail;
    }
    if (object->actor.flags & ACTOR_FLAG_COW_CUE_TODAY) {
        goto fail;
    }
    if (!(object->actor.flags & ACTOR_FLAG_FED_YESTERDAY)) {
        object->actor.flags |= ACTOR_FLAG_COW_CUE_TODAY;
        goto fail;
    }
    level = object->actor.affection;
    cueId = 0;
    if (level != 0) {
        if (level > 200) {
            cueId = 0x8D;
            if (object->actor.flags & ACTOR_FLAG_PRIZE_WINNER) {
                cueId = 0xE3;
            }
        } else if (level > 100) {
            cueId = 0x8C;
        } else if (level > 1) {
            cueId = 0x8B;
        }
    }
    slot = object->helperNumber;
    if (slot == -1) {
        if (cueId != 0) {
            object->actor.flags |= ACTOR_FLAG_COW_CUE_TODAY;
            stored_item_set(&cue, cueId, 0, 1);
            player_show_item(&cue);
            farm_cue_praise(&object->actor);
        }
    } else if (cueId != 0) {
        if (game_state.helpers[slot].item == ITEM_NOTHING) {
            object->actor.flags |= ACTOR_FLAG_COW_CUE_TODAY;
            game_state.helpers[slot].item = cueId;
            farm_cue_praise(&object->actor);
        } else {
            cueId = 0;
        }
    }
    if (cueId != 0 && farm_area.mode.place.location == object->actor.place.location) {
        farm_request_track(CD_TRACK_COW_PRAISE);
    }
    return cueId != 0 ? 0 : -1;
}

/* Talk: the cow is praised the first time today, and its message opens. */
static s32 livestock_talk(FarmLivestock *animal) {
    if (livestock_is_live(&animal->actor) == 0) return -1;
    if ((animal->actor.flags & ACTOR_FLAG_TALKED_TODAY) == 0) farm_cue_praise(&animal->actor);
    animal->actor.flags |= ACTOR_FLAG_TALKED_TODAY;
    farm_message_request(animal);
    if (farm_area.mode.place.location != animal->actor.place.location) return 0;
    farm_request_track(CD_TRACK_COW_PRAISE);
    return 0;
}

/* Approach: with a chance of affection out of 200 the cow walks up to the player; returns -1
 * when it does not, or is not in the player's place. */
static s32 livestock_approach_player_chance(FarmLivestock *request) {
    s32 result;
    result = 0;
    if (farm_area.mode.place.location != request->actor.place.location) return -1;
    if (rand_range(200) < request->actor.affection) {
        request->actor.destination.vx = game_state.player.position.vx;
        request->actor.destination.vy = game_state.player.position.vy;
        request->actor.destination.vz = game_state.player.position.vz;
        request->actor.destination.pad = game_state.player.position.pad;
        request->actor.flags |= ACTOR_FLAG_APPROACHING_PLAYER;
        livestock_action_dispatch(request, LIVESTOCK_STATE_WALK);
    } else {
        result = -1;
    }
    return result;
}

/* Approach: the cow walks up to the player at once. */
static s32 livestock_approach_player(FarmLivestock *animal) {
    animal->actor.destination.vx = game_state.player.position.vx;
    animal->actor.destination.vy = game_state.player.position.vy;
    animal->actor.destination.vz = game_state.player.position.vz;
    animal->actor.destination.pad = game_state.player.position.pad;
    animal->actor.flags |= ACTOR_FLAG_APPROACHING_PLAYER;
    livestock_action_dispatch(animal, LIVESTOCK_STATE_WALK);
    return 0;
}

/* Move a living animal. */
static s32 livestock_position_update(FarmActor *obj) {
    if (livestock_is_live(obj) == 0) return -1;
    return farm_mover_update(obj);
}

static s32 livestock_action_none(void) {
    return 0;
}

FARM_CUE_RUNNER(cow_idle_cue, {0, 30}, {-1, 0})

FARM_CUE_RUNNER_NO_TIMER(cow_sleep_cue, {0, 10}, {5, 10}, {2, 0}, {-1, 0})

static s32 cow_wake_cue(FarmActor *self) {
    static FarmCueStep steps[3] = {{5, 10}, {0, 10}, {-1, 0}};
    s16 step;
    s32 finished;

    step = self->taskId;
    finished = 0;
    switch (step) {
    case CUE_STEP_START:
        self->taskId = CUE_STEP_PLAY;
        self->flags |= ACTOR_FLAG_CUE_RUNNING;
        break;
    case LIVESTOCK_WAKE_DONE:
        finished = 1;
        self->interruptTime = 300;
        self->flags &= ~ACTOR_FLAG_CUE_RUNNING;
        break;
    case CUE_STEP_PLAY:
    default:
        if (farm_cue_step_run(self, steps, &self->taskId, -1, &self->taskTime) != 0) {
            self->taskId = LIVESTOCK_WAKE_DONE;
        }
        break;
    }
    return finished;
}

/* Neglect: the cow walks off, slowly as its neglect pose plays, then stops. */
static s32 cow_neglect_cue(FarmActor *self) {
    static FarmCueStep steps[3] = {{4, 60}, {1, 0}, {-1, 0}};
    s16 step;
    s32 finished;

    step = self->taskId;
    finished = 0;
    switch (step) {
    case CUE_STEP_START:
        self->taskId = CUE_STEP_PLAY;
        self->interruptTime = 0;
        self->flags |= ACTOR_FLAG_CUE_RUNNING;
        farm_walker_move(self, LIVESTOCK_WALK_SPEED_NEGLECT, FARM_WALK_AROUND_PLAYER);
        break;
    case CUE_STEP_STOP:
        finished = 1;
        self->interruptTime = 300;
        self->flags &= ~ACTOR_FLAG_BIT14;
        self->flags &= ~ACTOR_FLAG_CUE_RUNNING;
        break;
    case CUE_STEP_PLAY:
    default:
        if (self->interruptTime != 0) {
            self->interruptTime--;
        }
        if (farm_cue_step_run(self, steps, &self->taskId, -1, &self->interruptTime) != 0) {
            self->taskId = CUE_STEP_STOP;
        } else {
            farm_walker_move(self, LIVESTOCK_WALK_SPEED_NEGLECT, FARM_WALK_AROUND_PLAYER);
        }
        break;
    }
    return finished;
}

FARM_CUE_RUNNER(cow_scold_cue, {7, 30}, {0, 0}, {-1, 0})

FARM_CUE_RUNNER(cow_happy_cue, {6, 30}, {0, 0}, {-1, 0})

FARM_CUE_RUNNER_SHARED_STORE(cow_cue_14, {3, 0}, {-1, 0})

FARM_CUE_RUNNER_SHARED_STORE(cow_cue_17, {8, 0}, {-1, 0})

/* Approach state: ends once the cow is no longer walking up to the player; otherwise it walks on. */
static s32 cow_cue_13(FarmLivestock *p) {
    u32 flags = p->actor.flags | ACTOR_FLAG_CUE_RUNNING;
    p->actor.flags = flags;
    if (!(flags & ACTOR_FLAG_APPROACHING_PLAYER)) {
        p->actor.flags = flags & ~ACTOR_FLAG_CUE_RUNNING;
        p->actor.interruptTime = 300;
        return 1;
    }
    p->actor.place.value = 0;
    livestock_walk_cue(p);
    return 0;
}

/* Walk a cow one step for its state: the approach walk goes through the player's cell, and a
 * pregnant cow walks slowly. Ends in rest, or moving while it still has speed. */
static s32 livestock_walk_cue(FarmLivestock *obj) {
    s32 cue_kind;
    s32 special;
    u16 pregnancy;
    s32 cue;
    cue_kind = LIVESTOCK_WALK_SPEED;
    pregnancy = obj->pregnancy;
    special = obj->actor.taskNo == LIVESTOCK_STATE_APPROACH_WALK;
    if (pregnancy != 0) {
        cue_kind = LIVESTOCK_WALK_SPEED_PREGNANT;
    }
    cue = FARM_WALK_AROUND_PLAYER;
    if (special != 0) {
        cue = FARM_WALK_TO_PLAYER;
        if (pregnancy == 0) {
            obj->actor.flags |= ACTOR_FLAG_BIT26;
        }
    } else {
        if (game_state.player.place.location == PLACE_5 && game_state.world.weather == 0) {
            if (pregnancy == 0) {
                obj->actor.flags |= ACTOR_FLAG_BIT26;
            }
        } else {
            obj->actor.flags &= ~ACTOR_FLAG_BIT26;
        }
    }
    farm_walker_move(&obj->actor, cue_kind, cue);
    if (obj->actor.speed.vx == 0 && obj->actor.speed.vy == 0 && obj->actor.speed.vz == 0) {
        if (special != 0) {
            obj->actor.flags &= ~ACTOR_FLAG_APPROACHING_PLAYER;
        }
        obj->actor.action = ACTOR_ACTION_REST;
    } else {
        obj->actor.action = ACTOR_ACTION_MOVING;
    }
    obj->actor.flags &= ~ACTOR_FLAG_CUE_RUNNING;
    return 1;
}

/* Show the sheep's sprite for its current action and facing. */
void livestock_sprite_refresh_b(FarmActor *object) {
    u16 animation;
    u16 frame;

    livestock_frame_lookup_b(object->action, object->facing, &animation, &frame);
    if (animation != object->sprite.animation) {
        anim_select_range(&object->sprite, animation);
    }
    if ((frame != 0) && (frame != object->sprite.frame)) {
        anim_set_frame(&object->sprite, frame);
    }
    farm_draw_actor(object);
    if ((object->action == ACTOR_ACTION_REST) || (object->action == ACTOR_ACTION_5)) {
        object->sprite.flags = object->sprite.flags | 0x100;
    }
}

void livestock_pair_at_38_b(FarmLivestock *obj) {
    farm_pair_at_38(&obj->actor);
}

/* Spawn a lamb or sheep of the given kind into a free record; returns the record, or 0 when the
 * kind is not a sheep or no record is free. */
FarmLivestock *livestock_spawn_b(s32 slot, s32 unused1, s32 unused2) {
    FarmLivestock *item;
    s32 value;

    if ((slot - ITEM_LAMB) < 2U) {
        item = livestock_find_free();
        if (item != 0) {
            value = livestock_index(item);
            livestock_record_init_b(item, slot);
            farm_place_spot(item, value, 1);
            return item;
        }
    }
    return 0;
}

/* Place a newborn lamb in the destination record, a step behind its mother at source along z,
 * with the mother's place and facing. */
FarmLivestock *livestock_copy_placement_b(s32 source, s32 destination) {
    FarmLivestock *destination_record;
    FarmLivestock *source_record;

    destination_record = &game_state.animals.livestock[destination];
    livestock_record_init_b(destination_record, ITEM_LAMB);
    source_record = &game_state.animals.livestock[source];
    destination_record->actor.position.vx = source_record->actor.position.vx;
    destination_record->actor.position.vy = source_record->actor.position.vy;
    destination_record->actor.position.vz = source_record->actor.position.vz - LIVESTOCK_OFFSPRING_Z_OFFSET;
    destination_record->actor.place.location = source_record->actor.place.location;
    destination_record->actor.facing = source_record->actor.facing;
    return destination_record;
}

/* Advance a sheep's daily timer: in the pasture outside winter it grazes or loses affection,
 * by the weather, and the cue stops outside hours 6 to 20. */
s32 livestock_action_timer_b(FarmLivestock *obj) {
    s16 mode;
    u32 flags;
    s32 count;
    s32 value;
    s32 clamped;
    s32 shifted;
    if (livestock_is_live_b(&obj->actor) == 0) return -1;
    flags = obj->actor.flags;
    if (flags & ACTOR_FLAG_IN_EVENT) return -1;
    mode = obj->actor.place.location;
    if (mode == PLACE_FARM) {
        if (game_state.progress.clock.date.season != SEASON_WINTER) {
            if (game_state.world.weather != mode) {
                count = obj->grazeCount + 1;
                obj->grazeCount = count;
                if (obj->grazeCount >= LIVESTOCK_FED_GRAZE && (flags & ACTOR_FLAG_FED_TODAY) == 0 &&
                    field_find_ready_grown(&obj->actor.position) == mode)
                    obj->actor.flags |= ACTOR_FLAG_FED_TODAY;
            } else {
                value = (u16)obj->actor.affection - LIVESTOCK_BAD_WEATHER_AFFECTION;
                obj->actor.affection = value;
                shifted = value << 16;
                clamped = value;
                if (shifted < 0) clamped = 0;
                obj->actor.affection = clamped;
            }
        }
    }
    if ((u32)((u16)game_state.progress.clock.time.hm.hour - FARM_DAY_HOUR_FIRST) >= FARM_DAY_HOUR_COUNT) {
        obj->actor.interruptTime = 300;
        obj->actor.flags &= ~ACTOR_FLAG_CUE_RUNNING;
    }
    return 0;
}

/* The overnight update of one sheep: as for a cow, but sheep count days since shearing while fed
 * yesterday, and die of old age from year 3 (a cow from year 8). */
s32 livestock_overnight_update_alt(FarmLivestock *animal) {
    ItemCue *cue;
    s32 i;
    s32 value;
    s32 capped;

    if (livestock_is_live_b(&animal->actor) == 0) {
        return -1;
    }
    if (animal->actor.flags & ACTOR_FLAG_IN_EVENT) {
        return -1;
    }
    livestock_index(animal);
    cue = &animal->actor.cue;
    if (animal->actor.place.location == PLACE_5 && !(animal->actor.flags & ACTOR_FLAG_FED_TODAY)) {
        for (i = 0; i < BARN_FEED_BINS; i++) {
            if (game_state.progress.estate.barnFeedBins[i] != 0) {
                animal->actor.flags |= ACTOR_FLAG_FED_TODAY;
                game_state.progress.estate.barnFeedBins[i] = 0;
                break;
            }
        }
    }
    if (animal->actor.flags & ACTOR_FLAG_TALKED_TODAY) {
        animal->actor.affection += LIVESTOCK_TALK_AFFECTION;
    }
    if (animal->actor.flags & ACTOR_FLAG_FED_YESTERDAY) {
        animal->daysSinceShearing++;
        animal->daysSinceShearing = (animal->daysSinceShearing > LIVESTOCK_SHEAR_DAYS) ? LIVESTOCK_SHEAR_DAYS : animal->daysSinceShearing;
    }
    if (animal->actor.flags & ACTOR_FLAG_FED_TODAY) {
        animal->actor.flags |= ACTOR_FLAG_FED_YESTERDAY;
        animal->actor.affection += LIVESTOCK_FED_AFFECTION;
    } else {
        animal->actor.flags &= ~ACTOR_FLAG_FED_YESTERDAY;
        animal->actor.affection -= LIVESTOCK_UNFED_AFFECTION;
    }
    if (animal->grazeCount >= LIVESTOCK_FED_GRAZE) {
        animal->actor.affection += LIVESTOCK_GRAZE_AFFECTION;
    }
    value = (u16)animal->actor.affection;
    if (animal->actor.affection < 0) {
        value = 0;
    }
    animal->actor.affection = value;
    capped = value;
    if ((s16)capped > LIVESTOCK_MAX_AFFECTION) {
        capped = LIVESTOCK_MAX_AFFECTION;
    }
    animal->actor.affection = capped;
    if (!(animal->actor.flags & ACTOR_FLAG_DIED)) {
        value = animal->daysSick;
        if (value != 0) {
            value++;
            animal->daysSick = value;
            if ((u16)value > LIVESTOCK_MAX_DAYS_SICK) {
                value = LIVESTOCK_MAX_DAYS_SICK;
            }
            animal->daysSick = value;
            if ((u16)value >= LIVESTOCK_SICK_DEATH_FROM && rand_range(100) <= animal->daysSick * LIVESTOCK_SICK_DEATH_PER_DAY) {
                animal->actor.flags |= ACTOR_FLAG_DIED;
            }
        } else if (animal->actor.affection <= 0) {
            if ((u32)rand_range(100) < LIVESTOCK_SICK_CHANCE) {
                animal->daysSick++;
            }
        }
    }
    if (animal->pregnancy != 0) {
        animal->pregnancy++;
        if (animal->pregnancy >= LIVESTOCK_GESTATION_DAYS) {
            animal->pregnancy = 0;
            animal->actor.flags |= ACTOR_FLAG_NAMING_PENDING;
        }
    }
    if (!(animal->actor.flags & ACTOR_FLAG_DIED)) {
        if (animal->actor.date.year == LIVESTOCK_SHEEP_OLD_YEAR) {
            if ((u32)rand_range(100) < LIVESTOCK_OLD_CHANCE_FIRST) {
                animal->actor.flags |= (ACTOR_FLAG_DIED | ACTOR_FLAG_DIED_OF_AGE);
            }
        } else if (animal->actor.date.year == LIVESTOCK_SHEEP_OLD_YEAR + 1) {
            if ((u32)rand_range(100) < LIVESTOCK_OLD_CHANCE_SECOND) {
                animal->actor.flags |= (ACTOR_FLAG_DIED | ACTOR_FLAG_DIED_OF_AGE);
            }
        }
        if (animal->actor.date.year >= LIVESTOCK_SHEEP_OLD_YEAR + 2) {
            animal->actor.flags |= (ACTOR_FLAG_DIED | ACTOR_FLAG_DIED_OF_AGE);
        }
    }
    animal->growthDays++;
    animal->growthDays = (animal->growthDays > LIVESTOCK_MAX_GROWTH_DAYS) ? LIVESTOCK_MAX_GROWTH_DAYS : animal->growthDays;
    animal->actor.sprite.source = 0;
    if (animal->actor.date.day == game_state.progress.clock.date.day && animal->actor.date.season == game_state.progress.clock.date.season) {
        animal->actor.date.year++;
    }
    animal->actor.interruptTime = rand_range(300);
    if (!(animal->actor.flags & ACTOR_FLAG_FED_TODAY)) {
        game_state.progress.estate.estateFlags |= ESTATE_FLAG_ANIMAL_AGED;
    }
    animal->grazeCount = 0;
    animal->actor.flags &= ~ACTOR_FLAG_FED_TODAY;
    animal->actor.flags &= ~ACTOR_FLAG_TALKED_TODAY;
    animal->actor.flags &= ~ACTOR_FLAG_BIT2;
    animal->actor.flags &= ~ACTOR_FLAG_COW_CUE_TODAY;
    animal->actor.flags &= ~ACTOR_FLAG_APPROACHING_PLAYER;
    cue_reset(cue);
    return 0;
}

/* The sheep's growth stage kind (lamb or sheep), or -1 when the record is not live. */
static s32 livestock_growth_kind_b(FarmLivestock *obj) {
    if (livestock_is_live_b(&obj->actor) == 0) return -1;
    if (obj->growthDays < LIVESTOCK_YOUNG_DAYS) return ITEM_LAMB;
    return ITEM_SHEEP;
}

void *const livestock_action_start_handlers_b[20] = {
    0, 0, 0, livestock_start_neglect_cue_b, livestock_start_scold_cue_b, livestock_talk_b, livestock_set_bit2_cue_b, 0, livestock_sheep_harvest,
    livestock_cure_sickness_b, livestock_start_pregnancy_b, livestock_approach_player_chance_b, livestock_approach_player_b, livestock_position_update_b,
    livestock_action_none_b, 0, 0, 0, 0, 0,
};

void *const livestock_action_tick_handlers_b[20] = {
    sheep_idle_cue, sheep_sleep_cue, sheep_wake_cue, sheep_neglect_cue, sheep_scold_cue,
    sheep_happy_cue, sheep_happy_cue, 0, sheep_happy_cue, sheep_happy_cue, sheep_happy_cue, 0, 0,
    livestock_action_check, sheep_cue_14, sheep_sleep_cue, sheep_wake_cue, 0, livestock_walk_start, 0,
};

/* Advance a sheep: load its sprite in the pasture or barn, run its current action's
 * tick, and pick a new state when its timer runs out. */
s32 livestock_dispatch_tick_b(FarmLivestock *object) {
    void (*callback)();
    void (**callbacks)();
    u16 callback_index;
    u16 timer;
    s16 mode;

    if (object->actor.kind == ITEM_NOTHING) {
        return -1;
    }
    mode = farm_area.mode.place.location;
    if (mode == PLACE_FARM || mode == PLACE_5) {
        if (object->actor.sprite.source == 0) {
            livestock_load_mode_sprite_b(object);
        }
    }
    callbacks = (void (**)())livestock_action_tick_handlers_b;
    callback_index = object->actor.taskNo;
    callback = callbacks[callback_index];
    if (callback != 0) {
        callback(object);
    }
    if ((object->actor.flags & ACTOR_FLAG_CUE_RUNNING) == 0 && object->actor.interruptTime >= 300) {
        livestock_pick_state_b(object);
        object->actor.interruptTime = 0;
    }
    timer = object->actor.interruptTime;
    timer++;
    object->actor.interruptTime = timer;
    return 0;
}

/* Start a state on a sheep if its start handler allows it, then reset its timing and speed and
 * clear its helper. Returns -1 when the state is blocked. */
s32 livestock_action_dispatch_b(FarmLivestock *action, s32 state) {
    s32 result;

    result = 0;
    if (livestock_action_start_handlers_b[state] != 0) {
        if (action->actor.taskNo == state && (action->actor.flags & ACTOR_FLAG_CUE_RUNNING)) {
            result = -1;
        } else {
            result = ((s32(*)(FarmLivestock *))livestock_action_start_handlers_b[state])(action);
        }
    }
    if (livestock_action_tick_handlers_b[state] != 0 && result != -1) {
        action->actor.taskNo = state;
        action->actor.taskId = 0;
        action->actor.taskTime = 0;
        action->actor.interruptTime = 0;
        farm_vector_adjust(4, &action->actor.speed, 0);
    }
    action->helperNumber = -1;
    return result;
}

static void livestock_clear_record_b(FarmLivestock *p) {
    memset(p, 0, sizeof(FarmLivestock));
    p->actor.kind = 0;
}

/* Start a fresh lamb or sheep record, aged 0 years, with its wool grown (days since shearing at
 * LIVESTOCK_SHEAR_DAYS). */
static s32 livestock_record_init_b(FarmLivestock *item, s32 kind) {
    if (livestock_is_live_b(&item->actor) != 0) return -1;
    memset(item, 0, sizeof(FarmLivestock));
    if (kind == ITEM_SHEEP) item->growthDays = LIVESTOCK_YOUNG_DAYS;
    item->actor.affection = 10;
    item->actor.kind = kind;
    item->daysSinceShearing = LIVESTOCK_SHEAR_DAYS;
    item->actor.date = game_state.progress.clock.date;
    item->actor.date.year = 0;
    item->actor.name[0] = 0xFFFF;
    item->actor.flags |= ACTOR_FLAG_BIT16;
    if (farm_area.mode.place.location == PLACE_FARM || farm_area.mode.place.location == PLACE_5)
        livestock_load_mode_sprite_b(item);
    else
        item->actor.sprite.source = 0;
    cue_reset(&item->actor.cue);
    return 0;
}

/* Set the sheep's kind from its growth stage (lamb or sheep) and load its sprite: a pregnant
 * sheep and one shorn within LIVESTOCK_SHEAR_DAYS get their own sprites, and a sick one gets
 * its animation offsets. */
static s32 livestock_load_mode_sprite_b(FarmLivestock *arg) {
    FarmLivestock *object = arg;
    s32 kind;
    s32 resource;

    kind = livestock_growth_kind_b(arg);
    resource = LIVESTOCK_PACK_LAMB;
    switch (kind) {
    case ITEM_LAMB:
        object->actor.kind = kind;
        break;
    case ITEM_SHEEP:
        object->actor.kind = kind;
        resource = LIVESTOCK_PACK_SHEEP;
        break;
    }
    object->actor.flags &= ~ACTOR_FLAG_BIT26;
    if (object->pregnancy != 0) resource = LIVESTOCK_PACK_PREGNANT_SHEEP;
    if (object->daysSinceShearing < LIVESTOCK_SHEAR_DAYS) resource = LIVESTOCK_PACK_SHORN_SHEEP;
    anim_bind_file(&object->actor.sprite, (AnimFile *)&farm_area.slotAnims[ANIM_PACK_START(farm_slot_tocs[0], resource - 1)]);

    if (object->daysSick != 0) {
        switch (kind) {
        case ITEM_LAMB:
            anim_set_offsets(&object->actor.sprite, 0, 0, 0, 3);
            break;
        case ITEM_SHEEP:
            anim_set_offsets(&object->actor.sprite, 0, 0, 0, 2);
            break;
        }
        if (object->pregnancy != 0) {
            anim_set_offsets(&object->actor.sprite, 0, 0, 0x20, 0);
        }
    }
    return 0;
}

static LivestockFrameEntry livestock_frame_table_b[6][4] = {
    {{0, 0}, {0, 1}, {0, 2}, {0, 3}},
    {{1, 0}, {2, 0}, {3, 0}, {4, 0}},
    {{5, 0}, {6, 0}, {7, 0}, {8, 0}},
    {{9, 0}, {10, 0}, {11, 0}, {12, 0}},
    {{14, 0}, {15, 0}, {16, 0}, {17, 0}},
    {{13, 0}, {13, 1}, {13, 2}, {13, 3}},
};

/* The animation and frame of an action (row) and a facing (column) of a sheep. */
static s32 livestock_frame_lookup_b(u16 row, u16 col, u16 *out_x, u16 *out_y) {
    *out_x = livestock_frame_table_b[row][col].x;
    if (out_y != 0) *out_y = livestock_frame_table_b[row][col].y;
    return 0;
}

/* Pick a sheep's next state: asleep (BED) outside hours 6 to 20, GET_UP when it is already
 * in BED, otherwise a random pick among 0xE, BED and WALK (out of 3, or out of 9 when it
 * is approaching the player). The state is only set on a change. */
static s32 livestock_pick_state_b(FarmLivestock *action) {
    s32 state;

    if ((u32)((u16)game_state.progress.clock.time.hm.hour - FARM_DAY_HOUR_FIRST) >= FARM_DAY_HOUR_COUNT) {
        state = LIVESTOCK_STATE_BED;
    } else {
        if (action->actor.taskNo == LIVESTOCK_STATE_BED) {
            state = LIVESTOCK_STATE_GET_UP;
        } else {
            switch (rand_range((action->actor.flags & ACTOR_FLAG_APPROACHING_PLAYER) ? 9 : 3)) {
            case 1:
                state = 0xE;
                break;
            case 2:
                state = LIVESTOCK_STATE_BED;
                break;
            case 0:
            default:
                state = LIVESTOCK_STATE_WALK;
                break;
            }
        }
    }
    if (state != action->actor.taskNo) {
        livestock_action_dispatch_b(action, state);
    }
    return 0;
}

static s32 livestock_is_live_b(FarmActor *arg) {
    u32 mask = ACTOR_FLAG_BIT16;
    if (arg->kind == ITEM_NOTHING) return 0;
    return (arg->flags & mask) != 0;
}

/* Neglect cue: the sheep loses affection and the cue plays. */
static s32 livestock_start_neglect_cue_b(FarmActor *obj) {
    if (livestock_is_live_b(obj) == 0) {
        return -1;
    }
    obj->affection -= LIVESTOCK_NEGLECT_AFFECTION;
    obj->affection = (obj->affection < 0) ? 0 : obj->affection;
    cue_set(&obj->cue, CUE_ICON_NEGLECT, CUE_PHASE_LONG);
    if (farm_area.mode.place.location != obj->place.location) {
        return 0;
    }
    farm_request_track(CD_TRACK_SHEEP_NEGLECT);
    return 0;
}

/* Scold: affection falls by one and the scold icon shows, for the long phase. */
static s32 livestock_start_scold_cue_b(FarmActor *object) {
    if (livestock_is_live_b(object) == 0) {
        return -1;
    }
    object->affection--;
    object->affection = (object->affection < 0) ? 0 : object->affection;
    cue_set(&object->cue, CUE_ICON_SCOLD, CUE_PHASE_LONG);
    return 0;
}

/* Cure: a sick sheep loses its sickness, and is praised; the praise track plays here. */
static s32 livestock_cure_sickness_b(FarmLivestock *obj) {
    if (livestock_is_live_b(&obj->actor) == 0) return -1;
    if (obj->daysSick == 0) return -1;
    obj->daysSick = 0;
    anim_set_offsets(&obj->actor.sprite, 0, 0, 0, 0);
    farm_cue_praise(&obj->actor);
    if (farm_area.mode.place.location != obj->actor.place.location) return 0;
    farm_request_track(CD_TRACK_SHEEP_PRAISE);
    return 0;
}

/* Praise: the sheep is praised once a day (BIT2 marks it done); the praise track plays here. */
static s32 livestock_set_bit2_cue_b(FarmActor *obj) {
    if (!livestock_is_live_b(obj)) return -1;
    if (!(obj->flags & ACTOR_FLAG_BIT2)) farm_cue_praise(obj);
    obj->flags |= ACTOR_FLAG_BIT2;
    if (farm_area.mode.place.location != obj->place.location) return 0;
    farm_request_track(CD_TRACK_SHEEP_PRAISE);
    return 0;
}

/* Breed: a healthy sheep that is not pregnant and has its wool grown reserves a free record for
 * the lamb and is praised. */
static s32 livestock_start_pregnancy_b(FarmLivestock *entry) {
    FarmModeSlot *source;
    FarmLivestock *item;
    s16 item_slot;
    s16 object_index;

    if (entry->actor.kind == ITEM_NOTHING) {
        return -1;
    }
    if (entry->actor.kind != ITEM_SHEEP) {
        return -1;
    }
    if (entry->pregnancy != 0) {
        return -1;
    }
    if (entry->daysSick != 0) {
        return -1;
    }
    if (entry->daysSinceShearing < LIVESTOCK_SHEAR_DAYS) {
        return -1;
    }
    source = farm_mode_slot_find(1);
    if (source == 0) {
        return -1;
    }
    item = livestock_find_free();
    if (item == 0) {
        return -1;
    }
    item_slot = livestock_index(item);
    object_index = livestock_index(entry);
    farm_mode_slot_set(source, item_slot, object_index);
    item->actor.flags |= ACTOR_FLAG_CLAIMED;
    entry->pregnancy = 1;
    farm_cue_praise(&entry->actor);
    if (farm_area.mode.place.location == entry->actor.place.location) {
        farm_request_track(CD_TRACK_SHEEP_PRAISE);
    }
    return 0;
}

/* Wool cue: a sheep whose wool has grown back (LIVESTOCK_SHEAR_DAYS since shearing) plays a cue by
 * its affection: 0x93 above 200, 0x92 above 100, else 0x91 (0xE7 for a prize winner
 * above 200). With no helper on it the cue plays at once and the wool count restarts;
 * otherwise it goes to the helper if the helper's hands are free. Returns 0 when a cue
 * was issued, else -1. */
static s32 livestock_sheep_harvest(FarmLivestock *obj) {
    FarmStoredItem cue;
    s32 cueId;
    s16 level;
    s16 slot;
    s16 mode;

    if (livestock_is_live_b(&obj->actor) == 0) {
        return -1;
    }
    if (obj->actor.kind != ITEM_SHEEP) {
        return -1;
    }
    if (obj->pregnancy != 0) {
        return -1;
    }
    if (obj->daysSinceShearing < LIVESTOCK_SHEAR_DAYS) {
        return -1;
    }
    level = obj->actor.affection;
    cueId = 0;
    if (level > 200) {
        cueId = 0x93;
        if (obj->actor.flags & ACTOR_FLAG_PRIZE_WINNER) {
            cueId = 0xE7;
        }
    } else if (level > 100) {
        cueId = 0x92;
    } else if (level >= 0) {
        cueId = 0x91;
    }
    slot = obj->helperNumber;
    if (slot == -1) {
        if (cueId != 0) {
            obj->daysSinceShearing = 0;
            stored_item_set(&cue, cueId, 0, 1);
            player_show_item(&cue);
            farm_cue_praise(&obj->actor);
            livestock_load_mode_sprite_b(obj);
        }
    } else if (cueId != 0) {
        if (game_state.helpers[slot].item == ITEM_NOTHING) {
            obj->daysSinceShearing = 0;
            game_state.helpers[slot].item = cueId;
            farm_cue_praise(&obj->actor);
            mode = farm_area.mode.place.location;
            if (mode == PLACE_FARM || mode == PLACE_5) {
                livestock_load_mode_sprite_b(obj);
            } else {
                obj->actor.sprite.source = 0;
            }
        } else {
            cueId = 0;
        }
    }
    if (cueId != 0 && farm_area.mode.place.location == obj->actor.place.location) {
        farm_request_track(CD_TRACK_SHEEP_PRAISE);
    }
    return cueId != 0 ? 0 : -1;
}

/* Talk: the sheep is praised the first time today, and its message opens. */
static s32 livestock_talk_b(FarmLivestock *obj) {
    if (livestock_is_live_b(&obj->actor) == 0) return -1;
    if (!(obj->actor.flags & ACTOR_FLAG_TALKED_TODAY)) farm_cue_praise(&obj->actor);
    obj->actor.flags |= ACTOR_FLAG_TALKED_TODAY;
    farm_message_request(obj);
    if (farm_area.mode.place.location == obj->actor.place.location) farm_request_track(CD_TRACK_SHEEP_PRAISE);
    return 0;
}

/* Approach: with a chance of affection out of 200 the sheep walks up to the player; returns -1
 * when it does not, or is not in the player's place. */
static s32 livestock_approach_player_chance_b(FarmLivestock *object) {
    s32 result;
    result = 0;
    if (farm_area.mode.place.location != object->actor.place.location) {
        return -1;
    }
    if (rand_range(200) < object->actor.affection) {
        object->actor.destination.vx = game_state.player.position.vx;
        object->actor.destination.vy = game_state.player.position.vy;
        object->actor.destination.vz = game_state.player.position.vz;
        object->actor.destination.pad = game_state.player.position.pad;
        object->actor.flags |= ACTOR_FLAG_APPROACHING_PLAYER;
        livestock_action_dispatch_b(object, LIVESTOCK_STATE_WALK);
    } else {
        result = -1;
    }
    return result;
}

/* Approach: the sheep walks up to the player at once. */
static s32 livestock_approach_player_b(FarmLivestock *animal) {
    animal->actor.destination.vx = game_state.player.position.vx;
    animal->actor.destination.vy = game_state.player.position.vy;
    animal->actor.destination.vz = game_state.player.position.vz;
    animal->actor.destination.pad = game_state.player.position.pad;
    animal->actor.flags |= ACTOR_FLAG_APPROACHING_PLAYER;
    livestock_action_dispatch_b(animal, LIVESTOCK_STATE_WALK);
    return 0;
}

/* Move a sheep with the mover update. */
static s32 livestock_position_update_b(FarmActor *object) {
    return farm_mover_update(object);
}

static s32 livestock_action_none_b(void) {
    return 0;
}

FARM_CUE_RUNNER(sheep_idle_cue, {0, 30}, {-1, 0})

FARM_CUE_RUNNER_NO_TIMER(sheep_sleep_cue, {0, 10}, {5, 10}, {2, 0}, {-1, 0})

static s32 sheep_wake_cue(FarmActor *self) {
    static FarmCueStep steps[3] = {{5, 10}, {0, 10}, {-1, 0}};
    s16 step;
    s32 finished;

    step = self->taskId;
    finished = 0;
    switch (step) {
    case CUE_STEP_START:
        self->taskId = CUE_STEP_PLAY;
        self->flags |= ACTOR_FLAG_CUE_RUNNING;
        break;
    case LIVESTOCK_WAKE_DONE:
        finished = 1;
        self->interruptTime = 300;
        self->flags &= ~ACTOR_FLAG_CUE_RUNNING;
        break;
    case CUE_STEP_PLAY:
    default:
        if (farm_cue_step_run(self, steps, &self->taskId, -1, &self->taskTime) != 0) {
            self->taskId = LIVESTOCK_WAKE_DONE;
        }
        break;
    }
    return finished;
}

/* Neglect: the sheep walks off, slowly as its neglect pose plays, then stops. */
static s32 sheep_neglect_cue(FarmActor *self) {
    static FarmCueStep steps[3] = {{4, 60}, {1, 0}, {-1, 0}};
    s16 step;
    s32 finished;

    step = self->taskId;
    finished = 0;
    switch (step) {
    case CUE_STEP_START:
        self->taskId = CUE_STEP_PLAY;
        self->interruptTime = 0;
        self->flags |= ACTOR_FLAG_CUE_RUNNING;
        farm_walker_move(self, LIVESTOCK_WALK_SPEED_NEGLECT, FARM_WALK_AROUND_PLAYER);
        break;
    case CUE_STEP_STOP:
        finished = 1;
        self->interruptTime = 300;
        self->flags &= ~ACTOR_FLAG_BIT14;
        self->flags &= ~ACTOR_FLAG_CUE_RUNNING;
        break;
    case CUE_STEP_PLAY:
    default:
        if (self->interruptTime != 0) {
            self->interruptTime--;
        }
        if (farm_cue_step_run(self, steps, &self->taskId, -1, &self->interruptTime) != 0) {
            self->taskId = CUE_STEP_STOP;
        } else {
            farm_walker_move(self, LIVESTOCK_WALK_SPEED_NEGLECT, FARM_WALK_AROUND_PLAYER);
        }
        break;
    }
    return finished;
}

FARM_CUE_RUNNER(sheep_scold_cue, {0, 30}, {0, 0}, {-1, 0})

FARM_CUE_RUNNER(sheep_happy_cue, {0, 30}, {0, 0}, {-1, 0})

FARM_CUE_RUNNER_SHARED_STORE(sheep_cue_14, {3, 0}, {-1, 0})

/* Approach state: ends once the sheep is no longer walking up to the player; otherwise it walks on. */
static s32 livestock_action_check(FarmLivestock *obj) {
    s32 flags = obj->actor.flags | ACTOR_FLAG_CUE_RUNNING;
    obj->actor.flags = flags;
    if (!(flags & ACTOR_FLAG_APPROACHING_PLAYER)) {
        obj->actor.flags = flags & ~ACTOR_FLAG_CUE_RUNNING;
        obj->actor.interruptTime = 300;
        return 1;
    }
    obj->actor.place.value = 0;
    livestock_walk_start(obj);
    return 0;
}

/* Walk a sheep one step for its state: the approach walk goes through the player's cell.
 * Ends in rest, or moving while it still has speed. */
static s32 livestock_walk_start(FarmLivestock *obj) {
    s32 special;
    u16 pregnancy;
    s32 cue;
    special = obj->actor.taskNo == LIVESTOCK_STATE_APPROACH_WALK;
    pregnancy = obj->pregnancy;
    cue = FARM_WALK_AROUND_PLAYER;
    if (special != 0) {
        cue = FARM_WALK_TO_PLAYER;
        if (pregnancy == 0) obj->actor.flags |= ACTOR_FLAG_BIT26;
    } else {
        if (game_state.player.place.location == PLACE_5 && game_state.world.weather == 0) {
            if (pregnancy == 0) obj->actor.flags |= ACTOR_FLAG_BIT26;
        } else {
            obj->actor.flags &= ~ACTOR_FLAG_BIT26;
        }
    }
    farm_walker_move(&obj->actor, LIVESTOCK_WALK_SPEED, cue);
    if (obj->actor.speed.vx == 0 && obj->actor.speed.vy == 0 && obj->actor.speed.vz == 0) {
        if (special != 0) obj->actor.flags &= ~ACTOR_FLAG_APPROACHING_PLAYER;
        obj->actor.action = ACTOR_ACTION_REST;
    } else {
        obj->actor.action = ACTOR_ACTION_MOVING;
    }
    obj->actor.flags &= ~ACTOR_FLAG_CUE_RUNNING;
    return 1;
}

/* Show the animal's sprite for its current action and facing, from the shared animal table. */
void livestock_frame_refresh(FarmActor *object) {
    u16 animation;
    u16 frame;

    farm_anim_lookup(object->action, object->facing, &animation, &frame);
    if (animation != object->sprite.animation) {
        anim_select_range(&object->sprite, animation);
    }
    if ((frame != 0) && (frame != object->sprite.frame)) {
        anim_set_frame(&object->sprite, frame);
    }
    farm_draw_actor(object);
    if ((object->action == ACTOR_ACTION_REST) || (object->action == ACTOR_ACTION_5)) {
        object->sprite.flags = object->sprite.flags | 0x100;
    }
}

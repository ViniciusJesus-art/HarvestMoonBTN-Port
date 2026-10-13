#ifndef EVENT_COMMANDS_H
#define EVENT_COMMANDS_H

/* The farm event commands and the values their scripts read. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "villager.h"

struct FarmTable;

/* Script step type 0x51 runs one of these commands. arg is the step's operand, often a
 * sub-command; x and y are the step's own values. */
enum EventCommand {
    EVENT_CMD_ESTATE_SLOTS = 1,        /* arg: EstateSlotsCommand */
    EVENT_CMD_WEDDING = 2,             /* arg: WeddingCommand */
    EVENT_CMD_CHILD_BORN = 3,
    EVENT_CMD_WIFE_SET_AFFECTION = 4,  /* arg is the new affection of the player's wife */
    EVENT_CMD_DAY_RECORD = 5,          /* arg: DayRecordCommand */
    EVENT_CMD_ESTATE_BITS = 6,    /* x == 0 sets the bits `arg` of estate.eventBits, else tests them */
    EVENT_CMD_UTENSIL_GET = 7,         /* arg: EventUtensil (a Utensil, or EVENT_UTENSIL_OFFER_9) */
    EVENT_CMD_SHOP_DIALOGUE = 8,       /* arg: EventShop; y == 0 starts the dialogue, else waits for it to end */
    EVENT_CMD_SET_FLAG_66 = 9,
    EVENT_CMD_POST_MESSAGE = 10,       /* posts a timed estate message: kind arg, message x, days y */
    EVENT_CMD_SET_VASE = 11,
    EVENT_CMD_FESTIVAL = 12,           /* arg: FestivalCommand */
    EVENT_CMD_ANIMAL_NAMING = 13,      /* arg: AnimalNamingCommand */
    EVENT_CMD_ANIMAL_DEATH = 14,       /* arg: AnimalDeathCommand */
    EVENT_CMD_ESTATE_GIFT = 15,        /* arg: EstateGift */
    EVENT_CMD_LIVESTOCK_ENTRANT = 16,  /* marks the livestock entered in a festival once its affection is 240, else 0 */
    EVENT_CMD_IS_EVENT = 17,           /* is the running event number `arg`? vars[x] = the number */
    EVENT_CMD_SPAWN_GATE = 18,         /* x != 0 sets animals.spawnGate to the place `arg` (y != 0: vars[arg]), else tests it */
    EVENT_CMD_SET_MILESTONE_RESULT_RUNNING = 19, /* arg != 0 sets EVENT_FLAG_MILESTONE_RESULT_RUNNING, else tests that it is not set */
    EVENT_CMD_ORDER_STATUS = 20,       /* arg: OrderStatusCommand */
    EVENT_CMD_LIVESTOCK_EXISTS = 21,
    EVENT_CMD_BUMP_COUNT_1 = 22,       /* bumps the day counter 1, capped at 15 */
    EVENT_CMD_ARM_TRIGGER = 23,
    EVENT_CMD_RELOAD_RESOURCE = 24,
    EVENT_CMD_SET_ESTATE_BIT21 = 25,
    EVENT_CMD_VILLAGER_ANIMATION = 26, /* vars[x] = the villager's animation */
    EVENT_CMD_SET_STATUS_BIT23 = 27,
    EVENT_CMD_NAP = 28,                /* arg 0 lies the player down, 1 gets him up */
    EVENT_CMD_CHILD_LEVEL = 29,        /* vars[arg] = the child's level; compares it with x under y */
    EVENT_CMD_VAR_ARITH = 30,
    EVENT_CMD_DOG_AFFECTION = 31,      /* changes the dog's affection, clamped to 0..255 */
};

enum EstateSlotsCommand {
    ESTATE_SLOTS_COUNT_FREE = 0,       /* vars[x] = how many slots are unclaimed */
    ESTATE_SLOTS_SET_CLAIMED = 1,      /* claims them (x == 1) or releases them */
};

/* Weddings are numbered as in RivalCouple; the player's own is -1, or CHARACTER_PLAYER
 * (the player's actor id) where the script names it. */
enum WeddingCommand {
    WEDDING_HOLD = 0,                  /* wedding x took place today */
    WEDDING_COUPLE_DONE = 1,           /* is couple x - 1 married? (y != 0: x is a variable) */
    WEDDING_PLAN = 2,                  /* x 0 plans rival couple y; x 2 lets the player marry the bride y */
    WEDDING_SET_TEXT_VARIANT = 3,      /* vars[x] - 1 is the family's address mode */
    WEDDING_BRIDE = 4,                 /* vars[x] = how the player's bride stands (-1 none, 0 engaged: EVENT_FLAG_SINGLE, 1 married), vars[y] = her id */
};

enum DayRecordCommand {
    DAY_RECORD_COLLAPSED = 0,          /* fatigue 100 and EVENT_FLAG_REST_REQUESTED: the player sleeps through to 5 AM */
    DAY_RECORD_WIFE_TALKED = 1,        /* the wife was spoken to today */
};

/* The minigame that FESTIVAL_RESULT reports on. */
enum Festival {
    FESTIVAL_HORSE_RACE = 0,
    FESTIVAL_CHICKEN = 1,
    FESTIVAL_TOMATO = 2,
    FESTIVAL_DOG_RACE = 3,
    FESTIVAL_SWIMMING = 4,
};

enum FestivalCommand {
    FESTIVAL_PRIZES_RESTOCK = 0,       /* restocks the prize shop */
    FESTIVAL_PRIZES_MODE = 1,          /* sets the prize shop's mode to x */
    FESTIVAL_PRIZES_EDIT = 2,          /* x != 0 begins the edit, else waits for it */
    FESTIVAL_PRIZES_MENU = 3,          /* x != 0 opens the medal exchange, else waits for it */
    FESTIVAL_PRIZES_ROW_EMPTY = 4,
    FESTIVAL_RESULT = 5,               /* x: Festival; the result of its minigame, vars[y] its second value */
    FESTIVAL_MINIGAME = 6,             /* x == 0: has the minigame ended? else restart it, then FESTIVAL_ENTRANT_INFO */
    FESTIVAL_ENTRANT_INFO = 7,         /* the entered hen: its affection and name for the messages */
    FESTIVAL_ENTRANT_MARK = 8,         /* flags the entered hen for the contest */
    FESTIVAL_TEAM_RESET = 9,           /* no team chosen */
    FESTIVAL_TEAM_SET = 10,            /* team x (y != 0: vars[x]) */
    FESTIVAL_TEAM_GET = 11,            /* vars[x] = the team */
    FESTIVAL_SCORE_TO_MEDALS = 12,     /* player.medals = the race score; the score is cleared */
};

enum AnimalNamingCommand {
    ANIMAL_NAMING_BEGIN = 0,           /* pause the farm and bring the animals' slots back */
    ANIMAL_NAMING_SLOT_READY = 1,      /* is the newborn of slot x (0, 1 livestock; 2, 3 chickens) loaded? */
    ANIMAL_NAMING_STEP = 2,            /* one frame of the name entry for slot x */
    ANIMAL_NAMING_END = 3,             /* unpause */
};

/* Which animal died (x: 0 cow, 1 sheep, 2 chicken) of what (y: 0 old age, 1 illness). */
enum AnimalDeathCommand {
    ANIMAL_DEATH_TEST = 0,             /* tests whether that kind of animal died of that cause */
    ANIMAL_DEATH_CLEAR = 1,            /* clears the flagged items, then runs the gift and livestock-entrant steps too */
};

enum EstateGift {
    ESTATE_GIFT_ITEM_83 = 0,
    ESTATE_GIFT_ROW97_BONUS = 1,
};

/* The actor ids the scripts pass besides the villager records and CHARACTER_PLAYER /
 * CHARACTER_ALL_VILLAGERS. */
enum EventActor {
    EVENT_ACTOR_FARM_ACTIONS = 0x2C,   /* hiding it clears the farm actions instead of setting a flag */
    EVENT_ACTOR_FLAG_ONLY = 0x2D,      /* resetting it only raises the no-target-action flag */
    EVENT_ACTOR_FIRST_HELPER = 0x32,   /* EVENT_ACTOR_FIRST_HELPER + n is helper n */
};

/* Which ending the game plays. 0 to 4 is the bride the player married (her index in the
 * wedding order Karen, Ann, Elli, Mary, Popuri). 5 to 9 is the bride who likes him best
 * if he married none (plus her index). The rest are the reasons the game found wanting,
 * in the order they are tested. */
enum EndingRoute {
    ENDING_ROUTE_MARRIED = 0,
    ENDING_ROUTE_SWEETHEART = 5,
    ENDING_ROUTE_DISCARDS = 10,     /* progress.estate.discardCount >= 10 */
    ENDING_ROUTE_FEW_FRIENDS = 12,  /* no other villager's friendship reaches 50 */
    ENDING_ROUTE_FEW_SHIPMENTS = 13,/* ten items shipped or fewer */
    ENDING_ROUTE_BARE_FIELD = 14,   /* under 6% of the field planted */
    ENDING_ROUTE_NO_SWEETHEART = 15,/* no bride reaches affection 500 */
    ENDING_ROUTE_DOG_UNLOVED = 16,  /* the dog's affection is under 6 */
};

/* Messages of text bank 1 that scripted scenes show by id. */
enum ContestMessage {
    CONTEST_JUDGE_LIGHT_SEASONING = 0x45C,   /* "The light seasoning lets the true flavors..." */
    CONTEST_JUDGE_RIGHT_SWEETNESS = 0x45D,   /* "Oh, this has just the right amount of sweetness" */
    CONTEST_JUDGE_NEVER_TASTED = 0x45E,      /* "Hmmmm, I've never tasted anything like this" */
    CONTEST_JUDGE_LIKE_A_PRO = 0x45F,        /* "Like a pro, as I expected" */
    CONTEST_JUDGE_COMPLEX_TASTE = 0x460,     /* "It looks simple but has a very complex taste" */
    CONTEST_JUDGE_WONDERFUL = 0x461,         /* "Oh, this is wonderful" (the player's dish) */
    CONTEST_JUDGE_JUST_AS_IT_IS = 0x462,     /* "Maybe a little more... very good just as it is" */
    /* The winner's announcement ("Now let's ask the Gourmet to judge this year's winner"); the
     * Gourmet's verdict is the next message. */
    CONTEST_WINNER_PLAYER = 0x472,
    CONTEST_WINNER_MANNA = 0x474,
    CONTEST_WINNER_ANNA = 0x476,
    CONTEST_WINNER_DOUG = 0x478,
    CONTEST_WINNER_ANN = 0x47A,
};

/* The ending scene's messages (text bank 1), picked by route. */
enum EndingMessage {
    ENDING_MESSAGE_CROWD = 0x7B7,            /* "A lot of people gathered here today for you" */
    ENDING_MESSAGE_CLIFF_SLIPS = 0x7B8,      /* "Hey, whoaaa (slip)... It's still cold in these parts": 8 lines, 4 per Cliff's whereabouts */
    ENDING_MESSAGE_RIVAL_PRAISE = 0x7C0,     /* "You did a great job. Our aims are different..." x3 */
    ENDING_MESSAGE_INVITATION = 0x7C3,       /* "There's somewhere I want to go with you": 5 brides, each twice */
};

/* The Tomato Festival comments (text bank 1). */
enum TomatoComment {
    TOMATO_COMMENT_LOSS = 0x526,             /* three per villager: "We lost, definitely because we had Kai" ... */
    TOMATO_COMMENT_TEAMS_MET = 0x548,        /* two per villager: "Our teamwork was the best!" ... */
    TOMATO_COMMENT_ABSENT = 0x70D,           /* one per villager: "Hey, why didn't you participate? It was fun." */
};

/* Text bank 0's snow variants of nine messages: 0x731 plus the message's place in the list. */
enum WeatherMessage {
    WEATHER_MESSAGE_SNOW_FIRST = 0x731,
};

/* The carpenters' order (estate.carpenterOrder: 0x13F coop, 0x140 barn, 0x141 full barn). */
enum OrderStatusCommand {
    ORDER_STATUS_PENDING = 0,          /* is one waiting? */
    ORDER_STATUS_READ = 1,             /* vars[x] = which */
    ORDER_STATUS_APPLY = 2,            /* build it and clear the order */
};

/* Base points of the rivals in the contest: one row per level (the year, capped at 4),
 * one column per rival. */
typedef struct RaceScores {
    s16 points[5][5];
} RaceScores;

s32 event_message_start(s16 channel, s16 message, s32 slot, s32 indirect);
s32 event_cmd_wait_frames(s16 duration, s16 counter_slot, s32 indirect, s32 pause_aware);
s32 event_cmd_set_position(s16 actor_id, s16 y, s32 x, s32 z);
s32 event_cmd_set_sprite(s16 actor, s16 animation, s32 frame, s32 immediate);
s32 event_cmd_set_actor_hidden(s16 actor, s16 hide, s32 indirect, s32 unused3);
s32 event_cmd_check_location(CharacterId villager, s16 value, s32 slot, s32 depth);
s32 event_cmd_set_target(s16 id, s16 absolute, s32 reloadTarget, s32 holdMusic);
s32 event_cmd_camera_glide(s16 mode, s16 y, s32 x, s32 z);
s32 event_cmd_player_lock(s16 unlock, s16 lock_input, s32 unused2, s32 unused3);
s32 event_cmd_talk_end(s16 slot, s16 face_mode, s32 mark_talked, s32 wait_delivery);
s32 event_cmd_face_player(s16 index, s16 indirect, s32 mode, s32 kind);
s32 event_cmd_test_var(s16 action, s16 mode, s32 context, s32 value);
s32 event_cmd_test_var_slot(s16 var, s16 element, s32 comparison, s32 slot);
s32 event_cmd_test_slot(s16 slot, s16 unused1, s32 comparison, s32 value);
s32 event_cmd_test_slots(s16 slot_a, s16 slot_b, s32 comparison, s32 unused3);
s32 event_cmd_test_var_range(s16 var, s16 element, s32 lower, s32 upper);
s32 event_cmd_test_slot_range(s16 flags, s16 index, s32 lower, s32 upper);
s32 event_cmd_set_var(s16 var, s16 element, s32 value, s32 mode);
s32 event_cmd_set_var_from_slot(s16 var, s16 element, s32 value_slot, s32 mode);
s32 event_cmd_slot_arith(s16 slot, s16 indirect, s32 operand, s32 operation);
s32 event_cmd_slot_arith_var(s16 var, s16 element, s32 slot, s32 operand);
s32 event_cmd_wait_control(s16 track, s16 unused1, s32 play, s32 unused3);
s32 event_cmd_set_mode(s16 mode, s16 enabled, s32 action, s32 unused3);
s32 event_cmd_select_mode(s16 requested_mask, s16 unused1, s32 unused2, s32 unused3);
s32 event_cmd_stop_music(s16 frames, s16 mode, s32 unused2, s32 unused3);
s32 event_cmd_fade(s16 mode, s16 value, s32 enabled, s32 alternate);
s32 event_cmd_event_bracket(s16 finish, s16 cues, s32 unused2, s32 unused3);
s32 event_cmd_has_room(s16 where, s16 which, s32 unused2, s32 unused3);
s32 event_item_can_take(s16 item_id, s16 where_arg, s32 which_arg, s32 flags);
s32 event_cue_issue(s16 id, s16 byTable, s32 queued, s32 unused3);
s32 event_item_store(s16 item, s16 byTable, s32 quiet, s32 notify);
s32 event_cmd_name_entry(s16 id, s16 restart, s32 slot, s32 unused3);
s32 event_cmd_player_near(s16 rangeX, s16 rangeZ, s32 x, s32 z);
s32 event_cmd_actor_reset(s16 id0, s16 id1, s32 id2, s32 id3);
s32 event_cmd_wait_time(s16 hourArg, s16 minuteArg, s32 flags, s32 unused3);
s32 event_cmd_enter_mode(s16 mode, s16 hour, s32 update, s32 flags);
s32 event_cmd_random(s16 range, s16 slot, s32 comparison, s32 value);
s32 event_cmd_set_dispatch(s16 mode, s16 unused1, s32 unused2, s32 unused3);
s32 event_cmd_door(s16 value, s16 action, s32 direction, s32 use_table);
s32 event_cmd_door_wait_moving(s16 place, s16 unused1, s32 unused2, s32 unused3);
s32 event_cmd_fill_screen(s16 front, s16 blue, s32 green, s32 red);
s32 event_cmd_cue_reset(CharacterId character, s16 cue_id_arg, s32 unused2, s32 unused3);
s32 event_cmd_change_gold(s16 reset, s16 result_slot, s32 delta, s32 indirect);
s32 event_cmd_scatter_crops(s16 mode, s16 keep, s32 slot, s32 unused3);
s32 event_cmd_stash_items(s16 restore, s16 keep, s32 unused2, s32 unused3);
s32 event_cmd_result_select(s16 indirect, s16 text_slot, s32 number, s32 unused3);
s32 event_cmd_chicken_slots(s16 mode, s16 unused1, s32 unused2, s32 unused3);
s32 event_cmd_horse_spawn(s16 remove, s16 unused1, s32 unused2, s32 unused3);
s32 event_cmd_queue_event(s16 event, s16 flags, s32 indirect, s32 immediate);
s32 event_cmd_load_record(s16 record, s16 indirect, s32 mode, s32 value);
s32 event_cmd_publish_item(s16 index, s16 y, s32 x, s32 z);
s32 event_cmd_item_enable(s16 index, s16 enable, s32 unused2, s32 unused3);
s32 event_cmd_publish_offset(s16 index, CharacterId character, s32 direction, s32 use_facing);
s32 event_cmd_get_item_id(s16 index, s16 slot, s32 unused2, s32 unused3);
s32 event_cmd_zone_entered(s16 unused0, s16 unused1, s32 unused2, s32 unused3);
s32 event_cmd_gift_reaction(s16 action, s16 slot, s32 from_slot, s32 unused);
s32 event_cmd_get_held_item(s16 slot, s16 which, s32 allow_substitute, s32 unused3);
s32 event_cmd_wait_text_bank(s16 bank, s16 unused1, s32 unused2, s32 unused3);
s32 event_cmd_set_position_vars(s16 actor_arg, s16 y_slot, s32 x_slot, s32 z_slot);
s32 event_cmd_set_player_position(s16 mode, s16 first, s32 second, s32 third);
s32 event_cmd_player_step(s16 phase, s16 edge, s32 value, s32 indirect);
s32 event_cmd_player_sprite(s16 action, s16 animation, s32 frame_arg, s32 immediate);
s32 event_cmd_set_facing(s16 facing_arg, s16 actor_arg, s32 unused2, s32 unused3);
s32 event_cmd_receive_item(s16 item, s16 indirect, s32 pose, s32 unused3);
s32 event_cmd_event_flag(s16 mode, s16 value, s32 flag, s32 unused3);
s32 event_cmd_item_has_reaction(s16 item, s16 indirect, s32 unused2, s32 unused3);
s32 event_cmd_actor_reset_vars(s16 first, s16 second, s32 third, s32 fourth);
s32 event_cmd_player_flags(s16 mode, s16 set, s32 mask, s32 wide);
s32 event_cmd_sound_play(s16 sound, s16 unused1, s32 unused2, s32 unused3);
s32 event_cmd_release_villager(CharacterId villager, s16 mode, s32 unused2, s32 unused3);
s32 event_cmd_camera_glide_vars(s16 mode, s16 y_slot, s32 x_slot, s32 z_slot);
s32 event_cmd_get_position(CharacterId character, s16 value4_slot, s32 value0_slot, s32 value8_slot);
s32 event_cmd_get_camera_centre(s16 first, s16 second, s32 third, s32 fourth);
s32 event_cmd_check_gift(s16 mode, s16 action, s32 result_slot, s32 unused3);
s32 event_cmd_skip_to_time(s16 hour, s16 minute, s32 immediate, s32 flags);
s32 event_cmd_fade_music(s16 fade_in, s16 frames, s32 start, s32 unused3);
s32 event_cmd_reset_schedule(CharacterId character, s16 unused1, s32 unused2, s32 unused3);
s32 event_cmd_result_store(s16 slot_arg, s16 unused1, s32 unused2, s32 unused3);
s32 event_cmd_return(s16 slot_arg, s16 unused1, s32 unused2, s32 unused3);
s32 event_cmd_dispatch(s16 op, s16 arg, s32 x, s32 y);
s32 event_cmd_walk_axis(s16 actor, s16 axis, s32 target, s32 animation);
s32 event_cmd_stop_actor(s16 actor, s16 animation, s32 frame, s32 mode);
s32 event_cmd_presentation_cues(s16 mode, s16 unused1, s32 unused2, s32 unused3);
s32 event_cmd_walk_actors(s16 step, s16 who, s32 unused2, s32 unused3);
s32 event_cmd_actor_walk_lane(CharacterId character, s16 from, s32 to, s32 lane);
s32 event_cmd_test_var_by_slot(s16 var, s16 slot, s32 comparison, s32 value);
s32 event_cmd_set_var_by_slot(s16 var, s16 slot, s32 value, s32 mode);
s32 event_cmd_set_villager_place(s16 slot, s16 mode, s32 use_table, s32 unused3);
s32 event_cmd_five_actor_dance(s16 var, s16 unused1, s32 unused2, s32 unused3);
s32 event_cmd_apply_var_table(s16 index, s16 extra, s32 operation, s32 pairOperation);
s32 event_cmd_object_show(s16 kind, s16 index, s32 action, s32 indirect);
s32 event_cmd_walk_route(s16 step_slot, s16 actor, s32 x_slot, s32 z_slot);
s32 event_cmd_is_birthday(s16 actor_arg, CharacterId character, s32 season_slot, s32 day_slot);
s32 event_cmd_store_levels(s16 house_slot, s16 barn_slot, s32 coop_slot, s32 greenhouse_slot);
s32 event_cmd_contest_stage(s16 var, s16 item, s32 score, s32 winner);
s32 event_cmd_adjust_present_villagers(s16 id, s16 arg, s32 operation, s32 alt_operation);
s32 event_cmd_select_livestock(s16 kind, s16 any_arg, s32 slot, s32 unused3);
s32 event_cmd_player_walk(s16 mode, s16 amount, s32 unused2, s32 unused3);
s32 event_cmd_set_pending_pair(s16 index, s16 value, s32 indirect, s32 unused3);
s32 event_cmd_pick_bride(s16 all, s16 out, s32 unused2, s32 unused3);
s32 event_cmd_affection_add(s16 result_slot, s16 helper_slot, s32 given_slot, s32 unused3);
s32 event_cmd_result_direction(s16 actor_arg, s16 indirect, s32 animation, s32 target_arg);
s32 event_actor_position(CharacterId character, s16 dy, s32 dx, s32 dz);
s32 event_cmd_villager_available(s16 villager_arg, s16 indirect, s32 unused2, s32 unused3);
s32 event_cmd_tomato_comment(s16 crop, s16 day, s32 watered, s32 out);
s32 event_cmd_fireworks_flash(s16 restart, s16 base, s32 unused2, s32 unused3);
s32 event_cmd_result_cell(CharacterId character, s16 slot_arg, s32 unused2, s32 unused3);
s32 event_cmd_camera_pull_back(s16 state, s16 counter, s32 unused, s32 reset);
s32 event_cmd_action_state(s16 mode, s16 slot, s32 unused2, s32 unused3);
s32 event_cmd_current_record(s16 route_slot, s16 second_slot, s32 third_slot, s32 fourth_slot);
s32 event_cmd_ending_text(s16 route_slot_arg, s16 second_slot_arg, s32 third_slot, s32 unused3);
s16 event_camera_record(void);
s32 event_vars_clear(s32 unused0, s32 unused1, s32 unused2, s32 unused3);
s32 event_items_take(s16 item, s16 where, s16 which, s32 count);
void rival_weddings_hold_due(void);
/* What the wedding offer check reports for a wedding. */
enum WeddingOfferState {
    WEDDING_OFFER_NO = 0,
    WEDDING_OFFER_YES = 1,
    WEDDING_OFFER_YES_VARIANT = 2,
};

enum WeddingOfferState wedding_offer_state(s16 mode);
void wedding_hold(s16 slot);
void event_cmd_init_mode(s16 mode, s16 kind);
s32 event_cmd_mode_check(s16 mode);
void *event_var_address(s32 index, s32 value, s32 *output);
void farm_item_rows_init(void);
void farm_events_load(struct FarmTable *source);
s32 event_triggers_run(void);
void event_trigger_mode_set(s32 arg);
u32 event_request_force(u32 arg, s32 value);

/* arg 0..7 is the Utensil whose bit of estate.utensils is set (as its index). */
enum EventUtensil {
    EVENT_UTENSIL_OFFER_9 = 8,         /* not a utensil: the ninth ending offer (bit 4 of estate.eventBits) */
};

/* event_cmd_fill_screen: its otTags slot for a back fill and a front fill, and the screen size. */
#define EVENT_FILL_SLOT_BACK   0
#define EVENT_FILL_SLOT_FRONT  5
#define EVENT_SCREEN_WIDTH     320
#define EVENT_SCREEN_HEIGHT    240

/* Limits the event commands test or cap. */
#define LIVESTOCK_ENTRANT_AFFECTION 240
#define BUMP_COUNT_1_MAX            15

s32 farm_nudge_axis(s16 index, s16 axis, s32 value, s32 amount);
s32 farm_nudge_cue(s16 actor, s16 axis, s32 value, s32 animation);

#endif /* EVENT_COMMANDS_H */

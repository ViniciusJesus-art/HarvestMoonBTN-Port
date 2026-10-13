/* Script commands the farm's events run: moving actors, showing messages, setting up
 * the farm's modes and checking the conditions that gate them; and nudging an actor's
 * limits.
 */
#include "abs.h"
#include <memory.h>
#include <libmath.h>
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "types.h"
#include "actor.h"
#include "anim.h"
#include "animals.h"
#include "cd_audio.h"
#include "chicken.h"
#include "clock.h"
#include "cue.h"
#include "dog.h"
#include "doors.h"
#include "estate.h"
#include "event_commands.h"
#include "event_flags.h"
#include "event_request.h"
#include "event_rules.h"
#include "event_script.h"
#include "family.h"
#include "farm_init.h"
#include "farm_load.h"
#include "farm_state.h"
#include "farm_tables.h"
#include "field.h"
#include "harvest_sprites.h"
#include "horse.h"
#include "house.h"
#include "item_storage.h"
#include "items.h"
#include "main.h"
#include "math_util.h"
#include "menu_window.h"
#include "milestones.h"
#include "name_entry.h"
#include "pad.h"
#include "pdasamp.h"
#include "picture_cells.h"
#include "player.h"
#include "prim.h"
#include "scene.h"
#include "scene_view.h"
#include "screen_fade.h"
#include "shop_dialogues.h"
#include "shop_page.h"
#include "shop_restock.h"
#include "sound.h"
#include "sound_request.h"
#include "system.h"
#include "text.h"
#include "text_bank.h"
#include "villager.h"
#include "weather.h"

/* A colour copied whole. */
typedef struct FarmFlashColour {
    s16 rgb[3];
} FarmFlashColour;

/* A scene's five actor ids, and the partner who stands in for each. */
typedef struct FarmActorIds {
    s16 ids[2][5];
} FarmActorIds;

/* How a farm variable is stored. Kinds 0 and 1 are both bytes and kinds 4 and 5 both
 * 32-bit words, read and written alike. A record kind (a position, a speed, a sprite)
 * is used through its address. */
enum FarmVariableKind {
    FARM_VAR_U8 = 0,
    FARM_VAR_U8_ALT = 1,
    FARM_VAR_U16 = 2,
    FARM_VAR_S16 = 3,
    FARM_VAR_U32 = 4,
    FARM_VAR_S32 = 5,
    FARM_VAR_RECORD = 6,
};

/* A farm variable that the event scripts read and write by number: where it lives in
 * FarmState, how it is stored, and its range. */
typedef struct FarmVariable {
    void *address;  /* the value, or the first record's */
    u8 kind;        /* a FarmVariableKind */
    s16 stride;     /* per-index step: sizeof(FarmVillager) or sizeof(FarmHelper) for a record row */
    s16 low;        /* low end of the range */
    s32 high;       /* high end of the range */
    s32 mask;       /* nonzero: the variable is this bit of the value */
} FarmVariable;

/* An item record index plus the position to give it. */
typedef struct FarmItemPlacement {
    s16 item;
    s16 x;
    s16 y;
} FarmItemPlacement;

#define NUDGE_CUE_AMOUNT 12

/* Move one component of the object's limit vector to `value`, if the value lies
 * on the axis's side of it (NUDGE_Z_UP and NUDGE_X_UP raise vz and vx,
 * NUDGE_Z_DOWN and NUDGE_X_DOWN lower them, NUDGE_Y_UP and NUDGE_Y_DOWN raise
 * and lower vy). Then report through the result vector: cue 4 with no amount
 * when it moved, otherwise the axis cue with twice `amount`. */
s32 farm_nudge_axis(s16 index, s16 axis, s32 value, s32 amount) {
    VECTOR *limits;
    VECTOR *result;
    s32 moved;

    limits = event_var_address(EVENT_VAR_VILLAGER_EVENT_POSITION, index, 0);
    result = event_var_address(EVENT_VAR_VILLAGER_EVENT_SPEED, index, 0);
    moved = 0;
    switch (axis) {
    case NUDGE_Z_UP:
        if (value >= limits->vz) {
            limits->vz = value;
            moved = 1;
        }
        break;
    case NUDGE_X_UP:
        if (value >= limits->vx) {
            limits->vx = value;
            moved = 1;
        }
        break;
    case NUDGE_Z_DOWN:
        if (limits->vz >= value) {
            limits->vz = value;
            moved = 1;
        }
        break;
    case NUDGE_X_DOWN:
        if (limits->vx >= value) {
            limits->vx = value;
            moved = 1;
        }
        break;
    case NUDGE_Y_UP:
        if (value >= limits->vy) {
            limits->vy = value;
            moved = 1;
        }
        break;
    case NUDGE_Y_DOWN:
        if (limits->vy >= value) {
            limits->vy = value;
            moved = 1;
        }
        break;
    }
    if (moved) {
        farm_vector_adjust(4, result, 0);
        return 1;
    }
    farm_vector_adjust((u16)axis, result, amount * 2);
    return 0;
}

s32 farm_nudge_cue(s16 index, s16 axis, s32 value, s32 animation) {
    event_cmd_set_sprite(index, animation, 0, 0);
    return farm_nudge_axis(index, axis, value, NUDGE_CUE_AMOUNT);
}

typedef void (*EventHandler)(s16, s16, s16);

typedef struct
{
    s32 x;
    s32 y;
    s32 z;
} Offset3;

/* Five candidate record ids; -1 = none. */
typedef struct FarmCandidates
{
    s16 ids[5];
} FarmCandidates;

typedef struct
{
    s16 first;
    s16 second;
    s16 third;
} ResultEntry;

/* A tile of the farm field, or of the 7 x 7 slot grid. */
typedef struct
{
    s16 kind;
    s16 id;
    u8 pad4[4];
} FarmTile;

/* The farm's event table: up to 100 entries, ended by the id 0xFFFF. */
typedef struct FarmTableEntry
{
    u16 marker;
    u8 pad02[0x24 - 0x02];
} FarmTableEntry;

typedef struct FarmTable
{
    FarmTableEntry entries[100];
} FarmTable;

static void rival_wedding_clear(s16 slot);
static s32 wedding_date_allowed(s16 kind, s16 season, s16 day);
static void event_var_update(s32 index, s32 element, s32 operand, s32 operation);
static s32 event_var_get(s32 index, s32 value);
static s32 event_var_clamp(s32 index, s32 unused, s32 value);
static s32 event_var_compare(s32 result, s32 value, s32 context);
static s32 event_var_arith(s32 *result, s32 value, u32 operation);
static s32 ending_best_friendship(void);
static s32 ending_married_bride(void);
static s32 ending_shipment_total(void);
static s32 ending_planted_field_percent(void);
static s32 field_tile_is_planted(s16 result);
static s32 ending_favourite_bride(void);
static void animal_naming_begin(void);
static s32 animal_naming_slot_ready(s16 mode);
static s32 animal_naming_step(s16 mode);
static void animal_naming_end(void);
static u16 farm_vec_heading(VECTOR *a, VECTOR *b);
static s16 gift_reaction_delta(s16 villager, s32 kind, s16 reaction);
static s32 farm_walk_route_step(CharacterId villager, s32 x, s32 z, s32 *step);
static s32 farm_image_load_cell(s16 key);
static s32 event_triggers_fire(FarmEventEntry *event, s32 count);
static s32 event_request_add(s32 event, s32 flags);
static s32 event_request_add_any(s32 event, s32 flags);
static s32 event_hour_window_match(s32 start_arg, s32 count_arg, s16 *value);
static s32 event_day_window_match(s32 start, s32 span, FarmDate *date);
static s32 event_location_match(u16 id, s32 flags);
static s32 event_var_in_range(s32 raw_variable, s32 raw_element, s32 raw_lower, s32 raw_upper, s32 flags);
static s32 event_wedding_role_match(s32 condition);
static s32 event_bond_match(s32 villager_arg, s32 amount_arg, s32 flags);
static s32 event_flags_match(s32 value, s32 mode);
static s32 event_prerequisites_match(s32 arg);
static s32 event_trigger_mode_match(s32 mode, s32 value);
static s32 event_availability_match(s32 condition);
static s32 event_not_done(s32 id);
static s32 event_calendar_match(s32 flags);
static s32 event_season_match(u32 flags);
static u32 event_weekday_match(u32 flags);
static s32 event_weather_match(s32 flags);
static void event_window_unpack(u32 packed, s16 *out);
static void event_flag_ids_unpack(u32 packed, u16 *out, s32 mode);
static s32 bride_of_index(u32 arg);

static VECTOR glide_centre;
/* The held item and the rucksack, put away by event_cmd_stash_items. */
static FarmStoredItem stashed_held_item;
static u32 stashed_rucksack[12];
static u32 scroll_lerp_x_from;
static u32 scroll_lerp_z_from;
static s16 scroll_lerp_y_from;
static u32 scroll_lerp_x_to;
static u32 scroll_lerp_z_to;
static s16 scroll_lerp_y_to;
/* The script variables: the event command's operands name them by index. */
#define SCRIPT_VAR_COUNT 20
/* The variable indices from EVENT_VAR_FLAGS_FIRST on are event flags: this many. */
#define EVENT_FLAG_VAR_COUNT 751

static ScriptVar script_vars[SCRIPT_VAR_COUNT];
/* Digits for text_format_number, ended by -1. */
static s16 number_digits[11];
/* The current farm record: the player's own, none, or a villager id. */
enum EventRecordId
{
    EVENT_RECORD_NONE = -1,
    EVENT_RECORD_PLAYER = -2,
};
static s16 event_current_record;
/* The name being entered, edited in place. */
static FarmPayload name_entry_buffer[4];
static u32 event_table_count;
static u32 event_trigger_pending;

/* The player's animation script: steps of animation, frame, holdFlag, ended by 9999. */
static s16 event_queued_record[7] = {0, 0, 0, 0, 0, 0, 9999};

static s16 villager_reaction_table[4][5] = {
    {9, 3, 1, -3, -9},
    {500, 300, 100, -500, -800},
    {3, 1, 0, -3, -9},
    {300, 100, 0, -500, -800},
};

#define WEATHER_MESSAGE_COUNT 9
#define MUSIC_FADE_DEFAULT_FRAMES 10

static s16 weather_message_ids[WEATHER_MESSAGE_COUNT] = {
    0x02B6, 0x02C2, 0x02D9, 0x030B, 0x039D, 0x03D6, 0x03EA, 0x06FF, 0x02DF};

/* Show a message on a channel. With `indirect` the message id comes from
 * script_vars. In snow the weather messages switch to the snow set. An accepted
 * request stores its choice in script_vars[slot]. */
s32 event_message_start(s16 channel, s16 message, s32 slot, s32 indirect)
{
    s32 result;
    s32 id;
    u32 i;

    result = 0;
    if (indirect != 0)
    {
        id = script_vars[message].value;
    }
    else
    {
        id = message;
    }
    if (game_state.world.weather == WEATHER_SNOW && channel == 0)
    {
        for (i = 0; i < WEATHER_MESSAGE_COUNT; i++)
        {
            if (id == weather_message_ids[i])
            {
                id = WEATHER_MESSAGE_SNOW_FIRST + i;
                break;
            }
        }
    }
    if (cd_audio_get_state() == CD_AUDIO_IDLE)
    {
        menu_request_open(channel, id);
        if (menu_get_phase() == TEXT_WINDOW_CLOSED)
        {
            result = 1;
            if (slot != -1)
            {
                script_vars[slot].value = menu_choice();
            }
        }
    }
    return result;
}

/* Finished once the counter in script_vars[counter_slot] passes half of `duration`
 * (read from script_vars when `indirect`). The counter only runs while the game is
 * not paused, if `pause_aware`. */
s32 event_cmd_wait_frames(s16 duration, s16 counter_slot, s32 indirect, s32 pause_aware)
{
    s32 elapsed;
    s32 result;

    elapsed = script_vars[counter_slot].value;
    result = 0;
    if (indirect != 0 ? script_vars[duration].value / 2 < elapsed : duration / 2 < elapsed)
    {
        result = 1;
    }
    if (pause_aware == 0 || !(game_state.globalFlags & GLOBAL_FLAG_PAUSED))
    {
        script_vars[counter_slot].value++;
    }
    return result;
}

/* Places an actor (a helper, or a villager, which is hidden first) at (x, y, z). */
s32 event_cmd_set_position(s16 actor_id, s16 y, s32 x, s32 z)
{
    s32 slot;
    VECTOR *position;

    slot = actor_id;
    if (slot >= EVENT_ACTOR_FIRST_HELPER)
    {
        position = &game_state.helpers[slot - EVENT_ACTOR_FIRST_HELPER].actor.position;
    }
    else
    {
        event_cmd_set_actor_hidden(slot, 0, 0, 0);
        position = event_var_address(EVENT_VAR_VILLAGER_EVENT_POSITION, slot, 0);
    }
    position->vx = x;
    position->vy = y;
    position->vz = z;
    return 1;
}

/* Sets the actor's animation and frame. Without `immediate` it returns 0 until the
 * animation has played through, then clears the player's event sprite flag and returns 1. */
s32 event_cmd_set_sprite(s16 actor, s16 animation, s32 frame, s32 immediate)
{
    AnimState *sprite;

    sprite = event_var_address(EVENT_VAR_VILLAGER_EVENT_SPRITE, actor, 0);
    if (immediate == 0)
    {
        if (sprite->animation != animation)
        {
            anim_select_range(sprite, animation);
            return 0;
        }
        if (sprite->flags & (ANIM_FLAG_ENDED | ANIM_FLAG_JUMPED))
        {
            game_state.player.stateFlags = game_state.player.stateFlags & ~PLAYER_FLAG_EVENT_SPRITE;
            return 1;
        }
        return 0;
    }
    if (sprite->animation != animation)
    {
        anim_select_range(sprite, animation);
    }
    anim_set_frame(sprite, frame);
    return 1;
}

/* Shows (hide 0) or hides a villager by its PRESENT flag. EVENT_ACTOR_FARM_ACTIONS
 * clears the farm actions instead. */
s32 event_cmd_set_actor_hidden(s16 actor, s16 hide, s32 indirect, s32 unused3)
{
    s32 actor_id;
    s16 *cell;

    if (actor != EVENT_ACTOR_FARM_ACTIONS)
    {
        if (indirect != 0)
        {
            actor_id = script_vars[actor].value;
        }
        else
        {
            actor_id = actor;
        }
        cell = event_var_address(EVENT_VAR_VILLAGER_FLAGS, actor_id, 0);
        if (hide == 0)
        {
            *cell = *cell | VILLAGER_FLAG_PRESENT;
        }
        else
        {
            *cell = *cell & ~VILLAGER_FLAG_PRESENT;
        }
    }
    else
    {
        farm_actions_clear(0);
    }
    return 1;
}

/* Checks where a villager is. CHARACTER_ALL_VILLAGERS checks every villager.
 * Optionally copies the villager's location into script_vars[slot]; if it equals
 * `value` (-1: the player's location), runs up to `depth` follow-up actions and
 * returns 1. */
s32 event_cmd_check_location(CharacterId villager, s16 value, s32 slot, s32 depth)
{
    CharacterId index;
    s32 expected;

    if (villager == CHARACTER_ALL_VILLAGERS)
    {
        for (index = 1; index < CHARACTER_COUNT; index++)
        {
            event_cmd_check_location(index, value, slot, depth);
        }
        return 1;
    }
    if (value == -1)
    {
        expected = game_state.player.place.location;
    }
    else
    {
        expected = value;
    }
    if (slot != -1)
    {
        script_vars[slot].value = game_state.villagers[villager].place.location;
    }
    if (game_state.villagers[villager].place.location == expected)
    {
        if (depth > 0)
        {
            event_cmd_actor_reset(villager, -1, -1, -1);
            if (depth >= 2)
            {
                event_cmd_set_actor_hidden(villager, 0, 0, 0);
                if (depth >= 3)
                {
                    event_cmd_set_sprite(villager, 0, game_state.villagers[villager].facing, 1);
                }
            }
        }
        return 1;
    }
    return 0;
}

/* The farm values the target ids -1 to -4 stand for (other ids are used as is). */
enum EventTarget
{
    EVENT_TARGET_FARM_LAYOUT = -1, /* the farm layout index, less the season */
    EVENT_TARGET_HOUSE = -2,       /* house level * 4 + 80 */
    EVENT_TARGET_BARN = -3,        /* the barn's upgrade and full flags */
    EVENT_TARGET_COOP = -4,        /* the coop's built flag */
};

/* Sets the transition target. A non-negative id is used as is; -1..-4 pick a
 * value from the farm's state. Unless `absolute`, it is relative to the season.
 * The last two arguments set the two farm flags. */
s32 event_cmd_set_target(s16 id, s16 absolute, s32 reloadTarget, s32 holdMusic)
{
    s32 value;
    u32 flags;

    switch (id)
    {
    case EVENT_TARGET_FARM_LAYOUT:
        value = farm_layout_index() - game_state.progress.clock.date.season;
        break;
    case EVENT_TARGET_HOUSE:
        value = game_state.progress.estate.houseLevel * 4 + 0x50;
        break;
    case EVENT_TARGET_COOP:
        if (game_state.progress.estate.estateFlags & ESTATE_FLAG_COOP_BUILT)
        {
            value = 120;
        }
        else if (event_flag_get(EVENT_FLAG_COOP_LEVEL_1) == 1)
        {
            value = 116;
        }
        else
        {
            value = 112;
        }
        break;
    case EVENT_TARGET_BARN:
        flags = game_state.progress.estate.estateFlags;
        if (flags & ESTATE_FLAG_BARN_UPGRADED)
        {
            value = 108;
        }
        else if (flags & ESTATE_FLAG_BARN_FULL)
        {
            value = 104;
        }
        else if (event_flag_get(EVENT_FLAG_BARN_LEVEL_1) == 1)
        {
            value = 100;
        }
        else
        {
            value = 96;
        }
        break;
    default:
        value = id;
        break;
    }
    if (absolute == 0)
    {
        farm_area.mode.transitionTicks = value + game_state.progress.clock.date.season;
    }
    else
    {
        farm_area.mode.transitionTicks = value;
    }
    farm_area.mode.eventMode = FARM_MODE_TARGET_LOAD;
    if (reloadTarget == 0)
    {
        game_state.statusFlags &= ~STATUS_FLAG_TARGET_RELOAD;
    }
    else
    {
        game_state.statusFlags |= STATUS_FLAG_TARGET_RELOAD;
    }
    if (holdMusic != 0)
    {
        game_state.statusFlags |= STATUS_FLAG_MUSIC_HOLD;
    }
    else
    {
        game_state.statusFlags &= ~STATUS_FLAG_MUSIC_HOLD;
    }
    return 1;
}

static s32 camera_glide_active = 0;

/* The speeds of event_cmd_camera_glide. Each step is the distance moved per frame at
 * the start of the glide (the step divides the distance to the target), so a larger
 * value is quicker. */
enum CameraGlideSpeed
{
    CAMERA_GLIDE_JUMP = -1,
    CAMERA_GLIDE_SLOWEST = -2, /* step 5 */
    CAMERA_GLIDE_NORMAL = 0,   /* step 24 */
    CAMERA_GLIDE_FAST = 1,     /* step 36 */
    CAMERA_GLIDE_FASTER = 2,   /* step 48 */
    CAMERA_GLIDE_FASTEST = 3,  /* step 72 */
};

/* Glide the farm camera target toward (x, y, z) at the speed `mode` picks
 * (-1 jumps straight there). Returns 1 once there. */
s32 event_cmd_camera_glide(s16 mode, s16 y, s32 x, s32 z)
{
    s32 step;
    s32 delta[3];
    s32 distance;
    s32 steps;
    s32 arrived;

    event_current_record = EVENT_RECORD_NONE;
    game_state.globalFlags |= GLOBAL_FLAG_CAMERA_BUSY;
    event_cmd_player_lock(0, 0, 0, 0);
    step = 24;
    switch (mode)
    {
    case CAMERA_GLIDE_JUMP:
        farm_area.mode.scroll.centre.vy = y;
        farm_area.mode.scroll.centre.vx = x;
        farm_area.mode.scroll.centre.vz = z;
        return 1;
    case CAMERA_GLIDE_SLOWEST:
        step = 5;
        break;
    case CAMERA_GLIDE_NORMAL:
        step = 24;
        break;
    case CAMERA_GLIDE_FAST:
        step = 36;
        break;
    case CAMERA_GLIDE_FASTER:
        step = 48;
        break;
    case CAMERA_GLIDE_FASTEST:
        step = 72;
        break;
    }
    if (camera_glide_active == 0)
    {
        camera_glide_active = 1;
        glide_centre.vx = farm_area.mode.scroll.centre.vx;
        glide_centre.vy = farm_area.mode.scroll.centre.vy;
        glide_centre.vz = farm_area.mode.scroll.centre.vz;
    }
    delta[1] = abs(y - glide_centre.vy);
    delta[0] = abs(x - glide_centre.vx);
    delta[2] = abs(z - glide_centre.vz);
    distance = SquareRoot0(delta[0] * delta[0] + delta[2] * delta[2]);
    if (distance < delta[1])
    {
        distance = delta[1];
    }
    steps = distance / step;
    if (steps == 0)
    {
        if (glide_centre.vz == z && glide_centre.vx == x && glide_centre.vy == y)
        {
            camera_glide_active = 0;
            arrived = 1;
        }
        else
        {
            arrived = 0;
            glide_centre.vz = z;
            glide_centre.vx = x;
            glide_centre.vy = y;
            farm_area.mode.scroll.centre.vz = glide_centre.vz;
            farm_area.mode.scroll.centre.vx = glide_centre.vx;
            farm_area.mode.scroll.centre.vy = glide_centre.vy;
        }
    }
    else
    {
        glide_centre.vz += (z - glide_centre.vz) / steps;
        glide_centre.vx += (x - glide_centre.vx) / steps;
        glide_centre.vy += (y - glide_centre.vy) / steps;
        arrived = 0;
        farm_area.mode.scroll.centre.vz = glide_centre.vz;
        farm_area.mode.scroll.centre.vx = glide_centre.vx;
        farm_area.mode.scroll.centre.vy = glide_centre.vy;
    }
    return arrived;
}

/* Ends the player's scene lock (unlock != 0) or re-locks them. Unlocking also clears the
 * scene and scripted flags, and sets the fatigue latch away from locations 1 and 6.
 * lock_input 0 gives input back. */
s32 event_cmd_player_lock(s16 unlock, s16 lock_input, s32 unused2, s32 unused3)
{
    s16 status;

    if (unlock != 0)
    {
        game_state.player.stateFlags &= ~(PLAYER_FLAG_SCENE_CHANGE | PLAYER_FLAG_SCRIPTED);
        game_state.globalFlags &= ~GLOBAL_FLAG_CAMERA_BUSY;
        status = game_state.player.place.location;
        if (status != 1 && status != 6)
        {
            game_state.player.latches |= PLAYER_LATCH_FATIGUE_LOCKED;
        }
        else
        {
            game_state.player.latches &= ~PLAYER_LATCH_FATIGUE_LOCKED;
        }
    }
    else
    {
        game_state.player.stateFlags |= PLAYER_FLAG_SCRIPTED;
    }
    if (lock_input == 0)
    {
        game_state.player.stateFlags &= ~PLAYER_FLAG_NO_INPUT;
    }
    return 1;
}

/* Ends a talk with the villager the player is talking to (-1 when none). Stores their id in
 * `slot` and, once the delivery (if waited for) is done, optionally faces them and marks them
 * talked to. Returns 1 once the talk is over. */
s32 event_cmd_talk_end(s16 slot, s16 face_mode, s32 mark_talked, s32 wait_delivery)
{
    s16 villager;
    s32 result;

    result = 0;
    villager = game_state.player.talkVillager;
    if (villager != -1)
    {
        if (slot != -1)
        {
            script_vars[slot].value = villager;
        }
        result = 1;
        if (wait_delivery != 0)
        {
            result = event_flag_get(EVENT_FLAG_DELIVERING_TO_VILLAGER);
        }
        if (result == 1)
        {
            if (face_mode != 0)
            {
                event_cmd_face_player(villager, 0, face_mode - 1, 0);
            }
            if (mark_talked != 0)
            {
                event_cmd_set_var(EVENT_VAR_VILLAGER_FLAG_0400, villager, 1, 0);
            }
        }
    }
    if (wait_delivery == 0 || result == 1)
    {
        game_state.player.talkVillager = -1;
        event_flag_set(EVENT_FLAG_DELIVERING_TO_VILLAGER, 0);
    }
    event_cmd_player_lock(result == 0, 1, 0, 0);
    return result;
}

/* The kinds of event_cmd_face_player: a marker (a helper record), or a villager. */
enum FacePlayerKind
{
    FACE_PLAYER_MARKER = 1,
    FACE_PLAYER_VILLAGER = 2,
};

/* Turns an actor to face the player. The actor is `index`, or the one whose id
 * is in script_vars[index]. Kind 1 turns a marker and only stores the heading;
 * other kinds turn an actor and apply `mode`. For kind 2, an actor in state 5..9
 * is forced to mode 5, and one in state 10 to mode 11 with heading 0. */
s32 event_cmd_face_player(s16 index, s16 indirect, s32 mode, s32 kind)
{
    s32 target;
    s32 heading;
    void *point;
    u8 state;
    s16 animation;

    animation = mode;

    if (indirect != 0)
    {
        target = script_vars[index].value;
    }
    else
    {
        target = index;
    }
    if (kind == FACE_PLAYER_MARKER)
    {
        point = &game_state.helpers[target].actor.position;
    }
    else
    {
        point = &game_state.villagers[target].ePosition;
    }
    heading = farm_vec_heading(&game_state.player.position, point);
    switch (kind)
    {
    case FACE_PLAYER_MARKER:
        game_state.helpers[target].actor.facing = heading;
        break;
    case FACE_PLAYER_VILLAGER:
        state = game_state.villagers[target].sprite80.animation;
        if ((u32)(state - 5) < 5)
        {
            animation = 5;
        }
        else if (state == 10)
        {
            animation = 11;
            heading = 0;
        }
        /* fallthrough */
    default:
        event_cmd_set_sprite(target, animation, heading, 1);
        event_cmd_set_actor_hidden(target, 0, 0, 0);
        break;
    }
    return 1;
}

s32 event_cmd_test_var(s16 var, s16 element, s32 comparison, s32 value)
{
    s32 result;

    result = event_var_get(var, element);
    if (var == EVENT_VAR_WEDDING_BRIDE)
    {
        if (event_flag_get(EVENT_FLAG_SINGLE) == 1 && value != -1)
        {
            return 0;
        }
    }
    return event_var_compare(result, value, comparison);
}

s32 event_cmd_test_var_slot(s16 var, s16 element, s32 comparison, s32 slot)
{
    s32 value = event_var_get(var, element);
    return event_var_compare(value, script_vars[slot].value, comparison);
}

s32 event_cmd_test_slot(s16 slot, s16 unused1, s32 comparison, s32 value)
{
    return event_var_compare(script_vars[slot].value, value, comparison);
}

s32 event_cmd_test_slots(s16 slot_a, s16 slot_b, s32 comparison, s32 unused3)
{
    return event_var_compare(script_vars[slot_a].value, script_vars[slot_b].value, comparison);
}

s32 event_cmd_test_var_range(s16 var, s16 element, s32 lower, s32 upper)
{
    s32 result = 0;
    s32 below = lower;
    s32 above = upper;
    s32 value;
    value = event_var_get(var, element);
    below = value < below;
    if (!below)
        result = !(above < value);
    return result;
}

/* Flag bit 1 says the lower bound is a script variable slot, bit 2 the upper. */
s32 event_cmd_test_slot_range(s16 flags, s16 index, s32 lower, s32 upper)
{
    s32 value;
    s32 lo;
    s32 hi;
    s32 result;
    value = script_vars[index].value;
    if (flags & 1)
        lo = script_vars[lower].value;
    else
        lo = lower;
    if (flags & 2)
        hi = script_vars[upper].value;
    else
        hi = upper;
    result = 0;
    if (value >= lo)
        result = (hi >= value);
    return result;
}

s32 event_cmd_set_var(s16 var, s16 element, s32 value, s32 mode)
{
    event_var_update(var, element, value, mode);
    return 1;
}

s32 event_cmd_set_var_from_slot(s16 var, s16 element, s32 value_slot, s32 mode)
{
    event_var_update(var, element, script_vars[value_slot].value, mode);
    return 1;
}

s32 event_cmd_slot_arith(s16 slot, s16 indirect, s32 operand, s32 operation)
{
    s32 *cell;
    s32 value;
    cell = &script_vars[slot].value;
    if (indirect != 0)
        value = script_vars[operand].value;
    else
        value = operand;
    event_var_arith(cell, value, operation);
    return 1;
}

s32 event_cmd_slot_arith_var(s16 var, s16 element, s32 slot, s32 operand)
{
    s32 value = event_var_get(var, element);
    event_var_arith(&script_vars[slot].value, value, operand);
    return 1;
}

/* Plays track `track` (play != 0). Otherwise returns 1 once the CD has stopped. Waits
 * while a menu is open. */
s32 event_cmd_wait_control(s16 track, s16 unused1, s32 play, s32 unused3)
{
    s32 result;
    if (game_state.system.menuState != MENU_STATE_IDLE)
    {
        return 1;
    }
    result = 0;
    if (play != 0)
    {
        cd_audio_play_track(track);
    }
    else
    {
        if (cd_audio_get_state() == CD_AUDIO_IDLE)
        {
            result = 1;
        }
    }
    return result;
}

/* Fades out first when `enabled`. Then, if the farm is already in `mode`, runs the select
 * (action 1) or stop (action -1) of the music and returns 1. Otherwise it queues the mode
 * change and returns 0. */
s32 event_cmd_set_mode(s16 mode, s16 enabled, s32 action, s32 unused3)
{
    if (enabled != 0)
    {
        event_cmd_fade(1, 48, 1, 0);
    }
    if (farm_area.mode.currentMode != mode)
    {
        game_state.pendingMode = mode;
    }
    else
    {
        if (action == 1)
        {
            event_cmd_select_mode(0, 0, 0, 0);
        }
        else if (action == -1)
        {
            event_cmd_stop_music(0, 2, 0, 0);
        }
        return 1;
    }
    return 0;
}

/* Pick the sound mask for the farm action mode, and start the reverb with it. */
s32 event_cmd_select_mode(s16 requested_mask, s16 unused1, s32 unused2, s32 unused3)
{
    s16 mask;
    s32 selected;
    if (requested_mask == 0)
    {
        if ((farm_area.mode.data->head.flags & AREA_FLAG_BIT12) != 0)
        {
            selected = farm_area.mode.currentMode;
            if (selected == 5)
                goto mode_default;
            if (selected == 6)
                goto mode_default;
            if (selected == 7)
                goto mode_default;
            if (selected == 8)
                goto mode_default;
            if (selected == 9)
                goto mode_default;
            if (selected != 0xE)
                goto mode_special;
            mask = 0x7F;
            goto mode_done;
        mode_default:
            mask = 0x7F;
            goto mode_done;
        mode_special:
            mask = 0x3F;
        }
        else
        {
            mask = 0x7F;
        }
    }
    else
    {
        mask = requested_mask;
    }
mode_done:
    sound_update();
    sound_seq_start_reverb(0, 0, farm_area.mode.currentMode, mask);
    return 1;
}

/* Mode 0 fades the music out over `frames` (the default if 0). Other modes stop the first
 * sequence once the fader is idle (mode 2 stops it at once). */
s32 event_cmd_stop_music(s16 frames, s16 mode, s32 unused2, s32 unused3)
{
    s32 result = 0;
    if (mode == 0)
    {
        s32 fade_frames;
        if (frames != 0)
        {
            fade_frames = frames;
        }
        else
        {
            fade_frames = MUSIC_FADE_DEFAULT_FRAMES;
        }
        sound_fade_out(fade_frames);
    }
    else
    {
        enum SoundFaderMode fader = sound_fader_mode();
        if (fader == SOUND_FADER_IDLE || mode == 2)
        {
            sound_seq_stop_first(0);
            result = 1;
        }
    }
    return result;
}

/* Starts a fade in (odd mode) or out, then returns 0. With `enabled` 0 it reports whether
 * the fade has settled. Mode 2 also locks the player. */
s32 event_cmd_fade(s16 mode, s16 value, s32 enabled, s32 alternate)
{
    s32 first_mode;
    s32 second_mode;
    EventHandler handler;

    if (enabled != 0)
    {
        first_mode = 0;
        switch (mode)
        {
        case 0:
        case 1:
            second_mode = 2;
            break;
        case 2:
            event_cmd_player_lock(0, 0, 0, 0);
        case 3:
            first_mode = 1;
            second_mode = 2;
            break;
        default:
            second_mode = 2;
        }
        if (alternate != 0)
            second_mode = 1;
        if (mode & 1)
            handler = fade_start_in;
        else
            handler = fade_start_out;
        handler(value, first_mode, second_mode);
        return 0;
    }
    return fade_is_settled() == 1;
}

/* Opens the event (finish 0) or closes it once the menu is idle, returning 1 then. Cues
 * (when `cues` is not -1) are presentation mode 2 when closing or when `cues` is 2, else 0. */
s32 event_cmd_event_bracket(s16 finish, s16 cues, s32 unused2, s32 unused3)
{
    s32 result;
    s16 mode;

    result = 0;
    mode = 0;
    if (finish == 0)
    {
        event_flag_set(EVENT_FLAG_EVENT_ACTIVE, 1);
    }
    else if (game_state.system.menuState == MENU_STATE_IDLE)
    {
        event_flag_set(EVENT_FLAG_EVENT_ACTIVE, 0);
        result = 1;
        mode = 2;
    }
    if (cues != -1)
    {
        if (cues == 2)
        {
            mode = 2;
        }
        event_cmd_presentation_cues(mode, 0, 0, 0);
    }
    return result;
}

s32 event_cmd_has_room(s16 where, s16 which, s32 unused2, s32 unused3)
{
    return event_item_can_take(0, where, which, 0);
}

s32 event_item_can_take(s16 item_id, s16 where_arg, s32 which_arg, s32 flags)
{
    s16 item = item_id;
    s16 where = where_arg;
    s16 which;
    if (flags & 2)
    {
        item = script_vars[item].half;
    }
    which = which_arg;
    flags &= 1;
    return event_items_take(item, where, which, flags) != 0;
}

/* Issues a cue (an item the player is shown). Its id is `id`, or script_vars[id]
 * when byTable is set. Unqueued, it is shown at once. Queued, it goes into the
 * first free slot of the cue queue, which holds 2^(rucksackLevel + 1) cues.
 * Returns 0 when event_item_can_take refuses, else 1. */
s32 event_cue_issue(s16 id, s16 byTable, s32 queued, s32 unused3)
{
    FarmStoredItem cue;
    FarmStoredItem *slot;
    s32 capacity;
    s16 i;
    u16 cueId;

    if (byTable != 0)
    {
        cueId = script_vars[id].half;
    }
    else
    {
        cueId = id;
    }
    capacity = math_int_pow(2, game_state.player.rucksackLevel + 1);
    if (queued == 0)
    {
        if (event_item_can_take(0, 1, 0, 0) == 0)
        {
            return 0;
        }
        stored_item_set(&cue, (s16)cueId, 0, 1);
        player_show_item(&cue);
        return 1;
    }
    if (event_item_can_take(0, 1, -1, 0) == 0)
    {
        return 0;
    }
    slot = game_state.player.items;
    i = 0;
    while (i < capacity && slot->id != 0)
    {
        i++;
        slot++;
    }
    picture_load_to_vram((s16)cueId);
    stored_item_set(slot, (s16)cueId, 0, 1);
    return 1;
}

/* Stores an item: its id is `item`, or script_vars[item] when byTable is set.
 * It goes into the held tool slot if that is free, else the first free slot of
 * player.tools, else one of the spill slots in the toolbox. `quiet` picks the
 * event_item_can_take `which` (1, else 2). The player is shown the item when notify
 * is set and it did not spill. Returns 0 when event_item_can_take refuses or nothing
 * is free, else 1. */
s32 event_item_store(s16 item, s16 byTable, s32 quiet, s32 notify)
{
    FarmStoredItem *slot;
    FarmStoredItem *p;
    s32 capacity;
    s32 i;
    u32 j;
    s32 index;
    s32 spilled;
    s16 id;
    s16 column;
    s32 result;

    if (event_item_can_take(0, 0, quiet ? 1 : 2, 0) == 0)
    {
        return 0;
    }
    if (byTable != 0)
    {
        id = script_vars[item].half;
    }
    else
    {
        id = item;
    }
    slot = 0;
    spilled = 0;
    index = -1;
    if (game_state.player.held[HELD_TOOL].id == 0)
    {
        slot = &game_state.player.held[HELD_TOOL];
    }
    else
    {
        /* index: -1 for the held tool, else the tools slot the item went into */
        index = 0;
        capacity = math_int_pow(2, game_state.player.rucksackLevel + 1);
        p = game_state.player.tools;
        i = 0;
        while (i < capacity)
        {
            if (p->id == 0)
            {
                slot = p;
                break;
            }
            i++;
            p++;
            index++;
        }
        if (slot == 0)
        {
            p = game_state.progress.estate.toolboxItems;
            for (j = 0; j < ITEM_STORE_SLOTS; j++)
            {
                if (p->id == 0)
                {
                    slot = p;
                    spilled = 1;
                    break;
                }
                p++;
            }
        }
    }
    result = 0;
    if (slot != 0)
    {
        column = 0;
        if (id >= 1 && id <= 25)
        {
            column = (id - 1) % 5;
        }
        if (!spilled)
        {
            picture_load_to_vram(id);
        }
        stored_item_set(slot, id, column, 1);
        if (!spilled && notify != 0)
        {
            player_show_tool(&game_state.player, index != -1, index, notify == 1 ? 10 : 60);
        }
        result = 1;
    }
    return result;
}

/* The name entry's id: the horse's name, the child's name (family.childName), or
 * 1 to 3, which are not identified yet. */
enum NameEntryId
{
    EVENT_NAME_ID_HORSE = 0,
    EVENT_NAME_ID_1 = 1,
    EVENT_NAME_ID_2 = 2,
    EVENT_NAME_ID_3 = 3,
    EVENT_NAME_ID_CHILD = 4,
};

/* The name entry for `id`. Returns 1 once it is finished. */
s32 event_cmd_name_entry(s16 id, s16 restart, s32 slot, s32 unused3)
{
    s32 finished;
    s32 status;

    finished = 0;
    status = name_entry_step();
    switch (status)
    {
    case NAME_STEP_CANCELLED:
        if (slot != -1)
        {
            /* 1 = cancelled, 0 = accepted */
            script_vars[slot].value = 1;
        }
        finished = 1;
        break;
    case NAME_STEP_ACCEPTED:
        if (slot != -1)
        {
            script_vars[slot].value = 0;
        }
        switch (id)
        {
        case EVENT_NAME_ID_HORSE:
            *(FarmPayload *)game_state.animals.horse.actor.name = name_entry_buffer[0];
            break;
        case EVENT_NAME_ID_CHILD:
            *(FarmPayload *)game_state.family.childName = name_entry_buffer[0];
            break;
        }
        finished = 1;
        break;
    case NAME_STEP_IDLE:
        switch (id)
        {
        case EVENT_NAME_ID_HORSE:
        case EVENT_NAME_ID_CHILD:
            name_entry_start_no_cancel((u16 *)&name_entry_buffer[0]);
            break;
        case EVENT_NAME_ID_1:
        case EVENT_NAME_ID_2:
        case EVENT_NAME_ID_3:
            if (restart == 0)
            {
                name_entry_start_cancellable((u16 *)&name_entry_buffer[0]);
            }
            else
            {
                name_entry_start_no_cancel((u16 *)&name_entry_buffer[0]);
            }
            break;
        }
        break;
    }
    return finished;
}

/* Is the player within (rangeX, rangeZ) of (x, z)? */
s32 event_cmd_player_near(s16 rangeX, s16 rangeZ, s32 x, s32 z)
{
    s32 result = 0;
    s16 px = game_state.player.position.vx;
    s16 pz = game_state.player.position.vz;

    if (abs(px - x) < rangeX)
    {
        if (abs(pz - z) < rangeZ)
        {
            result = 1;
        }
    }
    return result;
}

/* Is `id` a villager record (1..CHARACTER_COUNT-1), not the player or an actor id? */
static inline s32 villager_record_valid(s32 id)
{
    return (u32)(id - 1) < CHARACTER_COUNT - 1;
}

/* Resets up to four villagers: their position becomes their home position, their
 * speed and animation are cleared, and their cue is reset. CHARACTER_ALL_VILLAGERS
 * resets every villager, four at a time. EVENT_ACTOR_FLAG_ONLY sets a farm flag
 * instead. Always returns 1. */
s32 event_cmd_actor_reset(s16 id0, s16 id1, s32 id2, s32 id3)
{
    s32 ids[4];
    s32 i;
    s32 id;
    FarmVillager *villager;

    switch (id0)
    {
    case CHARACTER_ALL_VILLAGERS:
        for (i = 1; i < CHARACTER_COUNT; i += 4)
        {
            event_cmd_actor_reset(i, i + 1, i + 2, i + 3);
        }
        break;
    case EVENT_ACTOR_FLAG_ONLY:
        game_state.statusFlags |= STATUS_FLAG_NO_TARGET_ACTION;
        break;
    default:
        ids[0] = id0;
        ids[1] = id1;
        ids[2] = id2;
        ids[3] = id3;
        for (i = 0; i < 4; i++)
        {
            id = ids[i];
            if (villager_record_valid(id))
            {
                villager = &game_state.villagers[id];
                villager->ePosition = villager->position;
                villager->sprite.animation = 0;
                villager->eSpeed.vx = 0;
                villager->eSpeed.vy = 0;
                villager->eSpeed.vz = 0;
                villager->flags |= VILLAGER_FLAG_IN_EVENT;
                villager->placeFlags &= ~(VILLAGER_PLACE_CURSOR_ALT | VILLAGER_PLACE_NO_MARKER);
                cue_reset(&villager->cue);
            }
        }
        break;
    }
    return 1;
}

/* Has the farm clock reached (hour, minute)? Hours before 6 count as the next
 * day. Flag bit 1 reads the hour from script_vars, bit 2 the minute. With hour
 * -1, checks for 6:00 sharp. Once reached, the talk and delivery state is cleared. */
s32 event_cmd_wait_time(s16 hour, s16 minute, s32 flags, s32 unused3)
{
    s16 hour_value;
    s16 minute_value;
    s16 target;
    s16 now;
    s32 reached;

    now = game_state.progress.clock.time.hm.hour;
    if (hour == -1)
    {
        if (now == 6 && game_state.progress.clock.time.hm.minute == 0 && fade_is_settled() == 0)
        {
            return 1;
        }
        return 0;
    }
    if (flags & 1)
    {
        hour_value = script_vars[hour].half;
    }
    else
    {
        hour_value = hour;
    }
    target = hour_value;
    if (flags & 2)
    {
        minute_value = script_vars[minute].half;
    }
    else
    {
        minute_value = minute;
    }
    if (hour_value < 6)
    {
        target = hour_value + 24;
    }
    if (now < 6)
    {
        now += 24;
    }
    reached = 0;
    if (target < now)
    {
        reached = 1;
    }
    else if (target == now && game_state.progress.clock.time.hm.minute >= minute_value)
    {
        reached = 1;
    }
    if (reached == 1)
    {
        game_state.player.talkVillager = -1;
        event_flag_set(EVENT_FLAG_DELIVERING_TO_VILLAGER, 0);
    }
    return reached;
}

enum EventEnterMode
{
    EVENT_ENTER_OVERNIGHT = 1,      /* the overnight change: the player sleeps */
    EVENT_ENTER_SKIP_IMMEDIATE = 2, /* skips the clock to the hour at once */
    EVENT_ENTER_SKIP_CHECKED = 3,   /* skips it, refused when that time has passed */
};

/* The mode event_cmd_release_villager takes to let a villager stay present and in the event. */
enum EventReleaseMode
{
    EVENT_RELEASE_KEEP_PRESENT = 2,
};

/* Switches the farm into `mode`: 1 is the overnight change, 2 and 3 skip the
 * clock to `hour`. Then refreshes the panels and flags the state for update. */
s32 event_cmd_enter_mode(s16 mode, s16 hour, s32 update, s32 flags)
{
    switch (mode)
    {
    case EVENT_ENTER_OVERNIGHT:
        farm_area.mode.eventMode = FARM_MODE_SLEEP;
        game_state.player.stateFlags |= PLAYER_FLAG_SCENE_CHANGE;
        event_flag_set(EVENT_FLAG_OVERNIGHT, 1);
        break;
    case EVENT_ENTER_SKIP_IMMEDIATE:
        event_cmd_skip_to_time(hour, 0, 1, 0);
        break;
    case EVENT_ENTER_SKIP_CHECKED:
        event_cmd_skip_to_time(hour, 0, 0, 0);
        break;
    }
    /* Flag bit 1 releases every villager (mode 2 keeps them present); bit 0 is the
     * presentation cue flag. */
    if (flags & 2)
    {
        event_cmd_release_villager(CHARACTER_ALL_VILLAGERS, EVENT_RELEASE_KEEP_PRESENT, 0, 0);
    }
    event_cmd_presentation_cues(2, flags & 1, 0, 0);
    if (update != 0)
    {
        game_state.statusFlags |= STATUS_FLAG_SKIP_ANIMAL_UPDATE;
    }
    game_state.requests.state = FARM_REQUEST_FINISH;
    return 1;
}

/* Rolls a number below `range` into script_vars[slot] (-1 skips) and compares it with `value`. */
s32 event_cmd_random(s16 range, s16 slot, s32 comparison, s32 value)
{
    s32 result = rand_range((u16)range);
    if (slot != -1)
    {
        script_vars[slot].value = result;
    }
    return event_var_compare(result, value, comparison);
}

/* Zeroes the background colour of both draw environments. */
static inline void clear_colours(FrameState *fb)
{
    fb->drawEnv.r0 = 0;
    fb->drawEnv.g0 = 0;
    fb->drawEnv.b0 = 0;
    fb++;
    fb->drawEnv.r0 = 0;
    fb->drawEnv.g0 = 0;
    fb->drawEnv.b0 = 0;
}

/* The modes event_cmd_set_dispatch selects; each is the farm mode it sets. */
enum EventDispatch
{
    EVENT_DISPATCH_MINIGAME_1 = 0, /* FARM_MODE_MINIGAME_1 */
    EVENT_DISPATCH_MINIGAME_5 = 1, /* FARM_MODE_MINIGAME_5, and the frame colours are cleared */
    EVENT_DISPATCH_MINIGAME_2 = 2, /* FARM_MODE_MINIGAME_2 */
    EVENT_DISPATCH_MINIGAME_3 = 3, /* FARM_MODE_MINIGAME_3 */
    EVENT_DISPATCH_MINIGAME_4 = 4, /* FARM_MODE_MINIGAME_4 */
    EVENT_DISPATCH_OVERNIGHT = 5,  /* FARM_MODE_OVERNIGHT */
};

/* Reset farm dispatch state and select the requested farm mode. */
s32 event_cmd_set_dispatch(s16 mode, s16 unused1, s32 unused2, s32 unused3)
{
    game_state.progress.entrySubStep = 0;
    switch (mode)
    {
    case EVENT_DISPATCH_MINIGAME_1:
        farm_area.mode.eventMode = FARM_MODE_MINIGAME_1;
        break;
    case EVENT_DISPATCH_MINIGAME_5:
        farm_area.mode.eventMode = FARM_MODE_MINIGAME_5;
        clear_colours(frame_records);
        break;
    case EVENT_DISPATCH_MINIGAME_2:
        farm_area.mode.eventMode = FARM_MODE_MINIGAME_2;
        break;
    case EVENT_DISPATCH_MINIGAME_3:
        farm_area.mode.eventMode = FARM_MODE_MINIGAME_3;
        break;
    case EVENT_DISPATCH_MINIGAME_4:
        farm_area.mode.eventMode = FARM_MODE_MINIGAME_4;
        break;
    case EVENT_DISPATCH_OVERNIGHT:
        farm_area.mode.eventMode = FARM_MODE_OVERNIGHT;
        game_state.player.stateFlags |= PLAYER_FLAG_SCENE_CHANGE;
        break;
    }
    return 1;
}

/* The state of a door entry, as door_entry_for_event returns it. */
enum DoorState
{
    DOOR_NONE = 0,
    DOOR_SHUT = 1,
    DOOR_OPENING = 2,
    DOOR_HELD_OPEN = 3,
    DOOR_CLOSING = 4,
};

/* The actions of event_cmd_door. The door tests read the state of the farm's door
 * entry (a DoorState). */
enum EventDoorAction
{
    EVENT_DOOR_PAIR_LOOKUP = 0, /* look up the door pair */
    EVENT_DOOR_IS_SHUT = 1,
    EVENT_DOOR_IS_OPEN_WAIT = 2, /* held open */
    EVENT_DOOR_IS_OPENING = 3,   /* opening */
    EVENT_DOOR_WAIT_MOVING = 4,  /* moving: any state but none and shut */
};

/* Resolves a farm value and runs the requested farm action. */
s32 event_cmd_door(s16 value, s16 action, s32 direction, s32 use_table)
{
    s16 resolved;
    u16 result;

    if (use_table != 0)
    {
        resolved = script_vars[value].half;
    }
    else
    {
        resolved = value;
    }
    result = 1;
    switch (action)
    {
    case EVENT_DOOR_PAIR_LOOKUP:
        event_pending_pair_lookup(resolved, direction, 0, 1);
        break;
    case EVENT_DOOR_IS_SHUT:
        result = door_entry_for_event(resolved) == DOOR_SHUT;
        break;
    case EVENT_DOOR_IS_OPEN_WAIT:
        result = door_entry_for_event(resolved) == DOOR_HELD_OPEN;
        break;
    case EVENT_DOOR_IS_OPENING:
        result = door_entry_for_event(resolved) == DOOR_OPENING;
        break;
    case EVENT_DOOR_WAIT_MOVING:
        result = 0;
        if (door_entry_for_event(resolved) != DOOR_NONE)
        {
            result = door_entry_for_event(resolved) != DOOR_SHUT;
        }
        break;
    }
    return result;
}

s32 event_cmd_door_wait_moving(s16 place, s16 unused1, s32 unused2, s32 unused3)
{
    return event_cmd_door(place, EVENT_DOOR_WAIT_MOVING, 0, 0);
}

/* Adds a full-screen rectangle of colour (red, green, blue) to the front or back ordering table. */
s32 event_cmd_fill_screen(s16 front, s16 blue, s32 green, s32 red)
{
    s32 index = EVENT_FILL_SLOT_BACK;
    if (front != 0)
    {
        index = EVENT_FILL_SLOT_FRONT;
    }
    prim_add_rect(&current_frame->otTags[0][index], 0, 0, EVENT_SCREEN_WIDTH, EVENT_SCREEN_HEIGHT,
                  ((red & 0xFF) | ((green & 0xFFFF) << 8) | ((blue & 0xFF) << 16)), 1);
    return 1;
}

/* The frames a cue set here counts down before a later call can reset it. */
#define EVENT_CUE_COUNTDOWN_FRAMES 30

/* Resets the cue of the player or a villager. Returns 1 when a finished cue was
 * reset, else 0 (still playing, or just set). */
s32 event_cmd_cue_reset(CharacterId character, s16 cue_id, s32 unused2, s32 unused3)
{
    ItemCue *cue;

    if (character == CHARACTER_PLAYER)
    {
        cue = &game_state.player.cue;
    }
    else
    {
        cue = &game_state.villagers[character].cue;
    }
    if (cue->id != -1)
    {
        if (cue->phase == 0)
        {
            cue_reset(cue);
            return 1;
        }
        return 0;
    }
    cue_set(cue, cue_id, EVENT_CUE_COUNTDOWN_FRAMES);
    return 0;
}

/* Takes `delta` gold (or script_vars[delta]) through estate_gold_change, with `reset`. The
 * result goes to `result_slot`. Returns 1 when the result is 0. */
s32 event_cmd_change_gold(s16 reset, s16 result_slot, s32 delta, s32 indirect)
{
    s32 amount;
    s16 result;

    if (indirect != 0)
        amount = script_vars[delta].value;
    else
        amount = delta;
    result = estate_gold_change(amount, reset);
    if (result_slot != -1)
        script_vars[result_slot].value = result;
    return result == 0;
}

/* Mode 1 of event_cmd_scatter_crops plants this many crops. */
#define SCATTER_PLANT_COUNT 50

/* The six crops event_cmd_scatter_crops plants, by their index in its table. */

enum ScatterCrop
{
    SCATTER_CROP_MOONDROP = 0,
    SCATTER_CROP_PINK_CAT = 1,
    SCATTER_CROP_FLOWER_178 = 2,
    SCATTER_CROP_TOY_FLOWER = 3,
    SCATTER_CROP_ORANGE_CUP = 4,
    SCATTER_CROP_FLOWER_179 = 5,
};

/* Scatters wild crops. Mode 0 (unless `keep`) turns the weeds in the field back
 * into soil. Mode 1 plants 50 of six crops on random free tiles of the field or,
 * while the greenhouse has room, the greenhouse. Mode 2 plants the first crop that fits,
 * greenhouse then field, and records the tile's stage in script_vars[slot].
 * Returns 0 when nothing fits, otherwise 1. */
s32 event_cmd_scatter_crops(s16 mode, s16 keep, s32 slot, s32 unused3)
{
    FarmFieldTile request;
    s16 open[2][6];
    s16 col;
    s16 row;
    s32 field;
    s32 greenhouse;
    s32 pick;
    s32 i;
    s32 placed = 0;
    s32 found;

    if (mode == 0)
    {
        if (keep == 0)
        {
            FarmFieldTile *tile;
            s32 y;
            s32 x;

            for (y = 0; y < FIELD_ROWS; y++)
            {
                for (x = 0; x < FIELD_COLS; x++)
                {
                    tile = &game_state.fieldTiles[y].rows[x];
                    if (tile->value == ITEM_WEEDS)
                    {
                        tile->value = ITEM_SOIL;
                        tile->kind = 0;
                        tile->unused5 = 0;
                        tile->growthFlags = 0;
                    }
                }
            }
        }
    }
    else if (mode == 1)
    {
        s32 placed;

        for (i = 0; i < 6; i++)
        {
            open[0][i] = 1;
            open[1][i] = 1;
        }
        greenhouse = game_state.progress.estate.greenhouseLevel;
        placed = 0;
        while (placed < SCATTER_PLANT_COUNT)
        {
            if (greenhouse != 0)
            {
                for (i = 0;; i++)
                {
                    if (i >= 6)
                    {
                        field = 1;
                        greenhouse = 0;
                        break;
                    }
                    if (open[0][i] != 0)
                    {
                        field = rand_range(2) != 0;
                        break;
                    }
                }
            }
            else
            {
                for (i = 5; i >= 0; i--)
                {
                    open[0][i] = 0;
                }
                field = 1;
            }
            for (i = 0; i < 12; i++)
            {
                if (open[i / 6][i % 6] != 0)
                {
                    break;
                }
            }
            if (i >= 12)
            {
                return 0;
            }
            while (1)
            {
                pick = rand_range(6);
                if (open[field][pick] != 0)
                {
                    switch (pick)
                    {
                    case SCATTER_CROP_MOONDROP:
                        request.value = ITEM_MOONDROP_SEEDS;
                        break;
                    case SCATTER_CROP_PINK_CAT:
                        request.value = ITEM_PINK_CAT_SEEDS;
                        break;
                    case SCATTER_CROP_FLOWER_178:
                        request.value = ITEM_BLUE_MAGIC_RED_PLANT;
                        break;
                    case SCATTER_CROP_TOY_FLOWER:
                        request.value = ITEM_TOY_FLOWER_SEEDS;
                        break;
                    case SCATTER_CROP_ORANGE_CUP:
                        request.value = ITEM_ORANGE_CUP_SEEDS;
                        break;
                    case SCATTER_CROP_FLOWER_179:
                        request.value = ITEM_RED_MAGIC_RED_PLANT;
                        break;
                    }
                    request.kind = crop_grown_stage(request.value);
                    found = field_find_free_tile(&col, &row, field, &request, 2);
                    break;
                }
            }
            if (found)
            {
                FarmFieldTile *tile;

                if (field == 1)
                {
                    tile = &game_state.fieldTiles[row].rows[col];
                }
                else
                {
                    tile = &game_state.slots7x7[row].rows[col];
                }
                placed++;
                tile->kind = 0;
                tile->unused5 = 0;
                tile->growthFlags = 0;
                tile->value = tile->ground;
            }
            else
            {
                open[field][pick] = 0;
            }
        }
    }
    else if (mode == 2)
    {
        s16 col;
        s16 row;
        s32 pass;

        for (pass = 0; pass < 2; pass++)
        {
            for (pick = 0; pick < 6; pick++)
            {
                field = pass != 0;
                switch (pick)
                {
                case SCATTER_CROP_MOONDROP:
                    request.value = ITEM_MOONDROP_SEEDS;
                    break;
                case SCATTER_CROP_PINK_CAT:
                    request.value = ITEM_PINK_CAT_SEEDS;
                    break;
                case SCATTER_CROP_FLOWER_178:
                    request.value = ITEM_BLUE_MAGIC_RED_PLANT;
                    break;
                case SCATTER_CROP_TOY_FLOWER:
                    request.value = ITEM_TOY_FLOWER_SEEDS;
                    break;
                case SCATTER_CROP_ORANGE_CUP:
                    request.value = ITEM_ORANGE_CUP_SEEDS;
                    break;
                case SCATTER_CROP_FLOWER_179:
                    request.value = ITEM_RED_MAGIC_RED_PLANT;
                    break;
                }
                request.kind = crop_grown_stage(request.value);
                if (field_find_free_tile(&col, &row, field, &request, 2))
                {
                    FarmFieldTile *tile;

                    if (field == 1)
                    {
                        tile = &game_state.fieldTiles[row].rows[col];
                    }
                    else
                    {
                        tile = &game_state.slots7x7[row].rows[col];
                    }
                    script_vars[slot].value = field_tile_item_stage(tile);
                    tile->kind = 0;
                    tile->unused5 = 0;
                    tile->growthFlags = 0;
                    tile->value = tile->ground;
                    return 1;
                }
            }
        }
        return 0;
    }
    if (mode == 1 && placed < SCATTER_PLANT_COUNT)
    {
        return 0;
    }
    return 1;
}

/* Puts the held item and the rucksack away (`restore` 0), or brings them back.
 * Whatever is held is put down first. With `keep`, a saved item that allows it
 * is held again; otherwise it is shown to the player. */
s32 event_cmd_stash_items(s16 restore, s16 keep, s32 unused2, s32 unused3)
{
    s32 i, j;
    u16 current;
    FarmStoredItem *items;

    if (restore == 0)
    {
        stored_item_copy(&stashed_held_item, &game_state.player.held[HELD_ITEM]);
        memcpy(stashed_rucksack, game_state.player.items, sizeof stashed_rucksack);
        if (game_state.player.held[HELD_ITEM].id != 0)
        {
            game_state.player.stateFlags &= ~(PLAYER_FLAG_SHOWING_ITEM | PLAYER_FLAG_CARRY_BIG_ITEM | PLAYER_FLAG_CARRY_ITEM);
            picture_release_item(game_state.player.held[HELD_ITEM].id);
            stored_item_clear(&game_state.player.held[HELD_ITEM]);
            player_reset_pose();
        }
        items = game_state.player.items;
        for (i = 0; i < 8; i++, items++)
        {
            picture_release_item(items->id);
            stored_item_clear(items);
        }
        player_reset_pose();
    }
    else
    {
        current = game_state.player.held[HELD_ITEM].id;
        if (stashed_held_item.id != 0)
        {
            if (current != ITEM_NOTHING)
            {
                game_state.player.stateFlags &= ~(PLAYER_FLAG_SHOWING_ITEM | PLAYER_FLAG_CARRY_BIG_ITEM | PLAYER_FLAG_CARRY_ITEM);
                picture_release_item(game_state.player.held[HELD_ITEM].id);
                stored_item_clear(&game_state.player.held[HELD_ITEM]);
                player_reset_pose();
                switch (current)
                {
                case ITEM_ADULT_DOG:
                case ITEM_YOUNG_DOG:
                    game_state.animals.dog.actor.flags &= ~ACTOR_FLAG_HELD;
                    dog_place(0);
                    break;
                case ITEM_DOG_BALL:
                    game_state.runs.records[ESTATE_RUN_STAMPS][ESTATE_STAMP_DOG_BALL].recordFlags |= ESTATE_RECORD_ENABLED;
                    estate_trigger_stamp_second();
                    break;
                case ITEM_BASKET:
                    game_state.runs.records[ESTATE_RUN_STAMPS][ESTATE_STAMP_BASKET].recordFlags |= ESTATE_RECORD_ENABLED;
                    estate_trigger_stamp_first();
                    break;
                }
            }
            if (keep == 0 || (game_state.itemInfo[stashed_held_item.id].itemFlags & 0x100))
            {
                player_show_item(&stashed_held_item);
            }
            else
            {
                game_state.player.held[HELD_ITEM] = stashed_held_item;
                picture_load_to_vram(game_state.player.held[HELD_ITEM].id);
                game_state.player.stateFlags |= PLAYER_FLAG_SHOWING_ITEM | PLAYER_FLAG_CARRY_ITEM;
                game_state.player.stateFlags &= ~PLAYER_FLAG_BIT14;
                player_reset_pose();
            }
        }
        memcpy(game_state.player.items, stashed_rucksack, sizeof stashed_rucksack);
        items = game_state.player.items;
        for (j = 0; j < 8; j++, items++)
        {
            picture_load_to_vram(items->id);
        }
    }
    return 1;
}

/* Formats a number into the text slot, skipping the leading padding. */
s32 event_cmd_result_select(s16 indirect, s16 text_slot, s32 number, s32 unused3)
{
    s16 *entry;
    s32 value;
    s32 count;

    if (indirect != 0)
    {
        value = script_vars[number].value;
    }
    else
    {
        value = number;
    }
    *(s16 *)text_format_number(value, number_digits, 10) = -1;
    entry = number_digits;
    count = 0;
    while (count < 11 && *entry == 0x0FFD)
    {
        count++;
        entry++;
    }
    text_set_source((u16)text_slot, entry);
    return 1;
}

/* The contest chickens are named from these offsets into the chicken names. */
#define CONTEST_CHICKEN_COUNT 5

static s32 chicken_name_offsets[CONTEST_CHICKEN_COUNT] = {
    offsetof(TextNames, chicken_names.cocka),
    offsetof(TextNames, chicken_names.doodle),
    offsetof(TextNames, chicken_names.doo),
    offsetof(TextNames, chicken_names.bukbuk),
    offsetof(TextNames, chicken_names.begaak),
};

/* Mode 0 creates five contest chickens, each named from the chicken names.
 * Otherwise the contest chickens are cleared. */
s32 event_cmd_chicken_slots(s16 mode, s16 unused1, s32 unused2, s32 unused3)
{
    s32 i;

    if (mode == 0)
    {
        FarmChicken *chicken;

        for (i = 0; i < CONTEST_CHICKEN_COUNT; i++)
        {
            chicken = farm_action_create(ITEM_CHICKEN, -1);
            if (chicken != 0)
            {
                chicken->actor.flags |= ACTOR_FLAG_CONTEST_ENTRANT;
                memcpy(chicken->actor.name, TEXT_AT_OFFSET(text_bank_starts.names + chicken_name_offsets[i]), sizeof(chicken->actor.name));
            }
        }
    }
    else
    {
        for (i = 0; i < MAX_CHICKENS; i++)
        {
            if (game_state.animals.chickens[i].actor.flags & ACTOR_FLAG_CONTEST_ENTRANT)
            {
                chicken_clear(&game_state.animals.chickens[i]);
            }
        }
    }
    return 1;
}

/* Shows the horse (remove 0) or clears its record. */
s32 event_cmd_horse_spawn(s16 remove, s16 unused1, s32 unused2, s32 unused3)
{
    if (remove == 0)
    {
        farm_action_create(ITEM_PONY, -1);
    }
    else
    {
        horse_clear_record(&game_state.animals.horse);
    }
    return 1;
}

/* Queues farm event `event` (or script_vars[event]) with `flags`, then enters the event
 * mode. */
s32 event_cmd_queue_event(s16 event, s16 flags, s32 indirect, s32 immediate)
{
    s32 value = event;
    if (indirect)
    {
        value = script_vars[event].value;
    }
    event_request_force((u16)value, immediate == 0);
    event_cmd_enter_mode(0, 0, 0, flags);
    return 1;
}

/* Glides the camera to farm record `record` and makes it the current record.
 * `record` is a villager id, or CHARACTER_PLAYER for the player; with `indirect`
 * it is read from script_vars. Mode 0 makes it current without a glide. Asking for the
 * current record succeeds at once; a bad index fails with 0. Mode 2 glides without
 * making the record current. */
s32 event_cmd_load_record(s16 record, s16 indirect, s32 mode, s32 speed)
{
    s32 selected;
    s32 status;
    VECTOR *source;

    if (indirect != 0)
    {
        selected = script_vars[record].value;
    }
    else
    {
        selected = record;
    }
    if (selected == CHARACTER_PLAYER)
    {
        selected = EVENT_RECORD_PLAYER;
    }
    status = 1;
    game_state.globalFlags |= GLOBAL_FLAG_CAMERA_BUSY;
    if (selected != EVENT_RECORD_NONE && mode != 0)
    {
        if (event_current_record == selected)
        {
            return 1;
        }
        if (selected == EVENT_RECORD_PLAYER)
        {
            source = &game_state.player.position;
        }
        else if (villager_record_valid(selected))
        {
            source = &game_state.villagers[selected].ePosition;
        }
        else
        {
            return 0;
        }
        status = event_cmd_camera_glide(speed, source->vy, source->vx, source->vz);
    }
    if (status == 1 && mode != 2)
    {
        event_current_record = selected;
        if (selected == EVENT_RECORD_PLAYER)
        {
            game_state.globalFlags &= ~GLOBAL_FLAG_CAMERA_BUSY;
        }
    }
    return status;
}

/* Publishes an item at a position, stamped with the current location, and enables it. */
s32 event_cmd_publish_item(s16 index, s16 y, s32 x, s32 z)
{
    FarmEstateRecord *record;

    record = &game_state.runs.records[ESTATE_RUN_ITEMS][index];
    record->location = farm_area.mode.place.location;
    record->position.vy = y;
    record->position.vx = x;
    record->position.vz = z;
    record->recordFlags |= ESTATE_RECORD_ENABLED;
    return 1;
}

/* Set or clear bit 0 (the enable) of one item's flags. */
s32 event_cmd_item_enable(s16 index, s16 enable, s32 unused2, s32 unused3)
{
    FarmEstateRecord *record;

    record = &game_state.runs.records[ESTATE_RUN_ITEMS][index];
    if (enable == 0)
    {
        record->recordFlags &= ~ESTATE_RECORD_ENABLED;
    }
    else
    {
        record->recordFlags |= ESTATE_RECORD_ENABLED;
    }
    return 1;
}

/* Publishes an item 100 units to one side of a villager: the side the player
 * faces when `use_facing` is set, else side `direction` (0..3). */
s32 event_cmd_publish_offset(s16 index, CharacterId character, s32 direction, s32 use_facing)
{
    FarmEstateRecord *record;
    VECTOR *source;
    Offset3 offset;
    u16 side;

    record = &game_state.runs.records[ESTATE_RUN_ITEMS][index];
    record->location = farm_area.mode.place.location;
    source = &game_state.villagers[character].ePosition;
    record->recordFlags |= ESTATE_RECORD_ENABLED;
    if (use_facing)
    {
        side = farm_vec_heading(&game_state.player.position, source);
    }
    else
    {
        side = direction;
    }
    switch (side)
    {
    case FACING_DOWN:
        offset.x = 0;
        offset.y = -100;
        offset.z = -100;
        break;
    case FACING_LEFT:
        offset.x = -100;
        offset.y = -100;
        offset.z = 0;
        break;
    case FACING_UP:
        offset.x = 0;
        offset.y = -100;
        offset.z = 100;
        break;
    case FACING_RIGHT:
        offset.x = 100;
        offset.y = -100;
        offset.z = 0;
        break;
    }
    record->position.vy = source->vy + offset.y;
    record->position.vx = source->vx + offset.x;
    record->position.vz = source->vz + offset.z;
    return 1;
}

/* Stores the item id of the farm item record at `index` in a slot. */
s32 event_cmd_get_item_id(s16 index, s16 slot, s32 unused2, s32 unused3)
{
    FarmEstateRecord *record;

    record = &game_state.runs.records[ESTATE_RUN_ITEMS][index];
    script_vars[slot].value = record->itemId;
    return 1;
}

/* Whether the farm is being left by a zone (the exit load mode). */
s32 event_cmd_zone_entered(s16 unused0, s16 unused1, s32 unused2, s32 unused3)
{
    return farm_area.mode.eventMode == FARM_MODE_EXIT_LOAD;
}

/* Stores the villager's reaction to the held gift (1-based) in a slot, 0 if there is no
 * reaction. With nothing held the slot is left alone and 0 is returned. */
s32 event_cmd_gift_reaction(s16 villager, s16 slot, s32 from_slot, s32 unused)
{
    s32 target;
    s32 value;
    s16 substitute;
    s32 result;

    if (from_slot)
        target = script_vars[villager].value;
    else
        target = villager;
    value = game_state.player.held[HELD_ITEM].id;
    if (value == 0)
        return 0;
    substitute = stored_item_contents(&game_state.player.held[HELD_ITEM]);
    if (substitute != 0)
        value = substitute;
    result = item_villager_reaction(value, target);
    if (result < 7)
    {
        if (result >= 0)
        {
            script_vars[slot].value = result + 1;
            return 1;
        }
    }
    script_vars[slot].value = 0;
    return 1;
}

/* Stores the id of the held tool (`which` HELD_TOOL) or held item in `slot`, and returns 0
 * when it is empty. With `allow_substitute` a held gift stands for its contents. */
s32 event_cmd_get_held_item(s16 slot, s16 which, s32 allow_substitute, s32 unused3)
{
    s16 value;
    s16 substitute;

    if (which == HELD_TOOL)
    {
        value = game_state.player.held[HELD_TOOL].id;
        substitute = game_state.player.held[HELD_TOOL].modifier;
    }
    else
    {
        value = game_state.player.held[HELD_ITEM].id;
        substitute = game_state.player.held[HELD_ITEM].modifier;
    }
    if (allow_substitute && value == ITEM_GIFT)
        value = substitute;
    if (!value)
        return 0;
    if (slot != -1)
        script_vars[slot].value = value;
    return 1;
}

/* Returns 1 once text bank `bank` is the current one. Bank -1 clears the current bank. */
s32 event_cmd_wait_text_bank(s16 bank, s16 unused1, s32 unused2, s32 unused3)
{
    s32 current = text_get_bank();
    if (bank == current)
        return 1;
    if (bank == -1)
        text_clear_bank();
    return 0;
}

s32 event_cmd_set_position_vars(s16 actor, s16 y_slot, s32 x_slot, s32 z_slot)
{
    VECTOR *position;

    event_cmd_set_actor_hidden(actor, 0, 0, 0);
    position = event_var_address(EVENT_VAR_VILLAGER_EVENT_POSITION, actor, 0);

    position->vx = script_vars[x_slot].value;
    position->vy = script_vars[y_slot].value;
    position->vz = script_vars[z_slot].value;
    return 1;
}

enum PlayerPositionMode
{
    PLAYER_POSITION_SET = 0,        /* the given x, y, z */
    PLAYER_POSITION_FROM_VARS = 1,  /* the three script variables */
    PLAYER_POSITION_HOUSE_DOOR = 2, /* the house door, facing 0 */
};

s32 event_cmd_set_player_position(s16 mode, s16 y, s32 x, s32 z)
{
    VECTOR *position;
    position = &game_state.player.position;
    switch (mode)
    {
    case PLAYER_POSITION_SET:
        position->vy = y;
        position->vx = x;
        position->vz = z;
        break;
    case PLAYER_POSITION_FROM_VARS:
        position->vy = script_vars[y].value;
        position->vx = script_vars[x].value;
        position->vz = script_vars[z].value;
        break;
    case PLAYER_POSITION_HOUSE_DOOR:
        house_door_position(position);
        event_cmd_set_facing(0, 0, 0, 0);
        break;
    }
    player_reset_pose();
    game_state.player.place = farm_area.mode.place;
    return 1;
}

/* The edges event_cmd_player_step holds the player to. */
enum PlayerStepEdge
{
    PLAYER_STEP_RAISE_Z = 0,
    PLAYER_STEP_RAISE_X = 1,
    PLAYER_STEP_LOWER_Z = 2,
    PLAYER_STEP_LOWER_X = 3,
};

/* Moves one coordinate of the player's position to `value`, if that keeps it on
 * the right side of its limit. `edge` picks the coordinate and direction:
 * 0 raises z, 1 raises x, 2 lowers z, 3 lowers x. A move the wrong way is refused:
 * the player is pushed back toward the limit instead, and on odd `phase` they also
 * press cross. Bit 0 / bit 1 of `indirect` read `value` / `edge` from script_vars.
 * Returns 1 if the position moved. */
s32 event_cmd_player_step(s16 phase, s16 edge, s32 value, s32 indirect)
{
    s32 side;
    s32 moved;
    s16 odd;
    VECTOR *position;

    if (indirect & 1)
    {
        value = script_vars[value].value;
    }
    side = (indirect & 2) ? script_vars[edge].value : edge;
    position = &game_state.player.position;
    moved = 0;
    odd = phase % 2;
    game_state.player.stateFlags |= PLAYER_FLAG_NO_INPUT;
    switch (side)
    {
    case PLAYER_STEP_RAISE_Z:
        if (value < position->vz)
        {
            game_state.player.scriptHeld = PAD_DOWN;
            game_state.player.scriptPressed = PAD_DOWN;
        }
        else
        {
            position->vz = value;
            moved = 1;
        }
        break;
    case PLAYER_STEP_RAISE_X:
        if (value < position->vx)
        {
            game_state.player.scriptHeld = PAD_LEFT;
            game_state.player.scriptPressed = PAD_LEFT;
        }
        else
        {
            position->vx = value;
            moved = 1;
        }
        break;
    case PLAYER_STEP_LOWER_Z:
        if (value <= position->vz)
        {
            position->vz = value;
            moved = 1;
        }
        else
        {
            game_state.player.scriptHeld = PAD_UP;
            game_state.player.scriptPressed = PAD_UP;
        }
        break;
    case PLAYER_STEP_LOWER_X:
        if (value <= position->vx)
        {
            position->vx = value;
            moved = 1;
        }
        else
        {
            game_state.player.scriptHeld = PAD_RIGHT;
            game_state.player.scriptPressed = PAD_RIGHT;
        }
        break;
    }
    if (moved == 1)
    {
        game_state.player.stateFlags &= ~PLAYER_FLAG_NO_INPUT;
    }
    else if (odd != 0)
    {
        game_state.player.scriptHeld |= PAD_CROSS;
        game_state.player.scriptPressed |= PAD_CROSS;
    }
    return moved;
}

/* Drives the player's sprite. `action` -1 clears it, 1 sets it up (with
 * `immediate` 0 it waits for the animation to play through instead of playing
 * it straight through). Anything else queues the animation for the event system.
 * A frame of -1 keeps the player's facing. */
s32 event_cmd_player_sprite(s16 action, s16 animation, s32 frame_arg, s32 immediate)
{
    s16 mode;
    AnimState *sprite;

    sprite = &game_state.player.sprite;
    if (frame_arg == -1)
    {
        mode = game_state.player.facing38;
    }
    else
    {
        mode = frame_arg;
    }
    if (action == -1)
    {
        sprite->animation = 0;
        game_state.player.stateFlags &= ~PLAYER_FLAG_EVENT_SPRITE;
    }
    else if (action == 1)
    {
        game_state.player.stateFlags |= PLAYER_FLAG_EVENT_SPRITE;
        if (immediate == 0)
        {
            if (sprite->animation != animation)
            {
                anim_select_range(sprite, animation);
                return 0;
            }
            if ((sprite->flags & (ANIM_FLAG_ENDED | ANIM_FLAG_JUMPED)) != 0)
            {
                game_state.player.stateFlags &= ~PLAYER_FLAG_EVENT_SPRITE;
                sprite->animation = 0;
                return 1;
            }
            return 0;
        }
        if (sprite->animation != animation)
        {
            anim_select_range(sprite, animation);
        }
        anim_set_frame(sprite, mode);
        return 1;
    }
    else
    {
        game_state.player.scriptSteps = event_queued_record;
        event_queued_record[3] = animation;
        event_queued_record[4] = mode;
        event_queued_record[5] = immediate - 10;
        event_queued_record[6] = 9999;
        game_state.player.stateFlags &= ~PLAYER_FLAG_EVENT_SPRITE;
        game_state.player.latches |= PLAYER_LATCH_SPRITE_REQUEST;
        return 0;
    }
    return 1;
}

/* Turns the player to `facing` (or sets a helper's facing when `actor` is a helper). The
 * player's frame is redrawn with the pause flag cleared. */
s32 event_cmd_set_facing(s16 facing, s16 actor, s32 unused2, s32 unused3)
{
    s32 old_flags;
    if (actor >= EVENT_ACTOR_FIRST_HELPER)
    {
        game_state.helpers[actor - EVENT_ACTOR_FIRST_HELPER].actor.facing = facing;
        return 1;
    }
    if (game_state.player.facing38 == facing)
        return 1;
    old_flags = game_state.globalFlags;
    game_state.player.facing38 = facing;
    game_state.globalFlags = old_flags & ~GLOBAL_FLAG_PAUSED;
    player_frame_update();
    game_state.globalFlags = old_flags;
    return 1;
}

s32 event_cmd_receive_item(s16 item, s16 indirect, s32 pose, s32 unused3)
{
    FarmPlayer *p;
    s32 value;
    p = &game_state.player;
    if (indirect != 0)
    {
        value = script_vars[item].value;
    }
    else
    {
        value = item;
    }
    player_receive_item(p, value, pose);
    return 1;
}

/* Sets (mode 0) or reads (other modes) an event flag. */
s32 event_cmd_event_flag(s16 mode, s16 value, s32 flag, s32 unused3)
{
    if (mode == 0)
    {
        event_flag_set(flag & 0xffff, value != 0);
        return 1;
    }
    else
    {
        return event_flag_get(flag & 0xffff);
    }
}

s32 event_cmd_item_has_reaction(s16 item, s16 indirect, s32 unused2, s32 unused3)
{
    s16 action;
    ItemInfo *record;

    if (indirect)
    {
        action = script_vars[item].half;
    }
    else
    {
        action = item;
    }
    if (action == ITEM_GIFT)
    {
        return 1;
    }
    record = &game_state.itemInfo[action];
    if ((record->itemFlags & 0x1C00) != 0 && (item_villager_reaction(action, 3) & 0xFFFF) < 3U)
    {
        return 1;
    }
    return 0;
}

/* Resets up to four actors, each id read from its script variable (-1 passes -1). */
s32 event_cmd_actor_reset_vars(s16 first, s16 second, s32 third, s32 fourth)
{
    s32 first_value;
    s32 second_value;
    s32 third_value;
    s32 fourth_value;

    if (first != -1)
    {
        first_value = script_vars[first].half;
    }
    else
    {
        first_value = -1;
    }
    if (second != -1)
    {
        second_value = script_vars[second].half;
    }
    else
    {
        second_value = -1;
    }
    if (third != -1)
    {
        third_value = script_vars[third].value;
    }
    else
    {
        third_value = -1;
    }
    if (fourth != -1)
    {
        fourth_value = script_vars[fourth].value;
    }
    else
    {
        fourth_value = -1;
    }
    event_cmd_actor_reset(first_value, second_value, third_value, fourth_value);
    return 1;
}

/* Mode 0 sets or clears `mask` in the player's state flags (or latches, when `latches` is
 * set). Other modes return whether the mask is set. */
s32 event_cmd_player_flags(s16 mode, s16 set, s32 mask, s32 latches)
{
    if (latches == 0)
    {
        if (mode != 0)
        {
            return (game_state.player.stateFlags & mask) != 0;
        }
        if (set != 0)
        {
            game_state.player.stateFlags |= mask;
        }
        else
        {
            game_state.player.stateFlags &= ~mask;
        }
    }
    else
    {
        if (mode != 0)
        {
            return (game_state.player.latches & mask) != 0;
        }
        if (set != 0)
        {
            game_state.player.latches |= mask;
        }
        else
        {
            game_state.player.latches &= ~mask;
        }
    }
    return 1;
}

s32 event_cmd_sound_play(s16 sound, s16 unused1, s32 unused2, s32 unused3)
{
    sound_play((signed short)sound);
    return 1;
}

/* Releases a villager from the event: mode 2 keeps PRESENT and IN_EVENT, any other mode clears
 * them, and mode 1 and up also clear ON_MAP. CHARACTER_ALL_VILLAGERS releases every villager.
 * Returns 0 for a bad id. */
s32 event_cmd_release_villager(CharacterId villager, s16 mode, s32 unused2, s32 unused3)
{
    s32 slot;
    FarmVillager *entry;
    u16 *status;
    if (villager == CHARACTER_ALL_VILLAGERS)
    {
        slot = 1;
        do
        {
            event_cmd_release_villager(slot, mode, unused2, unused3);
            slot++;
        } while (slot < CHARACTER_COUNT);
    }
    else if ((villager > 0) && (mode < CHARACTER_COUNT))
    {
        entry = &game_state.villagers[villager];
        status = &entry->flags;
        if (mode != EVENT_RELEASE_KEEP_PRESENT)
        {
            *status &= ~(VILLAGER_FLAG_PRESENT | VILLAGER_FLAG_IN_EVENT);
        }
        if (mode != 0)
        {
            if ((*status) & VILLAGER_FLAG_ON_MAP)
            {
                *status &= ~VILLAGER_FLAG_ON_MAP;
            }
        }
    }
    else
    {
        return 0;
    }
    return 1;
}

s32 event_cmd_camera_glide_vars(s16 mode, s16 y_slot, s32 x_slot, s32 z_slot)
{
    return event_cmd_camera_glide(mode, script_vars[y_slot].half, script_vars[x_slot].value, script_vars[z_slot].value);
}

/* Stores the character's position in the three slots (-1 skips one). Returns 0 for a bad villager. */
s32 event_cmd_get_position(CharacterId character, s16 y_slot, s32 x_slot, s32 z_slot)
{
    VECTOR *source;
    if (character == CHARACTER_PLAYER)
    {
        source = &game_state.player.position;
    }
    else
    {
        if (!villager_record_valid(character))
        {
            return 0;
        }
        source = &game_state.villagers[character].ePosition;
    }
    if (y_slot != -1)
    {
        script_vars[y_slot].value = source->vy;
    }
    if (x_slot != -1)
    {
        script_vars[x_slot].value = source->vx;
    }
    if (z_slot != -1)
    {

        script_vars[z_slot].value = source->vz;
    }
    return 1;
}

/* Stores the farm camera centre in the y, x and z slots (-1 skips one). With `consecutive`
 * the three go into y_slot, y_slot + 1 and y_slot + 2. */
s32 event_cmd_get_camera_centre(s16 consecutive, s16 y_slot, s32 x_slot, s32 z_slot)
{
    if (consecutive != 0)
    {
        x_slot = y_slot + 1;
        if (y_slot == -1)
        {
            return 0;
        }
        z_slot = y_slot + 2;
    }
    if (y_slot != -1 && y_slot < SCRIPT_VAR_COUNT)
    {
        script_vars[y_slot].value = farm_area.mode.scroll.centre.vy;
    }
    if (x_slot != -1 && x_slot < SCRIPT_VAR_COUNT)
    {
        script_vars[x_slot].value = farm_area.mode.scroll.centre.vx;
    }
    if (z_slot != -1 && z_slot < SCRIPT_VAR_COUNT)
    {
        script_vars[z_slot].value = farm_area.mode.scroll.centre.vz;
    }
    return 1;
}

enum GiftCheckMode
{
    GIFT_CHECK_TAKE = 0,    /* takes the player's current action (the gift) */
    GIFT_CHECK_PENDING = 1, /* STATUS_FLAG_GIFT_PENDING */
};

/* Mode 0 takes the player's held gift: success when `action`'s villager gives a 0
 * reaction to it. The gift goes to `result_slot` and is cleared. Mode 1 reports whether a
 * gift is pending. */
s32 event_cmd_check_gift(s16 mode, s16 action, s32 result_slot, s32 unused3)
{
    s32 success;
    s16 current_action;

    success = 0;
    switch (mode)
    {
    case GIFT_CHECK_TAKE:
        current_action = game_state.player.currentAction;
        if (current_action != 0)
        {
            if (item_villager_reaction(current_action, action) == 0)
                success = 1;
        }
        if (result_slot != -1)
            script_vars[result_slot].value = game_state.player.currentAction;
        game_state.player.currentAction = 0;
        break;
    case GIFT_CHECK_PENDING:
        if ((game_state.statusFlags & STATUS_FLAG_GIFT_PENDING) != 0)
            success = 1;
        break;
    default:
        break;
    }
    return success;
}

/* Fast-forwards the farm clock to hour:minute. Bits 0 and 1 of `flags` read the
 * hour and minute from script_vars. Without `immediate`, a time already past is
 * refused. */
s32 event_cmd_skip_to_time(s16 hour, s16 minute, s32 immediate, s32 flags)
{
    s16 hour_value;
    s16 adjusted;
    s16 minute_value;
    s32 minutes;

    if (flags & 1)
        hour_value = script_vars[hour].half;
    else
        hour_value = hour;
    adjusted = hour_value;
    if (flags & 2)
        minute_value = script_vars[minute].half;
    else
        minute_value = minute;
    if (immediate != 0)
    {
        clock_fast_forward_unflagged(hour_value * 60 + minute_value);
    }
    else
    {
        if (game_state.progress.clock.time.hm.hour < 6)
        {
            if (hour_value >= 6)
                return 0;
        }
        else if (hour_value < 6)
        {
            adjusted = hour_value + 24;
        }
        minutes = (adjusted - game_state.progress.clock.time.hm.hour) * 60 + (minute_value - game_state.progress.clock.time.hm.minute);
        if (minutes < 0)
            return 0;
        clock_fast_forward_unflagged(minutes);
    }
    return 1;
}

s32 event_cmd_fade_music(s16 fade_in, s16 frames, s32 start, s32 unused3)
{
    s32 result = 1;
    if (start)
    {
        if (fade_in)
            sound_fade_in(frames);
        else
            sound_fade_out(frames);
    }
    else
    {
        result = sound_fader_mode() == SOUND_FADER_IDLE;
    }
    return result;
}

/* Puts a villager in the event cell with an empty route, action and schedule. */
s32 event_cmd_reset_schedule(CharacterId character, s16 unused1, s32 unused2, s32 unused3)
{
    FarmVillager *villager;
    u16 *entry_cursor;
    u32 index;
    u16 fill;

    villager = &game_state.villagers[character];
    villager->place.value = PLACE_EVENT_CELL;
    villager->place.location = PLACE_EVENT_CELL;
    villager->lockCount = 0;
    villager->delay = 0;
    villager->column = 0;
    villager->row = 0;
    villager->wait = 0;
    villager->destination = 0;
    villager->prevDestination = 0;
    villager->unused18 = 0;
    villager->action = VILLAGER_ACTION_NONE;
    villager->prevAction = VILLAGER_ACTION_NONE;
    villager->step = 0;
    villager->flags = (villager->flags | VILLAGER_FLAG_JUST_RESET) & VILLAGER_FLAG_RESET_KEEP;
    cue_reset(&villager->cue);
    index = 0;
    fill = PLACE_EVENT_CELL;
    entry_cursor = &villager->schedule[0].place.location;
    do
    {
        entry_cursor[-3] = 0;
        entry_cursor[-2] = 0;
        entry_cursor[-1] = fill;
        entry_cursor[0] = fill;
        entry_cursor += 4;
    } while (++index < VILLAGER_SCHEDULE_STEPS);
    return 1;
}

/* Stores the current result step's value in a script variable. When that value is 0xFF (the
 * step has none), stores the step's position in the results, plus one, instead. */
s32 event_cmd_result_store(s16 slot_arg, s16 unused1, s32 unused2, s32 unused3)
{
    s32 index;
    s32 slot;
    FarmScriptStep *cursor;
    FarmScriptStep *result;

    index = slot_arg;
    cursor = (FarmScriptStep *)(game_state.work.results);
    result = event_result_cursor();
    if (result->nextOther == 0xFF)
    {
        slot = 0;
        if (result != cursor)
        {
            do
            {
                slot++;
                if (slot >= 206)
                {
                    break;
                }
                cursor++;
            } while (result != cursor);
        }
        if (slot == 206)
        {
            return 0;
        }
        script_vars[index].value = slot + 1;
    }
    else
    {
        script_vars[index].value = result->nextOther;
    }
    return 1;
}

/* Sets the done branch (nextDone) of the current result step to script_vars[slot]. */
s32 event_cmd_return(s16 slot, s16 unused1, s32 unused2, s32 unused3)
{
    FarmScriptStep *step = event_result_cursor();
    step->nextDone = script_vars[slot].byte;

    return 1;
}

/* The rival couple argument of EVENT_CMD_WEDDING's WEDDING_COUPLE_DONE. */
enum WeddingCouple
{
    WEDDING_COUPLE_NONE = 0,
    WEDDING_COUPLE_BY_MODE = -1, /* Popuri and Kai, tested by event_cmd_mode_check */
};

/* A wedding's slot: the player's own, or a rival couple (RivalCouple). */
enum WeddingSlot
{
    WEDDING_SLOT_PLAYER = -1,
};

/* event_cmd_init_mode's mode: plan a rival couple, or let the player marry the bride. */
enum WeddingPlanMode
{
    WEDDING_PLAN_RIVAL = 0,
    WEDDING_PLAN_PLAYER_BRIDE = 2,
};

/* Script command handler: `op` selects the command, `arg` the sub-command, and `x` / `y`
 * are operands (often indices into the script variables script_vars). */
s32 event_cmd_dispatch(s16 op, s16 arg, s32 x, s32 y)
{
    s32 value = 0;
    u16 n = 0;
    ScriptVar *cell;
    ScriptVar *cells;

    switch (op)
    {
    case EVENT_CMD_ESTATE_SLOTS:
        if (arg == ESTATE_SLOTS_SET_CLAIMED)
        {
            estate_slots_set_claimed(x == 1);
            return 1;
        }
        script_vars[x].value = estate_slots_free_count();
        break;
    case EVENT_CMD_WEDDING:
        switch (arg)
        {
        case WEDDING_HOLD:
            wedding_hold(x == CHARACTER_PLAYER ? WEDDING_SLOT_PLAYER : x);
            return 1;
        case WEDDING_COUPLE_DONE:
            if (y != 0)
            {
                x = script_vars[x].value;
            }
            switch (x)
            {
            case 1 + RIVAL_COUPLE_KAREN_RICK:
                n = EVENT_FLAG_KAREN_RICK_MARRIED;
                break;
            case 1 + RIVAL_COUPLE_ANN_CLIFF:
                n = EVENT_FLAG_ANN_CLIFF_MARRIED;
                break;
            case 1 + RIVAL_COUPLE_ELLI_DOCTOR:
                n = EVENT_FLAG_ELLI_DOCTOR_MARRIED;
                break;
            case 1 + RIVAL_COUPLE_MARY_GRAY:
                n = EVENT_FLAG_MARY_GRAY_MARRIED;
                break;
            case 1 + RIVAL_COUPLE_POPURI_KAI:
                n = EVENT_FLAG_POPURI_KAI_MARRIED;
                break;
            case WEDDING_COUPLE_BY_MODE:
                return event_cmd_mode_check(y);
            case WEDDING_COUPLE_NONE:
            default:
                break;
            }
            return event_flag_get(n);
        case WEDDING_PLAN:
            event_cmd_init_mode(x, y);
            return 1;
        case WEDDING_SET_TEXT_VARIANT:
            game_state.family.addressMode = script_vars[x].value - 1;
            break;
        case WEDDING_BRIDE:
            if (x != -1)
            {
                if (game_state.player.wedding.bride == -1)
                {
                    script_vars[x].value = -1;
                }
                else if (event_flag_get(EVENT_FLAG_SINGLE) == 1)
                {
                    script_vars[x].value = 0;
                }
                else
                {
                    script_vars[x].value = 1;
                }
            }
            if (y != -1)
            {
                script_vars[y].value = game_state.player.wedding.bride;
            }
            break;
        }
        break;
    case EVENT_CMD_CHILD_BORN:
        event_flag_set(EVENT_FLAG_HAS_CHILD, 1);
        event_flag_set(EVENT_FLAG_CHILD_LEVEL_0, 1);
        game_state.family.childNamed = 1;
        game_state.family.childGrowthDays = 0;
        family_child_born();
        /* When the player's child is born, the mayor writes, and so does every
         * married rival couple whose groom is a friend (friendship 200). Popuri and
         * Kai are not in that loop: Kai writes from across the sea. */
        estate_timed_message_add(2, ESTATE_MESSAGE_BIRTH_MAYOR, 2);
        for (n = 0; n < RIVAL_COUPLE_POPURI_KAI; n++)
        {
            if (event_flag_get(EVENT_FLAG_KAREN_RICK_MARRIED + n) != 0 && game_state.villagers[game_state.world.rivalWeddings[n].groom].friendship >= 200)
            {
                estate_timed_message_add(2, ESTATE_MESSAGE_BIRTH_RIVAL_FIRST + n, 2);
            }
        }
        if (event_cmd_mode_check(0) != 1)
        {
            return 1;
        }
        if (game_state.villagers[CHARACTER_KAI].friendship < 200)
        {
            return 1;
        }
        /* Kai's letter arrives seven days on. */
        if (game_state.progress.clock.date.season == SEASON_SUMMER)
        {
            if (game_state.progress.clock.date.day < 27)
            {
                n = ESTATE_MESSAGE_BIRTH_KAI_POPURI_PRESENT;
            }
            else
            {
                n = ESTATE_MESSAGE_BIRTH_KAI_POPURI_AWAY;
            }
        }
        else if (game_state.progress.clock.date.season == SEASON_SPRING && game_state.progress.clock.date.day < 27)
        {
            n = ESTATE_MESSAGE_BIRTH_KAI_POPURI_AWAY;
        }
        else
        {
            n = ESTATE_MESSAGE_BIRTH_KAI_POPURI_PRESENT;
        }
        estate_timed_message_add(2, n, 7);
        return 1;
    case EVENT_CMD_WIFE_SET_AFFECTION:
        n = game_state.player.wedding.bride;
        game_state.villagers[n].affection = arg;
        game_state.villagers[n].flags |= VILLAGER_FLAG_BIT10;
        event_flag_set(EVENT_FLAG_WIFE_REQUEST_PENDING, 0);
        event_flag_set(EVENT_FLAG_4A, 0);
        game_state.family.wifeRequests = 0;
        break;
    case EVENT_CMD_DAY_RECORD:
        switch (arg)
        {
        case DAY_RECORD_COLLAPSED:
            game_state.player.fatigue = 100;
            event_flag_set(EVENT_FLAG_REST_REQUESTED, 1);
            return 1;
        case DAY_RECORD_WIFE_TALKED:
            game_state.family.wifeIgnoredDays = 0;
            game_state.villagers[game_state.player.wedding.bride].flags |= VILLAGER_FLAG_BIT10;
            event_flag_set(EVENT_FLAG_WIFE_TALKED_TODAY, 1);
            event_flag_set(EVENT_FLAG_4C, 1);
            return 1;
        }
        break;
    case EVENT_CMD_ESTATE_BITS:
        if (x != 0)
        {
            return (game_state.progress.estate.eventBits & arg) != 0;
        }
        game_state.progress.estate.eventBits |= arg;
        break;
    case EVENT_CMD_UTENSIL_GET:
        switch (arg)
        {
        case UTENSIL_KNIFE:
            game_state.progress.estate.utensils |= UTENSIL_FLAG_KNIFE;
            event_flag_set(EVENT_FLAG_UTENSIL_DUE_KNIFE, 0);
            break;
        case UTENSIL_FRYING_PAN:
            game_state.progress.estate.utensils |= UTENSIL_FLAG_FRYING_PAN;
            event_flag_set(EVENT_FLAG_UTENSIL_DUE_FRYING_PAN, 0);
            break;
        case UTENSIL_POT:
            game_state.progress.estate.utensils |= UTENSIL_FLAG_POT;
            event_flag_set(EVENT_FLAG_UTENSIL_DUE_POT, 0);
            break;
        case UTENSIL_MIXER:
            game_state.progress.estate.utensils |= UTENSIL_FLAG_MIXER;
            event_flag_set(EVENT_FLAG_UTENSIL_DUE_MIXER, 0);
            break;
        case UTENSIL_WHISK:
            game_state.progress.estate.utensils |= UTENSIL_FLAG_WHISK;
            event_flag_set(EVENT_FLAG_UTENSIL_DUE_WHISK, 0);
            break;
        case UTENSIL_ROLLING_PIN:
            game_state.progress.estate.utensils |= UTENSIL_FLAG_ROLLING_PIN;
            event_flag_set(EVENT_FLAG_UTENSIL_DUE_ROLLING_PIN, 0);
            break;
        case UTENSIL_OVEN:
            game_state.progress.estate.utensils |= UTENSIL_FLAG_OVEN;
            event_flag_set(EVENT_FLAG_UTENSIL_DUE_OVEN, 0);
            break;
        case UTENSIL_SEASONING_SET:
            game_state.progress.estate.utensils |= UTENSIL_FLAG_SEASONING_SET;
            event_flag_set(EVENT_FLAG_UTENSIL_DUE_SEASONING_SET, 0);
            break;
        case EVENT_UTENSIL_OFFER_9:
            event_cmd_dispatch(6, 4, 0, 0);
            event_flag_set(EVENT_FLAG_OFFER_9_DUE, 0);
            break;
        default:
            return 0;
        }
        return 1;
    case EVENT_CMD_SHOP_DIALOGUE:
        if (y == 0)
        {
            switch (arg)
            {
            case EVENT_SHOP_POULTRY:
                shop_request_dispatch(SHOP_DIALOGUE_POULTRY);
                return 1;
            case EVENT_SHOP_BARN:
                shop_request_dispatch(SHOP_DIALOGUE_BARN);
                return 1;
            case EVENT_SHOP_TOOLS:
                shop_request_dispatch(SHOP_DIALOGUE_TOOLS);
                return 1;
            case EVENT_SHOP_PAY:
                shop_request_dispatch(SHOP_DIALOGUE_PAY);
                return 1;
            case EVENT_SHOP_DELIVERY:
                shop_request_dispatch(SHOP_DIALOGUE_GREENHOUSE);
                return 1;
            case EVENT_SHOP_PLOT_UPGRADE:
                shop_request_dispatch(SHOP_DIALOGUE_DOCTOR);
                return 1;
            case EVENT_SHOP_6:
                shop_request_dispatch(SHOP_DIALOGUE_15);
                return 1;
            case EVENT_SHOP_ITEM_16:
                shop_request_dispatch(SHOP_DIALOGUE_ITEM_16);
                return 1;
            case EVENT_SHOP_ITEM_17:
                shop_request_dispatch(SHOP_DIALOGUE_ITEM_17);
                return 1;
            case EVENT_SHOP_LUMBER:
                if (x == 0)
                {
                    shop_request_dispatch(SHOP_DIALOGUE_LUMBER);
                    return 1;
                }
                return event_flag_get(EVENT_FLAG_BUILD_IN_PROGRESS);
            case EVENT_SHOP_SEEDS:
                shop_request_dispatch(SHOP_DIALOGUE_SEEDS);
                return 1;
            case EVENT_SHOP_PLOT_SALE:
                shop_request_dispatch(SHOP_DIALOGUE_PLOT_SALE);
                return 1;
            }
            return 0;
        }
        return shop_dialogue_current() == 0;
    case EVENT_CMD_SET_FLAG_66:
        event_flag_set(EVENT_FLAG_ENDING_TRACK_3_EVENT, 1);
        return 1;
    case EVENT_CMD_POST_MESSAGE:
        estate_timed_message_add(arg, x, y);
        return 1;
    case EVENT_CMD_SET_VASE:
        game_state.progress.estate.estateFlags |= ESTATE_FLAG_VASE_SET;
        break;
    case EVENT_CMD_FESTIVAL:
        switch (arg)
        {
        case FESTIVAL_PRIZES_RESTOCK:
            shop_restock();
            return 1;
        case FESTIVAL_PRIZES_MODE:
            game_state.raceBets.race = x;
            break;
        case FESTIVAL_PRIZES_EDIT:
            if (x != 0)
            {
                shop_edit_begin();
                return 1;
            }
            return shop_menu_idle();
        case FESTIVAL_PRIZES_MENU:
            if (x != 0)
            {
                shop_request_dispatch(SHOP_DIALOGUE_MEDAL_EXCHANGE);
                return 1;
            }
            return shop_medal_exchange_done();
        case FESTIVAL_PRIZES_ROW_EMPTY:
            return shop_row_all_zero();
        case FESTIVAL_RESULT:
            switch (x)
            {
            case FESTIVAL_HORSE_RACE:
                if (game_state.raceBets.horseWon == 1)
                {
                    game_state.animals.horse.actor.flags |= ACTOR_FLAG_PRIZE_WINNER;
                }
                return game_state.raceBets.horseWon;
            case FESTIVAL_CHICKEN:
                return game_state.minigames.mg2Done;
            case FESTIVAL_TOMATO:
                script_vars[y].value = game_state.minigames.courseId;
                return game_state.minigames.mg3Done;
            case FESTIVAL_DOG_RACE:
                script_vars[y].value = game_state.minigames.mg4Winner;
                return game_state.minigames.mg4PlayerWon;
            case FESTIVAL_SWIMMING:
                script_vars[y].value = game_state.minigames.mg5Slot;
                return game_state.minigames.mg5PlayerWon;
            }
            return 0;
        case FESTIVAL_MINIGAME:
            if (x == 0)
            {
                return game_state.progress.entrySubStep == 0xFF;
            }
            game_state.progress.entrySubStep = 0;
            /* fallthrough */
        case FESTIVAL_ENTRANT_INFO:
            for (n = 0; n < MAX_CHICKENS; n++)
            {
                if (game_state.animals.chickens[n].actor.flags & ACTOR_FLAG_HELD)
                {
                    game_state.minigames.chickenSkill = game_state.animals.chickens[n].actor.affection;
                    game_state.minigames.textSource = game_state.animals.chickens[n].actor.name;
                    return 1;
                }
            }
            return 0;
        case FESTIVAL_ENTRANT_MARK:
            for (n = 0; n < MAX_CHICKENS; n++)
            {
                if (game_state.animals.chickens[n].actor.flags & ACTOR_FLAG_HELD)
                {
                    game_state.animals.chickens[n].actor.flags |= ACTOR_FLAG_PRIZE_WINNER;
                    return 1;
                }
            }
            return 0;
        case FESTIVAL_TEAM_RESET:
            game_state.minigames.mg3Team = -1;
            game_state.minigames.courseId = -1;
            game_state.minigames.mg3Done = 0;
            break;
        case FESTIVAL_TEAM_SET:
            if (y == 0)
            {
                game_state.minigames.mg3Team = x;
            }
            else
            {
                game_state.minigames.mg3Team = script_vars[x].value;
            }
            break;
        case FESTIVAL_TEAM_GET:
            script_vars[x].value = game_state.minigames.mg3Team;
            break;
        case FESTIVAL_SCORE_TO_MEDALS:
            game_state.player.medals = game_state.raceBets.medalTotal;
            game_state.raceBets.medalTotal = 0;
            break;
        default:
            return 0;
        }
        break;
    case EVENT_CMD_ANIMAL_NAMING:
        switch (arg)
        {
        case ANIMAL_NAMING_BEGIN:
            game_state.globalFlags |= GLOBAL_FLAG_PAUSED;
            animal_naming_begin();
            return 1;
        case ANIMAL_NAMING_SLOT_READY:
            return animal_naming_slot_ready(x);
        case ANIMAL_NAMING_STEP:
            return animal_naming_step(x);
        case ANIMAL_NAMING_END:
            game_state.globalFlags &= ~GLOBAL_FLAG_PAUSED;
            animal_naming_end();
            return 1;
        }
        return 0;
    case EVENT_CMD_ANIMAL_DEATH:
        switch (arg)
        {
        case ANIMAL_DEATH_TEST:
            return farm_kind_selected(1 << x, 1 << y);
        case ANIMAL_DEATH_CLEAR:
            farm_clear_flagged_items();
            break;
        default:
            return 0;
        }
        /* fallthrough */
    case EVENT_CMD_ESTATE_GIFT:
        switch (arg)
        {
        case ESTATE_GIFT_ITEM_83:
            game_state.progress.estate.estateFlags |= ESTATE_FLAG_ITEM_83;
            break;
        case ESTATE_GIFT_ROW97_BONUS:
            game_state.progress.estate.estateFlags |= ESTATE_FLAG_ROW97_BONUS;
            break;
        default:
            return 0;
        }
        /* fallthrough */
    case EVENT_CMD_LIVESTOCK_ENTRANT:
        for (n = 0; n < MAX_LIVESTOCK; n++)
        {
            if (game_state.animals.livestock[n].actor.flags & ACTOR_FLAG_IN_EVENT)
            {
                if (game_state.animals.livestock[n].actor.affection >= LIVESTOCK_ENTRANT_AFFECTION)
                {
                    game_state.animals.livestock[n].actor.flags |= ACTOR_FLAG_PRIZE_WINNER;
                    return 1;
                }
                return 0;
            }
        }
        return 0;
    case EVENT_CMD_IS_EVENT:
        if (x != -1)
        {
            script_vars[x].value = game_state.request->args[0];
        }
        return arg == game_state.request->args[0];
    case EVENT_CMD_SPAWN_GATE:
        if (y == 0)
        {
            n = arg;
        }
        else
        {
            n = script_vars[arg].value;
        }
        if (x != 0)
        {
            game_state.animals.spawnGate = n;
        }
        else if (game_state.animals.spawnGate == n)
        {
            return 1;
        }
        return 0;
    case EVENT_CMD_SET_MILESTONE_RESULT_RUNNING:
        if (arg != 0)
        {
            event_flag_set(EVENT_FLAG_MILESTONE_RESULT_RUNNING, 1);
            break;
        }
        return event_flag_get(EVENT_FLAG_MILESTONE_RESULT_RUNNING) != 1;
    case EVENT_CMD_ORDER_STATUS:
        switch (arg)
        {
        case ORDER_STATUS_PENDING:
            return milestone_status_query(0);
        case ORDER_STATUS_READ:
            script_vars[x].value = milestone_status_query(1);
            return 1;
        case ORDER_STATUS_APPLY:
            milestone_status_query(2);
            return 1;
        }
        /* fallthrough */
    case EVENT_CMD_LIVESTOCK_EXISTS:
        n = shop_livestock_lookup(arg, 1, -1);
        return n != 0;
    case EVENT_CMD_BUMP_COUNT_1:
        if (game_state.player.counts70[1] < BUMP_COUNT_1_MAX)
        {
            game_state.player.counts70[1]++;
        }
        break;
    case EVENT_CMD_ARM_TRIGGER:
        game_state.progress.estate.estateFlags |= ESTATE_FLAG_TRIGGER_ARMED;
        break;
    case EVENT_CMD_RELOAD_RESOURCE:
        DrawSync(0);
        main_frame_clear();
        if (farm_area.mode.data->head.resourceId != 999)
        {
            farm_load_resource_id(farm_area.mode.data->head.resourceId, 0);
        }
        return 1;
    case EVENT_CMD_SET_ESTATE_BIT21:
        game_state.progress.estate.estateFlags |= ESTATE_FLAG_BIT21;
        break;
    case EVENT_CMD_VILLAGER_ANIMATION:
        cells = script_vars;
        cell = &cells[x];
        cell->value = game_state.villagers[arg].sprite80.animation;
        break;
    case EVENT_CMD_SET_STATUS_BIT23:
        game_state.progress.estate.statusFlags |= ESTATE_STATUS_BIT23;
        break;
    case EVENT_CMD_NAP:
        if (arg == 0)
        {
            event_cmd_player_sprite(0, 84, 0, 11);
            game_state.statusFlags |= STATUS_FLAG_ASLEEP;
            game_state.player.stateFlags |= PLAYER_FLAG_SCRIPTED;
        }
        else
        {
            game_state.statusFlags &= ~STATUS_FLAG_ASLEEP;
        }
        break;
    case EVENT_CMD_CHILD_LEVEL:
        if (arg != -1)
        {
            script_vars[arg].value = game_state.family.childLevel;
        }
        return event_var_compare(game_state.family.childLevel, x, y);
    case EVENT_CMD_VAR_ARITH:
        event_var_arith(&script_vars[arg].value, x / 2, y);
        return 1;
    case EVENT_CMD_DOG_AFFECTION:
        value = game_state.animals.dog.actor.affection;
        event_var_arith(&value, x, y);
        if (value >= 256)
        {
            value = 255;
        }
        if (value < 0)
        {
            value = 0;
        }
        game_state.animals.dog.actor.affection = value;
        break;
    default:
        return 0;
    }
    return 1;
}

static const FarmActorIds scene_actor_ids = {{
    {0x1C, 0x13, 0x05, 0x0B, 0x1B},
    {0x14, 0x03, 0x0E, 0x07, 0x09},
}};

/* Starts `animation` on the actor and nudges it along `axis` toward `target`, 20 units a call. */
s32 event_cmd_walk_axis(s16 actor, s16 axis, s32 target, s32 animation)
{
    event_cmd_set_sprite(actor, animation, 0, 0);
    return farm_nudge_axis(actor, axis, target, 20);
}

/* The modes of event_cmd_stop_actor. */
enum ActorStopMode
{
    ACTOR_STOP_SPRITE_WAIT = 0, /* the sprite, waiting for the animation to play through */
    ACTOR_STOP_SPRITE_NOW = 1,  /* the sprite at once */
    ACTOR_STOP_BY_SPEED = 2,    /* the frame is picked by the way the actor was moving */
    ACTOR_STOP_FACE_PLAYER = 3,
};

/* Stops an actor: modes 0 and 1 set its sprite (waiting for the animation, or at once), mode 2
 * picks the facing frame from the way it was moving, and mode 3 faces the player. Then its
 * speed is adjusted. */
s32 event_cmd_stop_actor(s16 actor, s16 animation, s32 frame, s32 mode)
{
    VECTOR *speed;
    s32 speed_x;
    s32 speed_z;

    speed = event_var_address(EVENT_VAR_VILLAGER_EVENT_SPEED, actor, 0);
    switch (mode)
    {
    case ACTOR_STOP_SPRITE_WAIT:
    case ACTOR_STOP_SPRITE_NOW:
        event_cmd_set_sprite(actor, animation, frame, mode);
        break;
    case ACTOR_STOP_BY_SPEED:
        speed_z = speed->vz;
        if (speed_z < 0)
        {
            event_cmd_set_sprite(actor, animation, FACING_DOWN, 1);
        }
        else
        {
            speed_x = speed->vx;
            if (speed_x < 0)
            {
                event_cmd_set_sprite(actor, animation, FACING_LEFT, 1);
            }
            else if (speed_z > 0)
            {
                event_cmd_set_sprite(actor, animation, FACING_UP, 1);
            }
            else if (speed_x > 0)
            {
                event_cmd_set_sprite(actor, animation, FACING_RIGHT, 1);
            }
        }
        break;
    case ACTOR_STOP_FACE_PLAYER:
        event_cmd_face_player(actor, 0, animation, 0);
        break;
    }
    farm_vector_adjust(4, speed, 0);
    return 1;
}

/* Select the three farm presentation cues for the requested mode. */
s32 event_cmd_presentation_cues(s16 mode, s16 unused1, s32 unused2, s32 unused3)
{
    if (mode == 1)
    {
        goto mode_one;
    }
    if (mode >= 2)
    {
        goto other_mode;
    }
    if (mode != 0)
    {
        goto other_mode;
    }
    event_flag_set(EVENT_FLAG_CUE_LATCH_SET, 0);
    event_flag_set(EVENT_FLAG_CUE_LATCH_RESET, 1);
    event_flag_set(EVENT_FLAG_B0, 1);
    goto done;
mode_one:
    event_flag_set(EVENT_FLAG_CUE_LATCH_SET, 1);
    event_flag_set(EVENT_FLAG_CUE_LATCH_RESET, 0);
    event_flag_set(EVENT_FLAG_B0, 0);
    goto done;
other_mode:
    event_flag_set(EVENT_FLAG_CUE_LATCH_SET, 0);
    event_flag_set(EVENT_FLAG_CUE_LATCH_RESET, 0);
    event_flag_set(EVENT_FLAG_B0, 0);
done:
    return 1;
}

/* The scene's ten actors on their walk route. */
#define WALK_ACTOR_COUNT 10

static s32 walk_route_x[WALK_ACTOR_COUNT] = {
    500,
    1200,
    -800,
    -500,
    200,
    500,
    1200,
    -800,
    -500,
    200,
};

static s32 walk_route_z[WALK_ACTOR_COUNT] = {
    -2000,
    1600,
    1600,
    -2000,
    1600,
    -1600,
    2000,
    2000,
    -1600,
    2000,
};

static s16 walk_route_facing[WALK_ACTOR_COUNT] = {
    2,
    2,
    2,
    2,
    2,
    0,
    0,
    0,
    0,
    0,
};

/* Script command: walks the scene's ten actors. The partner in script_vars[who]
 * takes the place of the last one. Villagers who are not available leave the
 * scene. On the first call (script_vars[step] == -1) the actors are placed on
 * their route and turned; afterwards each steps along it, sharing script_vars[step]. */
s32 event_cmd_walk_actors(s16 step, s16 who, s32 unused2, s32 unused3)
{
    FarmActorIds actors;
    s16 *displaced;
    s16 *ids;
    s16 *actor;
    s32 *xs;
    s32 *zs;
    const s16 *facings;
    s32 slot;
    s32 i;
    s32 progress;

    actors = scene_actor_ids;
    actors.ids[1][4] = script_vars[who].half;
    switch (actors.ids[1][4])
    {
    case CHARACTER_POPURI:
        slot = 0;
        break;
    case CHARACTER_KAREN:
        slot = 1;
        break;
    case CHARACTER_MARY:
        slot = 2;
        break;
    case CHARACTER_ANN:
        slot = 3;
        break;
    case CHARACTER_ELLI:
        slot = -1;
        break;
    default:
        slot = 4;
        break;
    }
    if (slot != -1)
    {
        displaced = &actors.ids[0][slot];
        event_cmd_set_actor_hidden(*displaced, 1, 0, 0);
        ids = actors.ids[0];
        *displaced = 8;
        actors.ids[1][slot] = 9;
    }
    else
    {
        event_cmd_set_actor_hidden(8, 1, 0, 0);
        ids = actors.ids[0];
    }
    if (event_cmd_villager_available(CHARACTER_CLIFF, 0, 0, 0) == 0 && actors.ids[1][4] != CHARACTER_ANN)
    {
        event_cmd_set_actor_hidden(actors.ids[0][3], 1, 0, 0);
        actors.ids[0][3] = -1;
        actors.ids[1][3] = -1;
    }
    if (event_cmd_villager_available(actors.ids[1][0], 0, 0, 0) == 0)
    {
        event_cmd_set_actor_hidden(actors.ids[0][0], 1, 0, 0);
        actors.ids[0][0] = -1;
        actors.ids[1][0] = -1;
    }
    actor = ids;
    xs = walk_route_x;
    zs = walk_route_z;
    facings = walk_route_facing;
    if (script_vars[step].value == -1)
    {
        for (i = 0; i < WALK_ACTOR_COUNT; i++, actor++, xs++, zs++, facings++)
        {
            if (*actor != -1)
            {
                event_cmd_set_position(*actor, 0, *xs, *zs);
                event_cmd_set_sprite(*actor, 0, *facings, 1);
            }
        }
        script_vars[step].value++;
    }
    else
    {
        for (i = 0; i < WALK_ACTOR_COUNT; i++, actor++, xs++, zs++)
        {
            if (*actor != -1)
            {
                progress = script_vars[step].value;
                farm_walk_route_step(*actor, *xs, *zs, &progress);
            }
        }
        script_vars[step].value = progress;
    }
    return 1;
}

/* Walks a villager onto the scene from a lane. A negative `from` means straight
 * in from the edge; otherwise the villager walks a wide circle, or a small
 * quarter turn when the target is behind. Faces along the larger step. Returns
 * 1 for the quarter turn. */
s32 event_cmd_actor_walk_lane(CharacterId character, s16 from, s32 to, s32 lane)
{
    VECTOR *pos;
    s32 x;
    s32 y;
    s32 z;
    double step;
    double dx;
    double dz;
    s32 turned;
    s32 facing;

    pos = event_var_address(EVENT_VAR_VILLAGER_EVENT_POSITION, character, 0);
    x = rand_range(51) - 25 + script_vars[lane].value;
    y = rand_range(51) - 700;
    z = rand_range(51) - 25 + script_vars[lane + 1].value;
    turned = 0;
    if (y < pos->vy - 25)
    {
        y = pos->vy - 25;
    }
    step = 0.13089958333333332;
    if (script_vars[from].value < 0)
    {
        dx = 157.0795;
        dz = 0.0;
        pos->vy = -700;
        pos->vx += 157;
    }
    else if (script_vars[to].value >= script_vars[from].value)
    {
        double radius = 1200.0;

        step = step * script_vars[from].value;
        dx = radius * sin(step);
        dz = radius * cos(step);
        pos->vy = y;
        pos->vx = x + dx;
        pos->vz = z + dz;
    }
    else
    {
        double radius = 157.0;

        step = step * script_vars[to].value;
        dx = radius * sin(step + 1.570795);
        dz = radius * cos(step + 1.570795);
        pos->vy = -700;
        pos->vx += (s32)dx;
        pos->vz += (s32)dz;
        turned = 1;
    }
    if (abs((s32)dx) >= abs((s32)dz))
    {
        facing = (dx < 0.0) ? 9 : 7;
    }
    else
    {
        facing = (dz < 0.0) ? 8 : 10;
    }
    event_cmd_set_sprite(character, facing, 0, 0);
    return turned;
}

s32 event_cmd_test_var_by_slot(s16 var, s16 slot, s32 comparison, s32 value)
{
    s32 v = event_var_get(var, script_vars[slot].value);
    return event_var_compare(v, value, comparison);
}

s32 event_cmd_set_var_by_slot(s16 var, s16 slot, s32 value, s32 mode)
{
    event_var_update(var, script_vars[slot].value, value, mode);
    return 1;
}

/* Sets the villager's place flags: mode 0 hides the cursor marker, mode 2 shows the alternate
 * cursor, mode 1 clears both. */
s32 event_cmd_set_villager_place(s16 slot, s16 mode, s32 use_table, s32 unused3)
{
    s32 value;
    u16 flags;

    if (use_table != 0)
        value = script_vars[slot].value;
    else
        value = slot;
    flags = game_state.villagers[value].placeFlags & ~(VILLAGER_PLACE_CURSOR_ALT | VILLAGER_PLACE_NO_MARKER);
    game_state.villagers[value].placeFlags = flags;
    switch (mode)
    {
    case 0:
        game_state.villagers[value].placeFlags = flags | VILLAGER_PLACE_NO_MARKER;
        break;
    case 1:
        break;
    case 2:
        game_state.villagers[value].placeFlags = flags | VILLAGER_PLACE_CURSOR_ALT;
        break;
    }
    return 1;
}

/* The actors who dance, in order. The sixth entry (0) is never read. */
static s16 dance_actor_slots[6] = {20, 14, 9, 7, 3, 0};

/* Start x of each dancer, then start z. */
static s32 dance_start_positions[10] = {
    0,
    -1000,
    1000,
    -500,
    500,
    2200,
    2200,
    2200,
    500,
    500,
};

/* The stages of event_cmd_five_actor_dance. DANCE_TURN_DANCER_n turns
 * dance_actor_slots[n]. */
enum DanceStage
{
    DANCE_PLACE = 0,
    DANCE_POSE_1 = 1,
    DANCE_POSE_2 = 2,
    DANCE_POSE_3 = 3,
    DANCE_POSE_4 = 4,
    DANCE_WALK_OUT = 5,
    DANCE_TURN_DANCER_1 = 6,
    DANCE_TURN_DANCER_3 = 7,
    DANCE_TURN_DANCER_0 = 8,
    DANCE_TURN_DANCER_4 = 9,
    DANCE_TURN_DANCER_2 = 10,
    DANCE_WALK_IN = 11,
    DANCE_POSE_FINAL = 12,
    DANCE_IDLE_1 = 13,
    DANCE_IDLE_2 = 14,
    DANCE_IDLE_3 = 15,
    DANCE_WALK_HOME = 16, /* the last stage: the stage wraps to DANCE_POSE_1 */
};

/* Script command: the five-actor dance, one stage per call. script_vars[var] is
 * the stage. Stage 0 places the dancers; later stages pose them, walk them in
 * two groups and turn each arrival. The first dancer sits out unless they are
 * available. The stage advances when the last call reports 1, wrapping from 16
 * back to 1. Returns 1. */
s32 event_cmd_five_actor_dance(s16 var, s16 unused1, s32 unused2, s32 unused3)
{
    s16 i;
    s32 done;
    s16 stage;
    s16 dancer0Out;

    stage = script_vars[var].half;
    dancer0Out = event_cmd_villager_available(dance_actor_slots[0], 0, 0, 0) != 1;
    done = 0;
    switch (stage)
    {
    case DANCE_PLACE:
        for (i = dancer0Out; i < 5; i++)
        {
            event_cmd_set_position(dance_actor_slots[i], 0, dance_start_positions[i], dance_start_positions[i + 5]);
            event_cmd_set_sprite(dance_actor_slots[i], 5, 0, 1);
        }
        done = 1;
        break;
    case DANCE_POSE_1:
    case DANCE_POSE_2:
    case DANCE_POSE_3:
    case DANCE_POSE_4:
        for (i = dancer0Out; i < 5; i++)
        {
            done = event_cmd_set_sprite(dance_actor_slots[i], 10, 0, 0);
        }
        break;
    case DANCE_WALK_OUT:
        for (i = dancer0Out; i < 3; i++)
        {
            done = farm_nudge_cue(dance_actor_slots[i], 0, 500, 11);
            if (done == 1)
            {
                event_cmd_set_sprite(dance_actor_slots[i], 5, 0, 1);
            }
        }
        for (; i < 5; i++)
        {
            done = farm_nudge_cue(dance_actor_slots[i], 2, 2200, 6);
            if (done == 1)
            {
                event_cmd_set_sprite(dance_actor_slots[i], 5, 0, 11);
            }
        }
        break;
    case DANCE_TURN_DANCER_1:
        done = event_cmd_set_sprite(dance_actor_slots[1], 12, 0, 0);
        break;
    case DANCE_TURN_DANCER_3:
        event_cmd_set_sprite(dance_actor_slots[1], 5, 0, 1);
        done = event_cmd_set_sprite(dance_actor_slots[3], 12, 0, 0);
        if (done == 1)
        {
            /* With dancer 0 out, the stage skips DANCE_TURN_DANCER_0. */
            stage += dancer0Out;
        }
        break;
    case DANCE_TURN_DANCER_0:
        event_cmd_set_sprite(dance_actor_slots[3], 5, 0, 1);
        done = event_cmd_set_sprite(dance_actor_slots[0], 12, 0, 0);
        break;
    case DANCE_TURN_DANCER_4:
        event_cmd_set_sprite(dance_actor_slots[3], 5, 0, 1);
        event_cmd_set_sprite(dance_actor_slots[0], 5, 0, 1);
        done = event_cmd_set_sprite(dance_actor_slots[4], 12, 0, 0);
        break;
    case DANCE_TURN_DANCER_2:
        event_cmd_set_sprite(dance_actor_slots[4], 5, 0, 1);
        done = event_cmd_set_sprite(dance_actor_slots[2], 12, 0, 0);
        break;
    case DANCE_WALK_IN:
        for (i = dancer0Out; i < 3; i++)
        {
            done = farm_nudge_cue(dance_actor_slots[i], 2, 1350, 11);
            if (done == 1)
            {
                event_cmd_set_sprite(dance_actor_slots[i], 5, 0, 1);
            }
        }
        for (; i < 5; i++)
        {
            done = farm_nudge_cue(dance_actor_slots[i], 0, 1350, 11);
            if (done == 1)
            {
                event_cmd_set_sprite(dance_actor_slots[i], 5, 0, 1);
            }
        }
        break;
    case DANCE_POSE_FINAL:
        for (i = dancer0Out; i < 5; i++)
        {
            done = event_cmd_set_sprite(dance_actor_slots[i], 13, 0, 0);
        }
        break;
    case DANCE_IDLE_1:
    case DANCE_IDLE_2:
    case DANCE_IDLE_3:
        for (i = dancer0Out; i < 5; i++)
        {
            done = event_cmd_set_sprite(dance_actor_slots[i], 5, 0, 0);
        }
        break;
    case DANCE_WALK_HOME:
        for (i = dancer0Out; i < 3; i++)
        {
            done = farm_nudge_cue(dance_actor_slots[i], 2, dance_start_positions[i + 5], 8);
            if (done == 1)
            {
                event_cmd_set_sprite(dance_actor_slots[i], 5, 0, 1);
            }
        }
        for (; i < 5; i++)
        {
            done = farm_nudge_cue(dance_actor_slots[i], 0, dance_start_positions[i + 5], 6);
            if (done == 1)
            {
                event_cmd_set_sprite(dance_actor_slots[i], 5, 0, 1);
            }
        }
        break;
    }
    if (done == 1)
    {
        if (stage == DANCE_WALK_HOME)
        {
            stage = DANCE_POSE_1;
        }
        else
        {
            stage++;
        }
    }
    script_vars[var].value = stage;
    return 1;
}

/* Applies an update to every villager, except two unnamed ones. For affection
 * or friendship, the bachelorette gets `pairOperation` on affection and everyone
 * else gets `operation` on friendship. The helpers' affection gets `operation` too. */
s32 event_cmd_apply_var_table(s16 index, s16 extra, s32 operation, s32 pairOperation)
{
    s16 i;

    for (i = 1; i < CHARACTER_COUNT; i++)
    {
        if (i == CHARACTER_UNNAMED_27 || i == CHARACTER_UNNAMED_33)
        {
            continue;
        }
        if (index == EVENT_VAR_VILLAGER_AFFECTION || index == EVENT_VAR_VILLAGER_FRIENDSHIP)
        {
            if (villager_is_bachelorette(i) == 1)
            {
                event_var_update(EVENT_VAR_VILLAGER_AFFECTION, i, pairOperation, extra);
            }
            else
            {
                event_var_update(EVENT_VAR_VILLAGER_FRIENDSHIP, i, operation, extra);
            }
        }
        else
        {
            event_var_update(index, i, operation, extra);
        }
    }
    if (index == EVENT_VAR_VILLAGER_AFFECTION || index == EVENT_VAR_VILLAGER_FRIENDSHIP)
    {
        for (i = 0; i < HELPER_COUNT; i++)
        {
            event_var_update(EVENT_VAR_HELPER_AFFECTION, i, operation, extra);
        }
    }
    return 1;
}

/* The kinds event_cmd_object_show can show. */
enum ObjectKind
{
    OBJECT_KIND_HORSE = 0,
    OBJECT_KIND_LIVESTOCK_1 = 1,
    OBJECT_KIND_LIVESTOCK_2 = 2,
    OBJECT_KIND_CHICKEN = 3,
    OBJECT_KIND_DOG = 4,
};

/* The actions of event_cmd_object_show. */
enum ObjectAction
{
    OBJECT_ACTION_HIDE = 0, /* clears ACTOR_FLAG_IN_EVENT; for livestock and chickens the first one in the scene */
    OBJECT_ACTION_SHOW = 1, /* sets ACTOR_FLAG_IN_EVENT */
    OBJECT_ACTION_DOG_PUT_AWAY = 2,
    OBJECT_ACTION_DOG_TAKE_OUT = 3,
};

/* Script command: shows (action 1) or hides (action 0) an animal or the dog, or
 * (2, 3) puts the dog away and takes it out. Kind 0 is the horse, 1 and 2 are
 * livestock, 3 the chickens and 4 the dog. With action 0 the first animal in the
 * scene is picked. Returns 0 for an unknown kind, else 1. */
s32 event_cmd_object_show(s16 kind, s16 index, s32 action, s32 indirect)
{
    FarmActor *object;
    s32 i;
    s16 slot;
    s16 found;

    if (indirect)
    {
        slot = script_vars[index].half;
    }
    else
    {
        slot = index;
    }
    object = 0;
    switch (kind)
    {
    case OBJECT_KIND_HORSE:
        object = &game_state.animals.horse.actor;
        break;
    case OBJECT_KIND_LIVESTOCK_1:
    case OBJECT_KIND_LIVESTOCK_2:
        if (action == OBJECT_ACTION_HIDE)
        {
            i = 0;
            do
            {
                if (game_state.animals.livestock[i].actor.flags & ACTOR_FLAG_IN_EVENT)
                {
                    found = i;
                    object = &game_state.animals.livestock[found].actor;
                    break;
                }
                i++;
            } while (i < MAX_LIVESTOCK);
            if (object == 0)
            {
                return 1;
            }
        }
        else
        {
            object = &game_state.animals.livestock[slot].actor;
        }
        break;
    case OBJECT_KIND_CHICKEN:
        if (action == OBJECT_ACTION_HIDE)
        {
            for (i = 0; i < MAX_CHICKENS; i++)
            {
                if (game_state.animals.chickens[i].actor.flags & ACTOR_FLAG_IN_EVENT)
                {
                    found = i;
                    object = &game_state.animals.chickens[found].actor;

                    break;
                }
            }
            if (object == 0)
            {
                return 1;
            }
        }
        else
        {
            object = &game_state.animals.chickens[slot].actor;
        }
        break;
    case OBJECT_KIND_DOG:
        object = &game_state.animals.dog.actor;
        break;
    default:
        return 0;
    }
    switch (action)
    {
    case OBJECT_ACTION_HIDE:
        object->flags &= ~ACTOR_FLAG_IN_EVENT;
        break;
    case OBJECT_ACTION_SHOW:
        object->flags |= ACTOR_FLAG_IN_EVENT;
        break;
    case OBJECT_ACTION_DOG_PUT_AWAY:
        event_flag_set(EVENT_FLAG_DOG_PLACED, 0);
        dog_place(0);
        break;
    case OBJECT_ACTION_DOG_TAKE_OUT:
        event_item_can_take(ITEM_ADULT_DOG, 1, 0, 1);
        game_state.animals.dog.actor.flags &= ~ACTOR_FLAG_HELD;
        event_flag_set(EVENT_FLAG_DOG_PLACED, 1);
        dog_place(1);
        break;
    }
    return 1;
}

/* Advances the actor's walk route by one step toward (x, z), keeping the stage in
 * script_vars[step_slot]. Returns 1 at the end of the route. */
s32 event_cmd_walk_route(s16 step_slot, s16 actor, s32 x_slot, s32 z_slot)
{
    return farm_walk_route_step(actor, script_vars[x_slot].value, script_vars[z_slot].value, &script_vars[step_slot].value);
}

/* Stores the actor's birthday (season and day) in the two slots (-1 skips one), and returns
 * 1 when today is that birthday. A helper's birthday is its pair's kind and count. */
s32 event_cmd_is_birthday(s16 actor_arg, CharacterId character, s32 season_slot, s32 day_slot)
{
    s16 first;
    s16 second;
    s32 index;
    s16 actor;
    if (character != 0)
    {
        character = script_vars[actor_arg].half;
    }
    else
    {
        character = actor_arg;
    }
    if (character == CHARACTER_PLAYER)
    {
        first = game_state.player.birthday.season;
        second = game_state.player.birthday.day;
    }
    else if (character >= EVENT_ACTOR_FIRST_HELPER)
    {
        index = (s16)(character - EVENT_ACTOR_FIRST_HELPER);
        first = game_state.helpers[index].pairC4.kind;
        second = game_state.helpers[index].pairC4.count;
    }
    else
    {
        first = game_state.villagers[character].birthday.season;
        second = game_state.villagers[character].birthday.day;
    }
    if (season_slot != -1)
        script_vars[season_slot].value = first;
    if (day_slot != -1)
        script_vars[day_slot].value = second;
    if (game_state.progress.clock.date.season == first)
    {
        if (game_state.progress.clock.date.day == second)
            return 1;
    }
    return 0;
}

/* Stores the house, barn, coop and greenhouse levels in the four slots (-1 skips one). */
s32 event_cmd_store_levels(s16 house_slot, s16 barn_slot, s32 coop_slot, s32 greenhouse_slot)
{
    if (house_slot != -1)
    {
        script_vars[house_slot].value = game_state.progress.estate.houseLevel;
    }
    if (barn_slot != -1)
    {
        script_vars[barn_slot].value = game_state.progress.estate.barnLevel;
    }
    if (coop_slot != -1)
    {
        script_vars[coop_slot].value = game_state.progress.estate.coopLevel;
    }
    if (greenhouse_slot != -1)
    {
        script_vars[greenhouse_slot].value = game_state.progress.estate.greenhouseLevel;
    }
    return 1;
}

static const RaceScores race_scores = {{
    {5, -10, 15, 40, 40},
    {20, -10, 40, 50, 40},
    {20, -10, 50, 30, 50},
    {20, -10, 50, 40, 60},
    {30, 30, 50, 40, 60},
}};

static u16 table_point_jitter[5][5] = {
    {3, 0, 0, 47, 36},
    {0, 0, 2, 45, 42},
    {5, 0, 10, 60, 5},
    {61, 0, 2, 47, 53},
    {0, 10, 2, 13, 5},
};

/* The winners of event_cmd_contest_stage, as the table's column (CONTEST_RIVAL_PLAYER
 * when the player's dish wins). Column 1 has no winner case. */
enum ContestRival
{
    CONTEST_RIVAL_PLAYER = -1,
    CONTEST_RIVAL_MANNA = 0,
    CONTEST_RIVAL_ANNA = 2,
    CONTEST_RIVAL_DOUG = 3,
    CONTEST_RIVAL_ANN = 4,
};

/* The stages of event_cmd_contest_stage, held in script_vars[stage_slot]. */
enum ContestStage
{
    CONTEST_STAGE_WAIT_DISH = 0,
    CONTEST_STAGE_PLACE_CAST = 1,
    CONTEST_STAGE_JUDGE_SET_MODE = 2,
    CONTEST_STAGE_FADE_SETTLE = 3,
    CONTEST_STAGE_JUDGE_SEASONING = 4,
    CONTEST_STAGE_JUDGE_WALK_1 = 5,
    CONTEST_STAGE_JUDGE_NEVER_TASTED = 6,
    CONTEST_STAGE_JUDGE_WALK_2 = 7,
    CONTEST_STAGE_JUDGE_SWEETNESS = 8,
    CONTEST_STAGE_JUDGE_WALK_3 = 9,
    CONTEST_STAGE_JUDGE_WALK_4 = 10,
    CONTEST_STAGE_JUDGE_LIKE_A_PRO = 11,
    CONTEST_STAGE_JUDGE_WALK_5 = 12,
    CONTEST_STAGE_JUDGE_COMPLEX_TASTE = 13,
    CONTEST_STAGE_JUDGE_WALK_6 = 14,
    CONTEST_STAGE_JUDGE_WALK_7 = 15,
    CONTEST_STAGE_JUDGE_VERDICT = 16,
    CONTEST_STAGE_FADE_OUT_JUDGING = 17,
    CONTEST_STAGE_FADE_OUT_WAIT = 18,
    CONTEST_STAGE_SET_SCENE = 19,
    CONTEST_STAGE_FADE_IN = 20,
    CONTEST_STAGE_FADE_IN_WAIT = 21,
    CONTEST_STAGE_MUSIC_START = 22,
    CONTEST_STAGE_MUSIC_WAIT = 23,
    CONTEST_STAGE_ANNOUNCE = 24,
    CONTEST_STAGE_WINNER_WALK = 25,
    CONTEST_STAGE_WINNER_TURN = 26,
    CONTEST_STAGE_CAMERA_BACK = 27,
    CONTEST_STAGE_COUNT = 28,
};

/* Script command: a five-rival cooking contest, one stage per call. The stage
 * is script_vars[stage_slot] (0..27); stage -1 only places the five entrants.
 * Stage 0 waits for a dish: a cooked dish scores its points, and a spa boiled
 * egg scores 4. Stage 1 places the cast; each rival's points get a random bonus
 * and the highest wins. With an entry, the player wins if their score reaches
 * the best rival. Stages 2..27 are the walk, the judge's comments and the
 * prize, keyed on the winner. The stage advances when its step reports 1 (13
 * jumps to 17 without an entry). Returns 1 only at stage 20 and at the end. */
s32 event_cmd_contest_stage(s16 stage_slot, s16 entry_slot, s32 score_slot, s32 winner_slot)
{
    RaceScores table;
    s32 done;
    s16 step;
    s16 year_row;
    s16 best;
    s16 i;
    s16 message;
    s16 x;
    s16 z;
    s16 next;
    s16 *entry;
    table = race_scores;
    if (stage_slot == -1)
    {
        event_cmd_publish_item(0, -200, -1227, 1402);
        event_cmd_publish_item(2, -200, -653, 1402);
        event_cmd_publish_item(1, -200, -996, -999);
        event_cmd_publish_item(3, -200, 992, -1081);
        event_cmd_publish_item(4, -200, 992, -630);
        return 1;
    }
    step = script_vars[stage_slot].half;
    done = 0;
    switch (step)
    {
    case CONTEST_STAGE_WAIT_DISH:
        entry = &game_state.player.held[1].id;
        if (cooking_table_has_id(entry[0]) == 1)
        {
            done = 1;
            script_vars[score_slot].value = entry[1];
        }
        else if (entry[0] == ITEM_SPA_BOILED_EGG)
        {
            script_vars[score_slot].value = 4;
            done = 1;
        }
        if (done == 1)
        {
            farm_image_load_cell(entry[0]);
            game_state.camera.vy = 1000;
            script_vars[entry_slot].value = entry[0];
            event_item_can_take(entry[0], 1, 0, 1);
        }
        return done;
    case CONTEST_STAGE_PLACE_CAST:
        year_row = game_state.progress.clock.date.year;
        event_cmd_set_position(CHARACTER_GOURMET, 0, -1227, 1010);
        event_cmd_set_sprite(CHARACTER_GOURMET, 0, 2, 1);
        event_cmd_set_position(CHARACTER_MANNA, 0, -1316, 1790);
        event_cmd_set_sprite(CHARACTER_MANNA, 0, 0, 1);
        event_cmd_set_position(CHARACTER_KAREN, 0, -797, 1790);
        event_cmd_set_sprite(CHARACTER_KAREN, 0, 0, 1);
        event_cmd_set_position(CHARACTER_ANNA, 0, -1395, -969);
        event_cmd_set_sprite(CHARACTER_ANNA, 0, 3, 1);
        event_cmd_set_position(CHARACTER_DOUG, 0, 1403, -1357);
        event_cmd_set_sprite(CHARACTER_DOUG, 0, 1, 1);
        event_cmd_set_position(CHARACTER_ANN, 0, 1403, -824);
        event_cmd_set_sprite(CHARACTER_ANN, 0, 1, 1);
        event_cmd_set_position(CHARACTER_BASIL, 0, -2256, -1702);
        event_cmd_set_sprite(CHARACTER_BASIL, 0, 3, 1);
        event_cmd_set_position(CHARACTER_MARY, 0, -2133, -2276);
        event_cmd_set_sprite(CHARACTER_MARY, 0, 3, 1);
        event_cmd_set_position(CHARACTER_GRAY, 0, -1723, -2645);
        event_cmd_set_sprite(CHARACTER_GRAY, 0, 2, 1);
        event_cmd_set_position(CHARACTER_DOCTOR, 0, 819, -2561);
        event_cmd_set_sprite(CHARACTER_DOCTOR, 0, 2, 1);
        event_cmd_set_position(CHARACTER_ELLI, 0, 1803, -2561);
        event_cmd_set_sprite(CHARACTER_ELLI, 0, 2, 1);
        event_cmd_set_position(CHARACTER_GOTZ, 0, -2467, 1468);
        event_cmd_set_sprite(CHARACTER_GOTZ, 0, 3, 1);
        event_cmd_set_position(CHARACTER_PASTOR_CARTER, 0, -2344, 2057);
        event_cmd_set_sprite(CHARACTER_PASTOR_CARTER, 0, 3, 1);
        event_cmd_set_position(CHARACTER_MAYOR_THOMAS, 0, 1345, 2249);
        event_cmd_set_sprite(CHARACTER_MAYOR_THOMAS, 0, 0, 1);
        event_cmd_set_position(CHARACTER_SASHA, 0, -2115, 807);
        event_cmd_set_sprite(CHARACTER_SASHA, 0, 3, 1);
        if (event_cmd_villager_available(CHARACTER_POPURI, 0, 0, 0) == 1)
        {
            event_cmd_set_position(CHARACTER_POPURI, 0, 1311, -2561);
            event_cmd_set_sprite(CHARACTER_POPURI, 0, 2, 1);
        }
        /* The table has four year rows; later years use the last one. */
        if (year_row >= 5)
        {
            year_row = 4;
        }
        best = 0;
        for (i = 0; i < 5; i++)
        {
            table.points[year_row][i] += rand_range(table_point_jitter[year_row][i]);
            if (table.points[year_row][best] < table.points[year_row][i])
            {
                best = i;
            }
        }
        if (script_vars[entry_slot].value != 0)
        {
            event_cmd_set_player_position(PLAYER_POSITION_SET, 0, 1001, 1790);
            event_cmd_set_facing(0, 0, 0, 0);
            game_state.camera.vy = -400;
            game_state.camera.vx = 887;
            game_state.camera.vz = 1352;
            if (table.points[year_row][best] <= script_vars[score_slot].value)
            {
                best = -1;
                event_cmd_adjust_present_villagers(EVENT_VAR_VILLAGER_FRIENDSHIP, 1, 20, 2000);
            }
        }
        switch (best)
        {
        case CONTEST_RIVAL_PLAYER:
            script_vars[winner_slot].value = CHARACTER_PLAYER;
            break;
        case CONTEST_RIVAL_MANNA:
            script_vars[winner_slot].value = CHARACTER_MANNA;
            break;
        case CONTEST_RIVAL_ANNA:
            script_vars[winner_slot].value = CHARACTER_ANNA;
            break;
        case CONTEST_RIVAL_DOUG:
            script_vars[winner_slot].value = CHARACTER_DOUG;
            break;
        case CONTEST_RIVAL_ANN:
            script_vars[winner_slot].value = CHARACTER_ANN;
            break;
        }
        event_cmd_load_record(CHARACTER_GOURMET, 0, 0, 0);
        done = 1;
        break;
    case CONTEST_STAGE_JUDGE_SET_MODE:
        done = event_cmd_set_mode(19, 1, 0, 0);
        break;
    case CONTEST_STAGE_FADE_SETTLE:
        done = event_cmd_fade(1, 0, 0, 0);
        if (done == 1)
        {
            event_cmd_select_mode(0, 0, 0, 0);
        }
        break;
    case CONTEST_STAGE_JUDGE_SEASONING:
        event_cmd_set_sprite(CHARACTER_GOURMET, 7, 0, 0);
        done = event_message_start(1, CONTEST_JUDGE_LIGHT_SEASONING, -1, 0);
        break;
    case CONTEST_STAGE_JUDGE_WALK_1:
        done = farm_nudge_cue(CHARACTER_GOURMET, 3, -653, 4);
        break;
    case CONTEST_STAGE_JUDGE_NEVER_TASTED:
        event_cmd_set_sprite(CHARACTER_GOURMET, 7, 0, 0);
        done = event_message_start(1, CONTEST_JUDGE_NEVER_TASTED, -1, 0);
        break;
    case CONTEST_STAGE_JUDGE_WALK_2:
        done = farm_nudge_cue(CHARACTER_GOURMET, 0, -999, 1);
        break;
    case CONTEST_STAGE_JUDGE_SWEETNESS:
        event_cmd_set_sprite(CHARACTER_GOURMET, 6, 0, 0);
        done = event_message_start(1, CONTEST_JUDGE_RIGHT_SWEETNESS, -1, 0);
        break;
    case CONTEST_STAGE_JUDGE_WALK_3:
        done = farm_nudge_cue(CHARACTER_GOURMET, 0, -1081, 1);
        break;
    case CONTEST_STAGE_JUDGE_WALK_4:
        done = farm_nudge_cue(CHARACTER_GOURMET, 3, 618, 4);
        break;
    case CONTEST_STAGE_JUDGE_LIKE_A_PRO:
        event_cmd_set_sprite(CHARACTER_GOURMET, 8, 0, 0);
        done = event_message_start(1, CONTEST_JUDGE_LIKE_A_PRO, -1, 0);
        break;
    case CONTEST_STAGE_JUDGE_WALK_5:
        done = farm_nudge_cue(CHARACTER_GOURMET, 2, -630, 3);
        break;
    case CONTEST_STAGE_JUDGE_COMPLEX_TASTE:
        event_cmd_set_sprite(CHARACTER_GOURMET, 8, 0, 0);
        done = event_message_start(1, CONTEST_JUDGE_COMPLEX_TASTE, -1, 0);
        break;
    case CONTEST_STAGE_JUDGE_WALK_6:
        done = farm_nudge_cue(CHARACTER_GOURMET, 2, 969, 3);
        break;
    case CONTEST_STAGE_JUDGE_WALK_7:
        done = farm_nudge_cue(CHARACTER_GOURMET, 3, 987, 4);
        break;
    case CONTEST_STAGE_JUDGE_VERDICT:
        event_cmd_set_sprite(CHARACTER_GOURMET, 7, 0, 0);
        if (script_vars[winner_slot].value == CHARACTER_PLAYER)
        {
            done = event_message_start(1, CONTEST_JUDGE_WONDERFUL, -1, 0);
        }
        else
        {
            done = event_message_start(1, CONTEST_JUDGE_JUST_AS_IT_IS, -1, 0);
        }
        break;
    case CONTEST_STAGE_FADE_OUT_JUDGING:
        event_cmd_set_sprite(CHARACTER_GOURMET, 0, 2, 1);
        event_cmd_fade_music(0, 60, 1, 0);
        event_cmd_fade(0, 60, 1, 0);
        done = 1;
        break;
    case CONTEST_STAGE_FADE_OUT_WAIT:
        done = event_cmd_fade(0, 0, 0, 0);
        break;
    case CONTEST_STAGE_SET_SCENE:
        event_cmd_skip_to_time(15, 0, 0, 0);
        event_cmd_set_position(CHARACTER_GOURMET, 0, 78, 2255);
        event_cmd_set_sprite(CHARACTER_GOURMET, 0, 0, 1);
        event_cmd_set_position(CHARACTER_MAYOR_THOMAS, 0, -332, 2255);
        event_cmd_set_sprite(CHARACTER_MAYOR_THOMAS, 0, 0, 1);
        if (script_vars[entry_slot].value != 0)
        {
            event_cmd_set_player_position(PLAYER_POSITION_SET, 0, -127, 205);
        }
        else
        {
            event_cmd_set_player_position(PLAYER_POSITION_SET, 0, 488, -861);
        }
        event_cmd_set_facing(2, 0, 0, 0);
        event_cmd_set_position(CHARACTER_MANNA, 0, -783, 738);
        event_cmd_set_sprite(CHARACTER_MANNA, 0, 2, 1);
        event_cmd_set_position(CHARACTER_KAREN, 0, 529, 205);
        event_cmd_set_sprite(CHARACTER_KAREN, 0, 2, 1);
        switch (script_vars[winner_slot].value)
        {
        case CHARACTER_PLAYER:
            x = -127;
            z = 205;
            break;
        case CHARACTER_MANNA:
            x = -783;
            z = 738;
            break;
        case CHARACTER_DOUG:
            x = -783;
            z = 205;
            break;
        case CHARACTER_ANN:
            x = 529;
            z = 738;
            break;
        default:
            x = -127;
            z = 738;
            break;
        }
        event_cmd_set_position(CHARACTER_ANNA, 0, x, z);
        event_cmd_set_sprite(CHARACTER_ANNA, 0, 2, 1);
        event_cmd_set_position(CHARACTER_DOUG, 0, -783, 205);
        event_cmd_set_sprite(CHARACTER_DOUG, 0, 2, 1);
        event_cmd_set_position(CHARACTER_ANN, 0, 529, 738);
        event_cmd_set_sprite(CHARACTER_ANN, 0, 2, 1);
        if (script_vars[winner_slot].value == CHARACTER_PLAYER)
        {
            event_cmd_set_player_position(PLAYER_POSITION_SET, 0, -127, 738);
        }
        else
        {
            event_cmd_set_position(script_vars[winner_slot].half, 0, -127, 738);
        }
        event_cmd_camera_glide(-1, 0, 41, 1148);
        done = event_cmd_set_mode(15, 0, 0, 0);
        break;
    case CONTEST_STAGE_FADE_IN:
        event_cmd_fade_music(1, 60, 1, 0);
        event_cmd_fade(1, 60, 1, 0);
        done = 1;
        break;
    case CONTEST_STAGE_FADE_IN_WAIT:
        done = event_cmd_fade(1, 0, 0, 0);
        break;
    case CONTEST_STAGE_MUSIC_START:
        event_cmd_wait_control(3, 0, 1, 0);
        done = 1;
        break;
    case CONTEST_STAGE_MUSIC_WAIT:
        done = event_cmd_wait_control(3, 0, 0, 0);
        if (done == 1)
        {
            event_cmd_select_mode(0, 0, 0, 0);
        }
        break;
    case CONTEST_STAGE_ANNOUNCE:
        switch (script_vars[winner_slot].value)
        {
        case CHARACTER_MANNA:
            message = CONTEST_WINNER_MANNA;
            break;
        case CHARACTER_ANNA:
            message = CONTEST_WINNER_ANNA;
            break;
        case CHARACTER_DOUG:
            message = CONTEST_WINNER_DOUG;
            break;
        case CHARACTER_ANN:
            message = CONTEST_WINNER_ANN;
            break;
        case CHARACTER_PLAYER:
        default:
            message = CONTEST_WINNER_PLAYER;
            break;
        }
        done = event_message_start(1, message, -1, 0);
        break;
    case CONTEST_STAGE_WINNER_WALK:
        if (script_vars[winner_slot].value == CHARACTER_PLAYER)
        {
            done = event_cmd_player_step(0, 2, 1444, 0);
        }
        else
        {
            done = farm_nudge_cue(script_vars[winner_slot].half, 2, 1444, 3);
        }
        break;
    case CONTEST_STAGE_WINNER_TURN:
        if (script_vars[winner_slot].value == CHARACTER_PLAYER)
        {
            event_cmd_set_facing(2, 0, 0, 0);
        }
        else
        {
            event_cmd_set_sprite(script_vars[winner_slot].half, 0, 2, 1);
        }
        switch (script_vars[winner_slot].value)
        {
        case CHARACTER_MANNA:
            message = CONTEST_WINNER_MANNA + 1;
            break;
        case CHARACTER_ANNA:
            message = CONTEST_WINNER_ANNA + 1;
            break;
        case CHARACTER_DOUG:
            message = CONTEST_WINNER_DOUG + 1;
            break;
        case CHARACTER_ANN:
            message = CONTEST_WINNER_ANN + 1;
            break;
        case CHARACTER_PLAYER:
        default:
            message = CONTEST_WINNER_PLAYER + 1;
            break;
        }
        done = event_message_start(1, message, -1, 0);
        break;
    case CONTEST_STAGE_CAMERA_BACK:
        done = event_cmd_load_record(-2, 0, 1, 2);
        break;
    }
    if (done == 1)
    {
        if (step == CONTEST_STAGE_JUDGE_COMPLEX_TASTE && script_vars[entry_slot].value == 0)
        {
            next = CONTEST_STAGE_FADE_OUT_JUDGING;
        }
        else
        {
            next = step + 1;
        }
        step = next;
    }
    script_vars[stage_slot].value = step;
    /* Only stage 20 and the end report 1; the stages between them never do. */
    if (step != CONTEST_STAGE_FADE_IN && step < CONTEST_STAGE_COUNT)
    {
        done = 0;
    }
    return done;
}

/* Applies an update to every present villager. For affection or friendship, the
 * bachelorette gets `alt_operation` on affection and everyone else gets
 * `operation` on friendship. */
s32 event_cmd_adjust_present_villagers(s16 id, s16 arg, s32 operation, s32 alt_operation)
{
    CharacterId villager;

    for (villager = 1; villager < CHARACTER_COUNT; villager++)
    {
        if (game_state.villagers[villager].flags & VILLAGER_FLAG_PRESENT)
        {
            if ((u32)(id - EVENT_VAR_VILLAGER_AFFECTION) < 2)
            {
                if (villager_is_bachelorette(villager) == 1)
                {
                    event_var_update(EVENT_VAR_VILLAGER_AFFECTION, villager, alt_operation, arg);
                }
                else
                {
                    event_var_update(EVENT_VAR_VILLAGER_FRIENDSHIP, villager, operation, arg);
                }
            }
            else
            {
                event_var_update(id, villager, operation, arg);
            }
        }
    }
    return 1;
}

s32 event_cmd_select_livestock(s16 kind, s16 any_arg, s32 slot, s32 unused3)
{
    s16 value = 0;
    s32 result;
    if (any_arg == 0)
        value = -1;
    result = shop_record_list(kind, 1, value);
    script_vars[slot].value = result;
    if (result == -2)
        return 1;
    return result != -1;
}

/* The modes of event_cmd_player_walk. */
enum PlayerWalkMode
{
    PLAYER_WALK_TURN_AND_GO = 2, /* turns to the axis the player is furthest along, then walks */
    PLAYER_WALK_GO = 1,
    PLAYER_WALK_STOP = -1,
};

/* Walks the player. Mode 2 first turns them to face the axis they are furthest
 * along; modes 1 and 2 walk for amount / 2 frames (12 when zero). Mode -1 stops
 * the walk. Returns 1, or for other modes whether the player is idle. */
s32 event_cmd_player_walk(s16 mode, s16 amount, s32 unused2, s32 unused3)
{
    s32 x;
    s32 z;
    s32 direction;
    s32 duration;
    s32 result;
    s32 busy;

    switch (mode)
    {
    case PLAYER_WALK_TURN_AND_GO:
    {
        VECTOR *position = &game_state.player.position;

        x = position->vx;
        z = position->vz;
        if (abs(x) > abs(z))
        {
            direction = (x > 0) ? FACING_RIGHT : FACING_LEFT;
        }
        else
        {
            direction = (z > 0) ? FACING_UP : FACING_DOWN;
        }
        event_cmd_set_facing(direction, 0, 0, 0);
    }
        /* fallthrough */
    case PLAYER_WALK_GO:
        if (amount != 0)
        {
            duration = amount / 2;
        }
        else
        {
            duration = 12;
        }
        game_state.player.stateFlags |= PLAYER_FLAG_SCRIPTED;
        player_walk_start(duration);
        result = 1;
        break;
    case PLAYER_WALK_STOP:
        game_state.player.countdown = 0;
        game_state.player.stateFlags &= ~(PLAYER_FLAG_AUTO_WALK | PLAYER_FLAG_SCRIPTED);
        result = 1;
        break;
    default:
        busy = game_state.player.stateFlags & PLAYER_FLAG_AUTO_WALK;
        result = busy == 0;
        break;
    }
    return result;
}

/* Records `value` for villager `id` in the first free or matching pending-result slot (five
 * slots). Returns 0 when none is free. */
s32 event_cmd_set_pending_pair(s16 index, s16 value, s32 indirect, s32 unused3)
{
    s32 id;
    s32 i;
    PendingResult *p;
    if (indirect)
        id = script_vars[index].value;
    else
        id = index;
    p = game_state.pendingResults;
    i = 0;
    do
    {
        if (p->id == -1 || p->id == id)
        {
            p->id = id;
            p->value = value;
            return 1;
        }
        i++;
        p++;
    } while (i < 5);
    return 0;
}

static const FarmCandidates candidate_records = {
    CHARACTER_KAREN, CHARACTER_ANN, CHARACTER_ELLI, CHARACTER_MARY, CHARACTER_POPURI};

/* Script command: picks one of the five candidate brides, at random among those
 * with the highest value. Candidates who are married, or ruled out by the
 * game, are dropped (unless `all` is set). The pick goes into script_vars[out].
 * Returns 0 when none is left. */
s32 event_cmd_pick_bride(s16 all, s16 out, s32 unused2, s32 unused3)
{
    FarmCandidates candidates;
    s16 count; /* the dropped candidates, then the ties, then the pick */
    s16 i;
    u16 best;
    u16 value;

    candidates = candidate_records;
    count = 0;
    if (all == 0)
    {
        for (i = 0; i < 4; i++)
        {
            if (event_flag_get(EVENT_FLAG_KAREN_RICK_MARRIED + i) == 1)
            {
                candidates.ids[i] = -1;
                count++;
            }
        }
        if (event_cmd_mode_check(0) == 1)
        {
            candidates.ids[4] = -1;
            count++;
        }
    }
    if (candidates.ids[4] != -1 && event_cmd_villager_available(candidates.ids[4], 0, 0, 0) == 0)
    {
        candidates.ids[4] = -1;
        count++;
    }
    if (count == 5)
    {
        return 0;
    }
    best = 0;
    count = 0;
    for (i = 0; i < 5; i++)
    {
        if (candidates.ids[i] != -1)
        {
            value = game_state.villagers[candidates.ids[i]].affection;
            if (best < value)
            {
                best = value;
                count = 1;
            }
            else if (best == value)
            {
                count++;
            }
        }
    }
    count = rand_range((u16)count);
    for (i = 0; i < 5; i++)
    {
        if (candidates.ids[i] != -1 && best == game_state.villagers[candidates.ids[i]].affection)
        {
            if (count == 0)
            {
                break;
            }
            count--;
        }
    }
    if (out != -1)
    {
        script_vars[out].value = candidates.ids[i];
    }
    return 1;
}

/* A helper is given a gift. Without a gift the result is 11 the first time and
 * 12 after that. A gift the helper will not take returns 0. Otherwise the
 * helper's affection rises, and the result is 13, or 14 once all seven have been
 * given, when everyone's affection also rises by 10. The result goes into
 * script_vars[result_slot]. */
s32 event_cmd_affection_add(s16 result_slot, s16 helper_slot, s32 given_slot, s32 unused3)
{
    s32 villager;
    FarmHelper *record;
    s32 grade;
    s16 gain;
    s16 i;

    villager = script_vars[helper_slot].half;
    record = &game_state.helpers[villager];
    record->actor.flags |= ACTOR_FLAG_COW_CUE_TODAY;
    if (event_cmd_gift_reaction(CHARACTER_UNNAMED_27, result_slot, 0, 0) == 0 || script_vars[result_slot].value == 0)
    {
        if (script_vars[11 + villager].value != 2)
        {
            script_vars[11 + villager].value = 1;
            script_vars[given_slot].value = 1;
            script_vars[result_slot].value = 11;
        }
        else
        {
            script_vars[result_slot].value = 12;
        }
        return 1;
    }
    grade = script_vars[result_slot].value;
    if (grade < 4)
    {
        if (grade > 0)
        {
            if (script_vars[given_slot].value != 0)
            {
                if (!(record->flags & 0x10))
                {
                    record->flags |= 0x10;
                    gain = gift_reaction_delta(villager + EVENT_ACTOR_FIRST_HELPER, 0, script_vars[result_slot].half - 1);
                }
                else
                {
                    gain = gift_reaction_delta(villager + EVENT_ACTOR_FIRST_HELPER, 2, script_vars[result_slot].half - 1);
                }
                if (record->actor.affection < 255 - gain)
                {
                    record->actor.affection += gain;
                }
                else
                {
                    record->actor.affection = 255;
                }
                script_vars[11 + villager].value = 2;
                for (i = 0; i < 7 && script_vars[11 + i].value == 2; i++)
                {
                }
                if (i != 7)
                {
                    script_vars[result_slot].value = 13;
                }
                else
                {
                    script_vars[result_slot].value = 14;
                    record = game_state.helpers;
                    for (i = 0; i < HELPER_COUNT; i++, record++)
                    {
                        if (record->actor.affection < 245)
                        {
                            record->actor.affection += 10;
                        }
                        else
                        {
                            record->actor.affection = 255;
                        }
                    }
                }
                return 1;
            }
        }
    }
    harvest_helper_set_action(record, 6);
    return 0;
}

/* Turns an actor, or the player, to face another, by the heading between them. */
s32 event_cmd_result_direction(s16 actor_arg, s16 indirect, s32 animation, s32 target_arg)
{
    s16 actor;
    s16 target;
    s32 direction;
    VECTOR *actor_point;
    VECTOR *target_point;
    if (indirect & 1)
        actor = script_vars[actor_arg].half;
    else
        actor = actor_arg;
    if (indirect & 2)
        target = script_vars[target_arg].half;
    else
        target = target_arg;
    if (actor == CHARACTER_PLAYER)
        actor_point = &game_state.player.position;
    else
        actor_point = &game_state.villagers[actor].ePosition;
    if (target == CHARACTER_PLAYER)
        target_point = &game_state.player.position;
    else
        target_point = &game_state.villagers[target].ePosition;
    direction = farm_vec_heading(target_point, actor_point);
    if (actor == CHARACTER_PLAYER)
        event_cmd_set_facing(direction, 0, 0, 0);
    else
    {
        event_cmd_set_sprite(actor, animation, direction, 1);
        event_cmd_set_actor_hidden(actor, 0, 0, 0);
    }
    return 1;
}

/* Moves an actor's position, or the player's, by twice each delta. */
s32 event_actor_position(CharacterId character, s16 dy, s32 dx, s32 dz)
{
    VECTOR *p;
    if (character == CHARACTER_PLAYER)
        p = &game_state.player.position;
    else
        p = &game_state.villagers[character].ePosition;
    p->vy += dy << 1;
    p->vx += dx << 1;
    p->vz += dz << 1;
    return 1;
}

/* Whether a villager can take part in an event. `villager_arg` is the id, or
 * the id in script_vars when `indirect` is set. Popuri is away outside summer
 * once she and Kai are married. Cliff is away while flag 129 is set, or while
 * his day counter is not 0xFF. */
s32 event_cmd_villager_available(s16 villager_arg, s16 indirect, s32 unused2, s32 unused3)
{
    s16 villager;

    if (indirect)
    {
        villager = script_vars[villager_arg].half;
    }
    else
    {
        villager = villager_arg;
    }

    switch (villager)
    {
    case CHARACTER_POPURI:
        if (event_flag_get(EVENT_FLAG_POPURI_KAI_MARRIED) == 1 && game_state.progress.clock.date.season != SEASON_SUMMER)
        {
            return 0;
        }
        break;
    case CHARACTER_CLIFF:
        if (event_flag_get(EVENT_FLAG_129) == 1)
        {
            return 0;
        }
        if (game_state.days.count0[7] != 0xFF)
        {
            return 0;
        }
        break;
    }
    return 1;
}

/* Script command: picks the judge's comment on the tomato contest into
 * script_vars[out], by who speaks and how the teams did. */
s32 event_cmd_tomato_comment(s16 speaker, s16 winner_slot, s32 player_team_slot, s32 out)
{
    s16 speaker_id;
    s16 winning_team;
    s16 player_team;
    s16 index;
    s16 own_team;
    s16 message;

    speaker_id = script_vars[speaker].half;
    winning_team = script_vars[winner_slot].half;
    player_team = script_vars[player_team_slot].half;
    switch (speaker_id)
    {
    case CHARACTER_RICK:
        index = 0;
        own_team = 20;
        break;
    case CHARACTER_POPURI:
        index = 1;
        own_team = 20;
        break;
    case CHARACTER_MAY:
        index = 2;
        own_team = 9;
        break;
    case CHARACTER_GRAY:
        index = 3;
        own_team = 7;
        break;
    case CHARACTER_DUKE:
    case CHARACTER_MANNA:
        own_team = 22;
        index = 4;
        break;
    case CHARACTER_STU:
        index = 5;
        own_team = 9;
        break;
    case CHARACTER_ELLI:
        index = 6;
        own_team = 9;
        break;
    case CHARACTER_CLIFF:
        own_team = 22;
        index = 7;
        break;
    case CHARACTER_DOUG:
        index = 8;
        own_team = 7;
        break;
    case CHARACTER_ANN:
        index = 9;
        own_team = 7;
        break;
    case CHARACTER_KAI:
        own_team = 20;
        index = 10;
        break;
    default:
        index = 11;
        own_team = -1;
        break;
    }
    if (player_team == -1)
    {
        if (index >= 5 || speaker_id == CHARACTER_MANNA)
        {
            message = index + TOMATO_COMMENT_ABSENT + 1;
        }
        else
        {
            message = index + TOMATO_COMMENT_ABSENT;
        }
    }
    else if (winning_team == player_team)
    {
        index = index * 2;
        switch (speaker_id)
        {
        case CHARACTER_CLIFF:
        case CHARACTER_GRAY:
        case CHARACTER_MAY:
        case CHARACTER_KAI:
            if (player_team == own_team)
            {
                message = index + TOMATO_COMMENT_TEAMS_MET;
            }
            else
            {
                message = index + TOMATO_COMMENT_TEAMS_MET + 1;
            }
            break;
        default:
            if (player_team == own_team)
            {
                message = index + TOMATO_COMMENT_TEAMS_MET + 1;
            }
            else
            {
                message = index + TOMATO_COMMENT_TEAMS_MET;
            }
            break;
        }
    }
    else
    {
        index = index * 3;
        switch (speaker_id)
        {
        case CHARACTER_CLIFF:
        case CHARACTER_GRAY:
        case CHARACTER_MAY:
        case CHARACTER_KAI:
            if (player_team == own_team)
            {
                message = index + TOMATO_COMMENT_LOSS;
            }
            else if (winning_team == own_team)
            {
                message = index + TOMATO_COMMENT_LOSS + 1;
            }
            else
            {
                message = index + TOMATO_COMMENT_LOSS + 2;
            }
            break;
        default:
            if (player_team == own_team)
            {
                message = index + TOMATO_COMMENT_LOSS + 2;
            }
            else if (winning_team == own_team)
            {
                message = index + TOMATO_COMMENT_LOSS + 1;
            }
            else
            {
                message = index + TOMATO_COMMENT_LOSS;
            }
            break;
        }
    }
    script_vars[out].value = message;
    return 1;
}

/* The grey the fireworks flash fades to. */
static const FarmFlashColour flash_colour = {0x00A0, 0x00A0, 0x00A0};

/* The cues of the fireworks timer, script_vars[base] (counted up 5 a call). */
enum FireworkCue
{
    FIREWORK_CUE_LAUNCH = 0,
    FIREWORK_CUE_COLOUR = 300,
    FIREWORK_CUE_BURST = 330,
    FIREWORK_CUE_FADE_END = 360,
};

/* Script command: a fireworks flash, timed by script_vars[base] (+5 per call).
 * At 0 it plays the launch sound, at 300 it picks a bright random colour, and
 * at 330 it plays the burst sound. Past 360 the flash fades to grey and
 * restarts. While a colour is set, the screen is flashed with it and the colour
 * fades by 15 per call. `restart` re-arms the timer with a random delay. */
s32 event_cmd_fireworks_flash(s16 restart, s16 base, s32 unused2, s32 unused3)
{
    FarmFlashColour colour;
    s16 i;

    colour = flash_colour;
    if (restart == 0)
    {
        script_vars[base].value += 5;
        switch (script_vars[base].value)
        {
        case FIREWORK_CUE_LAUNCH:
            sound_play(SOUND_FIREWORK_LAUNCH);
            break;
        case FIREWORK_CUE_COLOUR:
            colour.rgb[(s16)rand_range(3)] = 0xFF;
            colour.rgb[(s16)rand_range(3)] = 0xFF;
            for (i = 0; i < 3; i++)
            {
                script_vars[base + 1 + i].value = colour.rgb[i] - (s16)rand_range(32);
            }
            break;
        case FIREWORK_CUE_BURST:
            sound_play(SOUND_FIREWORK_BURST);
            break;
        default:
            if (script_vars[base].value >= FIREWORK_CUE_FADE_END)
            {
                for (i = 0; i < 3; i++)
                {
                    colour.rgb[i] = 0xA0;
                }
                script_vars[base + 1].value = 0;
                restart = 1;
            }
            break;
        }
        if (script_vars[base + 1].value != 0)
        {
            prim_add_rect_mode(&current_frame->otTags[0][EVENT_FILL_SLOT_FRONT], 0, 0, EVENT_SCREEN_WIDTH, EVENT_SCREEN_HEIGHT,
                               script_vars[base + 3].byte | ((u16)script_vars[base + 2].half << 8) |
                                   (script_vars[base + 1].byte << 16),
                               1, 1);
            for (i = 1; i < 4; i++)
            {
                script_vars[base + i].value -= 15;
                if (script_vars[base + i].value < 0)
                {
                    script_vars[base + i].value = 0;
                }
            }
        }
    }
    if (restart != 0)
    {
        script_vars[base].value = -((s16)rand_range(80) * 5) - 200;
    }
    return 1;
}

/* Draws a result cell's sprite in the result area. */
s32 event_cmd_result_cell(CharacterId character, s16 slot_arg, s32 unused2, s32 unused3)
{
    AnimState *sprite;
    s32 animation;

    if ((u16)slot_arg >= 10)
    {
        return 0;
    }
    sprite = event_var_address(EVENT_VAR_VILLAGER_EVENT_SPRITE, character, 0);
    animation = script_vars[slot_arg].value;
    if (sprite->animation != animation)
    {
        anim_select_range(sprite, animation);
    }
    prim_add_tile(&current_frame->otTags[0][4], 0, 0, EVENT_SCREEN_WIDTH, EVENT_SCREEN_HEIGHT, 0, 0);
    anim_draw_ft4_advance(&current_frame->otTags[0][2], sprite, 80, 60);
    return 1;
}

/* The steps of event_cmd_camera_pull_back, held in script_vars[state]. */
enum CameraPullBackStep
{
    CAMERA_PULL_BACK_LATCH = 0,
    CAMERA_PULL_BACK_EASE = 1,
    CAMERA_PULL_BACK_DONE = 2,
};

/* A camera pull-back eases over this many steps. */
#define CAMERA_PULL_BACK_STEPS 177

/* One step of a camera pull-back, driven by script_vars[state]. 0 latches the
 * start and the targets, then moves on. 1 eases all three over 177 steps, counted
 * in script_vars[counter]. 2 and anything else reports done (1). `reset` first
 * resets the farm's scroll. */
s32 event_cmd_camera_pull_back(s16 state, s16 counter, s32 unused, s32 reset)
{
    s32 x;
    s32 z;
    s32 v;
    s32 done;

    done = 0;
    if (reset)
    {
        farm_scroll_reset();
    }
    switch (script_vars[state].value)
    {
    case CAMERA_PULL_BACK_LATCH:
        scroll_lerp_x_to = -800;
        /* The record is cleared here: the camera is pulled back from the farm, not a record. */
        event_current_record = EVENT_RECORD_NONE;
        script_vars[state].value = CAMERA_PULL_BACK_EASE;
        scroll_lerp_x_from = farm_area.mode.scroll.x;
        scroll_lerp_z_from = farm_area.mode.scroll.z;
        scroll_lerp_y_from = farm_area.mode.scroll.centre.vy;
        scroll_lerp_z_to = farm_area.mode.scroll.z - 1100;
        scroll_lerp_y_to = farm_area.mode.scroll.centre.vy - 1000;
        break;
    case CAMERA_PULL_BACK_EASE:
        math_lerp_scalar(script_vars[counter].value, CAMERA_PULL_BACK_STEPS, scroll_lerp_x_from, scroll_lerp_x_to, &x);
        math_lerp_scalar(script_vars[counter].value, CAMERA_PULL_BACK_STEPS, scroll_lerp_z_from, scroll_lerp_z_to, &z);
        math_lerp_scalar(script_vars[counter].value, CAMERA_PULL_BACK_STEPS, scroll_lerp_y_from, scroll_lerp_y_to, &v);
        farm_area.mode.scroll.x = x;
        farm_area.mode.scroll.z = z;
        farm_area.mode.scroll.centre.vy = v;
        script_vars[counter].value++;
        if (script_vars[counter].value >= CAMERA_PULL_BACK_STEPS)
        {
            script_vars[state].value = CAMERA_PULL_BACK_DONE;
        }
        break;
    case CAMERA_PULL_BACK_DONE:
    default:
        done = 1;
        break;
    }
    return done;
}

/* Mode 0: choose the farm's action state and publish it; otherwise copy it to the caller's slot. */
s32 event_cmd_action_state(s16 mode, s16 slot, s32 unused2, s32 unused3)
{
    s32 route;
    s32 selected;

    if (mode == 0)
    {
        route = ending_married_bride();
        if (route >= 0)
        {
            game_state.ending.route = route;
        }
        else
        {
            selected = ending_favourite_bride();
            if (game_state.progress.estate.discardCount >= 10)
            {
                game_state.ending.route = ENDING_ROUTE_DISCARDS;
            }
            else if (ending_best_friendship() < 50)
            {
                game_state.ending.route = ENDING_ROUTE_FEW_FRIENDS;
            }
            else if (ending_shipment_total() < 11)
            {
                game_state.ending.route = ENDING_ROUTE_FEW_SHIPMENTS;
            }
            else if (ending_planted_field_percent() < 6)
            {
                game_state.ending.route = ENDING_ROUTE_BARE_FIELD;
            }
            else if (selected == -1)
            {
                game_state.ending.route = ENDING_ROUTE_NO_SWEETHEART;
            }
            else if (game_state.animals.dog.actor.affection < 6)
            {
                game_state.ending.route = ENDING_ROUTE_DOG_UNLOVED;
            }
            else
            {
                if (selected >= 5)
                    selected = 0;
                game_state.ending.route = ENDING_ROUTE_SWEETHEART + selected;
            }
        }
    }
    else
    {
        script_vars[slot].value = game_state.ending.route;
    }
    return 1;
}

static u8 ending_route_offsets[10] = {0x01, 0x04, 0x03, 0x02, 0x00, 0x06, 0x09, 0x08, 0x07, 0x05};

/* Stores the ending's message ids in the four slots for the chosen route: the crowd, the cliff's
 * slip, the rival's praise and the invitation. */
s32 event_cmd_current_record(s16 route_slot, s16 second_slot, s32 third_slot, s32 fourth_slot)
{
    s32 route;
    event_cmd_action_state(1, route_slot, 0, 0);
    route = script_vars[route_slot].value;
    script_vars[route_slot].value = ENDING_MESSAGE_CROWD;
    if (route == 1)
        script_vars[second_slot].value = ENDING_MESSAGE_CLIFF_SLIPS + 2;
    else if (route == 2)
        script_vars[second_slot].value = ENDING_MESSAGE_CLIFF_SLIPS + 3;
    else if (route == 4)
        script_vars[second_slot].value = ENDING_MESSAGE_CLIFF_SLIPS + 1;
    else
        script_vars[second_slot].value = ENDING_MESSAGE_CLIFF_SLIPS;
    if (game_state.villagers[CHARACTER_CLIFF].place.value != PLACE_13)
        script_vars[second_slot].value += 4;
    if (route == 3)
        script_vars[third_slot].value = ENDING_MESSAGE_RIVAL_PRAISE + 1;
    else if (route == 0)
        script_vars[third_slot].value = ENDING_MESSAGE_RIVAL_PRAISE + 2;
    else
        script_vars[third_slot].value = ENDING_MESSAGE_RIVAL_PRAISE;
    script_vars[fourth_slot].value = ending_route_offsets[route] + ENDING_MESSAGE_INVITATION;
    return 1;
}

static ResultEntry result_entries[5] = {
    {1927, 1937, 1947},
    {1930, 1940, 1950},
    {1929, 1939, 1949},
    {1928, 1938, 1948},
    {1926, 1936, 1946},
};

/* Stores the route's three result ids in the slots. On the route's second half the first and
 * third gain 5. */
s32 event_cmd_ending_text(s16 route_slot_arg, s16 second_slot_arg, s32 third_slot, s32 unused3)
{
    s32 value;
    s32 second_half;

    event_cmd_action_state(1, route_slot_arg, 0, 0);
    value = script_vars[route_slot_arg].value;
    if (value >= 10)
    {
        value = 0;
    }
    second_half = 0;
    if (value < 5)
    {
        second_half = 0;
    }
    else
    {
        value -= 5;
        second_half = 1;
    }
    script_vars[route_slot_arg].value = result_entries[value].first;
    script_vars[second_slot_arg].value = result_entries[value].second;
    script_vars[third_slot].value = result_entries[value].third;
    if (second_half != 0)
    {
        script_vars[route_slot_arg].value += 5;
        script_vars[third_slot].value += 5;
    }
    return 1;
}

/* The record the camera is on (EVENT_RECORD_NONE for none). */
s16 event_camera_record(void)
{
    return event_current_record;
}

s32 event_vars_clear(s32 unused0, s32 unused1, s32 unused2, s32 unused3)
{
    memset(script_vars, 0, sizeof(script_vars));
    return 1;
}

static s32 event_var_read(s32 index, s32 value)
{
    return event_var_get(index, value);
}

static int event_always_true(void)
{
    return 1;
}

/* The stores event_items_take searches, by kind. */
enum EventItemStore
{
    EVENT_STORE_HELD = 0,
    EVENT_STORE_RUCKSACK = 1,
    EVENT_STORE_BOXES = 2,
    EVENT_STORE_FRIDGE = 3,
};

/* Remove up to `count` copies of item `item` from the player's stores and
 * count the copies seen. Store kinds 0..`which` are searched (only kind 1
 * when `which` is -1): 0 the held item (`where` picks the row), 1 the
 * rucksack, 2 the boxes, 3 the fridge (none when `where` is set). */
s32 event_items_take(s16 item, s16 where, s16 which, s32 count)
{
    FarmStoredItem *list;
    s32 n;
    s16 found;
    s32 first;
    s32 kind;
    s32 i;

    list = 0;
    n = 0;
    found = 0;
    if (which == -1)
    {
        first = 1;
        which = 1;
    }
    else
    {
        first = 0;
    }
    for (kind = first; kind <= which; kind++)
    {
        switch (kind)
        {
        case EVENT_STORE_HELD:
            list = &game_state.player.held[where];
            n = 1;
            break;
        case EVENT_STORE_RUCKSACK:
            if (where == 0)
            {
                list = game_state.player.tools;
            }
            else
            {
                list = game_state.player.items;
            }
            n = math_int_pow(2, game_state.player.rucksackLevel + 1);
            break;
        case EVENT_STORE_BOXES:
            if (where == 0)
            {
                list = game_state.progress.estate.toolboxItems;
                n = ITEM_STORE_SLOTS;
            }
            else
            {
                list = game_state.progress.estate.iceboxItems;
                n = ITEM_STORE_SLOTS;
            }
            break;
        case EVENT_STORE_FRIDGE:
            list = 0;
            if (where == 0)
            {
                list = game_state.progress.estate.cabinetItems;
                n = ITEM_STORE_SLOTS;
            }
            else
            {
                n = 0;
            }
            break;
        }
        for (i = first; i < n; i++)
        {
            if (list[i].id != item)
            {
                continue;
            }
            if (count != 0)
            {
                switch (kind)
                {
                case EVENT_STORE_HELD:
                    if (where != 0)
                    {
                        game_state.player.stateFlags &= ~(PLAYER_FLAG_SHOWING_ITEM | PLAYER_FLAG_CARRY_ITEM);
                        player_reset_pose();
                        player_drop_held_item(&game_state.player);
                    }
                    else
                    {
                        picture_release_item(list[i].id);
                        stored_item_clear(&list[i]);
                    }
                    /* fallthrough */
                case EVENT_STORE_RUCKSACK:
                    picture_release_item(list[i].id);
                    stored_item_clear(&list[i]);
                    break;
                case EVENT_STORE_BOXES:
                case EVENT_STORE_FRIDGE:
                    stored_item_clear(&list[i]);
                    break;
                }
                count--;
            }
            found++;
        }
    }
    return found;
}

/* Clears the wedding-today flags, holds each rival wedding that is due, and on
 * spring day 7 resets a day counter for a married player. */
void rival_weddings_hold_due(void)
{
    s16 i;

    event_flag_set(EVENT_FLAG_KAREN_RICK_WEDDING_TODAY, 0);
    event_flag_set(EVENT_FLAG_ANN_CLIFF_WEDDING_TODAY, 0);
    event_flag_set(EVENT_FLAG_ELLI_DOCTOR_WEDDING_TODAY, 0);
    event_flag_set(EVENT_FLAG_MARY_GRAY_WEDDING_TODAY, 0);
    event_flag_set(EVENT_FLAG_POPURI_KAI_WEDDING_TODAY, 0);
    for (i = 0; i < RIVAL_COUPLE_COUNT; i++)
    {
        if (wedding_offer_state(i) == WEDDING_OFFER_YES)
        {
            wedding_hold(i);
        }
    }
    if (game_state.progress.clock.date.season == SEASON_SPRING && game_state.progress.clock.date.day == 7 && event_flag_get(EVENT_FLAG_SINGLE) == 0)
    {
        game_state.days.count0[40] = 0;
    }
}

/* Finds the next free date on the wedding grid and stores it in the result.
 * Mode 4 returns a fixed date in the next year. */
static void event_cmd_grid_result(s16 mode, FarmDate *date)
{
    s16 x;
    s16 y;
    s16 layer;

    layer = 0;
    x = game_state.progress.clock.date.season;
    y = game_state.progress.clock.date.day;
    if (mode == RIVAL_COUPLE_POPURI_KAI)
    {
        date->year = game_state.progress.clock.date.year + 1;
        date->season = 0;
        date->day = 29;
        return;
    }
    y += 6;
    do
    {
        y++;
        if (y >= 30)
        {
            y -= 30;
            x++;
            if (x >= 4)
            {
                x -= 4;
                layer++;
            }
        }
        wedding_date_allowed(mode, x, y);
    } while (clock_find_date_entry(x, y) != 0);
    date->year = layer + game_state.progress.clock.date.year;
    date->season = x;
    date->day = y;
}

/* Whether wedding `mode` (-1: the player's, else rivalWeddings[mode]) can be
 * offered today: 0 no, 1 yes, 2 yes with the second variant. The date must be
 * allowed, and the event flags must not have it done already. The couple then
 * decides between 1 and 2: no rival wedding while the bride's affection for the
 * player is over 59999, and 2 once the groom's friendship reaches 200. */
enum WeddingOfferState wedding_offer_state(s16 mode)
{
    s16 *today;
    FarmWedding *record;
    u16 done_flag;
    u16 open_flag;

    done_flag = 0;
    open_flag = 0;
    today = &game_state.progress.clock.date.year;
    if (wedding_date_allowed(mode, today[1], today[2]) == 0)
    {
        return WEDDING_OFFER_NO;
    }
    if (mode != WEDDING_SLOT_PLAYER)
    {
        record = &game_state.world.rivalWeddings[mode];
    }
    else
    {
        record = &game_state.player.wedding;
    }
    if (record->groom == -1)
    {
        return WEDDING_OFFER_NO;
    }
    switch (mode)
    {
    case WEDDING_SLOT_PLAYER:
        if (event_flag_get(EVENT_FLAG_SINGLE) == 0)
        {
            return WEDDING_OFFER_NO;
        }
        /* fallthrough */
    case RIVAL_COUPLE_POPURI_KAI:
        if (event_flag_get(EVENT_FLAG_122) != 0)
        {
            return WEDDING_OFFER_NO;
        }
        if (event_flag_get(EVENT_FLAG_POPURI_KAI_WEDDING_TODAY) == 1)
        {
            return WEDDING_OFFER_YES;
        }
        if (event_cmd_mode_check(0) == 1)
        {
            return WEDDING_OFFER_NO;
        }
        break;
    default:
        switch (mode)
        {
        case RIVAL_COUPLE_KAREN_RICK:
            done_flag = EVENT_FLAG_KAREN_RICK_MARRIED;
            open_flag = EVENT_FLAG_KAREN_RICK_WEDDING_TODAY;
            break;
        case RIVAL_COUPLE_ANN_CLIFF:
            done_flag = EVENT_FLAG_ANN_CLIFF_MARRIED;
            open_flag = EVENT_FLAG_ANN_CLIFF_WEDDING_TODAY;
            break;
        case RIVAL_COUPLE_ELLI_DOCTOR:
            done_flag = EVENT_FLAG_ELLI_DOCTOR_MARRIED;
            open_flag = EVENT_FLAG_ELLI_DOCTOR_WEDDING_TODAY;
            break;
        case RIVAL_COUPLE_MARY_GRAY:
            done_flag = EVENT_FLAG_MARY_GRAY_MARRIED;
            open_flag = EVENT_FLAG_MARY_GRAY_WEDDING_TODAY;
            break;
        }
        if (event_flag_get(open_flag) == 1)
        {
            return WEDDING_OFFER_YES;
        }
        if (event_flag_get(done_flag) == 1)
        {
            return WEDDING_OFFER_NO;
        }
        break;
    }
    if (mode != RIVAL_COUPLE_POPURI_KAI)
    {
        if (record->date.year > today[0])
        {
            return WEDDING_OFFER_NO;
        }
        if (record->date.year == today[0] && record->date.season > today[1])
        {
            return WEDDING_OFFER_NO;
        }
        if (record->date.season == today[1] && record->date.day > today[2])
        {
            return WEDDING_OFFER_NO;
        }
    }
    else
    {
        if (record->date.year > today[0])
        {
            return WEDDING_OFFER_NO;
        }
        if (record->date.season != today[1])
        {
            return WEDDING_OFFER_NO;
        }
        if (record->date.day != today[2])
        {
            return WEDDING_OFFER_NO;
        }
    }
    if (mode == WEDDING_SLOT_PLAYER)
    {
        return WEDDING_OFFER_YES;
    }
    if (mode != RIVAL_COUPLE_POPURI_KAI)
    {
        if (game_state.villagers[record->bride].affection > 59999 && game_state.player.wedding.bride != -1)
        {
            return WEDDING_OFFER_NO;
        }
        if (game_state.villagers[record->groom].friendship >= 200)
        {
            return WEDDING_OFFER_YES_VARIANT;
        }
        if (mode != RIVAL_COUPLE_KAREN_RICK)
        {
            return WEDDING_OFFER_YES;
        }
        if (game_state.player.wedding.bride != CHARACTER_POPURI)
        {
            return WEDDING_OFFER_YES;
        }
        if (event_flag_get(EVENT_FLAG_SINGLE) == 0)
        {
            return WEDDING_OFFER_YES_VARIANT;
        }
        return WEDDING_OFFER_YES;
    }
    if (event_flag_get(EVENT_FLAG_C9) == 1)
    {
        if (game_state.villagers[record->bride].affection > 59999 && game_state.player.wedding.bride != -1)
        {
            return WEDDING_OFFER_NO;
        }
        return WEDDING_OFFER_YES;
    }
    if (event_flag_get(EVENT_FLAG_11E) == 1)
    {
        return WEDDING_OFFER_YES_VARIANT;
    }
    return WEDDING_OFFER_YES;
}

/* Records that wedding `slot` (-1: the player's, to player.wedding.bride; else
 * rivalWeddings[slot]) took place today: stamps its record with the date, raises
 * its completion flags and, for a rival couple, gives both villagers the wedding role. */
void wedding_hold(s16 slot)
{
    s16 *date;
    FarmWedding *record;
    s32 flag;
    s32 flag2;

    flag = 0;
    flag2 = 0;
    date = &game_state.progress.clock.date.year;
    switch (slot)
    {
    case WEDDING_SLOT_PLAYER:
        family_set_bride(game_state.player.wedding.bride);
        switch (game_state.player.wedding.bride)
        {
        case CHARACTER_KAREN:
            flag = EVENT_FLAG_MARRIED_KAREN;
            break;
        case CHARACTER_ANN:
            flag = EVENT_FLAG_MARRIED_ANN;
            break;
        case CHARACTER_ELLI:
            flag = EVENT_FLAG_MARRIED_ELLI;
            break;
        case CHARACTER_MARY:
            flag = EVENT_FLAG_MARRIED_MARY;
            break;
        case CHARACTER_POPURI:
        default:
            flag = EVENT_FLAG_MARRIED_POPURI;
            break;
        }
        break;
    case RIVAL_COUPLE_KAREN_RICK:
        flag = EVENT_FLAG_KAREN_RICK_MARRIED;
        flag2 = EVENT_FLAG_KAREN_RICK_WEDDING_TODAY;
        break;
    case RIVAL_COUPLE_ANN_CLIFF:
        flag = EVENT_FLAG_ANN_CLIFF_MARRIED;
        flag2 = EVENT_FLAG_ANN_CLIFF_WEDDING_TODAY;
        break;
    case RIVAL_COUPLE_ELLI_DOCTOR:
        flag = EVENT_FLAG_ELLI_DOCTOR_MARRIED;
        flag2 = EVENT_FLAG_ELLI_DOCTOR_WEDDING_TODAY;
        break;
    case RIVAL_COUPLE_MARY_GRAY:
        flag = EVENT_FLAG_MARY_GRAY_MARRIED;
        flag2 = EVENT_FLAG_MARY_GRAY_WEDDING_TODAY;
        break;
    case RIVAL_COUPLE_POPURI_KAI:
        flag = EVENT_FLAG_POPURI_KAI_MARRIED;
        flag2 = EVENT_FLAG_POPURI_KAI_WEDDING_TODAY;
        break;
    }
    if (slot == WEDDING_SLOT_PLAYER)
    {
        record = &game_state.player.wedding;
    }
    else
    {
        record = &game_state.world.rivalWeddings[slot];
    }
    record->date.year = date[0];
    record->date.season = date[1];
    record->date.day = date[2];
    record->date.weekday = date[3];
    event_flag_set(flag, 1);
    if (slot == WEDDING_SLOT_PLAYER)
    {
        event_flag_set(EVENT_FLAG_SINGLE, 0);
        event_flag_set(EVENT_FLAG_PLAYER_MARRIED, 1);
    }
    else
    {
        event_flag_set(flag2, 1);
        game_state.villagers[game_state.world.rivalWeddings[slot].groom].flags |= VILLAGER_FLAG_WEDDING_ROLE;
        game_state.villagers[game_state.world.rivalWeddings[slot].bride].flags |= VILLAGER_FLAG_WEDDING_ROLE;
    }
}

/* Plans a wedding. `mode` WEDDING_PLAN_PLAYER_BRIDE makes the player's wedding to bride `kind`;
 * otherwise rival couple `kind` is planned (RIVAL_COUPLE_COUNT means Popuri and Kai, and also
 * sets their married flag and wedding roles). Each gets its date from event_cmd_grid_result. */
void event_cmd_init_mode(s16 mode, s16 kind)
{
    FarmWedding *record;
    s32 bride;
    s32 groom;
    if (kind == RIVAL_COUPLE_COUNT)
    {
        kind = RIVAL_COUPLE_POPURI_KAI;
        mode = WEDDING_PLAN_RIVAL;
        event_flag_set(EVENT_FLAG_POPURI_KAI_MARRIED, 1);
        game_state.villagers[CHARACTER_KAI].flags |= VILLAGER_FLAG_WEDDING_ROLE;
        game_state.villagers[CHARACTER_POPURI].flags |= VILLAGER_FLAG_WEDDING_ROLE;
    }
    if (mode == WEDDING_PLAN_PLAYER_BRIDE)
    {
        groom = CHARACTER_PLAYER;
        bride = kind;
    }
    else
    {
        switch (kind)
        {
        case RIVAL_COUPLE_KAREN_RICK:
            bride = CHARACTER_KAREN;
            groom = CHARACTER_RICK;
            break;
        case RIVAL_COUPLE_ANN_CLIFF:
            bride = CHARACTER_ANN;
            groom = CHARACTER_CLIFF;
            break;
        case RIVAL_COUPLE_ELLI_DOCTOR:
            bride = CHARACTER_ELLI;
            groom = CHARACTER_DOCTOR;
            break;
        case RIVAL_COUPLE_MARY_GRAY:
            bride = CHARACTER_MARY;
            groom = CHARACTER_GRAY;
            break;
        case RIVAL_COUPLE_POPURI_KAI:
        default:
            bride = CHARACTER_POPURI;
            groom = CHARACTER_KAI;
            break;
        }
    }
    if (mode != WEDDING_PLAN_RIVAL)
    {
        record = &game_state.player.wedding;
        record->groom = CHARACTER_PLAYER;
        record->bride = bride;
        event_cmd_grid_result(WEDDING_SLOT_PLAYER, &record->date);
        rival_wedding_clear(kind);
        game_state.villagers[bride].flags |= VILLAGER_FLAG_WEDDING_ROLE;
    }
    else
    {
        record = &game_state.world.rivalWeddings[kind];
        record->groom = groom;
        record->bride = bride;
        event_cmd_grid_result(kind, &record->date);
    }
}

/* Resets one rival wedding to its empty state. */
static void rival_wedding_clear(s16 slot)
{
    game_state.world.rivalWeddings[slot].date.year = -1;
    game_state.world.rivalWeddings[slot].date.season = SEASON_SPRING;
    game_state.world.rivalWeddings[slot].date.day = 0;
    game_state.world.rivalWeddings[slot].groom = -1;
    game_state.world.rivalWeddings[slot].bride = -1;
}

/* Whether a wedding may be held on (season, day) for `kind`: -1 the player's,
 * 0 and 1 the rival couples. Winter day 13 is never allowed. The player's
 * wedding also avoids spring day 29 and the birthdays of the player and the
 * bride. Karen and Rick need day counter 14 clear; Ann and Cliff avoid Ann's
 * birthday season and fall day 4. */
static s32 wedding_date_allowed(s16 kind, s16 season, s16 day)
{
    s16 slot;

    slot = game_state.player.wedding.bride;
    if (season == SEASON_WINTER && day == 13)
    {
        return 0;
    }
    switch (kind)
    {
    case WEDDING_SLOT_PLAYER:
        if (season == SEASON_SPRING && day == 29)
        {
            return 0;
        }
        if (game_state.player.birthday.season == season && game_state.player.birthday.day == day)
        {
            return 0;
        }
        if (game_state.villagers[slot].birthday.season == season &&
            game_state.villagers[slot].birthday.day == day)
        {
            return 0;
        }
        break;
    case RIVAL_COUPLE_KAREN_RICK:
        if (game_state.days.count0[14] != 0xFF)
        {
            return 0;
        }
        break;
    case RIVAL_COUPLE_ANN_CLIFF:
        if (season == game_state.villagers[CHARACTER_ANN].birthday.season &&
            game_state.villagers[CHARACTER_ANN].birthday.day != 0)
        {
            return 0;
        }
        if (season == SEASON_FALL && day == 4)
        {
            return 0;
        }
        break;
    }
    return 1;
}

/* Check the farm's progression flags and the selected mode. */
s32 event_cmd_mode_check(s16 mode)
{
    s32 kai_married;
    s32 other_flag;

    kai_married = event_flag_get(EVENT_FLAG_POPURI_KAI_MARRIED);
    if (kai_married == 1)
    {
        other_flag = event_flag_get(EVENT_FLAG_C9);
        if (other_flag == kai_married)
        {
            return 1;
        }
        if (game_state.days.count0[2] >= 90)
        {
            return 1;
        }
    }
    if (mode != 0)
    {
        other_flag = event_flag_get(EVENT_FLAG_MARRIED_POPURI);
        if (other_flag == 1)
        {
            return 1;
        }
    }
    return 0;
}

static FarmVariable farm_variables[48] = {
    [EVENT_VAR_GOLD] = {&game_state.player.gold, FARM_VAR_S32, 0, 0, FARM_MONEY_MAX, 0},
    [EVENT_VAR_STAMINA] = {&game_state.player.stamina, FARM_VAR_S16, 0, 0, 255, 0},
    [EVENT_VAR_MAX_STAMINA] = {&game_state.player.maxStamina, FARM_VAR_S16, 0, 0, 255, 0},
    [EVENT_VAR_BIRTH_SEASON] = {&game_state.player.birthday.season, FARM_VAR_S16, 0, 0, 3, 0},
    [EVENT_VAR_BIRTH_DAY] = {&game_state.player.birthday.day, FARM_VAR_S16, 0, 0, 29, 0},
    [EVENT_VAR_LUMBER] = {&game_state.progress.estate.lumber, FARM_VAR_U16, 0, 0, 999, 0},
    [EVENT_VAR_FODDER] = {&game_state.progress.estate.fodder, FARM_VAR_U16, 0, 0, 999, 0},
    [EVENT_VAR_POND_FISH] = {&game_state.progress.estate.pondFish, FARM_VAR_U16, 0, 0, 999, 0},
    [EVENT_VAR_FISH_FOOD] = {&game_state.progress.estate.fishFood, FARM_VAR_U16, 0, 0, 999, 0},
    [EVENT_VAR_CHICKEN_FEED] = {&game_state.progress.estate.chickenFeed, FARM_VAR_U16, 0, 0, 999, 0},
    [EVENT_VAR_HOUR] = {&game_state.progress.clock.time.hm.hour, FARM_VAR_S16, 0, 0, 23, 0},
    [EVENT_VAR_WEEKDAY] = {&game_state.progress.clock.date.weekday, FARM_VAR_S16, 0, 0, 6, 0},
    [EVENT_VAR_YEAR] = {&game_state.progress.clock.date.year, FARM_VAR_S16, 0, 0, 99, 0},
    [EVENT_VAR_SEASON] = {&game_state.progress.clock.date.season, FARM_VAR_S16, 0, 0, 3, 0},
    [EVENT_VAR_DAY] = {&game_state.progress.clock.date.day, FARM_VAR_S16, 0, 0, 29, 0},
    [EVENT_VAR_PLAYER_LOCATION] = {&game_state.player.place.location, FARM_VAR_S16, 0, 0, 1024, 0},
    [EVENT_VAR_WIFE_REQUESTS] = {&game_state.family.wifeRequests, FARM_VAR_S16, 0, 0, 90, 0},
    [EVENT_VAR_PREGNANCY_DAYS] = {&game_state.family.pregnancyDays, FARM_VAR_S16, 0, 0, 90, 0},
    [EVENT_VAR_CHILD_GROWTH_DAYS] = {&game_state.family.childGrowthDays, FARM_VAR_S16, 0, 0, 128, 0},
    [EVENT_VAR_WEATHER] = {&game_state.world.weather, FARM_VAR_S16, 0, 0, 8, 0},
    [EVENT_VAR_WEDDING_BRIDE] = {&game_state.player.wedding.bride, FARM_VAR_S16, 0, 0, 50, 0},
    [EVENT_VAR_PLAYER_X] = {&game_state.player.position.vx, FARM_VAR_S32, 0, 0, 0, 0},
    [EVENT_VAR_PLAYER_Y] = {&game_state.player.position.vy, FARM_VAR_S32, 0, 0, 0, 0},
    [EVENT_VAR_PLAYER_Z] = {&game_state.player.position.vz, FARM_VAR_S32, 0, 0, 0, 0},
    [EVENT_VAR_SHIPPED_VALUE] = {&game_state.progress.estate.shipment.shippedValue, FARM_VAR_S32, 0, 0, FARM_MONEY_MAX, 0},
    [EVENT_VAR_PLAYER_FACING] = {&game_state.player.facing38, FARM_VAR_U16, 0, 0, 0, 0},
    [EVENT_VAR_MINUTE] = {&game_state.progress.clock.time.hm.minute, FARM_VAR_U16, 0, 0, 59, 0},
    [EVENT_VAR_VILLAGER_FACING] = {&game_state.villagers[0].facing, FARM_VAR_U16, sizeof(FarmVillager), 0, 3, 0},
    [EVENT_VAR_VILLAGER_ACTION] = {&game_state.villagers[0].action, FARM_VAR_U16, sizeof(FarmVillager), 0, 99, 0},
    [EVENT_VAR_VILLAGER_POSITION] = {&game_state.villagers[0].position, FARM_VAR_RECORD, sizeof(FarmVillager), 0, 0, 0},
    [EVENT_VAR_VILLAGER_SPEED] = {&game_state.villagers[0].speed, FARM_VAR_RECORD, sizeof(FarmVillager), 0, 0, 0},
    [EVENT_VAR_VILLAGER_AFFECTION] = {&game_state.villagers[0].affection, FARM_VAR_U16, sizeof(FarmVillager), 0, 65535, 0},
    [EVENT_VAR_VILLAGER_FRIENDSHIP] = {&game_state.villagers[0].friendship, FARM_VAR_S16, sizeof(FarmVillager), 0, FRIENDSHIP_MAX, 0},
    [EVENT_VAR_VILLAGER_SPRITE80] = {&game_state.villagers[0].sprite80, FARM_VAR_RECORD, sizeof(FarmVillager), 0, 0, 0},
    [EVENT_VAR_VILLAGER_EVENT_POSITION] = {&game_state.villagers[0].ePosition, FARM_VAR_RECORD, sizeof(FarmVillager), 0, 0, 0},
    [EVENT_VAR_VILLAGER_EVENT_SPEED] = {&game_state.villagers[0].eSpeed, FARM_VAR_RECORD, sizeof(FarmVillager), 0, 0, 0},
    [EVENT_VAR_VILLAGER_EVENT_SPRITE] = {&game_state.villagers[0].sprite, FARM_VAR_RECORD, sizeof(FarmVillager), 0, 0, 0},
    [EVENT_VAR_VILLAGER_FLAGS] = {&game_state.villagers[0].flags, FARM_VAR_U16, sizeof(FarmVillager), 0, 65535, 0},
    [EVENT_VAR_VILLAGER_DESTINATION] = {&game_state.villagers[0].destination, FARM_VAR_S16, sizeof(FarmVillager), 0, 65535, 0},
    [EVENT_VAR_VILLAGER_PREV_DESTINATION] = {&game_state.villagers[0].prevDestination, FARM_VAR_S16, sizeof(FarmVillager), 0, 65535, 0},
    [EVENT_VAR_DAY_COUNT0] = {&game_state.days.count0[0], FARM_VAR_U8, 1, 0, 255, 0},
    [EVENT_VAR_DAY_COUNT1] = {&game_state.days.count1[0], FARM_VAR_U8, 1, 0, 255, 0},
    [EVENT_VAR_DAY_VALUE] = {&game_state.days.values[0], FARM_VAR_U8, 1, 0, 255, 0},
    [EVENT_VAR_HELPER_AFFECTION] = {&game_state.helpers[0].actor.affection, FARM_VAR_S16, sizeof(FarmHelper), 0, 255, 0},
    [EVENT_VAR_VILLAGER_FLAG_0400] = {&game_state.villagers[0].flags, FARM_VAR_U16, sizeof(FarmVillager), 0, 65535, VILLAGER_FLAG_BIT10},
    [EVENT_VAR_VILLAGER_FLAG_0200] = {&game_state.villagers[0].flags, FARM_VAR_U16, sizeof(FarmVillager), 0, 65535, VILLAGER_FLAG_TALKED_TODAY},
    [EVENT_VAR_VILLAGER_FLAG_0010] = {&game_state.villagers[0].flags, FARM_VAR_U16, sizeof(FarmVillager), 0, 65535, VILLAGER_FLAG_BIT4},
    [EVENT_VAR_HELPER_FLAG_0002] = {&game_state.helpers[0].actor.flags, FARM_VAR_U32, sizeof(FarmHelper), 0, 65535, ACTOR_FLAG_TALKED_TODAY},
};

/* Resolve a farm entry, apply its operation, and store at its selected width.
 * Flag variables set the event flag instead; masked entries set or clear their bit. */
static void event_var_update(s32 index, s32 element, s32 operand, s32 operation)
{
    s32 *cell;
    s32 kind;
    s32 result;
    if ((u32)(index - EVENT_VAR_FLAGS_FIRST) < EVENT_FLAG_VAR_COUNT)
    {
        event_flag_set((index - EVENT_VAR_FLAG_BIAS) & 0xFFFF, operand & 0xFFFF);
        return;
    }
    cell = event_var_address(index, element, &kind);
    switch (kind)
    {
    case FARM_VAR_U8:
        result = *(u8 *)cell;
        break;
    case FARM_VAR_U8_ALT:
        result = *(u8 *)cell;
        break;
    case FARM_VAR_U16:
        result = *(u16 *)cell;
        break;
    case FARM_VAR_S16:
        result = *(s16 *)cell;
        break;
    default:
    case FARM_VAR_U32:
        result = *cell;
        break;
    case FARM_VAR_S32:
        result = *cell;
        break;
    }
    if (farm_variables[index].mask != 0)
    {
        if (operand != 0)
            result |= farm_variables[index].mask;
        else
            result &= ~farm_variables[index].mask;
    }
    else
    {
        event_var_arith(&result, operand, operation);
        result = event_var_clamp(index, element, result);
    }
    switch (kind)
    {
    case FARM_VAR_U8:
    case FARM_VAR_U8_ALT:
        *(u8 *)cell = result;
        break;
    case FARM_VAR_U16:
    case FARM_VAR_S16:
        *(u16 *)cell = result;
        break;
    default:
    case FARM_VAR_U32:
    case FARM_VAR_S32:
        *cell = result;
        break;
    }
}

/* Reads farm variable `index`. The flag variables are the progression flags; the
 * rest are values stored at the width their table entry gives, optionally reduced
 * to a masked bit. The year and the day are reported one higher than stored. */
static s32 event_var_get(s32 index, s32 value)
{
    s32 kind;
    void *cell;
    s32 result;
    s32 mask;

    if ((u32)(index - EVENT_VAR_FLAGS_FIRST) < EVENT_FLAG_VAR_COUNT)
    {
        result = event_flag_get(index - EVENT_VAR_FLAG_BIAS);
    }
    else
    {
        cell = event_var_address(index, value, &kind);
        switch (kind)
        {
        case FARM_VAR_U8:
        case FARM_VAR_U8_ALT:
            result = *(u8 *)cell;
            break;
        case FARM_VAR_U16:
            result = *(u16 *)cell;
            break;
        case FARM_VAR_S16:
            result = *(s16 *)cell;
            break;
        case FARM_VAR_U32:
        case FARM_VAR_S32:
        default:
            result = *(s32 *)cell;
            break;
        }
        mask = farm_variables[index].mask;
        if (mask != 0)
        {
            if (result & mask)
            {
                result = 1;
            }
            else
            {
                result = 0;
            }
        }
    }
    if (index == EVENT_VAR_YEAR || index == EVENT_VAR_DAY)
    {
        result++;
    }
    return result;
}

static s32 event_var_clamp(s32 index, s32 unused, s32 value)
{
    s16 low = farm_variables[index].low;
    s32 high = farm_variables[index].high;
    if (value < low)
        value = low;
    else if (high < value)
        value = high;
    return value;
}

/* The rules event_var_compare tests a result against. It returns 1 when the result
 * fails the rule. */
enum EventComparison
{
    EVENT_CMP_LESS = 0,
    EVENT_CMP_LESS_EQUAL = 1,
    EVENT_CMP_GREATER = 2,
    EVENT_CMP_GREATER_EQUAL = 3,
    EVENT_CMP_NOT_EQUAL = 4,
};

/* Compares a result against a rule and returns 1 when the test fails. Rule 4
 * fails when the values are equal. */
static s32 event_var_compare(s32 result, s32 value, s32 context)
{
    s32 failed;

    failed = 0;
    switch (context)
    {
    case EVENT_CMP_LESS:
        if (result < value)
            break;
        failed = 1;
        break;
    case EVENT_CMP_LESS_EQUAL:
        if (value < result)
        {
            failed = 1;
        }
        break;
    case EVENT_CMP_GREATER:
        if (value < result)
            break;
        failed = 1;
        break;
    case EVENT_CMP_GREATER_EQUAL:
        if (result < value)
        {
            failed = 1;
        }
        break;
    case EVENT_CMP_NOT_EQUAL:
        if (result != value)
            break;
        /* fallthrough */
    default:
        failed = 1;
        break;
    }
    return failed;
}

/* The operations event_var_arith applies. */
enum EventArith
{
    EVENT_ARITH_SET = 0,
    EVENT_ARITH_ADD = 1,
    EVENT_ARITH_SUB = 2,
    EVENT_ARITH_MUL = 3,
    EVENT_ARITH_DIV = 4,
    EVENT_ARITH_MOD = 5,
};

/* Apply the selected arithmetic update to the result slot. */
static s32 event_var_arith(s32 *result, s32 value, u32 operation)
{
    switch (operation)
    {
    case EVENT_ARITH_SET:
        *result = value;
        break;
    case EVENT_ARITH_ADD:
        *result += value;
        break;
    case EVENT_ARITH_SUB:
        *result -= value;
        break;
    case EVENT_ARITH_MUL:
        *result *= value;
        break;
    case EVENT_ARITH_DIV:
        if (value != 0)
            *result /= value;
        break;
    case EVENT_ARITH_MOD:
        if (value != 0)
            *result %= value;
        break;
    }
    return 1;
}

/* The address of farm variable `index` for element `value` (its table stride times value). Its
 * width kind is written to `output` when given. */
void *event_var_address(s32 index, s32 value, s32 *output)
{
    s32 kind;
    void *result;

    if (output == 0)
        output = &kind;
    if (farm_variables[index].stride != 0)
        result = farm_variables[index].address + farm_variables[index].stride * value;
    else
        result = farm_variables[index].address;
    *output = farm_variables[index].kind;
    if (result == 0)
        while (host_poll() != 0)
        {
        }
    return result;
}

/* The highest friendship among the villagers that villager_is_bachelorette does
 * not pick (0 if none is higher). */
static s32 ending_best_friendship(void)
{
    s32 i;
    s32 best;

    best = 0;
    for (i = 1; i < CHARACTER_COUNT; i++)
    {
        if (villager_is_bachelorette(i) != 1)
        {
            if (best < game_state.villagers[i].friendship)
            {
                best = game_state.villagers[i].friendship;
            }
        }
    }
    return best;
}

/* The slot (0..4) of the first bride the player married, in wedding order. -1 when single, or
 * when none of them is married. */
static s32 ending_married_bride(void)
{
    s32 slot;
    s32 one;
    u32 status;

    if (event_flag_get(EVENT_FLAG_SINGLE) == 1)
    {
        return -1;
    }
    slot = 0;
    one = 1;
    /* The five marriage flags, EVENT_FLAG_MARRIED_KAREN (0x1C) to _POPURI (0x20), in
     * the order of the slots. */
    status = 0x1C0000;
    while (slot < 5)
    {
        if (event_flag_get(status >> 16) == one)
        {
            return slot;
        }
        slot++;
        status += 0x10000;
    }
    return -1;
}

/* The total of the shipment counts by kind, plus the shipment total. */
static s32 ending_shipment_total(void)
{
    s32 sum = 0;
    s32 i;
    for (i = 0; i < SHIPPING_KINDS; i++)
        sum += game_state.progress.estate.shipment.counts[i];
    return sum + game_state.progress.estate.shipmentTotal;
}

/* Percentage of the field's tiles that are planted, plus the 7 x 7 greenhouse
 * slots once the greenhouse is built. */
static s32 ending_planted_field_percent(void)
{
    s32 clear;
    s32 total;
    s32 column;
    s32 row;

    clear = 0;
    total = 25 * 45;
    if (event_flag_get(EVENT_FLAG_GREENHOUSE_LEVEL_1) == 1)
    {
        total = 25 * 45 + 7 * 7;
        for (column = 0; column < 7; column++)
        {
            for (row = 0; row < 7; row++)
            {
                if (field_tile_is_planted(game_state.slots7x7[row].rows[column].ground) == 1)
                {
                    clear++;
                }
            }
        }
    }
    for (column = 0; column < FIELD_COLS; column++)
    {
        for (row = 0; row < FIELD_ROWS; row++)
        {
            if (field_tile_is_planted(game_state.fieldTiles[row].rows[column].ground) == 1)
            {
                clear++;
            }
        }
    }
    return clear * 100 / total;
}

/* Whether a field tile's ground is planted: 0 for empty ground (stones, weeds, branches, lumber,
 * stumps, soil), 1 otherwise. */
static s32 field_tile_is_planted(s16 result)
{
    if (result == ITEM_STONE || result == ITEM_WEEDS || result == ITEM_BRANCH || result == ITEM_LUMBER || result == FIELD_TILE_LUMBER ||
        result == ITEM_ROCK || result == ITEM_BIG_STONE || result == FIELD_TILE_ROCK_1 || result == FIELD_TILE_BIG_STONE_1 || result == FIELD_TILE_ROCK_2 ||
        result == FIELD_TILE_BIG_STONE_2 || result == FIELD_TILE_ROCK_3 || result == FIELD_TILE_BIG_STONE_3 || result == ITEM_STUMP ||
        result == FIELD_TILE_STUMP_1 || result == FIELD_TILE_STUMP_2 || result == FIELD_TILE_STUMP_3 || result == ITEM_SOIL)
    {
        return 0;
    }
    return 1;
}

static s16 affection_candidate_ids[6] = {
    3,
    7,
    9,
    14,
    20,
    31074,
};

/* The index (0..4) of the candidate with the highest affection, or -1 when that is under 500. */
static s32 ending_favourite_bride(void)
{
    s32 best_value;
    s32 best_slot;
    s32 slot;
    u16 value;

    best_value = 0;
    best_slot = 0;
    slot = 0;
    do
    {
        value = game_state.villagers[affection_candidate_ids[slot]].affection;

        if (best_value < value)
        {
            best_slot = slot;
            best_value = value;
        }
        slot += 1;
    } while (slot < 5);
    if (best_value >= 500)
    {
        return best_slot;
    }
    return -1;
}

static void animal_naming_begin(void)
{
    farm_mode_slot_respawn(1);
    farm_mode_slot_respawn(0);
}

/* Whether the animal in the slot for `mode` is ready to be named: a livestock
 * with a pending name (modes 0 and 1), or a chick (modes 2 and 3). Modes 1 and 3
 * also need their building. */
static s32 animal_naming_slot_ready(s16 mode)
{
    if ((mode == 1 && game_state.progress.estate.barnLevel == 0) ||
        (mode == 3 && game_state.progress.estate.coopLevel == 0))
    {
        return 0;
    }
    /* Modes 0 and 1 are livestock, 2 and 3 chickens. */
    if ((mode & 2) == 0)
    {
        if (game_state.animals.livestock[game_state.animals.slots[mode].animal].actor.flags & ACTOR_FLAG_NAMING_PENDING)
        {
            return 1;
        }
    }
    else
    {
        if (game_state.animals.chickens[game_state.animals.slots[mode].value].actor.kind == ITEM_CHICK)
        {
            return 1;
        }
    }
    return 0;
}

/* Picks the name slot for `mode` (other modes use the scratch buffer), then
 * polls the name entry: 0 restarts the load, 1 copies the name into the slot and
 * reports success. */
static s32 animal_naming_step(s16 mode)
{
    FarmPayload *slot;
    s32 loaded;
    s32 status;

    if (mode < 0)
    {
        slot = &name_entry_buffer[0];
    }
    else if (mode < 2)
    {
        slot = (FarmPayload *)game_state.animals.livestock[game_state.animals.slots[mode].value].actor.name;
    }
    else if (mode < 4)
    {
        slot = (FarmPayload *)game_state.animals.chickens[game_state.animals.slots[mode].value].actor.name;
    }
    else
    {
        slot = &name_entry_buffer[0];
    }

    loaded = 0;
    status = name_entry_step();
    if (status != 0)
    {
        if (status > 0)
        {
            if (status == 1)
            {
                *slot = name_entry_buffer[0];
                loaded = 1;
            }
        }
    }
    else
    {
        name_entry_start_no_cancel((u16 *)&name_entry_buffer[0]);
    }
    return loaded;
}

/* Finishes the naming of each ready animal slot: for slots 0 and 1 the livestock
 * is no longer pending a name and is unclaimed, then the slot is released. */
static void animal_naming_end(void)
{
    s32 i;

    for (i = 0; i < 4; i++)
    {
        if (animal_naming_slot_ready(i) == 1)
        {
            if ((u32)i < 2)
            {
                game_state.animals.livestock[game_state.animals.slots[i].animal].actor.flags &= ~ACTOR_FLAG_NAMING_PENDING;
                game_state.animals.livestock[game_state.animals.slots[i].value].actor.flags &= ~ACTOR_FLAG_CLAIMED;
            }
            animal_slot_clear(&game_state.animals.slots[i].value);
        }
    }
    farm_items_apply_kind(1);
    farm_items_apply_kind(0);
}

/* The facing that points from `b` toward `a`, along the larger axis of their difference. */
static u16 farm_vec_heading(VECTOR *a, VECTOR *b)
{
    s32 ax = a->vx;
    s32 bx = b->vx;
    s32 az = a->vz;
    s32 bz = b->vz;
    s32 dx = ax - bx;
    s32 dz = az - bz;
    s32 adx = dx < 0 ? -dx : dx;
    s32 adz = dz < 0 ? -dz : dz;
    if (adz < adx)
        return dx > 0 ? FACING_RIGHT : FACING_LEFT;
    return (dz > 0) ? FACING_UP : FACING_DOWN;
}

/* A villager's affection delta for a gift: from the reaction table, more when the held item has
 * contents, and five times as much on the villager's birthday. */
static s16 gift_reaction_delta(s16 villager, s32 kind, s16 reaction)
{
    s16 result;
    s16 adjusted_result;

    result = villager_reaction_table[kind][reaction];
    if (stored_item_contents(&game_state.player.held[1]) != 0)
    {
        if (kind & 1)
            adjusted_result = result + 20;
        else
            adjusted_result = result + 2;
        result = adjusted_result;
    }
    if (event_cmd_is_birthday(villager, 0, -1, -1) != 0)
        result = result * 5;
    return result;
}

/* The stages of farm_walk_route_step, held in *step: the walk out, the side trip and
 * the walk home. */
enum WalkRouteStep
{
    WALK_ROUTE_START = 0,
    WALK_ROUTE_WAIT_1 = 1,
    WALK_ROUTE_WAIT_2 = 2,
    WALK_ROUTE_WAIT_3 = 3,
    WALK_ROUTE_OUT_Z = 4,
    WALK_ROUTE_BACK_Z = 5,
    WALK_ROUTE_SIDE_OUT = 6,
    WALK_ROUTE_SIDE_TURN = 7,
    WALK_ROUTE_SIDE_BACK = 8,
    WALK_ROUTE_HOME_TURN = 9,
};

/* One step of a villager's walk out from (x, z) and back: along z to z + 600,
 * sideways 500 on the side villager_is_bachelorette picks, back, and down to z
 * again, with the matching animation. `*step` (0..9) advances when a leg is done
 * and wraps to 0 after the last; returns 1 then. */
static s32 farm_walk_route_step(CharacterId villager, s32 x, s32 z, s32 *step)
{
    VECTOR *actor;
    s32 side;
    s32 done;
    s32 limit;

    actor = &game_state.villagers[villager].ePosition;
    side = villager_is_bachelorette(villager);
    done = 0;
    switch (*step)
    {
    case WALK_ROUTE_START:
    case WALK_ROUTE_WAIT_1:
    case WALK_ROUTE_WAIT_2:
    case WALK_ROUTE_WAIT_3:
        done = event_cmd_set_sprite(villager, 5, 0, 0);
        break;
    case WALK_ROUTE_OUT_Z:
        event_cmd_set_sprite(villager, 6, 0, 0);
        actor->vz += 10;
        if (z + 600 < actor->vz)
        {
            actor->vz = z + 600;
            done = 1;
        }
        break;
    case WALK_ROUTE_BACK_Z:
        event_cmd_set_sprite(villager, 6, 0, 0);
        actor->vz -= 10;
        if (actor->vz < z)
        {
            actor->vz = z;
            done = 1;
        }
        break;
    case WALK_ROUTE_SIDE_OUT:
        if (side == 0)
        {
            event_cmd_set_sprite(villager, 4, 0, 0);
            limit = x + 500;
            actor->vx += 10;
            if (limit < actor->vx)
            {
                actor->vx = limit;
                done = 1;
            }
        }
        else
        {
            event_cmd_set_sprite(villager, 2, 0, 0);
            limit = x - 500;
            actor->vx -= 10;
            if (actor->vx < limit)
            {
                actor->vx = limit;
                done = 1;
            }
        }
        break;
    case WALK_ROUTE_SIDE_TURN:
        done = event_cmd_set_sprite(villager, 8, 0, 0);
        break;
    case WALK_ROUTE_SIDE_BACK:
        if (side == 0)
        {
            event_cmd_set_sprite(villager, 2, 0, 0);
            actor->vx -= 10;
            if (actor->vx < x)
            {
                actor->vx = x;
                done = 1;
            }
        }
        else
        {
            event_cmd_set_sprite(villager, 4, 0, 0);
            actor->vx += 10;
            if (x < actor->vx)
            {
                actor->vx = x;
                done = 1;
            }
        }
        break;
    case WALK_ROUTE_HOME_TURN:
        done = event_cmd_set_sprite(villager, (side == 0) ? 8 : 7, 0, 0);
        break;
    }
    if (done == 1)
    {
        if (*step == WALK_ROUTE_HOME_TURN)
        {
            *step = WALK_ROUTE_START;
        }
        else
        {
            (*step)++;
            done = 0;
        }
    }
    return done;
}

/* Loads the cell image for `key` into the farm's image and moves its pixels and
 * palette to the cell area in VRAM. Prints "Error" and returns 0 when the key is
 * not in the table. */
static s32 farm_image_load_cell(s16 key)
{
    FarmState *state;
    FarmGridEntry *image;
    s32 row;
    u32 column;
    s16 *row0;
    s16 *row1;
    s16 left;
    RECT rect;

    row = -1;
    row0 = farm_area.mode.pictureIds[0];
    row1 = farm_area.mode.pictureIds[1];
    column = 0;
    while (column < 9)
    {
        if (*row0 == key)
        {
            row = 0;
            break;
        }
        if (*row1 == key)
        {
            row = 1;
            break;
        }
        column++;
        row0++;
        row1++;
    }
    if (row == -1)
    {
        /* the trailing "0" is never printed */
        FntPrint("Error\n\0"
                 "0");
        return 0;
    }

    state = &game_state;
    image = &state->image;
    state->image = farm_area.mode.pictureEntries[row][column];
    left = 832;
    if (image->clutX >= 1008)
    {
        left = 960;
    }
    rect.x = left + state->image.x;
    rect.y = image->y;
    rect.w = image->w / 4;
    rect.h = image->h;
    MoveImage2(&rect, 704, 448);
    state->image.x = 704;
    image->y = 448;
    rect.w = 16;
    rect.h = 1;
    rect.x = image->clutX;
    rect.y = image->clutY;
    MoveImage2(&rect, 704, 447);
    image->clutX = 704;
    image->clutY = 447;
    image->hidden = 0;
    return 1;
}

static FarmInitPair helper_init_pairs[7] = {
    {0x0002, 0x000D},
    {0x0003, 0x0015},
    {0x0001, 0x000F},
    {0x0002, 0x0009},
    {0x0000, 0x000E},
    {0x0000, 0x0003},
    {0x0000, 0x0019},
};

/* The five brides' birthdays: [0] at the start, [1] the birthday each moves to when
 * the player's birthday is on its [0] day. */
static FarmItemPlacement item_places[2][5] = {
    {{3, 2, 14}, {7, 1, 16}, {9, 0, 15}, {14, 3, 19}, {20, 1, 2}},
    {{3, 2, 22}, {7, 1, 21}, {9, 0, 19}, {14, 3, 24}, {20, 1, 9}},
};

/* Birthdays to move: {villager, season, day} rows, ended by a -1. */
static struct
{
    FarmItemPlacement rows[28];
    s16 end;
} birthday_moves = {
    {
        {1, 3, 28},
        {2, 0, 29},
        {4, 0, 10},
        {5, 3, 5},
        {6, 3, 10},
        {8, 2, 16},
        {10, 2, 19},
        {11, 1, 5},
        {12, 1, 10},
        {13, 2, 22},
        {15, 0, 16},
        {16, 3, 25},
        {17, 1, 24},
        {18, 0, 18},
        {19, 2, 26},
        {21, 3, 14},
        {22, 2, 10},
        {23, 3, 12},
        {24, 2, 1},
        {25, 1, 21},
        {26, 1, 28},
        {28, 1, 3},
        {29, 3, 1},
        {30, 2, 19},
        {31, 0, 1},
        {32, 0, 28},
        {34, 2, 4},
        {35, 3, 18},
    },
    -1,
};

/* Sets up the helpers and the villagers' birthdays. */
void farm_item_rows_init(void)
{
    FarmItemPlacement *placement;
    FarmVillager *villager;
    u32 i;
    u32 end;
    s32 row;

    for (i = 0; i < HELPER_COUNT; i++)
    {
        game_state.helpers[i].pairC4.kind = helper_init_pairs[i].kind;
        game_state.helpers[i].pairC4.count = helper_init_pairs[i].count - 1;
    }

    for (i = 0; i < 5; i++)
    {
        villager = &game_state.villagers[item_places[0][i].item];
        villager->birthday.season = item_places[0][i].x;
        villager->birthday.day = item_places[0][i].y;
    }

    for (i = 0; i < 5; i++)
    {
        placement = &item_places[1][i];
        villager = &game_state.villagers[placement->item];
        if (game_state.player.birthday.season == villager->birthday.season && game_state.player.birthday.day == villager->birthday.day)
        {
            villager->birthday.season = placement->x;
            villager->birthday.day = placement->y;
        }
    }

    end = -1;
    i = 0;
    do
    {
        placement = &birthday_moves.rows[i];
        row = placement->item;
        if (row == end)
        {
            break;
        }
        villager = &game_state.villagers[row];
        villager->birthday.season = placement->x;
        villager->birthday.day = placement->y;
        i++;
    } while (1);
}

/* Copy a table into FarmState and count its entries up to the 0xFFFF end
 * marker. */
void farm_events_load(FarmTable *source)
{
    FarmTable *table;
    s32 count;

    event_table_count = 0;
    if (source == 0)
    {
        event_table_count = 0;
    }
    else
    {
        table = (FarmTable *)game_state.work.events;
        memset(table, 0, sizeof(FarmTable));
        *table = *source;
        count = 0;
        if (table->entries[count].marker == 0xFFFF)
        {
            event_table_count = count;
        }
        else
        {
            goto next;
            do
            {
            next:
                count++;
            } while ((&table->entries[0] + count)->marker != 0xFFFF);
            event_table_count = count;
        }
    }
    event_trigger_pending = 0;
}

/* Fires the farm events for a pending trigger mode (unless the debug flag turns triggers off).
 * Always returns 1. */
s32 event_triggers_run(void)
{
    if (!(game_state.debugFlags & DEBUG_FLAG_NO_EVENT_TRIGGERS))
    {
        if (event_trigger_pending != 0)
        {
            event_triggers_fire(game_state.work.events, event_table_count);
        }
        event_trigger_pending = 0;
        return 1;
    }
    return 1;
}

/* Sets the pending event trigger mode to `arg`, if no request is busy (or the pending mode is 1).
 * At 6:00 with the clock queue busy it is cleared again, unless it is mode 8. */
void event_trigger_mode_set(s32 arg)
{
    if (event_trigger_pending == 1 || event_flag_get(EVENT_FLAG_REQUEST_BUSY) == 0)
    {
        event_trigger_pending = arg;
    }
    if (game_state.progress.clock.time.word == 6)
    {
        if (clock_queue_busy() != 0 && event_trigger_pending != EVENT_TRIGGER_MODE_8)
        {
            event_trigger_pending = 0;
        }
    }
}

/* Queues farm event `arg` (low 16 bits) with `value` through event_request_add_any. */
u32 event_request_force(u32 arg, s32 value)
{
    return event_request_add_any(arg & 0xFFFF, value);
}

static void event_request_add_masked(s32 event, s32 value)
{
    event_request_add(event & 0xFFFF, value);
}

/* Goes through `count` farm events and fires every one whose conditions all
 * hold. Events with trigger mode 2 are skipped while both flag records have bit
 * 0x1000 set, unless their id is 0xF0..0xF9. Always returns 1. */
static s32 event_triggers_fire(FarmEventEntry *event, s32 count)
{
    u16 window[6];
    s16 current;
    s32 i;

    for (i = 0; i < count; i++, event++)
    {
        game_state.currentEvent = event;
        if (event->triggerMode == EVENT_TRIGGER_MODE_2 && farm_area.mode.previous->flags & 0x1000 &&
            farm_area.mode.data->head.flags & 0x1000 &&
            (u32)((event->id & EVENT_TRIGGER_ID_MASK) - 240) >= 10)
        {
            continue;
        }
        if (event_trigger_mode_match(event->triggerMode, event->triggerValue) == 0)
            continue;
        event_window_unpack(event->window, window);
        current = game_state.progress.clock.date.year;
        if (current < window[0])
            continue;
        if (window[1] < current)
            continue;
        if (event_day_window_match(window[2], window[3], &game_state.progress.clock.date) == 0)
            continue;
        if (event_hour_window_match(window[4], window[5], &game_state.progress.clock.time.hm.hour) == 0)
            continue;
        if (event_location_match(event->id, event->triggerFlags.word) == 0)
            continue;
        if (event_var_in_range(event->varId, event->varElement, event->rangeLow, event->rangeHigh,
                               event->triggerFlags.word) == 0)
            continue;
        if (event_wedding_role_match(event->roleCondition) == 0)
            continue;
        if (event_bond_match(event->bondVillager, event->bondAmount, event->triggerFlags.word) == 0)
            continue;
        if (event_flags_match(event->needFlags, 1) == 0)
            continue;
        if (event_flags_match(event->needClear, 0) == 0)
            continue;
        if (event_prerequisites_match(event->prerequisites) == 0)
            continue;
        if (event_calendar_match(event->triggerFlags.word) == 0)
            continue;
        if (event_not_done(event->id) == 0)
            continue;
        if (event_availability_match(event->availability) == 0)
            continue;
        event_request_add(event->id & EVENT_TRIGGER_ID_MASK, event->triggerFlags.bytes[1] >> 7);
    }
    return 1;
}

/* Queues farm event `event` with `flags` in a free request slot (claimed as kind 0), tagged with
 * the current place. Returns 0 when queued. Returns 1 when refused: no slot, or, with the debug
 * flag set, an event under 300. */
static s32 event_request_add(s32 event, s32 flags)
{
    u16 v;
    FarmRequestSlot *p;
    v = event;
    if ((game_state.statusFlags & STATUS_FLAG_DEBUG_EVENTS_300_AND_UP) != 0 && v < 300U)
        return 1;
    p = event_request_claim(0);
    if (p != 0)
    {
        p->args[0] = v;
        p->args[2] = flags;
        p->args[1] = farm_area.mode.place.location;
        return 0;
    }
    return 1;
}

/* As event_request_add, but claims a request slot of any kind. */
static s32 event_request_add_any(s32 event, s32 flags)
{
    u16 val;
    FarmRequestSlot *p;
    val = event;
    if ((game_state.statusFlags & STATUS_FLAG_DEBUG_EVENTS_300_AND_UP) && (val < 300))
        return 1;
    p = event_request_claim_any(0);
    if (p)
    {
        p->args[0] = val;
        p->args[2] = flags;
        p->args[1] = farm_area.mode.place.location;
        return 0;
    }
    return 1;
}

/* Is there a slot of kind 0 whose first argument is `value`? */
static s32 farm_slot_has_kind0(s32 value)
{
    s32 found = 0;
    s32 i;

    for (i = 0; i < 5; i++)
    {
        if (game_state.requests.items[i].value == 0 && game_state.requests.items[i].args[0] == (u16)value)
        {
            found = 1;
            break;
        }
    }
    return found;
}

/* Whether the hour (value[0]) falls in the window of `count` hours from `start`, wrapping past 24.
 * A window of 0 hours matches only `start`, with value[1] 0. */
static s32 event_hour_window_match(s32 start_arg, s32 count_arg, s16 *value)
{
    s32 start;
    s32 count;
    s16 target;
    s32 found;
    s32 index;
    s32 current;

    start = start_arg & 0xFFFF;
    count = count_arg & 0xFFFF;
    if (start == EVENT_WINDOW_ANY || count == EVENT_WINDOW_ANY)
    {
        return 1;
    }
    target = value[0];
    if (count >= 25U)
    {
        count = 24;
    }
    found = 0;
    if (count != 0)
    {
        index = 0;
        while (1)
        {
            if (index >= count)
            {
                break;
            }
            current = start + index;
            if (current >= 24)
            {
                current -= 24;
            }
            index++;
            if (current == target)
            {
                found = 1;
                break;
            }
        }
    }
    else if (start == target && value[1] == 0)
    {
        found = 1;
    }
    return found;
}

/* Whether the day of the month falls in the window of days from `start`, `span` days after it. */
static s32 event_day_window_match(s32 start, s32 span, FarmDate *date)
{
    s32 first;
    u32 count;
    s32 i;
    s32 found;
    s32 slot;
    s16 target;

    first = start & 0xFFFF;
    count = span & 0xFFFF;
    if (first == EVENT_WINDOW_ANY || count == EVENT_WINDOW_ANY)
        return 1;
    target = date->day;
    found = 0;
    if (count >= 30)
        count = 29;
    i = 0;
    count++;
    while (i < (s32)count)
    {
        slot = first + i;
        if (slot >= 31)
            slot -= 29;
        if (slot != target)
        {
            i++;
            continue;
        }
        found = 1;
        break;
    }
    return found;
}

/* Whether the event's place matches. The wildcard place always passes. Otherwise
 * a negative `flags` wants a different place from the previous one, and a
 * positive one the same place. */
static s32 event_location_match(u16 id, s32 flags)
{
    u32 sector = id >> 10;
    s32 result;

    if (sector == PLACE_ANY)
    {
        result = 1;
    }
    else if (flags < 0)
    {
        result = sector != farm_area.mode.previous->place.location;
    }
    else
    {
        result = sector == farm_area.mode.previous->place.location;
    }
    return result;
}

/* Test whether the current farm value lies in the requested range. A lower or upper bound of
 * 0xFF leaves that end open. The result is inverted with EVENT_TRIGGER_INVERT_RANGE. */
static s32 event_var_in_range(s32 raw_variable, s32 raw_element, s32 raw_lower, s32 raw_upper, s32 flags)
{
    u32 first = raw_variable & 0xFF;
    u32 second = raw_element & 0xFF;
    u32 lower = raw_lower & 0xFFFF;
    u32 upper = raw_upper & 0xFFFF;
    u32 value;
    s32 result;

    if (first == 0xFF || second == 0xFF)
    {
        if (flags & EVENT_TRIGGER_INVERT_RANGE)
        {
            return 0;
        }
        return 1;
    }
    value = event_var_read(first, second) & 0xFF;
    result = 0;
    if (lower == 0xFF)
    {
        if (upper == lower || value <= upper)
        {
            result = 1;
        }
    }
    else if (upper == 0xFF)
    {
        if (lower <= value)
        {
            result = 1;
        }
    }
    else if ((u16)value >= lower && (u16)value <= upper)
    {
        result = 1;
    }
    if (flags & EVENT_TRIGGER_INVERT_RANGE)
    {
        return result ^ 1;
    }
    return result;
}

/* Tests a wedding-role condition byte. Its enable bit depends on whether the
 * player is single. The low bits pick a villager kind (the wildcard always
 * passes), and the role bits say whether that villager may have a wedding role
 * or must not. */
static s32 event_wedding_role_match(s32 condition)
{
    u8 flags = condition;
    s32 enabled;
    s32 kind;
    CharacterId villager;

    if (game_state.player.wedding.bride == -1)
    {
        enabled = flags & EVENT_ROLE_WHEN_SINGLE;
    }
    else
    {
        enabled = flags & EVENT_ROLE_WHEN_ENGAGED;
    }
    if (enabled == 0)
    {
        return 0;
    }
    kind = flags & EVENT_ROLE_KIND_MASK;
    if (kind == EVENT_ROLE_ANY_KIND)
    {
        return 1;
    }
    villager = bride_of_index(kind);
    if (game_state.villagers[villager].flags & VILLAGER_FLAG_WEDDING_ROLE)
    {
        if (!(flags & EVENT_ROLE_WITH_ROLE_OK))
        {
            return 0;
        }
    }
    else
    {
        if (!(flags & EVENT_ROLE_WITHOUT_ROLE_OK))
        {
            return 0;
        }
    }
    return 1;
}

/* Compare a requested amount against the selected farm slot's unsigned limit. */
static s32 event_bond_match(s32 villager_arg, s32 amount_arg, s32 flags)
{
    s32 slot;
    u32 amount;
    u16 limit;
    s32 result;

    slot = villager_arg & 0xFF;
    amount = amount_arg & 0xFFFF;
    if (slot == 0xFF)
    {
        return 1;
    }
    if (slot == EVENT_BOND_HORSE)
    {
        limit = game_state.animals.horse.actor.affection;
    }
    else if (slot == EVENT_BOND_DOG)
    {
        limit = game_state.animals.dog.actor.affection;
    }
    else
    {
        if (villager_is_bachelorette(slot) == 0)
        {
            limit = game_state.villagers[slot].friendship;
        }
        else
        {
            limit = game_state.villagers[slot].affection;
        }
    }
    result = 0;
    if (flags & EVENT_TRIGGER_BOND_BELOW)
    {
        result = amount >= limit;
    }
    else if (limit >= amount)
    {
        result = 1;
    }
    return result;
}

/* Check the three packed farm values against the active flag state: with `mode` 1 each
 * flag must be set, otherwise each must be clear. The no-id sentinel is skipped. */
static s32 event_flags_match(s32 value, s32 mode)
{
    u16 values[3];
    s32 result;
    s32 index;
    s32 sentinel;
    u16 *current;
    s32 one;
    u16 value_at_slot;

    event_flag_ids_unpack(value, values, mode);
    result = 1;
    index = 0;
    sentinel = EVENT_TRIGGER_NO_ID;
    current = values;
    one = 1;
    do
    {
        if (mode == one)
        {
            value_at_slot = *current;
            if (value_at_slot != sentinel)
            {
                if (event_flag_get(value_at_slot + EVENT_DONE_FLAG_BASE) != mode)
                {
                    result = 0;
                }
            }
        }
        else
        {
            value_at_slot = *current;
            if (value_at_slot != sentinel)
            {
                if (event_flag_get(value_at_slot + EVENT_DONE_FLAG_BASE) != 0)
                {
                    result = 0;
                }
            }
        }
        index++;
        if (result == 0)
        {
            break;
        }
        current++;
    } while (index < 3);
    return result;
}

/* The story flags a trigger needs: the low id's flag set, and the high id's flag clear. The
 * no-id sentinel skips either test. */
static s32 event_prerequisites_match(s32 arg)
{
    s32 lo;
    s32 hi;
    s32 one;
    s32 max;
    lo = arg & EVENT_TRIGGER_ID_MASK;
    one = 1;
    if (lo != EVENT_TRIGGER_NO_ID)
    {
        if (event_flag_get(lo + EVENT_STORY_FLAG_BASE) != one)
            return 0;
    }
    hi = (u32)arg >> 10;
    lo = hi & EVENT_TRIGGER_ID_MASK;
    max = EVENT_TRIGGER_NO_ID;
    if (lo != max)
    {
        if (event_flag_get(lo + EVENT_STORY_FLAG_BASE) != 0)
            return 0;
    }
    return 1;
}

static s32 event_talk_villager_is(s32 villager)
{
    return game_state.player.talkVillager == (villager & 0xFF);
}

/* Whether the event's trigger mode matches: mode ALWAYS always does. Otherwise the pending mode
 * must be the same, and the talk and gift modes also test the villager or the gift. */
static s32 event_trigger_mode_match(s32 mode, s32 value)
{
    s32 result;

    mode = mode & 0xFF;
    value = value & 0xFF;
    if (mode == EVENT_TRIGGER_MODE_ALWAYS)
        return 1;
    result = 0;
    if (mode == event_trigger_pending)
    {
        result = 1;
        switch (mode)
        {
        case EVENT_TRIGGER_MODE_TALK:
            if (event_talk_villager_is(value) == 0)
            {
                result = 0;
            }
            break;
        case EVENT_TRIGGER_MODE_GIFT:
            if (game_state.player.currentAction != ITEM_FISHING_MARKER)
            {
                result = event_cmd_check_gift(GIFT_CHECK_TAKE, value, -1, 0);
                game_state.player.currentAction = 0;
            }
            else
            {
                result = 0;
            }
            break;
        case EVENT_TRIGGER_MODE_2:
        case EVENT_TRIGGER_MODE_5:
        case EVENT_TRIGGER_MODE_6:
        case EVENT_TRIGGER_MODE_7:
        case EVENT_TRIGGER_MODE_8:
            break;
        }
    }
    return result;
}

static s32 event_availability_match(s32 condition)
{
    condition &= 0xFF;
    if (condition == 0xFF)
        return 1;
    return farm_event_available((enum EventCondition)condition);
}

/* Whether the event's done flag is clear, and no kind-0 request is already queued for it. */
static s32 event_not_done(s32 id)
{
    s32 event = id & EVENT_TRIGGER_ID_MASK;
    if (event_flag_get(event + EVENT_DONE_FLAG_BASE) == 1)
    {
        return 0;
    }
    return farm_slot_has_kind0(event) == 0;
}

/* Whether the season, weekday and weather match the flags, and, for the tool-room and free-hand
 * bits, that the player has room for an item or a free hand. */
static s32 event_calendar_match(s32 flags)
{
    if (event_season_match(flags) == 0)
    {
        return 0;
    }
    if (event_weekday_match(flags) == 0)
    {
        return 0;
    }
    if (event_weather_match(flags) == 0)
    {
        return 0;
    }
    if (flags & EVENT_TRIGGER_TOOL_ROOM)
    {
        if (event_item_can_take(0, 0, 1, 0) == 0)
        {
            return 0;
        }
    }
    if (flags & EVENT_TRIGGER_FREE_HAND)
    {
        if (event_item_can_take(0, 1, 0, 0) == 0)
        {
            return 0;
        }
    }
    return 1;
}

static s32 event_season_match(u32 flags)
{
    u32 mask = 0;
    switch (game_state.progress.clock.date.season)
    {
    case SEASON_SPRING:
        mask = EVENT_TRIGGER_SPRING;
        break;
    case SEASON_SUMMER:
        mask = EVENT_TRIGGER_SUMMER;
        break;
    case SEASON_FALL:
        mask = EVENT_TRIGGER_FALL;
        break;
    case SEASON_WINTER:
        mask = EVENT_TRIGGER_WINTER;
        break;
    default:
        break;
    }
    return (mask & flags) != 0;
}

/* Select the active farm flag bit and test it against the current flags. */
static u32 event_weekday_match(u32 flags)
{
    s32 weekday;
    u32 bit;

    bit = 0;
    weekday = game_state.progress.clock.date.weekday;
    switch (weekday)
    {
    case WEEKDAY_SUNDAY:
        bit = EVENT_TRIGGER_SUNDAY;
        break;
    case WEEKDAY_MONDAY:
        bit = EVENT_TRIGGER_MONDAY;
        break;
    case WEEKDAY_TUESDAY:
        bit = EVENT_TRIGGER_TUESDAY;
        break;
    case WEEKDAY_WEDNESDAY:
        bit = EVENT_TRIGGER_WEDNESDAY;
        break;
    case WEEKDAY_THURSDAY:
        bit = EVENT_TRIGGER_THURSDAY;
        break;
    case WEEKDAY_FRIDAY:
        bit = EVENT_TRIGGER_FRIDAY;
        break;
    case WEEKDAY_SATURDAY:
        bit = EVENT_TRIGGER_SATURDAY;
        break;
    }
    return (bit & flags) != 0;
}

/* Whether the current weather matches the event's weather flags. */
static s32 event_weather_match(s32 flags)
{
    s32 original_flags;
    s32 masked;
    s32 weather;

    original_flags = flags;
    masked = original_flags & EVENT_TRIGGER_WEATHER_ANY;
    if (masked == EVENT_TRIGGER_WEATHER_ANY)
    {
        return 1;
    }
    weather = game_state.world.weather;
    masked = 0;
    switch (weather)
    {
    case WEATHER_SUNNY:
        masked = EVENT_TRIGGER_SUNNY;
        break;
    case WEATHER_SNOW:
        masked = EVENT_TRIGGER_SNOW;
        break;
    case WEATHER_RAIN:
        masked = EVENT_TRIGGER_RAIN;
        break;
    case WEATHER_STORM:
        masked = EVENT_TRIGGER_STORM;
        break;
    case WEATHER_BLIZZARD:
        masked = EVENT_TRIGGER_BLIZZARD;
        break;
    }
    return (masked & original_flags) != 0;
}

/* Unpacks six 5-bit fields of a packed date/time word into halfwords. A field of
 * 0x1F is a wildcard. The first three results are stored one lower (1-based in
 * the word, 0-based out). */
static void event_window_unpack(u32 packed, s16 *out)
{
    s32 i;
    s32 field;
    s32 wildcard;

    for (i = 0; i < 6; i++)
    {
        switch (i)
        {
        case 0:
            field = (packed >> 25) & 0x1F;
            wildcard = 1;
            break;
        case 1:
            field = (packed >> 20) & 0x1F;
            wildcard = 0x7FFF;
            break;
        case 2:
            field = (packed >> 15) & 0x1F;
            wildcard = EVENT_WINDOW_ANY;
            break;
        case 3:
            field = (packed >> 10) & 0x1F;
            wildcard = EVENT_WINDOW_ANY;
            break;
        case 4:
            field = (packed >> 5) & 0x1F;
            wildcard = EVENT_WINDOW_ANY;
            break;
        case 5:
            field = packed & 0x1F;
            wildcard = EVENT_WINDOW_ANY;
            break;
        default:
            field = 0;
            wildcard = 0;
            break;
        }
        if (field == EVENT_WINDOW_ANY)
        {
            field = wildcard;
        }
        switch (i)
        {
        case 0:
            out[i] = field - 1;
            break;
        case 1:
            out[i] = field - 1;
            break;
        case 2:
            out[i] = field - 1;
            break;
        case 3:
            out[i] = field;
            break;
        case 4:
            out[i] = field;
            break;
        case 5:
        default:
            out[i] = field;
            break;
        }
    }
}

/* Unpacks three 10-bit fields of `packed` into out[0..2]. */
static void event_flag_ids_unpack(u32 packed, u16 *out, s32 mode)
{
    s32 i;
    u16 v;

    for (i = 0; i < 3; i++)
    {
        if (mode == 1)
        {
            switch (i)
            {
            case 0:
                v = packed & 0x3FF;
                break;
            case 1:
                v = (packed >> 10) & 0x3FF;
                break;
            case 2:
                v = (packed >> 20) & 0x3FF;
                break;
            default:
                v = 0;
                break;
            }
        }
        else
        {
            switch (i)
            {
            case 0:
                v = packed & 0x3FF;
                break;
            case 1:
                v = (packed >> 10) & 0x3FF;
                break;
            case 2:
                v = (packed >> 20) & 0x3FF;
                break;
            default:
                v = 0;
                break;
            }
        }
        out[i] = v;
    }
}

/* The brides in wedding order (the index ENDING_ROUTE_MARRIED reports). */
enum WeddingOrder
{
    WEDDING_ORDER_KAREN = 0,
    WEDDING_ORDER_ANN = 1,
    WEDDING_ORDER_ELLI = 2,
    WEDDING_ORDER_MARY = 3,
    WEDDING_ORDER_POPURI = 4,
};

/* The bride for a wedding-order index (0..4). Anything else is Karen. */
static s32 bride_of_index(u32 arg)
{
    u32 i = (u16)arg;

    switch (i)
    {
    case WEDDING_ORDER_KAREN:
        return CHARACTER_KAREN;
    case WEDDING_ORDER_ANN:
        return CHARACTER_ANN;
    case WEDDING_ORDER_ELLI:
        return CHARACTER_ELLI;
    case WEDDING_ORDER_MARY:
        return CHARACTER_MARY;
    case WEDDING_ORDER_POPURI:
        return CHARACTER_POPURI;
    default:
        return CHARACTER_KAREN;
    }
}

static void event_noop(void)
{
}

/* The player: input, movement, tools, carried and thrown items. */

#include <sys/types.h>
#include <abs.h>
#include <memory.h>
#include <libgte.h>
#include "types.h"
#include "actor.h"
#include "anim.h"
#include "animal_actions.h"
#include "animals.h"
#include "calendar.h"
#include "cd_audio.h"
#include "chicken.h"
#include "clock.h"
#include "clock_events.h"
#include "cue.h"
#include "demo.h"
#include "dog.h"
#include "doors.h"
#include "estate.h"
#include "event_commands.h"
#include "event_flags.h"
#include "event_rules.h"
#include "farm.h"
#include "farm_data.h"
#include "farm_load.h"
#include "farm_probe.h"
#include "farm_state.h"
#include "farm_tables.h"
#include "farm_vars.h"
#include "field.h"
#include "field_item_requests.h"
#include "fishing.h"
#include "greenhouse.h"
#include "ground.h"
#include "harvest_sprites.h"
#include "horse.h"
#include "house.h"
#include "item_storage.h"
#include "items.h"
#include "livestock.h"
#include "main.h"
#include "math_util.h"
#include "menu_window.h"
#include "messages.h"
#include "mine.h"
#include "model.h"
#include "options_menu.h"
#include "pad.h"
#include "pad_vibration.h"
#include "photo_album.h"
#include "picture_cells.h"
#include "player.h"
#include "pond.h"
#include "scene.h"
#include "scene_view.h"
#include "screen_fade.h"
#include "shop_dialogues.h"
#include "sound.h"
#include "sound_request.h"
#include "status_pages.h"
#include "storage_swap.h"
#include "texture_pack.h"
#include "ui_icons.h"
#include "villager.h"
#include "weather.h"

typedef struct FarmHopStep {
    s16  dx;
    s16  dz;
} FarmHopStep;

typedef struct FarmSlideStep {
    s16  firstDx;
    s16  firstDz;
    s16  secondDx;
    s16  secondDz;
} FarmSlideStep;

/* Rows of the result camera table. */
typedef struct ResultCameraPair {
    s16 first;
    s16 second;
} ResultCameraPair;

typedef struct ResultCameraTable {
    ResultCameraPair pairs[3];
} ResultCameraTable;

typedef s32 (*CellFromWorldMoveCandidates)(s32 *, u16 *, u16 *);

typedef s32 (*CellFromWorldMoveRun)(s32 *, s16 *, s16 *);

typedef s32 (*CellFromWorldMoveUpdate)(s32 *, s16 *, s16 *);

typedef s32 (*CellFromWorldMoveResolve)(s32 *, s16 *, s16 *);

typedef s32 (*CellFromWorldToolClearAlt)(s32 *, u16 *, u16 *);

typedef s32 (*CellFromWorldToolClear)(s32 *, u16 *, u16 *);

typedef s32 (*WorldToCellHandler)(VECTOR *, s16 *, s16 *);

/* Hop steps indexed [row][facing]: row 0 while the jump crouches, row 1 otherwise. */
#define HOPS ((FarmHopStep(*)[12])move_steps_large)

/* Tool experience: the base gain, and the gain of a committed move. */
#define TOOL_EXP_BASE 40
#define TOOL_EXP_COMMIT 150
/* One tool level is this many toolCharge units. */
#define TOOL_CHARGE_STEP 20
/* Frames a full tool charge may be held before the tool is released on its own. */
#define OVERCHARGE_FRAMES 61

/* The idle frames after which the player lies down to sleep. */
#define PLAYER_SLEEP_IDLE_FRAMES 101

/* The watering can's refill: its count when it is full. */
#define WATERING_CAN_FULL 100

/* How far the ground may be off while the player is airborne, and otherwise. */
#define GROUND_TOLERANCE_JUMP 50
#define GROUND_TOLERANCE_DEFAULT 200
/* How many frames a blocked step waits before it gives up. */
#define BLOCKED_FRAMES_LIMIT 5

/* Fixed point: a velocity is divided by this to give the step per frame. */
#define FIXED_POINT_ONE 4096
/* A thrown item falls this much faster each frame, up to THROWN_ITEM_MAX_FALL. */
#define THROWN_ITEM_GRAVITY 9
#define THROWN_ITEM_MAX_FALL 45

/* The launch of a jump: how long the player rises before he turns, and how fast. */
#define JUMP_LAUNCH_TIMER 45
#define JUMP_LAUNCH_SPEED -250

/* Frames between footsteps; PLAYER_MODE_POSE_4 steps more often. */
#define STEP_SOUND_INTERVAL 12
#define STEP_SOUND_INTERVAL_POSE_4 8

/* The command that opens the farm overview (not an item). */
#define PLAYER_COMMAND_FARM_OVERVIEW 0x12D

typedef struct
{
    unsigned char pad[0x1a];
    u16 value;
} FarmNearbyAction;

typedef struct
{
    u16 talkVillager;
    u8 pad02[0x1C];
    u16 mode;
} FarmObjectStatus;

typedef struct
{
    u16 item;
    u8 pad02[0x22 - 0x02];
    u16 facing;
    u8 pad24[0xAC - 0x24];
    u16 talkVillager;
} FarmObjectRecord;

typedef struct FarmVelocity
{
    s32 x;
    s32 z;
} FarmVelocity;

/* The step of a tool's use on its target, passed to player_apply_tool_target. */
enum TargetPhase
{
    TARGET_PHASE_HIT = 0,      /* the first hit: the swing, or a farm tool's use */
    TARGET_PHASE_CONFIRM = 1,  /* the confirm step */
    TARGET_PHASE_BRUSH = 2,    /* brushing an animal */
    TARGET_PHASE_MEDICINE = 4, /* animal medicine */
    TARGET_PHASE_MILK = 5,     /* the milker */
    TARGET_PHASE_CLIP = 6,     /* the clippers */
    TARGET_PHASE_POTION = 8,   /* a miracle potion */
};

/* The kind of result a farm tool move commits: dig (hoe), gate (watering can) or harvest (sickle). */
enum ResultKind
{
    RESULT_KIND_DIG = 0,
    RESULT_KIND_GATE = 1,
    RESULT_KIND_HARVEST = 2,
};
static void player_update_controller_flags(void);
static void player_build_result(FarmPlayer *player);
static void player_result_params_init(s32 kind, s16 status, s32 originX, s32 originY, s16 mode, s32 steps);
static s32 player_run_action(FarmPlayer *player);
static void player_entry_refresh(FarmPlayer *player);
static s32 player_anim_advance(FarmPlayer *player);
static s32 player_start_heavy_carry(FarmPlayer *player);
static s32 player_mode_tick(FarmPlayer *player);
static void player_set_mode(FarmPlayer *player, s32 mode, s32 animation, s32 state);
static void player_anim_state_update(FarmPlayer *player, s32 mode, s32 animation, s32 state);
static void player_release_target(void);
static void player_zone_corn_use(FarmPlayer *player, FarmZone *unused);
static void player_zone_wool_box_use(FarmPlayer *player, FarmZone *zone);
static void player_zone_egg_box_use(FarmPlayer *player, FarmZone *zone);
static void player_zone_milk_box_use(FarmPlayer *player, FarmZone *zone);
static void player_zone_lumber_stock(FarmPlayer *player);
static void player_zone_place_chicken_feed(FarmPlayer *player, FarmZone *zone);
static void player_zone_place_fodder(FarmPlayer *player, FarmZone *zone);
static void player_zone_item_check(FarmPlayer *player);
static s32 player_deliver_to_villager(FarmPlayer *player, ZonePoint *point, FarmZone *zone);
static void player_zone_fodder_exchange(FarmPlayer *player);
static void player_zone_chicken_feed_exchange(FarmPlayer *player);
static s32 player_submit_item_action(FarmPlayer *player, s16 selected, s16 value);
static s32 player_advance_state(FarmPlayer *player);
static s32 player_input_step(void);
static s32 player_start_sleep(FarmPlayer *player);
static s32 player_update_mode(void);
static s32 player_location_kind(void);
static void player_compact_items(void);
static void player_compact_tools(void);
static s32 player_exchange_item(s32 mode);
static s32 player_exchange_tool(s32 mode);
static void player_draw_riding_cow(FarmPlayer *player);
static void player_cow_animate(FarmPlayer *player);
static s32 stored_item_table_compact(FarmStoredItem *table, s32 count);
static s32 stored_item_table_exchange(FarmStoredItem *current, s32 count, FarmStoredItem *other, s32 mode);
static s32 player_tool_animation(s16 tool);
static s16 player_tool_kind(s16 arg);
static s16 player_probe_target(FarmPlayer *player, VECTOR *pos, s32 kind_arg);
static void player_carry_tick(void);
static void player_set_facing(s16 facing);
static void thrown_item_clear(FarmThrownItem *item);
static FarmThrownItem *thrown_item_spawn(VECTOR *pos, FarmStoredItem *item, s16 dir);
static s32 thrown_item_land(FarmThrownItem *effect);
static void thrown_item_retire(FarmThrownItem *item);
static void stored_item_release(void *p);
static void player_load_tool_anim(FarmPlayer *player, s32 reload);
static s32 player_apply_tool_target(FarmPlayer *player, s32 phase);
static s32 player_sleep_stamina_loss(s32 minutes);
static s32 player_sleep_fatigue_gain(s32 value);
static void player_add_tool_exp(FarmPlayer *player, s32 amount);
static void player_autowalk_input(FarmPlayer *player);
static s32 player_update_condition_flags(FarmPlayer *player, s32 quiet);
static s32 player_start_fishing(FarmPlayer *player);
static s32 player_draw_toys(FarmPlayer *player);
static s32 player_draw_slow_trail(FarmPlayer *player);
static void player_facing_point(FarmPlayer *player, VECTOR *output, s32 mode);
static void player_draw_toy(VECTOR *point, FarmPlayer *player, s32 mode, s32 slot);
static void player_result_camera_points(s32 *result);
static s32 player_result_camera_step(FarmPlayer *player);
static s32 item_is_fish(s32 kind);
static s16 item_fish_print_id(s16 kind);
static s32 item_is_print(s16 kind);
static s32 player_settle_ground_tolerance(FarmGroundProbe *input);
static SVECTOR hand_vertex;
static FarmScreenXY hand_screen;
static FarmScreenXY scroll_screen_point;
static FarmScreenXY thrown_item_screen;
static SVECTOR thrown_item_vertex;
static FarmThrownItem thrown_items[3];
/* The object the current tool acts on, set by player_probe_target. */
static void *farmTarget;
static u32 player_status_word;
static u32 farmTargetFlags;
/* The item the player set aside while holding another. */
static FarmStoredItem player_stored_item;
static FarmFieldTile *tile_row_ptr;
static u16 player_step_counter;
static u32 blocked_frames;
static u32 tool_overcharge_frames;
static s16 player_sound_voice;
/* The animation a thrown item plays when it lands. */
static AnimState thrown_item_anim;
static s16 input_step_result;
static s32 tool_step_index;
static s32 tool_step_count;
static s32 tool_exp_amount;
static u16 tool_origin_x;
static u16 tool_origin_y;
static s16 tool_target_kind;
static s16 player_result_kind;
static s16 tool_cell_mode;
static s32 result_camera_start[4];
static s32 result_camera_end[3];
static s32 result_camera_frame;
static FarmZone *probe_zone;
static void *action_object;
static u32 idle_turn_pending;
static AnimState winter_anim_state;
static u32 idle_frames;

VramPlace player_sprite_places[2][15] = {
    {
        {0x000, 0x1E5, 0x3C0, 0x100},
        {0x000, 0x1E7, 0x3C0, 0x140},
        {0x010, 0x1E7, 0x3C8, 0x140},
        {0x020, 0x1E7, 0x3C5, 0x140},
        {0x030, 0x1E7, 0x3C8, 0x140},
        {0x040, 0x1E7, 0x3C0, 0x164},
        {0x050, 0x1E7, 0x3C8, 0x140},
        {0x060, 0x1E7, 0x3C6, 0x140},
        {0x070, 0x1E7, 0x3C8, 0x140},
        {0x080, 0x1E7, 0x3C8, 0x151},
        {0x090, 0x1E7, 0x3C0, 0x160},
        {0x0A0, 0x1E7, 0x3C4, 0x160},
        {0x0B0, 0x1E7, 0x3C8, 0x140},
        {0x0C0, 0x1E7, 0x3C0, 0x158},
        {0x0D0, 0x1E7, 0x3C8, 0x154},
    },
    {
        {0x000, 0x1E6, 0x3E0, 0x100},
        {0x000, 0x1E8, 0x3E0, 0x140},
        {0x010, 0x1E8, 0x3E8, 0x140},
        {0x020, 0x1E8, 0x3E5, 0x140},
        {0x030, 0x1E8, 0x3E8, 0x140},
        {0x040, 0x1E8, 0x3E0, 0x164},
        {0x050, 0x1E8, 0x3E8, 0x140},
        {0x060, 0x1E8, 0x3E6, 0x140},
        {0x070, 0x1E8, 0x3E8, 0x140},
        {0x080, 0x1E8, 0x3E8, 0x151},
        {0x090, 0x1E8, 0x3E0, 0x160},
        {0x0A0, 0x1E8, 0x3E4, 0x160},
        {0x0B0, 0x1E8, 0x3E8, 0x140},
        {0x0C0, 0x1E8, 0x3E0, 0x158},
        {0x0D0, 0x1E8, 0x3E8, 0x154},
    },
};

s16 tool_tint_modifier_ids[6] = {0x0070, 0x0059, 0x00A3, 0x005A, 0x005B, 0x0000};

s16 tool_charge_tint_ids[6] = {0x00DF, 0x00DA, 0x00DD, 0x00DB, 0x00DC, 0x0000};

s16 tool_tint_modifier_alt_ids[6] = {0x0063, 0x0059, 0x00A3, 0x005A, 0x005B, 0x0000};

s16 move_steps_small[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, -41, -41, 0, 0, 41, 41, 0};

s16 move_steps_large[16] = {
    0,
    -73,
    -73,
    0,
    0,
    73,
    73,
    0,
    0,
    -128,
    -128,
    0,
    0,
    128,
    128,
    0,
};

s16 move_steps_riding[8] = {
    0,
    -112,
    -112,
    0,
    0,
    112,
    112,
    0,
};

static FarmHopStep hop_steps[4] = {
    {0, -51},
    {-51, 0},
    {0, 51},
    {51, 0},
};

/* The (dx, dz) reach for each facing. */
s16 facing_reach[8][2] = {
    {0, -224},
    {-224, 0},
    {0, 224},
    {224, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
};

static FarmVelocity facing_velocities[4] = {{0, -180224}, {-180224, 0}, {0, 180224}, {180224, 0}};

u32 step_sound_timer = 0;
u32 winter_anim_loaded = 0;
u32 livestock_bump_count = 0;

/* The (dx, dy) screen offsets of an actor's mark: four groups (by kind) of four
 * directions. */
s16 mark_screen_offsets[16][2] = {
    {-8, -10},
    {-6, -18},
    {6, -18},
    {8, -10},
    {-2, -22},
    {0, -24},
    {0, -24},
    {-2, -22},
    {-1, -31},
    {-1, -31},
    {-1, -31},
    {-1, -31},
    {-9, -18},
    {-9, -18},
    {-9, -18},
    {-9, -18},
};

s16 tool_charge_steps_a[6] = {1, 2, 3, 4, 6, 0};

s16 tool_charge_steps_b[6] = {1, 3, 6, 9, 15, 0};

s16 tool_charge_steps_c[6] = {1, 3, 6, 9, 25, 0};

/* The stamina cost of a swing: [tool kind][charge step]. */
s16 tool_stamina_costs[5][5] = {
    {-1, -2, -3, -4, -5},
    {-1, -2, -3, -4, -5},
    {-1, -2, -3, -4, -5},
    {-1, -2, -3, -4, -5},
    {-1, -2, -3, -4, -5},
};

s32 farm_buttons_held = 0;
s32 farm_buttons_pressed = 0;

/* Per-frame update of the farm player: input, state, timer and trail. */
void player_frame_update(void)
{
    FarmPlayer *player;
    s32 expired;

    player = &game_state.player;
    if (game_state.globalFlags & GLOBAL_FLAG_PAUSED)
    {
        if (player_sound_voice != SOUND_VOICE_NONE)
        {
            sound_release_voice(player_sound_voice);
            player_sound_voice = SOUND_VOICE_NONE;
        }
        return;
    }
    if (clock_is_pending() == 1)
    {
        return;
    }
    if ((player->stateFlags & (PLAYER_FLAG_NO_INPUT | PLAYER_FLAG_SCRIPTED)) == 0 && fade_is_settled() == 0)
    {
        return;
    }
    if (player->stateFlags & PLAYER_FLAG_SCENE_CHANGE)
    {
        return;
    }
    if (scene_place_mismatch(&player->place.value) != 0)
    {
        return;
    }
    if (player->stateFlags & PLAYER_FLAG_REFRESH_HELD)
    {
        player_refresh_carry_state(player);
        player->stateFlags &= ~PLAYER_FLAG_REFRESH_HELD;
    }
    if (event_flag_get(EVENT_FLAG_MORNING_MESSAGE) == 1)
    {
        event_flag_set(EVENT_FLAG_MORNING_MESSAGE, 0);
        menu_request_open(0, 0x724);
    }
    player_build_result(player);
    if (player->stateFlags & PLAYER_FLAG_POSE_RESET_PENDING)
    {
        if ((player->stateFlags & (PLAYER_FLAG_FISHING | PLAYER_FLAG_MODE_ANIM)) == 0)
        {
            player_reset_pose();
            livestock_bump_count = 0;
            player->stateFlags &= ~PLAYER_FLAG_POSE_RESET_PENDING;
        }
    }
    player->latches &= ~PLAYER_LATCH_MARKER;
    if (player->stateFlags & PLAYER_FLAG_SCRIPTED)
    {
        farm_buttons_held = game_state.player.scriptHeld;
        farm_buttons_pressed = game_state.player.scriptPressed;
    }
    else
    {
        farm_buttons_held = input_pad1->held;
        farm_buttons_pressed = input_pad1->pressed;
        if (demo_step() != 0)
        {
            return;
        }
        player_update_controller_flags();
        if (player->latches & PLAYER_LATCH_UNPAUSE)
        {
            if (farm_buttons_held & PAD_CIRCLE)
            {
                farm_buttons_held &= ~PAD_CIRCLE;
                farm_buttons_pressed &= ~PAD_CIRCLE;
            }
            else
            {
                player->latches &= ~PLAYER_LATCH_UNPAUSE;
            }
        }
    }
    cue_countdown(&player->cue);
    input_step_result = 0;
    if ((player->stateFlags & PLAYER_FLAG_MODE_ANIM) == 0)
    {
        switch (player->stateFlags & (PLAYER_FLAG_FISHING | PLAYER_FLAG_SCRIPTED))
        {
        case PLAYER_FLAG_SCRIPTED:
            player_autowalk_input(player);
            if (player_input_step() != 0)
            {
                player_step_counter = 0;
            }
            if (player->stateFlags & PLAYER_FLAG_AUTO_WALK)
            {
                if (player->countdown == 0)
                {
                    player->stateFlags &= ~(PLAYER_FLAG_NO_INPUT | PLAYER_FLAG_AUTO_WALK);
                }
                else
                {
                    player->countdown--;
                }
            }
            break;
        case PLAYER_FLAG_FISHING:
            fishing_player_frame(player);
            return;
        case 0:
            player_autowalk_input(player);
            if ((input_step_result = player_input_step()) != 0)
            {
                player_step_counter = 0;
                idle_frames = 0;
            }
            else if (idle_frames == 16)
            {
                idle_frames = 0;
                idle_turn_pending = 1;
                if (player->facing38 == FACING_UP || player->facing38 == FACING_DOWN)
                {
                    idle_turn_pending = 0;
                }
            }
            else
            {
                idle_frames++;
            }
            if (player->stateFlags & PLAYER_FLAG_AUTO_WALK)
            {
                if (player->countdown == 0)
                {
                    player->stateFlags &= ~(PLAYER_FLAG_NO_INPUT | PLAYER_FLAG_AUTO_WALK);
                }
                else
                {
                    player->countdown--;
                }
            }
            break;
        }
    }
    else
    {
        player_mode_tick(player);
    }
    expired = 0;
    if (player->modeState != -2)
    {
        if (player->modeState != -3)
        {
            player->modeState--;
        }
        if (player->modeState == -1)
        {
            expired = 1;
        }
    }
    else if ((player->latches & PLAYER_LATCH_SPRITE_REQUEST) || (player->secondSprite.flags & 0x100))
    {
        expired = 1;
    }
    if (expired)
    {
        if (player_run_action(player))
        {
            player_set_mode(player, PLAYER_MODE_IDLE, 0, -2);
            player->stateFlags = (player->stateFlags & ~PLAYER_FLAG_MODE_ANIM) | PLAYER_FLAG_IDLE_POSE;
        }
    }
    if (game_state.frameCount & 1)
    {
        player->trail[9] = player->trail[8];
        player->trail[8] = player->trail[7];
        player->trail[7] = player->trail[6];
        player->trail[6] = player->trail[5];
        player->trail[5] = player->trail[4];
        player->trail[4] = player->trail[3];
        player->trail[3] = player->trail[2];
        player->trail[2] = player->trail[1];
        player->trail[1] = player->trail[0];
        player->trail[0] = player->position;
    }
    if ((game_state.frameCount & 7) == 7)
    {
        player->slowTrail[9] = player->slowTrail[8];
        player->slowTrail[8] = player->slowTrail[7];
        player->slowTrail[7] = player->slowTrail[6];
        player->slowTrail[6] = player->slowTrail[5];
        player->slowTrail[5] = player->slowTrail[4];
        player->slowTrail[4] = player->slowTrail[3];
        player->slowTrail[3] = player->slowTrail[2];
        player->slowTrail[2] = player->slowTrail[1];
        player->slowTrail[1] = player->slowTrail[0];
        player->poses[9] = player->poses[8];
        player->poses[8] = player->poses[7];
        player->poses[7] = player->poses[6];
        player->poses[6] = player->poses[5];
        player->poses[5] = player->poses[4];
        player->poses[4] = player->poses[3];
        player->poses[3] = player->poses[2];
        player->poses[2] = player->poses[1];
        player->poses[1] = player->poses[0];
        if ((player->stateFlags & PLAYER_FLAG_RIDING) == 0)
        {
            player->slowTrail[0] = player->position;
            player->poses[0].facing = (game_state.frameCount & 0x8) >> 3;
            player->poses[0].animation = player->facing38;
        }
        else
        {
            player->poses[0].animation = 5;
        }
    }
}

/* Permute the four pad bits 0x1000..0x8000 when the option is on (a pending idle turn, or the
 * message option set to 1); the other buttons pass through. */
static void player_update_controller_flags(void)
{
    s32 enabled;
    s16 mode;
    s32 first;
    s32 second;

    enabled = 0;
    if (game_state.world.options.messageType == 3)
    {
        if (input_pad1->analog >= 86)
        {
            farm_buttons_held |= 0x20;
        }
    }
    if (idle_turn_pending != 0)
    {
        enabled = 1;
    }
    mode = game_state.world.options.messageType;
    if (mode == 1)
    {
        enabled = 1;
    }
    if (mode == 0)
    {
        enabled = 0;
    }
    if (mode == 3)
    {
        enabled = 0;
    }
    if (enabled != 0)
    {
        first = farm_buttons_held;
        second = farm_buttons_pressed;
        farm_buttons_held = first & 0xFFFF0FFF;
        farm_buttons_pressed = second & 0xFFFF0FFF;
        if (first & 0x1000)
        {
            farm_buttons_held |= 0x8000;
        }
        if (second & 0x1000)
        {
            farm_buttons_pressed |= 0x8000;
        }
        if (first & 0x4000)
        {
            farm_buttons_held |= 0x2000;
        }
        if (second & 0x4000)
        {
            farm_buttons_pressed |= 0x2000;
        }
        if (first & 0x8000)
        {
            farm_buttons_held |= 0x4000;
        }
        if (second & 0x8000)
        {
            farm_buttons_pressed |= 0x4000;
        }
        if (first & 0x2000)
        {
            farm_buttons_held |= 0x1000;
        }
        if (second & 0x2000)
        {
            farm_buttons_pressed |= 0x1000;
        }
    }
}

/* Probe the tile ahead of the player; the ground hit goes to player_status_word, 0x8000 when it has no high bits. */
static void player_build_result(FarmPlayer *player)
{
    VECTOR values;
    s32 index;
    s16 *table;
    s32 result_value;
    u32 *status_word;

    player_probe_target(player, &player->position, 0x18);
    values.vy = player->position.vy;
    values.pad = player->position.pad;
    index = player->facing38 << 2;
    values.vx = player->position.vx;
    values.vz = player->position.vz;
    table = facing_reach[0] + index / 2;
    values.vx += table[0];
    values.vz += table[1];
    farm_settle_on_ground(&values, 0);
    status_word = &player_status_word;
    result_value = *(s32 *)ground_hit_primitive();
    *status_word = result_value;
    if (!(result_value & 0xFFFF0000))
    {
        *status_word = 0x8000;
    }
}

u16 player_anim_banks[28] = {
    424,
    424,
    496,
    380,
    508,
    484,
    148,
    260,
    460,
    424,
    776,
    420,
    352,
    480,
    360,
    312,
    424,
    360,
    1788,
    700,
    804,
    540,
    3012,
    1960,
    780,
    380,
    3012,
    0,
};

static void *world_to_cell_handlers[2] = {field_world_to_cell, greenhouse_world_to_cell};

/* Build and validate farm movement candidates for each of nine slots. */
static void player_build_move_candidates(FarmPlayer *player)
{
    s32 status;
    s32 slot;
    s16 mode;
    s32 values[3];
    u16 resultX;
    u16 resultY;
    s16 deltaX;
    s16 deltaY;
    CellFromWorldMoveCandidates callback;

    status = player_location_kind();
    values[0] = player->position.vx;
    values[1] = player->position.vy;
    values[2] = player->position.vz;
    if (status == 2)
    {
        return;
    }
    callback = world_to_cell_handlers[status];
    callback(values, &resultX, &resultY);
    slot = 0;
    do
    {
        if (picture_grid_find_cell(&player->held[HELD_TOOL].id, slot, player->facing38, &deltaX, &deltaY, 0) != 0)
        {
            mode = status;
            deltaX = deltaX + resultX;
            deltaY = deltaY + resultY;
            if (field_coords_check(deltaX, deltaY, 1, mode) == 0)
            {
                field_event_dispatch(mode, deltaX, deltaY, player->held[HELD_TOOL].id);
            }
        }
        slot++;
    } while (slot < 9U);
}

/* Run the movement callback for the tool's mode and schedule the result. Unlike
 * player_move_record_update, this does not use up the tool's count. */
static s32 player_move_record_run(FarmPlayer *player)
{
    s32 status;
    s32 values[3];
    s32 mode;
    s32 result;
    s16 callbackX;
    s16 callbackY;
    CellFromWorldMoveRun callback;

    status = player_location_kind();
    if (status == 2)
    {
        player_add_tool_exp(player, TOOL_EXP_BASE);
        return 0;
    }
    values[0] = player->position.vx;
    values[1] = player->position.vy;
    values[2] = player->position.vz;
    mode = (player->toolCharge / TOOL_CHARGE_STEP);
    result = tool_charge_steps_a[mode];
    mode = mode - player->held[HELD_TOOL].modifier;
    callback = world_to_cell_handlers[status];
    callback(values, &callbackX, &callbackY);
    player_result_params_init(RESULT_KIND_DIG, status, callbackX, callbackY, mode, result);
    player_set_mode(player, PLAYER_MODE_RESULT_STEP, 0, -2);
    return 1;
}

/* Update a farm movement record, invoke its mode callback, and schedule the result. */
static s32 player_move_record_update(FarmPlayer *player)
{
    s32 status;
    s32 values[3];
    s32 mode;
    s32 result;
    s16 callbackX;
    s16 callbackY;
    s32 difference;
    s16 count;
    CellFromWorldMoveUpdate callback;
    status = player_location_kind();
    if (status == 2)
    {
        player_add_tool_exp(player, TOOL_EXP_BASE);
        return 0;
    }
    values[0] = player->position.vx;
    values[1] = player->position.vy;
    values[2] = player->position.vz;
    mode = (player->toolCharge / TOOL_CHARGE_STEP);
    result = tool_charge_steps_b[mode];
    count = player->held[HELD_TOOL].count;
    mode = mode - player->held[HELD_TOOL].modifier;
    difference = count - result;
    if (count < result)
        result = count - 1;
    player->held[HELD_TOOL].count = difference;
    if ((difference << 16) < 0)
        player->held[HELD_TOOL].count = 0;
    callback = world_to_cell_handlers[status];
    callback(values, &callbackX, &callbackY);
    player_result_params_init(RESULT_KIND_GATE, status, callbackX, callbackY, mode, result);
    player_set_mode(player, PLAYER_MODE_RESULT_STEP, 0, -2);
    return 1;
}

/* Resolve a farm movement: out of reach, cancel it (150 tool experience when the
 * farm target allows, 40 otherwise); else run its mode callback and schedule the
 * result. */
static s32 player_move_record_resolve(FarmPlayer *player)
{
    s32 status;
    s32 values[3];
    s32 mode;
    s32 result;
    s16 callbackX;
    s16 callbackY;
    CellFromWorldMoveResolve callback;

    status = player_location_kind();
    if (status == 2)
    {
        if (farmTargetFlags & 0x8000)
        {
            if (estate_record_submit(farmTarget) != 0)
            {
                player_add_tool_exp(player, TOOL_EXP_COMMIT);
            }
        }
        else
        {
            player_add_tool_exp(player, TOOL_EXP_BASE);
        }
        return 0;
    }
    values[0] = player->position.vx;
    values[1] = player->position.vy;
    values[2] = player->position.vz;
    mode = (player->toolCharge / TOOL_CHARGE_STEP);
    result = tool_charge_steps_c[mode];
    mode = mode - player->held[HELD_TOOL].modifier;
    callback = world_to_cell_handlers[status];
    callback(values, &callbackX, &callbackY);
    player_result_params_init(RESULT_KIND_HARVEST, status, callbackX, callbackY, mode, result);
    player_set_mode(player, PLAYER_MODE_RESULT_STEP, 0, -2);
    return 1;
}

/* Show the held tool (use_index 0) or the stored tool `index` in the player's hands. */
void player_show_tool(FarmPlayer *player, s32 use_index, s32 index, s32 state)
{
    FarmStoredItem *entry;
    s32 flags;

    if (use_index == 0)
    {
        entry = &player->held[HELD_TOOL];
    }
    else
    {
        entry = &player->tools[index];
    }
    if (entry->id != 0)
    {
        flags = player->stateFlags & ~(PLAYER_FLAG_CARRY_BIG_ITEM | PLAYER_FLAG_CARRY_ITEM);
        player->stateFlags = flags;
        player_anim_state_update(player, PLAYER_MODE_SHOW_TOOL, 0x47, state);
        stored_item_copy(&player->shown, entry);
        player->stateFlags |= PLAYER_FLAG_SHOWING_ITEM;
        sound_play(7);
    }
}

/* Start the rucksack upgrade when the held item is one of the two rucksacks. */
void player_start_rucksack(FarmPlayer *player)
{
    u32 mask;

    if (((u16)player->held[HELD_ITEM].id - ITEM_RUCKSACK_LEVEL_2) < 2U)
    {
        sound_play(8);
        mask = ~(PLAYER_FLAG_CARRY_ITEM | PLAYER_FLAG_SHOWING_ITEM);
        player->stateFlags &= mask;
        player_set_mode(player, PLAYER_MODE_RUCKSACK_UPGRADE, player->facing38 + 0x1D, -2);
    }
}

/* Set the parameters of the next result screen's move. */
static void player_result_params_init(s32 kind, s16 status, s32 originX, s32 originY, s16 mode, s32 steps)
{
    tool_step_index = 0;
    tool_exp_amount = TOOL_EXP_BASE;
    tool_target_kind = status;
    player_result_kind = kind;
    tool_origin_x = originX;
    tool_origin_y = originY;
    tool_step_count = steps;
    tool_cell_mode = mode;
}

/* One step of the move set up by player_result_params_init: while the step counter
 * has not passed the last step, take the step's offset, move it by the origin, and if
 * the target is valid, try the commit that `kind` selects. A commit raises the
 * experience to 150. Past the last step, the experience is added and the function
 * returns 1. */
static s32 player_result_step(FarmPlayer *player)
{
    s16 x;
    s16 y;

    if (tool_step_index <= tool_step_count)
    {
        if (picture_grid_find_cell(&player->held[HELD_TOOL].id, tool_step_index, player->facing38, &x, &y, tool_cell_mode) != 0)
        {
            x += tool_origin_x;
            y += tool_origin_y;
            if (field_coords_check(x, y, 0, tool_target_kind) != 1)
            {
                switch (player_result_kind)
                {
                case RESULT_KIND_DIG:
                    if (field_request_dispatch(tool_target_kind, x, y) == 1)
                    {
                        tool_exp_amount = TOOL_EXP_COMMIT;
                    }
                    else
                    {
                        /* a refused dig ends the move: no later step runs */
                        tool_step_index = tool_step_count + 1;
                    }
                    break;
                case RESULT_KIND_GATE:
                    if (field_gate_open(tool_target_kind, x, y, 0) == 1)
                    {
                        tool_exp_amount = TOOL_EXP_COMMIT;
                    }
                    break;
                case RESULT_KIND_HARVEST:
                    if (field_tile_harvest(tool_target_kind, x, y) == 1)
                    {
                        tool_exp_amount = TOOL_EXP_COMMIT;
                    }
                    break;
                }
            }
        }
    }
    else
    {
        player_add_tool_exp(player, tool_exp_amount);
        return 1;
    }
    tool_step_index++;
    return 0;
}

/* Like player_tool_clear, but calls field_tile_clear_tool_alt and estate_record_submit_costed:
 * run the movement callback for the current status and, on a valid step, add 1 << quotient
 * to the step counter and try to commit the move. A commit gives TOOL_EXP_COMMIT tool
 * experience, otherwise TOOL_EXP_BASE. */
static void player_tool_clear_alt(FarmPlayer *player)
{
    s32 status;
    s32 found;
    s32 values[3];
    s16 quotient;
    s16 total;
    u16 origin_x;
    u16 origin_y;
    s16 delta_x;
    s16 delta_y;
    s32 delay;
    delay = TOOL_EXP_BASE;
    status = player_location_kind();
    quotient = player->toolCharge / TOOL_CHARGE_STEP;
    values[0] = player->position.vx;
    values[1] = player->position.vy;
    values[2] = player->position.vz;
    if (status != 2)
    {
        ((CellFromWorldToolClearAlt)world_to_cell_handlers[status])(values, &origin_x, &origin_y);
        found = picture_grid_find_cell(&player->held[HELD_TOOL].id, 0, player->facing38, &delta_x, &delta_y, 0);
        if (found == 1)
        {
            delta_x = delta_x + origin_x;
            delta_y = delta_y + origin_y;
            if (field_coords_check(delta_x, delta_y, 0, status) == 0)
            {
                total = (player_step_counter = (player_step_counter + (found << quotient)));
                if (field_tile_clear_tool_alt(status, delta_x, delta_y, total, player->held[HELD_TOOL].modifier) ==
                    found)
                {
                    delay = TOOL_EXP_COMMIT;
                }
            }
        }
    }
    else if ((farmTargetFlags & 0x8000) != 0)
    {
        player_step_counter = (total = ((player_step_counter) + (1 << quotient)));
        if (estate_record_submit_costed(farmTarget, total, player->held[HELD_TOOL].modifier) == 1)
        {
            delay = TOOL_EXP_COMMIT;
        }
    }
    player_add_tool_exp(player, delay);
}

/* Clear the tile ahead with the held tool (or the farm target when it is an estate record). */
static void player_tool_clear(FarmPlayer *player)
{
    s32 status;
    s32 found;
    s32 values[3];
    s16 quotient;
    s16 total;
    u16 origin_x;
    u16 origin_y;
    s16 delta_x;
    s16 delta_y;
    s32 delay;
    delay = TOOL_EXP_BASE;
    status = player_location_kind();
    quotient = player->toolCharge / TOOL_CHARGE_STEP;
    values[0] = player->position.vx;
    values[1] = player->position.vy;
    values[2] = player->position.vz;
    if (status != 2)
    {
        ((CellFromWorldToolClear)world_to_cell_handlers[status])(values, &origin_x, &origin_y);
        found = picture_grid_find_cell(&player->held[HELD_TOOL].id, 0, player->facing38, &delta_x, &delta_y, 0);
        if (found == 1)
        {
            delta_x = delta_x + origin_x;
            delta_y = delta_y + origin_y;
            if (field_coords_check(delta_x, delta_y, 0, status) == 0)
            {
                total = (player_step_counter = (player_step_counter + (found << quotient)));
                if (field_tile_clear_tool(status, delta_x, delta_y, total, player->held[HELD_TOOL].modifier) ==
                    found)
                {
                    delay = TOOL_EXP_COMMIT;
                }
            }
        }
    }
    else if ((farmTargetFlags & 0x8000) != 0)
    {
        player_step_counter = (total = ((player_step_counter) + (1 << quotient)));
        if (estate_record_submit_limited(farmTarget, total, player->held[HELD_TOOL].modifier) == 1)
        {
            delay = TOOL_EXP_COMMIT;
        }
    }
    player_add_tool_exp(player, delay);
}

/* Put the held item on the selected farm tile. The winter stone, branch and lumber
 * are stored as their summer items. */
static void player_selection_to_result(FarmPlayer *player)
{
    VECTOR data;
    s16 row;
    s16 column;
    s32 state;
    FarmFieldTile *output;
    state = player_location_kind();
    player_facing_point(player, &data, 1);
    if (state != 2)
    {
        ((WorldToCellHandler)world_to_cell_handlers[state])(&data, &row, &column);
        if (state != 0)
        {
            output = &game_state.slots7x7[column].rows[row];
            greenhouse_retexture_quad(row, column, 0);
        }
        else
        {
            output = &game_state.fieldTiles[column].rows[row];
            field_cell_retexture(row, column, 0);
        }
        output->value = player->held[HELD_ITEM].id;
        if (output->value == ITEM_WINTER_STONE)
        {
            output->value = ITEM_STONE;
        }
        if (output->value == ITEM_WINTER_BRANCH)
        {
            output->value = ITEM_BRANCH;
        }
        if (output->value == ITEM_WINTER_LUMBER)
        {
            output->value = ITEM_LUMBER;
        }
        output->kind = 0;
        output->unused5 = 0;
        output->growthFlags = 0;
        output->ground = ITEM_SOIL;
    }
}

/* Turn to each facing in turn and keep the first where the spot settles and the probe is clear. */
static void player_calc_first_valid(FarmPlayer *player)
{
    VECTOR values;
    s32 index;
    u16 saved_index;

    saved_index = player->facing38;
    index = 0;
    do
    {
        player->facing38 = index;
        values.vx = player->position.vx;
        values.vy = player->position.vy;
        values.vz = player->position.vz;
        values.pad = player->position.pad;
        player_facing_point(player, &values, 2);
        if ((farm_settle_on_ground(&values, 0xC080) | player_probe_target(player, &values, 0x18)) == 0)
        {
            player->position.vx = values.vx;
            player->position.vy = values.vy;
            player->position.vz = values.vz;
            player->position.pad = values.pad;
            return;
        }
        index++;
    } while (index < 4U);
    player->facing38 = saved_index;
}

static u16 rucksack_item_slots[4] = {2, 4, 8, 0};

static s32 anim_wait_frames[3] = {
    15,
    15,
    -2,
};

/* Run the current mode's action: while an action is armed, end it and use the tool or held
 * item; otherwise run the mode's step. Returns 1 when the mode is finished and the player goes
 * back to idle. */
static s32 player_run_action(FarmPlayer *player)
{
    VECTOR timing;
    FarmStoredItem cheese_item;
    FarmStoredItem yarn_item;
    FarmStoredItem mayo_item;
    s16 tool;
    s16 kind;
    s16 stamina;
    s16 row;
    s16 column;
    s16 *step;
    s32 amount;
    s32 charge;
    s32 aborted;

    aborted = 0;
    if (player->stateFlags & PLAYER_FLAG_ACTION)
    {
        switch (player->mode)
        {
        case PLAYER_MODE_STAMINA_HALF:
        case PLAYER_MODE_STAMINA_LOW:
        case PLAYER_MODE_STAMINA_CRITICAL:
        case PLAYER_MODE_EXHAUSTED:
        case PLAYER_MODE_41:
        case PLAYER_MODE_FATIGUE_50:
        case PLAYER_MODE_FATIGUE_80:
            player->stateFlags &= ~PLAYER_FLAG_BIT14;
            player_end_action(player);
            break;
        case PLAYER_MODE_REFILL_CAN:
            player_end_action(player);
            sound_play(0x15);
            player->held[HELD_TOOL].count = WATERING_CAN_FULL;
            farm_clamp_status(-1, player_fatigue_cost(0), 0, 0);
            break;
        case PLAYER_MODE_USE_POTION:
            player_end_action(player);
            sound_play(0x11);
            player_apply_tool_target(player, TARGET_PHASE_POTION);
            if (player->latches & PLAYER_LATCH_FATIGUE_LOCKED)
            {
                item_run_use(&player->held[HELD_TOOL]);
            }
            stored_item_release(&player->held[HELD_TOOL]);
            player->stateFlags &= ~(PLAYER_FLAG_BIT2 | PLAYER_FLAG_SHOWING_ITEM);
            player_reset_pose();
            farm_clamp_status(0, player_fatigue_cost(0), 0, 0);
            return 0;
        case PLAYER_MODE_USE_ANIMAL_TOOL + ANIMAL_TOOL_BELL:
            player_end_action(player);
            farm_mode_check_start(0);
            if (player->latches & PLAYER_LATCH_FATIGUE_LOCKED)
            {
                item_run_use(&player->held[HELD_TOOL]);
            }
            player_reset_pose();
            farm_clamp_status(0, player_fatigue_cost(0), 0, 0);
            return 0;
        case PLAYER_MODE_USE_MEDICINE:
            player_end_action(player);
            sound_play(0x11);
            player_apply_tool_target(player, TARGET_PHASE_MEDICINE);
            if (player->latches & PLAYER_LATCH_FATIGUE_LOCKED)
            {
                item_run_use(&player->held[HELD_TOOL]);
            }
            stored_item_release(&player->held[HELD_TOOL]);
            player->stateFlags &= ~(PLAYER_FLAG_BIT2 | PLAYER_FLAG_SHOWING_ITEM);
            player_reset_pose();
            farm_clamp_status(0, player_fatigue_cost(0), 0, 0);
            return 0;
        case PLAYER_MODE_USE_ANIMAL_TOOL + ANIMAL_TOOL_CLIPPERS:
            player_end_action(player);
            player_apply_tool_target(player, TARGET_PHASE_CLIP);
            if (player->latches & PLAYER_LATCH_FATIGUE_LOCKED)
            {
                item_run_use(&player->held[HELD_TOOL]);
            }
            player->stateFlags &= ~PLAYER_FLAG_BIT2;
            farm_clamp_status(0, player_fatigue_cost(0), 0, 0);
            return 0;
        case PLAYER_MODE_USE_ANIMAL_TOOL + ANIMAL_TOOL_BRUSH:
            player_end_action(player);
            player_apply_tool_target(player, TARGET_PHASE_BRUSH);
            if (player->latches & PLAYER_LATCH_FATIGUE_LOCKED)
            {
                item_run_use(&player->held[HELD_TOOL]);
            }
            player_reset_pose();
            farm_clamp_status(0, player_fatigue_cost(0), 0, 0);
            return 0;
        case PLAYER_MODE_USE_ANIMAL_TOOL + ANIMAL_TOOL_MILKER:
            player_end_action(player);
            player_apply_tool_target(player, TARGET_PHASE_MILK);
            if (player->latches & PLAYER_LATCH_FATIGUE_LOCKED)
            {
                item_run_use(&player->held[HELD_TOOL]);
            }
            player->stateFlags &= ~PLAYER_FLAG_BIT2;
            farm_clamp_status(0, player_fatigue_cost(0), 0, 0);
            return 0;
        case PLAYER_MODE_PLANT_SEED:
            player_end_action(player);
            sound_play(0xE);
            player_build_move_candidates(player);
            player->stateFlags &= ~PLAYER_FLAG_CARRY_ITEM;
            stored_item_release(&player->held[HELD_TOOL]);
            player_reset_pose();
            farm_clamp_status(-1, player_fatigue_cost(0), 0, 0);
            break;
        case PLAYER_MODE_LIE_DOWN:
            player_anim_state_update(player, PLAYER_MODE_ASLEEP, 1, -2);
            return 0;
        case PLAYER_MODE_SHOW_ITEM_PICKUP:
            player->mode = PLAYER_MODE_SHOW_ITEM_2;
            player_entry_refresh(player);
            player_end_action(player);
            player->stateFlags &= ~PLAYER_FLAG_BIT14;
            player_reset_pose();
            return 0;
        case PLAYER_MODE_JUMP_CROUCH:
            sound_play(0x10);
            player->velocity[1] = 0;
            player->facing38 = FACING_DOWN;
            player->stateFlags |= PLAYER_FLAG_STILL;
            player_set_mode(player, PLAYER_MODE_JUMP_TAKEOFF, 0, -2);
            break;
        case PLAYER_MODE_JUMP_TAKEOFF:
            player->latches |= PLAYER_LATCH_JUMPING;
            player->stateFlags = (player->stateFlags | PLAYER_FLAG_ACTION_START) & ~PLAYER_FLAG_AIRBORNE;
            player_set_mode(player, 0x3C, 0, one_minute * 10);
            player->stateFlags |= PLAYER_FLAG_AIRBORNE;
            player_anim_state_update(player, PLAYER_MODE_JUMP_RISE, 0, one_minute * 10);
            pad_vibration_start(0, 3);
            return 0;
        case PLAYER_MODE_JUMP_RISE:
            player_anim_state_update(player, PLAYER_MODE_JUMP_HANG, 1, one_minute * 10);
            return 0;
        case PLAYER_MODE_FISHING:
            player->stateFlags = (player->stateFlags | PLAYER_FLAG_FISHING) & ~PLAYER_FLAG_MODE_ANIM;
            break;
        case PLAYER_MODE_USE_HELD_ITEM:
            player_end_action(player);
            player->stateFlags &= ~(PLAYER_FLAG_BIT14 | PLAYER_FLAG_SHOWING_ITEM | PLAYER_FLAG_CARRY_ITEM);
            item_run_use(&player->held[HELD_ITEM]);
            stored_item_release(&player->held[HELD_ITEM].id);
            player_reset_pose();
            return 0;
        case PLAYER_MODE_SHIP_BASKET:
            if ((player_status_word & 0x80) && estate_basket_merge())
            {
                sound_play(0x13);
                event_flag_set(EVENT_FLAG_ITEM_SHIPPED, 1);
            }
            player->stateFlags &= ~PLAYER_FLAG_BIT14;
            player_end_action(player);
            player_reset_pose();
            return 0;
        case PLAYER_MODE_JUMP_LAUNCH:
            player_end_action(player);
            event_pending_pair_lookup(ITEM_SPINACH_SEEDS, 0, 0, 1);
            player->facing38 = (player->facing38 + 2) & 3;
            player_walk_start(20);
            return 0;
        case PLAYER_MODE_GO_TO_BED:
            if (player->secondSprite.animation == 3)
            {
                farm_area.mode.eventMode = FARM_MODE_SLEEP;
                fade_start_out(30, 0, 2);
                sound_fade_out(30);
                player->stateFlags |= PLAYER_FLAG_SCENE_CHANGE;
                event_flag_set(EVENT_FLAG_OVERNIGHT, 1);
                return 0;
            }
            if (player->secondSprite.animation == 1)
            {
                event_flag_set(EVENT_FLAG_SLEEPING, 1);
            }
            player_anim_state_update(player, PLAYER_MODE_GO_TO_BED, player->secondSprite.animation + 1, anim_wait_frames[player->secondSprite.animation]);
            return 0;
        default:
            player_end_action(player);
            break;
        }
        return 0;
    }

    charge = 0;
    switch (player->mode)
    {
    case PLAYER_MODE_BASKET_ADD:
        sound_play(9);
        game_state.runs.records[18]->recordFlags |= 0x100;
        record18_bounce_step = 3;
        if (estate_basket_add(player->held[HELD_ITEM].id))
        {
            menu_request_open(3, 0x383);
        }
        player->stateFlags &= ~(PLAYER_FLAG_BIT14 | PLAYER_FLAG_SHOWING_ITEM | PLAYER_FLAG_CARRY_BIG_ITEM | PLAYER_FLAG_CARRY_ITEM);
        player_drop_held_item(player);
        player_reset_pose();
        return 0;
    case PLAYER_MODE_DISMOUNT:
        if (player->stateFlags & PLAYER_FLAG_RIDING_COW)
        {
            farm_place_held(*(s16 *)action_object, &player->place.value, &player->position);
        }
        else
        {
            farm_horse_place(&game_state.animals.horse, &player->place.value, &player->position);
        }
        player->stateFlags &= ~(PLAYER_FLAG_RIDING_COW | PLAYER_FLAG_RIDING);
        player_reset_trail(player);
        player_calc_first_valid(player);
        return 1;
    case PLAYER_MODE_MAKE_CHEESE:
    case PLAYER_MODE_MAKE_YARN:
    case PLAYER_MODE_MAKE_MAYONNAISE:
    case PLAYER_MODE_FEED_CHICKENS:
    case PLAYER_MODE_STOCK_LUMBER:
    case PLAYER_MODE_PLACE_CHICKEN_FEED:
    case PLAYER_MODE_PLACE_FODDER:
    case PLAYER_MODE_ITEM_CHECK:
    case PLAYER_MODE_62:
    case PLAYER_MODE_ADD_CHICKEN_FEED:
    case PLAYER_MODE_ADD_FODDER:
    case PLAYER_MODE_SUBMIT_ITEM:
    case PLAYER_MODE_69:
    case PLAYER_MODE_DELIVER_LUMBER:
    case PLAYER_MODE_REQUEST_ITEM:
    case PLAYER_MODE_GIFT:
        tool = player->held[HELD_ITEM].id;
        if (player->mode == PLAYER_MODE_ITEM_CHECK)
        {
            sound_play(0x13);
        }
        if (player->mode == PLAYER_MODE_62)
        {
            player_facing_point(player, &timing, 4);
            estate_marker_stamp(ITEM_BASKET, timing, player->place.location, 0);
            sound_play(7);
        }
        if (player->mode == PLAYER_MODE_GIFT)
        {
            sound_play(0x10);
            game_state.player.currentAction = player->held[HELD_ITEM].id;
            if (player->held[HELD_ITEM].id == ITEM_GIFT)
            {
                game_state.player.currentAction = game_state.player.held[HELD_ITEM].modifier;
            }
            event_trigger_mode_set(4);
        }
        player->stateFlags &= ~(PLAYER_FLAG_BIT14 | PLAYER_FLAG_SHOWING_ITEM | PLAYER_FLAG_CARRY_ITEM);
        picture_release_item(player->held[HELD_ITEM].id);
        stored_item_clear(&player->held[HELD_ITEM]);
        if (player->mode == PLAYER_MODE_DELIVER_LUMBER)
        {
            event_flag_set(EVENT_FLAG_45, 1);
            if (++game_state.progress.estate.lumberDeliveries < 2)
            {
                farm_clamp_status(0, -5, 0, 0);
            }
        }
        if (player->mode == PLAYER_MODE_MAKE_CHEESE)
        {
            cheese_item.id = ITEM_CHEESE_S;
            if (tool == ITEM_MILK_M)
            {
                cheese_item.id = ITEM_CHEESE_M;
            }
            if (tool == ITEM_MILK_L)
            {
                cheese_item.id = ITEM_CHEESE_L;
            }
            if (tool == ITEM_MILK_G)
            {
                cheese_item.id = ITEM_CHEESE_G;
            }
            cheese_item.count = 1;
            sound_play(7);
            player_show_item(&cheese_item);
            return 0;
        }
        else if (player->mode == PLAYER_MODE_MAKE_YARN)
        {
            yarn_item.id = ITEM_YARN_BALL_S;
            if (tool == ITEM_WOOL_M)
            {
                yarn_item.id = ITEM_YARN_BALL_M;
            }
            if (tool == ITEM_WOOL_L)
            {
                yarn_item.id = ITEM_YARN_BALL_L;
            }
            if (tool == ITEM_WOOL_G)
            {
                yarn_item.id = ITEM_YARN_G;
            }
            yarn_item.count = 1;
            sound_play(7);
            player_show_item(&yarn_item);
            return 0;
        }
        else if (player->mode == PLAYER_MODE_MAKE_MAYONNAISE)
        {
            mayo_item.id = ITEM_MAYONNAISE_S;
            if (tool == ITEM_GOOD_EGG)
            {
                mayo_item.id = ITEM_MAYONNAISE_M;
            }
            if (tool == ITEM_EXCELLENT_EGG)
            {
                mayo_item.id = ITEM_MAYONNAISE_L;
            }
            if (tool == ITEM_GOLDEN_EGG)
            {
                mayo_item.id = ITEM_MAYONNAISE_G;
            }
            if (tool == ITEM_POPURIS_EGG)
            {
                mayo_item.id = ITEM_MAYONNAISE_L;
            }
            mayo_item.count = 1;
            sound_play(7);
            player_show_item(&mayo_item);
            return 0;
        }
        else if (player->mode == PLAYER_MODE_FEED_CHICKENS)
        {
            estate_chicken_feed_add(1, 0);
        }
        player_reset_pose();
        break;
    case PLAYER_MODE_SELL_COIN:
        estate_gold_change(game_state.itemInfo[ITEM_COIN].sellPrice, 0);
        picture_release_item(player->held[HELD_ITEM].id);
        stored_item_clear(&player->held[HELD_ITEM]);
        player->stateFlags &= ~PLAYER_FLAG_CARRY_ITEM;
        player_reset_pose();
        break;
    case PLAYER_MODE_RUCKSACK_UPGRADE:
        player->rucksackLevel = 1;
        if (player->held[HELD_ITEM].id == ITEM_RUCKSACK_LEVEL_3)
        {
            player->rucksackLevel = 2;
        }
        player->itemSlots = rucksack_item_slots[player->rucksackLevel];
        picture_release_item(player->held[HELD_ITEM].id);
        stored_item_clear(&player->held[HELD_ITEM]);
        player->stateFlags &= ~PLAYER_FLAG_CARRY_ITEM;
        player_reset_pose();
        break;
    case PLAYER_MODE_FISHING:
        player->stateFlags |= PLAYER_FLAG_FISHING;
        break;
    case PLAYER_MODE_USE_FEATHER:
        if (!player_advance_state(player) && (farmTargetFlags & 0x400))
        {
            harvest_helper_set_action(farmTarget, 0xD);
            player_reset_pose();
        }
        stored_item_clear(&player->shown);
        player->stateFlags &= ~PLAYER_FLAG_SHOWING_ITEM;
        break;
    case PLAYER_MODE_EXCHANGE_ITEM:
        player->stateFlags |= PLAYER_FLAG_CARRY_ITEM;
    case PLAYER_MODE_SHOW_TOOL:
        if (player->mode == PLAYER_MODE_SHOW_TOOL)
        {
            player->stateFlags &= ~PLAYER_FLAG_SHOWING_ITEM;
        }
        stored_item_clear(&player->shown);
        player_reset_pose();
        return 0;
    case PLAYER_MODE_THROW_ITEM:
        sound_play(9);
        thrown_item_spawn(&player->position, &player->held[HELD_ITEM], player->facing38);
        player->stateFlags |= PLAYER_FLAG_ITEM_IN_FLIGHT;
    case PLAYER_MODE_DROP_ITEM:
        if (player->mode == PLAYER_MODE_DROP_ITEM)
        {
            sound_play(9);
        }
        player->stateFlags &= ~(PLAYER_FLAG_BIT14 | PLAYER_FLAG_SHOWING_ITEM | PLAYER_FLAG_CARRY_BIG_ITEM | PLAYER_FLAG_CARRY_ITEM);
        if (player->mode == PLAYER_MODE_DROP_ITEM)
        {
            player_drop_held_item(player);
        }
        player_reset_pose();
        return 0;
    case PLAYER_MODE_SELECT_RESULT:
        sound_play(9);
        player_selection_to_result(player);
        player->stateFlags &= ~(PLAYER_FLAG_BIT14 | PLAYER_FLAG_SHOWING_ITEM | PLAYER_FLAG_CARRY_BIG_ITEM | PLAYER_FLAG_CARRY_ITEM);
        player_drop_held_item(player);
        player_reset_pose();
        return 0;
    case PLAYER_MODE_STORE_ITEM:
        if (!item_is_print(player->held[HELD_ITEM].id))
        {
            stored_item_copy(&player->items[player->itemSlots - 1], &player->held[HELD_ITEM]);
            player_compact_items();
        }
        else
        {
            picture_release_item(player->held[HELD_ITEM].id);
        }
        stored_item_clear(&player->held[HELD_ITEM]);
        player->stateFlags &= ~PLAYER_FLAG_CARRY_ITEM;
        player_reset_pose();
        return 0;
    case PLAYER_MODE_0D:
        player->stateFlags |= PLAYER_FLAG_CARRY_ITEM;
    case PLAYER_MODE_06:
    case PLAYER_MODE_0E:
    case PLAYER_MODE_HOLD_OUT_ITEM:
        player_anim_advance(player);
        if (item_is_fish(player->held[HELD_ITEM].id) == 1)
        {
            player_set_mode(player, PLAYER_MODE_THROW_ITEM, player->facing38 + 0x17, -2);
        }
        if (item_is_print(player->held[HELD_ITEM].id) == 1)
        {
            player->stateFlags |= PLAYER_FLAG_CARRY_ITEM;
            player_set_mode(&game_state.player, PLAYER_MODE_CARRY_BIG_ITEM, 0x1B, 0x1E);
        }
        return 0;
    case PLAYER_MODE_CARRY_BIG_ITEM:
        player->stateFlags |= PLAYER_FLAG_CARRY_ITEM;
        player_set_mode(&game_state.player, PLAYER_MODE_STORE_ITEM, game_state.player.facing38 + 0x1D, -2);
        player->stateFlags &= ~PLAYER_FLAG_SHOWING_ITEM;
        return 0;
    case PLAYER_MODE_FALL_ASLEEP:
        player_set_mode(player, PLAYER_MODE_ASLEEP, 0xB1, -2);
        return 0;
    case PLAYER_MODE_SHOW_ITEM:
    case PLAYER_MODE_SHOW_ITEM_2:
        player->stateFlags &= ~PLAYER_FLAG_BIT14;
        player_entry_refresh(player);
        player_refresh_carry_state(player);
        player_reset_pose();
        break;
    case PLAYER_MODE_28:
        stored_item_release(&player->held[HELD_ITEM].id);
        player_reset_pose();
        break;
    case PLAYER_MODE_SWING_TOOL + 2 * TOOL_KIND_SICKLE:
        if (player_move_record_resolve(player) == 1)
        {
            charge = 0;
            aborted = 1;
        }
        else
        {
            charge = 1;
        }
        player_apply_tool_target(player, TARGET_PHASE_HIT);
        break;
    case PLAYER_MODE_SWING_TOOL + 2 * TOOL_KIND_HOE:
        kind = farm_area.mode.data->head.place.value;
        if (kind == ITEM_OCARINA || kind == ITEM_CUCUMBER)
        {
            amount = TOOL_EXP_BASE;
            aborted = mine_dig(player);
            if (aborted)
            {
                amount = TOOL_EXP_COMMIT;
            }
            player_add_tool_exp(player, amount);
            charge = 1;
        }
        else
        {
            if (player_move_record_run(player) == 1)
            {
                charge = 0;
                aborted = 1;
            }
            else
            {
                charge = 1;
            }
        }
        player_apply_tool_target(player, TARGET_PHASE_HIT);
        pad_vibration_start(0, 2);
        break;
    case PLAYER_MODE_SWING_TOOL + 2 * TOOL_KIND_WATERING_CAN:
        if (player_move_record_update(player) == 1)
        {
            charge = 0;
            aborted = 1;
        }
        else
        {
            charge = 1;
        }
        sound_release_voice(player_sound_voice);
        player_sound_voice = SOUND_VOICE_NONE;
        break;
    case PLAYER_MODE_RESULT_STEP:
        if (player_result_step(player) != 1)
        {
            charge = 0;
            aborted = 1;
        }
        else
        {
            charge = 1;
        }
        break;
    case PLAYER_MODE_SWING_TOOL + 2 * TOOL_KIND_HAMMER:
        player_apply_tool_target(player, TARGET_PHASE_HIT);
        player_tool_clear(player);
        pad_vibration_start(0, 2);
        charge = 1;
        break;
    case PLAYER_MODE_SWING_TOOL + 2 * TOOL_KIND_AX:
        player_apply_tool_target(player, TARGET_PHASE_HIT);
        player_tool_clear_alt(player);
        pad_vibration_start(0, 2);
        charge = 1;
        break;
    case PLAYER_MODE_RIDE:
        return 1;
    }

    if (charge == 1)
    {
        if (!aborted)
        {
            player_reset_pose();
        }
        column = player->toolCharge / TOOL_CHARGE_STEP;
        row = (s16)(player->held[HELD_TOOL].id - 1) / 5;
        stamina = tool_stamina_costs[row][column];
        farm_clamp_status(stamina, player_fatigue_cost(0), 0, 0);
    }
    if (aborted == 1)
    {
        return 0;
    }
    if (player->scriptSteps != 0)
    {
        step = player->scriptSteps += 3;
        if (step[0] == 9999)
        {
            player_reset_pose();
            player->scriptSteps = 0;
        }
        else
        {
            if (step[2] != 0)
            {
                player->stateFlags |= PLAYER_FLAG_ACTION_START;
            }
            player->latches &= ~PLAYER_LATCH_SPRITE_REQUEST;
            player_set_mode(player, player->scriptSteps[0], player->scriptSteps[1], -2);
            return 0;
        }
    }
    return 1;
}

/* Use up the held item, then bring back the item set aside when in PLAYER_MODE_SHOW_ITEM_2,
 * or clear the carry flags. */
static void player_entry_refresh(FarmPlayer *player)
{
    item_run_use(&player->held[HELD_ITEM]);
    if (player->held[HELD_ITEM].id == ITEM_MYSTIC_BERRY)
        player->stateFlags |= PLAYER_FLAG_HALF_FATIGUE;
    stored_item_release(&player->held[HELD_ITEM]);
    if (player->mode == PLAYER_MODE_SHOW_ITEM_2)
    {
        player->held[HELD_ITEM] = player_stored_item;
        picture_load_to_vram(player->held[HELD_ITEM].id);
        player->stateFlags = (player->stateFlags & ~PLAYER_FLAG_BIT14) | PLAYER_FLAG_SHOWING_ITEM;
        if (player->held[HELD_ITEM].id == 0)
        {
            player->stateFlags &= ~PLAYER_FLAG_CARRY_ITEM;
            player->stateFlags &= ~PLAYER_FLAG_SHOWING_ITEM;
        }
    }
    else
    {
        player->stateFlags &= ~PLAYER_FLAG_CARRY_ITEM;
        player->stateFlags &= ~PLAYER_FLAG_SHOWING_ITEM;
    }
}

/* Put the shown item in hand and start its carry animation. A coin and a heavy item
 * start their own action instead; returns 1 when one did. */
static s32 player_anim_advance(FarmPlayer *player)
{
    s32 state;

    if (player->mode == PLAYER_MODE_0E)
    {
        player->stateFlags |= PLAYER_FLAG_CARRY_BIG_ITEM;
    }
    else
    {
        player->stateFlags |= PLAYER_FLAG_CARRY_ITEM;
    }
    player->stateFlags &= ~PLAYER_FLAG_REACHING;

    if (player->mode == PLAYER_MODE_HOLD_OUT_ITEM)
    {
        player_set_mode(player, PLAYER_MODE_SHOW_ITEM_2, 5, -2);
        sound_play(0x4A);
        player->stateFlags |= PLAYER_FLAG_SHOWING_ITEM;
        return 1;
    }

    state = item_picture_row(player->shown.id);
    if (state == PICTURE_ROW_OTHER)
    {
        state = HELD_ITEM;
    }
    stored_item_copy(&player->held[state], &player->shown);
    stored_item_clear(&player->shown);
    player->stateFlags |= PLAYER_FLAG_SHOWING_ITEM;

    if (player->held[HELD_ITEM].id == ITEM_COIN)
    {
        sound_play(8);
        player->stateFlags &= ~(PLAYER_FLAG_SHOWING_ITEM | PLAYER_FLAG_CARRY_ITEM);
        player_set_mode(player, PLAYER_MODE_SELL_COIN, player->facing38 + 0x1D, -2);
        return 1;
    }
    if (player_start_heavy_carry(player) != 0)
    {
        return 1;
    }
    player_reset_pose();
    return 0;
}

static s32 player_start_heavy_carry(FarmPlayer *player)
{
    s16 index = player->held[HELD_ITEM].id;
    if ((game_state.itemInfo[index].itemFlags & 0x1200) != 0x1200)
        return 0;
    player_set_mode(player, PLAYER_MODE_SHOW_ITEM, 5, -2);
    sound_play(0x4A);
    return 1;
}

/* Advance one step of the tool-use animation: play its sound, set the next mode and animation,
 * and move the charge frame on. */
static void player_anim_position_step(FarmPlayer *player)
{
    s16 position;
    s32 value;
    s32 position_plus;
    position = player->toolCharge / TOOL_CHARGE_STEP;
    value = ((player->mode - 0x16) / 2) * 5;
    position_plus = position + 29;
    value += position_plus;
    player_sound_voice = sound_play(value);
    value = (s16)(player->held[HELD_TOOL].id - (player->held[HELD_TOOL].modifier - position));
    value = player_tool_animation(value) + player->facing38 * 5;
    player_set_mode(player, player->mode + 1, value, -2);
    position++;
    player->secondSprite.frame = position;
}

/* Per-mode update of the player: advance its animation and react to the buttons.
 * Returns 1 when the mode hands off. */
static s32 player_mode_tick(FarmPlayer *player)
{
    u16 first_flags;
    u16 second_flags;

    if (player->stateFlags & PLAYER_FLAG_ITEM_IN_FLIGHT)
    {
        return 0;
    }
    first_flags = farm_buttons_held;
    second_flags = farm_buttons_pressed;
    switch (player->mode)
    {
    case PLAYER_MODE_STEP_SOUND:
        if ((*(u32 *)&player->secondSprite.animation & ~0xFF) == 0x400)
        {
            sound_play(0xC);
        }
        return 0;
    case PLAYER_MODE_JUMP_RISE:
        if (second_flags & PAD_CROSS)
        {
            player->stateFlags = (player->stateFlags | PLAYER_FLAG_ACTION_START) & ~PLAYER_FLAG_AIRBORNE;
            player_set_mode(player, PLAYER_MODE_JUMP_LAUNCH, 0, JUMP_LAUNCH_TIMER);
            player->facing38 = FACING_DOWN;
            anim_select_range(&player->secondSprite, 4);
            player->velocity[1] = JUMP_LAUNCH_SPEED;
            player->secondSprite.frame = player->facing38;
            player->stateFlags = (player->stateFlags | PLAYER_FLAG_AIRBORNE) & ~1;
            player->latches &= ~PLAYER_LATCH_JUMPING;
            sound_play(6);
        }
        goto check_event;
    case PLAYER_MODE_JUMP_HANG:
        if (second_flags & PAD_CROSS)
        {
            player_anim_state_update(player, PLAYER_MODE_JUMP_RISE, 0, one_minute * 10);
        }
    check_event:
        if (game_state.globalFlags & GLOBAL_FLAG_MINUTE_PASSED)
        {
            farm_clamp_status(2, -1, 0, 0);
            return 0;
        }
        break;
    case PLAYER_MODE_SHOW_ITEM:
    case PLAYER_MODE_SHOW_ITEM_2:
        if (player->secondSprite.frame < 2)
        {
            return 0;
        }
        player->stateFlags &= ~PLAYER_FLAG_SHOWING_ITEM;
        break;
    case PLAYER_MODE_ASLEEP:
        if (!(first_flags & 0xF9FF))
        {
            if (!(player->stateFlags & PLAYER_FLAG_SCRIPTED))
            {
                break;
            }
            if (game_state.statusFlags & STATUS_FLAG_ASLEEP)
            {
                return 0;
            }
        }
        player_anim_state_update(player, 0x2E, 2, -2);
        player->stateFlags &= ~PLAYER_FLAG_ASLEEP;
        return 1;
    case PLAYER_MODE_CHARGE_TOOL + 2 * TOOL_KIND_SICKLE:
    case PLAYER_MODE_CHARGE_TOOL + 2 * TOOL_KIND_HOE:
    case PLAYER_MODE_CHARGE_TOOL + 2 * TOOL_KIND_AX:
    case PLAYER_MODE_CHARGE_TOOL + 2 * TOOL_KIND_HAMMER:
    case PLAYER_MODE_CHARGE_TOOL + 2 * TOOL_KIND_WATERING_CAN:
        if (first_flags & 0x80)
        {
            if (player->toolCharge < player->held[HELD_TOOL].modifier * TOOL_CHARGE_STEP)
            {
                player->toolCharge++;
            }
            else
            {
                tool_overcharge_frames++;
            }
            player->secondSprite.frame = player->toolCharge / TOOL_CHARGE_STEP;
            if (tool_overcharge_frames < OVERCHARGE_FRAMES)
            {
                return 0;
            }
            player->stateFlags |= PLAYER_FLAG_ACTION_START;
            player_set_mode(player, PLAYER_MODE_STAMINA_LOW, 0, -2);
            tool_overcharge_frames = 0;
            return 0;
        }
        player_anim_position_step(player);
        return 0;
    case PLAYER_MODE_GO_TO_BED:
        if (player->secondSprite.animation != 1)
        {
            return 0;
        }
        if (player_result_camera_step(player) == 1)
        {
            player->modeState = -2;
        }
        else
        {
            player->modeState = 132;
        }
        break;
    }
    return 0;
}

static void player_anim_carry_item(FarmPlayer *player, u32 *out)
{
    u16 mode = player->mode;
    switch (mode)
    {
    case PLAYER_MODE_IDLE:
        *out = 0x1B;
        break;
    case PLAYER_MODE_POSE_2:
        *out = player->facing38 + 0xF;
        break;
    case PLAYER_MODE_POSE_4:
        *out = player->facing38 + 0x13;
        break;
    }
    anim_select_range(&player->secondSprite, *out);
}

static void player_anim_carry_big_item(FarmPlayer *player, u32 *out)
{
    u16 mode = player->mode;
    switch (mode)
    {
    case PLAYER_MODE_IDLE:
        *out = 0x29;
        break;
    case PLAYER_MODE_POSE_2:
        *out = player->facing38 + 0x2A;
        break;
    case PLAYER_MODE_POSE_4:
        *out = player->facing38 + 0x13;
        break;
    }
    anim_select_range(&player->secondSprite, *out);
}

static void player_anim_reach(FarmPlayer *player, u32 *out)
{
    switch (player->mode)
    {
    case PLAYER_MODE_POSE_2:
        *out = player->facing38 + 0xAC;
        break;
    case PLAYER_MODE_POSE_4:
        player->mode = PLAYER_MODE_POSE_2;
        *out = player->facing38 + 0xAC;
        break;
    default:
        break;
    }
    anim_select_range(&player->secondSprite, *out);
}

/* Select the player's tool animation value and apply it to the sprite. */
static void player_anim_value_select(FarmPlayer *player, u32 *out)
{
    u16 mode;

    mode = player->mode;
    switch (mode)
    {
    case PLAYER_MODE_IDLE:
        *out = 0x36;
        break;
    case PLAYER_MODE_POSE_2:
        *out = player->facing38 + 0x3B;
        break;
    case PLAYER_MODE_POSE_4:
        *out = player->facing38 + 0x3B;
        pad_vibration_start(0, 0);
        break;
    default:
        break;
    }
    anim_select_range(&player->secondSprite, *out);
}

/* Pick the player's animation mode and refresh its animation state. */
static void player_anim_mode_select(FarmPlayer *player, u32 *out)
{
    u32 flags;
    AnimState *animation;

    flags = player->stateFlags;
    if (flags & PLAYER_FLAG_RIDING)
    {
        if (player->mode != PLAYER_MODE_RIDE)
        {
            *out = player->facing38 + 0x3F;
            anim_select_range(&player->secondSprite, *out);
            horse_set_anim_from_tool(player);
        }
    }
    else
    {
        if (flags & (PLAYER_FLAG_CARRY_BIG_ITEM | PLAYER_FLAG_CARRY_ITEM))
        {
            *out = 0x1C;
        }
        else
        {
            *out = 0xA;
        }
        animation = &player->secondSprite;
        anim_select_range(animation, *out);
        if (anim_frame_jumps(animation) != 0)
        {
            player->secondSprite.frame = player->facing38;
        }
    }
}

static void player_set_mode(FarmPlayer *player, s32 mode, s32 animation, s32 state)
{
    u16 old_mode;
    u32 flags;
    u32 masked;
    old_mode = player->mode;
    player->mode = mode;
    player->modeState = state;
    flags = player->stateFlags | PLAYER_FLAG_MODE_ANIM;
    masked = flags & ~PLAYER_FLAG_IDLE_POSE;
    player->stateFlags = masked;
    player->previousMode = old_mode;
    if (masked & PLAYER_FLAG_AIRBORNE)
    {
        anim_select_range(&player->secondSprite, animation);
        player_anim_mode_select(player, (&animation));
        return;
    }
    if (flags & PLAYER_FLAG_ACTION_BANK)
    {
        player_load_tool_anim(player, 1);
        anim_select_range(&player->secondSprite, animation);
        player->stateFlags &= ~PLAYER_FLAG_ACTION_BANK;
        return;
    }
    if (flags & PLAYER_FLAG_ACTION)
    {
        return;
    }
    anim_select_range(&player->secondSprite, animation);
    if (player->stateFlags & PLAYER_FLAG_CARRY_ITEM)
    {
        player_anim_carry_item(player, (&animation));
    }
    if (player->stateFlags & PLAYER_FLAG_CARRY_BIG_ITEM)
    {
        player_anim_carry_big_item(player, (&animation));
    }
    if (!(player->stateFlags & (PLAYER_FLAG_CARRY_BIG_ITEM | PLAYER_FLAG_CARRY_ITEM)))
    {
        if (player->stateFlags & PLAYER_FLAG_REACHING)
        {
            player_anim_reach(player, (&animation));
        }
    }
    if ((player->stateFlags & PLAYER_FLAG_RIDING) && (player->mode != PLAYER_MODE_RIDE))
    {
        player_anim_value_select(player, (&animation));
        if (!(player->stateFlags & PLAYER_FLAG_RIDING_COW))
        {
            horse_set_anim_from_tool(player);
        }
        else
        {
            player_cow_animate(player);
        }
    }
    if (anim_frame_jumps(&player->secondSprite) != 0)
    {
        player->secondSprite.frame = player->facing38;
    }
}

/* Update the player's animation state. The mode timer is set only when the animation starts
 * (anim_frame_jumps). */
static void player_anim_state_update(FarmPlayer *player, s32 mode, s32 animation, s32 state)
{
    AnimState *sprite;
    u16 previous;
    s32 result;

    sprite = &player->secondSprite;
    previous = player->mode;
    player->mode = mode;
    player->stateFlags = (player->stateFlags | PLAYER_FLAG_MODE_ANIM) & ~PLAYER_FLAG_IDLE_POSE;
    player->previousMode = previous;
    anim_select_range(sprite, animation);
    result = anim_frame_jumps(sprite);
    if (result != 0)
    {
        player->modeState = state;
        player->secondSprite.frame = player->facing38;
    }
}

/* Validate a farm command and schedule its corresponding player action. */
static s32 player_command_validate(FarmPlayer *player, s16 *command)
{
    s16 code;
    s32 action;
    s32 value;
    if (((((u16)(*command)) - 1)) < 25U)
    {
        player->toolCharge = 1;
        code = *command;
        tool_overcharge_frames = 0;
        action = player_tool_kind(code);
        value = player_tool_animation(*command) + (player->facing38 * 5);
        if (((((u16)(*command)) - 0x15)) < 5U)
        {
            if (player_status_word & 3)
            {
                player->stateFlags |= PLAYER_FLAG_ACTION_START;
                value = (player->facing38 * 5) + command[1];
                player_set_mode(player, PLAYER_MODE_REFILL_CAN, value, -2);
                return 1;
            }
            if (command[2] == 0)
            {
                return 0;
            }
        }
        player_set_mode(player, PLAYER_MODE_CHARGE_TOOL + action * 2, value, -2);
        return 1;
    }
    return 0;
}

static s32 player_start_command_tool(FarmPlayer *player, s16 *command)
{
    u16 command_value;

    command_value = *command;
    if ((command_value - 0x1A) < 4U)
    {
        if ((s16)command_value != 0x1D)
        {
            player_release_target();
        }
        player->stateFlags |= PLAYER_FLAG_ACTION_START;
        player_set_mode(player, PLAYER_MODE_USE_ANIMAL_TOOL + player_tool_kind(*command), player->facing38, -2);
        sound_play((player->mode + 0xFFBD));
        return 1;
    }
    return 0;
}

static s32 player_start_command_item(FarmPlayer *player, u16 *code)
{
    ItemInfo **table;

    if ((*code - 0x22) < 0x15U)
    {
        table = &game_state.itemInfo;
        player->stateFlags |= PLAYER_FLAG_ACTION_START;
        player_set_mode(player, PLAYER_MODE_PLANT_SEED, (*table)[player->held[HELD_TOOL].id].plantAnimation, -2);
        return 1;
    }
    return 0;
}

static inline void queue_transition(FarmPlayer *player)
{
    stored_item_copy(&player->shown, &player->held[HELD_TOOL]);
    player->stateFlags |= PLAYER_FLAG_SHOWING_ITEM;
}

/* Handle one farm command for the player; returns 1 if the command was taken. */
static s32 player_command_handle(FarmPlayer *player, s16 *command)
{
    switch (*command)
    {
    case ITEM_FISHING_ROD:
    case ITEM_FISHING_POLE:
        player_start_fishing(player);
        player_load_tool_anim(player, 0);
        anim_select_range(&player->secondSprite, player->facing38);
        return 1;
    case ITEM_BLUE_FEATHER:
        player_set_mode(player, PLAYER_MODE_USE_FEATHER, 0x47, 10);
        queue_transition(player);
        return 1;
    case ITEM_ANIMAL_MEDICINE:
        player_release_target();
        player->stateFlags |= PLAYER_FLAG_ACTION_START;
        player_set_mode(player, PLAYER_MODE_USE_MEDICINE, player->facing38, -2);
        queue_transition(player);
        return 1;
    case ITEM_COW_MIRACLE_POTION:
    case ITEM_SHEEP_MIRACLE_POTION:
        player_release_target();
        player->stateFlags |= PLAYER_FLAG_ACTION_START;
        player_set_mode(player, PLAYER_MODE_USE_POTION, player->facing38, -2);
        queue_transition(player);
        return 1;
    case PLAYER_COMMAND_FARM_OVERVIEW:
        player->stateFlags |= PLAYER_FLAG_SCENE_CHANGE;
        farm_overview_start();
        return 1;
    }
    return 0;
}

/* Stop the livestock or horse action that the current target started. */
static void player_release_target(void)
{
    u32 flags;

    flags = farmTargetFlags & 0x206;
    switch (flags)
    {
    case 2:
    case 4:
        if (farmTarget != 0)
            livestock_action_dispatch(farmTarget, 0);
        break;
    case 0x200:
        if (farmTarget != 0)
            horse_set_state(&game_state.animals.horse.actor, 0);
        break;
    }
}

static s32 player_command_step(FarmPlayer *player)
{
    s32 flags;
    u16 input_a;
    u16 input_b;
    s16 *command;
    flags = player->stateFlags;
    if (flags & (PLAYER_FLAG_CARRY_BIG_ITEM | PLAYER_FLAG_CARRY_ITEM))
        return 0;
    if (player->fatigue >= PLAYER_FATIGUE_MAX)
        goto success;
    input_b = *&farm_buttons_pressed;
    input_a = *&farm_buttons_held;
    if (flags & PLAYER_FLAG_ITEM_IN_FLIGHT)
        return 0;
    if (player->toolCharge != 0)
    {
        if (input_a & PAD_SQUARE)
        {
            player->toolCharge = player->toolCharge + 1;
            goto success;
        }
        player->toolCharge = 0;
        return 0;
    }
    goto process;
success:
    return 1;
process:
    if (!(input_b & PAD_SQUARE))
    {
        player->toolCharge = 0;
        return 0;
    }
    command = &player->held[HELD_TOOL].id;
    if (player_command_validate(player, command) != 0)
        goto failed;
    if (player_start_command_tool(player, command) != 0)
        goto failed;
    if (player_start_command_item(player, command) == 0)
        goto check;
failed:
    player->secondSprite.frame = 0;
    goto success;
check:
    if (player_command_handle(player, command) != 0)
        goto success;
    player->toolCharge = 0;
    return 0;
}

static s32 player_try_carry_throw(FarmPlayer *player, s32 request)
{
    if (!(request & PAD_SQUARE))
    {
        return 0;
    }
    if (!(player->stateFlags & PLAYER_FLAG_CARRY_ITEM))
    {
        return 0;
    }
    if (player->held[HELD_ITEM].id != 0)
    {
        if (player_use_held_item(1))
        {
            return 1;
        }
    }
    return 0;
}

static s32 player_try_nearby_action(FarmPlayer *player)
{
    FarmNearbyAction *p = (FarmNearbyAction *)player_start_nearby_actions(player);
    if (p == 0)
        return 0;
    return (p->value ^ 2) != 0;
}

/* Take the current target's item into `result`: an estate pickup, a dog ball or other item from
 * the target, or a field tile's item. Clears the target afterwards. */
static s32 player_collect_target(FarmPlayer *player, FarmStoredItem *result)
{
    s32 status;
    u32 flags;
    player->target = farmTarget;
    player->targetFlags = farmTargetFlags;
    flags = player->targetFlags & 0x8928;
    switch (flags)
    {
    case 0x8000:
        status = item_is_field_object(estate_record_item_id(player->target));
        if (status == 1 && estate_field_pickup(farmTarget, result) == status)
        {
            sound_play(7);
        }
        break;
    case 0x100:
        sound_play(7);
        if (farm_action_begin(player->target) == 2)
        {
            result->id = ITEM_DOG_BALL;
        }
        else
        {
            result->id = *(u16 *)player->target;
        }
        result->modifier = 0;
        result->count = 1;
        break;
    case 8:
    case 0x800:
        sound_play(7);
        result->id = *(u16 *)player->target;
        result->modifier = 0;
        result->count = 1;
        farm_action_begin(player->target);
        break;
    case 0x20:
        sound_play(7);
        status = field_tile_item_stage(player->target);
        result->id = status;
        result->modifier = 0;
        result->count = 1;
        break;
    }
    farmTargetFlags = 0;
    farmTarget = 0;
    return 0;
}

/* Find the farm zone at the probe point and return its id when it is one of the four result
 * kinds; any other zone, or none, gives 0. */
static s32 player_result_code_lookup(FarmGroundProbe *input)
{
    ZonePoint probe;
    FarmZone *zone;
    u16 kind;
    probe_zone = 0;
    probe.x = input->x;
    probe.y = input->y;
    probe.z = input->z;
    zone = farm_zone_at_probe(farm_area.mode.data, &probe);
    probe_zone = zone;
    if (zone != 0)
    {
        kind = zone->id;
        switch (kind)
        {
        case ZONE_EGG_TARGET:
            return ZONE_EGG_TARGET;
        case ZONE_KIND_43:
            return ZONE_KIND_43;
        case ZONE_KIND_42:
            return ZONE_KIND_42;
        case ZONE_GIFT_TARGET:
            return ZONE_GIFT_TARGET;
        default:
            return 0;
        }
    }
    return 0;
}

/* Act on the farm zone in front of the player; returns the action code (0: none). */
static s32 player_act_zone_front(FarmPlayer *player)
{
    ZonePoint probe;
    FarmStoredItem itemEvent;
    FarmStoredItem request_item;
    VECTOR target;
    FarmStoredItem shown_item;
    FarmZone *hit;
    u16 kind;
    s16 found;
    s32 value;
    s32 item;
    s32 taken;
    s16 *reach;

    if (player->stateFlags & PLAYER_FLAG_RIDING)
    {
        return 0;
    }
    reach = facing_reach[player->facing38];
    probe.y = player->position.vy;
    probe.x = player->position.vx + (player->velocity[0] + reach[0]);
    probe.z = player->position.vz + (player->velocity[2] + reach[1]);
    hit = farm_zone_at_probe(farm_area.mode.data, &probe);
    if (hit == 0)
    {
        return 0;
    }
    kind = hit->id;
    player_reset_pose();
    switch (kind)
    {
    case ZONE_POND:
        if (hit->facing != player->facing38)
        {
            return 0;
        }
        if (player->held[HELD_ITEM].id != 0)
        {
            return 0;
        }
        if (game_state.progress.clock.date.season == SEASON_WINTER)
        {
            return 0;
        }
        item = pond_take_fish();
        if (item == 0)
        {
            return 0;
        }
        itemEvent.id = item;
        itemEvent.count = 1;
        player_show_item(&itemEvent);
        return 0x2D;
    case ZONE_OPTIONS:
        if (hit->facing != player->facing38)
        {
            return 0;
        }
        options_menu_open();
        game_state.globalFlags |= (GLOBAL_FLAG_PAUSED | GLOBAL_FLAG_MENU_OPEN);
        return 1;
    case ZONE_STORAGE_4:
        if (hit->facing != player->facing38)
        {
            return 0;
        }
        item_result_begin_state4();
        game_state.globalFlags |= (GLOBAL_FLAG_PAUSED | GLOBAL_FLAG_MENU_OPEN);
        return 0x17;
    case ZONE_PICK_MENU:
        if (hit->facing != player->facing38)
        {
            return 0;
        }
        if (event_flag_get(EVENT_FLAG_REQUEST_BUSY) == 1)
        {
            return 0;
        }
        options_pick_clear();
        game_state.globalFlags |= (GLOBAL_FLAG_PAUSED | GLOBAL_FLAG_MENU_OPEN);
        return 2;
    case ZONE_CALENDAR:
        if (hit->facing != player->facing38)
        {
            return 0;
        }
        farm_calendar_open();
        game_state.globalFlags |= (GLOBAL_FLAG_PAUSED | GLOBAL_FLAG_MENU_OPEN);
        return 8;
    case ZONE_MESSAGE_MACHINE:
        if (hit->facing != player->facing38)
        {
            return 0;
        }
        message_machine_start();
        game_state.globalFlags |= (GLOBAL_FLAG_PAUSED | GLOBAL_FLAG_MENU_OPEN);
        return 3;
    case ZONE_STORAGE_7:
        if (hit->facing != player->facing38)
        {
            return 0;
        }
        item_result_begin_state7();
        game_state.globalFlags |= (GLOBAL_FLAG_PAUSED | GLOBAL_FLAG_MENU_OPEN);
        return 4;
    case ZONE_COOKING_MENU:
        if (hit->facing != player->facing38)
        {
            return 0;
        }
        if (player->held[HELD_ITEM].id != 0)
        {
            return 0;
        }
        cooking_menu_open_choose();
        game_state.globalFlags |= (GLOBAL_FLAG_PAUSED | GLOBAL_FLAG_MENU_OPEN);
        return 5;
    case ZONE_COOKING_RECORDS:
        if (hit->facing != player->facing38)
        {
            return 0;
        }
        if (player->held[HELD_ITEM].id != 0)
        {
            return 0;
        }
        cooking_menu_open_records();
        game_state.globalFlags |= (GLOBAL_FLAG_PAUSED | GLOBAL_FLAG_MENU_OPEN);
        return 0x18;
    case ZONE_REQUEST_BOX:
        if (hit->facing != player->facing38)
        {
            return 0;
        }
        if (player->held[HELD_ITEM].id == 0)
        {
            if (game_state.progress.estate.pendingRequest != 0)
            {
                request_item.id = game_state.progress.estate.pendingRequest;
                request_item.modifier = 0;
                request_item.count = 1;
                player_show_item(&request_item);
                game_state.progress.estate.pendingRequest = 0;
            }
        }
        else if (estate_request_begin(player->held[HELD_ITEM].id) != 0)
        {
            player->stateFlags |= PLAYER_FLAG_BIT14;
            player_anim_state_update(player, PLAYER_MODE_REQUEST_ITEM, player->facing38 + 0x17, -2);
        }
        return 0x14;
    case ZONE_STORAGE_6:
        if (hit->facing != player->facing38)
        {
            return 0;
        }
        item_result_begin_state6();
        game_state.globalFlags |= GLOBAL_FLAG_MENU_OPEN;
        game_state.globalFlags |= GLOBAL_FLAG_PAUSED;
        return 0x16;
    case ZONE_MARRIAGE_MENU:
        if (hit->facing != player->facing38 && hit->facing != FACING_ANY)
        {
            return 0;
        }
        value = hit->param;
        if (event_flag_get(EVENT_FLAG_MARY_GRAY_MARRIED) == 1)
        {
            value = 0x1B0;
        }
        if (event_flag_get(EVENT_FLAG_MARRIED_MARY) == 1)
        {
            value = 0x1B1;
        }
        menu_request_open(2, value);
        return 0x10;
    case ZONE_VALUE_MENU:
        if (hit->facing != player->facing38 && hit->facing != FACING_ANY)
        {
            return 0;
        }
        menu_request_open(2, hit->param);
        return 0x10;
    case ZONE_EVENT_TRIGGER:
        event_trigger_mode_set(7);
        return 0x27;
    case ZONE_FODDER_STORE:
        player_zone_fodder_exchange(player);
        return 9;
    case ZONE_LUMBER_SPOT:
        if (hit->facing != player->facing38)
        {
            return 0;
        }
        if (player->held[HELD_ITEM].id == ITEM_LUMBER || player->held[HELD_ITEM].id == ITEM_WINTER_LUMBER)
        {
            if (farm_area.mode.data->head.scene == AREA_HOUSE2_WINTER)
            {
                player_anim_state_update(player, PLAYER_MODE_DELIVER_LUMBER, player->facing38 + 0x17, -2);
            }
        }
        return 0x12;
    case ZONE_MILK_BOX:
        if (hit->facing == player->facing38)
        {
            player_zone_milk_box_use(player, hit);
        }
        return 0x21;
    case ZONE_SHIPPING_BOX:
        if (hit->facing == player->facing38)
        {
            player_submit_item_action(player, player->held[HELD_ITEM].id, hit->modelIndex);
        }
        return 0x22;
    case ZONE_CHICKEN_FEED_STORE:
        if (hit->facing == player->facing38)
        {
            player_zone_chicken_feed_exchange(player);
        }
        return 0x1A;
    case ZONE_KIND_12:
        if (hit->facing == player->facing38)
        {
            player_zone_corn_use(player, hit);
        }
        return 0xC;
    case ZONE_EGG_BOX:
        if (hit->facing == player->facing38)
        {
            player_zone_egg_box_use(player, hit);
        }
        return 0xE;
    case ZONE_WOOL_BOX:
        if (hit->facing == player->facing38)
        {
            player_zone_wool_box_use(player, hit);
        }
        return 0xF;
    case ZONE_FISH_FOOD_STORE:
        if (hit->facing != player->facing38)
        {
            return 0;
        }
        if (player->held[HELD_ITEM].id != 0)
        {
            return 0xA;
        }
        if (estate_fish_food_take(1) == 0)
        {
            return 0;
        }
        request_item.id = ITEM_FISH_FOOD;
        request_item.modifier = 0;
        request_item.count = 1;
        player_show_item(&request_item);
        return 0xA;
    case ZONE_ENDING_TRACK:
        ending_track_request(hit->param);
        return 0x23;
    case ZONE_DELIVERY:
        probe.x = hit->targetX;
        probe.y = hit->targetY;
        probe.z = hit->targetZ;
        player_deliver_to_villager(player, &probe, hit);
        return 0x24;
    case ZONE_SHOP_COUNTER:
        player_facing_point(player, &target, 1);
        probe.x = target.vx;
        probe.y = target.vy;
        probe.z = target.vz;
        found = farm_record_find_key(&probe, &player->place);
        if (found != 0)
        {
            if (found != 0x177 && player->held[HELD_ITEM].id != 0)
            {
                return 0;
            }
            shop_request_dispatch((s16)shop_key_value_lookup(found));
            player_reset_pose();
        }
        return 0x25;
    case ZONE_DAY_TIER_MENU:
        if (hit->facing == player->facing38)
        {
            menu_request_open(2, calendar_day_tier());
        }
        return 0x1F;
    case ZONE_ITEM_CHECK:
        player_zone_item_check(player);
        return 0x20;
    case ZONE_FEE_DESK:
        shop_request_dispatch(SHOP_DIALOGUE_FEE);
        player_reset_pose();
        return 0x15;
    case ZONE_PENDING_OP:
        if (player->held[HELD_ITEM].id != 0)
        {
            return 6;
        }
        if (hit->facing != player->facing38)
        {
            return 6;
        }
        estate_pending_op_start();
        return 6;
    case ZONE_BARN_FEED_BIN:
        player_zone_place_fodder(player, hit);
        return 0x29;
    case ZONE_COOP_FEED_BIN:
        player_zone_place_chicken_feed(player, hit);
        return 0x28;
    case ZONE_LUMBER_STOCK:
        player_zone_lumber_stock(player);
        return 0xD;
    case ZONE_HONEY:
        if ((u32)(player->facing38 - 1) < 2 && player->held[HELD_ITEM].id == 0)
        {
            taken = estate_flag_take_once();
            if (taken == 1)
            {
                shown_item.id = ITEM_HONEY;
                shown_item.modifier = 0;
                shown_item.count = taken;
                player_show_item(&shown_item);
            }
        }
        return 0x2F;
    case ZONE_TREASURE:
        if (player->held[HELD_ITEM].id == 0)
        {
            if (event_flag_get(hit->storyFlag + 0xA1) == 0)
            {
                shown_item.id = hit->param;
                shown_item.modifier = 0;
                shown_item.count = 1;
                player_show_item(&shown_item);
                event_flag_set(hit->storyFlag + 0xA1, 1);
            }
        }
        return 0x30;
    case ZONE_KIND_49:
        if (hit->facing != player->facing38)
        {
            return 0;
        }
        sound_play(7);
        player_anim_state_update(player, PLAYER_MODE_GIFT, player->facing38 + 0x17, -2);
        return 0x31;
    }
    return 0;
}

static void player_zone_corn_use(FarmPlayer *player, FarmZone *unused)
{
    if (player->held[HELD_ITEM].id == ITEM_CORN)
    {
        player_anim_state_update(player, PLAYER_MODE_FEED_CHICKENS, player->facing38 + 0x17, -2);
        sound_play(0x13);
    }
    else
    {
        menu_request_open(2, 0x201);
    }
}

static void player_zone_wool_box_use(FarmPlayer *player, FarmZone *zone)
{
    if (item_is_wool(player->held[HELD_ITEM].id) != 0)
    {
        if (player->facing38 == zone->facing)
        {
            player_anim_state_update(player, PLAYER_MODE_MAKE_YARN, player->facing38 + 0x17, -2);
        }
    }
    else
    {
        menu_request_open(2, 0x1A4);
    }
}

static void player_zone_egg_box_use(FarmPlayer *player, FarmZone *zone)
{
    if (item_is_egg(player->held[HELD_ITEM].id))
    {
        if (player->facing38 == zone->facing)
        {
            player_anim_state_update(player, PLAYER_MODE_MAKE_MAYONNAISE, player->facing38 + 0x17, -2);
        }
    }
    else
    {
        menu_request_open(2, 0x1a2);
    }
}

static void player_zone_milk_box_use(FarmPlayer *player, FarmZone *zone)
{
    if (item_is_milk(player->held[HELD_ITEM].id))
    {
        if (player->facing38 == zone->facing)
        {
            player_anim_state_update(player, PLAYER_MODE_MAKE_CHEESE, player->facing38 + 0x17, -2);
        }
    }
    else
    {
        menu_request_open(2, 0x1A3);
    }
}

static void player_zone_lumber_stock(FarmPlayer *player)
{
    s16 held_id;
    FarmStoredItem pair;

    held_id = player->held[HELD_ITEM].id;
    if (held_id == ITEM_LUMBER || held_id == ITEM_WINTER_LUMBER)
    {
        player_anim_state_update(player, PLAYER_MODE_STOCK_LUMBER, player->facing38 + 0x17, -2);
        estate_lumber_add(LUMBER_ADD_CARRIED, 1);
        sound_play(7);
        return;
    }
    if (held_id != ITEM_NOTHING)
        return;
    if (estate_lumber_take(1) == 0)
        return;
    sound_play(7);
    pair.id = ITEM_LUMBER;
    pair.count = 1;
    player_show_item(&pair);
}

static void player_zone_place_chicken_feed(FarmPlayer *player, FarmZone *zone)
{
    s16 zone_value;
    s32 accepted;
    u16 object_value;
    if (player->held[HELD_ITEM].id == ITEM_CHICKEN_FEED)
    {
        object_value = player->facing38;
        zone_value = zone->facing;
        accepted = object_value == zone_value;
        if (zone_value == 6)
        {
            if ((player->facing38 == FACING_LEFT) || (player->facing38 == FACING_RIGHT))
            {
                accepted = 1;
            }
        }
        if (zone->facing == 7)
        {
            if ((player->facing38 == FACING_UP) || (player->facing38 == FACING_DOWN))
            {
                accepted = 1;
            }
        }
        if (accepted != 0)
        {
            player->stateFlags |= PLAYER_FLAG_BIT14;
            sound_play(7);
            player_anim_state_update(player, PLAYER_MODE_PLACE_CHICKEN_FEED, player->facing38 + 0x17, -2);
            farm_feed_bin_set(zone->param, 0);
        }
    }
    else
    {
        menu_request_open(2, 0x1E5);
    }
}

static void player_zone_place_fodder(FarmPlayer *player, FarmZone *zone)
{
    s16 zone_value;
    s32 accepted;
    u16 object_value;
    if (player->held[HELD_ITEM].id == ITEM_FODDER)
    {
        object_value = player->facing38;
        zone_value = zone->facing;
        accepted = object_value == zone_value;
        if (zone_value == 6)
        {
            if ((player->facing38 == FACING_LEFT) || (player->facing38 == FACING_RIGHT))
            {
                accepted = 1;
            }
        }
        if (zone->facing == 7)
        {
            if ((player->facing38 == FACING_UP) || (player->facing38 == FACING_DOWN))
            {
                accepted = 1;
            }
        }
        if (accepted != 0)
        {
            player->stateFlags |= PLAYER_FLAG_BIT14;
            sound_play(7);
            player_anim_state_update(player, PLAYER_MODE_PLACE_FODDER, player->facing38 + 0x17, -2);
            farm_feed_bin_set(zone->param, 1);
        }
    }
    else
    {
        menu_request_open(2, 0x1E4);
    }
}

static void player_zone_item_check(FarmPlayer *player)
{
    s16 record_id;
    ItemInfo *record;
    s32 should_act;

    record_id = player->held[HELD_ITEM].id;
    if (record_id == 0)
    {
        menu_request_open(2, 0x20A);
        return;
    }
    record = game_state.itemInfo + record_id;
    should_act = 0;
    if ((record->itemFlags & 0x9000) == 0x9000 || record->targets.word == -1)
    {
        should_act = 1;
    }
    if (should_act != 0)
    {
        player_anim_state_update(player, PLAYER_MODE_ITEM_CHECK, player->facing38 + 0x17, -2);
        sound_play(9);
    }
}

static s32 player_deliver_to_villager(FarmPlayer *player, ZonePoint *point, FarmZone *zone)
{
    FarmVillager *villager;
    s16 zone_kind;
    s16 zone_facing;

    villager = farm_villager_at_point(point, &player->place.value);
    if (villager != 0)
    {
        zone_kind = zone->param;
        if (zone_kind == 0x2E)
        {
            if (villager->id != game_state.family.wifeVillager)
                return 0;
        }
        else if (villager->id != zone_kind)
        {
            return 0;
        }

        if (player->facing38 != zone->facing)
        {
            zone_facing = zone->facing;
            if (zone_facing != FACING_ANY)
                return 0;
        }

        player_reset_pose();
        if (villager->id == CHARACTER_DOUG && farm_area.mode.data->head.place.location == PLACE_1B)
        {
            shop_request_dispatch(SHOP_DIALOGUE_ITEM_16);
            return 1;
        }

        player->stateFlags |= PLAYER_FLAG_SCRIPTED;
        event_trigger_mode_set(3);
        player->talkVillager = villager->id;
        event_flag_set(EVENT_FLAG_DELIVERING_TO_VILLAGER, 1);
        return 1;
    }
    return 0;
}

static void player_zone_fodder_exchange(FarmPlayer *player)
{
    s16 held_id;
    FarmStoredItem pair;

    held_id = player->held[HELD_ITEM].id;
    if (held_id == 0)
    {
        if (estate_fodder_take(1) == 1)
        {
            pair.id = ITEM_FODDER;
            pair.count = 1;
            player_show_item(&pair);
        }
        else
        {
            menu_request_open(2, 0x19C);
        }
    }
    else if (held_id == ITEM_FODDER)
    {
        player->stateFlags &= ~(PLAYER_FLAG_BIT14 | PLAYER_FLAG_SHOWING_ITEM);
        estate_fodder_add(FODDER_ADD_CARRIED, 1);
        sound_play(7);
        player_anim_state_update(player, PLAYER_MODE_ADD_FODDER, player->facing38 + 0x17, -2);
    }
    else
    {
        menu_request_open(2, 0x19C);
    }
}

static void player_zone_chicken_feed_exchange(FarmPlayer *player)
{
    s16 held_id;
    FarmStoredItem pair;

    held_id = player->held[HELD_ITEM].id;
    if (held_id == 0)
    {
        if (estate_chicken_feed_take(1) == 1)
        {
            pair.id = ITEM_CHICKEN_FEED;
            pair.count = 1;
            player_show_item(&pair);
        }
        else
        {
            menu_request_open(2, 0x19F);
        }
    }
    else if (held_id == ITEM_CHICKEN_FEED)
    {
        player->stateFlags &= ~(PLAYER_FLAG_BIT14 | PLAYER_FLAG_SHOWING_ITEM);
        estate_chicken_feed_add(0, 1);
        sound_play(7);
        player_anim_state_update(player, PLAYER_MODE_ADD_CHICKEN_FEED, player->facing38 + 0x17, -2);
    }
    else
    {
        menu_request_open(2, 0x19F);
    }
}

/* Submit the held egg to the selected farm slot; the kind follows the egg (golden 1, Popuri's 2).
 * On failure, show the error message: nothing was selected, or the reason the slot claim gave. */
static s32 player_submit_item_action(FarmPlayer *player, s16 selected, s16 value)
{
    s32 kind;
    s32 error;

    if (selected == 0)
    {
        menu_request_open(2, 0x1E7);
        return 0;
    }
    if (item_is_egg(player->held[HELD_ITEM].id) != 0)
    {
        kind = 0;
        if (player->held[HELD_ITEM].id == ITEM_POPURIS_EGG)
        {
            kind = 2;
        }
        else if (player->held[HELD_ITEM].id == ITEM_GOLDEN_EGG)
        {
            kind = 1;
        }
        error = farm_mode_slot_claim(value, kind);
        if (error != 0)
        {
            menu_request_open(2, error);
            return 0;
        }
    }
    else
    {
        return 0;
    }
    player->stateFlags &= ~(PLAYER_FLAG_BIT14 | PLAYER_FLAG_SHOWING_ITEM);
    player_anim_state_update(player, PLAYER_MODE_SUBMIT_ITEM, player->facing38 + 0x17, -2);
    return 1;
}

/* Start the farm operation that the request flags ask for. */
static s32 player_start_request(FarmPlayer *player, u16 flags)
{
    if (farm_area.mode.eventMode != FARM_MODE_IDLE)
    {
        return 1;
    }
    if (game_state.globalFlags & GLOBAL_FLAG_MENU_OPEN)
    {
        return 0;
    }
    if (flags & PAD_SELECT)
    {
        photo_viewer_start();
        game_state.globalFlags |= GLOBAL_FLAG_MENU_OPEN;
        game_state.globalFlags |= GLOBAL_FLAG_PAUSED;
        return 1;
    }
    if (flags & PAD_START)
    {
        status_page_open();
        game_state.globalFlags |= GLOBAL_FLAG_MENU_OPEN;
        game_state.globalFlags |= GLOBAL_FLAG_PAUSED;
        return 1;
    }
    if (player->stateFlags & (PLAYER_FLAG_AUTO_WALK | PLAYER_FLAG_RIDING | PLAYER_FLAG_AIRBORNE))
    {
        return 0;
    }
    if (flags & PAD_TRIANGLE)
    {
        storage_swap_begin();
        game_state.globalFlags |= GLOBAL_FLAG_MENU_OPEN;
        game_state.globalFlags |= GLOBAL_FLAG_PAUSED;
        return 1;
    }
    if (flags & PAD_CROSS)
    {
        if (farm_zone_find(&farm_area.mode.script, &player->position, 1) != 0)
        {
            return 1;
        }
        return player_act_zone_front(player);
    }
    return 0;
}

/* Take the item at the target into the player's hands, if there is one. Returns 1 when it did. */
static s32 player_collect_target_item(FarmPlayer *player)
{
    FarmStoredItem item;
    s32 id;
    item.id = 0;
    player_collect_target(player, &item);
    id = item.id;
    if (id == 0)
    {
        return 0;
    }
    player_show_item(&item);
    player->stateFlags &= ~PLAYER_FLAG_REACHING;
    return 1;
}

static s32 player_handle_pickup(FarmPlayer *player, s32 buttons)
{
    if (buttons & PAD_CROSS)
    {
        if (!(player->stateFlags & (PLAYER_FLAG_CARRY_BIG_ITEM | PLAYER_FLAG_CARRY_ITEM)))
        {
            if (farmTargetFlags & 0x2000)
            {
                if (player_collect_target_item(player))
                    return 1;
            }
        }
        else
        {
            if (player_use_held_item(0))
                return 1;
        }
    }
    return 0;
}

/* Put the first stored tool in hand, when the hand is empty or holds an item of the negative
 * class, and show it. */
static void player_check_tool_slots(FarmPlayer *player, s32 direction)
{
    s32 ready;
    FarmStoredItem *tool;
    u32 slot;
    FarmStoredItem *value;
    ready = 1;
    slot = 0;
    value = player->tools;
    do
    {
        if (value->id != 0)
        {
            ready = 0;
        }
        slot += 1;
        value++;
    } while (slot < PLAYER_BAG_SLOTS);
    if ((ready == 0) || (player->held[HELD_TOOL].id != 0))
    {
        tool = &player->held[HELD_TOOL];
        if (((player->held[HELD_TOOL].id) == 0) || (item_class_word_negative(player->held[HELD_TOOL].id) == 1))
        {
            player->stateFlags &= ~(PLAYER_FLAG_CARRY_BIG_ITEM | PLAYER_FLAG_CARRY_ITEM);
            player_compact_tools();
            if (player_exchange_tool(0) != (-1))
            {
                if (tool->id != 0)
                {
                    player_anim_state_update(player, PLAYER_MODE_SHOW_TOOL, 0x47, 10);
                    stored_item_copy(&player->shown, tool);
                    player->stateFlags |= PLAYER_FLAG_SHOWING_ITEM;
                    sound_play(7);
                }
            }
        }
    }
}

/* Carry a big held item on, or, with an empty hand, bring the first stored item into it and
 * show it. */
static void player_pending_transition(FarmPlayer *player, s32 direction)
{
    FarmStoredItem *slot;
    FarmStoredItem *state_ptr;
    s32 all_clear;
    u32 count;
    u16 state;
    s16 signed_state;
    all_clear = 1;
    count = 0;
    slot = player->items;
    do
    {
        if (slot->id != 0)
        {
            all_clear = 0;
        }
        count++;
        slot++;
    } while (count < PLAYER_BAG_SLOTS);
    state = player->held[HELD_ITEM].id;
    if (all_clear != 0)
    {
        if (state == 0)
        {
            return;
        }
    }
    signed_state = state;
    state_ptr = &player->held[HELD_ITEM];
    if (signed_state != 0)
    {
        if (item_class_word_negative(signed_state) == 1)
        {
            player_carry_tick();
        }
    }
    else
    {
        player->stateFlags &= ~(PLAYER_FLAG_CARRY_BIG_ITEM | PLAYER_FLAG_CARRY_ITEM);
        player_compact_items();
        if (player_exchange_item(0) != -1 && player->held[HELD_ITEM].id != 0)
        {
            player_anim_state_update(player, 0xA, player->facing38 + 0xB, -2);
            stored_item_copy(&player->shown, state_ptr);
            player->stateFlags |= PLAYER_FLAG_SHOWING_ITEM;
            sound_play(7);
        }
    }
}

/* Advance the player's state when the targeted object allows it. */
static s32 player_advance_state(FarmPlayer *player)
{
    FarmObjectStatus *status;
    u16 mode;
    if (farmTargetFlags & 0x80)
    {
        status = farmTarget;
        mode = status->mode;
        if ((mode & 3) == 0)
        {
            if (player->mode == PLAYER_MODE_USE_FEATHER)
            {
                if (event_flag_get(EVENT_FLAG_REQUEST_BUSY) == 0)
                {
                    event_trigger_mode_set(9);
                    player->talkVillager = status->talkVillager;
                    player->stateFlags |= PLAYER_FLAG_SCRIPTED;
                }
            }
            else
            {
                if ((mode & 0x1000) == 0)
                    player->stateFlags |= PLAYER_FLAG_SCRIPTED;
                event_trigger_mode_set(3);
                player->talkVillager = status->talkVillager;
                player_reset_pose();
                return 1;
            }
        }
    }
    return 0;
}

/* Confirm-button handler: start the action the current target or held item asks for;
 * returns 1 when one was started. */
static s32 player_confirm_action(FarmPlayer *player, s32 buttons)
{
    s32 item;
    s32 sound;

    if (!(buttons & PAD_CROSS))
    {
        return 0;
    }
    if (((s32 (*)())player_advance_state)())
    {
        return 1;
    }
    if (farmTargetFlags & 0x400)
    {
        if (!(game_state.statusFlags & STATUS_FLAG_NO_TARGET_ACTION))
        {
            harvest_helper_set_action(farmTarget, 6);
            player_reset_pose();
            return 1;
        }
        player->talkVillager = ((FarmObjectRecord *)farmTarget)->talkVillager;
        player_reset_pose();
    }
    if (farmTargetFlags & 0x200)
    {
        if (!(player->stateFlags & (PLAYER_FLAG_CARRY_BIG_ITEM | PLAYER_FLAG_CARRY_ITEM)))
        {
            if (((FarmObjectRecord *)farmTarget)->item == ITEM_HORSE)
            {
                game_state.animals.horse.actor.flags |= 0x20;
                sound_play(6);
                player->facing38 = game_state.animals.horse.actor.facing;
                player_set_mode(player, PLAYER_MODE_RIDE, player->facing38 + 0x32, -2);
                player->stateFlags |= PLAYER_FLAG_RIDING;
            }
            else if (((FarmObjectRecord *)farmTarget)->item == ITEM_PONY)
            {
                horse_set_state(&game_state.animals.horse.actor, 5);
            }
            return 1;
        }
        return ((FarmObjectRecord *)farmTarget)->item != ITEM_HORSE;
    }
    if ((farmTargetFlags & 2) && player->cowReins != 0)
    {
        action_object = farmTarget;
        sound_play(6);
        player->facing38 = ((FarmObjectRecord *)action_object)->facing;
        player_set_mode(player, PLAYER_MODE_RIDE, player->facing38 + 0x32, -2);
        player->stateFlags |= (PLAYER_FLAG_RIDING_COW | PLAYER_FLAG_RIDING);
        player->stateFlags &= ~(PLAYER_FLAG_POSE_RESET_PENDING | PLAYER_FLAG_REACHING);
        farm_action_begin(action_object);
        return 1;
    }
    sound = -1;
    if (farmTargetFlags & 0x8020)
    {
        item = -1;
        if (farmTargetFlags & 0x8000)
        {
            item = estate_record_item_id(farmTarget);
        }
        if (farmTargetFlags & 0x20)
        {
            item = (s16)((FarmObjectRecord *)farmTarget)->item;
        }
        switch (item)
        {
        case ITEM_BIG_STONE:
        case FIELD_TILE_BIG_STONE_1:
        case FIELD_TILE_BIG_STONE_2:
        case FIELD_TILE_BIG_STONE_3:
            sound = 0x25F;
            break;
        case ITEM_ROCK:
        case FIELD_TILE_ROCK_1:
        case FIELD_TILE_ROCK_2:
        case FIELD_TILE_ROCK_3:
            sound = 0x260;
            break;
        case ITEM_STUMP:
        case FIELD_TILE_STUMP_1:
        case FIELD_TILE_STUMP_2:
        case FIELD_TILE_STUMP_3:
            if (farm_area.mode.place.location != PLACE_E)
            {
                sound = 0x25E;
            }
            else if (event_flag_get(EVENT_FLAG_D7) == 1)
            {
                sound = 0x283;
            }
            else
            {
                sound = 0x282;
            }
            break;
        }
        if (sound != -1)
        {
            menu_request_open(2, sound);
            return 1;
        }
    }
    player_apply_tool_target(player, TARGET_PHASE_CONFIRM);
    return 0;
}

/* Leave the ground for a jump: crouch, face up and take off. */
static void player_start_jump(FarmPlayer *player)
{
    player->stateFlags |= PLAYER_FLAG_ACTION_START;
    player_set_mode(player, PLAYER_MODE_JUMP_CROUCH, 0, 20);
    player->position.vx = -8838;
    anim_select_range(&player->secondSprite, 4);
    player->facing38 = FACING_UP;
    player->secondSprite.frame = player->facing38;
    player->velocity[1] = -140;
    player->stateFlags |= PLAYER_FLAG_AIRBORNE;
    sound_play(6);
}

/* The player's input step. Returns 1 when an action started, 2 for a tool/item swap,
 * 0 otherwise. */
static s32 player_input_step(void)
{
    FarmPlayer *player = &game_state.player;
    u16 pressed = farm_buttons_pressed;
    u16 buttons_held = farm_buttons_held;
    s32 direction;
    u32 limit;

    if (farm_area.mode.place.value == PLACE_MINE || farm_area.mode.place.value == PLACE_MINE_43)
    {
        if (mine_check_player(player) != 0)
        {
            return 1;
        }
    }
    if (!(player->stateFlags & (PLAYER_FLAG_NO_INPUT | PLAYER_FLAG_AUTO_WALK)))
    {
        if ((player->stateFlags & PLAYER_FLAG_RIDING) && (pressed & PAD_CROSS))
        {
            player_set_mode(player, PLAYER_MODE_DISMOUNT, 0xB0, -2);
            return 1;
        }
        if (player->stateFlags & PLAYER_FLAG_ITEM_IN_FLIGHT)
        {
            return 1;
        }
        if (player->latches & PLAYER_LATCH_JUMP_PENDING)
        {
            player_start_jump(player);
            player->latches &= ~PLAYER_LATCH_JUMP_PENDING;
            return 1;
        }
        if (player->latches & PLAYER_LATCH_DOOR_ENTRY)
        {
            if (door_entry_for_event(0x20) == 1)
            {
                player->latches &= ~PLAYER_LATCH_DOOR_ENTRY;
                player->stateFlags |= PLAYER_FLAG_SCENE_CHANGE;
                photo_sequence_start();
            }
            return 1;
        }
        if (player_start_request(player, pressed) != 0)
        {
            return 1;
        }
        if (!(player->stateFlags & (PLAYER_FLAG_AUTO_WALK | PLAYER_FLAG_RIDING | PLAYER_FLAG_AIRBORNE)))
        {
            if ((game_state.globalFlags & GLOBAL_FLAG_HOUR_PASSED) && game_state.progress.clock.time.hm.hour == CLOCK_NOON_HOUR)
            {
                return 0;
            }
            if (player_update_condition_flags(player, 0) != 0)
            {
                return 0;
            }
            if (player_confirm_action(player, pressed) != 0)
            {
                return 1;
            }
            if (pressed & PAD_R1)
            {
                if (player->held[HELD_ITEM].id == 0)
                {
                    player_check_tool_slots(player, 0);
                }
                return 2;
            }
            if (pressed & PAD_R2)
            {
                player_pending_transition(player, 0);
                return 2;
            }
            if (!(player->stateFlags & (PLAYER_FLAG_CARRY_BIG_ITEM | PLAYER_FLAG_CARRY_ITEM)))
            {
                if (buttons_held & PAD_R1)
                {
                    direction = 2;
                    if (pressed & PAD_LEFT)
                    {
                        direction = 1;
                    }
                    if (pressed & PAD_RIGHT)
                    {
                        direction = 0;
                    }
                    if (direction != 2)
                    {
                        player_check_tool_slots(player, direction);
                        return 2;
                    }
                }
                if (buttons_held & PAD_R2)
                {
                    direction = 2;
                    if (pressed & PAD_LEFT)
                    {
                        direction = 1;
                    }
                    if (pressed & PAD_RIGHT)
                    {
                        direction = 0;
                    }
                    if (direction != 2)
                    {
                        player_pending_transition(player, direction);
                        return 2;
                    }
                }
                if (pressed & PAD_L1)
                {
                    player_set_mode(player, 0x14, player->facing38 + 0x43, -2);
                    player_try_nearby_action(player);
                    farm_mode_check_start(1);
                    sound_play(0x12);
                    return 1;
                }
                if (pressed & PAD_L2)
                {
                    player_set_mode(player, 0x14, player->facing38 + 0x43, -2);
                    player_try_nearby_action(player);
                    farm_mode_check_start(2);
                    sound_play(0x12);
                    return 1;
                }
            }
            if (player_handle_pickup(player, pressed) != 0)
            {
                return 1;
            }
            if (player_command_step(player) != 0)
            {
                return 0;
            }
            if (player_try_carry_throw(player, pressed) != 0)
            {
                return 1;
            }
            if (player_start_sleep(player) != 0)
            {
                return 1;
            }
        }
    }
    if (player->stateFlags & PLAYER_FLAG_AIRBORNE)
    {
        return 0;
    }
    if (player_update_mode() != 0)
    {
        limit = STEP_SOUND_INTERVAL;
        if (player->mode == PLAYER_MODE_POSE_4)
        {
            limit = STEP_SOUND_INTERVAL_POSE_4;
        }
        if (limit < step_sound_timer)
        {
            player_sound_voice = sound_play(0x48);
            step_sound_timer = 0;
        }
        else
        {
            step_sound_timer++;
        }
        return 1;
    }
    return 0;
}

static s32 player_start_sleep(FarmPlayer *player)
{
    s32 flags;

    if ((game_state.globalFlags & GLOBAL_FLAG_PAUSED) == 0 && fade_is_settled() != 0)
    {
        flags = player->stateFlags;
        if ((flags & (PLAYER_FLAG_CARRY_BIG_ITEM | PLAYER_FLAG_CARRY_ITEM)) == 0 && player->idleFrames >= PLAYER_SLEEP_IDLE_FRAMES && (flags & PLAYER_FLAG_ASLEEP) == 0)
        {
            player->stateFlags = flags | PLAYER_FLAG_ACTION_START;
            player_set_mode(player, PLAYER_MODE_LIE_DOWN, 0, -2);
            player->stateFlags |= PLAYER_FLAG_ASLEEP;
            return 1;
        }
    }
    return 0;
}

/* The facing for the held direction buttons: a single direction or a diagonal pair sets it,
 * anything else keeps `current`. */
static s32 player_dpad_facing(u16 buttons, s32 current)
{
    s32 facing;

    facing = current & 0xFFFF;
    switch (buttons)
    {
    case PAD_DOWN:
    case PAD_DOWN | PAD_LEFT:
        facing = FACING_DOWN;
        break;
    case PAD_LEFT:
    case PAD_UP | PAD_LEFT:
        facing = FACING_LEFT;
        break;
    case PAD_UP:
    case PAD_UP | PAD_RIGHT:
        facing = FACING_UP;
        break;
    case PAD_RIGHT:
    case PAD_DOWN | PAD_RIGHT:
        facing = FACING_RIGHT;
        break;
    default:
        break;
    }
    return facing;
}

static void player_add_move_velocity(FarmPlayer *player)
{
    u16 mode;
    s16 *entry;
    u16 index;
    mode = player->mode;
    entry = move_steps_small;
    if (mode == PLAYER_MODE_POSE_2)
        entry = move_steps_small + 8;
    if (mode == PLAYER_MODE_POSE_4)
        entry = move_steps_large;
    if ((player->stateFlags & PLAYER_FLAG_RIDING) != 0)
    {
        if (mode == PLAYER_MODE_POSE_2)
            entry = move_steps_large;
        else
            entry = move_steps_riding;
    }
    index = player->facing38;
    entry += index * 2;
    player->velocity[0] += entry[0];
    player->velocity[2] += entry[1];
}

/* Update the player's mode, animation and facing from the held direction buttons. */
static s32 player_update_mode(void)
{
    u16 raw_flags;
    u16 previous;
    s32 state;
    FarmPlayer *object;
    state = 0;
    raw_flags = *&farm_buttons_held;
    object = &game_state.player;
    previous = object->facing38;
    if (raw_flags & PAD_CIRCLE)
    {
        state = 1;
        if ((&game_state.player)->mode == PLAYER_MODE_POSE_4)
        {
            state = 2;
        }
    }
    raw_flags = raw_flags & PAD_ARROWS_MASK;
    player_set_facing(object->facing38);
    object->facing38 = player_dpad_facing(raw_flags, object->facing38);
    if ((object->stateFlags & PLAYER_FLAG_REACHING) && (state != 0))
    {
        state = 0;
        raw_flags = 1;
    }
    if (raw_flags == 0)
    {
        if (state != 0)
        {
            if (object->facing38 != previous || state == 1)
            {
                player_set_mode(object, PLAYER_MODE_POSE_4, game_state.player.facing38 + 6, -2);
            }
            object->stateFlags &= ~PLAYER_FLAG_MODE_ANIM;
            player_add_move_velocity(&game_state.player);
            return 1;
        }
        player_set_mode(object, PLAYER_MODE_IDLE, 0, -2);
        object->stateFlags = (object->stateFlags & ~PLAYER_FLAG_MODE_ANIM) | PLAYER_FLAG_IDLE_POSE;
        return 0;
    }
    player_add_move_velocity(&game_state.player);
    if (state != 0)
    {
        if ((object->facing38 != previous) || (object->mode != PLAYER_MODE_POSE_4))
        {
            player_set_mode(object, PLAYER_MODE_POSE_4, (&game_state.player)->facing38 + 6, -2);
            object->stateFlags &= ~PLAYER_FLAG_MODE_ANIM;
        }
    }
    else if ((object->facing38 != previous) || (object->mode != PLAYER_MODE_POSE_2))
    {
        player_set_mode(object, PLAYER_MODE_POSE_2, (&game_state.player)->facing38 + 1, -2);
        object->stateFlags &= ~PLAYER_FLAG_MODE_ANIM;
    }
    return 1;
}

/* Load the pictures of the held, tool and item slots into VRAM. */
void player_load_item_pictures(void)
{
    FarmPlayer *base;
    FarmStoredItem *entry;
    FarmStoredItem *tools;
    u32 count;

    count = 0;
    base = &game_state.player;
    entry = base->held;
    do
    {
        picture_load_to_vram(entry->id);
        entry++;
        count++;
    } while (count < 2U);
    tools = base->tools;
    count = 0;
    entry = base->items;
    do
    {
        picture_load_to_vram(entry->id);
        entry++;
        count++;
    } while (count < PLAYER_BAG_SLOTS);
    count = 0;
    entry = tools;
    do
    {
        picture_load_to_vram(entry->id);
        entry++;
        count++;
    } while (count < PLAYER_BAG_SLOTS);
}

/* Reset the day's targets and timers. On the demo start, give the starting tools and rucksack. */
void player_new_day_reset(void)
{
    FarmPlayer *farm;

    farmTargetFlags = 0;
    farmTarget = 0;
    farm_buttons_pressed = farm_buttons_held = 0;
    farm = &game_state.player;
    if (game_state.globalFlags & GLOBAL_FLAG_DEMO_START)
    {
        farm->tools[0].id = ITEM_SILVER_SICKLE;
        farm->tools[0].modifier = 2;
        farm->tools[1].id = ITEM_SILVER_HOE;
        farm->tools[1].modifier = 2;
        farm->tools[2].id = ITEM_SILVER_AX;
        farm->tools[2].modifier = 2;
        farm->tools[3].id = ITEM_SILVER_HAMMER;
        farm->tools[3].modifier = 2;
        farm->tools[4].id = ITEM_SILVER_WATERING_CAN;
        farm->tools[4].modifier = 2;
        farm->tools[4].count = 100;
        farm->tools[5].id = ITEM_TURNIP_SEEDS;
        farm->tools[5].count = 10;
        farm->rucksackLevel = 2;
        farm->itemSlots = PLAYER_BAG_SLOTS;
    }
    player_load_item_pictures();
    house_door_position(&farm->position);
    idle_frames = 0;
    blocked_frames = 0;
    tool_overcharge_frames = 0;
}

/* Stop the player's motion: no airborne state and no velocity. */
void player_stop_motion(void)
{
    game_state.player.stateFlags &= ~PLAYER_FLAG_AIRBORNE;
    game_state.player.velocity[0] = 0;
    game_state.player.velocity[1] = 0;
    game_state.player.velocity[2] = 0;
}

static s32 player_location_kind(void)
{
    s16 x;
    s32 r;
    x = farm_area.mode.place.location;
    r = (x != PLACE_FARM) * 2;
    if (x == PLACE_GREENHOUSE)
        r = 1;
    return r;
}

static void player_compact_items(void)
{
    stored_item_table_compact(game_state.player.items, PLAYER_BAG_SLOTS);
}

static void player_compact_tools(void)
{
    stored_item_table_compact(game_state.player.tools, PLAYER_BAG_SLOTS);
}

static s32 player_exchange_item(s32 mode)
{
    if (mode == 0)
    {
        stored_item_table_exchange(game_state.player.items, PLAYER_BAG_SLOTS, &game_state.player.held[HELD_ITEM], 0);
    }
    else
    {
        stored_item_table_exchange(game_state.player.items, PLAYER_BAG_SLOTS, &game_state.player.held[HELD_ITEM], 1);
    }
    return 1;
}

static s32 player_exchange_tool(s32 mode)
{
    if (mode == 0)
    {
        stored_item_table_exchange(game_state.player.tools, PLAYER_BAG_SLOTS, &game_state.player.held[HELD_TOOL], 0);
    }
    else
    {
        stored_item_table_exchange(game_state.player.tools, PLAYER_BAG_SLOTS, &game_state.player.held[HELD_TOOL], 1);
    }
    return 1;
}

/* Draw the farm cursor at (pos_x, pos_y). */
void farm_draw_cursor(u_long *ot, s32 pos_x, s32 pos_y, s32 frame)
{
    s32 x;
    s32 y;
    s32 anim_id;

    anim_id = 0x18;
    anim_select_range(&ui_icon_anim, anim_id);
    x = pos_x;
    y = pos_y;
    ui_icon_anim.frame = 0;
    anim_draw_sprt_cursor(ot, &ui_icon_anim, anim_id, frame & 0xFF, x, y);
}

/* Draw the item the player holds or shows, at the screen position moved by the offset
 * for its facing. The offset group depends on the pose and the tool. */
static void player_draw_carried_item(FarmPlayer *player, s32 depth, FarmScreenXY *pos)
{
    u32 flags;
    s16 key;
    s32 group;
    s32 dir;
    s16 *offset;

    flags = player->stateFlags;
    if (flags & PLAYER_FLAG_BIT14)
    {
        return;
    }
    if (!(flags & PLAYER_FLAG_SHOWING_ITEM))
    {
        return;
    }
    key = player->held[HELD_ITEM].id;
    group = 0;
    if (!(flags & (PLAYER_FLAG_CARRY_BIG_ITEM | PLAYER_FLAG_CARRY_ITEM)))
    {
        key = player->shown.id;
    }
    if (flags & PLAYER_FLAG_CARRY_ITEM)
    {
        group = 8;
    }
    if (player->mode == PLAYER_MODE_POSE_4)
    {
        group = 16;
    }
    if (player->mode == PLAYER_MODE_SHOW_ITEM || player->mode == PLAYER_MODE_SHOW_ITEM_2)
    {
        group = 24;
    }
    dir = player->facing38;
    if (player->mode == PLAYER_MODE_SHOW_TOOL || player->mode == PLAYER_MODE_USE_FEATHER)
    {
        dir = 0;
    }
    offset = (s16 *)mark_screen_offsets + (dir * 2 + group);
    picture_cell_build_prim(key, 0, &current_frame->ot[depth >> 2],
                            pos->x + offset[0], pos->y + offset[1], 2);
}

/* Draw the player's body, tinting the tool sprite while the tool is in use. */
static void player_draw_body(FarmPlayer *player, s32 offset, FarmScreenXY *pos)
{
    s16 tintA;
    s16 tintB;
    s16 tintC;
    AnimState *sprite;
    u_long *otEntry;

    otEntry = &current_frame->ot[offset >> 2];

    if (player->stateFlags & PLAYER_FLAG_EVENT_SPRITE)
    {
        sprite = &player->sprite;
        sprite_draw_traced(otEntry, current_frame->primCursor.ft4, sprite, player->sprite.animation, player->sprite.frame, pos->x,
                           pos->y, player->sprite.scale, 0, 64);
        anim_tick(sprite);
        return;
    }

    tintA = -1;
    tintB = -1;
    tintC = -1;
    if (player->mode == PLAYER_MODE_SWING_TOOL + 2 * TOOL_KIND_SICKLE || player->mode == PLAYER_MODE_SWING_TOOL + 2 * TOOL_KIND_HOE ||
        player->mode == PLAYER_MODE_SWING_TOOL + 2 * TOOL_KIND_AX || player->mode == PLAYER_MODE_SWING_TOOL + 2 * TOOL_KIND_HAMMER ||
        player->mode == PLAYER_MODE_SWING_TOOL + 2 * TOOL_KIND_WATERING_CAN)
    {
        if ((player->secondSprite.animation - 0x48) < 0x64U)
        {
            if (player->mode == PLAYER_MODE_SWING_TOOL + 2 * TOOL_KIND_WATERING_CAN)
            {
                tintA = tool_tint_modifier_alt_ids[player->held[HELD_TOOL].modifier];
            }
            else
            {
                tintA = tool_tint_modifier_ids[player->held[HELD_TOOL].modifier];
                tintC = tool_charge_tint_ids[player->held[HELD_TOOL].modifier];
                tintB = tool_charge_tint_ids[player->toolCharge / TOOL_CHARGE_STEP];
            }
        }
    }

    text_sprite_draw(otEntry, current_frame->primCursor.ft4, &player->secondSprite, pos->x, pos->y, 0x40, frame_ot_base,
                     player_sprite_places[frame_index], tintA, tintB, tintC);
    if ((game_state.globalFlags & GLOBAL_FLAG_PAUSED) == 0)
    {
        anim_tick(&game_state.player.secondSprite);
    }
}

/* Project the hand point and draw the toys, the marker, the carried item and the body (in the
 * order the pose needs), then the cursor, and the ridden cow or horse. */
void player_draw(VECTOR *position, s32 unused)
{
    FarmPlayer *player = &game_state.player;
    s32 depth;
    s32 reproject;

    if (scene_place_mismatch(&player->place.value) != 0)
    {
        return;
    }
    hand_vertex.vx = position->vx + 150;
    hand_vertex.vy = position->vy - 100;
    hand_vertex.vz = position->vz - 150;
    depth = gte_project_vertex(&hand_screen, &hand_vertex);
    if (player->chickToy != 0)
    {
        player_draw_toys(player);
    }
    if (depth == 0)
    {
        return;
    }
    if (player->latches & PLAYER_LATCH_MARKER)
    {
        anim_draw_sprt_cursor(&current_frame->ot[40], &ui_icon_anim, 0xE,
                              (game_state.frameCount >> 2) & 1,
                              hand_screen.x, (hand_screen.y - 50));
    }
    cue_draw_frame(&player->cue, &current_frame->ot[depth >> 2],
                   hand_screen.x, (hand_screen.y - 40));
    if (player->facing38 == FACING_DOWN || player->facing38 == FACING_RIGHT || (player->stateFlags & PLAYER_FLAG_CARRY_ITEM) ||
        player->mode == PLAYER_MODE_SHOW_TOOL || player->mode == PLAYER_MODE_POSE_4 || player->mode == PLAYER_MODE_USE_FEATHER ||
        player->secondSprite.animation == 5)
    {
        player_draw_carried_item(player, depth, &hand_screen);
        player_draw_body(player, depth, &hand_screen);
    }
    else
    {
        player_draw_body(player, depth, &hand_screen);
        player_draw_carried_item(player, depth, &hand_screen);
    }
    reproject = 0;
    if (player->mode == PLAYER_MODE_GO_TO_BED)
    {
        reproject = player->secondSprite.animation == 1;
    }
    if (player->stateFlags & PLAYER_FLAG_AIRBORNE)
    {
        reproject = 1;
    }
    if (reproject)
    {
        hand_vertex.vx = position->vx + 150;
        hand_vertex.vy = -100;
        hand_vertex.vz = position->vz - 150;
        depth = gte_project_vertex(&hand_screen, &hand_vertex);
        if (depth == 0)
        {
            return;
        }
    }
    if (!(player->stateFlags & PLAYER_FLAG_RIDING))
    {
        farm_draw_cursor(&current_frame->ot[depth >> 2], hand_screen.x, hand_screen.y, 0);
    }
    if (player->stateFlags & PLAYER_FLAG_RIDING)
    {
        if (player->stateFlags & PLAYER_FLAG_RIDING_COW)
        {
            player_draw_riding_cow(player);
        }
        else
        {
            horse_move_to_player(player);
            pet_reset_actor(&game_state.animals.horse);
        }
    }
    if (winter_anim_loaded != 0 && place_entries[player->place.location] == 0)
    {
        player_draw_slow_trail(player);
    }
}

static void player_draw_riding_cow(FarmPlayer *player)
{
    FarmActor *cow = action_object;
    cow->position.vx = player->position.vx;
    cow->position.vy = player->position.vy;
    cow->position.vz = player->position.vz;
    cow->position.pad = player->position.pad;
    farm_draw_actor(cow);
}

static void player_cow_animate(FarmPlayer *player)
{
    FarmActor *cow;
    cow = action_object;
    cow->facing = player->facing38;
    if (player->mode == PLAYER_MODE_IDLE)
    {
        anim_select_range(&cow->sprite, 0);
    }
    if (player->mode == PLAYER_MODE_POSE_2)
    {
        anim_select_range(&cow->sprite, (cow->facing + 1) & 0xFF);
    }
    if (player->mode == PLAYER_MODE_POSE_4)
    {
        anim_select_range(&cow->sprite, (cow->facing + 1) & 0xFF);
    }
    if (player->secondSprite.animation == player->facing38 + 0x3F)
    {
        anim_select_range(&cow->sprite, (cow->facing + 1) & 0xFF);
    }
    else if (anim_frame_jumps(&cow->sprite) != 0)
    {
        cow->sprite.frame = cow->facing;
    }
}

/* Count the frames the player stands still in the idle pose; riding or carrying resets the count. */
void player_count_idle(FarmPlayer *player)
{
    if (player->stateFlags & (PLAYER_FLAG_RIDING | PLAYER_FLAG_CARRY_BIG_ITEM | PLAYER_FLAG_CARRY_ITEM))
    {
        player->idleFrames = 0;
    }
    else if ((player->stateFlags & (PLAYER_FLAG_SCRIPTED | PLAYER_FLAG_IDLE_POSE)) == PLAYER_FLAG_IDLE_POSE)
    {
        player->idleFrames++;
    }
    else
    {
        player->idleFrames = 0;
    }
}

/* The two side cells of each facing. */
static FarmStep side_cell_offsets[4][2] = {
    {{-60, -10}, {60, -10}},
    {{-10, 60}, {-10, -60}},
    {{-60, 10}, {60, 10}},
    {{10, 60}, {10, -60}},
};

static FarmSlideStep slide_steps[4] = {
    {-10, 4, 10, 4},
    {4, 10, 4, -10},
    {-10, -4, 10, -4},
    {-4, 10, -4, -10},
};

/* Probe the cell ahead of the player. A blocked step waits BLOCKED_FRAMES_LIMIT frames; an object
 * the player can hop (target flag 0x20) starts a small jump while walking. Otherwise, while the
 * direction keys are held, slide sideways past the obstacle toward the free side cell. */
static void player_slide_on_bump(void)
{
    FarmPlayer *player = &game_state.player;
    VECTOR ahead;
    VECTOR side;
    FarmStep *offset = 0;
    FarmSlideStep *slide;
    s32 dx;
    s32 dz;
    s32 blocked;
    s32 open;
    s32 i;
    s32 hit;

    dx = 0;
    ahead = player->position;
    ahead.vx += player->velocity[0];
    ahead.vy += player->velocity[1];
    ahead.vz += player->velocity[2];
    dz = 0;
    switch (player->facing38)
    {
    case FACING_DOWN:
        dz = -48;
        break;
    case FACING_LEFT:
        dx = -48;
        break;
    case FACING_UP:
        dz = 48;
        break;
    case FACING_RIGHT:
        dx = 48;
        break;
    }
    ahead.vx += dx;
    ahead.vz += dz;
    hit = player_probe_target(player, &ahead, 0x38);
    if (hit == 999)
    {
        if ((farmTargetFlags & 0x20) && (player->velocity[0] != 0 || player->velocity[2] != 0) && farm_probe_steps((struct FarmProbePosition *)&player->position, player->facing38) != 0 && player->mode == PLAYER_MODE_POSE_4)
        {
            if (player->stateFlags & PLAYER_FLAG_AIRBORNE)
            {
                hit = 4;
            }
            else
            {
                player->stateFlags |= PLAYER_FLAG_AIRBORNE;
                sound_play(6);
                if (player->stateFlags & PLAYER_FLAG_RIDING)
                {
                    game_state.animals.horse.actor.flags |= ACTOR_FLAG_JUMPING;
                    player_set_mode(player, 5, player->facing38 + 0x3F, -2);
                }
                else
                {
                    player_set_mode(player, 5, 10, -2);
                }
                player->velocity[0] = 0;
                player->velocity[2] = 0;
                player->velocity[1] = -80;
                return;
            }
        }
        else
        {
            hit = 4;
        }
        farmTargetFlags = 0;
        farmTarget = 0;
    }
    if (!(farm_buttons_held & (PAD_ARROWS_MASK | PAD_CIRCLE)))
    {
        return;
    }
    open = 0;
    offset = side_cell_offsets[player->facing38 & 1];
    blocked = farm_settle_on_ground(&ahead, 0xE080);
    if (blocked != 0)
    {
        if (blocked_frames < BLOCKED_FRAMES_LIMIT)
        {
            blocked_frames++;
            player->velocity[0] = 0;
            player->velocity[2] = 0;
            return;
        }
    }
    else
    {
        if (hit == 4)
        {
            player->velocity[0] = 0;
            player->velocity[2] = 0;
        }
        blocked_frames = 0;
        return;
    }
    for (i = 0; i < 2; i++, offset++)
    {
        side = player->position;
        side.vx += offset->x;
        side.vz += offset->z;
        if (farm_settle_on_ground(&side, 0xE080) == 0 && player_probe_target(player, &side, 0) != 4)
        {
            open |= 1 << i;
        }
    }
    if (blocked == 0)
    {
        return;
    }
    if (player->stateFlags & PLAYER_FLAG_SCRIPTED)
    {
        return;
    }
    slide = &slide_steps[player->facing38 & 1];
    player->velocity[0] = 0;
    player->velocity[2] = 0;
    if (!(farm_buttons_held & PAD_CIRCLE))
    {
        return;
    }
    if (open == 1)
    {
        player->velocity[0] = slide->firstDx;
        player->velocity[2] = slide->firstDz;
    }
    if (open == 2)
    {
        player->velocity[0] = slide->secondDx;
        player->velocity[2] = slide->secondDz;
    }
    if (open == 3)
    {
        player->velocity[0] = slide->secondDx;
        player->velocity[2] = slide->secondDz;
    }
}

/* Move the player for one frame: slide past obstacles, add the step, and while airborne
 * add the jump's hop. Landing clears the airborne state, otherwise the fall speeds up to
 * 128. The position is then stored and the screen recentres on the player. */
void player_update_position(void)
{
    FarmPlayer *player = &game_state.player;
    FarmState *farm;
    FarmScroll *scroll;
    FarmHopStep *hop;
    VECTOR pos;
    u32 flags;
    u16 tile_x;
    u16 tile_y;
    u16 tile_z;

    if (!(player->stateFlags & PLAYER_FLAG_NO_INPUT) && !(game_state.debugFlags & DEBUG_FLAG_MUTEKI))
    {
        player_slide_on_bump();
    }
    player_status_word = *(u32 *)ground_hit_primitive();
    pos = player->position;
    if (player->stateFlags & PLAYER_FLAG_AIRBORNE)
    {
        if (!(player->stateFlags & PLAYER_FLAG_STILL))
        {
            hop = &HOPS[1][player->facing38];
            if (player->mode == PLAYER_MODE_JUMP_CROUCH)
            {
                hop = &HOPS[0][player->facing38];
            }
            if (player->mode == PLAYER_MODE_JUMP_LAUNCH)
            {
                hop = &HOPS[1][player->facing38];
            }
            player->velocity[0] += hop->dx;
            player->velocity[2] += hop->dz;
            ground_set_tolerance(GROUND_TOLERANCE_JUMP);
        }
    }
    pos.vx += player->velocity[0];
    pos.vy += player->velocity[1];
    pos.vz += player->velocity[2];
    if (player->stateFlags & PLAYER_FLAG_AIRBORNE)
    {
        if (farm_settle_on_ground(&pos, 0xC000) == 0)
        {
            pad_vibration_start(0, 2);
            flags = player->stateFlags;
            player->velocity[1] = 0;
            player->stateFlags = flags & ~PLAYER_FLAG_AIRBORNE;
            if (flags & PLAYER_FLAG_RIDING)
            {
                game_state.animals.horse.actor.flags &= ~ACTOR_FLAG_JUMPING;
                player_set_mode(&game_state.player, PLAYER_MODE_IDLE, 0, -2);
            }
        }
        else if (!(player->stateFlags & PLAYER_FLAG_STILL))
        {
            player->velocity[1] += 16;
            if (player->velocity[1] > 128)
            {
                player->velocity[1] = 128;
            }
        }
    }
    else
    {
        farm_settle_on_ground(&pos, 0xC080);
    }
    ground_set_tolerance(GROUND_TOLERANCE_DEFAULT);
    player->position = pos;
    farm = &game_state;
    if (!(farm->globalFlags & GLOBAL_FLAG_CAMERA_BUSY))
    {
        tile_x = farm->player.position.vx;
        tile_y = farm->player.position.vy;
        tile_z = farm->player.position.vz;
        farm_area.mode.scroll.centre.vx = tile_x;
        farm_area.mode.scroll.centre.vy = tile_y;
        farm_area.mode.scroll.centre.vz = tile_z;
    }
    player->velocity[0] = 0;
    player->velocity[2] = 0;
    if (farm->debugFlags & DEBUG_FLAG_SCR_LIMIT)
    {
        scene_clamp_result((struct FarmResultPair *)&farm_area.mode.scroll.centre);
    }
    PushMatrix();
    gte_project_vertex(&scroll_screen_point, &farm_area.mode.scroll.centre);
    scroll = &farm_area.mode.scroll;
    scroll->dx = 160 - scroll_screen_point.x;
    scroll->dy = 120 - scroll_screen_point.y;
    PopMatrix();
}

/* Close the gaps in a stored-item table: each empty entry takes the next non-empty one. */
static s32 stored_item_table_compact(FarmStoredItem *table, s32 count)
{
    s32 index;
    s32 search;

    for (index = 0; index < count; index++)
    {
        if (table[index].id == 0)
        {
            for (search = index + 1; search < count; search++)
            {
                if (table[search].id != 0)
                {
                    table[index].id = table[search].id;
                    table[index].modifier = table[search].modifier;
                    table[index].count = table[search].count;
                    table[search].id = 0;
                    table[search].modifier = 0;
                    table[search].count = 0;
                    break;
                }
            }
        }
    }
    return 0;
}

/* Swap the hand's item with the table. Mode 0 puts the hand's item at the end of the table and
 * takes the first entry into the hand; mode 1 puts it at the front and takes the last filled
 * entry. */
static s32 stored_item_table_exchange(FarmStoredItem *current, s32 count, FarmStoredItem *other, s32 mode)
{
    FarmStoredItem saved;
    s32 previous;
    if ((current->id == 0) && (other->id == 0))
    {
        return -1;
    }
    switch (mode)
    {
    case 0:
        saved = *current;

        current->id = 0;
        current->modifier = 0;
        current->count = 0;
        stored_item_table_compact(current, count);
        current[count - 1] = *other;
        stored_item_table_compact(current, count);
        *other = saved;
        break;
    case 1:
        previous = count - 1;

        while (previous >= 0 && current[previous].id == 0)
        {
            previous--;
        }
        saved = current[previous];
        current[previous].id = 0;
        current[previous].modifier = 0;
        current[previous].count = 0;
        memmove((unsigned char *)(current + 1), (unsigned char *)current, previous * sizeof(*current));
        *current = *other;
        *other = saved;
        stored_item_table_compact(current, count);
        break;
    }
    return -1;
}

/* The sprite animation for a tool id: the farm tools (1..25) come five to a kind, the animal
 * tools (26..29) follow. 0 for any other id. */
static s32 player_tool_animation(s16 tool)
{
    s32 index;
    s32 offset;
    s32 quotient;
    s32 row_index;
    s16 row;

    index = tool - 1;
    offset = tool - 26;
    if ((index & 0xFFFF) < 25U)
    {
        quotient = index / 5;
        row_index = quotient * 5;
        row = (quotient * 20 + 72);
        row_index = row_index + 1;
        return (s16)(row + (tool - row_index));
    }
    if ((offset & 0xFFFF) < 4U)
    {
        return (s16)(offset * 4 + 182);
    }
    return 0;
}

/* The ToolKind of a tool id: (id - 1) / 5 for the farm tools, id - 26 for the animal tools. */
static s16 player_tool_kind(s16 tool)
{
    if ((u16)(tool - 1) < 25)
    {
        return ((tool - 1) / 5);
    }
    if ((u16)(tool - 26) < 4)
    {
        return (tool - 26);
    }
    return 0;
}

/* Latch the tool target that player_apply_tool_target then applies. */
static inline void latch_tool(u32 flags, void *target)
{
    farmTargetFlags = flags;
    farmTarget = target;
}

/* Probe the cell in front of the player for a tool target and latch what is found
 * in farmTargetFlags and farmTarget. Returns 0 when the player cannot act, 999 when the probe
 * found the fishing spot for the rod, 4 when nothing reachable was found, and the probe result
 * times 2 otherwise. */
static s16 player_probe_target(FarmPlayer *player, VECTOR *pos, s32 kind_arg)
{
    s16 kind = kind_arg;
    VECTOR probe;
    void *target;
    u32 found;
    s16 *offset;
    u32 flags;
    s32 mask;
    s32 result;
    s32 mode;

    found = 0;
    target = 0;
    probe.vy = pos->vy;
    offset = &move_steps_small[kind + player->facing38 * 2];
    probe.vx = pos->vx + offset[0];
    probe.vz = pos->vz + offset[1];
    player->stateFlags &= ~PLAYER_FLAG_BIT18;
    mask = 0x8FAE;
    if (player->stateFlags & PLAYER_FLAG_RIDING)
    {
        mask = 0x8DAE;
    }
    if (farm_buttons_held & PAD_CIRCLE)
    {
        mask &= ~0x80;
    }
    result = farm_callback_capped(&probe, player->place, 1000, 1000, &target, &found, 0, mask) * 2;
    farmTargetFlags = 0;
    farmTarget = 0;
    if (kind != ITEM_NOTHING && kind != ITEM_FISHING_ROD)
    {
        if (found & 0x10000)
        {
            mode = 1;
            if (!(player->stateFlags & (PLAYER_FLAG_FISHING | PLAYER_FLAG_MODE_ANIM)))
            {
                if (!(player->stateFlags & (PLAYER_FLAG_CARRY_BIG_ITEM | PLAYER_FLAG_CARRY_ITEM)))
                {
                    mode = 2;
                }
            }
            else
            {
                mode = 0;
            }
            if (player->mode == PLAYER_MODE_POSE_4 || player->mode == PLAYER_MODE_POSE_2)
            {
                mode = 2;
            }
            if (player->mode == PLAYER_MODE_0E || player->mode == PLAYER_MODE_06)
            {
                mode = 0;
            }
            if (mode != 0)
            {
                if (mode == 2)
                {
                    if (!(player->stateFlags & PLAYER_FLAG_REACHING))
                    {
                        player_set_mode(&game_state.player, PLAYER_MODE_IDLE, 0, -2);
                    }
                    player->stateFlags |= PLAYER_FLAG_REACHING;
                }
                if (target != 0)
                {
                    if (found & 0x2)
                    {
                        if (livestock_bump_count >= 51)
                        {
                            livestock_action_dispatch(target, 0xD);
                        }
                        else
                        {
                            livestock_bump_count++;
                        }
                    }
                    if (found & 0x4)
                    {
                        if (livestock_bump_count >= 51)
                        {
                            livestock_action_dispatch_b(target, 0xD);
                        }
                        else
                        {
                            livestock_bump_count++;
                        }
                    }
                    if (found & 0x200)
                    {
                        if (livestock_bump_count >= 51)
                        {
                            horse_set_state(target, 0xD);
                        }
                        else
                        {
                            livestock_bump_count++;
                        }
                    }
                }
            }
            else
            {
                found |= 0x4000;
            }
        }
        else if (player->stateFlags & PLAYER_FLAG_REACHING)
        {
            player->stateFlags = (player->stateFlags & ~PLAYER_FLAG_REACHING) | PLAYER_FLAG_POSE_RESET_PENDING;
        }
        if (found & 0x2000)
        {
            player->stateFlags |= PLAYER_FLAG_BIT18;
            latch_tool(found, target);
        }
        latch_tool(found, target);
    }
    if (found & 0x4000)
    {
        result = 4;
        if (kind == ITEM_FISHING_ROD)
        {
            result = 999;
            latch_tool(found, target);
        }
    }
    flags = player->stateFlags;
    if (flags & PLAYER_FLAG_AUTO_WALK)
    {
        return 0;
    }
    if (flags & PLAYER_FLAG_RIDING_COW)
    {
        player->stateFlags = flags & ~(PLAYER_FLAG_POSE_RESET_PENDING | PLAYER_FLAG_REACHING);
        if (player->mode == PLAYER_MODE_RIDE)
        {
            return 0;
        }
        if (player->mode == PLAYER_MODE_DISMOUNT)
        {
            return 0;
        }
    }
    return result;
}

/* Reset the player's pose to idle. */
s32 player_reset_pose(void)
{
    FarmPlayer *player;

    player = &game_state.player;
    player_set_mode(player, PLAYER_MODE_IDLE, 0, -2);
    game_state.player.scriptRunning = 0;
    game_state.player.stateFlags = (game_state.player.stateFlags & ~PLAYER_FLAG_MODE_ANIM) | PLAYER_FLAG_IDLE_POSE;
    return 0;
}

/* Show `item` in the player's hands: the bigger pose (PLAYER_MODE_0E) when its item flags have
 * ITEM_FLAG_BIT8, otherwise PLAYER_MODE_06. */
s32 player_show_item(FarmStoredItem *item)
{
    FarmPlayer *player;
    s32 mode;
    s32 slot;

    player = &game_state.player;
    stored_item_clear(&player->shown);
    slot = PLAYER_MODE_06;
    mode = 11;
    if (game_state.itemInfo[item->id].itemFlags & ITEM_FLAG_BIT8)
    {
        slot = PLAYER_MODE_0E;
        mode = 0x25;
    }
    stored_item_copy(&player->shown, item);
    picture_load_to_vram(player->shown.id);
    player_set_mode(player, slot, mode + game_state.player.facing38, -2);
    player->stateFlags |= (PLAYER_FLAG_BIT2 | PLAYER_FLAG_SHOWING_ITEM);
    return 0;
}

/* Keep carrying: once the last item slot is free, put the carried item away. */
static void player_carry_tick(void)
{
    FarmPlayer *player = &game_state.player;

    if (!(player->stateFlags & PLAYER_FLAG_CARRY_ITEM))
    {
        player_reset_pose();
        return;
    }
    player_compact_items();
    if (player->items[player->itemSlots - 1].id == 0)
    {
        sound_play(8);
        player_set_mode(player, PLAYER_MODE_STORE_ITEM, player->facing38 + 0x1D, -2);
        player->stateFlags &= ~PLAYER_FLAG_SHOWING_ITEM;
    }
}

/* Hold `item` out to the viewer in the hold-out pose. */
static void player_hold_out_item(FarmStoredItem *item)
{
    FarmPlayer *player = &game_state.player;
    stored_item_clear(&player->shown);
    stored_item_copy(&player->shown, item);
    player_set_mode(player, PLAYER_MODE_HOLD_OUT_ITEM, player->facing38 + 0x21, -2);
}

/* Classify the record under the player's selection: 3 when there is none, 2 when it
 * holds an item, 0 otherwise. */
static s32 player_target_record_state(FarmPlayer *context)
{
    VECTOR local;
    s16 column;
    s16 row;
    s16 place;
    u16 value;
    player_facing_point(context, &local, 0);
    place = farm_area.mode.place.location;
    tile_row_ptr = 0;
    if (place == PLACE_FARM)
    {
        if (field_world_to_cell(&local, &column, &row) != 0)
            return 3;
        tile_row_ptr = &game_state.fieldTiles[row].rows[column];
    }
    else if (place == PLACE_GREENHOUSE)
    {
        if (greenhouse_world_to_cell(&local, &column, &row) != 0)
            return 3;
        tile_row_ptr = &game_state.slots7x7[row].rows[column];
    }
    if (tile_row_ptr == 0)
        return 3;
    value = tile_row_ptr->value;
    if ((u32)(value - ITEM_SOIL) >= 3)
    {
        if (value != 0)
            return 2;
    }
    return 0;
}

/* Start the player's pose `anim` for the held item or tool and report that one started. */
static inline s32 player_start_held_pose(FarmPlayer *player, s32 anim)
{
    player_set_mode(player, anim, player->facing38 + 0x17, -2);
    return 1;
}

/* Use the held item. With no argument (held == 0), pick the animation for the item and
 * the target in front of the player; otherwise finish the use. Returns 1 when an
 * animation was started. */
s32 player_use_held_item(s32 held)
{
    FarmPlayer *player = &game_state.player;
    ItemInfo *item;
    s32 state;
    s32 accepted;
    s32 bits;
    u16 flags;

    if (held == 0)
    {
        state = player_target_record_state(player);
        item = &game_state.itemInfo[player->held[HELD_ITEM].id];
        if (player->held[HELD_ITEM].id == ITEM_DOG_BALL)
        {
            if (farmTargetFlags == 0 || state == 0)
            {
                return player_start_held_pose(player, PLAYER_MODE_THROW_ITEM);
            }
            return 0;
        }
        if (player->held[HELD_ITEM].id == ITEM_BASKET)
        {
            if (player_status_word & 0xC087)
            {
                return 0;
            }
            if (farmTargetFlags != 0)
            {
                return 0;
            }
            return player_start_held_pose(player, PLAYER_MODE_62);
        }
        if (player->held[HELD_ITEM].id == ITEM_FISH_FOOD)
        {
            if (farmTargetFlags == 0 || state == 0)
            {
                return player_start_held_pose(player, PLAYER_MODE_THROW_ITEM);
            }
            return 0;
        }
        flags = item->itemFlags;
        if (flags & ITEM_FLAG_BIT8)
        {
            if (player_status_word & 0xC087)
            {
                return 0;
            }
            if (farmTargetFlags == 0 || state == 0)
            {
                return player_start_held_pose(player, PLAYER_MODE_DROP_ITEM);
            }
        }
        /* A target word of -1 accepts any target. */
        flags = item->itemFlags;
        if ((flags & 0x9000) == 0x9000 || (bits = item->targets.word) == -1)
        {
            accepted = 0;
            if (farmTargetFlags == 0)
            {
                accepted = 1;
            }
            else
            {
                if (farmTargetFlags & 0x8000)
                {
                    if (((u16 *)farmTarget)[1] == 100 && item->sellPrice != 0)
                    {
                        return player_start_held_pose(player, PLAYER_MODE_BASKET_ADD);
                    }
                    accepted = 1;
                }
                if (farmTargetFlags & 0x200)
                {
                    accepted = game_state.animals.horse.actor.kind == ITEM_HORSE;
                }
            }
            if (accepted)
            {
                return player_start_held_pose(player, PLAYER_MODE_THROW_ITEM);
            }
            bits = item->targets.word;
        }
        if (bits & 0x10)
        {
            if (state == 0 && farmTargetFlags == 0)
            {
                return player_start_held_pose(player, PLAYER_MODE_SELECT_RESULT);
            }
            if (state == 3 && farmTargetFlags == 0)
            {
                return player_start_held_pose(player, PLAYER_MODE_THROW_ITEM);
            }
            if (state == 2)
            {
                return 1;
            }
        }
        if (farmTargetFlags != 0 || state != 2)
        {
            return 0;
        }
        return player_start_held_pose(player, PLAYER_MODE_THROW_ITEM);
    }
    if (player->held[HELD_ITEM].id == ITEM_BASKET)
    {
        player->stateFlags |= PLAYER_FLAG_ACTION_START;
        player_set_mode(player, PLAYER_MODE_SHIP_BASKET, 0, -2);
        anim_select_range(&player->secondSprite, player->facing38);
        player->stateFlags |= PLAYER_FLAG_BIT14;
        return 1;
    }
    switch ((u32)stored_item_kind(&player->held[HELD_ITEM]))
    {
    case STORED_ITEM_SEED:
        return 1;
    case STORED_ITEM_FOOD:
        player_set_mode(player, PLAYER_MODE_SHOW_ITEM, 5, -2);
        sound_play(0x4A);
        return 1;
    case STORED_ITEM_DRINK:
        player->stateFlags |= (PLAYER_FLAG_ACTION_START | PLAYER_FLAG_BIT14);
        player_set_mode(player, PLAYER_MODE_USE_HELD_ITEM, 0, -2);
        sound_play(0x4B);
        return 1;
    }
    return 0;
}

/* Put the received item in hand, then pick the follow-up by the item's row flags. */
void player_receive_item(FarmPlayer *player, s32 value, s32 state)
{
    FarmStoredItem received;
    u16 flags;

    received.id = value;
    received.modifier = 0;
    received.count = 1;
    player_set_mode(player, PLAYER_MODE_SHOW_ITEM, 5, -2);
    player_stored_item = player->held[HELD_ITEM];
    if (player->held[HELD_ITEM].id != 0)
    {
        picture_release_item(player->held[HELD_ITEM].id);
    }
    stored_item_clear(&player->held[HELD_ITEM]);
    stored_item_clear(&player->shown);
    stored_item_copy(&player->shown, &received);
    picture_load_to_vram(player->shown.id);
    player->stateFlags &= ~PLAYER_FLAG_BIT14;
    flags = game_state.itemInfo[player->shown.id].itemFlags;
    if ((flags & 0x1200) == 0x1200)
    {
        state = RECEIVE_ITEM_EAT;
    }
    if ((flags & 0x1600) == 0x1600)
    {
        state = RECEIVE_ITEM_DRINK;
    }
    switch (state)
    {
    case RECEIVE_ITEM_HOLD:
    case RECEIVE_ITEM_HOLD_2:
        stored_item_copy(&player->held[HELD_ITEM], &player->shown);
        player_set_mode(&game_state.player, PLAYER_MODE_HOLD_OUT_ITEM, game_state.player.facing38 + 0x21, -2);
        break;
    case RECEIVE_ITEM_EAT:
        player->stateFlags |= PLAYER_FLAG_SHOWING_ITEM;
        stored_item_copy(&player->held[HELD_ITEM], &player->shown);
        player_set_mode(player, PLAYER_MODE_SHOW_ITEM_2, 5, -2);
        sound_play(0x4A);
        break;
    case RECEIVE_ITEM_DRINK:
        stored_item_copy(&player->held[HELD_ITEM], &player->shown);
        player->stateFlags |= (PLAYER_FLAG_ACTION_START | PLAYER_FLAG_BIT14);
        player_set_mode(player, PLAYER_MODE_SHOW_ITEM_PICKUP, 0, -2);
        sound_play(0x4B);
        break;
    }
}

/* Drop the held item: it is placed on the ground in front of the player when its item flags have
 * ITEM_FLAG_BIT8, then the hand is cleared. */
void player_drop_held_item(FarmPlayer *player)
{
    s16 item = player->held[HELD_ITEM].id;
    u16 flags = game_state.itemInfo[item].itemFlags;
    VECTOR local;
    if (flags & ITEM_FLAG_BIT8)
    {
        player_facing_point(player, &local, 2);
        farm_place_held(item, &player->place.value, &local);
    }
    picture_release_item(item);
    stored_item_clear(&player->held[HELD_ITEM]);
}

/* Whether the player can act: no timed mode is playing (PLAYER_FLAG_MODE_ANIM). */
int player_can_act(void)
{
    u32 mask = PLAYER_FLAG_MODE_ANIM;
    u32 x = game_state.player.stateFlags;
    x = x & mask;
    return x < 1;
}

/* Set the facing and keep the old one as prevFacing3A. */
static void player_set_facing(s16 facing)
{
    u16 old = game_state.player.facing38;
    game_state.player.facing38 = facing;
    game_state.player.prevFacing3A = old;
}

/* Ship a thrown item that lands in the shipping area. */
static s32 player_ship_thrown_item(FarmThrownItem *item)
{
    void *ignored_x;
    s32 ignored_y;
    s32 result;

    if (farm_area.mode.data->head.place.value != 0)
    {
        return 0;
    }
    if (game_state.animals.horse.actor.kind != ITEM_HORSE)
    {
        return 0;
    }
    result = farm_callback_capped(&item->position, game_state.player.place, 0, 0,
                                  &ignored_x, &ignored_y, 0, 0x200);
    if (result != 0)
    {
        estate_shipment_add((u16)item->kind);
        sound_play(0x13);
        thrown_item_retire(item);
        return 1;
    }
    return 0;
}

/* Clear the three thrown-item slots. */
void thrown_items_reset(void)
{
    u32 i = 0;
    FarmThrownItem *item = thrown_items;
    do
    {
        thrown_item_clear(item);
        i++;
        item++;
    } while (i < 3);
}

static void thrown_item_clear(FarmThrownItem *item)
{
    item->blinking = 0;
    item->phase = THROWN_ITEM_FLY;
    item->timer = 0;
    item->kind = 0;
    item->vx = 0;
    item->vy = 0;
    item->vz = 0;
    item->velocityY = 0;
    item->position.vx = 0;
    item->position.vy = 0;
    item->position.vz = 0;
}

/* Claim a free thrown-item slot for `item` thrown from `pos` in direction `dir`;
 * returns the slot, or 0 when all three are busy. */
static FarmThrownItem *thrown_item_spawn(VECTOR *pos, FarmStoredItem *item, s16 dir)
{
    FarmThrownItem *effect;
    FarmVelocity *velocity;
    u32 i;

    effect = thrown_items;
    i = 0;
    do
    {
        if (effect->kind == 0)
        {
            velocity = &facing_velocities[dir];
            effect->dir = dir;
            thrown_item_clear(effect);
            effect->kind = item->id;
            effect->position.vx = pos->vx;
            effect->position.vy = pos->vy - 300;
            effect->position.vz = pos->vz;
            effect->vx = velocity->x;
            effect->vy = -30;
            effect->vz = velocity->z;
            if (effect->kind == ITEM_DOG_BALL)
            {
                effect->vy = -12;
                effect->velocityY = -28;
                effect->vx *= 2;
                effect->vz *= 2;
            }
            if (effect->kind == ITEM_FISHING_MARKER)
            {
                effect->velocityY = -78;
                effect->vy = -30;
                effect->vx = velocity->x * 2 / 3;
                effect->vz = velocity->z * 2 / 3;
            }
            effect->groundY = game_state.player.position.vy;
            return effect;
        }
        i++;
        effect++;
    } while (i < 3);
    return 0;
}

/* Step and draw one thrown-item slot. In flight it moves and is handed to thrown_item_land
 * on landing; the hold and blink phases keep it on screen before it is cleared; the landed
 * phases wait for its landing animation to end. Its shadow is drawn at the ground below. */
static void thrown_item_update(FarmThrownItem *effect)
{
    FarmGroundProbe ground;
    FarmStoredItem pair;
    s32 landing;
    s32 depth;
    u_long *ot;

    landing = 0;
    thrown_item_vertex.vx = effect->position.vx + 150;
    thrown_item_vertex.vy = effect->position.vy + effect->velocityY - 100;
    thrown_item_vertex.vz = effect->position.vz - 150;
    if (!(game_state.globalFlags & GLOBAL_FLAG_PAUSED) && fade_is_settled() == 1)
    {
        switch (effect->phase)
        {
        case THROWN_ITEM_FLY:
            effect->position.vx += effect->vx / FIXED_POINT_ONE;
            effect->position.vz += effect->vz / FIXED_POINT_ONE;
            effect->position.vy += effect->velocityY;
            effect->velocityY += THROWN_ITEM_GRAVITY;
            if (effect->velocityY > THROWN_ITEM_MAX_FALL)
            {
                effect->velocityY = THROWN_ITEM_MAX_FALL;
            }
            landing = thrown_item_land(effect);
            if (landing == 1)
            {
                return;
            }
            if (player_ship_thrown_item(effect) != 0)
            {
                return;
            }
            break;
        case THROWN_ITEM_HOLD:
            if (--effect->timer == 0)
            {
                effect->phase = THROWN_ITEM_BLINK;
                effect->timer = 4;
                effect->blinking = 1;
            }
            break;
        case THROWN_ITEM_BLINK:
            /* Blink faster as the count runs out. */
            --effect->timer;
            if (effect->timer >= 4)
            {
                if (effect->timer & 4)
                {
                    return;
                }
            }
            else if (effect->timer >= 3)
            {
                if (effect->timer & 2)
                {
                    return;
                }
            }
            else if (effect->timer >= 2)
            {
                if (effect->timer & 1)
                {
                    return;
                }
            }
            if (effect->timer == 0)
            {
                thrown_item_retire(effect);
                return;
            }
            break;
        case THROWN_ITEM_LANDED:
        case THROWN_ITEM_EGG_LANDED:
            if (thrown_item_anim.flags & 0x100)
            {
                thrown_item_retire(effect);
                if (effect->phase == THROWN_ITEM_EGG_LANDED)
                {
                    pair.id = ITEM_SPA_BOILED_EGG;
                    pair.count = 1;
                    player_show_item(&pair);
                }
                return;
            }
            break;
        }
    }
    depth = gte_project_vertex(&thrown_item_screen, &thrown_item_vertex);
    if (depth == 0)
    {
        return;
    }
    ot = &current_frame->ot[depth >> 2];
    if (effect->phase == THROWN_ITEM_LANDED || effect->phase == THROWN_ITEM_EGG_LANDED)
    {
        anim_draw_sprt_advance(ot, &thrown_item_anim, thrown_item_screen.x, thrown_item_screen.y);
        return;
    }
    if (effect->kind == ITEM_FISHING_MARKER)
    {
        anim_draw_sprt_cursor(ot, &ui_icon_anim, fishing_marker_value, 0, thrown_item_screen.x, thrown_item_screen.y);
    }
    else
    {
        picture_cell_build_prim(effect->kind, effect->blinking, ot, thrown_item_screen.x, thrown_item_screen.y, 2);
    }
    ground.x = effect->position.vx;
    ground.y = effect->groundY;
    ground.z = effect->position.vz;
    player_settle_ground_tolerance(&ground);
    if (landing == 2 && farm_status_cache() == 0)
    {
        effect->groundY = ground.y;
    }
    if (effect->kind == ITEM_FISHING_MARKER)
    {
        return;
    }
    thrown_item_vertex.vx = effect->position.vx + 150;
    thrown_item_vertex.vy = effect->groundY - 100;
    thrown_item_vertex.vz = effect->position.vz - 150;
    depth = gte_project_vertex(&thrown_item_screen, &thrown_item_vertex);
    if (depth == 0)
    {
        return;
    }
    farm_draw_cursor(&current_frame->ot[depth >> 2], thrown_item_screen.x, thrown_item_screen.y, 0);
}

/* Land a thrown item on the ground: its landing animation starts, and fish, fish food, the
 * pond or a zone reacts. Returns 1 when it landed, 0 when the spot has no landing. */
static s32 thrown_item_settle(FarmThrownItem *item, FarmGroundProbe *probe)
{
    s32 result;
    s32 zone;
    result = farm_status_kind();
    if (result != 0)
    {
        anim_bind_file(&thrown_item_anim, (AnimFile *)farm_area.stageData);
        anim_select_range(&thrown_item_anim, 0x17);
        sound_play(0x10);
        item->phase = THROWN_ITEM_LANDED;
        item->velocityY = 0;
        if (result == 2)
        {
            if (item->kind >= ITEM_LARGE_FISH && item->kind <= ITEM_SMALL_FISH)
            {
                if (pond_add_fish(item->kind) == 0)
                    menu_request_open(3, 0x382);
            }
            if (item->kind == ITEM_FISH_FOOD)
                pond_mark_fed();
        }
        else
        {
            if ((farm_area.mode.data->head.place.location == PLACE_2C) && (item->kind == ITEM_SMALL_FISH))
                event_flag_set(EVENT_FLAG_SMALL_FISH_PLACED, 1);
            if ((result != 3) && (item->kind == ITEM_FISHING_MARKER))
                fishing_marker_active = 1;
        }
        zone = player_result_code_lookup(probe);
        if (zone != 0)
        {
            switch (zone)
            {
            case ZONE_EGG_TARGET:
                if (item_is_egg(item->kind) != 0)
                    item->phase = THROWN_ITEM_EGG_LANDED;
                break;
            case ZONE_GIFT_TARGET:
                game_state.player.currentAction = item->kind;
                if (item->kind == ITEM_GIFT)
                    game_state.player.currentAction = game_state.player.held[HELD_ITEM].modifier;
                event_trigger_mode_set(4);
                break;
            default:
                break;
            }
        }
        return 1;
    }
    return 0;
}

/* Find the farm tile a thrown item lands on. A thrown item off the tile grid is held in the air
 * for a frame; returns 2 when there is no tile at all, else 0. */
static s32 thrown_item_resolve_target(FarmThrownItem *item)
{
    s16 row;
    s16 column;
    s16 map_kind;
    s16 second_kind;
    s32 result;
    void *selected;

    selected = 0;
    map_kind = farm_area.mode.place.location;
    if (map_kind == PLACE_FARM)
    {
        if (field_world_to_cell(&item->position, &column, &row) != 0)
        {
            item->phase = THROWN_ITEM_HOLD;
            item->timer = 1;
            item->velocityY = 0;
            return 0;
        }
        selected = &game_state.fieldTiles[row].rows[column];
    }
    second_kind = farm_area.mode.place.location;
    if (second_kind == PLACE_GREENHOUSE)
    {
        if (greenhouse_world_to_cell(&item->position, &column, &row) != 0)
        {
            result = 0;
            goto clear;
        }
        selected = &game_state.slots7x7[row].rows[column];
    }
    if (selected == 0)
    {
        return 2;
    }
    result = 0;
clear:
    if (result == 0)
    {
        item->phase = THROWN_ITEM_HOLD;
        item->timer = 1;
        item->velocityY = 0;
    }
    return result;
}

/* Move a thrown item one step; on landing, hand it to whatever is below. */
static s32 thrown_item_land(FarmThrownItem *effect)
{
    FarmGroundProbe probe;
    FarmStoredItem pair;
    VECTOR timing;
    FarmVelocity *velocity;
    FarmEstateRecord *entry;
    FarmEstateRecord *hit;
    s32 target;
    s32 landed;

    landed = 0;
    probe.x = effect->position.vx + effect->vx / FIXED_POINT_ONE;
    probe.y = effect->position.vy;
    probe.z = effect->position.vz + effect->vz / FIXED_POINT_ONE;
    player_settle_ground_tolerance(&probe);
    if (effect->position.vy > 5000)
    {
        landed = 1;
    }
    if (!landed && (farm_status_cache() != 0 || probe.y < effect->position.vy - 100))
    {
        effect->dir = (effect->dir + 2) & 3;
        velocity = &facing_velocities[effect->dir];
        effect->vx = velocity->x * 2;
        effect->vz = velocity->z * 2;
        effect->position.vx += effect->vx / FIXED_POINT_ONE;
        effect->position.vz += effect->vz / FIXED_POINT_ONE;
        if (effect->kind != ITEM_DOG_BALL)
        {
            effect->vx = 0;
            effect->vz = 0;
        }
        return 0;
    }
    if (effect->position.vy <= effect->groundY && !landed)
    {
        return 2;
    }
    if (effect->kind == ITEM_DOG_BALL)
    {
        entry = &game_state.runs.records[18][1];
        entry->location = game_state.player.place.location;
        entry->position.vx = effect->position.vx;
        entry->position.vy = effect->position.vy;
        entry->position.vz = effect->position.vz;
        entry->position.pad = effect->position.pad;
    }
    if (item_is_fish(effect->kind) == 1)
    {
        pair.count = 1;
        pair.id = item_fish_print_id(effect->kind);
        thrown_item_retire(effect);
        player_show_item(&pair);
        return 1;
    }
    if (thrown_item_settle(effect, &probe) != 0)
    {
        return 0;
    }
    if (effect->kind == ITEM_FISHING_MARKER)
    {
        thrown_item_retire(effect);
        return 1;
    }
    hit = estate_request_near_active(&probe);
    if (hit != 0)
    {
        hit->recordFlags |= 0x100;
        if (game_state.itemInfo[effect->kind].sellPrice != 0 &&
            estate_basket_add(effect->kind) != 0)
        {
            menu_request_open(3, 0x383);
        }
        thrown_item_retire(effect);
        return 1;
    }
    if (farm_status_upper_bits() != 0 && game_state.itemInfo[effect->kind].sellPrice != 0)
    {
        estate_shipment_add((u16)effect->kind);
        sound_play(0x13);
        thrown_item_retire(effect);
        event_flag_set(EVENT_FLAG_ITEM_SHIPPED, 1);
        return 1;
    }
    if (effect->kind == ITEM_DOG_BALL)
    {
        timing.vx = effect->position.vx;
        timing.vy = effect->position.vy;
        timing.vz = effect->position.vz;
        thrown_item_retire(effect);
        estate_marker_stamp(ITEM_DOG_BALL, timing, game_state.player.place.location, 0);
        dog_set_phase(&game_state.animals.dog, DOG_PHASE_FETCH);
        return 1;
    }
    target = thrown_item_resolve_target(effect);
    if (target == 2)
    {
        villager_affection_decay();
        effect->phase = THROWN_ITEM_HOLD;
        effect->timer = 1;
        effect->velocityY = 0;
        return 0;
    }
    return target;
}

/* Update each live thrown-item slot. */
void thrown_items_update(void)
{
    FarmThrownItem *item = thrown_items;
    u32 i = 0;
    do
    {
        if (item->kind != 0)
            thrown_item_update(item);
        i++;
        item++;
    } while (i < 3);
}

/* Retire a thrown-item slot. A fishing marker stays where it landed; any other item is dropped
 * from the hand. */
static void thrown_item_retire(FarmThrownItem *item)
{
    s16 kind = item->kind;
    item->kind = 0;
    game_state.player.stateFlags &= ~PLAYER_FLAG_ITEM_IN_FLIGHT;
    if (kind == ITEM_FISHING_MARKER)
    {
        FishingMarkerPoint *p = &fishing_marker_origin;
        p->x = item->position.vx;
        p->y = item->position.vy - 10;
        p->z = item->position.vz;
    }
    else
    {
        player_drop_held_item(&game_state.player);
    }
}

/* Set the player's place to the current area, and the dog's too while the player holds
 * ITEM_ADULT_DOG or ITEM_YOUNG_DOG. */
void player_latch_place(void)
{
    FarmPlayer *player;

    player = &game_state.player;
    player->place = farm_area.mode.data->head.place;
    if ((u32)(player->held[HELD_ITEM].id - ITEM_ADULT_DOG) < 2)
    {
        game_state.animals.dog.actor.place = player->place;
    }
}

/* Morning reset of the player: clear the day's state, restore stamina and fatigue
 * (by how long the player slept, or by the bedtime when the collapsed flag is set),
 * note the wake time, advance the tiredness counter by the hours past 8 (capped at 15)
 * and clear the day's events. */
void player_morning_reset(void)
{
    FarmPlayer *player = &game_state.player;
    FarmState *farm;
    s32 slept;
    FarmState *state;
    s32 fatigue;

    if (player->stateFlags & PLAYER_FLAG_ACTION)
    {
        player_end_action(player);
    }
    player->talkVillager = -1;
    farm = &game_state;
    player->idleFrames = 0;
    player->toolCharge = 0;
    player->talkAnimal = 0;
    player->facing38 = FACING_DOWN;
    player->stateFlags &= (PLAYER_FLAG_BIT2 | PLAYER_FLAG_FISHING_LINE | PLAYER_FLAG_RIDING_COW | PLAYER_FLAG_HALF_FATIGUE | PLAYER_FLAG_REFRESH_HELD | PLAYER_FLAG_BIT16 | PLAYER_FLAG_POSE_RESET_PENDING | PLAYER_FLAG_BIT19 | PLAYER_FLAG_EVENT_SPRITE | PLAYER_FLAG_SCRIPTED | PLAYER_FLAG_BIT24 | PLAYER_FLAG_IDLE_POSE | PLAYER_FLAG_CARRY_BIG_ITEM | PLAYER_FLAG_CARRY_ITEM | PLAYER_FLAG_MODE_ANIM | PLAYER_FLAG_BIT31);
    farm->progress.estate.estateFlags &= ~(ESTATE_FLAG_CHICKEN_NOTICE | ESTATE_FLAG_VISITORS | ESTATE_FLAG_BIT16);
    player_stop_motion();
    player_reset_pose();
    event_flag_set(EVENT_FLAG_NOON_12H, 0);
    event_flag_set(EVENT_FLAG_EVENING_18H, 0);
    if (!(farm->statusFlags & STATUS_FLAG_COLLAPSED))
    {
        if (event_flag_get(EVENT_FLAG_OVERNIGHT) != 1)
        {
            farm_clamp_status(0, 10, 0, 0);
        }
        else
        {
            /* Minutes slept: wake time plus a day, less bedtime. */
            slept = farm->progress.clock.minuteOfDay + CLOCK_DAY_MINUTES;
            slept -= (s16)player->sleptAt;
            fatigue = (s8)player_sleep_fatigue_gain(slept);
            farm_clamp_status(fatigue, player_sleep_stamina_loss(slept), 0, 0);
        }
    }
    else
    {
        if ((u32)(player->sleptAt - 360) < 720)
        {
            farm_clamp_status(100, -25, 0, 0);
            farm_clamp_status(100, 0, 0, 0);
        }
        else
        {
            farm_clamp_status(50, -22, 0, 0);
        }
        game_state.statusFlags &= ~STATUS_FLAG_COLLAPSED;
    }
    player->latches = 0;
    player_update_condition_flags(player, 1);
    state = &game_state;
    player->wokeAt = state->progress.clock.minuteOfDay;
    if (state->progress.clock.time.hm.hour >= 8)
    {
        state->player.counts70[5] += state->progress.clock.time.hm.hour - 8;
    }
    if (state->player.counts70[5] >= 16)
    {
        state->player.counts70[5] = 15;
    }
    state->progress.estate.lumberDeliveries = 0;
    event_flag_set(EVENT_FLAG_SLEEPING, 0);
    event_flag_set(EVENT_FLAG_RESULT_SCREEN, 0);
    player_init_toys(player);
    player_reset_trail(player);
    event_flag_set(EVENT_FLAG_SHIPMENT_SETTLED, 0);
}

/* Stored-item release: when the counter runs out, release the item's picture and clear the slot. */
static void stored_item_release(void *p)
{
    if (counter_step(p, -1) != 0)
    {
        picture_release_item(*(s16 *)p);
        stored_item_clear(p);
    }
}

/* Set the carry state from the held item: an item with flag 0x180 in its item info is
 * carried in both hands, any other in one. With nothing held, the carry is cleared. */
void player_refresh_carry_state(FarmPlayer *player)
{
    s32 id;

    id = player->held[HELD_ITEM].id;
    if (id == 0)
    {
        player->stateFlags &= ~(PLAYER_FLAG_SHOWING_ITEM | PLAYER_FLAG_CARRY_BIG_ITEM | PLAYER_FLAG_CARRY_ITEM);
        player_reset_pose();
        return;
    }
    player->stateFlags &= ~(PLAYER_FLAG_CARRY_BIG_ITEM | PLAYER_FLAG_CARRY_ITEM);
    if (game_state.itemInfo[id].itemFlags & 0x180)
    {
        player->stateFlags |= PLAYER_FLAG_CARRY_BIG_ITEM;
    }
    else
    {
        player->stateFlags |= PLAYER_FLAG_CARRY_ITEM;
    }
    player->stateFlags |= PLAYER_FLAG_SHOWING_ITEM;
}

/* Bind the sprite bank of the tool action for the current mode, reloading its sector first when
 * `reload` is set. */
static void player_load_tool_anim(FarmPlayer *player, s32 reload)
{
    s32 raw_index;
    s32 index;
    u8 *file;
    AnimState *state;
    u16 *table;

    raw_index = player->mode;
    raw_index -= 0x3B;
    raw_index <<= 16;
    index = raw_index >> 16;
    if (reload != 0)
    {
        farm_load_sector_row(index);
    }
    state = &player->secondSprite;
    file = farm_area.sectorBuffer;
    anim_bind_file(state, (AnimFile *)file);
    table = player_anim_banks;
    anim_set_bank(state, (ImageBank *)(table[index] + file));
    anim_select_range(state, 0);
}

/* End the player's action: clear PLAYER_FLAG_ACTION, reload the player sprite and return to idle. */
void player_end_action(FarmPlayer *player)
{
    player->stateFlags &= ~PLAYER_FLAG_ACTION;
    farm_load_player_sprite();
    player_reset_pose();
}

/* Write the house door's position for the current house level into `out`. */
void house_door_position(VECTOR *out)
{
    u16 i = game_state.progress.estate.houseLevel;
    HouseDoorPosition *p = house_door_positions + i;
    out->vx = p->x;
    out->vz = p->y;
    out->vy = 0;
}

/* Count one more toward the player's fourth counter, up to 15. */
static inline void bump_field78(FarmState *farm)
{
    if (farm->player.counts70[4] < 15)
    {
        farm->player.counts70[4]++;
    }
}

/* The tool argument for each phase of player_apply_tool_target. */
static s16 tool_phase_arguments[10] = {4, 5, 6, 13, 9, 7, 8, 11, 10, 0};

/* Apply the latched tool to its target at step `phase` (the argument comes from
 * tool_phase_arguments). Returns 1 when a tool was handled, 0 otherwise. */
static s32 player_apply_tool_target(FarmPlayer *player, s32 phase)
{
    u32 flags = farmTargetFlags;
    s16 argument;

    if (flags & 0x80)
    {
        return 1;
    }
    if (flags & 0x400)
    {
        return 1;
    }
    if (flags & 0x2)
    {
        argument = tool_phase_arguments[phase];
        if (phase == TARGET_PHASE_HIT)
        {
            bump_field78(&game_state);
            cd_audio_play_track(10);
        }
        livestock_action_dispatch(farmTarget, argument);
        return 1;
    }
    if (flags & 0x4)
    {
        argument = tool_phase_arguments[phase];
        if (phase == TARGET_PHASE_HIT)
        {
            bump_field78(&game_state);
            cd_audio_play_track(12);
        }
        livestock_action_dispatch_b(farmTarget, argument);
        return 1;
    }
    if (flags & 0x8)
    {
        if (phase == TARGET_PHASE_HIT)
        {
            bump_field78(&game_state);
            if (*(u16 *)farmTarget == ITEM_CHICKEN)
            {
                farm_actor_dispatch_state(farmTarget, 4);
                sound_play(0x18);
            }
        }
        else if (phase == TARGET_PHASE_MEDICINE)
        {
            farm_actor_dispatch_state(farmTarget, 9);
        }
        return 1;
    }
    if (flags & 0x100)
    {
        if (phase == TARGET_PHASE_HIT)
        {
            dog_set_phase(farmTarget, DOG_PHASE_HIT);
            sound_play(0x1A);
        }
        return 1;
    }
    if (flags & 0x200)
    {
        if (phase == TARGET_PHASE_HIT)
        {
            bump_field78(&game_state);
            cd_audio_play_track(14);
        }
        else
        {
            horse_set_state(farmTarget, tool_phase_arguments[phase]);
        }
        return 1;
    }
    if (flags & 0x800)
    {
        if (phase == TARGET_PHASE_HIT && *(u16 *)farmTarget == ITEM_STRAY_DOG)
        {
            farm_action_set_countdown((u16)((player->mode - 0x48) % 5 * 10), game_state.player.facing38);
            farm_actor_set_task(farmTarget, FARM_TASK_COUNTDOWN);
            return 1;
        }
        if (phase == TARGET_PHASE_CONFIRM)
        {
            farm_actor_set_task(farmTarget, tool_phase_arguments[1]);
            return 1;
        }
    }
    return 0;
}

/* Fatigue cost of an action: `adjustment`, plus 1 when the weather is set, 1 at 22h to 23h, and
 * 2 before 6h and at zero stamina. Halved with PLAYER_FLAG_HALF_FATIGUE; 0 while fatigue is
 * locked. */
s32 player_fatigue_cost(s32 adjustment)
{
    FarmPlayer *player;
    FarmState *farm;
    u16 hour;
    s32 result;

    player = &game_state.player;
    farm = &game_state;
    result = adjustment;
    if (game_state.world.weather != 0)
    {
        result += 1;
    }
    hour = farm->progress.clock.time.hm.hour;
    if ((hour - 22) < 2U)
    {
        result += 1;
    }
    if (hour < 6U)
    {
        result += 2;
    }
    if (player->stamina == 0)
    {
        result += 2;
    }
    if (player->stateFlags & PLAYER_FLAG_HALF_FATIGUE)
    {
        result /= 2;
    }
    if (player->latches & PLAYER_LATCH_FATIGUE_LOCKED)
    {
        result = 0;
    }
    return result;
}

/* Apply the hour's status events while the player is awake. */
void player_hourly_status_tick(void)
{
    FarmPlayer *player = &game_state.player;
    if ((player->stateFlags & PLAYER_FLAG_ASLEEP) == 0)
    {
        s16 *hour_field = &game_state.progress.clock.time.hm.hour;
        FarmHourEvent *event = hour_events;
        u32 index = 0;
        do
        {
            if (*hour_field == event->value)
                farm_clamp_status(0, event->arg, 0, 0);
            index++;
            event++;
        } while (index < 4);
    }
}

/* Minutes of sleep the fatigue buys: none under 50, 120 under 70, 240 under 80, 360 under 100. */
static s32 player_sleep_minutes(FarmPlayer *player)
{
    s16 fatigue = player->fatigue;
    if (fatigue < 50)
        return 0;
    if (fatigue < 70)
        return 120;
    if (fatigue < 80)
        return 240;
    if (fatigue >= 80)
    {
        if (fatigue < 100)
            return 360;
    }
    return 0;
}

/* Stamina lost over a night: 2 per hour slept and 5 more, at most 128. */
static s32 player_sleep_stamina_loss(s32 minutes)
{
    s32 loss;
    loss = -2 * (minutes / 60) - 5;
    if (loss < -128)
    {
        loss = -128;
    }
    return loss;
}

/* Fatigue gained over a night: 7 per hour slept plus 50, at most 127. */
static s32 player_sleep_fatigue_gain(s32 value)
{
    s32 result;
    result = (value / 60) * 7 + 50;
    if (result >= 128)
        result = 127;
    return result;
}

/* Add `amount` to the experience of the held tool's kind, capped at 0xFFFF. Nothing is added
 * while PLAYER_LATCH_FATIGUE_LOCKED is set. */
static void player_add_tool_exp(FarmPlayer *player, s32 amount)
{
    if (!(player->latches & PLAYER_LATCH_FATIGUE_LOCKED))
    {
        s32 q = (player->held[HELD_TOOL].id - 1) / 5;
        amount += player->toolExp[q];
        if (amount > 0xFFFF)
            amount = 0xFFFF;
        player->toolExp[q] = amount;
    }
}

/* Record the bedtime and sleep until 5 am, for the minutes the fatigue allows. */
void player_go_to_sleep(void)
{
    FarmPlayer *player = &game_state.player;
    s32 minutes = player_sleep_minutes(player);
    u16 tmp;

    if (minutes == -1)
    {
        minutes = 0;
    }
    tmp = game_state.progress.clock.minuteOfDay;
    game_state.player.sleptAt = tmp;
    clock_sleep_until_5am(minutes);
}

/* Walk forward on its own for `frames` frames, unless the player is airborne or already walking. */
void player_walk_start(s32 frames)
{
    FarmPlayer *player = &game_state.player;
    frames <<= 16;
    frames >>= 16;
    if (!(player->stateFlags & PLAYER_FLAG_AIRBORNE))
    {
        if (!(player->stateFlags & PLAYER_FLAG_AUTO_WALK))
        {
            player->countdown = frames;
            player->stateFlags |= (PLAYER_FLAG_NO_INPUT | PLAYER_FLAG_AUTO_WALK);
            player->facingIndex = player->facing38;
            player_set_mode(player, PLAYER_MODE_POSE_2, player->facing38 + 1, -2);
            player->stateFlags &= ~PLAYER_FLAG_MODE_ANIM;
        }
    }
}

FarmHourEvent hour_events[4] = {{0, 3}, {2, 4}, {4, 5}, {6, 10}};

static u16 facing_buttons[4] = {
    0x1000,
    0x2000,
    0x4000,
    0x8000,
};

/* While auto-walking, the pad reads the one direction the walk was started in. */
static void player_autowalk_input(FarmPlayer *player)
{
    if (player->stateFlags & PLAYER_FLAG_AUTO_WALK)
    {
        farm_buttons_pressed = 0;
        farm_buttons_held = facing_buttons[player->facingIndex];
    }
}

/* Update the stamina and fatigue condition flags. Each band is latched when first
 * entered, and the last band entered picks the reaction animation. Stamina bands:
 * 0, under 1/10, 3/10 and 5/10 of the maximum. Fatigue bands: 100 (also drains
 * the event), 80 and 50 and up. Unless `quiet`, a new band plays its reaction and
 * returns 1. */
static s32 player_update_condition_flags(FarmPlayer *player, s32 quiet)
{
    s32 reaction = 0;
    s32 tenth;
    s32 maxStamina;

    maxStamina = player->maxStamina;
    tenth = maxStamina / 10;

    if (player->stamina == 0)
    {
        if (!(player->latches & PLAYER_LATCH_STAMINA_EMPTY))
        {
            player->latches |= PLAYER_LATCH_STAMINA_EMPTY;
            reaction = PLAYER_MODE_EXHAUSTED;
        }
    }
    else
    {
        player->latches &= ~PLAYER_LATCH_STAMINA_EMPTY;
    }
    if (player->stamina < tenth)
    {
        if (!(player->latches & (PLAYER_LATCH_STAMINA_EMPTY | PLAYER_LATCH_STAMINA_UNDER_10)))
        {
            player->latches |= PLAYER_LATCH_STAMINA_UNDER_10;
            reaction = PLAYER_MODE_STAMINA_CRITICAL;
        }
    }
    else
    {
        player->latches &= ~(PLAYER_LATCH_STAMINA_EMPTY | PLAYER_LATCH_STAMINA_UNDER_10);
    }
    if (player->stamina < tenth * 3)
    {
        if (!(player->latches & (PLAYER_LATCH_STAMINA_EMPTY | PLAYER_LATCH_STAMINA_UNDER_10 | PLAYER_LATCH_STAMINA_UNDER_30)))
        {
            player->latches |= PLAYER_LATCH_STAMINA_UNDER_30;
            reaction = PLAYER_MODE_STAMINA_LOW;
        }
    }
    else
    {
        player->latches &= ~(PLAYER_LATCH_STAMINA_EMPTY | PLAYER_LATCH_STAMINA_UNDER_10 | PLAYER_LATCH_STAMINA_UNDER_30);
    }
    if (player->stamina < tenth * 5)
    {
        if (!(player->latches & (PLAYER_LATCH_STAMINA_EMPTY | PLAYER_LATCH_STAMINA_UNDER_10 | PLAYER_LATCH_STAMINA_UNDER_30 | PLAYER_LATCH_STAMINA_UNDER_50)))
        {
            player->latches |= PLAYER_LATCH_STAMINA_UNDER_50;
            reaction = PLAYER_MODE_STAMINA_HALF;
        }
    }
    else
    {
        player->latches &= ~(PLAYER_LATCH_STAMINA_EMPTY | PLAYER_LATCH_STAMINA_UNDER_10 | PLAYER_LATCH_STAMINA_UNDER_30 | PLAYER_LATCH_STAMINA_UNDER_50);
    }
    if (player->fatigue == 100)
    {
        if (!(player->latches & PLAYER_LATCH_FATIGUE_MAX))
        {
            player->latches |= PLAYER_LATCH_FATIGUE_MAX;
            reaction = PLAYER_MODE_EXHAUSTED;
            event_request_drain();
        }
    }
    else
    {
        player->latches &= ~PLAYER_LATCH_FATIGUE_MAX;
    }
    if (player->fatigue >= 80)
    {
        if (!(player->latches & (PLAYER_LATCH_FATIGUE_MAX | PLAYER_LATCH_FATIGUE_80)))
        {
            player->latches |= PLAYER_LATCH_FATIGUE_80;
            reaction = PLAYER_MODE_FATIGUE_80;
        }
    }
    else
    {
        player->latches &= ~(PLAYER_LATCH_FATIGUE_MAX | PLAYER_LATCH_FATIGUE_80);
    }
    if (player->fatigue >= 50)
    {
        if (!(player->latches & (PLAYER_LATCH_FATIGUE_MAX | PLAYER_LATCH_FATIGUE_80 | PLAYER_LATCH_FATIGUE_50)))
        {
            player->latches |= PLAYER_LATCH_FATIGUE_50;
            reaction = PLAYER_MODE_FATIGUE_50;
        }
    }
    else
    {
        player->latches &= ~(PLAYER_LATCH_FATIGUE_MAX | PLAYER_LATCH_FATIGUE_80 | PLAYER_LATCH_FATIGUE_50);
    }
    if (quiet == 0 && reaction != 0)
    {
        player->stateFlags |= (PLAYER_FLAG_ACTION_START | PLAYER_FLAG_BIT14);
        player_set_mode(player, reaction, 0, -2);
        return 1;
    }
    return 0;
}

/* Check the zone under `player` for an action event and start it. Returns 0
 * when nothing may happen, 1 when an event started, 2 to fall through to the
 * ordinary action. */
s32 player_zone_action(FarmPlayer *player)
{
    FarmZone *zone;
    s32 refusal;
    s32 message;
    s16 mode;
    s32 result;
    u32 flags;

    if (game_state.globalFlags & GLOBAL_FLAG_PAUSED)
    {
        return 2;
    }
    flags = player->stateFlags;
    if (!(flags & (PLAYER_FLAG_AIRBORNE | PLAYER_FLAG_AUTO_WALK)))
    {
        if (flags & PLAYER_FLAG_MODE_ANIM)
        {
            return 0;
        }
    }
    if (flags & (PLAYER_FLAG_FISHING | PLAYER_FLAG_ITEM_IN_FLIGHT))
    {
        return 0;
    }
    if (input_step_result == 2)
    {
        return 0;
    }
    if (game_state.debugFlags & DEBUG_FLAG_ENTER_ARTR)
    {
        return 2;
    }
    zone = farm_zone_find(&farm_area.mode.script, &player->position, 1);
    current_zone = zone;
    if (zone == 0)
    {
        return 2;
    }
    mode = (player->stateFlags & (PLAYER_FLAG_NO_INPUT | PLAYER_FLAG_RIDING)) == 0;
    if (player->stateFlags & PLAYER_FLAG_SCRIPTED)
    {
        mode = 1;
    }
    if (player->stateFlags & PLAYER_FLAG_RIDING)
    {
        if (zone->event == 3)
        {
            mode = 2;
        }
        if (zone->id == 0)
        {
            mode = 2;
        }
    }
    if (mode != 0)
    {
        zone = current_zone;
        if (zone->facing == 999 || (zone->flags & (ZONE_FLAG_FACING | ZONE_FLAG_HAS_EVENT)) != ZONE_FLAG_FACING)
        {
            if ((zone->flags & ZONE_FLAG_HAS_EVENT) && zone->event != ZONE_EVENT_NONE &&
                event_pending_pair_lookup(zone->event, zone->facing, &refusal, 0) != 1)
            {
                if (refusal != -1)
                {
                    player->facing38 = current_zone->facing;
                    menu_request_open(2, refusal);
                    player_walk_start(20);
                    player_reset_pose();
                }
                return 0;
            }
            if (event_flag_get(EVENT_FLAG_WEATHER_STORM) == 1 || event_flag_get(EVENT_FLAG_WEATHER_BLIZZARD) == 1)
            {
                return 2;
            }
            farm_area.mode.eventMode = FARM_MODE_EXIT_LOAD;
            player->stateFlags |= PLAYER_FLAG_SCENE_CHANGE;
            player_reset_pose();
            return 1;
        }
        if (player_status_word & 0x2000)
        {
            if (zone->facing != player->facing38)
            {
                return 2;
            }
            if (!(farm_buttons_pressed & PAD_CROSS))
            {
                return 0;
            }
        }
        else if (zone->event != 0x2F)
        {
            if (mode != 2)
            {
                if (!(zone->flags & ZONE_FLAG_HAS_EVENT) && !(farm_buttons_pressed & PAD_CROSS))
                {
                    return zone->facing != player->facing38 ? 2 : 0;
                }
            }
            else if (current_zone->facing != player->facing38)
            {
                return 2;
            }
        }
        if (current_zone->facing != player->facing38)
        {
            return 2;
        }
        message = 0;
        if (current_zone->event == ZONE_EVENT_NONE)
        {
            return 1;
        }
        if (current_zone->event == 0x2E && (player->stateFlags & (PLAYER_FLAG_SHOWING_ITEM | PLAYER_FLAG_CARRY_BIG_ITEM | PLAYER_FLAG_CARRY_ITEM)))
        {
            return 0;
        }
        result = event_pending_pair_lookup(current_zone->event, current_zone->facing, &message, 0);
        if (result == 1)
        {
            pad_vibration_start(0, 2);
            farm_area.mode.eventMode = FARM_MODE_ZONE_WALK;
            player_walk_start(10);
            player_reset_pose();
            if (current_zone->id == 0xFFFE)
            {
                return 1;
            }
            event_flag_set(EVENT_FLAG_EVENT_ACTIVE, 1);
            return 1;
        }
        if (message != -1)
        {
            menu_request_open(2, message);
        }
        player_walk_start(5);
        player_reset_pose();
        return 0;
    }
    else
    {
        if (current_zone->facing != player->facing38)
        {
            return 2;
        }
        player_walk_start(10);
        return 2;
    }
}

/* Start fishing: set the fishing state, throw the bobber and pick its frame (0x1C with a fishing
 * pole, 0x1B with the rod). */
static s32 player_start_fishing(FarmPlayer *player)
{
    FarmStoredItem lure;
    player->stateFlags |= (PLAYER_FLAG_FISHING | PLAYER_FLAG_FISHING_LINE | PLAYER_FLAG_ITEM_IN_FLIGHT | PLAYER_FLAG_ACTION_START);
    player_set_mode(player, PLAYER_MODE_FISHING, 0, -2);
    anim_select_range(&player->secondSprite, player->facing38);
    sound_play(0xF);
    lure.id = ITEM_FISHING_MARKER;
    lure.modifier = 0;
    lure.count = 1;
    thrown_item_spawn(&player->position, &lure, player->facing38);
    fishing_marker_origin.x = 0;
    fishing_marker_origin.y = 0;
    fishing_marker_origin.z = 0;
    fishing_marker_value = 0x1B;
    if (player->held[HELD_TOOL].id == ITEM_FISHING_POLE)
    {
        fishing_marker_value = 0x1C;
    }
    fishing_marker_frame = 0;
    fishing_marker_active = 0;
    return 1;
}

/* Set the trail to the player's position and bind the toys' sprites. */
s32 player_init_toys(FarmPlayer *player)
{
    u32 i;
    u8 *file_base;
    AnimPackToc **table;
    AnimPackToc *entry;
    s32 file_offset;

    i = 0;
    do
    {
        player->trail[i].vx = player->position.vx;
        player->trail[i].vy = player->position.vy;
        player->trail[i].vz = player->position.vz;
        i++;
    } while (i < TRAIL_LENGTH);

    i = 0;
    file_base = farm_area.slotSixArt;
    table = farm_slot_tocs;
    player->chickToy = 0;
    entry = table[6];
    file_offset = ANIM_PACK_START(entry, 0);
    do
    {
        anim_bind_file(&player->toys[i].sprite, (void *)(file_offset + (s32)file_base));
        i++;
    } while (i < PLAYER_TOYS);

    return 0;
}

static s32 player_draw_toys(FarmPlayer *player)
{
    u32 i = 0;
    do
    {
        player_draw_toy(&player->trail[i], player, 0, i);
        i++;
    } while (i < PLAYER_TOYS);
    return 0;
}

/* Put the pose trail back to the player's position with no animation. */
s32 player_reset_trail(FarmPlayer *player)
{
    u32 i = 0;

    do
    {
        player->poses[i].animation = 5;
        player->poses[i].facing = i & 1;
        player->slowTrail[i] = player->position;
        i++;
    } while (i < TRAIL_LENGTH);
    return 0;
}

static s32 player_draw_slow_trail(FarmPlayer *player)
{
    u32 i = 0;
    s32 skip_animation = 5;
    do
    {
        if (player->poses[i].animation != skip_animation)
            player_draw_toy(&player->slowTrail[i], player, 1, i);
        i++;
    } while (i < TRAIL_LENGTH);
    return 0;
}

/* Copy the player's position into `output`, moved by the facing's step: a row further
 * on for mode 2, half the step for mode 4. */
static void player_facing_point(FarmPlayer *player, VECTOR *output, s32 mode)
{
    FarmStep *row;

    row = &facing_steps[player->facing38];
    if (mode == 2)
    {
        row = &facing_steps[player->facing38 + 4];
    }
    output->vx = player->position.vx;
    output->vy = player->position.vy;
    output->vz = player->position.vz;
    if (mode == 4)
    {
        output->vx += row->x / 2;
        output->vz += row->z / 2;
    }
    else
    {
        output->vx += row->x;
        output->vz += row->z;
    }
}

/* Draw one toy at `point`, or one pose of the slow trail when `mode` is nonzero. */
static void player_draw_toy(VECTOR *point, FarmPlayer *player, s32 mode, s32 slot)
{
    SVECTOR input;
    FarmScreenPoint screen;
    AnimState *item;
    s16 animation;
    s16 frame;

    if (mode != 0)
    {
        item = &winter_anim_state;
        animation = player->poses[slot].animation;
        frame = player->poses[slot].facing;
    }
    else
    {
        item = &player->toys[0].sprite;
        animation = item->animation;
        frame = item->frame;
    }
    input.vx = point->vx + 150;
    input.vy = point->vy - 100;
    input.vz = point->vz - 150;
    if (estate_point_project(&input, &screen) != 0)
    {
        sprite_draw_traced(&current_frame->ot[screen.depth >> 2], current_frame->primCursor.ft4, item,
                           animation & 0xFF, frame & 0xFF, screen.x, screen.y, item->scale, 0, 0x40);
    }
    if (mode == 0 && (game_state.globalFlags & GLOBAL_FLAG_PAUSED) == 0)
    {
        anim_tick(item);
    }
}

/* Start the result scene: the player faces down, and the camera points come from the
 * prepared result. */
void player_start_result_scene(FarmPlayer *player)
{
    u32 posX;
    u32 posY;
    u32 posZ;

    player->stateFlags |= (PLAYER_FLAG_ACTION_START | PLAYER_FLAG_MODE_ANIM);
    player->facing38 = FACING_DOWN;
    player_set_mode(player, PLAYER_MODE_GO_TO_BED, 0, -2);
    event_flag_set(EVENT_FLAG_09, 1);
    player_result_camera_points(result_camera_start);
    posX = player->position.vx;
    result_camera_end[0] = posX;
    posY = player->position.vy;
    result_camera_end[1] = posY;
    posZ = player->position.vz;
    result_camera_start[0] -= 550;
    result_camera_start[1] = posY;
    result_camera_start[2] += 440;
    result_camera_end[2] = posZ;
    game_state.globalFlags |= GLOBAL_FLAG_CAMERA_BUSY;
    result_camera_frame = 0;
    game_state.player.stateFlags |= PLAYER_FLAG_BIT14;
}

static ResultCameraTable result_camera_pairs = {{{-800, -1400}, {-2400, -1400}, {-2400, -3000}}};

/* Pick the camera's start and end points for a result scene from its scene number. */
static void player_result_camera_points(s32 *result)
{
    s32 house;
    switch (farm_area.mode.data->head.scene)
    {
    default:
    case AREA_HOUSE0_SPRING:
    case AREA_HOUSE0_SUMMER:
    case AREA_HOUSE0_FALL:
    case AREA_HOUSE0_WINTER:
        house = 0;
        break;
    case AREA_HOUSE1_SPRING:
    case AREA_HOUSE1_SUMMER:
    case AREA_HOUSE1_FALL:
    case AREA_HOUSE1_WINTER:
        house = 1;
        break;
    case AREA_HOUSE2_SPRING:
    case AREA_HOUSE2_SUMMER:
    case AREA_HOUSE2_FALL:
    case AREA_HOUSE2_WINTER:
        house = 2;
        break;
    }
    result[0] = result_camera_pairs.pairs[house].first;
    result[1] = 0;
    result[2] = result_camera_pairs.pairs[house].second;
}

/* Move the camera one step along its way to the player: the camera position is
 * interpolated, and the player follows it. Returns 1 when the camera has arrived. */
static s32 player_result_camera_step(FarmPlayer *player)
{
    s32 output[3];
    s32 lower;
    s32 distance;
    s32 offset;
    s32 next;

    math_lerp_vec3(result_camera_frame, 17, result_camera_end, result_camera_start, output);
    lower = result_camera_end[1] - 40;
    farm_area.mode.scroll.centre.vx = output[0];
    farm_area.mode.scroll.centre.vz = output[2];
    if (lower < output[1])
        lower = output[1];
    farm_area.mode.scroll.centre.vy = lower;

    if (result_camera_frame < 9)
        distance = 8 - result_camera_frame;
    else
        distance = result_camera_frame - 8;
    offset = distance * 111;
    offset /= 8;
    if (result_camera_frame < 9)
        next = player->position.vy - offset;
    else
        next = player->position.vy + offset;
    if (next >= result_camera_end[1] - 40)
        next = result_camera_end[1] - 40;

    if (result_camera_frame >= 17)
    {
        game_state.globalFlags &= ~GLOBAL_FLAG_CAMERA_BUSY;
        player->position.vx = output[0];
        player->position.vy = result_camera_end[1] - 40;
        player->position.vz = output[2];
        return 1;
    }
    player->position.vy = next;
    player->position.vx = output[0];
    player->position.vz = output[2];
    result_camera_frame++;
    return 0;
}

/* Whether a kind is one of the six fish. */
static s32 item_is_fish(s32 kind)
{
    return ((s16)kind - ITEM_CARP) < 6U;
}

/* The print item of a fish kind. */
static s16 item_fish_print_id(s16 kind)
{
    return kind - (ITEM_CARP - ITEM_CARP_PRINT);
}

/* Whether a kind is one of the six fish prints. */
static s32 item_is_print(s16 kind)
{
    return (kind - ITEM_CARP_PRINT) < 6U;
}

static s16 ground_tolerances[6] = {
    4,
    64,
    320,
    960,
    1720,
    25134,
};

/* Try the ground tolerances in turn; the first that settles on the ground sets the
 * probe's height. Returns 1 when none did. */
static s32 player_settle_ground_tolerance(FarmGroundProbe *input)
{
    VECTOR work;
    u32 attempt;
    s32 failed;

    failed = 1;
    work.vx = input->x;
    work.vy = input->y;
    work.vz = input->z;
    for (attempt = 0; attempt < 5; attempt++)
    {
        ground_set_tolerance(ground_tolerances[attempt]);
        if (farm_settle_on_ground(&work, 0) == 0)
        {
            failed = 0;
            input->y = work.vy;
            break;
        }
    }
    ground_set_tolerance(GROUND_TOLERANCE_DEFAULT);
    return failed;
}

/* Bind the winter art when the season is winter; winter_anim_loaded says whether it bound. */
void player_load_winter_anim(void)
{
    void *value;

    winter_anim_loaded = 0;
    if (game_state.progress.clock.date.season == SEASON_WINTER)
    {
        value = item_art_address(0x187);
        if (value != 0)
        {
            winter_anim_loaded = 1;
            anim_bind_file(&winter_anim_state, value);
        }
    }
}

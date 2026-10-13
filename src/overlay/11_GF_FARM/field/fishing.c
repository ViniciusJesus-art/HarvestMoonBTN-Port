/* Fishing for the player: the cast, the wait for a bite, the pull, and the catch,
 * with the marker drawn over the water. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "types.h"
#include "anim.h"
#include "clock.h"
#include "estate.h"
#include "event_flags.h"
#include "farm_load.h"
#include "farm_state.h"
#include "field.h"
#include "fishing.h"
#include "item_storage.h"
#include "items.h"
#include "main.h"
#include "model.h"
#include "pad.h"
#include "pad_vibration.h"
#include "picture_cells.h"
#include "player.h"
#include "scene.h"
#include "scene_view.h"
#include "screen_fade.h"
#include "sound_request.h"
#include "ui_icons.h"

#define SPOT (farm_area.mode.data->head.place.location)

#define FISHING_NO_CATCH (-1)
#define FISHING_STREAK_LIMIT 11
/* The angler is caught only outside 8:00 to 22:00 (minute 480 to 1321). */
#define FISHING_OPEN 480
#define FISHING_CLOSE 1321
#define FISHING_BITE_CHANCE 10
#define FISHING_POLE_BITE_CHANCE 20
#define FISHING_MARKER_BOB_DIV 1024
#define FISHING_MARKER_PHASE_STEP 22
#define FISHING_MARKER_PHASE_WRAP 4096
#define CELL_TEXTURE_SIZE 32
#define CELL_TEXTURE_V_BASE 256
#define CELL_TEXTURE_PAGE 0x1F
#define CELL_ENTRY_COL_MASK 0xE00
#define CELL_ENTRY_ROW_MASK 0x1C0
#define CELL_ENTRY_CLUT_ROW_MASK 0x1F
#define CELL_CLUT_X 960
#define CELL_CLUT_Y 480

static s16 fishing_marker_phase;
static SVECTOR fishing_marker_vertex;
static FarmScreenXY fishing_marker_screen;
static u8 fishing_cell_texture[16];
static FarmFishing fishing_state;
static FarmOverviewCamera overview_camera;
s32 fishing_unused_8012DB70[2];
FishingMarkerPoint fishing_marker_origin;
s32 fishing_marker_active;
s32 fishing_marker_value;
s32 fishing_marker_frame;
s32 fishing_streak;

/* The fishing states of player.mode. */
enum FishingStep {
    FISHING_WAIT_BITE = 0,
    FISHING_HOOKED = 1,
    FISHING_REEL_IN = 2,
    FISHING_CATCH = 3,
    FISHING_CAST = PLAYER_MODE_FISHING,     /* the mode when a cast starts */
};

/* One frame of fishing for the player. The cast (FISHING_CAST) picks the spot kind
 * from the map. Then the player waits for a bite (FISHING_WAIT_BITE; the chance is
 * doubled with a fishing pole), pulls (FISHING_HOOKED, the pull count runs down while
 * Square is held), and winds up the cast (FISHING_REEL_IN on a miss, FISHING_CATCH to
 * land the catch). Returns 0. */
s32 fishing_player_frame(FarmPlayer *player)
{
    FarmFishing *fishing;
    u16 buttons;
    s32 biteChance;
    FarmStoredItem caught;

    fishing = &fishing_state;
    buttons = farm_buttons_held;
    switch (player->mode) {
    case FISHING_CAST:
        if (player->stateFlags & PLAYER_FLAG_ITEM_IN_FLIGHT) {
            return 0;
        }
        player->mode = FISHING_WAIT_BITE;
        fishing->kind = 0;
        if (!(player->stateFlags & PLAYER_FLAG_AUTO_WALK)) {
            farm_clamp_status(-1, player_fatigue_cost(0), 0, 0);
        }
        fishing->kind = 4;
        if (SPOT == PLACE_2C) {
            fishing->kind = 0;
        }
        if (SPOT == PLACE_D) {
            fishing->kind = 1;
        }
        if (SPOT == PLACE_C) {
            fishing->kind = 2;
        }
        if (SPOT == PLACE_42) {
            fishing->kind = 3;
        }
        fishing->wait = fishing_bite_wait(fishing);
        if (fishing_marker_active == 0) {
            fishing->kind = FISHING_NO_CATCH;
            player->mode = FISHING_CATCH;
            anim_select_range(&player->secondSprite, player->facing38 + 4);
            pad_vibration_stop(0);
            sound_play(0xF);
        } else {
            fishing_marker_active = 0;
            pad_vibration_start(0, 6);
        }
        break;
    case FISHING_WAIT_BITE:
        if (player->stateFlags & PLAYER_FLAG_ITEM_IN_FLIGHT) {
            break;
        }
        if (!(buttons & PAD_SQUARE)) {
            player->mode = FISHING_REEL_IN;
            anim_select_range(&player->secondSprite, player->facing38 + 4);
            sound_play(0xF);
            break;
        }
        if (fishing->wait == 0) {
            biteChance = FISHING_BITE_CHANCE;
            if (player->held[HELD_TOOL].id == ITEM_FISHING_POLE) {
                biteChance = FISHING_POLE_BITE_CHANCE;
            }
            if (biteChance * 2 > rand_range(100)) {
                player->mode = FISHING_HOOKED;
                anim_select_range(&player->secondSprite, player->facing38 + 9);
                pad_vibration_start(0, 0);
                break;
            }
            fishing->wait = fishing_bite_wait(fishing);
        } else if (fishing->kind != FISHING_NO_CATCH) {
            fishing->wait--;
        }
        anim_select_range(&player->secondSprite, 8);
        player->secondSprite.frame = player->facing38;
        fishing_draw_marker(0);
        break;
    case FISHING_REEL_IN:
        if (player->secondSprite.flags & ANIM_FLAG_ENDED) {
            player->stateFlags &= ~(PLAYER_FLAG_FISHING | PLAYER_FLAG_FISHING_LINE);
            player_end_action(player);
        }
        break;
    case FISHING_CATCH:
        if (player->secondSprite.flags & ANIM_FLAG_ENDED) {
            caught.count = 1;
            player->stateFlags &= ~(PLAYER_FLAG_FISHING | PLAYER_FLAG_FISHING_LINE);
            player_end_action(player);
            if (fishing->kind != FISHING_NO_CATCH) {
                caught.id = fishing_roll_catch(fishing);
                if (caught.id != 0) {
                    player_show_item(&caught);
                }
            }
        }
        break;
    case FISHING_HOOKED:
        if (!(buttons & PAD_SQUARE)) {
            player->mode = FISHING_CATCH;
            anim_select_range(&player->secondSprite, player->facing38 + 4);
            pad_vibration_stop(0);
            sound_play(0xF);
        } else if (fishing_state.pull == 0) {
            player->mode = FISHING_WAIT_BITE;
            fishing->wait = fishing_bite_wait(fishing);
            pad_vibration_stop(0);
        } else {
            fishing_state.pull--;
            fishing_draw_marker(1);
        }
        break;
    }
    return 0;
}

typedef struct {
    s16 low;
    s16 high;
    s16 value;
    s16 pad06[4];
} FishingTimeRange;

static FishingTimeRange fishing_bite_ranges[4] = {
    1320, 1439, 15, 60, 60, 150, 30, 0, 479, 15,
    60, 60, 150, 30, 480, 1079, 60, 180, 180, 210,
    90, 1080, 1319, 30, 120, 120, 210, 60,
};

static u16 fishing_bite_amounts[82] = {
    0x0019, 0x0019, 0x0019, 0x000A, 0x0019, 0x0000, 0x000A, 0x0005, 0x0000, 0x0014, 0x00EB, 0x000F,
    0x00EC, 0x0019, 0x00E9, 0x0005, 0x003E, 0x0005, 0x00A3, 0x0005, 0x00A2, 0x000A, 0x00A1, 0x0005,
    0x00EA, 0x0005, 0x00F2, 0x0000, 0x0004, 0x001E, 0x00EB, 0x000A, 0x00EC, 0x0028, 0x00E9, 0x0014,
    0x00F0, 0x0000, 0x0009, 0x000F, 0x0000, 0x0014, 0x00EB, 0x000F, 0x00EC, 0x0014, 0x00E9, 0x0005,
    0x003E, 0x0005, 0x00A3, 0x0005, 0x00A2, 0x000A, 0x00A1, 0x0005, 0x00F2, 0x0000, 0x000B, 0x0005,
    0x0000, 0x0014, 0x00EB, 0x000F, 0x00EC, 0x0014, 0x00E9, 0x0005, 0x003E, 0x0005, 0x00A3, 0x0005,
    0x00A2, 0x000A, 0x00A1, 0x0005, 0x0067, 0x0005, 0x00F1, 0x0005, 0x00F2, 0x0000,
};

/* Roll the bite wait for the current minute of day, from the time range that holds
 * it and the row for the spot kind. The pull count is rolled here too, into
 * fishing->pull. Returns the wait, or 0 when no range holds the minute. */
s32 fishing_bite_wait(FarmFishing *fishing) {
    FishingTimeRange *range;
    u16 *amounts;
    u32 slot;
    s32 value;
    range = fishing_bite_ranges;
    value = game_state.progress.clock.minuteOfDay;
    slot = 0;
    amounts = fishing_bite_amounts;
    while (slot < 4) {
        if (value < range->low) {
            slot++;
        } else if (range->high < value) {
            slot++;
        } else {
            value = rand_range(amounts[fishing->kind]);
            fishing->pull = value + 5;
            value = fishing->kind;
            return (&range->value)[value];
        }
        range++;
    }

    return 0;
}

static s16 fishing_spot_catches_1[20] = {
    9, 10, 0, 5, ITEM_BOOT, 10, ITEM_DEAD_FISH, 5, ITEM_CAN, 15,
    ITEM_BRANCH, 35, ITEM_SMALL_FISH, 15, ITEM_MEDIUM_FISH, 0, ITEM_LARGE_FISH, 5, ITEM_CARP, 0,
};

static s16 fishing_spot_catches_3[8] = {3, 10, 0, 70, ITEM_DEAD_FISH, 20, ITEM_CATFISH, 0};

static s16 fishing_spot_catches_2[18] = {
    8, 20, 0, 5, ITEM_BOOT, 10, ITEM_DEAD_FISH, 10, ITEM_CAN, 20, ITEM_BRANCH, 20,
    ITEM_SMALL_FISH, 5, ITEM_MEDIUM_FISH, 10, ITEM_CHAR, 0,
};

static s16 fishing_spot_catches_4[14] = {
    6, 30, 0, 10, ITEM_BOOT, 10, ITEM_DEAD_FISH, 10, ITEM_CAN, 20, ITEM_BRANCH, 20, ITEM_SMALL_FISH, 0,
};

/* Catch tables for a spot: a count, then {weight, item} pairs (weights in percent). */
static s16 *fishing_season_catches[4] = {
    (s16 *)&fishing_bite_amounts[6], (s16 *)&fishing_bite_amounts[28], (s16 *)&fishing_bite_amounts[38], (s16 *)&fishing_bite_amounts[58],
};

/* Catch given after a run of 11 ordinary catches, indexed by spot kind. */
static s16 fishing_fallback_catch[6] = {
    ITEM_LARGE_FISH, ITEM_MEDIUM_FISH, ITEM_SMALL_FISH, 0, ITEM_SMALL_FISH, 0,
};

/* Roll the catch for the fishing spot. Walk the spot's weighted table (kind 0 uses
 * the season's table) until the running weight passes a 0..99 roll. A special catch
 * (the fried fish recipe, power berry, carp, squid, angler, sea bream, catfish, char)
 * is taken once: a refused one moves on to the next entry, and an accepted one sets
 * its event flag and, for most, an estate catch flag. The three fish reset the streak
 * counter; other items count it up to FISHING_STREAK_LIMIT, after which the spot's
 * fallback catch is returned. Returns the item, or 0. */
s16 fishing_roll_catch(FarmFishing *fishing)
{
    s16 *table;
    s32 count;
    s32 roll;
    s32 sum;
    s32 i;

    table = fishing_season_catches[game_state.progress.clock.date.season];
    switch (fishing->kind) {
    case 1:
        table = fishing_spot_catches_1;
        break;
    case 2:
        table = fishing_spot_catches_2;
        break;
    case 3:
        table = fishing_spot_catches_3;
        break;
    case 4:
        table = fishing_spot_catches_4;
        break;
    }
    count = *table++;
    sum = 0;
    roll = rand_range(100);
    for (i = 0; i < count; i++, table += 2) {
        sum += table[0];
        if (roll >= sum) {
            continue;
        }
        switch (table[1]) {
        case ITEM_FRIES_RECIPE:
            if (event_flag_get(EVENT_FLAG_FISHED_0EA) == 1) {
                continue;
            }
            event_flag_set(EVENT_FLAG_FISHED_0EA, 1);
            return table[1];
        case ITEM_POWER_BERRY:
            if (event_flag_get(EVENT_FLAG_FISHED_POWER_BERRY) == 1) {
                continue;
            }
            event_flag_set(EVENT_FLAG_FISHED_POWER_BERRY, 1);
            return table[1];
        case ITEM_CARP:
            if (event_flag_get(EVENT_FLAG_FISHED_0ED) == 1) {
                continue;
            }
            if (event_flag_get(EVENT_FLAG_FISHED_0EE) != 1 || event_flag_get(EVENT_FLAG_FISHED_CATFISH) != 1 ||
                event_flag_get(EVENT_FLAG_FISHED_0F0) != 1 || event_flag_get(EVENT_FLAG_FISHED_0F1) != 1 ||
                event_flag_get(EVENT_FLAG_FISHED_0F2) != 1) {
                continue;
            }
            event_flag_set(EVENT_FLAG_FISHED_0ED, 1);
            game_state.progress.estate.estateFlags |= ESTATE_FLAG_CATCH_ED;
            break;
        case ITEM_SQUID:
            if (event_flag_get(EVENT_FLAG_FISHED_0F0) == 1) {
                continue;
            }
            if (event_flag_get(EVENT_FLAG_SMALL_FISH_PLACED) != 1) {
                continue;
            }
            event_flag_set(EVENT_FLAG_FISHED_0F0, 1);
            game_state.progress.estate.estateFlags |= ESTATE_FLAG_CATCH_F0;
            break;
        case ITEM_ANGLER:
            if (event_flag_get(EVENT_FLAG_FISHED_0F1) == 1) {
                continue;
            }
            if ((u16)(game_state.progress.clock.minuteOfDay - FISHING_OPEN) < FISHING_CLOSE - FISHING_OPEN) {
                continue;
            }
            event_flag_set(EVENT_FLAG_FISHED_0F1, 1);
            game_state.progress.estate.estateFlags |= ESTATE_FLAG_CATCH_F1;
            break;
        case ITEM_SEA_BREAM:
            if (event_flag_get(EVENT_FLAG_FISHED_0F2) == 1) {
                continue;
            }
            if (event_flag_get(EVENT_FLAG_FISH_SHIPPED_200) != 1) {
                continue;
            }
            event_flag_set(EVENT_FLAG_FISHED_0F2, 1);
            game_state.progress.estate.estateFlags |= ESTATE_FLAG_CATCH_F2;
            break;
        case ITEM_CATFISH:
            if (event_flag_get(EVENT_FLAG_FISHED_CATFISH) != 0) {
                continue;
            }
            event_flag_set(EVENT_FLAG_FISHED_CATFISH, 1);
            game_state.progress.estate.estateFlags |= ESTATE_FLAG_CATCH_EF;
            break;
        case ITEM_CHAR:
            if (event_flag_get(EVENT_FLAG_FISHED_0EE) == 1) {
                continue;
            }
            if (event_flag_get(EVENT_FLAG_COOKED_SASHIMI) != 1 || event_flag_get(EVENT_FLAG_COOKED_GRILLED_FISH) != 1 ||
                event_flag_get(EVENT_FLAG_COOKED_SUSHI) != 1) {
                continue;
            }
            event_flag_set(EVENT_FLAG_FISHED_0EE, 1);
            game_state.progress.estate.estateFlags |= ESTATE_FLAG_CATCH_EE;
            return table[1];
        default:
            if ((u16)(table[1] - ITEM_LARGE_FISH) < 3) {
                fishing_streak = 0;
                break;
            }
            if (fishing_streak < FISHING_STREAK_LIMIT) {
                fishing_streak++;
                break;
            }
            fishing_streak = 0;
            return fishing_fallback_catch[fishing->kind];
        }
        return table[1];
    }
    return 0;
}

/* Draw the fishing marker over the water, offset from its origin by (150, -100, -150).
 * When not animating it bobs on a sine of its phase, which advances each frame and
 * wraps at 4096. When animating, its frame counter advances instead. */
void fishing_draw_marker(s32 animate)
{
    s32 bob;
    s32 depth;

    if (animate == 0) {
        bob = (s16)(csin(fishing_marker_phase) / FISHING_MARKER_BOB_DIV);
        fishing_marker_phase += FISHING_MARKER_PHASE_STEP;
        fishing_marker_phase = fishing_marker_phase - fishing_marker_phase / FISHING_MARKER_PHASE_WRAP * FISHING_MARKER_PHASE_WRAP;
    } else {
        bob = 0;
    }
    fishing_marker_vertex.vx = fishing_marker_origin.x + 150;
    fishing_marker_vertex.vy = fishing_marker_origin.y - 100;
    fishing_marker_vertex.vz = fishing_marker_origin.z - 150;
    depth = gte_project_vertex(&fishing_marker_screen, &fishing_marker_vertex);
    if (depth == 0) {
        return;
    }
    if (animate != 0) {
        fishing_marker_frame++;
    }
    anim_draw_sprt_cursor(&current_frame->ot[depth >> 2], &ui_icon_anim, fishing_marker_value, 0,
                          fishing_marker_screen.x, bob + (fishing_marker_screen.y + (fishing_marker_frame & 3)));
}

/* Switch the farm view to the overview and start its camera at set-up. */
void farm_overview_start(void) {
    farm_area.mode.view = FARM_VIEW_OVERVIEW;
    farm_area.mode.eventMode = FARM_MODE_OVERVIEW;
    overview_camera.step = OVERVIEW_CAMERA_SET_UP;
}

/* While the overview is up, move the scroll height by the drift, which loses 4 each frame. */
void farm_overview_drift(void) {
    if (farm_area.mode.eventMode != FARM_MODE_OVERVIEW) return;
    overview_camera.driftY += overview_camera.driftSpeed;
    farm_area.mode.scroll.centre.vy += overview_camera.driftY;
    overview_camera.driftSpeed -= 4;
}

/* The farm overview camera, run as a sequence of steps: set the camera up
 * (step 0), rebuild the cell textures (1), move it with the d-pad inside
 * x -999..10000 / z -7399..600 in 200-unit steps (2, until Circle is pressed),
 * then fade out and restore the farm view (3). */
void farm_overview_camera(void)
{
    s32 coord;

    switch (overview_camera.step) {
    case OVERVIEW_CAMERA_SET_UP:
        farm_area.mode.scroll.z = -2926;
        farm_area.mode.scroll.x = -29780;
        farm_area.mode.scroll.yaw = 0;
        game_state.globalFlags |= GLOBAL_FLAG_TRANSITION;
        overview_camera.step++;
        break;
    case OVERVIEW_CAMERA_RETEXTURE:
        farm_load_texture_block();
        farm_retexture_all_cells();
        fade_start_in(10, 0, 2);
        overview_camera.step++;
        game_state.globalFlags &= ~GLOBAL_FLAG_TRANSITION;
        break;
    case OVERVIEW_CAMERA_MOVE:
        if (input_pad1->held & PAD_UP) {
            coord = overview_camera.cameraZ;
            if (coord < 400) {
                coord += 200;
            }
            overview_camera.cameraZ = coord;
        }
        if (input_pad1->held & PAD_DOWN) {
            coord = overview_camera.cameraZ;
            if (coord >= -7199) {
                coord -= 200;
            }
            overview_camera.cameraZ = coord;
        }
        if (input_pad1->held & PAD_LEFT) {
            coord = overview_camera.cameraX;
            if (coord >= -799) {
                coord -= 200;
            }
            overview_camera.cameraX = coord;
        }
        if (input_pad1->held & PAD_RIGHT) {
            coord = overview_camera.cameraX;
            if (coord < 10000) {
                coord += 200;
            }
            overview_camera.cameraX = coord;
        }
        farm_area.mode.scroll.centre.vx = overview_camera.cameraX;
        farm_area.mode.scroll.centre.vz = overview_camera.cameraZ;
        if (input_pad1->pressed & PAD_CIRCLE) {
            fade_start_out(10, 0, 2);
            overview_camera.step = OVERVIEW_CAMERA_FADE_OUT;
        }
        break;
    case OVERVIEW_CAMERA_FADE_OUT:
        if (fade_is_settled() == 1) {
            fade_start_in(10, 0, 2);
            farm_scroll_reset();
            picture_cells_tick();
            farm_area.mode.view = FARM_VIEW_NORMAL;
            game_state.globalFlags |= GLOBAL_FLAG_TRANSITION;
        }
        break;
    }
}

/* Texture entry for each tile kind. */
static u16 cell_texture_entries[48] = {
    0x0039, 0x0208, 0x043A, 0x0637, 0x0836, 0x0A34, 0x0C00, 0x004E, 0x024D, 0x044B, 0x064C, 0x084A,
    0x0A49, 0x0C47, 0x009E, 0x0297, 0x0490, 0x0693, 0x0892, 0x0A91, 0x0C8F, 0x00DF, 0x02FC, 0x04FB,
    0x06C6, 0x08F8, 0x0AF5, 0x0183, 0x0385, 0x0584, 0x0782, 0x0981, 0x0BBF, 0x0DBD, 0x0FBE, 0x0114,
    0x0315, 0x051C, 0x0719, 0x015B, 0x035A, 0x055D, 0x075D, 0x0958, 0x0B58, 0x0D56, 0x0F56, 0x0000,
};

/* Fill `context` (a FarmCellTexture) with the texture for item `id`. Each known
 * id selects an entry of cell_texture_entries; stones, branches, lumber and stumps
 * use another entry in winter, and weeds one in fall. An entry packs the cell's
 * column (bits 9 to 11), its row (bits 6 to 8, the rows starting at v = 256), and
 * the palette position (bit 5 picks x 0x3C0 or 0x3D0, bits 0 to 4 give y from
 * 0x1E0). Returns 1, or 0 for an id with no texture. */
s32 cell_texture_fill(FarmCellTexture *context, s32 id)
{
    FarmCellTexture *texture = context;
    s32 season;
    s32 index;
    u16 entry;
    s32 u;
    s32 v;
    s32 clutBank;
    s32 clutRow;
    s32 cx;
    s32 cy;

    season = game_state.progress.clock.date.season;
    switch (id) {
    case ITEM_CABBAGE_SEEDS:
        index = 0;
        break;
    case ITEM_STRAWBERRY_SEEDS:
        index = 1;
        break;
    case ITEM_CUCUMBER_SEEDS:
        index = 2;
        break;
    case ITEM_MOONDROP_SEEDS:
        index = 3;
        break;
    case ITEM_TOY_FLOWER_SEEDS:
        index = 4;
        break;
    case ITEM_POTATO_SEEDS:
        index = 5;
        break;
    case ITEM_TURNIP_SEEDS:
        index = 6;
        break;
    case ITEM_ORANGE_CUP_SEEDS:
        index = 7;
        break;
    case ITEM_PUMPKIN_SEEDS:
        index = 8;
        break;
    case ITEM_PINEAPPLE_SEEDS:
        index = 9;
        break;
    case ITEM_PINK_CAT_SEEDS:
        index = 10;
        break;
    case ITEM_ONION_SEEDS:
        index = 11;
        break;
    case ITEM_TOMATO_SEEDS:
        index = 12;
        break;
    case ITEM_CORN_SEEDS:
        index = 13;
        break;
    case ITEM_MAGIC_RED_SEEDS:
    case ITEM_BLUE_MAGIC_RED_FLOWER:
    case ITEM_BLUE_MAGIC_RED_PLANT:
        index = 14;
        break;
    case ITEM_RED_MAGIC_RED_FLOWER:
    case ITEM_RED_MAGIC_RED_PLANT:
        index = 15;
        break;
    case ITEM_SPINACH_SEEDS:
        index = 16;
        break;
    case ITEM_EGGPLANT_SEEDS:
        index = 17;
        break;
    case ITEM_CARROT_SEEDS:
        index = 18;
        break;
    case ITEM_GREEN_PEPPER_SEEDS:
        index = 19;
        break;
    case ITEM_SWEET_POTATO_SEEDS:
        index = 20;
        break;
    case ITEM_STONE:
        index = 0x15;
        if (season == SEASON_WINTER) {
            index = 0x25;
        }
        break;
    case ITEM_WEEDS:
        index = 0x16;
        if (season == SEASON_FALL) {
            index = 0x17;
        }
        break;
    case ITEM_BRANCH:
        index = 0x18;
        if (season == SEASON_WINTER) {
            index = 0x26;
        }
        break;
    case ITEM_LUMBER:
        index = 0x19;
        if (season == SEASON_WINTER) {
            index = 0x24;
        }
        break;
    case FIELD_TILE_LUMBER:
        index = 0x1A;
        if (season == SEASON_WINTER) {
            index = 0x23;
        }
        break;
    case ITEM_BIG_STONE:
    case ITEM_ROCK:
        index = 0x1B;
        if (season == SEASON_WINTER) {
            index = 0x27;
        }
        break;
    case FIELD_TILE_BIG_STONE_1:
    case FIELD_TILE_ROCK_1:
        index = 0x1C;
        if (season == SEASON_WINTER) {
            index = 0x28;
        }
        break;
    case FIELD_TILE_BIG_STONE_2:
    case FIELD_TILE_ROCK_2:
        index = 0x1E;
        if (season == SEASON_WINTER) {
            index = 0x2A;
        }
        break;
    case FIELD_TILE_BIG_STONE_3:
    case FIELD_TILE_ROCK_3:
        index = 0x1D;
        if (season == SEASON_WINTER) {
            index = 0x29;
        }
        break;
    case ITEM_STUMP:
        index = 0x1F;
        if (season == SEASON_WINTER) {
            index = 0x2B;
        }
        break;
    case FIELD_TILE_STUMP_1:
        index = 0x20;
        if (season == SEASON_WINTER) {
            index = 0x2C;
        }
        break;
    case FIELD_TILE_STUMP_2:
        index = 0x22;
        if (season == SEASON_WINTER) {
            index = 0x2E;
        }
        break;
    case FIELD_TILE_STUMP_3:
        index = 0x21;
        if (season == SEASON_WINTER) {
            index = 0x2D;
        }
        break;
    case ITEM_NOTHING:
    default:
        return 0;
    }
    entry = cell_texture_entries[index];
    u = (entry & CELL_ENTRY_COL_MASK) >> 9;
    v = ((entry & CELL_ENTRY_ROW_MASK) >> 6) * CELL_TEXTURE_SIZE + CELL_TEXTURE_V_BASE;
    texture->uv[0] = (u + 1) * CELL_TEXTURE_SIZE - 1;
    texture->uv[1] = v + CELL_TEXTURE_SIZE - 1;
    texture->uv[2] = u * CELL_TEXTURE_SIZE;
    texture->uv[3] = v + CELL_TEXTURE_SIZE - 1;
    texture->uv[4] = (u + 1) * CELL_TEXTURE_SIZE - 1;
    texture->uv[5] = v;
    texture->uv[6] = u * CELL_TEXTURE_SIZE;
    texture->uv[7] = v;
    clutBank = (entry >> 5) & 1;
    clutRow = entry & CELL_ENTRY_CLUT_ROW_MASK;
    cx = clutBank * 16 + CELL_CLUT_X;
    cy = clutRow + CELL_CLUT_Y;
    texture->clut = (cx >> 4) | (cy << 6);
    return 1;
}

/* Copy the UV pairs of a texture onto a quad. The caller sets the palette. */
static inline void retexture(FarmTmdQuad *prim, FarmCellTexture *texture) {
    prim->u0 = texture->uv[0];
    prim->v0 = texture->uv[1];
    prim->u1 = texture->uv[2];
    prim->v1 = texture->uv[3];
    prim->u2 = texture->uv[4];
    prim->v2 = texture->uv[5];
    prim->u3 = texture->uv[6];
    prim->v3 = texture->uv[7];
}

static inline void retexture_reversed(FarmTmdQuad *prim, FarmCellTexture *texture) {
    prim->u0 = texture->uv[6];
    prim->v0 = texture->uv[7];
    prim->u1 = texture->uv[4];
    prim->v1 = texture->uv[5];
    prim->u2 = texture->uv[2];
    prim->v2 = texture->uv[3];
    prim->u3 = texture->uv[0];
    prim->v3 = texture->uv[1];
}

/* One bit per grid column, MSB first: set means the UVs run forward. */
static u8 grid_column_forward_bits[8] = {
    0x1B, 0xF0, 0x81, 0xBF, 0x08, 0x18, 0x00, 0x00,
};

/* Retexture the grid cell at (col, row) from `texture`, if its primitive is one of
 * the two textured quad kinds. The UVs run forward or reversed, by the column's
 * bit in grid_column_forward_bits. Sets texture page 0x1F and the texture's
 * palette. Returns 1 when it was changed. */
s32 farm_cell_retexture(s16 col, s16 row, FarmCellTexture *texture) {
    FarmTmdQuad *prim;
    u32 tag;
    u8 forward;
    s32 byte;
    s16 bit;

    byte = col / 8;
    bit = col % 8;
    forward = (grid_column_forward_bits[byte] & (0x80 >> bit)) != 0;
    prim = farm_area.mode.quads[row][col];
    tag = prim->header;
    if (tag & FARM_QUAD_CELL_BIT) {
        if ((tag & TMD_PRIM_CODE_MASK) == (TMD_PRIM_GT4_RAW << 16)) {
            if (forward) {
                retexture(prim, texture);
            } else {
                retexture_reversed(prim, texture);
            }
            prim->tsb = CELL_TEXTURE_PAGE;
            prim->cba = texture->clut;
            return 1;
        }
        if ((tag & TMD_PRIM_CODE_MASK) == (TMD_PRIM_FT4_RAW << 16)) {
            if (forward) {
                retexture(prim, texture);
            } else {
                retexture_reversed(prim, texture);
            }
            prim->tsb = CELL_TEXTURE_PAGE;
            prim->cba = texture->clut;
            return 1;
        }
    }
    return 0;
}

/* Retexture every cell of the farm grid whose tile has a texture. */
void farm_retexture_all_cells(void) {
    s32 row;
    s32 col;

    row = 0;
    do {
        col = 0;
        do {
            if (cell_texture_fill((FarmCellTexture *)fishing_cell_texture, game_state.fieldTiles[row].rows[col].value) == 1) {
                farm_cell_retexture(col, row, (FarmCellTexture *)fishing_cell_texture);
            }
            col++;
        } while (col < FIELD_COLS);
        row++;
    } while (row < FIELD_ROWS);
}

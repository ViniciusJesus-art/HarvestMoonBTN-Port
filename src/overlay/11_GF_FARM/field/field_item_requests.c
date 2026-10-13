/* Item requests: the farm's queue of items the field asks to have picked up and the
 * way each request is drawn. */

#include "types.h"
#include "anim.h"
#include "clock.h"
#include "estate.h"
#include "farm_state.h"
#include "field.h"
#include "field_item_requests.h"
#include "items.h"
#include "main.h"
#include "picture_cells.h"
#include "scene.h"

static void item_request_tick(FarmItemRequest *entry);
static void item_request_clear(FarmItemRequest *request);
static void item_request_fill(FarmItemRequest *request, VECTOR pos, u16 id);

/* The 13 day thresholds. */
static s16 day_tier_thresholds[] = {
    20, 27, 37, 41, 50, 56, 61,
    69, 73, 82, 88, 100, 119,
};
/* The base tier; each threshold the day passes adds one tier. */
#define DAY_TIER_BASE 0x1BD
#define DAY_TIER_STEP 0x10000
/* The threshold scale is the date plus this offset (date 0 is threshold 20). */
#define DAY_TIER_SCALE_OFFSET 20

/* Return DAY_TIER_BASE plus the index k of the threshold with thresholds[k - 1] < day <=
 * thresholds[k], day being the scaled date; DAY_TIER_BASE when none matches. */
s32 calendar_day_tier(void) {
    s16 *thresholds;
    s16 previous;
    s16 current;
    s16 day;
    s32 result;
    s32 marker;
    u32 count;
    count = 0;
    previous = -1;
    thresholds = day_tier_thresholds;
    result = DAY_TIER_BASE << 16;
    marker = DAY_TIER_STEP;
    day = ((game_state.progress.clock.date.season * DAYS_PER_SEASON + game_state.progress.clock.date.day + DAY_TIER_SCALE_OFFSET) % (SEASONS_PER_YEAR * DAYS_PER_SEASON));
    current = *thresholds;
    for (;;) {
        if (previous < day && current >= day) {
            return result >> 16;
        }
        thresholds += 1;
        count += 1;
        result += marker;
        if (count >= sizeof(day_tier_thresholds) / sizeof(s16)) {
            return DAY_TIER_BASE;
        }
        previous = current;
        current = *thresholds;
    }
}

#define ITEM_REQUEST_SLOTS 25

FarmItemRequest item_request_slots[ITEM_REQUEST_SLOTS];

/* Tick the request slots, unless the debug flag that skips breaking animations is set. */
void item_requests_tick_all(void) {
    s32 i;
    FarmItemRequest *p;
    if (!(game_state.debugFlags & DEBUG_FLAG_NO_BREAKING_ANIMATIONS)) {
        p = item_request_slots;
        i = 24;
        do {
            item_request_tick(p);
            i--;
            p++;
        } while (i >= 0);
    }
}

/* Clear every request slot. */
void item_requests_clear_all(void) {
    FarmItemRequest *p = item_request_slots;
    s32 i = 24;
    do {
        item_request_clear(p);
        i -= 1;
        p++;
    } while (i >= 0);
}

/* Fill the first free request slot with item `id` at `pos`; 0 when all are in use. */
s32 item_request_submit(s16 id, VECTOR pos) {
    FarmItemRequest *entry;
    s32 count;

    entry = item_request_slots;
    count = 0;
    while (1) {
        if (entry->record == 0) {
            item_request_fill(entry, pos, id);
            return 1;
        }
        count++;
        if (count >= ITEM_REQUEST_SLOTS) {
            return 0;
        }
        entry++;
    }
}

/* Advance one request slot and draw its sprite above the item's position; the slot is
 * cleared once its animation has run out. */
static void item_request_tick(FarmItemRequest *entry) {
    SVECTOR input;
    FarmScreenPoint result;
    s16 returned;
    s32 frame;
    s32 eligible;
    if (entry->record != 0) {
        returned = anim_range_frames(entry->record, entry->kind);
        frame = entry->state / 3;
        if ((game_state.globalFlags & GLOBAL_FLAG_PAUSED) == 0) {
            entry->state = entry->state + 1;
            eligible = frame < returned;
        } else {
            eligible = frame < returned;
        }
        if (eligible != 0) {
            input.vx = entry->pos[0] + 150;
            input.vy = entry->pos[1] - 100;
            input.vz = entry->pos[2] - 150;
            if (estate_point_project(&input, &result) != 0) {
                anim_draw_sprt_cursor(&current_frame->ot[result.depth >> 2], entry->record,
                              entry->kind, frame & 0xFF, result.x, result.y);
            }
        } else {
            item_request_clear(entry);
        }
    }
}

static void item_request_clear(FarmItemRequest *request) {
    request->record = 0;
}

/* Fill a free item_request_slots request slot for item `id` at `pos`. The stones,
 * rocks, stumps, lumber and grass seeds have fixed sprites; the other items use
 * the sprite of the place (base + 6). The kind is 1, or 3 for weeds in fall, a
 * seed out of its season, and a stone, rock, stump or branch away from the farm in
 * winter. */
static void item_request_fill(FarmItemRequest *request, VECTOR pos, u16 id) {
    s32 bonus;
    s32 base;
    s32 kind;
    s32 index;
    s16 item;
    s32 owner;
    s16 state;

    bonus = 0;
    base = 0;
    state = game_state.progress.clock.date.season;
    if (state == SEASON_WINTER) {
        bonus = (farm_area.mode.place.location != PLACE_FARM) * 2;
    }
    if (farm_area.mode.place.location != PLACE_FARM) {
        base = -1;
    }
    if (farm_area.mode.place.location == PLACE_GREENHOUSE) {
        base = 0x16;
    }
    switch (id) {
    case ITEM_STONE:
        index = 0;
        kind = bonus + 1;
        break;
    case ITEM_BIG_STONE:
    case FIELD_TILE_BIG_STONE_1:
    case FIELD_TILE_BIG_STONE_2:
    case FIELD_TILE_BIG_STONE_3:
        index = 1;
        kind = bonus + 1;
        break;
    case ITEM_ROCK:
    case FIELD_TILE_ROCK_1:
    case FIELD_TILE_ROCK_2:
    case FIELD_TILE_ROCK_3:
        index = 2;
        kind = bonus + 1;
        break;
    case ITEM_STUMP:
    case FIELD_TILE_STUMP_1:
    case FIELD_TILE_STUMP_2:
    case FIELD_TILE_STUMP_3:
        index = 3;
        kind = bonus + 1;
        break;
    case ITEM_BRANCH:
        index = 4;
        kind = bonus + 1;
        break;
    case ITEM_LUMBER:
    case FIELD_TILE_LUMBER:
        index = 5;
        kind = 1;
        break;
    case ITEM_GRASS_SEEDS:
        kind = 1;
        switch (state) {
        case SEASON_SPRING:
            index = 0xE;
            break;
        case SEASON_SUMMER:
        case SEASON_FALL:
            index = 0x13;
            break;
        default:
            return;
        }
        break;
    case ITEM_WEEDS:
        if (state != SEASON_FALL) {
            kind = 1;
        } else {
            kind = 3;
        }
        index = base + 6;
        break;
    default:
        item = id;
        if (item_is_seed(item) != 1) {
            kind = 1;
        } else {
            owner = crop_season(item);
            kind = 1;
            if (game_state.progress.clock.date.season != owner && farm_area.mode.place.location != PLACE_GREENHOUSE && owner != 4) {
                kind = 3;
                index = base + 6;
                break;
            }
        }
        index = base + 6;
        break;
    }
    request->record = &game_state.work.sprites[index];
    request->kind = kind;
    request->state = 0;
    request->pos[0] = pos.vx;
    request->pos[1] = pos.vy;
    request->pos[2] = pos.vz;
}

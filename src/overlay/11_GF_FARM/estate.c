/* Farm estate bookkeeping: gold, the message queues, the daily notices and the
 * farm's search records. */

#include <memory.h>
#include "types.h"
#include "anim.h"
#include "animals.h"
#include "clock.h"
#include "cue.h"
#include "dog.h"
#include "estate.h"
#include "event_commands.h"
#include "event_flags.h"
#include "farm_main.h"
#include "farm_probe.h"
#include "farm_state.h"
#include "farm_tables.h"
#include "field_item_requests.h"
#include "game_types.h"
#include "harvest_sprites.h"
#include "house.h"
#include "item_storage.h"
#include "items.h"
#include "main.h"
#include "menu_window.h"
#include "model.h"
#include "pad.h"
#include "picture_cells.h"
#include "player.h"
#include "scene.h"
#include "shop_dialogues.h"
#include "status_pages.h"
#include "text.h"
#include "villager.h"

#define NPC(n) (game_state.villagers[n])

/* The queue kind of the daily notices, and the message of the invitation. */
#define ESTATE_NOTICE_KIND 2
#define ESTATE_MESSAGE_INVITATION 0x19B
#define BASKET_CAPACITY 30
/* The fish shipped in total that sets EVENT_FLAG_FISH_SHIPPED_200. */
#define ESTATE_FISH_SHIPPED_GOAL 200
/* The point a record's sprite is drawn at, as an offset from the record's position. */
#define ESTATE_SPRITE_OFFSET_X 150
#define ESTATE_SPRITE_OFFSET_Y (-100)
#define ESTATE_SPRITE_OFFSET_Z (-150)
/* The result of a search handler for a seasonal item: its sprite is the seasonal one. */
#define ESTATE_SPRITE_SEASONAL 2
/* The ordering table index of a sprite is its depth (OTZ) shifted down by this. */
#define ESTATE_OT_DEPTH_SHIFT 2
/* The screen box estate_point_on_screen accepts: left and top are the lowest x and y. */
#define ESTATE_SCREEN_BOX_LEFT (-49)
#define ESTATE_SCREEN_BOX_WIDTH 419
#define ESTATE_SCREEN_BOX_TOP (-49)
#define ESTATE_SCREEN_BOX_HEIGHT 339

/* The bins of the shipment counts: the eggs, milk, wool, mayonnaise, cheese, yarn, fish
 * and the three grasses follow the fifteen crops. */
enum EstateShipmentSlot {
    ESTATE_SLOT_EGG = 0xF,
    ESTATE_SLOT_MILK = 0x10,
    ESTATE_SLOT_WOOL = 0x11,
    ESTATE_SLOT_MAYONNAISE = 0x12,
    ESTATE_SLOT_CHEESE = 0x13,
    ESTATE_SLOT_YARN = 0x14,
    ESTATE_SLOT_FISH = 0x15,
    ESTATE_SLOT_BLUE_GRASS = 0x16,
    ESTATE_SLOT_RED_GRASS = 0x17,
    ESTATE_SLOT_GREEN_GRASS = 0x18,
};

typedef struct EstatePlaceProbe {
    VECTOR position;
    LimitPair place;
} EstatePlaceProbe;

/* Three alternative values per kind, chosen by the record's flag bits. */
typedef struct KindValues {
    s16 valueWhenBit13;
    s16 valueWhenBit11;
    s16 valueWhenBit12;
} KindValues;


static s32 estate_queue_serve_head(void);
static void estate_queue_push(s16 kind, s16 message);
static void estate_queue_remove(s16 slot, s32 active);
static void estate_post_daily_notices(void);
static s32 estate_is_single(void);
static void estate_issue_invitation(void);
static void estate_record18_bounce_step(void);
static void estate_trigger_disarm(void);
static void estate_item_enable(s32 index, s32 mode, s16 value);
static void estate_records_kind_refresh(s16 mode);
static void estate_record_ids_update(s16 kind, s16 count, FarmEstateRecord *records);
static void estate_items_enable_by_flags(s16 unused0);
static void estate_record_table_enable(s16 count, FarmEstateRecord *records);
static void estate_items_restore(void);
static void estate_item_set_enable(s32 itemId, FarmEstateRecord *items, s32 count, u32 mask, u32 bits);
static FarmEstateRecord *estate_record_find(s16 itemId, FarmEstateRecord *items, s32 count);
static void estate_gate_results_place(void);
static s32 estate_season_bit(void);
static void estate_search_run(void);
static void estate_search_records(s32 count, FarmEstateRecord *record, s32 (*callback)());
static s32 estate_action_status(s32 action, s32 *result, s32 *status);
static s32 estate_code_status(s32 code, s32 *result, s32 *status);
static s32 estate_code_lookup(s32 code, s32 *result, s32 *value);
static s32 estate_code_value(s32 code, s32 *out1, s32 *out2);

static FarmScreenXY estate_screen_xy;
static SVECTOR estate_point;
static void *estate_current_object;
u32 record18_bounce_step;

/* Add gold to the purse, clamped to its limits, and book it as earnings or expenses. */
s32 estate_gold_change(s32 amount, s32 reset) {
    s32 adjusted;
    s32 direction;
    s32 expenses;

    if (amount == 0) {
        return 0;
    }
    direction = 0;
    adjusted = game_state.player.gold;
    adjusted += amount;
    if (adjusted < 0) {
        direction = 2;
        if (reset != 0) {
            game_state.player.gold = 0;
            amount -= adjusted;
        } else {
            amount = 0;
        }
    } else if (adjusted > FARM_MONEY_MAX - 1) {
        direction = 1;
        game_state.player.gold = FARM_MONEY_MAX;
    } else {
        game_state.player.gold = adjusted;
    }
    if (amount > 0) {
        game_state.progress.estate.ledger[0].earnings += amount;
    } else {
        expenses = game_state.progress.estate.ledger[0].expenses;
        expenses += (amount < 0) ? -amount : amount;
        game_state.progress.estate.ledger[0].expenses = expenses;
    }
    return direction;
}

/* Set while a queued message is being shown; cleared when the queue is idle again. */
u32 estate_op_pending = 0;

/* Set when the 0x19B message is served, and cleared once its cue has been issued. */
u32 estate_op_cue_pending = 0;

/* Advance the pending message operation, showing queued messages in turn. */
void estate_pending_op_tick(void) {
    s32 state;

    if (estate_op_pending == 0) {
        estate_op_cue_pending = 0;
        return;
    }
    state = menu_get_phase();
    if (state == TEXT_WINDOW_CLOSED) {
        if (estate_op_cue_pending != 0) {
            estate_op_pending = 0;
            estate_op_cue_pending = 0;
            estate_issue_invitation();
            return;
        }
        state = estate_queue_serve_head();
        if (state == 0) {
            estate_op_pending = 0;
        }
        return;
    }
    if ((input_pad1->pressed & PAD_CIRCLE) != 0) {
        estate_op_pending = 0;
        menu_begin_close();
    }
}

/* Start serving the queue as the pending operation. When the queue is empty, open message
 * 0x221 instead. Returns 1 when the queue had a message to serve. */
s32 estate_pending_op_start(void) {
    s32 result;
    if (estate_op_pending == 0) {
        result = estate_queue_serve_head();
        if (result == 1) {
            estate_op_pending = result;
            return result;
        }
        menu_request_open(2, 0x221);
    }
    return 0;
}

/* Clear the message queues and forget the notices seen. */
void estate_queues_clear(void) {
    s32 empty = -1;
    u8 *p = (u8 *)&game_state;
    s32 i;
    for (i = 19; i >= 0; i--) {
        p[0x49E6] = empty;
        p[0x4A36] = empty;
        p += 4;
    }
    game_state.progress.estate.noticesSeen = 0;
}

/* Queue a timed message in the second queue; a zero timer posts it to the live queue
 * at once, and when the second queue is full, its oldest entry is dropped. */
void estate_timed_message_add(s16 kind, s16 message, s16 timer) {
    s32 slot;
    FarmMessage *entry;
    FarmMessage *table;
    if (timer == 0) {
        estate_queue_push(kind, message);
        return;
    }
    if (message == 0) {
        return;
    }
    entry = game_state.progress.estate.queues[1];
    for (slot = 0; slot < MESSAGE_QUEUE_LEN; slot++, entry++) {
        if (entry->kind == -1) {
            entry->kind = kind;
            entry->message = message;
            entry->timer = timer;
            break;
        }
    }
    if (slot == MESSAGE_QUEUE_LEN) {
        table = game_state.progress.estate.queues[1];
        memmove((unsigned char *)table, (unsigned char *)(table + 1), (MESSAGE_QUEUE_LEN - 1) * sizeof(FarmMessage));
        game_state.progress.estate.queues[1][MESSAGE_QUEUE_LEN - 1].kind = kind;
        game_state.progress.estate.queues[1][MESSAGE_QUEUE_LEN - 1].message = message;
        game_state.progress.estate.queues[1][MESSAGE_QUEUE_LEN - 1].timer = timer;
    }
}

/* Daily refresh: retire live messages due today, move queued messages whose timer
 * has run out into the live list, then post the day's notices. */
void estate_queues_daily(void) {
    FarmMessage *active;
    FarmMessage *queued;
    FarmMessage *queue;
    FarmState *state;
    s32 slot;
    s32 queue_slot;
    s16 category;

    state = &game_state;
    active = state->progress.estate.queues[0];
    slot = 0;
    category = state->progress.clock.date.day;
    do {
        if ((active->kind != (-1)) && (active->timer == category)) {
            estate_queue_remove(slot, 1);
            active->kind++;
            active->kind--;
        }
        slot++;
        active++;
    } while (slot < MESSAGE_QUEUE_LEN);
    queue = game_state.progress.estate.queues[0];
    queued = queue + MESSAGE_QUEUE_LEN;
    queue_slot = 0;
    do {
        if (queued->kind != (-1)) {
            queued->timer--;
            if (queued->timer <= 0) {
                estate_queue_push(queued->kind, queued->message);
                queued--;
                estate_queue_remove(queue_slot, 0);
                queue_slot--;
            }
        }
        queue_slot++;
        queued++;
    } while (queue_slot < MESSAGE_QUEUE_LEN);
    estate_op_pending = 0;
    estate_post_daily_notices();
}

/* Open the message at the head of the live queue and drop it from the queue. Returns 0
 * when the queue is empty, and 1 otherwise; a message the menu refuses stays at the head. */
static s32 estate_queue_serve_head(void) {
    FarmState *farm = &game_state;
    s8 kind = farm->progress.estate.queues[0][0].kind;

    if (kind == -1) return 0;
    menu_begin_close();
    if (menu_request_open(farm->progress.estate.queues[0][0].kind, farm->progress.estate.queues[0][0].message) != 0) return 1;
    if (farm->progress.estate.queues[0][0].message == ESTATE_MESSAGE_INVITATION) estate_op_cue_pending = 1;
    memmove((unsigned char *)&farm->progress.estate.queues[0][0], (unsigned char *)&farm->progress.estate.queues[0][1], (MESSAGE_QUEUE_LEN - 1) * sizeof(FarmMessage));
    farm->progress.estate.queues[0][MESSAGE_QUEUE_LEN - 1].kind = -1;
    return 1;
}

/* Queue a message: take the first free entry of the twenty, or, when all are
 * taken, drop the oldest and append at the tail. */
static void estate_queue_push(s16 kind, s16 message) {
    FarmState *farm = &game_state;
    FarmMessage *entry = farm->progress.estate.queues[0];
    s16 timer = farm->progress.clock.date.day;
    s32 slot;

    for (slot = 0; slot < MESSAGE_QUEUE_LEN; slot++, entry++) {
        if (entry->kind == -1) {
            entry->kind = kind;
            entry->message = message;
            entry->timer = timer;
            break;
        }
    }
    if (slot == MESSAGE_QUEUE_LEN) {
        memmove((unsigned char *)&game_state.progress.estate.queues[0][0], (unsigned char *)&game_state.progress.estate.queues[0][1], (MESSAGE_QUEUE_LEN - 1) * sizeof(FarmMessage));
        game_state.progress.estate.queues[0][MESSAGE_QUEUE_LEN - 1].kind = kind;
        game_state.progress.estate.queues[0][MESSAGE_QUEUE_LEN - 1].message = message;
        game_state.progress.estate.queues[0][MESSAGE_QUEUE_LEN - 1].timer = timer;
    }
}

/* Remove entry `slot` from one of the two message queues: shift the entries
 * behind it down and release the tail. */
static void estate_queue_remove(s16 slot, s32 active) {
    FarmMessage *entry;
    FarmMessage *next;
    FarmMessage *last;
    s32 length;

    if (active == 1) {
        entry = &game_state.progress.estate.queues[0][slot];
        next = &game_state.progress.estate.queues[0][slot + 1];
        length = MESSAGE_QUEUE_LEN * sizeof(FarmMessage) - (slot + 1) * sizeof(FarmMessage);
        last = &game_state.progress.estate.queues[0][MESSAGE_QUEUE_LEN - 1];
    } else {
        entry = &game_state.progress.estate.queues[1][slot];
        next = &game_state.progress.estate.queues[1][slot + 1];
        length = MESSAGE_QUEUE_LEN * sizeof(FarmMessage) - (slot + 1) * sizeof(FarmMessage);
        last = &game_state.progress.estate.queues[1][MESSAGE_QUEUE_LEN - 1];
    }
    if (slot == MESSAGE_QUEUE_LEN - 1) {
        entry->kind = -1;
        entry->message = 0;
        entry->timer = 0;
    } else {
        memmove((unsigned char *)entry, (unsigned char *)next, length);
    }
    last->kind = -1;
    last->message = 0;
    last->timer = 0;
}

/* Post the day's notices: the villagers' friendship notices on the first day of
 * spring, the birthday notices on the player's birthday, then the one-shot notices,
 * each remembered by a bit of `seen`. */
static void estate_post_daily_notices(void) {
    s32 *seen;
    s16 season;
    s16 day;
    s32 notice = 0;
    s32 i;
    s16 friendship;

    seen = &game_state.progress.estate.noticesSeen;
    season = game_state.progress.clock.date.season;
    day = game_state.progress.clock.date.day;
    if (season == SEASON_SPRING && day == 0) {
        notice = 0;
        if (event_flag_get(EVENT_FLAG_E6) == 1) {
            notice = NPC(CHARACTER_CLIFF).friendship >= 100;
        } else if (NPC(CHARACTER_CLIFF).friendship >= 200) {
            notice = 2;
        }
        if (NPC(CHARACTER_CLIFF).friendship >= 200 && event_flag_get(EVENT_FLAG_ANN_CLIFF_MARRIED) == 1) {
            notice = 3;
        }
        if (notice != 0) {
            estate_timed_message_add(ESTATE_NOTICE_KIND, notice, 1);
        }

        notice = (NPC(CHARACTER_DOCTOR).friendship >= 100) ? 4 : 0;
        if (NPC(CHARACTER_DOCTOR).friendship >= 200 && event_flag_get(EVENT_FLAG_ELLI_DOCTOR_MARRIED) == 1) {
            notice = 5;
        }
        if (notice != 0) {
            estate_timed_message_add(ESTATE_NOTICE_KIND, notice, 0);
        }

        notice = 0;
        if (NPC(CHARACTER_RICK).friendship >= 100) {
            notice = 6;
            if (event_flag_get(EVENT_FLAG_POPURI_KAI_MARRIED) == 1) {
                notice = 7;
            }
        }
        if (event_flag_get(EVENT_FLAG_MARRIED_POPURI) == 1) {
            notice = 8;
        }
        if (NPC(CHARACTER_RICK).friendship >= 200 && event_flag_get(EVENT_FLAG_KAREN_RICK_MARRIED) == 1) {
            notice = 9;
        }
        if (notice != 0) {
            estate_timed_message_add(ESTATE_NOTICE_KIND, notice, 2);
        }

        friendship = NPC(CHARACTER_GRAY).friendship;
        notice = 0;
        if (friendship >= 100) {
            notice = 10;
        }
        if (friendship >= 200 && event_flag_get(EVENT_FLAG_MARY_GRAY_MARRIED) == 1) {
            notice = 11;
        }
        if (notice != 0) {
            estate_timed_message_add(ESTATE_NOTICE_KIND, notice, 3);
        }

        friendship = NPC(CHARACTER_KAI).friendship;
        notice = 0;
        if (friendship >= 100) {
            notice = 12;
        }
        if (friendship >= 200 && event_flag_get(EVENT_FLAG_POPURI_KAI_MARRIED) == 1) {
            if (event_cmd_mode_check(0) == 1) {
                notice = 14;
            } else if (event_flag_get(EVENT_FLAG_11E) == 1) {
                notice = 13;
            }
        }
        if (notice != 0) {
            estate_timed_message_add(ESTATE_NOTICE_KIND, notice, 4);
        }

        estate_timed_message_add(ESTATE_NOTICE_KIND, 15, 1);

        if (NPC(CHARACTER_SAIBARA).friendship >= 150) {
            estate_timed_message_add(ESTATE_NOTICE_KIND, 16, 0);
        }
        notice = 17;
        if (event_flag_get(EVENT_FLAG_MARRIED_ANN) == 1) {
            notice = 18;
        }
        estate_timed_message_add(ESTATE_NOTICE_KIND, notice, 2);
        estate_timed_message_add(ESTATE_NOTICE_KIND, 19, 4);
        if (NPC(CHARACTER_BASIL).friendship >= 150) {
            estate_timed_message_add(ESTATE_NOTICE_KIND, 20, 1);
        }
        if (NPC(CHARACTER_MAY).friendship >= 150) {
            estate_timed_message_add(ESTATE_NOTICE_KIND, 21, 0);
        }
        notice = (NPC(CHARACTER_MAYOR_THOMAS).friendship >= 150) ? 23 : 22;
        estate_timed_message_add(ESTATE_NOTICE_KIND, notice, 0);
        if (NPC(CHARACTER_DUKE).friendship >= 150) {
            estate_timed_message_add(ESTATE_NOTICE_KIND, 24, 1);
        }
        if (NPC(CHARACTER_ELLEN).friendship >= 100) {
            estate_timed_message_add(ESTATE_NOTICE_KIND, 25, 3);
        }

        notice = 26;
        for (i = 0; i < HELPER_COUNT; i++) {
            if (game_state.helpers[i].actor.affection < 100) {
                notice = 0;
            }
        }
        if (notice != 0) {
            estate_timed_message_add(ESTATE_NOTICE_KIND, notice, 0);
        }

        notice = 0;
        if (NPC(CHARACTER_POPURI).affection >= 60000 && event_flag_get(EVENT_FLAG_POPURI_KAI_MARRIED) == 0 && estate_is_single() == 1) {
            notice = 27;
        }
        if (notice != 0) {
            estate_timed_message_add(ESTATE_NOTICE_KIND, notice, 4);
        }
        notice = 0;
        if (NPC(CHARACTER_ANN).affection >= 60000 && event_flag_get(EVENT_FLAG_ANN_CLIFF_MARRIED) == 0 && estate_is_single() == 1) {
            notice = 28;
        }
        if (notice != 0) {
            estate_timed_message_add(ESTATE_NOTICE_KIND, notice, 3);
        }
        notice = 0;
        if (NPC(CHARACTER_KAREN).affection >= 60000 && event_flag_get(EVENT_FLAG_KAREN_RICK_MARRIED) == 0 && estate_is_single() == 1) {
            notice = 29;
        }
        if (notice != 0) {
            estate_timed_message_add(ESTATE_NOTICE_KIND, notice, 2);
        }
        notice = 0;
        if (NPC(CHARACTER_ELLI).affection >= 60000 && event_flag_get(EVENT_FLAG_ELLI_DOCTOR_MARRIED) == 0 && estate_is_single() == 1) {
            notice = 30;
        }
        if (notice != 0) {
            estate_timed_message_add(ESTATE_NOTICE_KIND, notice, 0);
        }
        notice = 0;
        if (event_flag_get(EVENT_FLAG_MARY_GRAY_MARRIED) == 0 && estate_is_single() == 1) {
            notice = (NPC(CHARACTER_MARY).affection >= 60000) ? 32 : 31;
        }
        if (notice != 0) {
            estate_timed_message_add(ESTATE_NOTICE_KIND, notice, 1);
        }
    }

    if (season == game_state.player.birthday.season && day == game_state.player.birthday.day) {
        if (NPC(CHARACTER_POPURI).affection >= 40000 && event_flag_get(EVENT_FLAG_MARRIED_POPURI) == 0) {
            estate_timed_message_add(ESTATE_NOTICE_KIND, 41, 0);
        }
        if (NPC(CHARACTER_ANN).affection >= 40000 && event_flag_get(EVENT_FLAG_MARRIED_ANN) == 0) {
            estate_timed_message_add(ESTATE_NOTICE_KIND, 42, 0);
        }
        if (NPC(CHARACTER_KAREN).affection >= 40000 && event_flag_get(EVENT_FLAG_MARRIED_KAREN) == 0) {
            estate_timed_message_add(ESTATE_NOTICE_KIND, 43, 0);
        }
        if (NPC(CHARACTER_ELLI).affection >= 40000 && event_flag_get(EVENT_FLAG_MARRIED_ELLI) == 0) {
            estate_timed_message_add(ESTATE_NOTICE_KIND, 44, 0);
        }
        if (NPC(CHARACTER_MARY).affection >= 40000 && event_flag_get(EVENT_FLAG_MARRIED_MARY) == 0) {
            estate_timed_message_add(ESTATE_NOTICE_KIND, 45, 0);
        }
        if (season != SEASON_SUMMER && NPC(CHARACTER_KAI).friendship >= 200) {
            estate_timed_message_add(ESTATE_NOTICE_KIND, 46, 0);
        }
        if (NPC(CHARACTER_CLIFF).friendship >= 200 && event_flag_get(EVENT_FLAG_129) == 1) {
            estate_timed_message_add(ESTATE_NOTICE_KIND, 47, 0);
        }
        if (NPC(CHARACTER_DOCTOR).friendship >= 200) {
            estate_timed_message_add(ESTATE_NOTICE_KIND, 48, 0);
        }
    }

    if (!(*seen & 0x1) && (season == SEASON_SPRING || game_state.progress.estate.greenhouseLevel == 1)
        && game_state.progress.estate.shipment.counts[estate_item_slot_index(ITEM_TURNIP)] >= 100
        && game_state.progress.estate.shipment.counts[estate_item_slot_index(ITEM_POTATO)] >= 100
        && game_state.progress.estate.shipment.counts[estate_item_slot_index(ITEM_CUCUMBER)] >= 100
        && game_state.progress.estate.shipment.counts[estate_item_slot_index(ITEM_CABBAGE)] >= 100) {
        estate_timed_message_add(ESTATE_NOTICE_KIND, 56, 0);
        *seen |= 0x1;
    }
    if (!(*seen & 0x2) && (season == SEASON_SUMMER || game_state.progress.estate.greenhouseLevel == 1)
        && game_state.progress.estate.shipment.counts[estate_item_slot_index(ITEM_TOMATO)] >= 100
        && game_state.progress.estate.shipment.counts[estate_item_slot_index(ITEM_CORN)] >= 100
        && game_state.progress.estate.shipment.counts[estate_item_slot_index(ITEM_ONION)] >= 100
        && game_state.progress.estate.shipment.counts[estate_item_slot_index(ITEM_PINEAPPLE)] >= 100) {
        estate_timed_message_add(ESTATE_NOTICE_KIND, 57, 0);
        *seen |= 0x2;
    }
    if (!(*seen & 0x4) && (season == SEASON_FALL || game_state.progress.estate.greenhouseLevel == 1)
        && game_state.progress.estate.shipment.counts[estate_item_slot_index(ITEM_EGGPLANT)] >= 100
        && game_state.progress.estate.shipment.counts[estate_item_slot_index(ITEM_CARROT)] >= 100
        && game_state.progress.estate.shipment.counts[estate_item_slot_index(ITEM_SWEET_POTATO)] >= 100
        && game_state.progress.estate.shipment.counts[estate_item_slot_index(ITEM_GREEN_PEPPER)] >= 100) {
        estate_timed_message_add(ESTATE_NOTICE_KIND, 58, 0);
        *seen |= 0x4;
    }
    if (!(*seen & 0x8) && (season == SEASON_WINTER || game_state.progress.estate.greenhouseLevel == 1)
        && event_flag_get(EVENT_FLAG_10E) == 1) {
        estate_timed_message_add(ESTATE_NOTICE_KIND, 59, 0);
        *seen |= 0x8;
    }
    /* The global flag test always passes: the notice needs only a villager at 50000 affection. */
    if (!(*seen & 0x10) && ~(game_state.globalFlags & GLOBAL_FLAG_BIT21)
        && (NPC(CHARACTER_KAREN).affection >= 50000 || NPC(CHARACTER_ANN).affection >= 50000 || NPC(CHARACTER_ELLI).affection >= 50000
            || NPC(CHARACTER_MARY).affection >= 50000 || NPC(CHARACTER_POPURI).affection >= 50000)) {
        estate_timed_message_add(ESTATE_NOTICE_KIND, 60, 0);
        *seen |= 0x10;
    }
    if (!(*seen & 0x20) && (game_state.progress.estate.statusFlags & ESTATE_STATUS_RUCKSACK_UPGRADE_OPEN)) {
        estate_timed_message_add(ESTATE_NOTICE_KIND, 61, 0);
        *seen |= 0x20;
    }
    if (!(*seen & 0x40) && (game_state.progress.estate.statusFlags & ESTATE_STATUS_SHIPPED_5D)) {
        estate_timed_message_add(ESTATE_NOTICE_KIND, 62, 0);
        *seen |= 0x40;
    }
    if (!(*seen & 0x80) && (game_state.progress.estate.statusFlags & ESTATE_STATUS_SHIPPED_5F)) {
        estate_timed_message_add(ESTATE_NOTICE_KIND, 63, 0);
        *seen |= 0x80;
    }
    if (!(*seen & 0x100) && (game_state.progress.estate.statusFlags & ESTATE_STATUS_NO_BRUSH)
        && (livestock_count_cattle() > 0 || livestock_count_sheep() > 0 || game_state.animals.horse.actor.kind != 0)) {
        estate_timed_message_add(ESTATE_NOTICE_KIND, 64, 0);
        *seen |= 0x100;
    }
    if (!(*seen & 0x200) && (game_state.progress.estate.statusFlags & ESTATE_STATUS_NO_MILKER) && livestock_count_cattle() > 0) {
        estate_timed_message_add(ESTATE_NOTICE_KIND, 65, 0);
        *seen |= 0x200;
    }
    if (!(*seen & 0x400) && (game_state.progress.estate.statusFlags & ESTATE_STATUS_NO_CLIPPERS) && livestock_count_sheep() > 0) {
        estate_timed_message_add(ESTATE_NOTICE_KIND, 66, 0);
        *seen |= 0x400;
    }
    if (!(*seen & 0x800) && event_flag_get(EVENT_FLAG_MARY_GRAY_MARRIED) == 1) {
        estate_timed_message_add(ESTATE_NOTICE_KIND, 67, 1);
        *seen |= 0x800;
    }
    if (!(*seen & 0x1000) && game_state.progress.clock.date.year == 0
        && game_state.progress.clock.date.season == SEASON_SUMMER && game_state.progress.clock.date.day == 1) {
        estate_timed_message_add(ESTATE_NOTICE_KIND, 38, 0);
        *seen |= 0x800;
    }
    if (game_state.progress.clock.date.season == SEASON_FALL && game_state.progress.clock.date.day == 0) {
        estate_timed_message_add(ESTATE_NOTICE_KIND, 39, 0);
    }
    if (!(*seen & 0x2000) && game_state.progress.clock.date.season == SEASON_SUMMER
        && game_state.progress.clock.date.day >= 0 && game_state.progress.clock.date.day < 17
        && game_state.player.wedding.bride == -1 && event_flag_get(EVENT_FLAG_ANN_CLIFF_MARRIED) == 0
        && NPC(CHARACTER_ANN).affection > 15000) {
        estate_timed_message_add(ESTATE_NOTICE_KIND, ESTATE_MESSAGE_INVITATION, 0);
        *seen |= 0x2000;
    }
}

static s32 estate_is_single(void) {
    return event_flag_get(EVENT_FLAG_SINGLE);
}

/* Give the invitation item and note the year after this one for it. */
static void estate_issue_invitation(void) {
    event_item_can_take(ITEM_INVITATION, 1, 3, 1);
    event_cue_issue(ITEM_INVITATION, 0, 0, 0);
    game_state.days.values[DAY_VALUE_INVITATION_YEAR] = game_state.progress.clock.date.year + 1;
}

/* Adds to the lumber stock by mode (carried or picked adds 1, a stump 6, bought `amount`), capped at ESTATE_STOCK_MAX. */
void estate_lumber_add(u16 mode, u16 amount) {
    u16 *counter = &game_state.progress.estate.lumber;
    u16 value;

    switch (mode) {
    case LUMBER_ADD_CARRIED:
        *counter += 1;
        break;
    case LUMBER_ADD_PICKED:
        *counter += 1;
        break;
    case LUMBER_ADD_STUMP:
        *counter += 6;
        break;
    case LUMBER_ADD_BOUGHT:
        *counter += amount;
        break;
    default:
        *counter += 1;
        break;
    }
    value = *counter;
    if (value >= 1000) {
        value = ESTATE_STOCK_MAX;
    }
    *counter = value;
}

s32 estate_lumber_take(u32 amount) {
    u16 *counter = &game_state.progress.estate.lumber;
    u16 value = *counter;
    amount &= 0xFFFF;
    if (value < amount) return 0;
    *counter = value - amount;
    return 1;
}

/* Adds to the fodder stock by mode (carried or picked adds 1, bought `amount`), capped at ESTATE_STOCK_MAX. */
void estate_fodder_add(u16 mode, u16 amount) {
    u16 *counter = &game_state.progress.estate.fodder;
    u16 value;

    switch (mode) {
    case FODDER_ADD_CARRIED:
        *counter += 1;
        break;
    case FODDER_ADD_PICKED:
        *counter += 1;
        break;
    case FODDER_ADD_BOUGHT:
        *counter += amount;
        break;
    default:
        *counter += 1;
        break;
    }
    value = *counter;
    if (value >= 1000) {
        value = ESTATE_STOCK_MAX;
    }
    *counter = value;
}

s32 estate_fodder_take(u16 amount) {
    u16 *counter = &game_state.progress.estate.fodder;
    u16 current = *counter;
    if (current < amount) return 0;
    *counter = current - amount;
    return 1;
}

/* Adds to the fish food stock: one unit, or `amount` in the amount mode, capped at ESTATE_STOCK_MAX. */
void estate_fish_food_add(u16 mode, u16 amount) {
    u16 *counter = &game_state.progress.estate.fishFood;
    u16 value;

    switch (mode) {
    case FISH_FOOD_ADD_ONE:
        *counter += 1;
        break;
    case FISH_FOOD_ADD_AMOUNT:
        *counter += amount;
        break;
    default:
        *counter += 1;
        break;
    }
    value = *counter;
    if (value >= 1000) {
        value = ESTATE_STOCK_MAX;
    }
    *counter = value;
}

s32 estate_fish_food_take(u16 amount) {
    u16 *counter = &game_state.progress.estate.fishFood;
    u16 current = *counter;
    if (current < amount) return 0;
    *counter = current - amount;
    return 1;
}

/* Adds to the chicken feed stock: the mode picks one unit, ten, or `amount`; capped at ESTATE_STOCK_MAX. */
void estate_chicken_feed_add(u16 mode, u16 amount) {
    u16 *counter = &game_state.progress.estate.chickenFeed;
    u16 value;

    switch (mode) {
    case 0:
        *counter += 1;
        break;
    case 1:
        *counter += 10;
        break;
    case 2:
        *counter += amount;
        break;
    default:
        *counter += 1;
        break;
    }
    value = *counter;
    if (value >= 1000) {
        value = ESTATE_STOCK_MAX;
    }
    *counter = value;
}

s32 estate_chicken_feed_take(u16 amount) {
    u16 *counter = &game_state.progress.estate.chickenFeed;
    u16 current = *counter;
    if (current < amount) return 0;
    *counter = current - amount;
    return 1;
}

/* Ship one item: add it to the shipment's bin and value, and count large, medium and small fish toward the goal. */
s32 estate_shipment_add(s32 itemId) {
    FarmShipment *shipment;
    s32 id;
    s32 index;

    id = itemId & 0xFFFF;
    shipment = &game_state.progress.estate.shipment;
    if ((u32)(id - ITEM_LARGE_FISH) < 3U) {
        game_state.progress.estate.shipmentTotal++;
        if (game_state.progress.estate.shipmentTotal >= ESTATE_FISH_SHIPPED_GOAL) {
            event_flag_set(EVENT_FLAG_FISH_SHIPPED_200, 1);
        }
    }
    shipment->shippedValue += item_sell_price(id);
    index = estate_item_slot_index(id);
    if (index == -1) {
        return 0;
    }
    shipment->counts[index]++;
    if (shipment->counts[index] >= 1000U) {
        shipment->counts[index] = ESTATE_STOCK_MAX;
    }
    return 1;
}

/* The shipment bin of an item, or -1 when the item is not shipped. */
s16 estate_item_slot_index(s32 itemId) {
    ItemId id;

    id = itemId;
    if (id < ITEM_TURNIP) {
        return -1;
    }
    if (id < ITEM_GRASS) {
        return id - ITEM_TURNIP;
    }
    switch (id) {
    case ITEM_GOLDEN_EGG:
    case ITEM_NORMAL_EGG:
    case ITEM_GOOD_EGG:
    case ITEM_EXCELLENT_EGG:
        return ESTATE_SLOT_EGG;
    case ITEM_MILK_S:
    case ITEM_MILK_M:
    case ITEM_MILK_L:
    case ITEM_MILK_G:
        return ESTATE_SLOT_MILK;
    case ITEM_WOOL_S:
    case ITEM_WOOL_M:
    case ITEM_WOOL_L:
    case ITEM_WOOL_G:
        return ESTATE_SLOT_WOOL;
    case ITEM_MAYONNAISE_S:
    case ITEM_MAYONNAISE_M:
    case ITEM_MAYONNAISE_L:
    case ITEM_MAYONNAISE_G:
        return ESTATE_SLOT_MAYONNAISE;
    case ITEM_CHEESE_S:
    case ITEM_CHEESE_M:
    case ITEM_CHEESE_L:
    case ITEM_CHEESE_G:
        return ESTATE_SLOT_CHEESE;
    case ITEM_YARN_BALL_S:
    case ITEM_YARN_BALL_M:
    case ITEM_YARN_BALL_L:
    case ITEM_YARN_G:
        return ESTATE_SLOT_YARN;
    case ITEM_LARGE_FISH:
    case ITEM_MEDIUM_FISH:
    case ITEM_SMALL_FISH:
        return ESTATE_SLOT_FISH;
    case ITEM_BLUE_GRASS:
    case ITEM_RED_GRASS:
    case ITEM_GREEN_GRASS:
        return ESTATE_SLOT_BLUE_GRASS + (id - ITEM_BLUE_GRASS);
    }
    return -1;
}

/* Empty the barn and coop feed bins. */
void estate_feed_bins_clear(void) {
    s32 i;

    for (i = 0; i < BARN_FEED_BINS; i++) {
        game_state.progress.estate.barnFeedBins[i] = 0;
    }
    for (i = 0; i < COOP_FEED_BINS; i++) {
        game_state.progress.estate.coopFeedBins[i] = 0;
    }
}

/* Set the shipment-settled flag and, on the first clock date index, pay the shipped value
 * into the purse. */
s32 estate_shipment_settle(void) {
    FarmShipment *shipment;

    if (event_flag_get(EVENT_FLAG_SHIPMENT_SETTLED) == 1) return 1;
    event_flag_set(EVENT_FLAG_SHIPMENT_SETTLED, 1);
    if (clock_date_index(game_state.progress.clock.date.season, game_state.progress.clock.date.day) != 0) return 1;
    shipment = &game_state.progress.estate.shipment;
    estate_gold_change(shipment->shippedValue, 0);
    shipment->shippedValue = 0;
    return 1;
}

/* The shipped count of an item's bin, or -1 when the item is not shipped. */
s32 estate_shipment_count(s32 id) {
    u16 *base = game_state.progress.estate.shipment.counts;
    s32 index = estate_item_slot_index((u16)id);
    if (index != -1) {
        return base[index];
    }
    return index;
}

/* Add one item to the basket. Returns 1 when the basket is full and the item was not taken. */
s32 estate_basket_add(s32 itemId) {
    FarmBasket *basket;
    s16 id16;
    ItemId id;
    s32 index;

    id16 = itemId;
    basket = &game_state.progress.estate.basket;
    if (basket->count < BASKET_CAPACITY) {
        id = id16;
        basket->contentsValue += item_sell_price(id);
        basket->count++;
        index = estate_item_slot_index(id);
        if (index == -1) {
            return 0;
        }
        basket->items[index]++;
        return 0;
    }
    return 1;
}

/* Move the basket into the shipment: add its counts and value to the shipment. */
s32 estate_basket_merge(void) {
    FarmShipment *shipment = &game_state.progress.estate.shipment;
    FarmBasket *basket = &game_state.progress.estate.basket;
    s32 total;
    s32 i;

    if (basket->count == 0) {
        return 0;
    }
    for (i = 0; i < SHIPPING_KINDS; i++) {
        total = shipment->counts[i] + basket->items[i];
        shipment->counts[i] = total;
        if ((u16)total >= 1000) {
            shipment->counts[i] = 999;
        }
        basket->items[i] = 0;
    }
    shipment->shippedValue += basket->contentsValue;
    basket->count = 0;
    basket->contentsValue = 0;
    return 1;
}

/* Take the ESTATE_FLAG_ITEM_83 item once: sets ESTATE_FLAG_TAKE_ONCE and returns 1 the first time. */
s32 estate_flag_take_once(void) {
    u32 *flags = &game_state.progress.estate.estateFlags;
    u32 v = *flags;
    if ((v & (ESTATE_FLAG_ITEM_83 | ESTATE_FLAG_TAKE_ONCE)) == ESTATE_FLAG_ITEM_83) {
        *flags = v | ESTATE_FLAG_TAKE_ONCE;
        return 1;
    }
    return 0;
}

/* Restore the item enables and search the screen's records, unless the debug flag
 * turns the 2D fixtures off. */
void estate_items_refresh(void) {
    if (!(game_state.debugFlags & DEBUG_FLAG_NO_2D_FIXTURES)) {
        estate_items_restore();
        estate_search_run();
        estate_record18_bounce_step();
    }
}

/* Refresh item and record state after a selection; in spring, also re-place the
 * stamp trigger and the dog when their item is a tool. */
void estate_selection_refresh(s32 selection) {
    s16 kind;

    estate_items_enable_by_flags(selection);
    estate_records_kind_refresh(selection);
    if (game_state.progress.clock.date.season == SEASON_SPRING) {
        kind = game_state.runs.records[18]->location;
        if (kind == ITEM_SILVER_AX || (u32)(kind - ITEM_FISHING_ROD) < 0xB) {
            estate_trigger_stamp_first();
        }
        kind = game_state.animals.dog.actor.place.location;
        if (kind == ITEM_SILVER_AX || (u32)(kind - ITEM_FISHING_ROD) < 0xB) {
            dog_place(0);
        }
    }
}

/* Re-enable every record table from its counts, then release the slots' claims. */
void estate_record_tables_refresh(void) {
    s32 i = 0;
    do {
        estate_record_table_enable(game_state.runs.counts[i], game_state.runs.records[i]);
        i++;
    } while (i < ESTATE_RUNS);
    estate_slots_set_claimed(0);
}

/* Re-enable the records gated by the season. */
void estate_seasonal_refresh(void) {
    estate_items_enable_by_flags(game_state.progress.clock.date.season);
}

/* Pick up the item of a field record when it is enabled for the current season.
 * Aja grapes can be taken once; anything else clears the record's enable bit. Returns
 * 1 when picked up, and reports the item in `result`. Weeds and stones take their
 * seasonal form. */
s32 estate_field_pickup(FarmEstateRecord *record, FarmStoredItem *result) {
    u16 flags;
    u16 required;

    required = estate_season_bit() | 0x81;
    flags = record->recordFlags;
    if (required != (flags & required)) return 0;
    if (record->itemId == ITEM_AJA_GRAPES) {
        if (flags & ESTATE_RECORD_CLAIMED) return 0;
        record->recordFlags = flags | ESTATE_RECORD_CLAIMED;
    } else {
        record->recordFlags = flags & ~1;
    }
    if (result != 0) {
        result->id = record->itemId;
        result->count = 1;
    }
    if (game_state.progress.clock.date.season == SEASON_FALL && result->id == ITEM_WEEDS) result->id = ITEM_DEAD_WEED;
    if (result->id == ITEM_STONE && game_state.progress.clock.date.season == SEASON_WINTER) result->id = ITEM_WINTER_STONE;
    if (result->id == ITEM_BRANCH && game_state.progress.clock.date.season == SEASON_WINTER) result->id = ITEM_WINTER_BRANCH;
    return 1;
}

/* The item a record shows this season: 0 when the record is not enabled for it, and
 * weeds turn to dead weed in fall. */
s16 estate_record_item_id(FarmEstateRecord *record) {
    s16 itemId;
    s16 result;
    s32 mask;
    s32 masked_flags;

    mask = (estate_season_bit() | 1) & 0xFFFF;
    masked_flags = record->recordFlags & mask;
    result = 0;
    if (mask != masked_flags) return result;
    itemId = record->itemId;
    result = itemId;
    if (game_state.progress.clock.date.season == SEASON_FALL) {
        result = itemId;
        if (itemId == ITEM_WEEDS) result = ITEM_DEAD_WEED;
    }
    return result;
}

/* Reset the item run; unless `clear` is set, also reset the egg run and clear the enable
 * bit of the stamp records in run 18. */
void estate_result_window_clear(unsigned short flags, s32 clear) {
    FarmEstateRecord *trigger;
    estate_record_tables_refresh();
    estate_items_restore();
    estate_items_enable_by_flags(flags);
    estate_records_kind_refresh(flags);
    estate_items_reset(game_state.runs.records[17], game_state.runs.counts[17]);
    if (clear == 0) {
        estate_items_reset(game_state.runs.records[16], game_state.runs.counts[16]);
        trigger = &game_state.runs.records[18][1];
        trigger->recordFlags &= 0xFFFE;
        trigger = game_state.runs.records[18];
        trigger->recordFlags &= 0xFFFE;
    }
}

/* Stamps a record with a value, an item id and a position, and enables it. */
static void estate_record_stamp(FarmEstateRecord *record, s32 id, s32 value, VECTOR position) {
    record->location = value;
    record->itemId = id;
    record->position.vx = position.vx;
    record->position.vy = position.vy;
    record->position.vz = position.vz;
    record->recordFlags |= ESTATE_RECORD_ENABLED;
}

/* Zero `count` records and clear their enable bits. */
void estate_items_reset(FarmEstateRecord *p, s32 count) {
    if (count <= 0) return;
    do {
        u16 x = p->recordFlags;
        count--;
        p->location = 0;
        p->itemId = 0;
        p->position.vx = 0;
        p->position.vy = 0;
        p->position.vz = 0;
        p->recordFlags = x & 0xFFFE;
        p++;
    } while (count != 0);
}

/* Stamp the record of an egg (in table slot `index`), the dog ball or the basket. */
void estate_marker_stamp(u16 kind, VECTOR position, u16 value, s32 index) {
    FarmEstateRecord *record;

    switch (kind) {
    case ITEM_GOLDEN_EGG:
    case ITEM_NORMAL_EGG:
    case ITEM_GOOD_EGG:
    case ITEM_EXCELLENT_EGG:
        record = &game_state.runs.records[16][index];
        break;
    case ITEM_DOG_BALL:
        record = &game_state.runs.records[18][1];
        break;
    case ITEM_BASKET:
        record = game_state.runs.records[18];
        break;
    default:
        return;
    }
    estate_record_stamp(record, kind, value, position);
}

/* Look up an egg, the dog ball or the basket: copy its position and area, and return
 * whether its enable bit is set. */
s32 estate_result_select(u16 kind, VECTOR *position, s16 *area, s32 index) {
    FarmEstateRecord *record;
    switch (kind) {
    case ITEM_GOLDEN_EGG:
    case ITEM_NORMAL_EGG:
    case ITEM_GOOD_EGG:
    case ITEM_EXCELLENT_EGG:
        record = game_state.runs.records[16] + index;
        break;
    case ITEM_DOG_BALL:
        record = game_state.runs.records[18] + 1;
        break;
    case ITEM_BASKET:
        record = game_state.runs.records[18];
        break;
    default:
        return 0;
    }
    position->vx = record->position.vx;
    position->vy = record->position.vy;
    position->vz = record->position.vz;
    *area = record->location;
    return record->recordFlags & 1;
}

/* Enable (publish == 1) or disable the first record of item `id` in runs[10]. */
void estate_record_publish(s16 id, s32 publish) {
    FarmEstateRecord *record;

    record = estate_record_find(id, game_state.runs.records[10], 13);
    if (record) {
        if (publish == 1)
            record->recordFlags |= ESTATE_RECORD_ENABLED;
        else
            record->recordFlags &= 0xFFFE;
    }
}

/* Set the enable bit and egg item of the two records at index 10 and 11 of runs[3],
 * from whether each farm mode is available and the animal in its slot. */
static void estate_enable_from_modes(void) {
    s32 i = 2;
    do {
        s16 v;
        s32 x;
        v = game_state.animals.slots[i].animal;
        if (v != 1) v = 0;
        x = farm_mode_available(i);
        estate_item_enable(i - 2, x == 1, v);
        i++;
    } while (i < 4);
}

/* Submit a record for pickup by hand when its item can be picked up by hand. */
s32 estate_record_submit(FarmEstateRecord *record) {
    s32 result;

    result = 0;
    if ((estate_record_item_id(record) & 0xFFFF) == 0) {
        return 0;
    }
    if (farm_area.mode.place.value == PLACE_13) {
        return 0;
    }
    if (item_tile_hand_pickable(record->itemId, 0) == 1) {
        result = 1;
        record->recordFlags = record->recordFlags & 0xFFFE;
        item_request_submit(record->itemId, record->position);
    }
    return result;
}

/* Submit a record that a tool can clear, when the tool's power and reach are high enough. */
s32 estate_record_submit_limited(FarmEstateRecord *record, s32 reach, s32 power) {
    u16 itemId;
    s32 result;
    s16 reachLimit;
    s16 powerLimit;

    result = 0;
    reachLimit = reach;
    powerLimit = power;
    itemId = estate_record_item_id(record);
    if (itemId == 0) {
        return 0;
    }
    if (item_tile_clear_by_tool(itemId, 0) == 1) {
        if (powerLimit >= item_tile_min_power(itemId, 0)) {
            if (reachLimit >= item_tile_min_reach(itemId, 0)) {
                result = 1;
                record->recordFlags = record->recordFlags & 0xFFFE;
                item_request_submit(record->itemId, record->position);
            }
        }
    }
    return result;
}

/* Submit a record that an alternate tool clears, when the power and reach are high
 * enough. A stump adds 6 to the lumber count and a branch 1. Item 0x174 instead sets the
 * gift-pending status and posts trigger mode 5, and is not submitted. Returns 1 once
 * submitted. */
s32 estate_record_submit_costed(FarmEstateRecord *record, s32 reach, s32 power) {
    u16 itemId;
    s32 result;
    s16 reachLimit;
    s16 powerLimit;
    u32 flags;

    result = 0;
    reachLimit = reach;
    powerLimit = power;
    itemId = estate_record_item_id(record);
    if (itemId == 0) {
        return 0;
    }

    flags = game_state.statusFlags & ~STATUS_FLAG_GIFT_PENDING;
    game_state.statusFlags = flags;
    if (itemId == 0x174) {
        game_state.statusFlags = flags | STATUS_FLAG_GIFT_PENDING;
        event_trigger_mode_set(5);
        return 0;
    }

    if (item_tile_clear_by_alt_tool(itemId, 0) == 1) {
        if (powerLimit >= item_tile_min_power(itemId, 0)) {
            if (reachLimit >= item_tile_min_reach(itemId, 0)) {
                record->recordFlags = record->recordFlags & 0xFFFE;
                switch (itemId) {
                case ITEM_STUMP:
                case 0x172:
                case 0x173:
                case 0x174:
                    estate_lumber_add(LUMBER_ADD_STUMP, 0);
                    break;
                case ITEM_BRANCH:
                    estate_lumber_add(LUMBER_ADD_PICKED, 0);
                    break;
                }
                item_request_submit(record->itemId, record->position);
                result = 1;
            }
        }
    }
    return result;
}

/* Return the active record when a request lies within 20 up or down and 200 sideways
 * of it, and start its countdown. */
FarmEstateRecord *estate_request_near_active(FarmGroundProbe *request) {
    FarmEstateRecord *active;
    active = game_state.runs.records[18];
    if (farm_area.mode.place.location == active->location) {
        if (active->position.vy - 20 < request->y) {
            if (request->y < active->position.vy + 20) {
                if (active->position.vx - 200 < request->x) {
                    if (request->x < active->position.vx + 200) {
                        if (active->position.vz - 200 < request->z) {
                            if (request->z < active->position.vz + 200) {
                                record18_bounce_step = 3;
                                return active;
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}

static void estate_stub_1(void) {
}

/* Count down the bounce of record 18; when the countdown reaches zero its bounce bit is
 * cleared. The countdown stops while the game is paused. */
static void estate_record18_bounce_step(void) {
    FarmState *state = &game_state;
    FarmEstateRecord *p = state->runs.records[18];
    u16 flags = p->recordFlags;
    if ((flags & ESTATE_RECORD_BOUNCE) && !(state->globalFlags & GLOBAL_FLAG_PAUSED)) {
        if (record18_bounce_step == 0) {
            p->recordFlags = flags & 0xFEFF;
            return;
        }
        record18_bounce_step--;
    }
}

/* Set (mode != 1) or clear (mode == 1) the claimed bit of the sixteen slots of runs[6]. */
void estate_slots_set_claimed(s32 mode) {
    FarmEstateRecord *item = game_state.runs.records[6];
    s32 one = 1;
    s32 i = 15;

    do {
        if (mode == one) {
            item->recordFlags &= 0xBFFF;
        } else {
            item->recordFlags |= ESTATE_RECORD_CLAIMED;
        }
        item++;
        i--;
    } while (i >= 0);
}

/* Count the sixteen slots of runs[6] that are not claimed. */
s32 estate_slots_free_count(void) {
    FarmEstateRecord *item = game_state.runs.records[6];
    s32 i;
    s32 count = 0;

    for (i = 15; i >= 0; i--) {
        if (!(item->recordFlags & ESTATE_RECORD_CLAIMED)) count++;
        item++;
    }
    return count;
}

/* Start a flower request when the vase is set and none is pending. Returns 1 when the
 * request, a flower from ITEM_MOON_DROP_FLOWER to ITEM_TOY_FLOWER, was taken. */
s32 estate_request_begin(s32 request) {
    s16 value = request;
    if ((game_state.progress.estate.estateFlags & ESTATE_FLAG_VASE_SET) == 0) return 0;
    if (game_state.progress.estate.pendingRequest != 0) {
        return 0;
    }
    if (value <= ITEM_TOY_FLOWER) {
        if (value >= ITEM_MOON_DROP_FLOWER) {
            game_state.progress.estate.pendingRequest = value;
            return 1;
        }
    }
    return 0;
}

/* While a flower request is pending, it is dropped once its season is over. The player's
 * fatigue then drops by 5 per tick, down to zero. */
void estate_fatigue_tick(void) {
    if (game_state.progress.estate.pendingRequest == 0) {
        return;
    }
    switch (game_state.progress.estate.pendingRequest) {
    case ITEM_MOON_DROP_FLOWER:
    case ITEM_TOY_FLOWER:
        if (game_state.progress.clock.date.season != SEASON_SPRING) {
            estate_request_clear();
        }
        break;
    case ITEM_PINK_CAT_FLOWER:
        if (game_state.progress.clock.date.season != SEASON_SUMMER) {
            estate_request_clear();
        }
        break;
    case ITEM_BLUE_MAGIC_RED_FLOWER:
    case ITEM_RED_MAGIC_RED_FLOWER:
        if (game_state.progress.clock.date.season != SEASON_FALL) {
            estate_request_clear();
        }
        break;
    }
    game_state.player.fatigue -= 5;
    if (game_state.player.fatigue < 0) {
        game_state.player.fatigue = 0;
    }
}

void estate_request_clear(void) {
    game_state.progress.estate.pendingRequest = 0;
}

/* Place the second stamp of run 18 at its fixed point and make it the current object. */
void estate_trigger_stamp_second(void) {
    FarmEstateRecord *q = &game_state.runs.records[18][1];
    estate_current_object = q;
    q->location = 1;
    q->position.vx = -1200;
    q->position.vy = 0;
    q->position.vz = 2900;
}

/* Place record 18 at its fixed point and make it the current object. */
void estate_trigger_stamp_first(void) {
    FarmEstateRecord *p;
    p = game_state.runs.records[18];
    estate_current_object = p;
    p->location = 1;
    p->position.vx = -5000;
    p->position.vy = 0;
    p->position.vz = 2300;
}

/* Check the basket and the dog ball against the probe point: place the stamps they are
 * near, and disarm the trigger when the dog ball lies outside places 1 to 6. */
void estate_basket_check(void) {
    EstatePlaceProbe probe;
    s32 near;
    s32 disarm;
    near = 0;
    if (estate_result_select(ITEM_BASKET, &probe.position, &probe.place.location, 0) == 0 &&
        (game_state.progress.estate.estateFlags & ESTATE_FLAG_BASKET) != 0 && game_state.player.held[HELD_ITEM].id == ITEM_BASKET) {
        near = 1;
        game_state.runs.records[18]->recordFlags |= ESTATE_RECORD_ENABLED;
    }
    if (farm_callback_capped(&probe.position, probe.place, 0, 0, 0, 0, 0, OCCUPANT_TILE) != 0) near = 1;
    disarm = 0;
    if (near != 0) estate_trigger_stamp_first();
    estate_result_select(ITEM_DOG_BALL, &probe.position, &probe.place.location, 0);
    near = farm_callback_capped(&probe.position, probe.place, 0, 0, 0, 0, 0, OCCUPANT_TILE) != 0;
    if (*(s16 *)&probe.place.location >= 7 || *(s16 *)&probe.place.location <= 0) disarm = 1;
    if (near != 0) estate_trigger_stamp_second();
    if (disarm != 0) estate_trigger_disarm();
}

/* Fire each armed trigger entry whose map is current and whose area is entered. */
void estate_trigger_fire(void) {
    FarmEstateRecord *entry;
    s32 hit;

    if (farm_area.mode.place.value == 0 && (game_state.progress.estate.estateFlags & ESTATE_FLAG_TRIGGER_ARMED)) {
        entry = &game_state.runs.records[18][1];
        if (farm_area.mode.place.location != entry->location) return;
        hit = farm_zone_find(farm_area.mode.data, &entry->position, 0) != 0;
        if (farm_settle_on_ground(&entry->position, FARM_SETTLE_GROUND_ONLY) != 0) hit = 1;
        if (hit) estate_trigger_disarm();
    }
    if (game_state.progress.estate.estateFlags & ESTATE_FLAG_BASKET) {
        entry = &game_state.runs.records[18][0];
        if (farm_area.mode.place.location != entry->location) return;
        hit = farm_zone_find(farm_area.mode.data, &entry->position, 0) != 0;
        if (farm_settle_on_ground(&entry->position, FARM_SETTLE_GROUND_ONLY) != 0) hit = 1;
        if (hit) estate_trigger_stamp_first();
    }
}

/* Clear the trigger-armed flag and the enable bit of the second stamp, and reset day counter 0x10. */
static void estate_trigger_disarm(void) {
    FarmState *fs = &game_state;
    u32 flags = fs->progress.estate.estateFlags;
    if (flags & ESTATE_FLAG_TRIGGER_ARMED) {
        u32 mask = ~ESTATE_FLAG_TRIGGER_ARMED;
        FarmEstateRecord *trigger = &fs->runs.records[18][1];
        u32 cleared_flags = flags & mask;
        fs->progress.estate.estateFlags = cleared_flags;
        trigger->recordFlags &= 0xFFFE;
        fs->days.count0[0x10] = 0;
    }
}

/* Set the enable bit and the egg item of coop record `index` (records 10 and 11 of runs[3]). */
static void estate_item_enable(s32 index, s32 mode, s16 value) {
    FarmEstateRecord *item = game_state.runs.records[3] + 10 + index;

    if (mode == 1)
        item->recordFlags = item->recordFlags | 1;
    else
        item->recordFlags = item->recordFlags & 0xFFFE;
    if (value == 0) {
        item->itemId = ITEM_GOOD_EGG;
        return;
    }
    item->itemId = ITEM_GOLDEN_EGG;
}

/* Refresh the item ids of every record table from the kind `mode`. */
static void estate_records_kind_refresh(s16 mode) {
    s32 i = 0;
    do {
        estate_record_ids_update(mode, game_state.runs.counts[i], game_state.runs.records[i]);
        i++;
    } while (i < ESTATE_RUNS);
}

/* The height steps of record 18's bounce, applied to its position while it bounces. */
static s16 record18_bounce_offsets[4] = {
    -20, -40, -70, -40,
};

/* The gate positions for each house level: one row per gate item, keyed by its item id. */
static FarmResultRow estate_result_rows_house0[2] = {{{-120, -200, 72}, 113}, {{-843, -600, 1429}, 392}};

static FarmResultRow estate_result_rows_house1[10] = {
    {{500, -200, -600}, 113},
    {{722, -600, 1416}, 392},
    {{-2283, -200, 1473}, 114},
    {{-2734, -200, 1473}, 115},
    {{-1464, -600, 1473}, 116},
    {{-3104, -200, 243}, 117},
    {{-2242, -200, 243}, 118},
    {{-2652, -200, 243}, 119},
    {{-972, -200, 1473}, 120},
    {{-1873, -200, 243}, 121},
};

static FarmResultRow estate_result_rows_house2[10] = {
    {{-180, -300, -3000}, 113},
    {{736, -600, 3072}, 392},
    {{-2663, -200, 3062}, 114},
    {{-3114, -200, 2365}, 115},
    {{-3114, -200, 2734}, 116},
    {{-2335, -200, 1832}, 117},
    {{-1925, -200, 3062}, 118},
    {{-2704, -200, 1832}, 119},
    {{-3114, -200, 1832}, 120},
    {{-1884, -200, 1832}, 121},
};

/* The item each kind gives for each state bit of its record (13, 11, 12): a flower or a grass. */
static KindValues estate_kind_values[4] = {
    {ITEM_MOON_DROP_FLOWER, ITEM_TOY_FLOWER, ITEM_BLUE_GRASS},
    {ITEM_PINK_CAT_FLOWER, ITEM_PINK_CAT_FLOWER, ITEM_RED_GRASS},
    {ITEM_BLUE_MAGIC_RED_FLOWER, ITEM_BLUE_MAGIC_RED_FLOWER, ITEM_GREEN_GRASS},
    {0, 0, 0},
};

/* Set the item of each record from its state flags, using the kind's three values. */
static void estate_record_ids_update(s16 kind, s16 count, FarmEstateRecord *records) {
    /* Pick the item value for the record's state flags, or 0 when none is set. */
    s32 estate_kind_value_for_flags(u16 flags, s16 valueWhenBit13, s16 valueWhenBit11, s16 valueWhenBit12)
    {
        if (flags & 0x2000) return valueWhenBit13;
        if (flags & 0x800) return valueWhenBit11;
        if (flags & 0x1000) return valueWhenBit12;
        return 0;
    }

    s32 i;
    s32 itemId;

    for (i = 0; i < count; i++) {
        itemId = estate_kind_value_for_flags(records[i].recordFlags, estate_kind_values[kind].valueWhenBit13,
                              estate_kind_values[kind].valueWhenBit11, estate_kind_values[kind].valueWhenBit12);
        if (itemId != 0) {
            records[i].itemId = itemId;
        }
    }
}

/* Enable or disable the thirteen records of runs[10] from the status flags and the season:
 * the basket, the feathers, the seed packets and the rucksack, whose item steps up with its upgrade. */
static void estate_items_enable_by_flags(s16 unused0) {
    FarmEstateRecord *item;
    u16 *id;
    FarmEstateRecord *items;
    s32 i;
    u16 flags;
    u32 statusFlags;

    items = game_state.runs.records[10];
    for (i = 0; i < 13; i++) {
        item = &items[i];
        id = &item->itemId;
        switch ((s16)*id) {
        case ITEM_BASKET:
            if (!(game_state.progress.estate.statusFlags & ESTATE_STATUS_NO_BASKET)) {
                item->recordFlags &= 0xFFFE;
            } else {
                item->recordFlags |= ESTATE_RECORD_ENABLED;
            }
            break;
        case ITEM_BLUE_FEATHER:
            if (!(game_state.progress.estate.statusFlags & ESTATE_STATUS_AFFECTION_OVER_50000)) {
                item->recordFlags &= 0xFFFE;
            } else {
                item->recordFlags |= ESTATE_RECORD_ENABLED;
            }
            break;
        case ITEM_SPRING_SEEDS:
            if (game_state.progress.clock.date.season != SEASON_SPRING && game_state.progress.estate.greenhouseLevel == 0) {
                item->recordFlags &= 0xFFFE;
            } else {
                item->recordFlags |= ESTATE_RECORD_ENABLED;
            }
            break;
        case ITEM_SUMMER_SEEDS:
            if (game_state.progress.clock.date.season != SEASON_SUMMER && game_state.progress.estate.greenhouseLevel == 0) {
                item->recordFlags &= 0xFFFE;
            } else {
                item->recordFlags |= ESTATE_RECORD_ENABLED;
            }
            break;
        case ITEM_SPRING_SEEDS_12A:
            if (game_state.progress.clock.date.season != SEASON_FALL && game_state.progress.estate.greenhouseLevel == 0) {
                item->recordFlags &= 0xFFFE;
            } else {
                item->recordFlags |= ESTATE_RECORD_ENABLED;
            }
            break;
        }
        flags = item->recordFlags;
        if (flags & ESTATE_RECORD_RUCKSACK) {
            statusFlags = game_state.progress.estate.statusFlags;
            if (statusFlags & ESTATE_STATUS_NO_RUCKSACK) {
                *id = ITEM_RUCKSACK_LEVEL_2;
                item->recordFlags = flags | ESTATE_RECORD_ENABLED;
            } else if (statusFlags & ESTATE_STATUS_RUCKSACK_UPGRADE_OPEN) {
                *id = ITEM_RUCKSACK_LEVEL_3;
                item->recordFlags = flags | ESTATE_RECORD_ENABLED;
            } else {
                item->recordFlags = flags & 0xFFFE;
            }
        }
    }
}

/* Enable the records flagged 0x20 and disable the ones flagged 0x40. */
static void estate_record_table_enable(s16 count, FarmEstateRecord *records) {
    s32 i;

    for (i = 0; i < count; i++) {
        u16 flags = records[i].recordFlags;
        if (flags & 0x20)
            records[i].recordFlags = flags | ESTATE_RECORD_ENABLED;
        else if (flags & 0x40)
            records[i].recordFlags = flags & 0xFFFE;
    }
}

/* Re-enable the tools, appliances, feed bins and the enable bits of the animal modes from
 * the state flags; then the gate results, when the farm is at place 2. */
static void estate_items_restore(void) {
    FarmState *farm;
    FarmState *state;
    u8 *bins;
    FarmEstateRecord *items;
    s32 count;
    u32 mask;
    s32 i;

    farm = &game_state;
    items = farm->runs.records[0];
    count = farm->runs.counts[0];
    mask = farm->progress.estate.estateFlags;
    estate_item_set_enable(ITEM_BEEHIVE, items, count, mask, ESTATE_FLAG_ITEM_83);
    items = farm->runs.records[1];
    count = farm->runs.counts[1];
    estate_item_set_enable(ITEM_VASE, items, count, mask, ESTATE_FLAG_VASE_SET);
    estate_item_set_enable(0x188, items, count, farm->animals.horse.actor.flags, 0x40);
    mask = farm->progress.estate.utensils;
    estate_item_set_enable(ITEM_KNIFE, items, count, mask, UTENSIL_FLAG_KNIFE);
    estate_item_set_enable(ITEM_FRYING_PAN, items, count, mask, UTENSIL_FLAG_FRYING_PAN);
    estate_item_set_enable(ITEM_POT, items, count, mask, UTENSIL_FLAG_POT);
    estate_item_set_enable(ITEM_MIXER, items, count, mask, UTENSIL_FLAG_MIXER);
    estate_item_set_enable(ITEM_WHISK, items, count, mask, UTENSIL_FLAG_WHISK);
    estate_item_set_enable(ITEM_ROLLING_PIN, items, count, mask, UTENSIL_FLAG_ROLLING_PIN);
    estate_item_set_enable(ITEM_OVEN, items, count, mask, UTENSIL_FLAG_OVEN);
    estate_item_set_enable(ITEM_SEASONING_SET, items, count, mask, UTENSIL_FLAG_SEASONING_SET);
    bins = farm->progress.estate.barnFeedBins;
    count = farm->runs.counts[2];
    items = farm->runs.records[2];
    for (i = 0; i < count; i++) {
        if (bins[i] != 0) {
            items[i].recordFlags |= ESTATE_RECORD_ENABLED;
        } else {
            items[i].recordFlags &= 0xFFFE;
        }
    }
    state = &game_state;
    bins = state->progress.estate.coopFeedBins;
    count = state->runs.counts[3];
    items = state->runs.records[3];
    for (i = 0; i < count; i++) {
        if (bins[i] != 0) {
            items[i].recordFlags |= ESTATE_RECORD_ENABLED;
        } else {
            items[i].recordFlags &= 0xFFFE;
        }
    }
    estate_enable_from_modes();
    if (farm_area.mode.place.location == PLACE_2) {
        estate_gate_results_place();
    }
}

/* Set or clear the enable bit of the record with this item id. */
static void estate_item_set_enable(s32 itemId, FarmEstateRecord *items, s32 count, u32 mask, u32 bits) {
    FarmEstateRecord *p = estate_record_find(itemId, items, count);
    if (p != 0) {
        if (mask & bits)
            p->recordFlags |= ESTATE_RECORD_ENABLED;
        else
            p->recordFlags &= 0xFFFE;
    }
}

/* Find the first of `count` records with item id `value`. */
static FarmEstateRecord *estate_record_find(s16 itemId, FarmEstateRecord *items, s32 count) {
    s32 i;

    for (i = 0; i < count; i++) {
        if (items[i].itemId == itemId) {
            return &items[i];
        }
    }
    return 0;
}

/* Give each gate record the position from the table for the current house level. */
static void estate_gate_results_place(void) {
    FarmEstateRecord *gate;
    FarmResultRow *table;
    s32 limit;
    s32 count;
    s32 i;
    s32 j;
    s32 houseLevel;

    gate = game_state.runs.records[1];
    count = game_state.runs.counts[1];
    houseLevel = game_state.progress.estate.houseLevel;
    switch (houseLevel) {
    case 0:
    default:
        table = estate_result_rows_house0;
        limit = 2;
        break;
    case 1:
        table = estate_result_rows_house1;
        limit = 10;
        break;
    case 2:
        table = estate_result_rows_house2;
        limit = 10;
        break;
    }
    for (i = 0; i < count; i++, gate++) {
        for (j = 0; j < limit; j++) {
            if (gate->itemId == table[j].key) {
                gate->position.vx = table[j].words[0];
                gate->position.vy = table[j].words[1];
                gate->position.vz = table[j].words[2];
                break;
            }
        }
    }
}

/* The season's bit in a record's flags: spring 2, summer 4, fall 8, winter 16; spring for any
 * other season. */
static s32 estate_season_bit(void) {
    s16 season;

    season = game_state.progress.clock.date.season;
    switch (season) {
    case SEASON_SPRING:
    default:
        return 2;
    case SEASON_SUMMER:
        return 4;
    case SEASON_FALL:
        return 8;
    case SEASON_WINTER:
        return 16;
    }
}

/* Whether a projected point lies inside the screen box that the sprites are drawn in. */
int estate_point_on_screen(s32 x, s32 y) {
    int result = 0;
    if ((u32)(x - ESTATE_SCREEN_BOX_LEFT) < ESTATE_SCREEN_BOX_WIDTH) result = (u32)(y - ESTATE_SCREEN_BOX_TOP) < ESTATE_SCREEN_BOX_HEIGHT;
    return result;
}

/* Projects a point onto the screen; returns 1 when it is visible, and fills in its screen position. */
s32 estate_point_project(SVECTOR *input, FarmScreenPoint *result) {
    s32 depth;
    u32 limit;

    estate_screen_xy.y = 1500;
    estate_screen_xy.x = 1500;
    estate_point.vx = input->vx;
    estate_point.vy = input->vy;
    estate_point.vz = input->vz;
    depth = gte_project_vertex(&estate_screen_xy, &estate_point);
    if (depth <= 0) return 0;
    if (estate_point_on_screen(estate_screen_xy.x, estate_screen_xy.y) == 0) return 0;
    result->x = estate_screen_xy.x;
    limit = estate_screen_xy.y;
    result->depth = depth;
    result->y = limit;
    return 1;
}

/* The search handler a screen's resource set picks. */
enum EstateSearchKind {
    ESTATE_SEARCH_LOOKUP = 0,
    ESTATE_SEARCH_LOOKUP_1 = 1,
    ESTATE_SEARCH_LOOKUP_2 = 2,
    ESTATE_SEARCH_LOOKUP_3 = 3,
    ESTATE_SEARCH_VALUE_4 = 4,
    ESTATE_SEARCH_ACTION_STATUS = 5,
    ESTATE_SEARCH_PICTURE = 6,
    ESTATE_SEARCH_VALUE = 7,
    ESTATE_SEARCH_STATUS = 8,
    ESTATE_SEARCH_VALUE_9 = 9,
};

/* Runs the screen's search handler over the records of the current map, then over the
 * shared records of runs 18 and 16. */
static void estate_search_run(void) {
    s32 (*handler)();
    s32 run;
    FarmEstateRecord record;

    switch (farm_area.mode.data->head.resourceSet) {
    case ESTATE_SEARCH_LOOKUP:
    case ESTATE_SEARCH_LOOKUP_1:
    case ESTATE_SEARCH_LOOKUP_2:
    case ESTATE_SEARCH_LOOKUP_3:
        handler = estate_code_lookup;
        break;
    case ESTATE_SEARCH_VALUE_4:
        handler = estate_code_value_4;
        break;
    case ESTATE_SEARCH_ACTION_STATUS:
        handler = estate_action_status;
        break;
    case ESTATE_SEARCH_PICTURE:
        handler = item_code_to_picture;
        break;
    case ESTATE_SEARCH_VALUE:
        handler = estate_code_value;
        break;
    case ESTATE_SEARCH_STATUS:
        handler = estate_code_status;
        break;
    case ESTATE_SEARCH_VALUE_9:
        handler = estate_code_value_9;
        break;
    default:
        return;
    }
    switch (farm_area.mode.place.location) {
    case PLACE_FARM: run = 0; break;
    case PLACE_2: run = 1; break;
    case PLACE_4: run = 3; break;
    case PLACE_5: run = 2; break;
    case PLACE_30: run = 4; break;
    case PLACE_8: run = 5; break;
    case PLACE_14: run = 7; break;
    case PLACE_1A: run = 9; break;
    case PLACE_1D: run = 8; break;
    case PLACE_1E: run = 10; break;
    case PLACE_22: run = 6; break;
    case PLACE_34: run = 11; break;
    case PLACE_C: run = 12; break;
    case PLACE_D: run = 13; break;
    case PLACE_E: run = 14; break;
    case PLACE_F: run = 15; break;
    default: run = -1; break;
    }
    if (run >= 0) {
        estate_search_records(game_state.runs.counts[run], game_state.runs.records[run],
                      handler);
    }
    estate_search_records(1, game_state.runs.records[18] + 1, handler);
    record = game_state.runs.records[18][0];
    if (record.recordFlags & ESTATE_RECORD_BOUNCE) {
        record.position.vy += record18_bounce_offsets[record18_bounce_step];
    }
    estate_search_records(1, &record, handler);
    estate_search_records(10, game_state.runs.records[16], handler);
}

/* Draw a sprite at each record of this season and place that is on screen; the handler
 * picks the sprite and its cell. */
static void estate_search_records(s32 count, FarmEstateRecord *record, s32 (*callback)()) {
    SVECTOR input;
    FarmScreenPoint position;
    u8 callback_value;
    s32 callback_result;
    s32 mode_bits;
    s32 remaining;
    s32 (*callback_fn)();
    FarmModeGlobal *mode_data;

    mode_bits = (estate_season_bit() | 1) & 0xFFFF;
    if (callback != 0 && count > 0) {
        callback_fn = callback;
        mode_data = &farm_area.mode;
        remaining = count;
        do {
            if ((record->recordFlags & mode_bits) == mode_bits &&
                record->location == mode_data->place.location) {
                input.vx = record->position.vx + ESTATE_SPRITE_OFFSET_X;
                input.vy = record->position.vy + ESTATE_SPRITE_OFFSET_Y;
                input.vz = record->position.vz + ESTATE_SPRITE_OFFSET_Z;
                if (estate_point_project(&input, &position) != 0 &&
                    callback_fn(record->itemId, &callback_value, &callback_result) != 0) {
                    anim_draw_sprt_cursor(&current_frame->ot[position.depth >> ESTATE_OT_DEPTH_SHIFT],
                                  &game_state.work.sprites[callback_result], callback_value, 0,
                                  position.x, position.y);
                }
            }
            remaining--;
            record++;
        } while (remaining != 0);
    }
}

/* Look up an action item's status (its sprite cell) and whether it takes the seasonal
 * sprite: stones, rocks, stumps and branches in winter, weeds in fall. Returns 1 when the
 * item is known. */
static s32 estate_action_status(s32 action, s32 *result, s32 *status) {
    s32 recognized;

    recognized = 1;
    *result = 0;
    *status = 0;
    switch (action) {
    case ITEM_STONE:
        if (game_state.progress.clock.date.season == SEASON_WINTER) *result = ESTATE_SPRITE_SEASONAL;
        *status = 0;
        break;
    case ITEM_BIG_STONE:
        if (game_state.progress.clock.date.season == SEASON_WINTER) *result = ESTATE_SPRITE_SEASONAL;
        *status = 1;
        break;
    case ITEM_ROCK:
        if (game_state.progress.clock.date.season == SEASON_WINTER) *result = ESTATE_SPRITE_SEASONAL;
        *status = 2;
        break;
    case ITEM_STUMP:
        if (game_state.progress.clock.date.season == SEASON_WINTER) *result = ESTATE_SPRITE_SEASONAL;
        *status = 3;
        break;
    case ITEM_BRANCH:
        if (game_state.progress.clock.date.season == SEASON_WINTER) *result = ESTATE_SPRITE_SEASONAL;
        *status = 4;
        break;
    case ITEM_WEEDS:
        if (game_state.progress.clock.date.season == SEASON_FALL) *result = ESTATE_SPRITE_SEASONAL;
        *status = 5;
        break;
    case ITEM_MOON_DROP_FLOWER:
        *status = 6;
        break;
    case ITEM_PINK_CAT_FLOWER:
        *status = 7;
        break;
    case ITEM_BLUE_MAGIC_RED_FLOWER:
        *status = 8;
        break;
    case ITEM_RED_MAGIC_RED_FLOWER:
        *status = 19;
        break;
    case ITEM_TOY_FLOWER:
        *status = 9;
        break;
    case ITEM_ORANGE_CUP_FRUIT:
        *status = 10;
        break;
    case ITEM_BAMBOO_SHOOT:
        *status = 11;
        break;
    case ITEM_WILD_GRAPE:
        *status = 12;
        break;
    case ITEM_MUSHROOM:
        *status = 13;
        break;
    case ITEM_POISONOUS_MUSHROOM:
        *status = 14;
        break;
    case ITEM_TRUFFLE:
        *status = 15;
        break;
    case ITEM_BLUE_GRASS:
        *status = 16;
        break;
    case ITEM_RED_GRASS:
        *status = 17;
        break;
    case ITEM_GREEN_GRASS:
        *status = 18;
        break;
    case ITEM_DOG_BALL:
        *status = 21;
        break;
    case ITEM_BASKET:
        *status = 32;
        break;
    default:
        recognized = 0;
        break;
    }
    return recognized;
}

/* Look up a search code's status (its sprite cell). Returns 1 when the code is known. */
static s32 estate_code_status(s32 code, s32 *result, s32 *status) {
    s32 recognized;
    recognized = 1;
    *result = 0;
    *status = 0;
    switch (code) {
    case ITEM_CORN:
        *status = 23;
        break;
    case ITEM_PINK_CAT_FLOWER:
        *status = 1;
        break;
    case ITEM_BLUE_MAGIC_RED_FLOWER:
        *status = 2;
        break;
    case ITEM_WILD_GRAPE:
    case ITEM_AJA_GRAPES:
        *status = 29;
        break;
    case ITEM_FISH_FOOD:
        *status = 7;
        break;
    case ITEM_RUCKSACK_LEVEL_2:
        *status = 8;
        break;
    case ITEM_RUCKSACK_LEVEL_3:
        *status = 9;
        break;
    case 0x177:
        *status = 10;
        break;
    case ITEM_OIL:
        *status = 12;
        break;
    case ITEM_FLOUR:
        *status = 13;
        break;
    case ITEM_CURRY_POWDER:
        *status = 14;
        break;
    case ITEM_SPRING_SEEDS:
        *status = 15;
        break;
    case ITEM_SUMMER_SEEDS:
        *status = 16;
        break;
    case ITEM_SPRING_SEEDS_12A:
        *status = 17;
        break;
    case ITEM_RED_MAGIC_RED_FLOWER:
        *status = 11;
        break;
    case ITEM_BASKET:
        *status = 24;
        break;
    case ITEM_BREAD:
        *status = 25;
        break;
    case ITEM_RICE_BALL:
        *status = 26;
        break;
    case ITEM_BLUE_FEATHER:
        *status = 27;
        break;
    case ITEM_POCKETSTATION:
        *status = 28;
        break;
    case ITEM_DOG_BALL:
        *status = 30;
        break;
    case ITEM_MOON_DROP_FLOWER:
        /* Recognised, with no status. */
        break;
    default:
        recognized = 0;
        break;
    }
    return recognized;
}

/* The seasons' values, five per season: one for each code estate_code_lookup reads. */
static u8 estate_season_codes[20] = {
    0x14, 0x0F, 0x10, 0x16, 0x17, 0x1A, 0x14, 0x15, 0x1B, 0x1C, 0x1A, 0x14, 0x15, 0x1B, 0x1C,
    0x0B, 0x06, 0x07, 0x0C, 0x0D,
};

/* Look up a search code's value for the current season from its row of estate_season_codes.
 * Returns 1 when the code is known and the season is valid. */
static s32 estate_code_lookup(s32 code, s32 *result, s32 *value) {
    u32 season;
    s32 success;
    s32 index;
    u8 *row;

    season = game_state.progress.clock.date.season;
    success = 1;
    *result = 0;
    *value = 0;
    if (season >= 4U) {
        return 0;
    }
    index = season * 5;
    row = estate_season_codes + index;
    switch (code) {
    case ITEM_BEEHIVE:
        *value = row[0];
        break;
    case ITEM_APPLE:
        *value = 0x12;
        break;
    case ITEM_BASKET:
        *value = row[1];
        break;
    case ITEM_DOG_BALL:
        *value = row[2];
        break;
    case ITEM_NORMAL_EGG:
    case ITEM_GOOD_EGG:
    case ITEM_EXCELLENT_EGG:
        *value = row[3];
        break;
    case ITEM_GOLDEN_EGG:
        *value = row[4];
        break;
    default:
        success = 0;
        break;
    }
    return success;
}

/* Look up a search code's out2 value; out1 stays 0. Returns 1 when the code is known. */
static s32 estate_code_value(s32 code, s32 *out1, s32 *out2) {
    s32 recognized;

    recognized = 1;
    *out1 = 0;
    *out2 = 0;
    switch (code) {
    case ITEM_NORMAL_EGG:
    case ITEM_GOOD_EGG:
    case ITEM_EXCELLENT_EGG:
        *out2 = 5;
        break;
    case ITEM_FODDER:
        *out2 = 4;
        break;
    case ITEM_CHICKEN_FEED:
        *out2 = 7;
        break;
    case ITEM_BASKET:
        *out2 = 2;
        break;
    case ITEM_DOG_BALL:
        *out2 = 3;
        break;
    case ITEM_GOLDEN_EGG:
        *out2 = 6;
        break;
    default:
        recognized = 0;
        break;
    }
    return recognized;
}


/* Map an item to its picture: *variant and *index pick the picture. Returns 1
 * for a known item, 0 otherwise. */
s32 item_code_to_picture(s32 code, s32 *variant, s32 *index) {
    s32 found = 1;
    s32 x;
    s32 y;

    *variant = 0;
    *index = 0;
    switch (code) {
    case ITEM_BASKET:
        break;
    case ITEM_DOG_BALL:
        *index = 1;
        break;
    case 0x188:
        *index = 20;
        break;
    case ITEM_VASE:
        x = 0;
        y = 19;
        switch (game_state.progress.estate.pendingRequest) {
        case ITEM_MOON_DROP_FLOWER:
            x = 0;
            break;
        case ITEM_TOY_FLOWER:
            x = 1;
            break;
        case ITEM_PINK_CAT_FLOWER:
            x = 2;
            break;
        case ITEM_BLUE_MAGIC_RED_FLOWER:
            x = 4;
            break;
        case ITEM_RED_MAGIC_RED_FLOWER:
            x = 5;
            break;
        default:
            y = 3;
            break;
        }
        *variant = x;
        *index = y;
        break;
    case ITEM_KNIFE: *index = 4; break;
    case ITEM_FRYING_PAN: *index = 5; break;
    case ITEM_POT: *index = 6; break;
    case ITEM_MIXER: *index = 7; break;
    case ITEM_WHISK: *index = 8; break;
    case ITEM_ROLLING_PIN: *index = 9; break;
    case ITEM_OVEN: *index = 10; break;
    case ITEM_SEASONING_SET: *index = 11; break;
    case ITEM_CARP_PRINT: *index = 12; break;
    case ITEM_CHAR_PRINT: *index = 13; break;
    case ITEM_CATFISH_PRINT: *index = 14; break;
    case ITEM_SQUID_PRINT: *index = 15; break;
    case ITEM_ANGLER_PRINT: *index = 16; break;
    case ITEM_SEA_BREAM_PRINT: *index = 17; break;
    default:
        found = 0;
        break;
    }
    return found;
}

/* Look up a search code's value. Returns 1 when the code is known. */
s32 estate_code_value_4(s32 code, s32 *result, s32 *value) {
    s32 status;

    status = 1;
    *result = 0;
    *value = 0;
    switch (code) {
    case ITEM_BASKET:
        *value = 0x19;
        break;
    case ITEM_DOG_BALL:
        *value = 0x1A;
        break;
    case ITEM_GOLDEN_EGG:
        *value = 0x1E;
        break;
    case ITEM_NORMAL_EGG ... ITEM_EXCELLENT_EGG:
        *value = 0x1D;
        break;
    default:
        status = 0;
        break;
    }
    return status;
}

/* Look up the value of the basket and the dog ball. Returns 1 when the code is known. */
s32 estate_code_value_9(s32 code, s32 *out1, s32 *out2) {
    s32 result = 1;
    s32 value = 100;
    *out1 = 0;
    *out2 = 0;
    switch (code) {
    case ITEM_BASKET:
        value = 17;
        *out2 = value;
        break;
    case ITEM_DOG_BALL:
        value = 19;
        *out2 = value;
        break;
    default:
        result = 0;
        break;
    }
    return result;
}

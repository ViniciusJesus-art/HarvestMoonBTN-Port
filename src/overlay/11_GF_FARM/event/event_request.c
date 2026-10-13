/* Event requests: the queue of events waiting to start, its handlers, and the message
 * timers. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include <memory.h>
#include "types.h"
#include "clock.h"
#include "event_commands.h"
#include "event_flags.h"
#include "event_request.h"
#include "farm_state.h"
#include "field.h"
#include "player.h"
#include "shop_dialogues.h"

/* A free request slot holds this value in `value`. */
#define REQUEST_FREE -1
/* The request queue holds five entries; each has five argument words. */
#define REQUEST_SLOT_COUNT 5
#define REQUEST_ARGS 5
/* A day timer: 0xFF stops it, and a running one stops at TIMER_MAX. */
#define TIMER_STOPPED 0xFF
#define TIMER_MAX 120

static FarmRequestSlot *scene_claim_entry(s32 id, s32 check, s32 any);
static s32 event_request_run_handler(s32 index);
static void event_request_pop(void);
static s32 event_request_drop_stale(s32 unused0);

/* Clear the request queue and the day counters, then set the day-count defaults. */
void scene_item_queue_reset(void)
{
    s32 i;

    memset(&game_state.requests, 0, sizeof(game_state.requests));
    for (i = 0; i < REQUEST_SLOT_COUNT; i++)
    {
        game_state.requests.items[i].value = REQUEST_FREE;
    }
    game_state.request = 0;
    for (i = 0; i < 128; i++)
    {
        game_state.days.count0[i] = TIMER_STOPPED;
    }
    for (i = 0; i < 8; i++)
    {
        game_state.days.count1[i] = TIMER_STOPPED;
    }
    for (i = 0; i < 128; i++)
    {
        game_state.days.values[i] = 0;
    }
    game_state.days.count0[0x01] = 1;
    game_state.days.count0[0x06] = 30;
    game_state.days.count0[0x0C] = 31;
    game_state.days.count0[0x10] = 0;
    game_state.days.count0[0x15] = 28;
    game_state.days.count0[0x16] = 0;
    game_state.days.count0[0x18] = 118;
    game_state.days.count0[0x19] = 28;
    game_state.days.count0[0x2C] = 0;
}

/* Run the request at the head of the queue. An empty head clears the busy flag and
 * the scripted state; a request left over for another place is dropped first. */
void event_request_step(void)
{
    FarmState *farm;
    s16 item;
    s32 result;

    farm = &game_state;
    if ((game_state.debugFlags & DEBUG_FLAG_NO_EVENTS) != 0)
    {
        return;
    }
    item = farm->requests.items[0].value;
    farm->request = &farm->requests.items[0];
    if (item == -1)
    {
        event_flag_set(EVENT_FLAG_REQUEST_BUSY, 0);
        result = shop_dialogue_current();
        if (result == 0)
        {
            if (game_state.player.scriptRunning == 0)
            {
                game_state.player.stateFlags &= ~PLAYER_FLAG_SCRIPTED;
            }
        }
        game_state.statusFlags &= ~STATUS_FLAG_ASLEEP;
        return;
    }
    if (item == 0)
    {
        result = event_flag_get(EVENT_FLAG_REQUEST_BUSY);
        if (result == 0)
        {
            if (event_request_drop_stale(0) != 0)
            {
                return;
            }
        }
    }
    event_request_run_handler(item);
}

static s32 event_request_queued(s32 id)
{
    s32 count = 0;
    s32 sentinel = REQUEST_FREE;
    FarmState *fs = &game_state;
    FarmRequestSlot *p = fs->requests.items;
    for (count = 0; count < REQUEST_SLOT_COUNT; count++, p++)
    {
        if (p->value != sentinel)
        {
            if (p->value == id)
                return 1;
        }
    }
    return 0;
}

/* Claim a request slot for `event` without checking whether it is already queued. */
static void event_request_claim_unchecked(s32 event)
{
    scene_claim_entry(event, 0, 0);
}

/* Claim a request slot for `event` unless it is already queued (see scene_claim_entry). */
struct FarmRequestSlot *event_request_claim(s32 event)
{
    scene_claim_entry(event, 1, 0);
}

/* As event_request_claim, but the claim may use any of the five slots. */
struct FarmRequestSlot *event_request_claim_any(s32 event)
{
    scene_claim_entry(event, 0, 1);
}

/* Advance one timer: 0xFF means stopped, and a running timer stops at 120. */
static inline void tick_timer(u8 *timer)
{
    s32 value = *timer;

    if (value != TIMER_STOPPED)
    {
        value++;
        *timer = value;
        if ((u8)value > TIMER_MAX)
        {
            value = TIMER_MAX;
        }
        *timer = value;
    }
}

/* Advance the day timers (0xFF = stopped, capped at 120). The first bank ticks every
 * call; the second bank waits while a dated event is today (clock_date_index is not 0),
 * while a wedding offer is pending, and while the weather is 3 or 4. */
void scene_message_timers_tick(void)
{
    s32 i;

    for (i = 0; i < 128; i++)
    {
        tick_timer(&game_state.days.count0[i]);
    }
    if (clock_date_index(game_state.progress.clock.date.season, game_state.progress.clock.date.day) != 0)
    {
        return;
    }
    for (i = 0; i < 5; i++)
    {
        if (wedding_offer_state(i) == WEDDING_OFFER_YES_VARIANT)
        {
            return;
        }
    }
    if (wedding_offer_state(-1) == WEDDING_OFFER_YES_VARIANT)
    {
        return;
    }
    if ((u16)game_state.world.weather - 3 < 2U)
    {
        return;
    }
    for (i = 0; i < 8; i++)
    {
        tick_timer(&game_state.days.count1[i]);
    }
}

/* Claim a free request slot (value -1): the first one, or any of the five when `any`
 * is 1. With `check` 1, nothing is claimed if the id is already queued. The claimed
 * slot is zeroed and returned; 0 when none is free. */
static FarmRequestSlot *scene_claim_entry(s32 id, s32 check, s32 any)
{
    s32 count;
    s32 i;
    s32 j;

    if (check == 1 && event_request_queued(id) == 1)
    {
        return 0;
    }
    count = 1;
    if (any == 1)
    {
        count = REQUEST_SLOT_COUNT;
    }
    for (i = 0; i < count; i++)
    {
        FarmState *farm = &game_state;
        s16 *args;

        if (farm->requests.items[i].value == REQUEST_FREE)
        {
            farm->requests.items[i].value = 0;
            args = farm->requests.items[i].args;
            for (j = 0; j < REQUEST_ARGS; j++)
            {
                args[j] = 0;
            }
            return &farm->requests.items[i];
        }
    }
    return 0;
}

/* Run the check of queue entry `index`: when it passes, start the request and pop it;
 * otherwise mark the request busy and return 0. */
static s32 event_request_run_handler(s32 index)
{
    s32 result = event_request_checks[index]();
    if (result != 1)
    {
        event_flag_set(EVENT_FLAG_REQUEST_BUSY, 1);
        return 0;
    }
    event_flag_set(EVENT_FLAG_REQUEST_BUSY, 0);
    game_state.requests.state = FARM_REQUEST_START;
    event_request_pop();
    return 1;
}

/* Shift the queue down one slot and free the last one. */
static void event_request_pop(void)
{
    FarmState *state = &game_state;
    FarmRequestSlot *p = state->requests.items;
    s32 i = 3;
    do
    {
        p[0] = p[1];
        i--;
        p++;
    } while (i >= 0);
    game_state.requests.items[4].value = REQUEST_FREE;
}

static s32 event_request_drop_stale(s32 unused0)
{
    FarmState *farm = &game_state;

    /* args[1] of the running request is the place it is for. */
    if (farm->request->args[1] == farm_area.mode.place.location)
    {
        return 0;
    }
    event_flag_set(EVENT_FLAG_REQUEST_BUSY, 0);
    game_state.requests.state = FARM_REQUEST_START;
    event_request_pop();
    return 1;
}

s32 event_queue_has_entries(void)
{
    s32 i = 1;
    s32 sentinel = REQUEST_FREE;
    FarmState *state = &game_state;

    /* Request 0 is the one running, so the queue is checked from request 1. */
    do
    {
        if (state->requests.items[i].value == sentinel)
        {
            i++;
        }
        else
        {
            return 1;
        }
    } while (i < REQUEST_SLOT_COUNT);

    return 0;
}

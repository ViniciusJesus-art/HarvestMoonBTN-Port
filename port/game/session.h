#ifndef HMBTN_SESSION_H
#define HMBTN_SESSION_H
#include "game_state.h"
#include "weather_effects.h"
#include "area_doors.h"
#include "farm_field.h"
#include "village.h"
#include "poultry.h"
#include "farm_visit.h"
#include "village_talk.h"
#include "event_item.h"
#include "house_control.h"
#include "services.h"
#include "farm_carry.h"
#include "farm_feed.h"
#include "farm_gather.h"
#include "farm_menu.h"
#include "feed_shop.h"
#include "construction.h"
#include "fishing.h"
#include "field_effects.h"
#include "house_sleep.h"

/* MA-04a ownership extraction from scene_preview.c, preserving current rules.
 * Reference: src/main/game/farm_state.h: FarmState; farm_init.c/farm_main.c in
 * src/overlay/11_GF_FARM (revision 4402de2). MA-04b owns event execution/actors;
 * Free-world phases are MA-06; game/runtime.c coordinates scripts since PF-04.
 * No SDL, file I/O or renderer ownership.
 * Nonmovable: consumers borrow members of this heap-owned object. Never copy
 * or serialize the struct. Create/destroy through the lifecycle below. */
typedef struct HmbtnSession {
    HmbtnServices services;
    struct HmbtnSessionFrame *active_frame; /* Scoped loan only during the public runtime tick. */
    struct { unsigned loaded_message,loaded_cell,house_music_ticks,ticks,audio_song; } runtime;
    uint64_t daily_date;
    bool morning_prepared,morning_save_requested,load_requested;
    HmbtnHouseControl control;
    HmbtnHouseSleep sleep;
    HmbtnDaylight daylight;
    HmbtnWeatherEffects weather_effects;
    HmbtnAreaDoors area_doors; /* Transient poses; original route cursors persist separately. */
    HmbtnFarmAction farm_action;
    HmbtnFishing fishing;
    HmbtnFarmCarry carry;
    HmbtnFarmFeed feed;
    HmbtnFarmGather gather;
    HmbtnFeedShop feed_shop;
    HmbtnFarmMenu menu;
    HmbtnFieldEffect field_effects[HMBTN_FIELD_EFFECTS];
    unsigned area,overnight_tick,morning_fade,travel_tick,door_tick,door_model;
    int door_speed,door_angle,door_twin,refused_zone;
    uint16_t door_base_angle,door_twin_base;
    HmbtnHouseExit travel_exit;
    unsigned quick_ticks,quick_id;
    bool quick_tool,completion_reported;
    HmbtnGameState game;
    HmbtnFarmField field;
    HmbtnVillage village;
    HmbtnPoultry poultry;
    HmbtnConstruction construction;
    bool village_initialized,village_ready;
    HmbtnFarmIntro intro;
    uint8_t event_code[3296]; /* Validated bytecode, never a loan from event art. */
    HmbtnFarmVisit visit;
    HmbtnVillageTalk talk;
    HmbtnHouseInteraction house; /* Includes counters borrowed by talk. */
    HmbtnNewGame dialogue,animal_name;
    HmbtnEventItem event_item;
    uint8_t trigger_data[4096];
    HmbtnFarmTriggers triggers; /* Borrows this session's trigger_data. */
    unsigned animal_event,previous_place,first_exit_tick;
    bool trigger_entry,animal_name_active,first_visit_started,first_visit_active;
    HmbtnHouseExit first_exit;
} HmbtnSession;

/* Ordered day updates: normal ticks, sleep and event fast-forward share the
 * same observer. Date key is native, never an on-disc pointer/layout. */
uint64_t hmbtn_session_date(const HmbtnFarmClock *clock);
void hmbtn_session_clock_events(void *session,const HmbtnFarmClock *clock,unsigned changes);
unsigned hmbtn_session_advance_minutes(HmbtnSession *session,unsigned minutes);
bool hmbtn_session_morning(HmbtnSession *session,bool slept);
/* Host acknowledges the prepared morning after resources are installed. */
void hmbtn_session_morning_ready(HmbtnSession *session);

/* The field initializer copies layout values; it does not retain the array.
 * NULL/0 retains its existing soil-only initialization for synthetic fixtures. */
HmbtnSession *hmbtn_session_create(const uint16_t *layout,size_t count);
void hmbtn_session_destroy(HmbtnSession *session);
/* Host owns schedule bytes. They must remain valid until rebind/unbind or
 * destruction. First binding initializes the village at the same handoff as
 * before; later bindings preserve records, flags, schedules and RNG. */
bool hmbtn_session_bind_schedules(HmbtnSession *session,const uint8_t *data,size_t size);
/* Detach host loans (schedule bytes, ground, game-command adapter). Owned
 * cursors, bytecode, actors and their village aliases survive. Pause host
 * coordination until the required ground/callback/resources are rebound. */
void hmbtn_session_unbind_resources(HmbtnSession *session);
/* Preserves existing opening/visit algorithms. Invalid input leaves execution
 * intact. Copies only bytecode; actor animation/texture data stays in the host. */
bool hmbtn_session_open_event(HmbtnSession *session,unsigned event,
    const uint8_t *code,size_t size,char *error,size_t error_size);
bool hmbtn_session_event_tick(HmbtnSession *session,char *error,size_t error_size);
bool hmbtn_session_talk_tick(HmbtnSession *session,char *error,size_t error_size);
typedef enum HmbtnSessionActorSource {
    HMBTN_SESSION_ACTOR_PRIMARY, /* Existing singleton: Thomas (17) only. */
    HMBTN_SESSION_ACTOR_INTRO,   /* Named NPC slots, never childhood IDs. */
    HMBTN_SESSION_ACTOR_VISIT    /* Temporary current-area resident slot. */
} HmbtnSessionActorSource;
/* A handoff, not synchronization: copy_route applies only on first binding.
 * Rejects conflicting bindings. Routes are dormant while an override exists. */
HmbtnIntroActor *hmbtn_session_bind_event_actor(HmbtnSession *session,unsigned id,
    HmbtnSessionActorSource source,bool copy_route);
void hmbtn_session_release_event_actors(HmbtnSession *session);
/* Existing visitor cleanup, including flags and route re-placement. Host
 * runtime restores controls/audio in the same preserved order. */
void hmbtn_session_finish_visit(HmbtnSession *session);
#endif

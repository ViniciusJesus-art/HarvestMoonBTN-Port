#ifndef HMBTN_SESSION_H
#define HMBTN_SESSION_H
#include "game_state.h"
#include "farm_field.h"
#include "village.h"
#include "poultry.h"

/* MA-04a ownership extraction from scene_preview.c, preserving current rules.
 * Reference: src/main/game/farm_state.h: FarmState; farm_init.c/farm_main.c in
 * src/overlay/11_GF_FARM (revision 4402de2). Event execution/actors are MA-04b;
 * host tick coordination is MA-05..08. No SDL, file I/O or renderer ownership.
 * Nonmovable: consumers borrow members of this heap-owned object. Never copy
 * or serialize the struct. Create/destroy through the lifecycle below. */
typedef struct HmbtnSession {
    HmbtnGameState game;
    HmbtnFarmField field;
    HmbtnVillage village;
    HmbtnPoultry poultry;
    bool village_initialized,village_ready;
} HmbtnSession;

/* The field initializer copies layout values; it does not retain the array.
 * NULL/0 retains its existing soil-only initialization for synthetic fixtures. */
HmbtnSession *hmbtn_session_create(const uint16_t *layout,size_t count);
void hmbtn_session_destroy(HmbtnSession *session);
/* Host owns schedule bytes. They must remain valid until rebind/unbind or
 * destruction. First binding initializes the village at the same handoff as
 * before; later bindings preserve records, flags, schedules and RNG. */
bool hmbtn_session_bind_schedules(HmbtnSession *session,const uint8_t *data,size_t size);
/* Detach host loans before their storage disappears. Domain records survive;
 * player/event actor loans must be explicitly rebound by the next adapter.
 * Event execution itself remains in the scene until MA-04b. */
void hmbtn_session_unbind_resources(HmbtnSession *session);
#endif

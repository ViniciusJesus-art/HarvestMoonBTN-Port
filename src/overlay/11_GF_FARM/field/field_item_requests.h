#ifndef FIELD_ITEM_REQUESTS_H
#define FIELD_ITEM_REQUESTS_H

/* Item requests on the fields. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "anim.h"

/* One farm request slot. */
typedef struct FarmItemRequest {
    s16 kind;               /* 1, 2 or 3 */
    s16 state;
    s32 pos[3];             /* the request's position */
    u8  pad10[0x14 - 0x10];
    AnimState *record;   /* one of FarmState.work.sprites; NULL while free */
} FarmItemRequest;

s32 calendar_day_tier(void);
void item_requests_tick_all(void);
void item_requests_clear_all(void);
s32 item_request_submit(s16 id, VECTOR pos);

#endif /* FIELD_ITEM_REQUESTS_H */

#ifndef HMBTN_SERVICES_H
#define HMBTN_SERVICES_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Logical pad masks from pad_buttons.h; no SDL events or host time here. */
typedef struct HmbtnLogicalInput { uint32_t pressed,held; } HmbtnLogicalInput;
typedef enum HmbtnServiceChannel {
    HMBTN_SERVICE_RESOURCE,HMBTN_SERVICE_MESSAGE,HMBTN_SERVICE_ANIMATION,
    HMBTN_SERVICE_ACTION,HMBTN_SERVICE_AUDIO,HMBTN_SERVICE_CHANNELS
} HmbtnServiceChannel;
typedef enum HmbtnServiceKind {
    HMBTN_SERVICE_EVENT_DATA,HMBTN_SERVICE_TRIGGER_DATA,HMBTN_SERVICE_ACTION_DATA,
    HMBTN_SERVICE_TEXT_DATA,HMBTN_SERVICE_SHOW_MESSAGE,HMBTN_SERVICE_WAIT_ANIMATION,
    HMBTN_SERVICE_WAIT_ACTION,HMBTN_SERVICE_LOAD_SONG,HMBTN_SERVICE_START_SONG,
    HMBTN_SERVICE_START_XA,HMBTN_SERVICE_WAIT_XA
} HmbtnServiceKind;
/* IDs are original logical IDs, never archive offsets or serialized pointers.
 * bank: text bank; actor: script actor ID for animation; cursor: issuing PC
 * (distinguishes successive identical message/animation commands). */
typedef struct HmbtnServiceRequest {
    HmbtnServiceKind kind;
    uint32_t id,bank,actor,cursor;
} HmbtnServiceRequest;
typedef enum HmbtnServiceStatus {
    HMBTN_SERVICE_IDLE,HMBTN_SERVICE_PENDING,HMBTN_SERVICE_DONE,
    HMBTN_SERVICE_FAILED,HMBTN_SERVICE_CANCELLED
} HmbtnServiceStatus;
enum { HMBTN_SERVICE_MAX_BYTES=1024*1024 };
typedef struct HmbtnServiceResult {
    uint64_t ticket; /* Scoped to this nonmovable session; 0 is never valid. */
    HmbtnServiceRequest request;
    HmbtnServiceStatus status;
    bool claimed;
    uint32_t value; /* Message choice, or action ANI byte count. */
    uint8_t *data;size_t bytes; /* Owned by services until ack/cancel/destroy. */
    char error[192];
} HmbtnServiceResult;
typedef struct HmbtnServices {
    uint64_t sequence;
    HmbtnServiceResult channels[HMBTN_SERVICE_CHANNELS];
} HmbtnServices;
/* Zero initialization is sufficient. Never copy/move an active services object. */
uint64_t hmbtn_service_begin(HmbtnServices *s,HmbtnServiceChannel channel,HmbtnServiceRequest request);
const HmbtnServiceResult *hmbtn_service_poll(const HmbtnServices *s,HmbtnServiceChannel channel);
/* Claims a pending request once; delayed providers retain ticket, not pointers.
 * Repeated polling never submits the effect a second time. */
const HmbtnServiceResult *hmbtn_service_claim(HmbtnServices *s,HmbtnServiceChannel channel);
bool hmbtn_service_complete(HmbtnServices *s,uint64_t ticket,uint32_t value);
/* Copies provider bytes. Provider retains/frees its source, including rejected
 * or late responses. Failed/oversized copies never publish partial data. */
bool hmbtn_service_complete_data(HmbtnServices *s,uint64_t ticket,const uint8_t *data,size_t bytes,uint32_t value);
bool hmbtn_service_fail(HmbtnServices *s,uint64_t ticket,const char *error);
bool hmbtn_service_cancel(HmbtnServices *s,uint64_t ticket);
/* Terminal responses are immutable and readable until this explicit ack. */
bool hmbtn_service_ack(HmbtnServices *s,uint64_t ticket);
/* Invalidate outstanding host tickets and republish pending intents on rebind.
 * Owned terminal results survive; an old provider cannot complete the new job. */
void hmbtn_services_rebind(HmbtnServices *s);
void hmbtn_services_clear(HmbtnServices *s);
#endif

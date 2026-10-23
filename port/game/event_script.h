#ifndef HMBTN_EVENT_SCRIPT_H
#define HMBTN_EVENT_SCRIPT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct HmbtnEventBlock {
    const uint8_t *data;
    size_t size, payload;
    unsigned count, script;
    uint16_t flags;
} HmbtnEventBlock;
typedef struct HmbtnEventScript {
    const uint8_t *data;
    unsigned count, pc;
    bool finished, boundary;
} HmbtnEventScript;
typedef struct HmbtnEventCommand {
    uint8_t type, next_done, next_other, flags;
    int16_t command, operand;
    int32_t x, y;
} HmbtnEventCommand;
typedef enum HmbtnEventResult {
    HMBTN_EVENT_ERROR = -1, HMBTN_EVENT_WAIT, HMBTN_EVENT_DONE, HMBTN_EVENT_BOUNDARY,
    HMBTN_EVENT_PENDING, /* Host resource request: preserve the cursor until fulfilled. */
    HMBTN_EVENT_JUMP /* Handler supplied a validated runtime return cursor. */
} HmbtnEventResult;
typedef HmbtnEventResult (*HmbtnEventHandler)(void *context, const HmbtnEventCommand *command,
    char *error, size_t error_size);

bool hmbtn_event_block_open(HmbtnEventBlock *block, const uint8_t *data, size_t size,
    char *error, size_t error_size);
bool hmbtn_event_block_entry(const HmbtnEventBlock *block, unsigned index,
    const uint8_t **data, size_t *size, uint16_t *flags);
bool hmbtn_event_script_open(HmbtnEventScript *script, const uint8_t *data, size_t size,
    char *error, size_t error_size);
/* The game's own event bytecode dispatches native C handlers, never MIPS. */
bool hmbtn_event_tick(HmbtnEventScript *script, HmbtnEventHandler handler, void *context,
    char *error, size_t error_size);
#endif

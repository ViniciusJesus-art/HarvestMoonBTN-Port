#ifndef HMBTN_EVENT_VARIABLES_H
#define HMBTN_EVENT_VARIABLES_H
#include "event_script.h"
struct HmbtnVillage;
struct HmbtnFarmClock;
struct HmbtnRegistration;
struct HmbtnIntroActor;
struct HmbtnGameProgress;
/* Shared event_cmd_dispatch story mutations. Unknown commands are boundaries. */
HmbtnEventResult hmbtn_event_progress_command(struct HmbtnGameProgress *progress,
    const HmbtnEventCommand *command,char *error,size_t error_size);
/* Borrow authoritative state and this execution's local slots; owns no values. */
typedef struct HmbtnEventVariables {
    struct HmbtnVillage *village;
    const struct HmbtnFarmClock *clock;
    const struct HmbtnRegistration *registration;
    const struct HmbtnIntroActor *player;
    unsigned target;
    int32_t *slots;
    size_t slot_count;
} HmbtnEventVariables;
int32_t hmbtn_event_arithmetic(int32_t previous,int32_t operand,int operation);
/* Original result: one when the comparison rule FAILS. */
int hmbtn_event_comparison(int32_t value,int32_t operand,int operation);
bool hmbtn_event_variable_get(const HmbtnEventVariables *context,int variable,int element,int32_t *value);
bool hmbtn_event_variable_update(HmbtnEventVariables *context,int variable,int element,int32_t operand,int operation);
/* Original command types 13..22. Unknown state is a boundary, never success;
 * malformed slot operands are errors. No resource/graphics side effects. */
HmbtnEventResult hmbtn_event_variables_command(HmbtnEventVariables *context,
    const HmbtnEventCommand *command,char *error,size_t error_size);
#endif

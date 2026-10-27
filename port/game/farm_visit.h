#ifndef HMBTN_FARM_VISIT_H
#define HMBTN_FARM_VISIT_H
#include "village.h"
#include "house_interaction.h"
#include "farm_field.h"
enum { HMBTN_FARM_TRIGGER_BYTES=36,HMBTN_FARM_TRIGGER_LIMIT=100 };
/* Borrowed, bounded BIN table. Only events with complete native handlers are eligible. */
typedef struct HmbtnFarmTriggers {
    const uint8_t *data;unsigned count;
    const HmbtnFarmField *field; /* Read-only session loan for cultivation conditions. */
} HmbtnFarmTriggers;
typedef struct HmbtnFarmVisit {
    unsigned event;bool active,free,talk;unsigned talk_target,start_area;
    HmbtnIntroActor actors[HMBTN_VILLAGERS]; /* Current-area event copies, never route records. */
    struct { unsigned item;int32_t position[3];bool visible; } prop; /* Staged gift art, not inventory. */
} HmbtnFarmVisit;
bool hmbtn_farm_triggers_open(HmbtnFarmTriggers *table,const uint8_t *data,size_t bytes,char *error,size_t size);
unsigned hmbtn_farm_visit_trigger(const HmbtnFarmTriggers *table,const HmbtnVillage *v,
    const HmbtnFarmClock *clock,unsigned weather,unsigned previous_place,unsigned mode);
unsigned hmbtn_farm_event_trigger(const HmbtnFarmTriggers *table,const HmbtnVillage *v,
    const HmbtnFarmClock *clock,unsigned weather,unsigned previous_place,unsigned mode,
    unsigned target,const HmbtnInventory *inventory);
bool hmbtn_farm_visit_supported(unsigned event);
bool hmbtn_farm_visit_begin(HmbtnFarmVisit *visit,const HmbtnFarmTriggers *table,unsigned event,HmbtnVillage *v);
/* Non-visual state commands. Visual commands continue through farm_intro. */
HmbtnEventResult hmbtn_farm_visit_command(HmbtnFarmVisit *visit,HmbtnFarmIntro *intro,
    HmbtnVillage *v,const HmbtnFarmClock *clock,const HmbtnEventCommand *command,char *error,size_t size);
#endif

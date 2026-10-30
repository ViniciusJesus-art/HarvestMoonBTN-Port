#ifndef HMBTN_POND_H
#define HMBTN_POND_H
#include <stdbool.h>
#include <stdint.h>
/* field/pond.h: content defaults, independent of the PS1 struct layout. */
enum { HMBTN_POND_FISH=99,HMBTN_POND_MEDIUM_DAYS=10,HMBTN_POND_LARGE_DAYS=20,HMBTN_POND_SPAWN_DAYS=30 };
typedef struct HmbtnPondFish { uint16_t item,days; } HmbtnPondFish;
typedef struct HmbtnPond {
    HmbtnPondFish fish[HMBTN_POND_FISH];
    bool fed;
    uint32_t random; /* Native persisted stream; never advanced by rendering. */
} HmbtnPond;
unsigned hmbtn_pond_count(const HmbtnPond *);
bool hmbtn_pond_add(HmbtnPond *,unsigned item);
unsigned hmbtn_pond_take(HmbtnPond *);
void hmbtn_pond_new_day(HmbtnPond *);
bool hmbtn_pond_valid(const HmbtnPond *);
#endif

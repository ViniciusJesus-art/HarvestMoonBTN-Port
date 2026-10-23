#ifndef HMBTN_FIELD_EFFECTS_H
#define HMBTN_FIELD_EFFECTS_H
#include "asset_decode.h"
enum { HMBTN_FIELD_EFFECTS=25 };
typedef struct HmbtnFieldEffect { unsigned art,tick;int32_t position[3];bool active; } HmbtnFieldEffect;
bool hmbtn_field_effect_submit(HmbtnFieldEffect slots[HMBTN_FIELD_EFFECTS],unsigned item,int column,int row);
void hmbtn_field_effect_tick(HmbtnFieldEffect slots[HMBTN_FIELD_EFFECTS],const HmbtnAnimation art[7]);
#endif

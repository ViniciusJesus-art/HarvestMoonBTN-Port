#ifndef HMBTN_WEATHER_EFFECTS_H
#define HMBTN_WEATHER_EFFECTS_H
#include "weather.h"
/* Original visual density, not a gameplay or host hardware limit. Cosmetic
 * stream only: deliberately not serialized and never consumes weather RNG. */
enum { HMBTN_WEATHER_PARTICLES=40 };
typedef struct HmbtnWeatherParticle { int x,y,delay;unsigned age,sprite,frame,flags; } HmbtnWeatherParticle;
typedef struct HmbtnWeatherEffects {
    unsigned mode;uint32_t random;
    HmbtnWeatherParticle particles[HMBTN_WEATHER_PARTICLES];
} HmbtnWeatherEffects;
void hmbtn_weather_effects_tick(HmbtnWeatherEffects *effects,unsigned weather,bool held);
/* weather.c: particle_step adds the camera's projected scroll, only after a
 * particle becomes visible. Cosmetic host tick; never called from draw. */
void hmbtn_weather_effects_scroll(HmbtnWeatherEffects *effects,int dx,int dy);
#endif

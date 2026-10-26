/* world/weather.c: weather_fx_set_mode, particle_respawn/step (d3c0f38).
 * Original icon animation 25=splashes, 26=snow; motion/delay tuning retained.
 * World renderer draws this tick-owned view; camera shake/thunder remain pending. */
#include "weather_effects.h"
#include "psx_random.h"
#include <string.h>
static unsigned random_range(HmbtnWeatherEffects *e,unsigned range)
{ return hmbtn_psx_random_range(&e->random,range); }
static void respawn(HmbtnWeatherEffects *e,HmbtnWeatherParticle *p)
{
    const bool moving=(p->flags&0x4000)!=0,fast=(p->flags&0x1000)!=0;
    const unsigned base=moving ? (fast ? 74 : 1690) : 25;
    p->delay=(int)random_range(e,base)+(moving ? (fast ? 12 : 20) : 15);
    random_range(e,base);p->x=(int)random_range(e,moving && fast ? 300 : 240)+(moving ? (fast ? 30 : 15) : 20);
    random_range(e,base);p->y=(int)random_range(e,moving ? (fast ? 180 : 60) : 160)+(moving ? (fast ? 10 : 5) : 20);
    p->age=p->frame=0;
}
void hmbtn_weather_effects_tick(HmbtnWeatherEffects *e,unsigned weather,bool held)
{
    if(weather>=HMBTN_WEATHER_KINDS) return;
    if(e->mode!=weather) {
        memset(e,0,sizeof(*e));e->mode=weather;e->random=1;
        for(unsigned i=0;i<HMBTN_WEATHER_PARTICLES;++i) {
            HmbtnWeatherParticle *p=&e->particles[i];
            if(!weather) continue;
            p->sprite=(weather==HMBTN_WEATHER_SNOW || weather==HMBTN_WEATHER_BLIZZARD) ? 26 : 25;
            if(weather==HMBTN_WEATHER_SNOW) {
                p->flags=0x4000;if(!(i&3)) p->flags|=0x2000;if(!(i&7)) p->sprite=0;
            } else if(weather==HMBTN_WEATHER_BLIZZARD) p->flags=0x5000|(!(i&1) ? 0x2000 : 0);
            respawn(e,p);
        }
    }
    if(held || !weather) return;
    bool seen_idle=false;
    for(unsigned i=0;i<HMBTN_WEATHER_PARTICLES;++i) {
        HmbtnWeatherParticle *p=&e->particles[i];
        if(p->sprite) {
            if(p->delay>=0) --p->delay;
            else if(p->flags&0x4000) {
                const bool fast=(p->flags&0x1000)!=0;
                p->y+=(fast ? 5 : 1)*(p->flags&0x2000 ? 2 : 1);
                p->x-=(fast ? 7 : 0);
                p->x+=(p->age&7)!=7 ? (p->age&8 ? 1 : -1) : 0;
                if(p->age++ >= (p->flags&0x2000 ? 55u : 110u)) respawn(e,p);
            } else {
                p->frame=p->age++/2;
                if(p->frame>=3) respawn(e,p);
            }
        }
        if(p->delay==0) {
            if(!seen_idle) seen_idle=true;
            else if(p->flags&0x4000) p->delay+=3;
        }
    }
}
void hmbtn_weather_effects_scroll(HmbtnWeatherEffects *e,int dx,int dy)
{
    for(unsigned i=0;i<HMBTN_WEATHER_PARTICLES;++i) {
        HmbtnWeatherParticle *p=&e->particles[i];
        if(p->sprite && p->delay<0) { p->x+=dx;p->y+=dy; }
    }
}

/* Source src/overlay/11_GF_FARM/world/clock_events.c: scene_overnight_step,
 * and world/clock.c: clock_advance_minute; reference d3c0f38. Existing native
 * callbacks/order preserved, with field growth before production/feed cleanup.
 * Each 06:00 date boundary applies once, including fast-forward across days.
 * Scene loads, music, fades and bed morph presentation remain host operations.
 * Weather follows field growth/feed cleanup; family/helpers/absent species remain pending. */
#include "session.h"
#include <string.h>
uint64_t hmbtn_session_date(const HmbtnFarmClock *clock)
{
    return ((uint64_t)clock->year*4+clock->season)*30+clock->day;
}
void hmbtn_session_clock_events(void *context,const HmbtnFarmClock *clock,unsigned changes)
{
    HmbtnSession *s=context;
    if(!s || !clock) return;
    if((changes&HMBTN_CLOCK_DAY) && s->daily_date==hmbtn_session_date(clock))
        changes&=~(unsigned)(HMBTN_CLOCK_DAY|HMBTN_CLOCK_SEASON|HMBTN_CLOCK_YEAR);
    hmbtn_farm_shipping_clock(hmbtn_game_economy(&s->game),clock,changes);
    if(s->village_ready && s->game.economy.shipping.fish_shipped>=200) s->village.flags[0x5F]=true;
    if(s->village_ready) hmbtn_village_clock(&s->village,clock,changes,s->game.weather.today,false);
    if(changes&HMBTN_CLOCK_HOUR) hmbtn_poultry_hour(&s->poultry,clock,s->game.weather.today);
    if((changes&HMBTN_CLOCK_DAY) && s->daily_date!=hmbtn_session_date(clock)) {
        s->daily_date=hmbtn_session_date(clock);
        hmbtn_farm_field_new_day(&s->field,clock->season);
        hmbtn_poultry_new_day(&s->poultry,&s->feed,clock);
        hmbtn_poultry_carry_sync(&s->poultry,&s->house);
        hmbtn_farm_feed_new_day(&s->feed);hmbtn_farm_gather_new_day(&s->gather);
        hmbtn_weather_new_day(&s->game.weather,clock,&s->game.registration,NULL);
        hmbtn_village_refresh_weather(&s->village);
        if(s->game.weather.today==HMBTN_WEATHER_RAIN) hmbtn_farm_field_rain(&s->field);
        hmbtn_daylight_weather_select(&s->daylight,clock,s->game.weather.today);
        s->house.tv_dirty=true;
    }
}

unsigned hmbtn_session_advance_minutes(HmbtnSession *s,unsigned minutes)
{
    return s ? hmbtn_farm_clock_advance_notify(&s->game.clock,minutes,hmbtn_session_clock_events,s) : 0;
}
/* Morning is a domain handoff, independent of loading the house. Repeating a
 * pending host load must not advance the date, consume feed or heal twice. */
bool hmbtn_session_morning(HmbtnSession *s,bool slept)
{
    if(!s || s->morning_prepared) return false;
    const unsigned bedtime=s->game.clock.hour*60+s->game.clock.minute;
    if(slept) {
        hmbtn_session_advance_minutes(s,hmbtn_farm_clock_sleep_minutes(&s->game.clock,s->farm_action.fatigue));
        const unsigned hours=(1440+s->game.clock.hour*60+s->game.clock.minute-bedtime)/60;
        const unsigned gain=hours*7+50,loss=hours*2+5;
        s->farm_action.stamina=s->farm_action.stamina+gain>s->farm_action.max_stamina ? s->farm_action.max_stamina : s->farm_action.stamina+gain;
        s->farm_action.fatigue=s->farm_action.fatigue>loss ? s->farm_action.fatigue-loss : 0;
    } else {
        s->farm_action.fatigue+=10;
        if(s->farm_action.fatigue>99) s->farm_action.fatigue=99;
    }
    s->morning_prepared=true;s->morning_save_requested=s->house.save_requested;
    memset(&s->sleep,0,sizeof(s->sleep));s->house.mode=HMBTN_HOUSE_FREE;s->house.save_requested=false;
    memset(&s->house.message,0,sizeof(s->house.message));s->house.request=s->house.loaded=0;
    memset(&s->dialogue.message,0,sizeof(s->dialogue.message));
    s->intro.action_active=false;s->intro.player.pose=false;s->intro.player.visible=true;s->intro.player.facing=0;
    hmbtn_fishing_cancel(&s->fishing);s->intro.player.sprite=(HmbtnAnimationPlayer){0};
    s->intro.player.position[0]=-90;s->intro.player.position[1]=0;s->intro.player.position[2]=541;
    memset(s->intro.player.velocity,0,sizeof(s->intro.player.velocity));
    s->control=(HmbtnHouseControl){0};s->control.enabled=true;
    s->farm_action.phase=HMBTN_FARM_ACTION_FREE;s->farm_action.bank=-1;
    const unsigned discarded=s->carry.discarded;
    memset(&s->carry,0,sizeof(s->carry));s->carry.discarded=discarded;
    memset(s->field_effects,0,sizeof(s->field_effects));
    s->travel_tick=s->door_tick=s->overnight_tick=0;s->morning_fade=45;
    s->intro.area=80;s->intro.camera_actor=0;s->intro.camera_gliding=false;
    memcpy(s->intro.camera,s->intro.player.position,sizeof(s->intro.camera));s->house.tv_dirty=true;
    return true;
}

void hmbtn_session_morning_ready(HmbtnSession *s)
{
    if(s) s->morning_prepared=false;
}

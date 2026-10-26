#include "save_fixture.h"
#include "event_variables.h"
#include "world_map.h"
#include "psx_data.h"
#include <stdio.h>
static void rules(void)
{
    HmbtnGameState g;hmbtn_game_state_init(&g);
    HmbtnFarmClock c={.day=3,.hour=6};HmbtnWeather w={.random=1};
    assert(hmbtn_weather_forecast(&w,&c,&g.registration,NULL)==HMBTN_WEATHER_RAIN && w.random==0x41c67ea6);
    w.random=0;assert(hmbtn_weather_forecast(&w,&c,&g.registration,NULL)==HMBTN_WEATHER_SUNNY && w.random==12345);
    c.season=1;w.random=30;assert(hmbtn_weather_forecast(&w,&c,&g.registration,NULL)==HMBTN_WEATHER_STORM && w.random==0xb53d60ff);
    w.random=9;assert(hmbtn_weather_forecast(&w,&c,&g.registration,NULL)==HMBTN_WEATHER_SUNNY);
    c.season=2;w.random=3;assert(hmbtn_weather_forecast(&w,&c,&g.registration,NULL)==HMBTN_WEATHER_RAIN);
    c.season=3;w.random=3;assert(hmbtn_weather_forecast(&w,&c,&g.registration,NULL)==HMBTN_WEATHER_BLIZZARD);
    w.random=9;assert(hmbtn_weather_forecast(&w,&c,&g.registration,NULL)==HMBTN_WEATHER_SNOW);
    w.random=0;assert(!hmbtn_weather_forecast(&w,&c,&g.registration,NULL));
    /* All calendar exclusions consume no RNG; forecast and application both enforce them. */
    unsigned dated=0;
    for(unsigned season=0;season<4;++season) for(unsigned day=0;day<30;++day)
        if(hmbtn_farm_clock_date_index(season,day)) {
            ++dated;c.season=day ? season : (season+3)%4;c.day=day ? day-1 : 29;
            w=(HmbtnWeather){.random=3};assert(!hmbtn_weather_forecast(&w,&c,&g.registration,NULL) && w.random==3);
            c.season=season;c.day=day;w.tomorrow=HMBTN_WEATHER_BLIZZARD;
            hmbtn_weather_new_day(&w,&c,&g.registration,NULL);assert(!w.today);
        }
    assert(dated==14);
    /* Storm before Summer 7 and blizzard before Winter 10 are downgraded. */
    c.season=1;c.day=4;w.random=30;assert(hmbtn_weather_forecast(&w,&c,&g.registration,NULL)==HMBTN_WEATHER_RAIN);
    c.day=5;w.tomorrow=3;hmbtn_weather_new_day(&w,&c,&g.registration,NULL);assert(w.today==1);
    c.season=3;c.day=7;w.random=3;assert(hmbtn_weather_forecast(&w,&c,&g.registration,NULL)==HMBTN_WEATHER_SNOW);
    c.day=8;w.tomorrow=4;hmbtn_weather_new_day(&w,&c,&g.registration,NULL);assert(w.today==2);
    c.season=2;c.day=11;w.random=3;assert(!hmbtn_weather_forecast(&w,&c,&g.registration,NULL) && w.random==3);
    /* Elli's birthday moves from Spring 16 to 20 if it matches the player. */
    unsigned season,day;assert(hmbtn_bride_birthday(&g.registration,9,&season,&day) && season==0 && day==15);
    c.season=0;c.day=14;w.random=1;assert(!hmbtn_weather_forecast(&w,&c,&g.registration,NULL) && w.random==1);
    g.registration.birthday_day=15;
    assert(hmbtn_bride_birthday(&g.registration,9,&season,&day) && day==19);
    assert(hmbtn_weather_forecast(&w,&c,&g.registration,NULL)==1);
    c.day=18;w.random=1;assert(!hmbtn_weather_forecast(&w,&c,&g.registration,NULL) && w.random==1);
    const unsigned ids[5]={3,7,9,14,20};
    for(unsigned i=0;i<5;++i) {
        assert(hmbtn_bride_birthday(&g.registration,ids[i],&season,&day));
        c.season=day ? season : (season+3)%4;c.day=day ? day-1 : 29;
        w.random=3;assert(!hmbtn_weather_forecast(&w,&c,&g.registration,NULL) && w.random==3);
        c.season=season;c.day=day;w.tomorrow=1;
        hmbtn_weather_new_day(&w,&c,&g.registration,NULL);assert(!w.today);
    }
    c.season=0;
    const HmbtnWeatherWedding wedding={.season=0,.day=4,.today=true};
    c.day=3;w.random=1;assert(!hmbtn_weather_forecast(&w,&c,&g.registration,&wedding) && w.random==1);
    w.tomorrow=1;hmbtn_weather_new_day(&w,&c,&g.registration,&wedding);assert(!w.today);
    /* Forecast crosses season and year using the upcoming season's rule. */
    c.season=2;c.day=29;w.random=3;assert(hmbtn_weather_forecast(&w,&c,&g.registration,NULL)==4);
    c.season=3;c.day=29;w.random=1;assert(!hmbtn_weather_forecast(&w,&c,&g.registration,NULL) && w.random==1);
}
static void daily(void)
{
    HmbtnSession *s=save_fixture(1);s->game.clock=(HmbtnFarmClock){.day=3,.hour=6};
    s->daily_date=hmbtn_session_date(&s->game.clock);s->field.season=0;
    s->field.cells[460]=(HmbtnFarmTile){.value=0x23,.ground=0x16a};
    s->field.cells[461]=(HmbtnFarmTile){.value=0x168,.ground=0x168};
    s->field.cells[462]=(HmbtnFarmTile){.value=0x36,.ground=0x16a,.kind=1};
    s->field.cells[463]=(HmbtnFarmTile){.value=0x168,.ground=0x168};s->field.active[463]=false;
    s->game.weather=(HmbtnWeather){.tomorrow=1,.random=9};
    hmbtn_session_advance_minutes(s,1440);
    assert(s->game.weather.today==1 && s->field.cells[460].ground==0x16b && !s->field.cells[460].watered_days);
    for(unsigned i=0;i<5;++i) assert(s->village.flags[18+i]==(i==1));
    assert(s->field.cells[461].ground==0x169 && s->field.cells[462].ground==0x16a && s->field.cells[463].ground==0x168);
    HmbtnWeather before=s->game.weather;HmbtnFarmTile tile=s->field.cells[460];
    hmbtn_session_clock_events(s,&s->game.clock,HMBTN_CLOCK_DAY);
    assert(!memcmp(&before,&s->game.weather,sizeof(before)) && !memcmp(&tile,&s->field.cells[460],sizeof(tile)));
    s->game.weather.tomorrow=0;hmbtn_session_advance_minutes(s,1440);
    assert(!s->game.weather.today && s->field.cells[460].watered_days==1 && s->field.cells[460].ground==0x16a);
    s->field.season=3;hmbtn_farm_field_rain(&s->field);
    assert(s->field.cells[460].ground==0x16a && !hmbtn_farm_field_water(&s->field,460));
    s->field.season=0;
    /* Long skips and daily notifications produce exactly the same forecast/RNG. */
    HmbtnSession *a=save_fixture(2),*b=save_fixture(2);
    hmbtn_session_advance_minutes(a,1440*95);
    for(unsigned i=0;i<95;++i) hmbtn_session_advance_minutes(b,1440);
    assert(!memcmp(&a->game.weather,&b->game.weather,sizeof(a->game.weather)) && a->daily_date==b->daily_date);
    hmbtn_session_destroy(a);hmbtn_session_destroy(b);
    /* Snow/storm/blizzard do not run the RAIN irrigation callback. */
    for(unsigned kind=0;kind<5;++kind) {
        a=save_fixture(1);a->game.clock=(HmbtnFarmClock){.season=kind==4 ? 3u : 1u,.day=3,.hour=6};
        a->daily_date=hmbtn_session_date(&a->game.clock);
        a->field.cells[460]=(HmbtnFarmTile){.value=0x168,.ground=0x168};a->game.weather.tomorrow=kind;
        hmbtn_session_advance_minutes(a,1440);
        assert(a->game.weather.today==kind && a->field.cells[460].ground==(kind==1 ? 0x169 : 0x168));
        hmbtn_session_destroy(a);
    }
    /* Every original weather kind is a live script/TV view; no hardcoded sun. */
    HmbtnEventVariables v={.village=&s->village};int32_t value;
    for(unsigned kind=0;kind<5;++kind) {
        s->game.weather.today=kind;s->game.weather.tomorrow=kind;unsigned message,group;
        hmbtn_village_refresh_weather(&s->village);
        /* Source event_flags.h: sunny begins at 18; exactly one derived flag. */
        for(unsigned i=0;i<5;++i) assert(s->village.flags[18+i]==(i==kind));
        assert(hmbtn_event_variable_get(&v,19,0,&value) && value==(int32_t)kind);
        hmbtn_house_tv_program(&s->house,0,&message,&group);
        assert(group==(kind>=3 ? 17u : kind==1 ? 5u : kind==2 ? 6u : 4u));
    }
    /* Cosmetic ticks pause and never consume the persistent stream. */
    before=s->game.weather;
    for(unsigned kind=0;kind<5;++kind) {
        for(unsigned i=0;i<2200;++i) hmbtn_weather_effects_tick(&s->weather_effects,kind,false);
        const HmbtnWeatherEffects fx=s->weather_effects;
        hmbtn_weather_effects_tick(&s->weather_effects,kind,true);
        assert(!memcmp(&fx,&s->weather_effects,sizeof(fx)));
        for(unsigned i=0;i<HMBTN_WEATHER_PARTICLES;++i) assert(s->weather_effects.particles[i].frame<3);
    }
    assert(!memcmp(&before,&s->game.weather,sizeof(before)));hmbtn_session_destroy(s);
}
static void scroll(void)
{
    HmbtnWeatherEffects e={0};
    e.particles[0]=(HmbtnWeatherParticle){.sprite=25,.delay=-1,.x=100,.y=100};
    e.particles[1]=(HmbtnWeatherParticle){.sprite=25,.delay=0,.x=100,.y=100};
    e.particles[2]=(HmbtnWeatherParticle){.sprite=26,.delay=-1,.x=80,.y=90};
    hmbtn_weather_effects_scroll(&e,-7,3);
    assert(e.particles[0].x==93 && e.particles[0].y==103);
    assert(e.particles[1].x==100 && e.particles[1].y==100);
    assert(e.particles[2].x==73 && e.particles[2].y==93);
    const HmbtnWeatherEffects before=e;hmbtn_weather_effects_scroll(&e,0,0);
    assert(!memcmp(&before,&e,sizeof(e)));
}
static void audio_selection(void)
{
    uint8_t section[16]={0};hmbtn_write_le16(section+14,999);
    for(unsigned season=0;season<4;++season) for(unsigned hour=0;hour<24;++hour) {
        assert(hmbtn_world_song_weather(section,16,season,hour,1)==22);
        assert(hmbtn_world_song_weather(section,16,season,hour,3)==23);
        assert(hmbtn_world_song_weather(section,16,season,hour,4)==23);
        assert(hmbtn_world_song_weather(section,16,season,hour,2)==season+1);
        assert(hmbtn_world_song_weather(section,16,season,hour,0)==hmbtn_world_song(section,16,season,hour));
    }
    hmbtn_write_le16(section+8,0x1000);
    assert(hmbtn_world_music_volume_weather(section,16,0,22,1)==40);
    hmbtn_write_le16(section+8,0);
    assert(hmbtn_world_music_volume_weather(section,16,0,22,1)==100);
    assert(hmbtn_world_music_volume_weather(section,16,0,22,0)==50);
    assert(!hmbtn_world_music_volume_weather(section,16,3,22,1));
    for(unsigned i=0;i<2;++i) {
        const unsigned exception=i ? 29 : 14;hmbtn_write_le16(section+14,(uint16_t)exception);
        assert(hmbtn_world_song_weather(section,16,0,10,1)==exception);
    }
    hmbtn_write_le16(section+14,24);
    assert(hmbtn_world_song_weather(section,16,0,10,1)==22);
    hmbtn_write_le16(section+2,15);assert(!hmbtn_world_music_volume_weather(section,16,0,22,1));
    hmbtn_write_le16(section+2,40);assert(hmbtn_world_music_volume_weather(section,16,3,22,1)==100);
    assert(!hmbtn_world_song_weather(NULL,16,0,10,1));
}
int main(void)
{ rules();daily();scroll();audio_selection();puts("Weather: original seasonal vectors, calendar/birthdays, rain order, winter, long skips, TV/scripts, projected scroll and cosmetic isolation passed.");return 0; }

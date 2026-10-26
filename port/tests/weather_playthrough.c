/* Real installed resources/input, domain fixtures only for the two field cells.
 * Forecast uses the new-game persistent seed and the original Spring rule. */
#include "poultry_helpers.h"
#include "field_helpers.h"
int main(int argc,char **argv)
{
    if(argc<3 || argc>4) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};
    const char *shots=argc==4 ? argv[3] : NULL;
    CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock clock={.day=2,.weekday=2,.hour=10};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&clock,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    CHECK(!g->game.weather.today && g->game.weather.tomorrow==1);
    g->field.cells[460]=(HmbtnFarmTile){.value=0x167,.ground=0x167};
    g->field.cells[461]=(HmbtnFarmTile){.value=0x167,.ground=0x167};
    cell(s,10,10);CHECK(tool(s,6,error,sizeof(error)));
    CHECK(g->field.cells[460].ground==0x168 && hmbtn_farm_field_plant(&g->field,10,10,0x23));
    CHECK(travel(s,80,error,sizeof(error)));
    /* Select the weather channel through its normal input/request/resource path. */
    g->house.mode=HMBTN_HOUSE_TV;CHECK(step(s,HMBTN_PAD_UP,error,sizeof(error)));
    CHECK(g->house.tv_group==5 && g->house.request==0x26d);
    const HmbtnWeather forecast=g->game.weather;
    for(unsigned i=0;i<90;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(g->house.loaded==0x26d && !memcmp(&forecast,&g->game.weather,sizeof(forecast)));
    CHECK(capture(s,&host,shots,"forecast",error,sizeof(error)));
    for(unsigned i=0;i<800 && g->house.mode!=HMBTN_HOUSE_FREE;++i) CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(g->house.mode==HMBTN_HOUSE_FREE && travel(s,0,error,sizeof(error)));
    CHECK(sleep_day(s,error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(g->game.weather.today==1 && g->runtime.audio_song==22
        && g->field.cells[460].ground==0x16b && !g->field.cells[460].watered_days);
    /* Hoeing during rain waters the new soil immediately, as field_request_dispatch. */
    cell(s,11,10);CHECK(tool(s,6,error,sizeof(error)));
    CHECK(g->field.cells[461].value==0x169 && g->field.cells[461].ground==0x169);
    for(unsigned i=0;i<120;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(capture(s,&host,shots,"rain",error,sizeof(error)));
    HmbtnWeather before=g->game.weather;HmbtnWeatherEffects fx=g->weather_effects;
    for(unsigned i=0;i<3;++i) CHECK(capture(s,&host,NULL,"unchanged",error,sizeof(error)));
    CHECK(!memcmp(&before,&g->game.weather,sizeof(before)) && !memcmp(&fx,&g->weather_effects,sizeof(fx)));
    /* A single visible splash is an opaque original palette texel. The world
     * light must tint it too (main_draw_ots), rather than draw it as a UI icon. */
    memset(g->weather_effects.particles,0,sizeof(g->weather_effects.particles));
    g->weather_effects.particles[0]=(HmbtnWeatherParticle){.sprite=25,.delay=-1,.x=80,.y=100};
    uint8_t icon[FRAME_BYTES]={0};
    CHECK(hmbtn_ui_icon(hmbtn_scene_preview_resources(s).ui,icon,25,0,80,100,error,sizeof(error)));
    hmbtn_daylight_apply(&g->daylight,icon,sizeof(icon));
    size_t pixel=0;while(pixel<FRAME_BYTES && !icon[pixel+3]) pixel+=4;CHECK(pixel<FRAME_BYTES);
    CHECK(capture(s,&host,shots,"rain-palette",error,sizeof(error)));
    SDL_Surface *surface=SDL_RenderReadPixels(host.renderer,NULL);CHECK(surface);
    const int ix=(int)(pixel/4%320),iy=(int)(pixel/4/320);
    uint8_t r,green,b,alpha;
    const bool read=SDL_ReadSurfacePixel(surface,ix*surface->w/320+surface->w/640,
        iy*surface->h/240+surface->h/480,&r,&green,&b,&alpha);
    SDL_DestroySurface(surface);
    CHECK(read && r==icon[pixel] && green==icon[pixel+1] && b==icon[pixel+2] && b>r);
    /* One movement tick: splash motion equals the scene's projected camera
     * scroll, not a fixed HUD position or the character's raw world velocity. */
    CHECK(step(s,HMBTN_PAD_RIGHT,error,sizeof(error))); /* Previous idle mode has zero initial velocity. */
    const int32_t *centre=g->intro.camera;const int32_t distance=g->intro.camera_distance;
    const int32_t dz=distance<0 ? -((-distance*2896+4095)/4096) : distance*2896/4096;
    const HmbtnCamera previous={{centre[0]-distance*2896/4096,centre[1]+g->intro.camera_height,centre[2]+dz},
        {centre[0],centre[1],centre[2]},1000};
    int ox,oy,nx,ny;CHECK(hmbtn_model_project_point(&previous,previous.target,&ox,&oy));
    CHECK(step(s,HMBTN_PAD_RIGHT,error,sizeof(error)));
    CHECK(hmbtn_model_project_point(&previous,g->intro.camera,&nx,&ny));
    CHECK((ox!=nx || oy!=ny) && g->weather_effects.particles[0].x==80+ox-nx
        && g->weather_effects.particles[0].y==100+oy-ny && g->game.weather.random==before.random);
    CHECK(capture(s,&host,shots,"rain-moving",error,sizeof(error)));
    CHECK(field_reload(&s,&host,argv[1],argv[2],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(!memcmp(&before,&g->game.weather,sizeof(before)) && g->runtime.audio_song==22
        && g->house.weather==&g->game.weather && g->village.weather==&g->game.weather);
    /* Visiting interiors and returning does not reroll or water/grow twice. */
    CHECK(travel(s,80,error,sizeof(error)) && travel(s,0,error,sizeof(error)));
    CHECK(!memcmp(&before,&g->game.weather,sizeof(before)) && !g->field.cells[460].watered_days);
    g->game.weather.tomorrow=0;
    HmbtnWeather expected=g->game.weather;HmbtnFarmClock next=g->game.clock;
    hmbtn_farm_clock_advance(&next,hmbtn_farm_clock_sleep_minutes(&next,0));
    hmbtn_weather_new_day(&expected,&next,&g->game.registration,NULL);
    CHECK(sleep_day(s,error,sizeof(error)));
    CHECK(!memcmp(&expected,&g->game.weather,sizeof(expected)));
    CHECK(!g->game.weather.today && g->field.cells[460].watered_days==1 && g->field.cells[460].ground==0x16a);
    CHECK(capture(s,&host,shots,"after-rain",error,sizeof(error)));
    /* The five kinds draw real original particles with no gameplay RNG use. */
    for(unsigned kind=0;kind<5;++kind) {
        CHECK(hmbtn_scene_preview_set_weather(s,kind,0));
        const uint32_t random=g->game.weather.random;
        for(unsigned i=0;i<2200;++i) CHECK(step(s,0,error,sizeof(error)));
        CHECK(g->game.weather.random==random);
        char name[32];snprintf(name,sizeof(name),"weather-%u",kind);
        CHECK(capture(s,&host,shots,name,error,sizeof(error)));
    }
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);
    puts("Weather playthrough: TV forecast, sleep, rain/tilling, save/load, travel, next-day growth, all five presentations and pure draw passed.");return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

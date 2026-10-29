/* Real room zones, model animation, event bytecode and original item pictures. */
#include "poultry_helpers.h"
#include "field_helpers.h"
#include "shop_ui_helpers.h"
static bool schedules(HmbtnScenePreview *s,char *error,size_t size)
{
    const HmbtnVillage *live=&hmbtn_scene_preview_session(s)->village;
    /* cue_history_item_3/_7: single Karen/Ann, ordinary Wednesday at 06:00.
     * Compare all original LE schedule cells, rather than just a rain flag. */
    for(unsigned weather=0;weather<5;++weather) {
        HmbtnVillage v;HmbtnWeather w={.today=weather};
        hmbtn_village_init(&v,live->schedule_data,live->schedule_size);v.weather=&w;
        const HmbtnFarmClock c={.day=3,.weekday=3,.hour=6};
        REQUIRE(hmbtn_village_sync(&v,&c,error,size));
        const unsigned ids[2]={3,7},dry[2]={0x780,0x23a0},wet[2]={0x870,0x2490};
        for(unsigned n=0;n<2;++n) {
            const unsigned offset=(weather==1 || weather==2) ? wet[n] : dry[n];
            REQUIRE(offset+240<=live->schedule_size);
            for(unsigned i=0;i<30;++i) for(unsigned a=0;a<4;++a)
                REQUIRE(v.people[ids[n]].schedule[i][a]==hmbtn_read_le16(live->schedule_data+offset+i*8+a*2));
        }
    }
    return true;
}
static bool rooms(HmbtnScenePreview *s,HmbtnHost *host,const char *shots,char *error,size_t size)
{
    const unsigned areas[]={156,192,196,204,208};unsigned doors=0;
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned id=1;id<HMBTN_VILLAGERS;++id) {
        g->village.people[id].friendship=255;g->village.people[id].affection=65535;
    }
    for(unsigned a=0;a<sizeof(areas)/sizeof(*areas);++a) {
        g->game.clock.hour=14; /* The original winery room is open 13..16. */
        REQUIRE(hmbtn_scene_preview_load_area(s,areas[a],error,size) && step(s,0,error,size));
        const uint8_t *section=hmbtn_scene_preview_resources(s).section;
        for(unsigned i=0;i<50;++i) {
            const uint8_t *z=section+16+i*44;
            if(hmbtn_read_le16(z)!=0x1000 || hmbtn_read_le16(z+2)!=0xfffe) continue;
            const unsigned model=hmbtn_read_le16(z+12);
            const uint16_t base=hmbtn_scene_preview_resources(s).scene->angle[model][1];
            at(s,z);g->game.inventory.held_item=(HmbtnStoredTool){0x44,7,1};
            int32_t start[3];memcpy(start,g->intro.player.position,sizeof(start));
            const unsigned face=g->intro.player.facing,axis=face&1u ? 0u : 2u;
            REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
            REQUIRE(g->house.mode==HMBTN_HOUSE_FREE && g->door_tick && !g->travel_tick && !g->carry.phase);
            for(unsigned t=0;t<16 && g->area_doors.models[model].phase!=HMBTN_DOOR_HELD;++t) REQUIRE(step(s,0,error,size));
            REQUIRE(g->area==areas[a] && g->area_doors.models[model].phase==HMBTN_DOOR_HELD
                && hmbtn_scene_preview_resources(s).scene->angle[model][1]!=base);
            REQUIRE(g->game.inventory.held_item.id==0x44 && g->game.inventory.held_item.modifier==7);
            if(areas[a]==204 || areas[a]==208) REQUIRE(capture(s,host,shots,"inn-room-open",error,size));
            for(unsigned t=0;t<40 && g->door_tick;++t) REQUIRE(step(s,HMBTN_PAD_START|HMBTN_PAD_RIGHT,error,size));
            REQUIRE(!g->door_tick && !g->travel_tick && !g->control.exit_pending && g->menu.mode==HMBTN_MENU_NONE);
            const int32_t walked=(g->intro.player.position[axis]-start[axis])*(face<2 ? -1 : 1);
            REQUIRE(g->area==areas[a] && g->intro.player.facing==face
                && walked>0 && walked<=37*41
                && g->intro.player.position[2-axis]==start[2-axis]);
            HmbtnGroundHit floor;
            REQUIRE(hmbtn_ground_find(&hmbtn_scene_preview_resources(s).scene->terrain,g->intro.player.position,0xc080,200,&floor)
                && floor.height==g->intro.player.position[1]);
            REQUIRE(face<2 ? g->intro.player.position[axis]<half(z+(axis==0 ? 14 : 18))
                : g->intro.player.position[axis]>half(z+(axis==0 ? 20 : 24)));
            if(areas[a]==204 || areas[a]==208) REQUIRE(capture(s,host,shots,"inn-room-crossed",error,size));
            for(unsigned t=0;t<40;++t) REQUIRE(step(s,0,error,size));
            REQUIRE(g->area_doors.models[model].phase==HMBTN_DOOR_CLOSED
                && hmbtn_scene_preview_resources(s).scene->angle[model][1]==base);
            ++doors;
        }
    }
    REQUIRE(doors==18);g->game.inventory.held_item=(HmbtnStoredTool){0};
    /* Internal clinic door still respects the original friendship refusal. */
    REQUIRE(hmbtn_scene_preview_load_area(s,156,error,size) && step(s,0,error,size));
    g->village.people[4].friendship=50;at(s,hmbtn_scene_preview_resources(s).section+16+2*44);
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->house.mode==HMBTN_HOUSE_BOOK
        && g->house.request==0x1d2 && !g->door_tick && !g->travel_tick);
    for(unsigned t=0;t<1200 && g->house.mode!=HMBTN_HOUSE_FREE;++t) REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
    REQUIRE(g->house.mode==HMBTN_HOUSE_FREE);return true;
}
static bool discard(HmbtnScenePreview *s,HmbtnHost *host,const char *shots,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const unsigned index=10*HMBTN_FIELD_COLS;
    REQUIRE(g->area==0 && g->field.active[index]);
    for(unsigned i=0;i<3;++i) g->field.cells[index+i]=(HmbtnFarmTile){.value=0x167,.ground=0x167};
    g->field.cells[index]=(HmbtnFarmTile){.value=0x3a,.ground=0x167}; /* items.h: ITEM_STONE */
    REQUIRE(hmbtn_farm_field_position(&g->field,0,10,g->intro.player.position));
    g->intro.player.position[0]=-4432;g->intro.player.facing=3;g->control.mode=0;
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->game.inventory.held_item.id==0x3a && g->field.cells[index].value==0x167);
    REQUIRE(idle(s,error,size));const unsigned discarded=g->carry.discarded;
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && idle(s,error,size));
    REQUIRE(g->carry.thrown[0].item.id==0x3a && capture(s,host,shots,"stone-throw-flight",error,size));
    for(unsigned t=0;t<100 && g->carry.thrown[0].item.id;++t) REQUIRE(step(s,0,error,size));
    REQUIRE(!g->carry.thrown[0].item.id && g->carry.discarded==discarded);
    unsigned placed=HMBTN_FIELD_CELLS;
    for(unsigned i=0;i<HMBTN_FIELD_CELLS;++i) if(g->field.cells[i].value==0x3a && i!=index) {
        /* Original layout contains other stones; the returned one is on row 10
         * immediately east of the boundary used for this controlled throw. */
        if(i/HMBTN_FIELD_COLS==10 && i%HMBTN_FIELD_COLS<3) { placed=i;break; }
    }
    REQUIRE(placed<HMBTN_FIELD_CELLS && capture(s,host,shots,"stone-returned-field",error,size));
    uint8_t *saved=NULL;size_t saved_bytes=0;
    REQUIRE(hmbtn_save_encode(g,&saved,&saved_bytes,error,size));
    HmbtnSession *loaded=hmbtn_save_decode(saved,saved_bytes,error,size);free(saved);
    REQUIRE(loaded);const bool restored=loaded->field.cells[placed].value==0x3a
        && loaded->carry.discarded==discarded;hmbtn_session_destroy(loaded);REQUIRE(restored);
    REQUIRE(hmbtn_farm_field_position(&g->field,placed%HMBTN_FIELD_COLS,10,g->intro.player.position));
    g->intro.player.position[0]-=224;g->intro.player.facing=3;g->control.mode=0;
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->game.inventory.held_item.id==0x3a && idle(s,error,size));
    /* The same stone breaks outside the cultivated rectangle. */
    g->intro.player.position[0]=-6000;g->intro.player.position[2]=-5000;g->intro.player.facing=1;g->control.mode=0;
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && idle(s,error,size));
    for(unsigned t=0;t<100 && !g->carry.thrown[0].phase;++t) REQUIRE(step(s,0,error,size));
    REQUIRE(g->carry.thrown[0].phase==1 && g->carry.discarded==discarded+1);
    REQUIRE(step(s,0,error,size) && g->carry.thrown[0].phase==2);
    const HmbtnFarmCarry before=g->carry;
    REQUIRE(capture(s,host,shots,"stone-throw-break",error,size) && !memcmp(&before,&g->carry,sizeof(before)));
    REQUIRE(!hmbtn_save_available(g,error,size));error[0]=0;
    for(unsigned t=0;t<4;++t) REQUIRE(step(s,0,error,size));
    REQUIRE(!g->carry.thrown[0].item.id && g->carry.discarded==discarded+1 && !g->game.inventory.held_item.id);
    REQUIRE(capture(s,host,shots,"stone-throw-gone",error,size));return true;
}
static bool rain(HmbtnScenePreview *s,HmbtnHost *host,const char *shots,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    g->game.clock.hour=10;
    REQUIRE(hmbtn_scene_preview_load_area(s,0,error,size) && hmbtn_scene_preview_set_weather(s,1,0));
    REQUIRE(step(s,0,error,size));
    /* Real exterior door: the clock stops, particles keep advancing. */
    const uint8_t *section=hmbtn_scene_preview_resources(s).section,*zone=NULL;
    for(unsigned i=0;i<50;++i) if(hmbtn_read_le16(section+16+i*44)==0x9000
            && hmbtn_read_le16(section+18+i*44)==80) { zone=section+16+i*44;break; }
    REQUIRE(zone);at(s,zone);
    g->weather_effects.particles[39]=(HmbtnWeatherParticle){.sprite=25,.delay=1000};
    const HmbtnFarmClock clock=g->game.clock;
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->door_tick);
    for(unsigned t=0;t<10;++t) REQUIRE(step(s,0,error,size));
    REQUIRE(g->weather_effects.particles[39].delay==989 && !memcmp(&clock,&g->game.clock,sizeof(clock)));
    REQUIRE(capture(s,host,shots,"rain-door",error,size));
    for(unsigned t=0;t<120 && (g->door_tick || g->travel_tick);++t) REQUIRE(step(s,0,error,size));
    REQUIRE(g->area==80 && travel(s,0,error,size));
    /* An actual character event with dialogue, not a fabricated held clock. */
    HmbtnSessionFrame frame=hmbtn_scene_preview_frame(s);
    REQUIRE(hmbtn_session_start_visit(g,&frame,128,error,size));
    bool dialogue=false;
    for(unsigned t=0;t<1200 && !dialogue;++t) {
        REQUIRE(step(s,0,error,size));dialogue=g->intro.message!=0;
    }
    REQUIRE(dialogue && g->visit.active && !g->control.enabled);
    g->weather_effects.particles[39]=(HmbtnWeatherParticle){.sprite=25,.delay=1000};
    for(unsigned t=0;t<10;++t) REQUIRE(step(s,0,error,size));
    REQUIRE(g->weather_effects.particles[39].delay==990);
    /* Pixel proof that an event draws the same weather palette and light. */
    memset(g->weather_effects.particles,0,sizeof(g->weather_effects.particles));
    g->weather_effects.particles[0]=(HmbtnWeatherParticle){.sprite=25,.delay=-1,.x=80,.y=40};
    uint8_t icon[FRAME_BYTES]={0};
    REQUIRE(hmbtn_ui_icon(hmbtn_scene_preview_resources(s).ui,icon,25,0,80,40,error,size));
    hmbtn_daylight_apply(&g->daylight,icon,sizeof(icon));
    const HmbtnWeatherEffects before=g->weather_effects;
    REQUIRE(capture(s,host,shots,"rain-character-event",error,size));
    REQUIRE(!memcmp(&before,&g->weather_effects,sizeof(before)));
    size_t pixel=0;while(pixel<FRAME_BYTES && !icon[pixel+3]) pixel+=4;REQUIRE(pixel<FRAME_BYTES);
    SDL_Surface *surface=SDL_RenderReadPixels(host->renderer,NULL);REQUIRE(surface);
    uint8_t r,b,green,alpha;const int x=(int)(pixel/4%320),y=(int)(pixel/4/320);
    const bool read=SDL_ReadSurfacePixel(surface,x*surface->w/320+surface->w/640,
        y*surface->h/240+surface->h/480,&r,&green,&b,&alpha);SDL_DestroySurface(surface);
    REQUIRE(read && r==icon[pixel] && green==icon[pixel+1] && b==icon[pixel+2]);
    return true;
}
static bool market(HmbtnScenePreview *s,HmbtnHost *host,const char *shots,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->game.clock.hour=12;
    REQUIRE(hmbtn_scene_preview_load_area(s,180,error,size) && step(s,0,error,size));
    const HmbtnGatherRecord *table=hmbtn_farm_gather_record(112);
    memcpy(g->intro.player.position,table->position,sizeof(table->position));
    g->intro.player.position[1]=0;g->intro.player.position[2]-=400;g->intro.player.facing=2;g->control.mode=0;
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && shop_wait(s,HMBTN_SHOP_SELECT,error,size));
    REQUIRE(g->feed_shop.seeds && g->feed_shop.request==0x46);
    REQUIRE(shop_cursor_pixels(s,20,20,error,size) && capture(s,host,shots,"market-cursor-first",error,size));
    REQUIRE(step(s,HMBTN_PAD_DOWN,error,size) && g->feed_shop.selection==1);
    REQUIRE(shop_cursor_pixels(s,20,34,error,size) && capture(s,host,shots,"market-cursor-second",error,size));
    REQUIRE(step(s,HMBTN_PAD_CIRCLE,error,size) && shop_wait(s,HMBTN_SHOP_OFF,error,size));return true;
}
int main(int argc,char **argv)
{
    if(argc<2 || argc>3) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};
    const char *shots=argc==3 ? argv[2] : NULL;
    CHECK(hmbtn_host_open(&host));const HmbtnFarmClock clock={.day=3,.weekday=3,.hour=10};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&clock,error,sizeof(error));CHECK(s);
    for(unsigned t=0;t<110;++t) CHECK(step(s,0,error,sizeof(error)));
    CHECK(schedules(s,error,sizeof(error)));
    CHECK(discard(s,&host,shots,error,sizeof(error)));
    CHECK(rooms(s,&host,shots,error,sizeof(error)));
    CHECK(market(s,&host,shots,error,sizeof(error)));
    CHECK(rain(s,&host,shots,error,sizeof(error)));
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);
    puts("World fidelity: 18 internal door crossings, refusal/carry/input lock, field return/outside discard, rain and original schedules/pixel passed.");return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

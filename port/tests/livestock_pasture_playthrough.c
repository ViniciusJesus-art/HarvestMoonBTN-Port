/* Installed-resource walk: Barley bell + named herd, actual pushed crossings,
 * shared outdoor art, clock-driven grass consumption, weather and return. */
#include "poultry_helpers.h"
#include "field_helpers.h"
#include "shop_ui_helpers.h"
static bool choose(HmbtnScenePreview *s,unsigned selection,char *error,size_t size)
{
    for(unsigned i=0;i<10 && hmbtn_scene_preview_session(s)->feed_shop.selection!=selection;++i) REQUIRE(step(s,HMBTN_PAD_DOWN,error,size));
    REQUIRE(hmbtn_scene_preview_session(s)->feed_shop.selection==selection);return true;
}
static bool buy(HmbtnScenePreview *s,unsigned selection,char *error,size_t size)
{
    REQUIRE(choose(s,selection,error,size) && step(s,HMBTN_PAD_CROSS,error,size));
    REQUIRE(shop_wait(s,HMBTN_SHOP_NAME,error,size) && editor_wait(s,error,size));
    if(selection==5) REQUIRE(step(s,HMBTN_PAD_RIGHT,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && step(s,HMBTN_PAD_START,error,size) && editor_wait(s,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && shop_wait(s,HMBTN_SHOP_SELECT,error,size));return true;
}
static void stage(HmbtnScenePreview *s,unsigned slot,bool outside)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);HmbtnFarmAnimal *a=&g->livestock.animals[slot];
    /* Only starting placement is a fixture. Input and source passage perform
     * all changes of area; no test hook transfers an animal. */
    a->timer=300;a->state=1;a->praise=0;
    a->actor.position[0]=outside ? 1240 : 1534;a->actor.position[1]=0;
    a->actor.position[2]=outside ? 2900 : -2750;a->actor.facing=outside ? 2 : 0;
    a->actor.sprite=(HmbtnAnimationPlayer){0,(uint16_t)a->actor.facing,0};
    memcpy(g->intro.player.position,a->actor.position,sizeof(a->actor.position));
    g->intro.player.position[2]+=outside ? -450 : 450;g->intro.player.facing=a->actor.facing;g->control.mode=0;
}
static bool push_through(HmbtnScenePreview *s,unsigned slot,unsigned target,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const unsigned area=g->area;
    const uint32_t pad=target==0 ? HMBTN_PAD_DOWN : HMBTN_PAD_UP;
    for(unsigned i=0;i<300 && g->livestock.animals[slot].area!=target;++i) REQUIRE(step(s,pad,error,size));
    if(g->livestock.animals[slot].area!=target) {
        const HmbtnFarmAnimal *a=&g->livestock.animals[slot];
        fprintf(stderr,"push stopped slot %u at %d,%d state %u timer %u contact %u player %d,%d door %u\n",slot,
            a->actor.position[0],a->actor.position[2],a->state,a->timer,g->livestock.push_frames,
            g->intro.player.position[0],g->intro.player.position[2],g->area_doors.models[0].phase);
    }
    REQUIRE(g->livestock.animals[slot].area==target && g->area==area && !g->door_tick && !g->travel_tick);
    if(area==0) {
        bool opened=false;const uint8_t *section=hmbtn_scene_preview_resources(s).section;
        for(unsigned i=0;i<50;++i) {
            const uint8_t *z=section+16+i*44;const unsigned model=hmbtn_read_le16(z+12);
            if(hmbtn_read_le16(z+10)==1 && model<HMBTN_SCENE_MODELS)
                opened|=g->area_doors.models[model].phase==HMBTN_DOOR_HELD;
        }
        REQUIRE(opened);
    }
    REQUIRE(step(s,0,error,size));return true;
}
static bool ring(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);REQUIRE(step(s,HMBTN_PAD_SQUARE,error,size));
    REQUIRE(g->farm_action.phase==HMBTN_FARM_ANIMAL_TOOL && g->farm_action.bank==21);
    for(unsigned i=0;i<250 && g->farm_action.phase!=HMBTN_FARM_ACTION_FREE;++i) REQUIRE(step(s,0,error,size));
    REQUIRE(g->farm_action.phase==HMBTN_FARM_ACTION_FREE);return true;
}
int main(int argc,char **argv)
{
    if(argc<3 || argc>4) return 2;const char *shots=argc==4 ? argv[3] : NULL;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock clock={0,0,3,3,12,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],136,&clock,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->game.economy.gold=10500;
    const HmbtnIntroActor *barley=&g->village.people[15].actor;CHECK(barley->visible);
    memcpy(g->intro.player.position,barley->position,sizeof(barley->position));g->intro.player.position[2]+=224;g->intro.player.facing=0;
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(choose(s,10,error,sizeof(error)) && shop_cursor_pixels(s,23,132,error,sizeof(error)) && capture(s,&host,shots,"barley-bell",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_TRIANGLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && g->feed_shop.request==0x281);
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)) && g->game.economy.gold==10000);
    CHECK(buy(s,4,error,sizeof(error)) && buy(s,5,error,sizeof(error)) && !g->game.economy.gold);
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_OFF,error,sizeof(error)));
    unsigned bell=8;for(unsigned i=0;i<8;++i) if(g->game.inventory.bag[i].id==0x1d) { bell=i;break; }CHECK(bell<8 || g->game.inventory.equipped.id==0x1d);
    if(bell<8) { g->game.inventory.equipped=g->game.inventory.bag[bell];g->game.inventory.bag[bell]=(HmbtnStoredTool){0}; }
    CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)) && travel(s,96,error,sizeof(error)));
    g->livestock.animals[0].growth=28;g->livestock.animals[0].affection=g->livestock.animals[1].affection=200;
    stage(s,0,false);CHECK(push_through(s,0,0,error,sizeof(error)));
    stage(s,1,false);
    for(unsigned i=0;i<120;++i) CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)));
    CHECK(g->livestock.animals[1].area==96); /* Occupied arrival queues the second. */
    CHECK(step(s,0,error,sizeof(error)) && travel(s,0,error,sizeof(error)));
    g->intro.player.position[0]=2200;g->intro.player.position[2]=2200;g->control.mode=0;
    CHECK(ring(s,error,sizeof(error)));
    for(unsigned i=0;i<250 && g->livestock.animals[0].state==4;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(g->livestock.animals[0].actor.position[0]>1600);
    CHECK(travel(s,96,error,sizeof(error)));
    for(unsigned i=0;i<120 && g->livestock.animals[1].area!=0;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(g->livestock.animals[1].area==0 && travel(s,0,error,sizeof(error)));
    CHECK(hmbtn_livestock_count(&g->livestock,false)==1 && hmbtn_livestock_count(&g->livestock,true)==1);
    /* A solid field obstacle must remain intact and outside the animal's
     * footprint while the bell sends it toward a goal across that cell. */
    hmbtn_farm_field_clear(&g->field);
    HmbtnFarmAnimal *moving=&g->livestock.animals[0];
    hmbtn_farm_field_position(&g->field,10,8,moving->actor.position);moving->state=1;moving->timer=300;
    hmbtn_farm_field_position(&g->field,16,12,g->livestock.animals[1].actor.position);
    g->field.cells[8*45+11]=(HmbtnFarmTile){.value=0x3c,.ground=0x167};
    hmbtn_farm_field_position(&g->field,15,8,g->intro.player.position);g->control.mode=0;
    CHECK(ring(s,error,sizeof(error)));
    for(unsigned i=0;i<220;++i) {
        CHECK(step(s,0,error,sizeof(error)));
        CHECK(!hmbtn_farm_field_probe(&g->field,moving->actor.position));
    }
    CHECK(g->field.cells[8*45+11].value==0x3c && moving->actor.position[0]>200);
    /* Herd/grass positions and maturity are fixtures; the shared hourly
     * observer and original ANI perform feeding/calling/presentation. */
    hmbtn_farm_field_clear(&g->field);
    for(unsigned i=0;i<2;++i) {
        HmbtnFarmAnimal *a=&g->livestock.animals[i];hmbtn_farm_field_position(&g->field,10+i*3,10,a->actor.position);
        a->state=1;a->timer=300;a->graze_count=0;a->fed_today=false;
        g->field.cells[10*45+10+i*3]=(HmbtnFarmTile){.value=0x36,.ground=0x168,.kind=3};
    }
    memcpy(g->intro.player.position,g->livestock.animals[0].actor.position,sizeof(g->intro.player.position));
    g->intro.player.position[2]+=900;g->intro.player.position[0]+=600;g->control.mode=0;
    memcpy(g->intro.camera,g->intro.player.position,sizeof(g->intro.camera));
    g->game.weather.today=0;g->game.clock.hour=12;g->game.clock.minute=g->game.clock.frames=0;
    CHECK(capture(s,&host,shots,"herd-in-pasture",error,sizeof(error)));
    CHECK(hmbtn_session_advance_minutes(g,240)&HMBTN_CLOCK_HOUR);
    CHECK(g->livestock.animals[0].graze_count==4 && !g->livestock.animals[0].fed_today);
    char pending[1024];CHECK(snprintf(pending,sizeof(pending),"%s.pasture",argv[2])<(int)sizeof(pending));
    CHECK(field_reload(&s,&host,argv[1],pending,error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(hmbtn_session_advance_minutes(g,60)&HMBTN_CLOCK_HOUR);
    CHECK(g->livestock.animals[0].fed_today && g->livestock.animals[1].fed_today
        && g->field.cells[10*45+10].kind==1 && g->field.cells[10*45+13].kind==1 && !g->game.inventory.supplies[0]);
    CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)));
    memcpy(g->intro.camera,g->intro.player.position,sizeof(g->intro.camera));
    const HmbtnLivestock before=g->livestock;CHECK(capture(s,&host,shots,"grazed-pasture",error,sizeof(error)));
    CHECK(!memcmp(&before,&g->livestock,sizeof(before)));
    CHECK(ring(s,error,sizeof(error)));
    CHECK(g->livestock.animals[0].state==4 || g->livestock.animals[1].state==4);
    const unsigned affection=g->livestock.animals[0].affection;g->game.weather.today=1;
    hmbtn_session_advance_minutes(g,60);CHECK(g->livestock.animals[0].affection==affection-5);
    CHECK(capture(s,&host,shots,"herd-rain",error,sizeof(error)));
    /* Return both through the outdoor door and its actual animation. */
    for(unsigned i=0;i<2;++i) {
        stage(s,i,true);CHECK(push_through(s,i,96,error,sizeof(error)));
        if(!i) {
            CHECK(travel(s,96,error,sizeof(error)));
            g->intro.player.position[0]=1534;g->intro.player.position[2]=-900;g->control.mode=0;
            CHECK(ring(s,error,sizeof(error)));
            for(unsigned j=0;j<170 && g->livestock.animals[0].state==4;++j) CHECK(step(s,0,error,sizeof(error)));
            CHECK(travel(s,0,error,sizeof(error)));
        }
    }
    CHECK(travel(s,96,error,sizeof(error)));
    for(unsigned i=0;i<2;++i) CHECK(g->livestock.animals[i].area==96);
    g->game.clock.hour=22;g->game.weather.today=0;
    for(unsigned i=0;i<2;++i) { g->livestock.animals[i].state=1;g->livestock.animals[i].timer=300; }
    const int32_t resting=g->livestock.animals[0].actor.position[2];
    for(unsigned i=0;i<60;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(g->livestock.animals[0].actor.position[2]==resting);
    CHECK(capture(s,&host,shots,"herd-returned-barn",error,sizeof(error)));
    CHECK(travel(s,0,error,sizeof(error)) && travel(s,80,error,sizeof(error)));
    CHECK(field_reload(&s,&host,argv[1],argv[2],error,sizeof(error)));
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);
    puts("Pasture: real Barley bell/herd purchase, delayed push both ways/queued arrival, original outdoor art and bell ANI, hourly grass, rain, night and resumed saves passed.");return 0;
fail:hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

/* Original-resource route. Only date/purse/positions and cleared soil are fixtures. */
#include "poultry_helpers.h"
#include "field_helpers.h"
#include "item_reaction.h"
static bool grow(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->game.economy.gold=1000;
    REQUIRE(hmbtn_scene_preview_load_area(s,180,error,size) && step(s,0,error,size));
    const HmbtnGatherRecord *bundle=hmbtn_farm_gather_record(112);
    memcpy(g->intro.player.position,bundle->position,sizeof(bundle->position));g->intro.player.position[2]-=400;
    g->intro.player.facing=2;g->control.mode=0;
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && shop_wait(s,HMBTN_SHOP_SELECT,error,size));
    for(unsigned i=0;i<3;++i) REQUIRE(step(s,HMBTN_PAD_DOWN,error,size));
    REQUIRE(g->feed_shop.item==36 && step(s,HMBTN_PAD_CROSS,error,size) && shop_wait(s,HMBTN_SHOP_COUNT,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && shop_wait(s,HMBTN_SHOP_SELECT,error,size) && g->game.economy.gold==800);
    REQUIRE(step(s,HMBTN_PAD_CIRCLE,error,size) && shop_wait(s,HMBTN_SHOP_OFF,error,size));
    REQUIRE(hmbtn_scene_preview_load_area(s,0,error,size));g->intro.area=0;
    for(unsigned row=9;row<=11;++row) for(unsigned col=9;col<=11;++col) g->field.cells[row*45+col]=(HmbtnFarmTile){.value=0x167,.ground=0x167};
    for(unsigned row=9;row<=11;++row) for(unsigned col=9;col<=11;++col) { cell(s,col,row);REQUIRE(tool(s,6,error,size)); }
    cell(s,11,10);REQUIRE(tool(s,36,error,size));
    for(unsigned day=0;day<9;++day) {
        REQUIRE(field_refill(s,error,size));
        for(unsigned row=9;row<=11;++row) for(unsigned col=9;col<=11;++col) { cell(s,col,row);REQUIRE(tool(s,21,error,size)); }
        g->game.weather.tomorrow=0;REQUIRE(sleep_day(s,error,size));
    }
    REQUIRE(g->field.cells[460].value==36 && g->field.cells[460].kind==3);return true;
}
static bool collect(HmbtnScenePreview *s,unsigned count,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);REQUIRE(!g->visit.active && !g->game.inventory.held_item.id);
    REQUIRE(hmbtn_scene_preview_load_area(s,0,error,size));g->intro.area=0;
    unsigned picked=0;
    for(unsigned row=9;row<=11 && picked<count;++row) for(unsigned col=9;col<=11 && picked<count;++col) {
        if(hmbtn_farm_field_harvest_item(&g->field,row*45+col)!=67) continue;
        cell(s,col,row);REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && idle(s,error,size) && g->game.inventory.held_item.id==67);++picked;
        if(picked<count) REQUIRE(step(s,HMBTN_PAD_R2,error,size) && idle(s,error,size) && !g->game.inventory.held_item.id);
    }
    REQUIRE(picked==count && hmbtn_scene_preview_load_area(s,68,error,size));g->intro.area=68;return true;
}
static bool launch(HmbtnScenePreview *s,bool target)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const HmbtnModel *m=&hmbtn_scene_preview_resources(s).scene->terrain;
    const uint8_t *section=hmbtn_scene_preview_resources(s).section,*z=NULL;
    for(unsigned i=0;i<50;++i) if(hmbtn_read_le16(section+18+i*44)==44) { z=section+16+i*44;break; }
    if(!z) return false;
    const int cx=(half(z+14)+half(z+20))/2,cz=(half(z+18)+half(z+24))/2,cy=half(z+16);
    for(int x=-2400;x<=2400;x+=100) for(int zz=-2400;zz<=2400;zz+=100) {
        int32_t dry[3]={cx+x,cy,cz+zz};HmbtnGroundHit hit;
        if(!hmbtn_ground_find(m,dry,0xE0C7,1720,&hit) || (hit.flags&0xE0C7u)) continue;dry[1]=hit.height;
        for(unsigned face=0;face<4;++face) {
            HmbtnFarmCarry trial={.area=g->area};trial.thrown[0]=(HmbtnThrownItem){.item=g->game.inventory.held_item,
                .position={dry[0],dry[1]-300,dry[2]},.ground=dry[1],.facing=face,.area=g->area};
            for(unsigned tick=0;tick<100 && !trial.thrown[0].phase;++tick) hmbtn_farm_carry_flight(&trial,NULL,m,NULL,section,0x2000);
            if(target ? !trial.offering : trial.thrown[0].phase!=1) continue;
            memcpy(g->intro.player.position,dry,sizeof(dry));memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));
            g->intro.player.facing=face;g->control.mode=0;memcpy(g->intro.camera,dry,sizeof(dry));return true;
        }
    }
    return false;
}
static bool wait(HmbtnScenePreview *s,unsigned stage,HmbtnHost *host,const char *shots,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);bool drawn=false,receipt_drawn=false;
    for(unsigned i=0;i<2200;++i) {
        if(stage ? g->visit.active && g->visit.free && !g->intro.message && g->intro.script.pc==(stage==1 ? 10u : 21u)
                && !g->carry.phase && !g->carry.thrown[0].item.id : !g->visit.active && !g->event_item.phase && !g->carry.thrown[0].item.id && !g->carry.phase) return true;
        REQUIRE(!g->intro.script.boundary);
        if(!drawn && g->intro.message && g->dialogue.message.phase==HMBTN_MESSAGE_WAIT_END && g->runtime.loaded_message==g->intro.message) {
            char name[64];snprintf(name,sizeof(name),"kappa-message-%u",g->intro.message);REQUIRE(capture(s,host,shots,name,error,size));drawn=true;
        }
        if(g->event_item.phase && g->event_item.shown==294) {
            REQUIRE(!g->farm_action.half_fatigue && !hmbtn_save_available(g,error,size));
            if(!receipt_drawn) { REQUIRE(capture(s,host,shots,"kappa-mystic-berry",error,size));receipt_drawn=true; }
        }
        REQUIRE(step(s,g->intro.message ? HMBTN_PAD_CROSS : 0,error,size));
    }
    fprintf(stderr,"Kappa stuck event=%u pc=%u free=%d position=%d,%d,%d message=%u\n",g->visit.event,g->intro.script.pc,g->visit.free,g->intro.player.position[0],g->intro.player.position[1],g->intro.player.position[2],g->intro.message);
    REQUIRE(false);return false;
}
static bool toss(HmbtnScenePreview *s,bool target,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);REQUIRE(g->game.inventory.held_item.id && launch(s,target));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->carry.phase);
    return true;
}
static bool next_cucumber(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);REQUIRE(!g->game.inventory.held_item.id);
    for(unsigned i=0;i<8 && g->game.inventory.held_item.id!=67;++i) REQUIRE(step(s,HMBTN_PAD_R2,error,size) && idle(s,error,size));
    REQUIRE(g->game.inventory.held_item.id==67);return true;
}
static bool refused_gift(HmbtnScenePreview *s,HmbtnHost *host,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);bool found=false;
    for(unsigned i=125;i<=172;++i) {
        const unsigned id=hmbtn_farm_gather_item(&g->gather,i,13,g->game.clock.season);
        if(!id || hmbtn_item_villager_reaction(id,39)==0) continue;
        const HmbtnGatherRecord *r=hmbtn_farm_gather_record(i);memcpy(g->intro.player.position,r->position,sizeof(r->position));
        g->intro.player.position[2]+=224;g->intro.player.facing=0;g->control.mode=0;
        REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && idle(s,error,size) && g->game.inventory.held_item.id==id);found=true;break;
    }
    REQUIRE(found && toss(s,true,error,size) && wait(s,1,host,NULL,error,size));
    REQUIRE(g->visit.active && g->visit.event==197 && !g->visit.gift && !g->village.flags[0xcc]);return true;
}
static bool action_cost(HmbtnScenePreview *s,unsigned expected,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    REQUIRE(hmbtn_scene_preview_load_area(s,0,error,size));g->intro.area=0;
    g->game.clock.hour=23;g->game.weather.today=1;g->farm_action.stamina=g->farm_action.fatigue=0;
    g->field.cells[500]=(HmbtnFarmTile){.value=0x167,.ground=0x167};cell(s,5,11);
    REQUIRE(tool(s,6,error,size) && g->farm_action.fatigue==expected);
    g->game.clock.hour=12;g->game.weather.today=0;g->farm_action.stamina=g->farm_action.max_stamina;
    REQUIRE(hmbtn_scene_preview_load_area(s,68,error,size));g->intro.area=68;return true;
}
int main(int argc,char **argv)
{
    if(argc<3 || argc>4) return 2;
    const char *shots=argc==4 ? argv[3] : NULL;HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};
    CHECK(hmbtn_host_open(&host));const HmbtnFarmClock date={0,0,2,3,12,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&date,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);CHECK(grow(s,error,sizeof(error)));
    CHECK(action_cost(s,4,error,sizeof(error)));
    CHECK(collect(s,1,error,sizeof(error)) && toss(s,false,error,sizeof(error)) && wait(s,0,&host,NULL,error,sizeof(error)) && !g->village.flags[0xcc]);
    CHECK(collect(s,1,error,sizeof(error)));g->game.clock.hour=10;
    CHECK(toss(s,true,error,sizeof(error)) && wait(s,0,&host,NULL,error,sizeof(error)) && !g->visit.active);
    /* Leaving the area resets this visit's unfinished sequence. */
    CHECK(collect(s,1,error,sizeof(error)));g->game.clock.hour=12;
    CHECK(toss(s,true,error,sizeof(error)) && wait(s,1,&host,shots,error,sizeof(error)));
    CHECK(travel(s,64,error,sizeof(error)) && wait(s,0,&host,NULL,error,sizeof(error)) && !g->village.flags[0xcc] && !g->farm_action.half_fatigue);
    /* Two gifts followed by 06:00 must release the script and its actors. */
    CHECK(collect(s,2,error,sizeof(error)));g->game.clock.hour=12;
    CHECK(toss(s,true,error,sizeof(error)) && wait(s,1,&host,NULL,error,sizeof(error))
        && refused_gift(s,&host,error,sizeof(error)) && next_cucumber(s,error,sizeof(error)));
    CHECK(toss(s,true,error,sizeof(error)) && wait(s,2,&host,shots,error,sizeof(error)));
    g->game.clock.hour=5;g->game.clock.minute=59;g->game.clock.frames=14;CHECK(step(s,0,error,sizeof(error)));
    for(unsigned i=0;i<300 && (g->area!=80 || g->morning_fade || g->overnight_tick || g->visit.active);++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(g->area==80 && !g->visit.active && !g->farm_action.half_fatigue && !g->village.flags[0xcc] && !hmbtn_village_event_actor(&g->village,27));
    char before[1024];snprintf(before,sizeof(before),"%s.before",argv[2]);CHECK(field_reload(&s,&host,argv[1],before,error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(collect(s,3,error,sizeof(error)));g->game.clock.hour=12;g->game.weather.today=1;
    const unsigned max=g->farm_action.max_stamina,berries=g->menu.stats.power_berries;
    CHECK(toss(s,true,error,sizeof(error)) && wait(s,1,&host,shots,error,sizeof(error)) && next_cucumber(s,error,sizeof(error)));
    CHECK(toss(s,true,error,sizeof(error)) && wait(s,2,&host,shots,error,sizeof(error)) && next_cucumber(s,error,sizeof(error)));
    CHECK(toss(s,true,error,sizeof(error)) && wait(s,0,&host,shots,error,sizeof(error)));
    CHECK(g->village.flags[0xcc] && g->farm_action.half_fatigue && g->farm_action.max_stamina==max && g->menu.stats.power_berries==berries);
    CHECK(action_cost(s,2,error,sizeof(error)));
    CHECK(collect(s,1,error,sizeof(error)) && toss(s,true,error,sizeof(error)) && wait(s,0,&host,NULL,error,sizeof(error)));
    CHECK(g->village.flags[0xcc] && g->farm_action.half_fatigue);
    CHECK(field_reload(&s,&host,argv[1],argv[2],error,sizeof(error)));
    puts("Kappa: bought/grew cucumbers, dry/hour/wrong-gift refusals, area/dawn cancellation, three gifts in rain, original dialogue/ANI/camera, Mystic Berry, real half-fatigue, no repeat and two checkpoints passed");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

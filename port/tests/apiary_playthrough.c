/* Buy and grow Pink Cat through installed resources; no hive, honey, gift
 * timer or bonus is granted by this driver. Money/positions/clear soil are fixtures. */
#include "poultry_helpers.h"
#include "field_helpers.h"
static bool wait_feed(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<200 && (g->feed.phase || g->quick_ticks);++i) REQUIRE(step(s,0,error,size));
    return !g->feed.phase && !g->quick_ticks;
}
static bool honey(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<50;++i) {
        const uint8_t *z=hmbtn_scene_preview_resources(s).section+16+i*44;
        if(!(hmbtn_read_le16(z)&0x4000u) || hmbtn_read_le16(z+2)!=47) continue;
        at(s,z);g->intro.player.facing=1;g->intro.player.position[0]+=224;
        memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));
        memcpy(g->intro.camera,g->intro.player.position,sizeof(g->intro.camera));
        REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && wait_feed(s,error,size));return true;
    }
    return false;
}
static bool sleep_bed(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);REQUIRE(g->area==0 && travel(s,80,error,size));
    const uint8_t *zone=NULL;
    for(unsigned i=0;i<50;++i) {
        const uint8_t *z=hmbtn_scene_preview_resources(s).section+16+i*44;
        if((hmbtn_read_le16(z)&0x4000u) && hmbtn_read_le16(z+2)==2) { zone=z;break; }
    }
    REQUIRE(zone);at(s,zone);const unsigned face=hmbtn_read_le16(zone+8);REQUIRE(face<4);
    g->intro.player.position[face&1u ? 0 : 2]+=face<2 ? 224 : -224;
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->house.mode==HMBTN_HOUSE_BED);
    for(unsigned i=0;i<500 && g->house.message.phase!=HMBTN_MESSAGE_WAIT_END;++i) REQUIRE(step(s,0,error,size));
    REQUIRE(g->house.message.phase==HMBTN_MESSAGE_WAIT_END && step(s,HMBTN_PAD_CROSS,error,size));
    const uint64_t date=g->daily_date;
    for(unsigned i=0;i<1600 && (g->daily_date==date || g->morning_fade || g->house.mode!=HMBTN_HOUSE_FREE);++i) REQUIRE(step(s,0,error,size));
    REQUIRE(g->daily_date==date+1 && !g->morning_fade && !g->apiary.collected && travel(s,0,error,size));return true;
}
static bool visit(HmbtnScenePreview *s,HmbtnHost *host,const char *shots,unsigned event,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);bool drawn=false,moved=false,said=false;
    int32_t previous_y=INT32_MIN;
    for(unsigned i=0;i<6000 && g->visit.active;++i) {
        REQUIRE(g->visit.event==event && !g->intro.script.boundary);
        const HmbtnIntroActor *actor=hmbtn_village_actor(&g->village,event==277 ? 27 : 31);
        if(actor->visible) {
            if(previous_y!=INT32_MIN && actor->position[1]!=previous_y) moved=true;
            previous_y=actor->position[1];
            if(!drawn && !g->intro.fade_level
                    && (event==277 ? g->intro.script.pc>=8 && g->intro.camera[0]<-6000 : g->intro.message==492)) {
                REQUIRE(capture(s,host,shots,event==277 ? "bees-arrival" : "louis-honey",error,size));drawn=true;
            }
        }
        if(g->intro.message) { REQUIRE(event==171 && (g->intro.message==492 || g->intro.message==493));said=true; }
        REQUIRE(step(s,g->intro.message ? HMBTN_PAD_CROSS : 0,error,size));
    }
    REQUIRE(!g->visit.active && !g->intro.script.boundary && g->control.enabled && drawn);
    REQUIRE(event==277 ? moved && g->apiary.owned : said && g->game.economy.shipping.honey_bonus);
    REQUIRE(!hmbtn_village_event_actor(&g->village,event==277 ? 27 : 31));return true;
}
static bool checkpoint(HmbtnScenePreview **s,HmbtnHost *host,const char *root,const char *base,const char *suffix,char *error,size_t size)
{ char path[1024];if(snprintf(path,sizeof(path),"%s%s",base,suffix)>=(int)sizeof(path)) return false;return field_reload(s,host,root,path,error,size); }
static bool ship_honey(HmbtnScenePreview *s,unsigned price,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);int32_t box[3];REQUIRE(g->game.inventory.held_item.id==151);
    REQUIRE(box_position(&hmbtn_scene_preview_resources(s).scene->terrain,box));
    const unsigned before=g->game.economy.shipping.pending_value;
    int y=-300,v=0;unsigned ticks=0;do { y+=v;v+=9;if(v>45)v=45;++ticks; } while(y<box[1] && ticks<100);
    g->intro.player.position[0]=box[0]-(int32_t)ticks*44;g->intro.player.position[1]=0;g->intro.player.position[2]=box[2];
    g->intro.player.facing=3;g->control.mode=0;
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && idle(s,error,size));
    for(unsigned i=0;i<100 && g->carry.thrown[0].item.id;++i) REQUIRE(step(s,0,error,size));
    REQUIRE(!g->game.inventory.held_item.id && !g->carry.thrown[0].item.id && g->game.economy.shipping.pending_value==before+price);return true;
}
static bool gift_louis(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);REQUIRE(g->game.inventory.held_item.id==151);
    hmbtn_session_advance_minutes(g,6*60);
    bool found=false;
    for(unsigned hour=0;hour<24 && !found;++hour) {
        REQUIRE(step(s,0,error,size));
        const unsigned target=g->village.people[31].location;
        for(unsigned area=0;target!=54 && area<216;area+=4) {
            unsigned location;
            if(!hmbtn_scene_preview_area_location(s,area,&location,error,size) || location!=target) continue;
            REQUIRE(hmbtn_scene_preview_load_area(s,area,error,size) && step(s,0,error,size));
            found=g->village.people[31].actor.visible;if(found) break;
        }
        if(!found) hmbtn_session_advance_minutes(g,60);
    }
    REQUIRE(found);HmbtnIntroActor *p=&g->village.people[31].actor;
    memcpy(g->intro.player.position,p->position,sizeof(p->position));g->intro.player.position[2]+=280;
    g->intro.player.facing=0;g->control.mode=0;memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));
    const unsigned before=g->village.people[31].friendship;
    REQUIRE(hmbtn_village_target(&g->village,&g->intro.player)==31 && step(s,HMBTN_PAD_CROSS,error,size) && g->talk.active);
    for(unsigned i=0;i<3000 && g->talk.active;++i) REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
    REQUIRE(!g->talk.active && !g->talk.script.boundary && !g->game.inventory.held_item.id
        && g->village.days.count0[19]==0 && g->village.people[31].friendship>before && !g->game.economy.shipping.honey_bonus);
    return true;
}
int main(int argc,char **argv)
{
    if(argc<3 || argc>4) return 2;
    const char *shots=argc==4 ? argv[3] : NULL;HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};
    CHECK(hmbtn_host_open(&host));const HmbtnFarmClock c={0,1,1,3,12,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],204,&c,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->game.economy.gold=5000;
    g->village.flags[251+288]=true; /* Unrelated Won welcoming visit. */
    HmbtnIntroActor *won=&g->village.people[35].actor;CHECK(won->visible);
    memcpy(g->intro.player.position,won->position,sizeof(won->position));g->intro.player.position[2]+=300;g->intro.player.facing=0;g->control.mode=0;
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    unsigned row=0;while(row<hmbtn_seed_shop_count(&g->feed_shop) && hmbtn_seed_shop_choice(&g->feed_shop,row)->item!=0x32) ++row;
    CHECK(row<hmbtn_seed_shop_count(&g->feed_shop));for(unsigned i=0;i<row;++i) CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_COUNT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && g->game.economy.gold==4800 && shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_OFF,error,sizeof(error)));
    for(unsigned i=0;i<800 && g->talk.active;++i) CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(!g->talk.active && hmbtn_inventory_count(&g->game.inventory,0x32,0,2)==1);
    CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)));g->intro.area=0;
    g->field.cells[460]=(HmbtnFarmTile){.value=0x167,.ground=0x167};
    cell(s,10,10);CHECK(tool(s,6,error,sizeof(error)));cell(s,11,10);CHECK(tool(s,0x32,error,sizeof(error)));
    CHECK(g->field.cells[460].value==0x32 && !g->apiary.owned && field_refill(s,error,sizeof(error)));
    for(unsigned day=0;day<6;++day) {
        cell(s,10,10);CHECK(tool(s,21,error,sizeof(error)));g->game.weather.tomorrow=HMBTN_WEATHER_SUNNY;
        CHECK(sleep_bed(s,error,sizeof(error)));
        if(day<5) CHECK(!g->apiary.owned && !g->visit.active);
    }
    for(unsigned i=0;i<20 && !g->visit.active;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(g->field.cells[460].kind==3 && g->visit.active && g->visit.event==277);
    CHECK(visit(s,&host,shots,277,error,sizeof(error)) && g->village.flags[528]);
    CHECK(checkpoint(&s,&host,argv[1],argv[2],".hive",error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(g->apiary.owned && !g->apiary.collected && honey(s,error,sizeof(error)) && g->game.inventory.held_item.id==151);
    CHECK(honey(s,error,sizeof(error)) && g->game.inventory.held_item.id==151 && !g->carry.phase);
    CHECK(step(s,HMBTN_PAD_R2,error,sizeof(error)) && wait_feed(s,error,sizeof(error)) && idle(s,error,sizeof(error)));
    CHECK(!g->game.inventory.held_item.id && honey(s,error,sizeof(error)) && !g->game.inventory.held_item.id);
    CHECK(checkpoint(&s,&host,argv[1],argv[2],".taken",error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(g->apiary.collected && honey(s,error,sizeof(error)) && !g->game.inventory.held_item.id);
    CHECK(step(s,HMBTN_PAD_R2,error,sizeof(error)) && idle(s,error,sizeof(error)) && g->game.inventory.held_item.id==151);
    g->farm_action.stamina=50;g->farm_action.fatigue=20;
    CHECK(step(s,HMBTN_PAD_SQUARE,error,sizeof(error)) && g->event_item.phase==HMBTN_EVENT_ITEM_USE_FOOD);
    for(unsigned i=0;i<200 && g->event_item.phase;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(!g->event_item.phase && !g->game.inventory.held_item.id && g->farm_action.stamina==52 && g->farm_action.fatigue==18);
    CHECK(sleep_bed(s,error,sizeof(error)) && !g->visit.active && honey(s,error,sizeof(error)) && ship_honey(s,50,error,sizeof(error)));
    const unsigned gold=g->game.economy.gold;CHECK(sleep_bed(s,error,sizeof(error)) && g->game.economy.gold==gold+50);
    CHECK(honey(s,error,sizeof(error)) && gift_louis(s,error,sizeof(error)));
    CHECK(checkpoint(&s,&host,argv[1],argv[2],".gift",error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(g->village.days.count0[19]==0 && hmbtn_scene_preview_load_area(s,0,error,sizeof(error)));
    CHECK(step(s,0,error,sizeof(error)) && !g->visit.active && sleep_bed(s,error,sizeof(error)));
    for(unsigned i=0;i<20 && !g->visit.active;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(g->village.days.count0[19]==1 && g->visit.active && g->visit.event==171 && visit(s,&host,shots,171,error,sizeof(error)));
    CHECK(g->village.flags[422] && honey(s,error,sizeof(error)) && ship_honey(s,60,error,sizeof(error)));
    const unsigned bonus_gold=g->game.economy.gold;CHECK(sleep_bed(s,error,sizeof(error)) && g->game.economy.gold==bonus_gold+60 && !g->visit.active);
    CHECK(checkpoint(&s,&host,argv[1],argv[2],"",error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(g->apiary.owned && !g->apiary.collected && g->game.economy.shipping.honey_bonus && !g->village.event_actors[31]);
    /* The hive's four original seasonal sprites remain renderable. */
    for(unsigned season=0;season<4;++season) {
        g->game.clock.season=season;g->daily_date=hmbtn_session_date(&g->game.clock);
        CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)) && step(s,0,error,sizeof(error)));
        const HmbtnGatherRecord *record=hmbtn_farm_gather_record(0);memcpy(g->intro.camera,record->position,sizeof(record->position));
        char name[32];snprintf(name,sizeof(name),"hive-season-%u",season);CHECK(capture(s,&host,shots,name,error,sizeof(error)));
    }
    puts("Apiary: bought Pink Cat, six watered mornings, original bees/Louis scripts, daily honey, occupied hands, bag/use, 50/60 G actual shipping/payment and checkpoints passed");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

/* Original farm records -> pickup/bag/food/recipe/gift/shipping and seasons.
 * Date, positions and kitchen ownership are fixtures; no apple is granted. */
#include "poultry_helpers.h"
#include "field_helpers.h"
#include "cooking.h"
static bool house_wait(HmbtnScenePreview *s,HmbtnHouseMode mode,char *error,size_t size)
{
    HmbtnHouseInteraction *h=&hmbtn_scene_preview_session(s)->house;
    for(unsigned i=0;i<2000;++i) {
        if(h->mode==mode && (mode==HMBTN_HOUSE_FREE || (!h->closing && h->message.phase==HMBTN_MESSAGE_WAIT_END))) return true;
        REQUIRE(step(s,h->message.phase==HMBTN_MESSAGE_WAIT_PAGE ? HMBTN_PAD_CROSS : 0,error,size));
    }
    return false;
}
static bool ready(HmbtnScenePreview *s,HmbtnKitchenPhase phase,char *error,size_t size)
{ REQUIRE(house_wait(s,HMBTN_HOUSE_COOKING,error,size));return hmbtn_scene_preview_session(s)->house.kitchen.phase==phase; }
static bool object(HmbtnScenePreview *s,unsigned kind,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<50;++i) {
        const uint8_t *z=hmbtn_scene_preview_resources(s).section+16+i*44;
        if(!(hmbtn_read_le16(z)&0x4000u) || hmbtn_read_le16(z+2)!=kind) continue;
        at(s,z);unsigned face=hmbtn_read_le16(z+8);REQUIRE(face<4);
        g->intro.player.position[face&1 ? 0 : 2]+=face<2 ? 224 : -224;
        memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));
        memcpy(g->intro.camera,g->intro.player.position,sizeof(g->intro.camera));
        return step(s,HMBTN_PAD_CROSS,error,size);
    }
    return false;
}
static bool apple(HmbtnScenePreview *s,unsigned index,bool exists,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const HmbtnGatherRecord *r=hmbtn_farm_gather_record(index);
    REQUIRE(g->area==0 && index>=1 && index<=3 && !g->game.inventory.held_item.id);
    memcpy(g->intro.player.position,r->position,sizeof(r->position));g->intro.player.position[2]+=224;
    /* The second record overlaps the tree's honey zone. Aim at its western
     * pickup edge, as in the original; zone interactions keep their priority. */
    if(index==2) g->intro.player.position[0]-=120;
    g->intro.player.facing=0;g->control.mode=0;memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));
    memcpy(g->intro.camera,g->intro.player.position,sizeof(g->intro.camera));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
    REQUIRE(exists ? g->carry.phase==1 && g->game.inventory.held_item.id==87 && g->gather.taken[index]
        : !g->carry.phase && !g->game.inventory.held_item.id);
    REQUIRE(idle(s,error,size));return true;
}
static bool consume(HmbtnScenePreview *s,unsigned item,unsigned gain,unsigned loss,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const unsigned stamina=g->farm_action.stamina,fatigue=g->farm_action.fatigue;
    REQUIRE(g->game.inventory.held_item.id==item && step(s,HMBTN_PAD_SQUARE,error,size) && hmbtn_event_item_consuming(&g->event_item));
    for(unsigned i=0;i<600 && g->event_item.phase;++i) REQUIRE(step(s,0,error,size));
    REQUIRE(!g->event_item.phase && !g->game.inventory.held_item.id
        && g->farm_action.stamina==stamina+gain && g->farm_action.fatigue==fatigue-loss);return true;
}
static bool sleep_bed(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);REQUIRE(g->area==0 && travel(s,80,error,size));
    REQUIRE(object(s,2,error,size) && house_wait(s,HMBTN_HOUSE_BED,error,size));
    const uint64_t date=g->daily_date;REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
    for(unsigned i=0;i<1600 && (g->daily_date==date || g->morning_fade || g->house.mode!=HMBTN_HOUSE_FREE);++i) REQUIRE(step(s,0,error,size));
    REQUIRE(g->daily_date==date+1 && !g->morning_fade && travel(s,0,error,size));return true;
}
static bool checkpoint(HmbtnScenePreview **s,HmbtnHost *host,const char *root,const char *base,const char *suffix,char *error,size_t size)
{ char path[1024];if(snprintf(path,sizeof(path),"%s%s",base,suffix)>=(int)sizeof(path)) return false;return field_reload(s,host,root,path,error,size); }
static bool juice(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    REQUIRE(g->area==80 && g->game.inventory.items[0].id==87 && object(s,5,error,size) && ready(s,HMBTN_KITCHEN_OPEN,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && ready(s,HMBTN_KITCHEN_FIRST,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && ready(s,HMBTN_KITCHEN_SECOND,error,size));
    REQUIRE(step(s,HMBTN_PAD_DOWN,error,size) && step(s,HMBTN_PAD_CROSS,error,size) && ready(s,HMBTN_KITCHEN_TOOLS,error,size));
    for(unsigned i=0;i<3;++i) REQUIRE(step(s,HMBTN_PAD_RIGHT,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->house.kitchen.options==8);
    REQUIRE(step(s,HMBTN_PAD_START,error,size) && ready(s,HMBTN_KITCHEN_ITEMS,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && step(s,HMBTN_PAD_START,error,size) && ready(s,HMBTN_KITCHEN_CONFIRM,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
    for(unsigned i=0;i<2500 && g->house.kitchen.phase!=HMBTN_KITCHEN_RESULT;++i) REQUIRE(step(s,0,error,size));
    REQUIRE(ready(s,HMBTN_KITCHEN_RESULT,error,size) && g->game.inventory.held_item.id==180 && g->game.inventory.held_item.modifier==20);
    REQUIRE(!g->game.inventory.items[0].id && step(s,HMBTN_PAD_CROSS,error,size) && house_wait(s,HMBTN_HOUSE_FREE,error,size));return true;
}
static bool deposit(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    REQUIRE(g->game.inventory.held_item.id==87 && object(s,4,error,size) && house_wait(s,HMBTN_HOUSE_TOOLBOX,error,size));
    REQUIRE(g->house.storage==2 && !g->house.cursor);
    if(g->house.detail) REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && house_wait(s,HMBTN_HOUSE_TOOLBOX,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && step(s,HMBTN_PAD_DOWN,error,size));
    REQUIRE(g->house.cursor==9 && step(s,HMBTN_PAD_CROSS,error,size));
    REQUIRE(g->game.inventory.icebox[0].id==87 && !g->game.inventory.held_item.id);
    REQUIRE(step(s,HMBTN_PAD_CIRCLE,error,size) && house_wait(s,HMBTN_HOUSE_FREE,error,size));return true;
}
static bool gift(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);hmbtn_session_advance_minutes(g,6*60);
    REQUIRE(hmbtn_scene_preview_load_area(s,128,error,size) && step(s,0,error,size));
    HmbtnIntroActor *p=&g->village.people[18].actor;REQUIRE(p->visible && g->game.inventory.held_item.id==87);
    memcpy(g->intro.player.position,p->position,sizeof(p->position));g->intro.player.position[2]+=280;
    g->intro.player.facing=0;g->control.mode=0;memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));
    const unsigned before=g->village.people[18].friendship;
    REQUIRE(hmbtn_village_target(&g->village,&g->intro.player)==18 && step(s,HMBTN_PAD_CROSS,error,size) && g->talk.active);
    for(unsigned i=0;i<3000 && g->talk.active;++i) REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
    REQUIRE(!g->talk.active && !g->talk.script.boundary && !g->game.inventory.held_item.id && g->village.people[18].friendship>before);
    return true;
}
int main(int argc,char **argv)
{
    if(argc<3 || argc>4) return 2;
    const char *shots=argc==4 ? argv[3] : NULL;HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};
    CHECK(hmbtn_host_open(&host));const HmbtnFarmClock date={0,2,28,2,6,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&date,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    g->poultry.upgraded=true;g->construction.house_level=1;g->game.cooking.utensils=8;
    g->game.weather.tomorrow=HMBTN_WEATHER_SUNNY;
    CHECK(g->game.clock.season==2 && !g->apiary.owned);
    CHECK(apple(s,1,true,error,sizeof(error)));
    CHECK(capture(s,&host,shots,"farm-apples-held",error,sizeof(error)));
    g->farm_action.stamina=50;g->farm_action.fatigue=20;
    CHECK(consume(s,87,2,1,error,sizeof(error)) && apple(s,1,false,error,sizeof(error)));
    CHECK(apple(s,2,true,error,sizeof(error)) && step(s,HMBTN_PAD_R2,error,sizeof(error)) && idle(s,error,sizeof(error)));
    CHECK(g->game.inventory.items[0].id==87 && apple(s,3,true,error,sizeof(error)) && ship(s,error,sizeof(error)));
    for(unsigned i=1;i<=3;++i) CHECK(apple(s,i,false,error,sizeof(error)));
    CHECK(checkpoint(&s,&host,argv[1],argv[2],".picked",error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    for(unsigned i=1;i<=3;++i) CHECK(g->gather.taken[i] && apple(s,i,false,error,sizeof(error)));
    CHECK(travel(s,80,error,sizeof(error)) && juice(s,error,sizeof(error)));
    CHECK(capture(s,&host,shots,"apple-juice",error,sizeof(error)));
    CHECK(checkpoint(&s,&host,argv[1],argv[2],".juice",error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(consume(s,180,20,4,error,sizeof(error)) && travel(s,0,error,sizeof(error)));
    const unsigned gold=g->game.economy.gold;
    CHECK(sleep_bed(s,error,sizeof(error)) && g->game.clock.day==29 && g->game.clock.season==2 && g->game.economy.gold==gold+50);
    for(unsigned i=1;i<=3;++i) CHECK(!g->gather.taken[i] && hmbtn_farm_gather_item(&g->gather,i,1,2)==87);
    CHECK(apple(s,1,true,error,sizeof(error)) && gift(s,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)) && step(s,0,error,sizeof(error)) && apple(s,2,true,error,sizeof(error)));
    CHECK(travel(s,80,error,sizeof(error)) && deposit(s,error,sizeof(error)) && travel(s,0,error,sizeof(error)));
    CHECK(apple(s,3,true,error,sizeof(error)) && ship(s,error,sizeof(error)));
    CHECK(checkpoint(&s,&host,argv[1],argv[2],".stock",error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(g->game.inventory.icebox[0].id==87 && g->game.inventory.icebox[0].count==1 && sleep_bed(s,error,sizeof(error)));
    CHECK(g->game.clock.season==3 && g->game.clock.day==0 && g->game.economy.gold==gold+100);
    for(unsigned i=1;i<=3;++i) CHECK(!g->gather.taken[i] && apple(s,i,false,error,sizeof(error)));
    CHECK(g->game.inventory.icebox[0].id==87 && capture(s,&host,shots,"winter-no-apples",error,sizeof(error)));
    CHECK(checkpoint(&s,&host,argv[1],argv[2],"",error,sizeof(error)));
    puts("Farm apples: three original fall pickups, no duplicates after reload, bag/food/real juice, Lillia gift, icebox, two 50 G shipments/payments, morning return and winter boundary passed");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

/* Real TV/inn phone, original scripts 290..298, kitchen and shipping. Prior
 * expansion, starting purse and cucumber are fixtures; no offer, due flag,
 * utensil or berry is granted. Area loads shorten unrelated village travel. */
#include "poultry_helpers.h"
#include "field_helpers.h"
#include "tv_shopping.h"
#include "cooking.h"
static bool wait_house(HmbtnScenePreview *s,HmbtnHouseMode mode,unsigned request,char *error,size_t size)
{
    HmbtnHouseInteraction *h=&hmbtn_scene_preview_session(s)->house;
    for(unsigned i=0;i<1600;++i) {
        if(h->mode==mode && (mode==HMBTN_HOUSE_FREE || (!h->closing && h->message.phase==HMBTN_MESSAGE_WAIT_END
                && (!request || h->request==request)))) return true;
        REQUIRE(step(s,h->message.phase==HMBTN_MESSAGE_WAIT_PAGE ? HMBTN_PAD_CROSS : 0,error,size));
    }
    fprintf(stderr,"House mode %u, request %x, phase %u; expected %u/%x\n",h->mode,h->request,h->message.phase,mode,request);return false;
}
static bool object(HmbtnScenePreview *s,unsigned id,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<50;++i) {
        const uint8_t *z=hmbtn_scene_preview_resources(s).section+16+i*44;
        if(hmbtn_read_le16(z)!=0x4000 || hmbtn_read_le16(z+2)!=id) continue;
        at(s,z);const unsigned face=hmbtn_read_le16(z+8);REQUIRE(face<4);
        g->intro.player.position[face&1 ? 0 : 2]+=face<2 ? 224 : -224;
        memcpy(g->intro.camera,g->intro.player.position,sizeof(g->intro.camera));
        return step(s,HMBTN_PAD_CROSS,error,size);
    }
    snprintf(error,size,"Missing original interaction %u",id);return false;
}
static bool close_house(HmbtnScenePreview *s,char *error,size_t size)
{
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
    return wait_house(s,HMBTN_HOUSE_FREE,0,error,size);
}
static bool kitchen_tools(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnHouseInteraction *h=&hmbtn_scene_preview_session(s)->house;
    REQUIRE(object(s,5,error,size) && wait_house(s,HMBTN_HOUSE_COOKING,0,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
    for(unsigned i=0;i<25 && h->kitchen.phase==HMBTN_KITCHEN_OPEN;++i) REQUIRE(step(s,0,error,size));
    REQUIRE(wait_house(s,HMBTN_HOUSE_COOKING,0,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && wait_house(s,HMBTN_HOUSE_COOKING,0,error,size));
    REQUIRE(step(s,HMBTN_PAD_DOWN,error,size) && step(s,HMBTN_PAD_CROSS,error,size)
        && wait_house(s,HMBTN_HOUSE_COOKING,0,error,size));
    REQUIRE(h->kitchen.phase==HMBTN_KITCHEN_TOOLS);return true;
}
static bool salad(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);REQUIRE(kitchen_tools(s,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->house.kitchen.options==1);
    REQUIRE(step(s,HMBTN_PAD_START,error,size) && wait_house(s,HMBTN_HOUSE_COOKING,0,error,size));
    REQUIRE(step(s,HMBTN_PAD_R1,error,size) && step(s,HMBTN_PAD_CROSS,error,size));
    REQUIRE(g->house.kitchen.values[0]==0x43);
    REQUIRE(step(s,HMBTN_PAD_START,error,size) && wait_house(s,HMBTN_HOUSE_COOKING,0,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && wait_house(s,HMBTN_HOUSE_COOKING,0,error,size));
    REQUIRE(g->game.inventory.held_item.id==0xb3 && !g->game.inventory.icebox[0].id);
    for(unsigned i=0;i<200 && g->house.mode!=HMBTN_HOUSE_FREE;++i) REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
    REQUIRE(g->house.mode==HMBTN_HOUSE_FREE && hmbtn_cooking_record_count(&g->game.cooking)==1);return true;
}
static void next_saturday(HmbtnSession *g)
{
    const unsigned minute=g->game.clock.hour*60+g->game.clock.minute;
    unsigned days=(13-g->game.clock.weekday)%7;
    if(!days && minute>=360) days=7;
    hmbtn_session_advance_minutes(g,days*1440+360-minute);
}
int main(int argc,char **argv)
{
    if(argc<3 || argc>4) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};
    const char *shots=argc==4 ? argv[3] : NULL;
    CHECK(hmbtn_host_open(&host));const HmbtnFarmClock clock={0,0,5,6,6,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&clock,error,sizeof(error));CHECK(s);
    CHECK(hmbtn_scene_preview_load_area(s,80,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_set_weather(s,0,0));
    CHECK(object(s,3,error,sizeof(error)) && wait_house(s,HMBTN_HOUSE_TV_SELECT,0,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_LEFT,error,sizeof(error)) && g->house.tv_group!=2 && !g->game.shopping.offers[0]);
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && wait_house(s,HMBTN_HOUSE_FREE,0,error,sizeof(error)));
    g->poultry.upgraded=true;g->construction.house_level=1;g->game.economy.gold=100000;g->game.inventory.rucksack_level=2;
    g->game.inventory.icebox[0]=(HmbtnStoredTool){0x43,0,1};
    g->village.flags[251+288]=g->village.flags[251+140]=true;
    CHECK(hmbtn_scene_preview_load_area(s,80,error,sizeof(error)));
    const uint16_t ingredients[8]={0x43};
    CHECK(!hmbtn_cooking_available(&g->game.cooking,&g->game.inventory,1,ingredients));
    CHECK(object(s,3,error,sizeof(error)) && wait_house(s,HMBTN_HOUSE_TV_SELECT,0,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_LEFT,error,sizeof(error)) && g->house.request==0x258);
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && wait_house(s,HMBTN_HOUSE_FREE,0,error,sizeof(error)));
    CHECK(g->game.shopping.offers[0]==HMBTN_TV_OFFERED);
    CHECK(hmbtn_scene_preview_load_area(s,204,error,sizeof(error)));
    /* Move the fixture ingredient to the hand to check zone/throw priority. */
    g->game.inventory.held_item=g->game.inventory.icebox[0];g->game.inventory.icebox[0]=(HmbtnStoredTool){0};
    CHECK(object(s,21,error,sizeof(error)) && wait_house(s,HMBTN_HOUSE_PHONE,0x11a,error,sizeof(error)));
    CHECK(close_house(s,error,sizeof(error)));
    CHECK(g->game.inventory.held_item.id==0x43 && !g->carry.phase && !g->carry.thrown[0].item.id);
    g->game.inventory.icebox[0]=g->game.inventory.held_item;g->game.inventory.held_item=(HmbtnStoredTool){0};
    for(unsigned offer=0;offer<9;++offer) {
        if(offer) next_saturday(g);
        CHECK(hmbtn_scene_preview_set_weather(s,0,0) && hmbtn_scene_preview_load_area(s,80,error,sizeof(error)));
        CHECK(object(s,3,error,sizeof(error)) && wait_house(s,HMBTN_HOUSE_TV_SELECT,0,error,sizeof(error)));
        CHECK(step(s,HMBTN_PAD_LEFT,error,sizeof(error)) && wait_house(s,HMBTN_HOUSE_TV,0x258+offer,error,sizeof(error)));
        CHECK(g->house.tv_group==2 && g->game.shopping.offers[offer]==HMBTN_TV_WATCHED);
        CHECK(capture(s,&host,shots,"tv-shopping-program",error,sizeof(error)));
        CHECK(close_house(s,error,sizeof(error)));
        CHECK(hmbtn_scene_preview_load_area(s,204,error,sizeof(error)));
        CHECK(object(s,21,error,sizeof(error)) && wait_house(s,HMBTN_HOUSE_PHONE,0x11b,error,sizeof(error)));
        CHECK(capture(s,&host,shots,"inn-phone-call",error,sizeof(error)));
        const unsigned gold=g->game.economy.gold,owned=g->game.cooking.utensils;
        CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && wait_house(s,HMBTN_HOUSE_PHONE,0x11c,error,sizeof(error)));
        CHECK(g->game.economy.gold==gold-10);
        CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && wait_house(s,HMBTN_HOUSE_PHONE,0x11d+offer,error,sizeof(error)));
        CHECK(capture(s,&host,shots,"inn-phone-purchase",error,sizeof(error)));
        CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && wait_house(s,HMBTN_HOUSE_PHONE,0x126,error,sizeof(error)));
        CHECK(g->game.economy.gold==gold-10-(unsigned)hmbtn_farm_item(hmbtn_tv_shopping_item(offer))->buy_price);
        CHECK(g->game.shopping.offers[offer]==HMBTN_TV_PAID && g->village.flags[0x67+offer] && g->game.cooking.utensils==owned);
        CHECK(close_house(s,error,sizeof(error)));
        if(!offer) {
            char pending[1024];CHECK(snprintf(pending,sizeof(pending),"%s.pending",argv[2])<(int)sizeof(pending));
            CHECK(field_reload(&s,&host,argv[1],pending,error,sizeof(error)));g=hmbtn_scene_preview_session(s);
        }
        CHECK(hmbtn_scene_preview_load_area(s,80,error,sizeof(error)) && travel(s,0,error,sizeof(error)));
        CHECK(step(s,0,error,sizeof(error)) && !g->visit.active); /* Saturday. */
        if(!offer) {
            HmbtnFarmClock candidate=g->game.clock;candidate.weekday=2;candidate.hour=11;candidate.minute=59;
            CHECK(hmbtn_farm_event_trigger(&g->triggers,&g->village,&candidate,0,2,2,0,&g->game.inventory)==290);
            candidate.hour=12;candidate.minute=0;
            CHECK(!hmbtn_farm_event_trigger(&g->triggers,&g->village,&candidate,0,2,2,0,&g->game.inventory));
            candidate.hour=6;
            CHECK(!hmbtn_farm_event_trigger(&g->triggers,&g->village,&candidate,0,1,2,0,&g->game.inventory));
            CHECK(!hmbtn_farm_event_trigger(&g->triggers,&g->village,&candidate,0,2,0,0,&g->game.inventory));
        }
        hmbtn_session_advance_minutes(g,2*1440); /* Monday is also excluded. */
        CHECK(hmbtn_scene_preview_load_area(s,80,error,sizeof(error)) && travel(s,0,error,sizeof(error)));
        CHECK(step(s,0,error,sizeof(error)) && !g->visit.active);
        hmbtn_session_advance_minutes(g,1440);
        CHECK(hmbtn_scene_preview_set_weather(s,0,0));
        const unsigned berries=g->menu.stats.power_berries,max=g->farm_action.max_stamina;
        CHECK(hmbtn_scene_preview_load_area(s,80,error,sizeof(error)) && travel(s,0,error,sizeof(error)));
        bool seen=false;
        for(unsigned i=0;i<5000;++i) {
            CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
            if(g->visit.active) {
                CHECK(g->visit.event==290+offer);
                if(!seen) { CHECK(capture(s,&host,shots,"zack-shopping-delivery",error,sizeof(error)));seen=true; }
            }
            if(seen && !g->visit.active && g->control.enabled && !g->intro.fade_level) break;
        }
        CHECK(seen && !g->visit.active && g->control.enabled && !g->intro.script.boundary);
        if(offer<8) CHECK(g->game.cooking.utensils==(owned|(1u<<offer)) && !g->village.flags[0x67+offer]);
        else CHECK(g->menu.stats.power_berries==berries+1 && g->farm_action.max_stamina==max+10 && !g->game.inventory.held_item.id);
        CHECK(g->village.flags[251+290+offer]);
        CHECK(hmbtn_scene_preview_load_area(s,80,error,sizeof(error)) && travel(s,0,error,sizeof(error)));
        CHECK(step(s,0,error,sizeof(error)) && !g->visit.active); /* No duplicate. */
        CHECK(hmbtn_scene_preview_load_area(s,80,error,sizeof(error)));
        g->intro.camera[0]=-2200;g->intro.camera[1]=0;g->intro.camera[2]=900;
        CHECK(capture(s,&host,shots,"kitchen-delivered-utensils",error,sizeof(error)));
        if(!offer) {
            CHECK(hmbtn_cooking_available(&g->game.cooking,&g->game.inventory,1,ingredients));
            CHECK(salad(s,error,sizeof(error)) && capture(s,&host,shots,"purchased-knife-salad",error,sizeof(error)));
            const unsigned pending=g->game.economy.shipping.pending_value;
            CHECK(travel(s,0,error,sizeof(error)) && ship(s,error,sizeof(error)));
            /* Prepared salad has zero shipping price in the original catalog. */
            CHECK(!g->game.inventory.held_item.id && g->game.economy.shipping.pending_value==pending);
        }
    }
    CHECK(g->game.cooking.utensils==255 && hmbtn_cooking_allowed_options(&g->game.cooking)==0xfff);
    next_saturday(g);CHECK(hmbtn_scene_preview_set_weather(s,0,0) && hmbtn_scene_preview_load_area(s,80,error,sizeof(error)));
    CHECK(object(s,3,error,sizeof(error)) && wait_house(s,HMBTN_HOUSE_TV_SELECT,0,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_LEFT,error,sizeof(error)) && g->house.tv_group!=2);
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && wait_house(s,HMBTN_HOUSE_FREE,0,error,sizeof(error)));
    CHECK(g->game.shopping.offers[8]==HMBTN_TV_CLOSED);
    g->construction.barn_level=1;g->construction.house_level=2;CHECK(hmbtn_scene_preview_load_area(s,80,error,sizeof(error)));
    g->intro.camera[0]=-2400;g->intro.camera[1]=0;g->intro.camera[2]=2500;
    CHECK(capture(s,&host,shots,"second-kitchen-utensils",error,sizeof(error)));
    CHECK(field_reload(&s,&host,argv[1],argv[2],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(g->game.cooking.utensils==255 && hmbtn_cooking_record_count(&g->game.cooking)==1);
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);
    puts("TV Shopping: nine real broadcasts/orders/deliveries, fee/cancellation, kitchen utensils, recipe/shipping and pending/completed resumes passed.");return 0;
fail:
    if(s) hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

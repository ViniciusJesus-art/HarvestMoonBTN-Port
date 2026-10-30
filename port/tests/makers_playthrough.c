/* Installed mine, Saibara script 354, actual base ANI/maps and shipping.
 * Fixtures: physical buildings, purse, rucksack, initial mine RNG and energy;
 * no material/machine is granted. Area loads shorten unrelated village travel. */
#include "poultry_helpers.h"
#include "field_helpers.h"
static bool settle(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<500 && (g->farm_action.phase!=HMBTN_FARM_ACTION_FREE || g->event_item.phase || g->mine.reward
            || g->carry.phase || g->quick_ticks);++i) REQUIRE(step(s,0,error,size));
    REQUIRE(!g->mine.reward && !g->event_item.phase && !g->carry.phase && !g->quick_ticks
        && g->farm_action.phase==HMBTN_FARM_ACTION_FREE);return true;
}
static bool clear_hand(HmbtnScenePreview *s,bool keep,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);if(!g->game.inventory.held_item.id) return true;
    if(keep) { REQUIRE(step(s,HMBTN_PAD_R2,error,size) && settle(s,error,size));REQUIRE(!g->game.inventory.held_item.id); }
    else {
        REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && settle(s,error,size));
        for(unsigned i=0;i<150;++i) { bool active=false;for(unsigned j=0;j<3;++j) active|=g->carry.thrown[j].item.id!=0;
            if(!active) return true;REQUIRE(step(s,0,error,size)); }
        return false;
    }
    return true;
}
static bool aim_cell(HmbtnScenePreview *s,unsigned cell,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    static const int offsets[4][2]={{0,400},{400,0},{0,-400},{-400,0}};
    for(unsigned face=0;face<4;++face) {
        int32_t p[3]={-1800+(int)(cell%10)*400+offsets[face][0],0,1800-(int)(cell/10)*400+offsets[face][1]};
        unsigned stand;
        if(!hmbtn_mine_cell(&g->mine,p,&stand) || (g->mine.layer && stand==g->mine.exit_cell[g->mine.layer-1])
                || stand==g->mine.exit_cell[g->mine.layer]) continue;
        if(!hmbtn_farm_settle(NULL,&hmbtn_scene_preview_resources(s).scene->terrain,p,0xE080,64)) continue;
        memcpy(g->intro.player.position,p,sizeof(p));g->intro.player.facing=face;g->control.mode=0;return true;
    }
    (void)error;(void)size;return false;
}
static bool dig(HmbtnScenePreview *s,HmbtnHost *host,unsigned cell,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);REQUIRE(aim_cell(s,cell,error,size));
    const unsigned stamina=g->farm_action.stamina,berries=g->menu.stats.power_berries;
    REQUIRE(tool(s,6,error,size));
    static bool seen[380];const unsigned shown=g->event_item.shown;
    if(shown && shown<380 && !seen[shown]) { REQUIRE(capture(s,host,NULL,"found",error,size));seen[shown]=true; }
    REQUIRE(settle(s,error,size));
    REQUIRE((g->farm_action.stamina<stamina || g->menu.stats.power_berries>berries) && g->mine.cells[g->mine.layer][cell]);return true;
}
static bool question(HmbtnScenePreview *s,HmbtnHost *host,const char *shots,unsigned cell,bool yes,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->intro.player.position[0]=-1800+(int)(cell%10)*400;
    g->intro.player.position[1]=0;g->intro.player.position[2]=1800-(int)(cell/10)*400;g->control.mode=0;
    REQUIRE(step(s,0,error,size) && g->mine.prompt);
    for(unsigned i=0;i<500 && g->mine.message.phase!=HMBTN_MESSAGE_WAIT_END;++i) REQUIRE(step(s,0,error,size));
    REQUIRE(g->mine.message.phase==HMBTN_MESSAGE_WAIT_END);
    if(!yes) REQUIRE(capture(s,host,shots,"mine-question",error,size));
    REQUIRE(step(s,yes ? HMBTN_PAD_CROSS : HMBTN_PAD_CIRCLE,error,size));
    for(unsigned i=0;i<120 && (g->mine.back_ticks || g->travel_tick);++i) REQUIRE(step(s,0,error,size));
    REQUIRE(!g->mine.prompt && !g->mine.back_ticks && !g->travel_tick && !g->control.exit_pending);return true;
}

static bool smith(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    REQUIRE(!g->game.inventory.held_item.id && !g->talk.active);
    const unsigned minute=g->game.clock.hour*60+g->game.clock.minute;
    if(minute<720 || minute>=960) hmbtn_session_advance_minutes(g,(1440+720-minute)%1440);
    if(g->game.clock.weekday==4) hmbtn_session_advance_minutes(g,1440);
    REQUIRE(hmbtn_scene_preview_load_area(s,156,error,size));
    for(unsigned i=0;i<110;++i) REQUIRE(step(s,0,error,size));
    HmbtnVillager *npc=&g->village.people[4];
    if(!npc->actor.visible) {
        for(unsigned area=128;area<216;area+=4) {
            unsigned location;REQUIRE(hmbtn_scene_preview_area_location(s,area,&location,error,size));
            if(location==npc->location) { REQUIRE(hmbtn_scene_preview_load_area(s,area,error,size) && step(s,0,error,size));break; }
        }
    }
    REQUIRE(npc->actor.visible);memcpy(g->intro.player.position,npc->actor.position,sizeof(npc->actor.position));
    g->intro.player.position[2]+=300;g->intro.player.facing=0;g->control.mode=0;
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->talk.active);return true;
}
static bool end_talk(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<3000 && (g->talk.active || g->feed_shop.departure || g->travel_tick);++i)
        REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
    REQUIRE(!g->talk.active && !g->talk.script.boundary && !g->feed_shop.departure && !g->travel_tick);return true;
}
static bool convert(HmbtnScenePreview *s,unsigned kind,unsigned input,unsigned output,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    /* Ingredients for quality vectors are fixtures; production is already
     * exercised by poultry/animal tools paths. Machines and orders are real. */
    g->game.inventory.held_item=(HmbtnStoredTool){(uint16_t)input,0,1};
    REQUIRE(feed(s,kind,0,error,size) && g->game.inventory.held_item.id==output);
    REQUIRE(!g->carry.phase);return true;
}
int main(int argc,char **argv)
{
    if(argc<3 || argc>4) return 2;const char *shots=argc==4 ? argv[3] : NULL;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock clock={0,0,3,3,12,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],68,&clock,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->game.inventory.rucksack_level=2;
    g->game.economy.gold=100000;g->farm_action.max_stamina=g->farm_action.stamina=255;
    g->poultry.upgraded=true;g->construction.house_level=g->construction.barn_level=1;
    g->village.flags[251+288]=g->village.flags[251+140]=true;
    CHECK(hmbtn_scene_preview_set_weather(s,0,0) && travel(s,234,error,sizeof(error)));g->mine.random=0x13579;
    unsigned ore=0;
    for(unsigned floor=0;floor<10 && ore<3;++floor) {
        g->farm_action.stamina=255;
        for(unsigned cell=11;cell<89 && ore<3;++cell) {
            if(cell%10==0 || cell%10==9 || !g->mine.active[cell] || g->mine.cells[floor][cell]) continue;
            if(floor && cell==g->mine.exit_cell[floor-1]) continue;
            if(!aim_cell(s,cell,error,sizeof(error))) continue;
            CHECK(dig(s,&host,cell,error,sizeof(error)));
            const bool keep=g->game.inventory.held_item.id==0x62;
            if(keep) ++ore;
            CHECK(clear_hand(s,keep,error,sizeof(error)));
        }
        if(ore<3) { CHECK(g->mine.exit_cell[floor]<100);CHECK(question(s,&host,NULL,g->mine.exit_cell[floor],true,error,sizeof(error))); }
    }
    CHECK(ore==3 && hmbtn_inventory_count(&g->game.inventory,0x62,1,1)==3);
    CHECK(hmbtn_scene_preview_load_area(s,68,error,sizeof(error)) && smith(s,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)) && g->feed_shop.blacksmith);
    for(unsigned order=0x13f;order<=0x141;++order) {
        if(order!=0x13f) { CHECK(smith(s,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error))); }
        CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && step(s,HMBTN_PAD_DOWN,error,sizeof(error))
            && g->feed_shop.selection==order-0x13f+2);
        CHECK(capture(s,&host,shots,"saibara-maker-menu",error,sizeof(error)));
        const unsigned gold=g->game.economy.gold;
        CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_MAKER_CONFIRM,error,sizeof(error)));
        CHECK(capture(s,&host,shots,"saibara-maker-first-confirm",error,sizeof(error)));
        CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
        CHECK(!g->makers.pending && g->game.economy.gold==gold);
        CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_MAKER_CONFIRM,error,sizeof(error)));
        CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_MAKER_AGAIN,error,sizeof(error)));
        CHECK(capture(s,&host,shots,"saibara-maker-second-confirm",error,sizeof(error)));
        CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_OFF,error,sizeof(error)));
        CHECK(end_talk(s,error,sizeof(error)) && g->area==32 && g->makers.pending==order && g->makers.mornings_left==5);
        CHECK(g->game.economy.gold==gold-20000 && !g->game.inventory.held_item.id);
        CHECK(hmbtn_inventory_count(&g->game.inventory,0x62,1,1)==0x141-order);
        CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)));
        const unsigned owned=g->makers.owned;
        CHECK(hmbtn_scene_preview_load_area(s,156,error,sizeof(error)));
        for(unsigned tick=0;tick<110;++tick) CHECK(step(s,0,error,sizeof(error)));
        CHECK(!g->village.people[4].actor.visible);
        CHECK(!hmbtn_blacksmith_shop_open(&g->feed_shop,&g->game.clock,&g->game.inventory));
        CHECK(g->makers.pending==order && g->makers.mornings_left==5 && g->makers.owned==owned);
        CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)));
        char snapshot[1024];CHECK(snprintf(snapshot,sizeof(snapshot),"%s.pending",argv[2])<(int)sizeof(snapshot));
        CHECK(field_reload(&s,&host,argv[1],snapshot,error,sizeof(error)));g=hmbtn_scene_preview_session(s);
        for(unsigned day=0;day<5;++day) {
            g->game.weather.tomorrow=HMBTN_WEATHER_SUNNY;
            CHECK(sleep_day(s,error,sizeof(error)) && g->makers.mornings_left==4-day && g->makers.pending==order);
        }
        CHECK(smith(s,error,sizeof(error)) && g->talk.pending);
        CHECK(g->talk.bank==2 && g->talk.request==0xa7+order-0x13f);
        CHECK(capture(s,&host,shots,"saibara-delivery",error,sizeof(error)));
        CHECK(end_talk(s,error,sizeof(error)) && !g->makers.pending && g->makers.owned==((1u<<(order-0x13f+1))-1));
        CHECK(g->construction.barn_level==1 && g->poultry.upgraded);
        const unsigned area=order==0x13f ? 112 : 96,kind=order==0x13f ? 14 : order==0x140 ? 33 : 15;
        CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)) && travel(s,area,error,sizeof(error)));
        CHECK(hmbtn_read_le16(hmbtn_scene_preview_resources(s).section+4)==(order==0x13f ? 120 : order==0x140 ? 104 : 108));
        const unsigned inputs[3][4]={{0x122,0x123,0x124,0xe8},{0x8b,0x8c,0x8d,0xe3},{0x91,0x92,0x93,0xe7}};
        const unsigned outputs[3][4]={{0x88,0x89,0x8a,0xe5},{0x8e,0x8f,0x90,0xe4},{0x94,0x95,0x96,0xe6}};
        for(unsigned quality=0;quality<4;++quality) {
            CHECK(convert(s,kind,inputs[order-0x13f][quality],outputs[order-0x13f][quality],error,sizeof(error)));
            memcpy(g->intro.camera,g->intro.player.position,sizeof(g->intro.camera));
            char picture[64];snprintf(picture,sizeof(picture),"maker-%u-quality-%u",order-0x13f,quality);
            CHECK(capture(s,&host,shots,picture,error,sizeof(error)));
            if(!quality) {
                char interior[1024];CHECK(snprintf(interior,sizeof(interior),"%s.interior",argv[2])<(int)sizeof(interior));
                CHECK(field_reload(&s,&host,argv[1],interior,error,sizeof(error)));g=hmbtn_scene_preview_session(s);
                CHECK(g->area==area && g->game.inventory.held_item.id==outputs[order-0x13f][quality]);
                CHECK(hmbtn_read_le16(hmbtn_scene_preview_resources(s).section+4)==(order==0x13f ? 120 : order==0x140 ? 104 : 108));
            }
            CHECK(travel(s,0,error,sizeof(error)) && ship(s,error,sizeof(error)) && travel(s,area,error,sizeof(error)));
        }
        CHECK(travel(s,0,error,sizeof(error)) && field_reload(&s,&host,argv[1],argv[2],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    }
    CHECK(g->makers.owned==7 && !g->makers.pending && hmbtn_inventory_count(&g->game.inventory,0x62,1,1)==0);
    puts("Mined adamantite -> three Saibara orders/deliveries -> all qualities/maps/shipping/resume passed");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

/* Installed maps + actual tool/item ANI and Yes/No/fade transitions. Only the
 * purse/status/backpack and initial RNG are fixtures; no ore or stair is given. */
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
int main(int argc,char **argv)
{
    if(argc<4 || argc>5) return 2;unsigned variant=(unsigned)strtoul(argv[2],NULL,10);const char *shots=argc==5 ? argv[4] : NULL;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};CHECK(hmbtn_host_open(&host));
    HmbtnFarmClock clock={0,variant ? 3u : 0u,3,3,12,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],variant ? 72 : 68,&clock,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->game.inventory.rucksack_level=2;
    g->farm_action.max_stamina=g->farm_action.stamina=255;g->farm_action.fatigue=0;
    CHECK(hmbtn_scene_preview_set_weather(s,0,0));
    const unsigned base=variant ? 245u : 234u;CHECK(travel(s,base,error,sizeof(error)));
    CHECK(g->mine.kind==(variant ? 2u : 1u) && !g->mine.layer);
    g->mine.random=0x13579;bool adamantite=false,copper=false;unsigned digs=0;
    const HmbtnFarmClock stopped=g->game.clock;const unsigned gold=g->game.economy.gold;
    CHECK(capture(s,&host,shots,variant ? "mine-other-entry" : "mine-entry",error,sizeof(error)));
    for(unsigned floor=0;floor<10;++floor) {
        CHECK(g->area==base+floor && g->mine.layer==floor);
        for(unsigned cell=11;cell<89;++cell) {
            if(cell%10==0 || cell%10==9 || !g->mine.active[cell] || g->mine.cells[floor][cell]) continue;
            if(floor && g->mine.exit_cell[floor-1]<100) {
                const int x=(int)(cell%10)-(int)(g->mine.exit_cell[floor-1]%10),y=(int)(cell/10)-(int)(g->mine.exit_cell[floor-1]/10);
                if(abs(x)<=2 && abs(y)<=2) continue;
            }
            if(!aim_cell(s,cell,error,sizeof(error))) continue;
            CHECK(dig(s,&host,cell,error,sizeof(error)));++digs;
            const unsigned item=g->game.inventory.held_item.id;
            if(item==0x62 && !adamantite) {
                CHECK(capture(s,&host,shots,"adamantite-found",error,sizeof(error)) && clear_hand(s,true,error,sizeof(error)));adamantite=true;
            } else if(item==0x7c && !copper) { CHECK(clear_hand(s,true,error,sizeof(error)));copper=true; }
            else CHECK(clear_hand(s,false,error,sizeof(error)));
            if(floor==9 || (g->mine.exit_cell[floor]<100 && (variant || adamantite || floor<3))) break;
        }
        if(floor==9) { CHECK(g->mine.exit_cell[9]==100);break; }
        CHECK(g->mine.exit_cell[floor]<100);
        if(!floor) {
            const unsigned exit=g->mine.exit_cell[0];CHECK(question(s,&host,shots,exit,false,error,sizeof(error)) && g->area==base);
            CHECK(!hmbtn_mine_check(&g->mine,&g->intro.player));
            CHECK(capture(s,&host,shots,variant ? "mine-other-dug" : "mine-dug",error,sizeof(error)));
        }
        if(floor==3) {
            char snapshot[1024];CHECK(snprintf(snapshot,sizeof(snapshot),"%s.underground",argv[3])<(int)sizeof(snapshot));
            CHECK(field_reload(&s,&host,argv[1],snapshot,error,sizeof(error)));g=hmbtn_scene_preview_session(s);
            CHECK(g->mine.layer==3 && g->area==base+3);
        }
        CHECK(question(s,&host,shots,g->mine.exit_cell[floor],true,error,sizeof(error)));
        CHECK(g->area==base+floor+1 && !(hmbtn_scene_preview_resources(s).scene->flags[0]&0x1000));
    }
    CHECK(!memcmp(&stopped,&g->game.clock,sizeof(stopped)) && digs>9 && g->farm_action.stamina<255);
    CHECK(variant ? copper : adamantite);
    if(!variant) {
        CHECK(g->game.economy.gold>gold && g->village.flags[0x62] && g->menu.stats.power_berries==1);
        const HmbtnMine before=g->mine;
        CHECK(travel(s,244,error,sizeof(error)) && g->mine.layer==9 && g->mine.kind==1);
        CHECK(travel(s,243,error,sizeof(error)) && !memcmp(g->mine.cells,before.cells,sizeof(before.cells)));
    }
    CHECK(capture(s,&host,shots,variant ? "mine-other-bottom" : "mine-bottom",error,sizeof(error)));
    CHECK(question(s,&host,shots,g->mine.exit_cell[8],true,error,sizeof(error)) && g->area==(variant ? 72u : 68u) && !g->mine.kind);
    CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)));
    /* The actual collected ore uses normal inventory cycling and shipping. */
    CHECK(step(s,HMBTN_PAD_R2,error,sizeof(error)) && settle(s,error,sizeof(error)));
    CHECK(g->game.inventory.held_item.id && ship(s,error,sizeof(error)) && idle(s,error,sizeof(error)));
    CHECK(travel(s,80,error,sizeof(error)) && field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);
    puts("Mine: real entry/ten source floor maps, hoe ANI/status, source RNG loot, item/coin/berry presentation, stairs cancel/accept/model, held clock, underground save/resume, ore shipping and exit passed.");return 0;
fail:fprintf(stderr,"Mine state: floor %u chance %u stamina %u fatigue %u\n",s ? hmbtn_scene_preview_session(s)->mine.layer : 99,
    s ? hmbtn_scene_preview_session(s)->mine.exit_chance : 0,s ? hmbtn_scene_preview_session(s)->farm_action.stamina : 0,s ? hmbtn_scene_preview_session(s)->farm_action.fatigue : 0);
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

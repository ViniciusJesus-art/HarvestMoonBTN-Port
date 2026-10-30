/* Installed ore acquisition -> Saibara script 354 -> original upgrades/ANI.
 * Fixtures: purse, energy and XP of non-hoe tools. Hoe XP is mined. */
#include "poultry_helpers.h"
#include "field_helpers.h"
#include "tool_upgrade.h"
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
    REQUIRE(!g->talk.active);
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
    memcpy(g->intro.camera,g->intro.player.position,sizeof(g->intro.camera));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->talk.active);return true;
}
static bool end_talk(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<3000 && (g->talk.active || g->feed_shop.departure || g->travel_tick || g->quick_ticks);++i)
        REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
    REQUIRE(!g->talk.active && !g->talk.script.boundary && !g->feed_shop.departure && !g->travel_tick);return true;
}
static bool upgraded_refill(HmbtnScenePreview *s,unsigned id,char *error,size_t size)
{
    int32_t pond[3]={0};bool found=false;
    for(unsigned ob=0;ob<(*hmbtn_scene_preview_resources(s).scene).terrain.object_count && !found;++ob) {
        HmbtnModelObject object;REQUIRE(hmbtn_model_object(&(*hmbtn_scene_preview_resources(s).scene).terrain,ob,&object));
        size_t off=12+object.primitives;
        for(unsigned k=0;k<object.primitive_count;++k) {
            HmbtnModelPrimitive primitive;REQUIRE(hmbtn_model_primitive(&(*hmbtn_scene_preview_resources(s).scene).terrain,off,&primitive));
            if(primitive.flags&3u) {
                memset(pond,0,sizeof(pond));
                for(unsigned v=0;v<primitive.count;++v) {
                    int32_t point[3];hmbtn_model_vertex(&(*hmbtn_scene_preview_resources(s).scene).terrain,&object,primitive.vertex[v],point);
                    for(unsigned axis=0;axis<3;++axis) pond[axis]+=point[axis];
                }
                for(unsigned axis=0;axis<3;++axis) pond[axis]/=(int32_t)primitive.count;
                HmbtnGroundHit hit;
                found=hmbtn_ground_find(&(*hmbtn_scene_preview_resources(s).scene).terrain,pond,0,200,&hit) && (hit.flags&3u);
                if(found) break;
            }
            off+=primitive.bytes;
        }
    }
    REQUIRE(found);memcpy(hmbtn_scene_preview_session(s)->intro.player.position,pond,sizeof(pond));
    hmbtn_scene_preview_session(s)->intro.player.position[0]-=224;hmbtn_scene_preview_session(s)->intro.player.facing=3;
    REQUIRE(tool(s,id,error,size) && hmbtn_scene_preview_session(s)->game.inventory.equipped.count==100);
    return true;
}

static bool equip(HmbtnScenePreview *s,unsigned id,char *error,size_t size)
{
    HmbtnInventory *h=&hmbtn_scene_preview_session(s)->game.inventory;
    if(h->equipped.id==id) return true;
    for(unsigned i=0;i<64;++i) if(h->toolbox[i].id==id) {
        REQUIRE(hmbtn_inventory_storage_swap(h,0,0,0,9+i%8,i/8));return h->equipped.id==id;
    }
    for(unsigned i=0;i<hmbtn_inventory_capacity(h);++i) if(h->bag[i].id==id) {
        REQUIRE(hmbtn_inventory_storage_swap(h,0,0,0,1+i,0));return h->equipped.id==id;
    }
    return false;
}
static bool hold_ore(HmbtnScenePreview *s,unsigned ore,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<12 && g->game.inventory.held_item.id!=ore;++i)
        REQUIRE(step(s,HMBTN_PAD_R2,error,size) && settle(s,error,size));
    REQUIRE(g->game.inventory.held_item.id==ore);return true;
}
static bool charged(HmbtnScenePreview *s,unsigned kind,unsigned tier,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const unsigned stamina=g->farm_action.stamina,xp=g->farm_action.experience[kind];
    REQUIRE(step(s,HMBTN_PAD_SQUARE,error,size) && g->farm_action.phase==HMBTN_FARM_TOOL_CHARGE);
    for(unsigned tick=0;tick<20*tier;++tick) REQUIRE(step(s,HMBTN_PAD_SQUARE,error,size));
    REQUIRE(g->farm_action.charge_frames/20==tier);
    REQUIRE(step(s,0,error,size) && g->farm_action.phase==HMBTN_FARM_TOOL_SWING);
    for(unsigned tick=0;tick<500 && g->farm_action.phase!=HMBTN_FARM_ACTION_FREE;++tick) REQUIRE(step(s,0,error,size));
    REQUIRE(g->farm_action.phase==HMBTN_FARM_ACTION_FREE && g->farm_action.stamina==stamina-1-tier
        && g->farm_action.experience[kind]>=xp);return true;
}
static bool use_tool(HmbtnScenePreview *s,unsigned kind,unsigned tier,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);REQUIRE(g->area==0);
    if(kind==4) REQUIRE(upgraded_refill(s,1+kind*5+tier,error,size));
    for(unsigned r=7;r<16;++r) for(unsigned c=16;c<26;++c) {
        const unsigned initial=kind==0 ? 0x40 : kind==4 ? 0x168 : 0x167;
        g->field.cells[r*45+c]=(HmbtnFarmTile){(uint16_t)initial,(uint16_t)(kind==4 ? 0x168 : 0x167),0,0,0};
    }
    const unsigned target=11*45+21;
    if(kind==2 || kind==3) {
        const unsigned item=kind==2 ? 0x3d : 0x3c,base=kind==2 ? 0x172 : 0x16f;
        g->field.cells[target].value=(uint16_t)item;g->field.cells[target+1].value=(uint16_t)base;
        g->field.cells[target+45].value=(uint16_t)(base+2);g->field.cells[target+46].value=(uint16_t)(base+1);
    }
    REQUIRE(hmbtn_scene_preview_load_area(s,0,error,size));cell(s,21,11);
    const unsigned wood=g->game.inventory.supplies[1];
    REQUIRE(charged(s,kind,tier,error,size));
    if(kind==2 || kind==3) {
        for(unsigned i=0;i<8 && g->field.cells[target].value!=0x167;++i) REQUIRE(charged(s,kind,tier,error,size));
        REQUIRE(g->field.cells[target].value==0x167 && g->field.cells[target+46].value==0x167);
        if(kind==2) REQUIRE(g->game.inventory.supplies[1]==wood+6);
    } else {
        unsigned changed=0;
        const unsigned initial=kind==0 ? 0x40 : kind==4 ? 0x168 : 0x167;
        for(unsigned r=7;r<16;++r) for(unsigned c=16;c<26;++c) changed+=g->field.cells[r*45+c].value!=initial;
        const unsigned counts[5]={25,2,1,1,6};REQUIRE(changed==counts[kind]);
        if(kind==4) REQUIRE(g->game.inventory.equipped.count==94);
    }
    return true;
}
int main(int argc,char **argv)
{
    if(argc<3 || argc>4) return 2;const char *shots=argc==4 ? argv[3] : NULL;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock clock={0,0,3,3,12,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],72,&clock,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->game.inventory.rucksack_level=2;
    g->game.economy.gold=50000;g->farm_action.max_stamina=g->farm_action.stamina=255;
    g->village.flags[251+288]=g->village.flags[251+140]=true;
    CHECK(hmbtn_scene_preview_set_weather(s,0,0) && travel(s,245,error,sizeof(error)));g->mine.random=0x13579;
    unsigned found[5]={0};bool enough=false;
    for(unsigned floor=0;floor<10 && !enough;++floor) {
        g->farm_action.stamina=255;
        for(unsigned cell=11;cell<89 && !enough;++cell) {
            if(cell%10==0 || cell%10==9 || !g->mine.active[cell] || g->mine.cells[floor][cell]) continue;
            if(floor && cell==g->mine.exit_cell[floor-1]) continue;
            if(!aim_cell(s,cell,error,sizeof(error))) continue;
            CHECK(dig(s,&host,cell,error,sizeof(error)));
            const unsigned level=hmbtn_tool_ore_level(g->game.inventory.held_item.id);
            const bool keep=level && found[level]<(level==4 ? 2u : 1u);
            if(keep) ++found[level];CHECK(clear_hand(s,keep,error,sizeof(error)));
            enough=found[1] && found[2] && found[3] && found[4]==2;
        }
        if(!enough) { CHECK(g->mine.exit_cell[floor]<100);CHECK(question(s,&host,NULL,g->mine.exit_cell[floor],true,error,sizeof(error))); }
    }
    CHECK(enough && g->farm_action.experience[1]>=6000);
    const unsigned mined_xp=g->farm_action.experience[1],starting_gold=g->game.economy.gold;
    for(unsigned kind=0;kind<5;++kind) if(kind!=1) g->farm_action.experience[kind]=65535;
    CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)));
    const unsigned kinds[]={1,4,2,3,0},tiers[]={1,2,3,4,4},ores[]={0x7c,0x7b,0x7a,0x60,0x60};
    for(unsigned i=0;i<5;++i) {
        const unsigned kind=kinds[i],tier=tiers[i],item=1+kind*5+tier;
        CHECK(equip(s,1+kind*5,error,sizeof(error)) && hold_ore(s,ores[i],error,sizeof(error)));
        CHECK(smith(s,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_MAKER_CONFIRM,error,sizeof(error)) && g->feed_shop.upgrade);
        CHECK(g->feed_shop.request==0x99+tier && capture(s,&host,shots,"upgrade-first-confirm",error,sizeof(error)));
        const unsigned gold=g->game.economy.gold;
        CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_MAKER_AGAIN,error,sizeof(error)));
        CHECK(capture(s,&host,shots,"upgrade-second-confirm",error,sizeof(error)));
        CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_OFF,error,sizeof(error)) && end_talk(s,error,sizeof(error)));
        CHECK(!g->makers.pending && g->game.economy.gold==gold && g->game.inventory.held_item.id==ores[i] && g->game.inventory.equipped.id==1+kind*5);
        CHECK(smith(s,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_MAKER_CONFIRM,error,sizeof(error)));
        CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_MAKER_AGAIN,error,sizeof(error)));
        CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_OFF,error,sizeof(error)) && end_talk(s,error,sizeof(error)));
        CHECK(g->area==32 && g->makers.pending==item && g->makers.mornings_left==3 && !g->game.inventory.equipped.id && !g->game.inventory.held_item.id);
        CHECK(g->game.economy.gold==gold-hmbtn_tool_upgrade_price(tier));
        CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)));
        char pending[1024];CHECK(snprintf(pending,sizeof(pending),"%s.pending",argv[2])<(int)sizeof(pending));
        CHECK(field_reload(&s,&host,argv[1],pending,error,sizeof(error)));g=hmbtn_scene_preview_session(s);
        for(unsigned day=0;day<3;++day) {
            g->game.weather.tomorrow=HMBTN_WEATHER_SUNNY;
            CHECK(sleep_day(s,error,sizeof(error)) && g->makers.mornings_left==2-day && g->makers.pending==item);
        }
        if(!i) {
            HmbtnStoredTool stored[8];memcpy(stored,g->game.inventory.bag,sizeof(stored));
            for(unsigned slot=0;slot<8;++slot) g->game.inventory.bag[slot]=(HmbtnStoredTool){28,0,1};
            g->game.inventory.equipped=(HmbtnStoredTool){28,0,1};
            CHECK(smith(s,error,sizeof(error)) && end_talk(s,error,sizeof(error)));
            CHECK(g->makers.pending==item && !g->makers.mornings_left && !hmbtn_inventory_count(&g->game.inventory,(int)item,0,1));
            memcpy(g->game.inventory.bag,stored,sizeof(stored));g->game.inventory.equipped=(HmbtnStoredTool){0};
        }
        CHECK(smith(s,error,sizeof(error)) && end_talk(s,error,sizeof(error)));
        CHECK(!g->makers.pending && g->game.inventory.equipped.id==item && g->game.inventory.equipped.modifier==tier && g->game.inventory.equipped.count==1);
        CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)) && use_tool(s,kind,tier,error,sizeof(error)));
        memcpy(g->intro.camera,g->intro.player.position,sizeof(g->intro.camera));
        char name[64];snprintf(name,sizeof(name),"upgrade-kind-%u",kind);CHECK(capture(s,&host,shots,name,error,sizeof(error)));
        CHECK(field_reload(&s,&host,argv[1],argv[2],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    }
    CHECK(g->farm_action.experience[1]>=mined_xp && g->game.economy.gold==starting_gold-16000);
    puts("Mined ores/hoe XP -> five original tool orders/deliveries -> charged field actions and resume passed");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

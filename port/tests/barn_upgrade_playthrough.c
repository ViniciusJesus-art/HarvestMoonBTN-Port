/* Original Gotz dialogue, real overnight work, expanded troughs and saves.
 * Prior house/coop, money/materials and preserved items are explicit fixtures;
 * this does not pretend cows/sheep or their acquisition are already ported. */
#include "poultry_helpers.h"
#include "field_helpers.h"
static bool gotz(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);HmbtnVillager *p=&g->village.people[24];
    REQUIRE(p->actor.visible && (p->schedule[p->step][1]>>8)==6);
    memcpy(g->intro.player.position,p->actor.position,sizeof(g->intro.player.position));g->intro.player.position[2]+=300;
    g->intro.player.facing=0;g->control.mode=0;
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && shop_wait(s,HMBTN_SHOP_SELECT,error,size));
    REQUIRE(g->talk.active && g->talk.target==24 && g->feed_shop.carpenter);return true;
}
static bool end_talk(HmbtnScenePreview *s,char *error,size_t size)
{
    REQUIRE(shop_wait(s,HMBTN_SHOP_OFF,error,size));HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<1000 && g->talk.active;++i) REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
    REQUIRE(!g->talk.active && !g->talk.script.boundary);return true;
}
static bool capture_stable(HmbtnScenePreview *s,HmbtnHost *host,const char *shots,const char *name,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const HmbtnGameState game=g->game;
    const HmbtnConstruction work=g->construction;const HmbtnFarmFeed feed_state=g->feed;
    REQUIRE(capture(s,host,shots,name,error,size));
    REQUIRE(!memcmp(&game,&g->game,sizeof(game)) && !memcmp(&work,&g->construction,sizeof(work))
        && !memcmp(&feed_state,&g->feed,sizeof(feed_state)));return true;
}
int main(int argc,char **argv)
{
    if(argc<4 || argc>5) return 2;const char *shots=argc==5 ? argv[4] : NULL;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock clock={0,0,3,3,12,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],212,&clock,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    g->poultry.upgraded=true;g->construction.house_level=1;
    g->game.economy.gold=8000;g->game.inventory.supplies[1]=510;g->game.inventory.supplies[0]=30;
    g->game.inventory.cabinet[63]=(HmbtnStoredTool){0x7b,0,2};g->game.inventory.icebox[63]=(HmbtnStoredTool){0x43,8,3};
    g->field.cells[460]=(HmbtnFarmTile){.value=0x24,.ground=0x168,.kind=1};
    g->village.flags[251+288]=g->village.flags[251+140]=true;
    /* The old section and its actual zones, rather than a hardcoded PC capacity. */
    CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)) && travel(s,96,error,sizeof(error)));
    const uint32_t before=hmbtn_farm_feed_bins(hmbtn_scene_preview_resources(s).section,0x2000,true);
    printf("Barn before: trough mask=%x\n",before);
    CHECK((before&0x3ff)==0x3ff && !(before&(1u<<19)));
    CHECK(capture_stable(s,&host,shots,"barn-before",error,sizeof(error)));
    CHECK(travel(s,0,error,sizeof(error)) && hmbtn_scene_preview_load_area(s,212,error,sizeof(error)));
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(gotz(s,error,sizeof(error)) && hmbtn_carpenter_shop_rows(&g->feed_shop)==2);
    CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && capture_stable(s,&host,shots,"gotz-barn-menu",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_TRIANGLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_INFO,error,sizeof(error)) && g->feed_shop.request==0x136);
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_INFO,error,sizeof(error)) && g->feed_shop.request==0x13e);
    CHECK(capture_stable(s,&host,shots,"gotz-barn-offer",error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_BUILD_CONFIRM,error,sizeof(error)) && step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)) && !g->construction.mornings_left
        && g->game.economy.gold==8000 && g->game.inventory.supplies[1]==510);
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_BUILD_CONFIRM,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && end_talk(s,error,sizeof(error)));
    CHECK(g->construction.pending==HMBTN_BUILD_BARN && g->construction.mornings_left==4
        && !g->construction.barn_level && g->game.economy.gold==1200 && g->game.inventory.supplies[1]==10);
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(gotz(s,error,sizeof(error)) && step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && g->feed_shop.request==0x144);
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)) && step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && end_talk(s,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)));
    for(unsigned day=0;day<4;++day) {
        g->game.weather.tomorrow=HMBTN_WEATHER_SUNNY;CHECK(sleep_day(s,error,sizeof(error)));
        CHECK(g->construction.mornings_left==3-day && g->construction.barn_level==(day==3));
        if(day==1) { CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s); }
    }
    CHECK(g->construction.pending==HMBTN_BUILD_NONE && hmbtn_village_flag(&g->village,0x3c)
        && hmbtn_farm_menu_building_score(&g->menu)==60 && g->field.cells[460].value==0x24);
    CHECK(hmbtn_read_le16(hmbtn_scene_preview_resources(s).section+4)==12);
    CHECK(capture_stable(s,&host,shots,"barn-expanded-farm",error,sizeof(error)));
    CHECK(travel(s,96,error,sizeof(error)) && hmbtn_read_le16(hmbtn_scene_preview_resources(s).section+4)==100);
    const uint32_t after=hmbtn_farm_feed_bins(hmbtn_scene_preview_resources(s).section,0x2000,true);
    printf("Barn after: trough mask=%x\n",after);
    CHECK((after&0xfffff)==0xfffff && (after&before)==before);
    /* New row: withdrawal, original feed action/art, held-item door transitions. */
    for(unsigned bin=10;bin<20;++bin) {
        CHECK(feed(s,9,0,error,sizeof(error)) && feed(s,41,bin,error,sizeof(error)));
        CHECK(g->feed.barn[bin] && !g->game.inventory.held_item.id);
    }
    /* The first free tick resumes camera tracking after the feed animation. */
    CHECK(step(s,0,error,sizeof(error)));
    CHECK(g->game.inventory.supplies[0]==20 && capture_stable(s,&host,shots,"barn-expanded-feed",error,sizeof(error)));
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(g->construction.barn_level==1 && g->feed.barn[10] && g->feed.barn[19]);
    CHECK(feed(s,9,0,error,sizeof(error)) && g->game.inventory.held_item.id==0x68);
    CHECK(travel(s,0,error,sizeof(error)) && travel(s,96,error,sizeof(error)) && g->game.inventory.held_item.id==0x68);
    CHECK(feed(s,9,0,error,sizeof(error)) && !g->game.inventory.held_item.id && g->game.inventory.supplies[0]==20);
    CHECK(travel(s,0,error,sizeof(error)) && sleep_day(s,error,sizeof(error)) && travel(s,96,error,sizeof(error)));
    CHECK(!g->feed.barn[10] && !g->feed.barn[19] && g->game.inventory.supplies[0]==20 && g->construction.barn_level==1);
    for(unsigned season=0;season<4;++season) {
        g->game.clock.season=season;g->field.season=season;g->daily_date=hmbtn_session_date(&g->game.clock);
        CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)) && hmbtn_read_le16(hmbtn_scene_preview_resources(s).section+4)==12+season);
        CHECK(travel(s,96,error,sizeof(error)) && hmbtn_read_le16(hmbtn_scene_preview_resources(s).section+4)==100+season);
        CHECK(hmbtn_farm_feed_bins(hmbtn_scene_preview_resources(s).section,0x2000,true)==after);
        CHECK(capture_stable(s,&host,shots,season==0 ? "barn-spring" : season==1 ? "barn-summer" : season==2 ? "barn-fall" : "barn-winter",error,sizeof(error)));
    }
    CHECK(travel(s,0,error,sizeof(error)) && travel(s,80,error,sizeof(error)));
    CHECK(hmbtn_read_le16(hmbtn_scene_preview_resources(s).section+4)==87
        && g->game.inventory.cabinet[63].count==2 && g->game.inventory.icebox[63].modifier==8);
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);
    puts("Barn construction/feed: real Gotz/cancel/busy, four nights/reload, original expanded sections, ten new troughs, held-item doors, daily clearing and all seasons passed.");return 0;
fail:hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

/* Imported Gotz 374 -> shared shop -> real sleep -> original upgraded sections.
 * Fixtures supply starting money/materials only, never spawn Gotz or call order. */
#include "poultry_helpers.h"
#include "field_helpers.h"
static bool gotz(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);HmbtnVillager *p=&g->village.people[24];
    if(!p->actor.visible || (p->schedule[p->step][1]>>8)!=6) {
        fprintf(stderr,"Gotz schedule location=%u step=%u action=%u hour=%u weekday=%u\n",p->location,p->step,p->schedule[p->step][1]>>8,g->game.clock.hour,g->game.clock.weekday);
    }
    REQUIRE(p->actor.visible && (p->schedule[p->step][1]>>8)==6);
    memcpy(g->intro.player.position,p->actor.position,sizeof(g->intro.player.position));g->intro.player.position[2]+=300;
    g->intro.player.facing=0;g->control.mode=0;
    REQUIRE(hmbtn_village_target(&g->village,&g->intro.player)==24);
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->talk.active && g->talk.target==24);
    REQUIRE(shop_wait(s,HMBTN_SHOP_SELECT,error,size));
    REQUIRE(g->feed_shop.carpenter && g->feed_shop.merchant==24 && g->talk.active);return true;
}
static bool end_talk(HmbtnScenePreview *s,char *error,size_t size)
{
    REQUIRE(shop_wait(s,HMBTN_SHOP_OFF,error,size));
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<1000 && g->talk.active;++i) REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
    REQUIRE(!g->talk.active && !g->talk.script.boundary);return true;
}
int main(int argc,char **argv)
{
    if(argc<4 || argc>5) return 2;
    const char *shots=argc==5 ? argv[4] : NULL;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock clock={0,0,5,3,12,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],212,&clock,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->game.economy.gold=10000;g->game.inventory.supplies[1]=400;g->game.inventory.supplies[2]=1;
    g->village.flags[251+288]=g->village.flags[251+140]=true;
    CHECK(gotz(s,error,sizeof(error)) && capture(s,&host,shots,"gotz-menu",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_TRIANGLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_INFO,error,sizeof(error)));
    CHECK(g->feed_shop.request==0x133 && capture(s,&host,shots,"gotz-description",error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_COUNT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_UP,error,sizeof(error)) && step(s,HMBTN_PAD_UP,error,sizeof(error)) && step(s,HMBTN_PAD_LEFT,error,sizeof(error)));
    CHECK(g->feed_shop.quantity==20 && capture(s,&host,shots,"gotz-quantity",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && end_talk(s,error,sizeof(error)));
    CHECK(g->game.inventory.supplies[1]==420 && g->game.economy.gold==9000);
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(gotz(s,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_INFO,error,sizeof(error)) && g->feed_shop.request==0x13c);
    CHECK(capture(s,&host,shots,"gotz-coop-offer",error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_BUILD_CONFIRM,error,sizeof(error)) && g->feed_shop.request==0x5f);
    CHECK(capture(s,&host,shots,"gotz-coop-confirm",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && end_talk(s,error,sizeof(error)));
    CHECK(g->construction.mornings_left==4 && g->game.economy.gold==4000 && !g->game.inventory.supplies[1] && !g->poultry.upgraded);
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(g->construction.mornings_left==4 && gotz(s,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && g->feed_shop.request==0x144);
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && end_talk(s,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)));
    for(unsigned day=0;day<4;++day) {
        g->game.weather.tomorrow=HMBTN_WEATHER_SUNNY;
        CHECK(sleep_day(s,error,sizeof(error)));
        CHECK(g->construction.mornings_left==3-day && g->poultry.upgraded==(day==3));
        if(day==1) { CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s); }
    }
    CHECK(g->game.economy.gold==4000 && !g->game.inventory.supplies[1]);
    CHECK(hmbtn_read_le16(hmbtn_scene_preview_resources(s).section+4)==4);
    CHECK(capture(s,&host,shots,"coop-expanded-farm",error,sizeof(error)));
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(hmbtn_read_le16(hmbtn_scene_preview_resources(s).section+4)==4 && g->poultry.upgraded);
    CHECK(travel(s,112,error,sizeof(error)) && hmbtn_read_le16(hmbtn_scene_preview_resources(s).section+4)==116);
    unsigned bins=0;const uint8_t *incubator=NULL;
    for(unsigned i=0;i<50;++i) {
        const uint8_t *z=hmbtn_scene_preview_resources(s).section+16+i*44;
        if(hmbtn_read_le16(z)!=0x4000) continue;
        if(hmbtn_read_le16(z+2)==40) bins|=1u<<hmbtn_read_le16(z+4);
        if(hmbtn_read_le16(z+2)==34 && hmbtn_read_le16(z+12)==1) incubator=z;
    }
    CHECK(bins==0x3ff); /* All ten feed positions come from upgraded collision/zones. */
    CHECK(incubator);
    CHECK(feed(s,26,0,error,sizeof(error)) && feed(s,40,9,error,sizeof(error)) && g->feed.coop[9]);
    /* Explicit egg fixture exercises the new bed through its original zone. */
    g->game.inventory.held_item=(HmbtnStoredTool){0x122,0,1};at(s,incubator);
    const unsigned face=hmbtn_read_le16(incubator+8);g->intro.player.facing=face;
    g->intro.player.position[face&1u ? 0 : 2]+=face<2 ? 224 : -224;
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && g->feed.phase);
    for(unsigned i=0;i<100 && g->feed.phase;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(!g->feed.phase && !g->game.inventory.held_item.id && g->poultry.hens[0].incubator==2);
    CHECK(capture(s,&host,shots,"coop-expanded-interior",error,sizeof(error)));
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(g->poultry.upgraded && !g->construction.mornings_left && g->feed.coop[9] && g->area==112 && g->poultry.hens[0].incubator==2);
    CHECK(travel(s,0,error,sizeof(error)) && travel(s,80,error,sizeof(error)));
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);
    puts("Gotz: original conversation, lumber/order, shared resources, busy reply, four real nights, save/reload and upgraded farm/coop with ten feed bins passed.");return 0;
fail:hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

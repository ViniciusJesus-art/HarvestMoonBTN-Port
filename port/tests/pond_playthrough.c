/* Rod, fish and food are obtained by gameplay; only starting money is a fixture. */
#include "pond_helpers.h"
int main(int argc,char **argv)
{
    if(argc<3 || argc>4) return 2;const char *shots=argc==4 ? argv[3] : NULL;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock date={0,0,5,5,7,0,0};s=hmbtn_scene_area_create_at(host.renderer,argv[1],56,&date,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->game.economy.gold=2000;g->first_visit_started=true;
    g->village.flags[251+288]=g->village.flags[251+140]=true;
    const HmbtnIntroActor *greg=&g->village.people[32].actor;CHECK(greg->visible);
    memcpy(g->intro.player.position,greg->position,sizeof(greg->position));g->intro.player.position[2]+=300;g->intro.player.facing=0;
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && g->talk.active);
    for(unsigned i=0;i<3000 && (g->talk.active || g->quick_ticks);++i) CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(!g->talk.active && g->game.inventory.equipped.id==56 && g->village.flags[0xe7]);
    int32_t shore[3];unsigned face;CHECK(cast_point(s,shore,&face));unsigned caught=0;
    for(unsigned cast=0;cast<24 && !caught;++cast) {
        memcpy(g->intro.player.position,shore,sizeof(shore));g->intro.player.facing=face;
        CHECK(held(s,HMBTN_PAD_SQUARE,HMBTN_PAD_SQUARE,error,sizeof(error)));
        for(unsigned t=0;t<5000 && g->fishing.phase!=HMBTN_FISHING_HOOKED;++t) CHECK(held(s,0,HMBTN_PAD_SQUARE,error,sizeof(error)));
        CHECK(g->fishing.phase==HMBTN_FISHING_HOOKED && held(s,0,0,error,sizeof(error)));
        for(unsigned t=0;t<400 && g->fishing.phase;++t) CHECK(step(s,0,error,sizeof(error)));
        CHECK(!g->fishing.phase);const unsigned item=g->game.inventory.held_item.id;
        if(item==163) { caught=item;break; }
        if(item) CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && wait_items(s,error,sizeof(error)));
    }
    CHECK(caught && capture(s,&host,shots,"pond-caught",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_R2,error,sizeof(error)) && wait_items(s,error,sizeof(error)) && !g->game.inventory.held_item.id);
    /* Advance to a real open supermarket time; no clock/date field is replaced. */
    if(g->game.clock.hour>=17) hmbtn_session_advance_minutes(g,(24-g->game.clock.hour)*60-g->game.clock.minute+600);
    if(g->game.clock.hour<9) hmbtn_session_advance_minutes(g,540-g->game.clock.hour*60-g->game.clock.minute);
    while(g->game.clock.weekday==0 || g->game.clock.weekday==2) hmbtn_session_advance_minutes(g,1440);
    CHECK(hmbtn_scene_preview_load_area(s,180,error,sizeof(error)));for(unsigned t=0;t<110;++t) CHECK(step(s,0,error,sizeof(error)));
    CHECK(counter(s,error,sizeof(error)) && step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && close_shop(s,error,sizeof(error)) && g->game.economy.gold==2000);
    CHECK(counter(s,error,sizeof(error)));g->game.economy.gold=19;
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && g->feed_shop.request==0x4f);
    CHECK(shop_wait(s,HMBTN_SHOP_COUNT,error,sizeof(error)) && !g->game.inventory.supplies[3] && g->game.economy.gold==19);g->game.economy.gold=2000;
    CHECK(step(s,HMBTN_PAD_TRIANGLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_INFO,error,sizeof(error)) && g->feed_shop.request==0x25b && shop_wait(s,HMBTN_SHOP_COUNT,error,sizeof(error)));
    for(unsigned i=0;i<4;++i) CHECK(step(s,HMBTN_PAD_UP,error,sizeof(error)));
    CHECK(g->feed_shop.quantity==41 && capture(s,&host,shots,"pond-food-purchase",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && g->feed_shop.request==0xd3);
    CHECK(g->game.economy.gold==2000 && !g->game.inventory.supplies[3]);
    CHECK(close_shop(s,error,sizeof(error)) && g->game.economy.gold==1180 && g->game.inventory.supplies[3]==41);
    CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)));for(unsigned t=0;t<110;++t) CHECK(step(s,0,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_R2,error,sizeof(error)) && wait_items(s,error,sizeof(error)) && g->game.inventory.held_item.id==caught);
    CHECK(pond_throw(s,&host,shots,error,sizeof(error)) && !g->pond.fed && hmbtn_pond_count(&g->pond)==1);
    /* An unfed night preserves age. */
    const unsigned first_age=g->pond.fish[0].days;CHECK(sleep_day(s,error,sizeof(error)) && g->pond.fish[0].days==first_age);
    CHECK(object(s,10,error,sizeof(error)) && g->game.inventory.held_item.id==132 && g->game.inventory.supplies[3]==40);
    CHECK(pond_throw(s,&host,shots,error,sizeof(error)) && checkpoint(&s,&host,argv[1],argv[2],".fed",error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    unsigned fed_days=0;
    do {
        CHECK(sleep_day(s,error,sizeof(error)) && !g->pond.fed);++fed_days;
        const uint32_t random=g->pond.random;const unsigned age=g->pond.fish[0].days;
        hmbtn_session_clock_events(g,&g->game.clock,HMBTN_CLOCK_DAY);CHECK(g->pond.fish[0].days==age && g->pond.random==random);
        if(fed_days==10 && first_age==0) CHECK(g->pond.fish[0].item==162 && age==10);
        if(fed_days==20 && first_age==0) CHECK(g->pond.fish[0].item==161 && age==20);
        if(hmbtn_pond_count(&g->pond)>1) break;
        CHECK(fed_days<40 && object(s,10,error,sizeof(error)) && pond_throw(s,&host,shots,error,sizeof(error)));
    } while(fed_days<40);
    CHECK(hmbtn_pond_count(&g->pond)>1 && g->pond.fish[0].item==161 && g->game.inventory.supplies[3]==41-fed_days);
    CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)) && g->menu.mode==HMBTN_MENU_STATUS);
    for(unsigned t=0;t<25;++t) CHECK(step(s,0,error,sizeof(error)));
    CHECK(g->menu.phase==3 && capture(s,&host,shots,"pond-status",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)));
    for(unsigned t=0;t<25;++t) CHECK(step(s,0,error,sizeof(error)));
    CHECK(g->menu.mode==HMBTN_MENU_NONE);
    CHECK(checkpoint(&s,&host,argv[1],argv[2],".grown",error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    const unsigned count=hmbtn_pond_count(&g->pond);CHECK(object(s,45,error,sizeof(error)) && g->game.inventory.held_item.id==161 && hmbtn_pond_count(&g->pond)==count-1);
    CHECK(capture(s,&host,shots,"pond-withdrawn",error,sizeof(error)) && ship(s,error,sizeof(error)));
    const unsigned now=g->game.clock.hour*60+g->game.clock.minute,gold=g->game.economy.gold;CHECK(now<1078);
    hmbtn_session_advance_minutes(g,1078-now);CHECK(g->game.economy.gold==gold+200 && g->game.economy.shipping.fish_shipped==1);
    CHECK(checkpoint(&s,&host,argv[1],argv[2],"",error,sizeof(error)));
    printf("Pond: Greg rod -> actual fish %u -> 41 bought food -> %u fed mornings/growth/spawn -> withdrawal/shipping/payment and stable resumes passed\n",caught,fed_days);
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

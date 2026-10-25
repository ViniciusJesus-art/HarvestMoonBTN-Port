#include "poultry_helpers.h"
int main(int argc,char **argv)
{
    if(argc<2 || argc>3) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};const char *shots=argc==3 ? argv[2] : NULL;
    CHECK(hmbtn_host_open(&host));s=hmbtn_scene_area_create(host.renderer,argv[1],0,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    s->session->game.clock=(HmbtnFarmClock){0,0,2,2,6,0,0};
    /* Declared animal fixture; money and merchandise must come from gameplay.
     * High affection keeps the adult healthy while the player earns the cost. */
    const uint16_t name[9]={0,1,2,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF};
    CHECK(hmbtn_poultry_buy(&s->session->poultry,name,&s->session->game.clock));s->session->poultry.hens[0].affection=255;
    CHECK(s->session->game.economy.gold==500);
    for(unsigned trip=0;trip<4;++trip) {
        CHECK(travel(s,72,error,sizeof(error)));
        for(unsigned i=146;i<=148;++i) {
            const HmbtnGatherRecord *r=hmbtn_farm_gather_record(i);
            memcpy(s->intro.player.position,r->position,sizeof(r->position));s->intro.player.position[2]+=224;
            s->intro.player.facing=0;s->control.mode=0;
            CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && s->house.inventory->held_item.id==r->item);
            CHECK(idle(s,error,sizeof(error)));
            if(i<148) { CHECK(step(s,HMBTN_PAD_R2,error,sizeof(error)));CHECK(idle(s,error,sizeof(error))); }
        }
        CHECK(travel(s,0,error,sizeof(error)));
        for(unsigned i=0;i<3;++i) {
            if(i) { CHECK(step(s,HMBTN_PAD_R2,error,sizeof(error)));CHECK(idle(s,error,sizeof(error))); }
            CHECK(ship(s,error,sizeof(error)));
        }
        s->session->game.clock.hour=16;CHECK(sleep_day(s,error,sizeof(error)));
    }
    CHECK(s->session->game.economy.gold==1100 && s->session->game.economy.ledger[0][0]==600 && !s->session->game.economy.ledger[0][1]);
    /* Move an existing starting tool out of its box so the shop uses the bag. */
    CHECK(!s->house.inventory->equipped.id && s->house.inventory->toolbox[0].id==1);
    s->house.inventory->equipped=s->house.inventory->toolbox[0];s->house.inventory->toolbox[0]=(HmbtnStoredTool){0};
    s->session->game.clock.hour=12;
    CHECK(travel(s,32,error,sizeof(error)) && travel(s,24,error,sizeof(error)) && travel(s,128,error,sizeof(error)));
    CHECK(merchant(s,error,sizeof(error)));CHECK(step(s,HMBTN_PAD_UP,error,sizeof(error)) && s->feed_shop.selection==3);
    CHECK(capture(s,&host,shots,"medicine-shop",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_OFF,error,sizeof(error)));
    CHECK(s->session->game.economy.gold==1100 && !s->house.inventory->bag[0].id);
    CHECK(merchant(s,error,sizeof(error)));CHECK(step(s,HMBTN_PAD_UP,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(s->session->game.economy.gold==100 && s->session->game.economy.ledger[0][1]==1000
        && s->house.inventory->bag[0].id==0x1E && s->house.inventory->bag[0].count==1 && !s->feed_shop.medicine_available);
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_COUNT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_RIGHT,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)) && s->session->game.economy.gold==80 && s->house.inventory->supplies[2]==2);
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_OFF,error,sizeof(error)));
    CHECK(travel(s,24,error,sizeof(error)) && travel(s,32,error,sizeof(error)) && travel(s,0,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_R1,error,sizeof(error)));CHECK(idle(s,error,sizeof(error)));
    CHECK(s->house.inventory->equipped.id==0x1E && s->house.inventory->equipped.count==1);
    CHECK(travel(s,112,error,sizeof(error)));
    /* Declared health fixture avoids waiting for random illness. */
    HmbtnHen *hen=&s->session->poultry.hens[0];hen->days_sick=2;
    CHECK(step(s,0,error,sizeof(error)) && hen->actor.sprite.animation==18);
    memcpy(s->intro.player.position,hen->actor.position,sizeof(hen->actor.position));
    s->intro.player.position[2]+=300;s->intro.player.facing=0;s->control.mode=0;
    memcpy(s->intro.camera,s->intro.player.position,sizeof(s->intro.camera));
    CHECK(capture(s,&host,shots,"medicine-sick",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_SQUARE,error,sizeof(error)) && s->farm_action.phase==HMBTN_FARM_MEDICINE);
    CHECK(s->intro.action_active && s->farm_action.target==0 && hen->days_sick==2 && s->house.inventory->equipped.count==1);
    const HmbtnIntroActor before=s->intro.player;const unsigned stamina=s->farm_action.stamina;
    CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)) && s->menu.mode==HMBTN_MENU_NONE);
    CHECK(step(s,HMBTN_PAD_CROSS|HMBTN_PAD_R1|HMBTN_PAD_UP,error,sizeof(error)) && !s->carry.phase && !s->quick_ticks);
    CHECK(!memcmp(before.position,s->intro.player.position,sizeof(before.position)) && hen->days_sick==2);
    for(unsigned i=0;i<5;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(capture(s,&host,shots,"medicine-treatment",error,sizeof(error)));
    for(unsigned i=0;i<200 && s->farm_action.phase!=HMBTN_FARM_ACTION_FREE;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(s->farm_action.phase==HMBTN_FARM_ACTION_FREE && !s->intro.action_active && !s->house.inventory->equipped.id
        && !hen->days_sick && hen->praise && s->farm_action.stamina==stamina);
    CHECK(capture(s,&host,shots,"medicine-cured",error,sizeof(error)));
    for(unsigned night=0;night<2;++night) {
        CHECK(feed(s,26,0,error,sizeof(error)) && feed(s,40,0,error,sizeof(error)));
        CHECK(travel(s,0,error,sizeof(error)) && sleep_day(s,error,sizeof(error)));
        CHECK(!hen->days_sick && !hen->dead);CHECK(travel(s,112,error,sizeof(error)));
    }
    CHECK(s->session->poultry.eggs[0].item && s->session->game.economy.gold==80 && s->session->game.economy.ledger[0][1]==1020);
    CHECK(capture(s,&host,shots,"medicine-production-restored",error,sizeof(error)));
    puts("Medicine: 500G + 12 forage shipments -> 1000G medicine in tool bag -> equip -> original treatment/cure -> two feeding nights and egg production passed.");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

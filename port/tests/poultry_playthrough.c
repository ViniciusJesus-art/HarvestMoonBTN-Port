#include "poultry_helpers.h"
int main(int argc,char **argv)
{
    if(argc<2 || argc>3) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};const char *shots=argc==3 ? argv[2] : NULL;
    CHECK(hmbtn_host_open(&host));s=hmbtn_scene_area_create(host.renderer,argv[1],0,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    hmbtn_scene_preview_session(s)->game.clock=(HmbtnFarmClock){0,0,2,2,6,0,0};
    CHECK(hmbtn_scene_preview_session(s)->game.economy.gold==500 && !hmbtn_poultry_count(&hmbtn_scene_preview_session(s)->poultry));
    /* Seven real daily trips, three original spring bamboo records per trip. */
    for(unsigned trip=0;trip<7;++trip) {
        const unsigned pending=hmbtn_scene_preview_session(s)->game.economy.shipping.pending_value;
        CHECK(travel(s,72,error,sizeof(error)));
        CHECK(hmbtn_read_le16(hmbtn_scene_preview_resources(s).section+2)==12);
        for(unsigned i=146;i<=148;++i) {
            const HmbtnGatherRecord *r=hmbtn_farm_gather_record(i);
            memcpy(hmbtn_scene_preview_session(s)->intro.player.position,r->position,sizeof(r->position));hmbtn_scene_preview_session(s)->intro.player.position[2]+=224;
            hmbtn_scene_preview_session(s)->intro.player.facing=0;hmbtn_scene_preview_session(s)->control.mode=0;
            CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && hmbtn_scene_preview_session(s)->house.inventory->held_item.id==r->item && hmbtn_scene_preview_session(s)->gather.taken[i]);
            CHECK(idle(s,error,sizeof(error)));
            if(i<148) { CHECK(step(s,HMBTN_PAD_R2,error,sizeof(error)));CHECK(idle(s,error,sizeof(error)));CHECK(!hmbtn_scene_preview_session(s)->house.inventory->held_item.id); }
        }
        CHECK(capture(s,&host,shots,"forage-collected",error,sizeof(error)));
        CHECK(travel(s,0,error,sizeof(error)));
        for(unsigned i=0;i<3;++i) {
            if(i) { CHECK(step(s,HMBTN_PAD_R2,error,sizeof(error)));CHECK(idle(s,error,sizeof(error))); }
            CHECK(ship(s,error,sizeof(error)));
        }
        CHECK(!hmbtn_scene_preview_session(s)->carry.discarded && hmbtn_scene_preview_session(s)->game.economy.shipping.pending_value==pending+150);
        hmbtn_scene_preview_session(s)->game.clock.hour=16;hmbtn_scene_preview_session(s)->game.clock.minute=0;
        CHECK(sleep_day(s,error,sizeof(error)));
        CHECK(hmbtn_scene_preview_session(s)->game.economy.gold+hmbtn_scene_preview_session(s)->game.economy.shipping.pending_value==500+150*(trip+1));
    }
    CHECK(hmbtn_scene_preview_session(s)->game.economy.gold==1550 && hmbtn_scene_preview_session(s)->game.economy.ledger[0][0]==1050);
    hmbtn_scene_preview_session(s)->game.clock.hour=12;
    CHECK(travel(s,32,error,sizeof(error)) && travel(s,24,error,sizeof(error)) && travel(s,128,error,sizeof(error)));
    CHECK(merchant(s,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && hmbtn_scene_preview_session(s)->feed_shop.item==0x109);
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_NAME,error,sizeof(error)));
    CHECK(editor_wait(s,error,sizeof(error)));
    CHECK(capture(s,&host,shots,"chicken-name",error,sizeof(error)));
    /* Cancel empty name: no chicken, no debit; return to the same menu. */
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->game.economy.gold==1550 && !hmbtn_poultry_count(&hmbtn_scene_preview_session(s)->poultry));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_NAME,error,sizeof(error)));
    CHECK(editor_wait(s,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && hmbtn_scene_preview_session(s)->feed_shop.editor.length==1);
    CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)));CHECK(editor_wait(s,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->game.economy.gold==50 && hmbtn_scene_preview_session(s)->menu.stats.animals[0]==1 && hmbtn_scene_preview_session(s)->poultry.hens[0].name[0]==0);
    /* Regression: Start -> chicken page after a real purchase must render. */
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_OFF,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)));
    for(unsigned i=0;i<25;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_RIGHT,error,sizeof(error)) && step(s,HMBTN_PAD_RIGHT,error,sizeof(error)) && hmbtn_scene_preview_session(s)->menu.page==2);
    CHECK(capture(s,&host,shots,"chicken-status",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)));
    for(unsigned i=0;i<25;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(merchant(s,error,sizeof(error)));CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->feed_shop.request==0x4F && hmbtn_poultry_count(&hmbtn_scene_preview_session(s)->poultry)==1);
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_UP,error,sizeof(error)));CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_COUNT,error,sizeof(error)));CHECK(step(s,HMBTN_PAD_RIGHT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->house.inventory->supplies[2]==2 && hmbtn_scene_preview_session(s)->game.economy.gold==30 && hmbtn_scene_preview_session(s)->game.economy.ledger[0][1]==1520);
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_OFF,error,sizeof(error)));
    CHECK(travel(s,24,error,sizeof(error)) && travel(s,32,error,sizeof(error)) && travel(s,0,error,sizeof(error)));
    for(unsigned night=0;night<2;++night) {
        CHECK(travel(s,112,error,sizeof(error)));
        if(!night) {
            HmbtnIntroActor before=hmbtn_scene_preview_session(s)->poultry.hens[0].actor;
            bool moved=false;
            for(unsigned tick=0;tick<600;++tick) {
                CHECK(step(s,0,error,sizeof(error)));
                const HmbtnIntroActor *hen=&hmbtn_scene_preview_session(s)->poultry.hens[0].actor;HmbtnGroundHit hit;
                CHECK(hmbtn_ground_find(&(*hmbtn_scene_preview_resources(s).scene).terrain,hen->position,0xE080,64,&hit) && hit.height==hen->position[1]);
                if(memcmp(before.position,hen->position,sizeof(before.position))) moved=true;
            }
            CHECK(moved);
            CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)) && hmbtn_scene_preview_session(s)->menu.mode==HMBTN_MENU_STATUS);
            before=hmbtn_scene_preview_session(s)->poultry.hens[0].actor;
            for(unsigned tick=0;tick<40;++tick) CHECK(step(s,0,error,sizeof(error)));
            CHECK(memcmp(&before,&hmbtn_scene_preview_session(s)->poultry.hens[0].actor,sizeof(before))==0);
            CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)));
            for(unsigned tick=0;tick<30;++tick) CHECK(step(s,0,error,sizeof(error)));
            hmbtn_scene_preview_session(s)->game.clock.hour=21;CHECK(step(s,0,error,sizeof(error)));
            CHECK(hmbtn_scene_preview_session(s)->poultry.hens[0].actor.sprite.animation==5+hmbtn_scene_preview_session(s)->poultry.hens[0].actor.facing);
            CHECK(capture(s,&host,shots,"chicken-sleeping",error,sizeof(error)));
            hmbtn_scene_preview_session(s)->game.clock.hour=12;
        }
        CHECK(feed(s,26,0,error,sizeof(error)) && feed(s,40,0,error,sizeof(error)));
        CHECK(hmbtn_scene_preview_session(s)->feed.coop[0] && !hmbtn_scene_preview_session(s)->house.inventory->held_item.id);
        memcpy(hmbtn_scene_preview_session(s)->intro.camera,hmbtn_scene_preview_session(s)->poultry.hens[0].actor.position,sizeof(hmbtn_scene_preview_session(s)->intro.camera));
        CHECK(capture(s,&host,shots,"chicken-fed",error,sizeof(error)));
        CHECK(travel(s,0,error,sizeof(error)) && sleep_day(s,error,sizeof(error)));
        CHECK(!hmbtn_scene_preview_session(s)->feed.coop[0] && hmbtn_scene_preview_session(s)->poultry.hens[0].affection==20+10*night);
        CHECK(hmbtn_scene_preview_session(s)->poultry.eggs[0].item==(night ? 0x122u : 0u));
    }
    CHECK(travel(s,112,error,sizeof(error)));
    HmbtnCoopEgg *egg=&hmbtn_scene_preview_session(s)->poultry.eggs[0];memcpy(hmbtn_scene_preview_session(s)->intro.player.position,egg->position,sizeof(egg->position));
    hmbtn_scene_preview_session(s)->intro.player.position[2]+=224;hmbtn_scene_preview_session(s)->intro.player.facing=0;hmbtn_scene_preview_session(s)->control.mode=0;
    memcpy(hmbtn_scene_preview_session(s)->intro.camera,hmbtn_scene_preview_session(s)->intro.player.position,sizeof(hmbtn_scene_preview_session(s)->intro.camera));
    CHECK(capture(s,&host,shots,"chicken-egg",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && hmbtn_scene_preview_session(s)->house.inventory->held_item.id==0x122 && !egg->item);
    CHECK(idle(s,error,sizeof(error)));
    CHECK(travel(s,0,error,sizeof(error)) && ship(s,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->game.economy.shipping.pending_value==50 && hmbtn_scene_preview_session(s)->game.economy.shipments[15]==1);
    hmbtn_scene_preview_session(s)->game.clock.hour=17;hmbtn_scene_preview_session(s)->game.clock.minute=57;hmbtn_scene_preview_session(s)->game.clock.frames=14;CHECK(step(s,0,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->game.economy.gold==80 && hmbtn_scene_preview_session(s)->game.economy.ledger[0][0]==1100 && !hmbtn_scene_preview_session(s)->carry.discarded);
    /* Sell the very same chicken through Lillia, after exercising both cancels. */
    CHECK(sleep_day(s,error,sizeof(error))); /* Next business visit, never rewind a resident's route. */
    if(hmbtn_scene_preview_session(s)->game.clock.weekday==0) CHECK(sleep_day(s,error,sizeof(error))); /* Shop closed Sunday. */
    hmbtn_scene_preview_session(s)->game.clock.hour=12;hmbtn_scene_preview_session(s)->game.clock.minute=0;
    CHECK(travel(s,32,error,sizeof(error)) && travel(s,24,error,sizeof(error)) && travel(s,128,error,sizeof(error)));
    CHECK(merchant(s,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && step(s,HMBTN_PAD_DOWN,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->feed_shop.selection==2);
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_SELL_LIST,error,sizeof(error)));
    CHECK(capture(s,&host,shots,"chicken-sale-list",error,sizeof(error)));
    const HmbtnIntroActor before_sale=hmbtn_scene_preview_session(s)->intro.player;const HmbtnFarmClock sale_clock=hmbtn_scene_preview_session(s)->game.clock;
    for(unsigned i=0;i<40;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(!memcmp(&hmbtn_scene_preview_session(s)->intro.player,&before_sale,sizeof(before_sale)) && !memcmp(&hmbtn_scene_preview_session(s)->game.clock,&sale_clock,sizeof(sale_clock)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->game.economy.gold==80 && hmbtn_poultry_count(&hmbtn_scene_preview_session(s)->poultry)==1);
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_SELL_LIST,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_SELL_CONFIRM,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->feed_shop.request==0x60);
    CHECK(capture(s,&host,shots,"chicken-sale-confirm",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->game.economy.gold==80 && hmbtn_poultry_count(&hmbtn_scene_preview_session(s)->poultry)==1);
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_SELL_LIST,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_SELL_CONFIRM,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->game.economy.gold==580 && hmbtn_scene_preview_session(s)->game.economy.ledger[0][0]==1600 && hmbtn_scene_preview_session(s)->game.economy.ledger[0][1]==1520
        && !hmbtn_scene_preview_session(s)->menu.stats.animals[0] && !hmbtn_poultry_count(&hmbtn_scene_preview_session(s)->poultry) && hmbtn_poultry_room(&hmbtn_scene_preview_session(s)->poultry));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->feed_shop.request==0x59 && hmbtn_scene_preview_session(s)->game.economy.gold==580);
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_OFF,error,sizeof(error)));
    CHECK(travel(s,24,error,sizeof(error)) && travel(s,32,error,sizeof(error)) && travel(s,0,error,sizeof(error)));
    CHECK(travel(s,112,error,sizeof(error)) && !hmbtn_scene_preview_session(s)->poultry.hens[0].occupied);
    CHECK(capture(s,&host,shots,"chicken-sold-empty-coop",error,sizeof(error)));
    /* Every item pack and seasonal forage region imports actual art. */
    for(unsigned season=0;season<4;++season) {
        hmbtn_scene_preview_session(s)->game.clock.season=season;
        for(unsigned area=64;area<=76;area+=4) {
            CHECK(hmbtn_scene_preview_load_area(s,area,error,sizeof(error)));
            CHECK(capture(s,&host,NULL,"season",error,sizeof(error)));
        }
    }
    /* Separate UI fixtures: the upgrade and contest cannot yet be earned in game.
     * They must not finance the economic playthrough above. */
    hmbtn_scene_preview_session(s)->game.clock=(HmbtnFarmClock){0,0,2,2,12,0,0};
    CHECK(hmbtn_scene_preview_load_area(s,128,error,sizeof(error)));
    hmbtn_scene_preview_session(s)->poultry=(HmbtnPoultry){0};hmbtn_scene_preview_session(s)->poultry.upgraded=true;
    const uint16_t name[9]={0,1,2,3,4,5,6,7,0xFFFF};
    for(unsigned i=0;i<10;++i) {
        CHECK(hmbtn_poultry_buy(&hmbtn_scene_preview_session(s)->poultry,name,&hmbtn_scene_preview_session(s)->game.clock));hmbtn_scene_preview_session(s)->poultry.hens[i].affection=255;
    }
    hmbtn_scene_preview_session(s)->poultry.hens[1].stage=HMBTN_HEN_EGG;hmbtn_scene_preview_session(s)->poultry.hens[3].stage=HMBTN_HEN_CHICK;
    hmbtn_scene_preview_session(s)->poultry.hens[5].dead=true;hmbtn_scene_preview_session(s)->poultry.hens[9].prize=true;
    CHECK(step(s,0,error,sizeof(error)));
    CHECK(merchant(s,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && hmbtn_scene_preview_session(s)->feed_shop.selection==2);
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_SELL_LIST,error,sizeof(error)));
    for(unsigned i=0;i<5;++i) CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->feed_shop.sale_cursor==5 && hmbtn_scene_preview_session(s)->feed_shop.sale_top==2);
    CHECK(capture(s,&host,shots,"chicken-sale-scroll",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && hmbtn_scene_preview_session(s)->feed_shop.sale_cursor==6 && hmbtn_scene_preview_session(s)->feed_shop.sale_top==3);
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_SELL_CONFIRM,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->feed_shop.sale_slot==9 && hmbtn_scene_preview_session(s)->feed_shop.request==0x61);
    CHECK(capture(s,&host,shots,"chicken-sale-prize",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->game.economy.gold==2580 && hmbtn_scene_preview_session(s)->game.economy.ledger[0][0]==3600 && !hmbtn_scene_preview_session(s)->poultry.hens[9].occupied);
    puts("Original-resource economy: 500G -> forage/shipping -> named chicken + feed -> egg shipping -> cancelled/confirmed chicken sale, 580G and empty coop passed.");
    puts("Separate sale UI fixtures: upgraded capacity, sparse adult list, scrolling, full names/hearts and 2000G prize sale passed.");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

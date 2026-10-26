/* Public scene/session APIs exercise the existing integration routes. Resources
 * come from the user's installation; no proprietary fixtures are distributed. */
#include "scene_fixture.h"
#include "sdl_host.h"
#include "economy_checks.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Economy route line %d: %s; %s\n",__LINE__,#x,error);goto fail; } } while(0)
static bool status_draw(HmbtnScenePreview *s,unsigned page,uint8_t *pixels,char *error,size_t size)
{
    hmbtn_scene_preview_session(s)->menu.mode=HMBTN_MENU_STATUS;hmbtn_scene_preview_session(s)->menu.phase=3;hmbtn_scene_preview_session(s)->menu.page=page;
    memset(pixels,0,FRAME_BYTES);
    return hmbtn_farm_menu_ui_draw(hmbtn_scene_preview_resources(s).menu_ui,hmbtn_scene_preview_resources(s).house_ui,hmbtn_scene_preview_resources(s).ui,&hmbtn_scene_preview_session(s)->menu,&hmbtn_scene_preview_session(s)->house,
        &hmbtn_scene_preview_session(s)->farm_action,hmbtn_game_registration_const(&hmbtn_scene_preview_session(s)->game),hmbtn_scene_preview_resources(s).root,0,pixels,error,size);
}
int main(int argc,char **argv)
{
    if(argc!=2) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL,*other=NULL;char error[512]={0};
    uint8_t *before=NULL,*after=NULL;
    CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock initial={0,0,2,2,12,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&initial,error,sizeof(error));CHECK(s);
    HmbtnEconomy *economy=hmbtn_game_economy(&hmbtn_scene_preview_session(s)->game);
    const HmbtnEconomy defaults={.gold=500};CHECK(economy_equal(economy,&defaults));
    CHECK(hmbtn_scene_preview_session(s)->menu.economy==economy);
    economy->gold=4321;economy->shipping=(HmbtnFarmShipping){120,199,false,true,true};
    for(unsigned i=0;i<25;++i) economy->shipments[i]=i+1;
    for(unsigned i=0;i<4;++i) for(unsigned j=0;j<2;++j) economy->ledger[i][j]=100+i*10+j;
    HmbtnEconomy expected=*economy;
    const unsigned areas[]={80,112,128,136,0,80};
    for(unsigned i=0;i<sizeof(areas)/sizeof(*areas);++i) {
        CHECK(hmbtn_scene_preview_load_area(s,areas[i],error,sizeof(error)));hmbtn_scene_preview_session(s)->intro.area=hmbtn_scene_preview_session(s)->area;
        CHECK(hmbtn_scene_preview_session(s)->menu.economy==economy && economy_equal(economy,&expected));
    }
    hmbtn_farm_menu_init(&hmbtn_scene_preview_session(s)->menu,economy);hmbtn_scene_preview_session(s)->menu.poultry=&hmbtn_scene_preview_session(s)->poultry;
    CHECK(economy_equal(economy,&expected));
    before=malloc(FRAME_BYTES);after=malloc(FRAME_BYTES);CHECK(before && after);
    /* Real status pages must consult live values and never mutate finances. */
    CHECK(status_draw(s,0,before,error,sizeof(error)));
    const HmbtnEventCommand reward={.type=44,.operand=-1,.x=300};
    hmbtn_scene_preview_session(s)->visit.event=130;
    CHECK(test_visit_command(s,&reward,error,sizeof(error))==HMBTN_EVENT_DONE);
    hmbtn_scene_preview_session(s)->visit=(HmbtnFarmVisit){0};expected.gold+=300;expected.ledger[0][0]+=300;
    CHECK(status_draw(s,0,after,error,sizeof(error)) && memcmp(before,after,FRAME_BYTES));
    CHECK(economy_equal(economy,&expected));
    hmbtn_scene_preview_session(s)->menu.mode=HMBTN_MENU_NONE;
    hmbtn_scene_preview_session(s)->feed_shop=(HmbtnFeedShop){.phase=HMBTN_SHOP_GREETING,.merchant=15,.poultry=&hmbtn_scene_preview_session(s)->poultry};
    memset(before,0,FRAME_BYTES);
    CHECK(hmbtn_feed_shop_ui_draw(&hmbtn_scene_preview_session(s)->feed_shop,economy,hmbtn_game_registration_const(&hmbtn_scene_preview_session(s)->game),hmbtn_scene_preview_resources(s).ui,before,0,error,sizeof(error)));
    hmbtn_scene_preview_session(s)->feed_shop.phase=HMBTN_SHOP_COUNT;hmbtn_scene_preview_session(s)->feed_shop.item=0x68;hmbtn_scene_preview_session(s)->feed_shop.quantity=2;
    hmbtn_scene_preview_session(s)->feed_shop.message.words[0]=0xffff;hmbtn_scene_preview_session(s)->feed_shop.message.count=1;
    hmbtn_scene_preview_session(s)->feed_shop.message.phase=HMBTN_MESSAGE_WAIT_END;
    hmbtn_feed_shop_tick(&hmbtn_scene_preview_session(s)->feed_shop,&hmbtn_scene_preview_session(s)->house,economy,&hmbtn_scene_preview_session(s)->menu.stats,HMBTN_PAD_CROSS,0);
    expected.gold-=40;expected.ledger[0][1]+=40;
    CHECK(economy_equal(economy,&expected) && hmbtn_scene_preview_session(s)->game.inventory.supplies[0]==2);
    hmbtn_scene_preview_session(s)->feed_shop.phase=HMBTN_SHOP_GREETING;memset(after,0,FRAME_BYTES);
    CHECK(hmbtn_feed_shop_ui_draw(&hmbtn_scene_preview_session(s)->feed_shop,economy,hmbtn_game_registration_const(&hmbtn_scene_preview_session(s)->game),hmbtn_scene_preview_resources(s).ui,after,0,error,sizeof(error)));
    CHECK(memcmp(before,after,FRAME_BYTES) && economy_equal(economy,&expected));
    hmbtn_scene_preview_session(s)->feed_shop.phase=HMBTN_SHOP_OFF;
    CHECK(status_draw(s,1,before,error,sizeof(error)));
    CHECK(hmbtn_farm_shipping_add(economy,0x41));
    expected.shipments[0]++;expected.shipping.pending_value+=60;
    CHECK(status_draw(s,1,after,error,sizeof(error)) && memcmp(before,after,FRAME_BYTES));
    CHECK(economy_equal(economy,&expected));
    CHECK(status_draw(s,6,before,error,sizeof(error)));
    hmbtn_scene_preview_session(s)->game.clock.hour=17;hmbtn_scene_preview_session(s)->game.clock.minute=58;
    hmbtn_session_clock_events(hmbtn_scene_preview_session(s),&hmbtn_scene_preview_session(s)->game.clock,HMBTN_CLOCK_MINUTE);
    expected.gold+=180;expected.ledger[0][0]+=180;
    expected.shipping.pending_value=0;expected.shipping.settled=true;
    CHECK(status_draw(s,6,after,error,sizeof(error)) && memcmp(before,after,FRAME_BYTES));
    CHECK(economy_equal(economy,&expected));
    CHECK(hmbtn_farm_shipping_add(economy,0x41));
    expected.shipments[0]++;expected.shipping.pending_value=60;
    hmbtn_farm_menu_init(&hmbtn_scene_preview_session(s)->menu,economy);hmbtn_scene_preview_session(s)->menu.poultry=&hmbtn_scene_preview_session(s)->poultry;
    hmbtn_session_clock_events(hmbtn_scene_preview_session(s),&hmbtn_scene_preview_session(s)->game.clock,HMBTN_CLOCK_MINUTE);
    CHECK(economy_equal(economy,&expected)); /* No second payment after UI recreation. */

    /* Existing load/save boundaries do not write or restore a save file. */
    hmbtn_scene_preview_session(s)->house.mode=HMBTN_HOUSE_BED;hmbtn_scene_preview_session(s)->house.bed_choice=2;
    hmbtn_scene_preview_session(s)->house.message.phase=HMBTN_MESSAGE_WAIT_END;
    hmbtn_house_interaction_tick(&hmbtn_scene_preview_session(s)->house,HMBTN_PAD_CROSS,0);
    CHECK(hmbtn_scene_preview_session(s)->house.boundary==2 && hmbtn_scene_preview_tick(s,(HmbtnLogicalInput){0},error,sizeof(error)));
    CHECK(economy_equal(economy,&expected));
    hmbtn_scene_preview_session(s)->house.mode=HMBTN_HOUSE_BED;hmbtn_scene_preview_session(s)->house.bed_choice=1;
    hmbtn_scene_preview_session(s)->house.message.phase=HMBTN_MESSAGE_WAIT_END;
    hmbtn_house_interaction_tick(&hmbtn_scene_preview_session(s)->house,HMBTN_PAD_CROSS,0);CHECK(hmbtn_scene_preview_session(s)->house.save_requested);
    CHECK(test_morning(s,true,error,sizeof(error)));
    expected.shipping.settled=false;
    CHECK(hmbtn_scene_preview_session(s)->game.clock.day==3 && hmbtn_scene_preview_session(s)->game.clock.hour==6 && !hmbtn_scene_preview_session(s)->house.save_requested);
    CHECK(hmbtn_scene_preview_session(s)->menu.economy==economy && economy_equal(economy,&expected));
    hmbtn_scene_preview_session(s)->game.clock.hour=17;hmbtn_scene_preview_session(s)->game.clock.minute=58;hmbtn_session_clock_events(hmbtn_scene_preview_session(s),&hmbtn_scene_preview_session(s)->game.clock,HMBTN_CLOCK_MINUTE);
    expected.gold+=60;expected.ledger[0][0]+=60;expected.shipping.pending_value=0;expected.shipping.settled=true;
    CHECK(economy_equal(economy,&expected));
    other=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&initial,error,sizeof(error));CHECK(other);
    CHECK(hmbtn_scene_preview_session(other)->menu.economy!=economy && economy_equal(hmbtn_scene_preview_session(other)->menu.economy,&defaults));
    CHECK(economy_equal(economy,&expected));
    free(after);free(before);hmbtn_scene_preview_destroy(other);hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);
    puts("Live financial UI, script reward, purchase, areas, morning, save/load boundaries and independent scenes passed.");return 0;
fail:
    free(after);free(before);hmbtn_scene_preview_destroy(other);hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

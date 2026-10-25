/* Private scene API follows existing integration drivers until MA-08. Resources
 * come from the user's installation; no proprietary fixtures are distributed. */
#include "scene_preview.c"
#include "sdl_host.h"
#include "economy_checks.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Economy route line %d: %s; %s\n",__LINE__,#x,error);goto fail; } } while(0)
static bool status_draw(HmbtnScenePreview *s,unsigned page,uint8_t *pixels,char *error,size_t size)
{
    s->menu.mode=HMBTN_MENU_STATUS;s->menu.phase=3;s->menu.page=page;
    memset(pixels,0,FRAME_BYTES);
    return hmbtn_farm_menu_ui_draw(s->menu_ui,s->house_ui,s->ui,&s->menu,&s->house,
        &s->farm_action,hmbtn_game_registration_const(&s->session->game),s->data_root,0,pixels,error,size);
}
int main(int argc,char **argv)
{
    if(argc!=2) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL,*other=NULL;char error[512]={0};
    uint8_t *before=NULL,*after=NULL;
    CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock initial={0,0,2,2,12,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&initial,error,sizeof(error));CHECK(s);
    HmbtnEconomy *economy=hmbtn_game_economy(&s->session->game);
    const HmbtnEconomy defaults={.gold=500};CHECK(economy_equal(economy,&defaults));
    CHECK(s->menu.economy==economy);
    economy->gold=4321;economy->shipping=(HmbtnFarmShipping){120,199,false,true,true};
    for(unsigned i=0;i<25;++i) economy->shipments[i]=i+1;
    for(unsigned i=0;i<4;++i) for(unsigned j=0;j<2;++j) economy->ledger[i][j]=100+i*10+j;
    HmbtnEconomy expected=*economy;
    const unsigned areas[]={80,112,128,136,0,80};
    for(unsigned i=0;i<sizeof(areas)/sizeof(*areas);++i) {
        CHECK(load_area(s,argv[1],areas[i],error,sizeof(error)));s->intro.area=s->area;
        CHECK(s->menu.economy==economy && economy_equal(economy,&expected));
    }
    hmbtn_farm_menu_init(&s->menu,economy);s->menu.poultry=&s->session->poultry;
    CHECK(economy_equal(economy,&expected));
    before=malloc(FRAME_BYTES);after=malloc(FRAME_BYTES);CHECK(before && after);
    /* Real status pages must consult live values and never mutate finances. */
    CHECK(status_draw(s,0,before,error,sizeof(error)));
    const HmbtnEventCommand reward={.type=44,.operand=-1,.x=300};
    s->visit.event=130;
    CHECK(visit_command(s,&reward,error,sizeof(error))==HMBTN_EVENT_DONE);
    s->visit=(HmbtnFarmVisit){0};expected.gold+=300;expected.ledger[0][0]+=300;
    CHECK(status_draw(s,0,after,error,sizeof(error)) && memcmp(before,after,FRAME_BYTES));
    CHECK(economy_equal(economy,&expected));
    s->menu.mode=HMBTN_MENU_NONE;
    s->feed_shop=(HmbtnFeedShop){.phase=HMBTN_SHOP_GREETING,.merchant=15,.poultry=&s->session->poultry};
    memset(before,0,FRAME_BYTES);
    CHECK(hmbtn_feed_shop_ui_draw(&s->feed_shop,economy,hmbtn_game_registration_const(&s->session->game),s->ui,before,0,error,sizeof(error)));
    s->feed_shop.phase=HMBTN_SHOP_COUNT;s->feed_shop.item=0x68;s->feed_shop.quantity=2;
    s->feed_shop.message.words[0]=0xffff;s->feed_shop.message.count=1;
    s->feed_shop.message.phase=HMBTN_MESSAGE_WAIT_END;
    hmbtn_feed_shop_tick(&s->feed_shop,&s->house,economy,&s->menu.stats,HMBTN_PAD_CROSS,0);
    expected.gold-=40;expected.ledger[0][1]+=40;
    CHECK(economy_equal(economy,&expected) && s->session->game.inventory.supplies[0]==2);
    s->feed_shop.phase=HMBTN_SHOP_GREETING;memset(after,0,FRAME_BYTES);
    CHECK(hmbtn_feed_shop_ui_draw(&s->feed_shop,economy,hmbtn_game_registration_const(&s->session->game),s->ui,after,0,error,sizeof(error)));
    CHECK(memcmp(before,after,FRAME_BYTES) && economy_equal(economy,&expected));
    s->feed_shop.phase=HMBTN_SHOP_OFF;
    CHECK(status_draw(s,1,before,error,sizeof(error)));
    CHECK(hmbtn_farm_shipping_add(economy,0x41));
    expected.shipments[0]++;expected.shipping.pending_value+=60;
    CHECK(status_draw(s,1,after,error,sizeof(error)) && memcmp(before,after,FRAME_BYTES));
    CHECK(economy_equal(economy,&expected));
    CHECK(status_draw(s,6,before,error,sizeof(error)));
    s->session->game.clock.hour=17;s->session->game.clock.minute=58;
    world_clock_events(s,&s->session->game.clock,HMBTN_CLOCK_MINUTE);
    expected.gold+=180;expected.ledger[0][0]+=180;
    expected.shipping.pending_value=0;expected.shipping.settled=true;
    CHECK(status_draw(s,6,after,error,sizeof(error)) && memcmp(before,after,FRAME_BYTES));
    CHECK(economy_equal(economy,&expected));
    CHECK(hmbtn_farm_shipping_add(economy,0x41));
    expected.shipments[0]++;expected.shipping.pending_value=60;
    hmbtn_farm_menu_init(&s->menu,economy);s->menu.poultry=&s->session->poultry;
    world_clock_events(s,&s->session->game.clock,HMBTN_CLOCK_MINUTE);
    CHECK(economy_equal(economy,&expected)); /* No second payment after UI recreation. */

    /* Existing load/save boundaries do not write or restore a save file. */
    s->house.mode=HMBTN_HOUSE_BED;s->house.bed_choice=2;
    s->house.message.phase=HMBTN_MESSAGE_WAIT_END;
    hmbtn_house_interaction_tick(&s->house,HMBTN_PAD_CROSS,0);
    CHECK(s->house.boundary==2 && opening_tick(s,0,error,sizeof(error)));
    CHECK(economy_equal(economy,&expected));
    s->house.mode=HMBTN_HOUSE_BED;s->house.bed_choice=1;
    s->house.message.phase=HMBTN_MESSAGE_WAIT_END;
    hmbtn_house_interaction_tick(&s->house,HMBTN_PAD_CROSS,0);CHECK(s->house.save_requested);
    CHECK(morning(s,true,error,sizeof(error)));
    expected.shipping.settled=false;
    CHECK(s->session->game.clock.day==3 && s->session->game.clock.hour==6 && !s->house.save_requested);
    CHECK(s->menu.economy==economy && economy_equal(economy,&expected));
    s->session->game.clock.hour=17;s->session->game.clock.minute=58;world_clock_events(s,&s->session->game.clock,HMBTN_CLOCK_MINUTE);
    expected.gold+=60;expected.ledger[0][0]+=60;expected.shipping.pending_value=0;expected.shipping.settled=true;
    CHECK(economy_equal(economy,&expected));
    other=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&initial,error,sizeof(error));CHECK(other);
    CHECK(other->menu.economy!=economy && economy_equal(other->menu.economy,&defaults));
    CHECK(economy_equal(economy,&expected));
    free(after);free(before);hmbtn_scene_preview_destroy(other);hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);
    puts("Live financial UI, script reward, purchase, areas, morning, save/load boundaries and independent scenes passed.");return 0;
fail:
    free(after);free(before);hmbtn_scene_preview_destroy(other);hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

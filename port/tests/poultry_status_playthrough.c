#include "poultry_helpers.h"
static bool status_open(HmbtnScenePreview *s,char *error,size_t size)
{
    REQUIRE(s->menu.mode==HMBTN_MENU_NONE);REQUIRE(step(s,HMBTN_PAD_START,error,size));
    for(unsigned i=0;i<30 && s->menu.phase!=3;++i) REQUIRE(step(s,0,error,size));
    REQUIRE(s->menu.mode==HMBTN_MENU_STATUS && s->menu.phase==3 && !s->menu.page && !s->menu.list_top);return true;
}
static bool chicken_page(HmbtnScenePreview *s,char *error,size_t size)
{
    REQUIRE(step(s,HMBTN_PAD_RIGHT,error,size) && s->menu.page==1);
    REQUIRE(step(s,HMBTN_PAD_RIGHT,error,size) && s->menu.page==2);return true;
}
static bool status_close(HmbtnScenePreview *s,char *error,size_t size)
{
    REQUIRE(step(s,HMBTN_PAD_START,error,size));
    for(unsigned i=0;i<30 && s->menu.mode!=HMBTN_MENU_NONE;++i) REQUIRE(step(s,0,error,size));
    REQUIRE(s->menu.mode==HMBTN_MENU_NONE);return true;
}
int main(int argc,char **argv)
{
    if(argc<2 || argc>3) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};const char *shots=argc==3 ? argv[2] : NULL;
    CHECK(hmbtn_host_open(&host));s=hmbtn_scene_area_create(host.renderer,argv[1],112,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    s->clock=(HmbtnFarmClock){0,0,2,2,12,0,0};sync_clock(s);
    /* Declared UI fixtures only: no animals, money or upgrades are granted in normal play. */
    const uint16_t name[9]={0,1,2,3,4,5,6,7,0xFFFF};
    CHECK(hmbtn_poultry_buy(&s->poultry,name,&s->clock));s->poultry.hens[0].affection=255;
    s->poultry.hens[0].season=2;s->poultry.hens[0].day=29;s->poultry.hens[0].age=3;s->poultry.hens[0].prize=true;
    CHECK(status_open(s,error,sizeof(error)) && chicken_page(s,error,sizeof(error)));
    CHECK(capture(s,&host,shots,"status-one-adult",error,sizeof(error)));
    HmbtnPoultry before=s->poultry;HmbtnFarmClock clock=s->clock;
    for(unsigned i=0;i<80;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(!memcmp(&s->poultry,&before,sizeof(before)) && !memcmp(&s->clock,&clock,sizeof(clock)));
    CHECK(status_close(s,error,sizeof(error)));
    s->poultry.upgraded=true;
    for(unsigned i=1;i<10;++i) {
        CHECK(hmbtn_poultry_buy(&s->poultry,name,&s->clock));
        s->poultry.hens[i].affection=i*25;s->poultry.hens[i].season=i%4;
        s->poultry.hens[i].day=i%2 ? 0 : 29;s->poultry.hens[i].age=i;
    }
    s->poultry.hens[0].days_sick=2;s->poultry.hens[2].stage=HMBTN_HEN_CHICK;
    s->poultry.hens[2].days_sick=2; /* Chick uses its normal icon, per status_draw_chicken_row. */
    s->poultry.hens[9].held=true;s->poultry.hens[9].actor.visible=false;
    s->house.held_item=(HmbtnStoredTool){0x109,0,1};
    CHECK(status_open(s,error,sizeof(error)) && chicken_page(s,error,sizeof(error)));
    CHECK(s->menu.stats.animals[0]==10 && !s->menu.list_top);
    CHECK(capture(s,&host,shots,"status-seven-rows",error,sizeof(error)));
    before=s->poultry;clock=s->clock;
    CHECK(step(s,HMBTN_PAD_UP,error,sizeof(error)) && s->menu.list_top==3);
    CHECK(capture(s,&host,shots,"status-last-seven",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && !s->menu.list_top);
    for(unsigned i=1;i<=3;++i) CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && s->menu.list_top==i);
    CHECK(capture(s,&host,shots,"status-rows-scrolled",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && !s->menu.list_top);
    CHECK(step(s,HMBTN_PAD_LEFT,error,sizeof(error)) && s->menu.page==1 && !s->menu.list_top);
    CHECK(step(s,HMBTN_PAD_RIGHT,error,sizeof(error)) && s->menu.page==2 && !s->menu.list_top);
    CHECK(!memcmp(&s->poultry,&before,sizeof(before)) && !memcmp(&s->clock,&clock,sizeof(clock)));
    s->poultry.hens[2].days_sick=0; /* End the impossible-health UI fixture before resuming gameplay. */
    CHECK(status_close(s,error,sizeof(error)));
    /* Sparse records: incubation, pending name and a cleared slot are omitted. */
    s->poultry.hens[1].stage=HMBTN_HEN_EGG;s->poultry.hens[4].stage=HMBTN_HEN_CHICK;
    s->poultry.hens[4].naming_pending=true;s->poultry.hens[7].occupied=false;
    CHECK(status_open(s,error,sizeof(error)) && chicken_page(s,error,sizeof(error)) && s->menu.stats.animals[0]==7);
    CHECK(step(s,HMBTN_PAD_UP,error,sizeof(error)) && !s->menu.list_top);
    CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && !s->menu.list_top);
    CHECK(hmbtn_poultry_status_slot(&s->poultry,6)==9); /* Held animal remains in the status list. */
    CHECK(capture(s,&host,shots,"status-filtered",error,sizeof(error)));
    CHECK(status_close(s,error,sizeof(error)));
    s->poultry=(HmbtnPoultry){0};s->house.held_item=(HmbtnStoredTool){0};
    CHECK(status_open(s,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_RIGHT,error,sizeof(error)) && s->menu.page==1);
    CHECK(step(s,HMBTN_PAD_RIGHT,error,sizeof(error)) && s->menu.page==5);
    CHECK(capture(s,&host,shots,"status-empty-skip",error,sizeof(error)));
    CHECK(status_close(s,error,sizeof(error)) && s->menu.stats.gold==500 && !s->carry.discarded);
    puts("Chicken status: original portraits, chick/sick/prize/date/hearts, 7-row scrolling, sparse/held records, pause, close/reopen and empty-page skip passed.");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

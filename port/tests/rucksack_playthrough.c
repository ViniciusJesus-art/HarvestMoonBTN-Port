/* Real supermarket stock/counter, item/equip ANI, ten nights and both upgrades.
 * Purse and ordinary bag contents are fixtures; upgrades are bought, never granted. */
#include "poultry_helpers.h"
#include "field_helpers.h"
static bool menu_wait(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<1000 && g->menu.message.phase!=HMBTN_MESSAGE_WAIT_END;++i) REQUIRE(step(s,0,error,size));
    REQUIRE(g->menu.message.phase==HMBTN_MESSAGE_WAIT_END);return true;
}
static bool menu_cell(HmbtnScenePreview *s,unsigned goal,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);unsigned queue[18],parent[18],arrow[18],head=0,tail=1;
    for(unsigned i=0;i<18;++i) parent[i]=99;
    const unsigned start=g->menu.cursor;queue[0]=start;parent[start]=start;
    const uint32_t pads[]={HMBTN_PAD_UP,HMBTN_PAD_DOWN,HMBTN_PAD_LEFT,HMBTN_PAD_RIGHT};
    while(head<tail && parent[goal]==99) {
        unsigned from=queue[head++];const HmbtnStorageSpot *spot=hmbtn_storage_spot(from);
        for(unsigned d=0;d<4;++d) {
            const unsigned to=spot->next[g->game.inventory.rucksack_level+d*3];
            if(to>=18 || parent[to]!=99 || !hmbtn_inventory_slot(&g->game.inventory,to)) continue;
            parent[to]=from;arrow[to]=d;queue[tail++]=to;
        }
    }
    REQUIRE(parent[goal]!=99);unsigned path[18],n=0;
    for(unsigned to=goal;to!=start;to=parent[to]) path[n++]=arrow[to];
    while(n) REQUIRE(step(s,pads[path[--n]],error,size));
    REQUIRE(g->menu.cursor==goal);return true;
}
static bool bag_menu(HmbtnScenePreview *s,HmbtnHost *host,const char *shots,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const unsigned cap=hmbtn_inventory_capacity(&g->game.inventory);
    REQUIRE(step(s,HMBTN_PAD_TRIANGLE,error,size) && g->menu.mode==HMBTN_MENU_RUCKSACK && menu_wait(s,error,size));
    REQUIRE(menu_cell(s,cap,error,size) && capture(s,host,shots,"rucksack-tools",error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && step(s,HMBTN_PAD_CROSS,error,size));
    REQUIRE(g->menu.marked==255);
    REQUIRE(menu_cell(s,9+cap,error,size) && capture(s,host,shots,"rucksack-items",error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && step(s,HMBTN_PAD_CROSS,error,size));
    REQUIRE(g->game.inventory.held_item.id==0x61 && g->menu.marked==255);
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && step(s,HMBTN_PAD_CROSS,error,size));
    REQUIRE(!g->game.inventory.held_item.id); /* Repeat on the emptied cell puts it back. */
    REQUIRE(step(s,HMBTN_PAD_CIRCLE,error,size));
    for(unsigned i=0;i<100 && g->menu.mode!=HMBTN_MENU_NONE;++i) REQUIRE(step(s,0,error,size));
    REQUIRE(g->menu.mode==HMBTN_MENU_NONE);return true;
}
static bool close_shop(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<3000 && (g->feed_shop.phase!=HMBTN_SHOP_OFF || g->event_item.phase);++i)
        REQUIRE(step(s,g->event_item.phase ? 0 : HMBTN_PAD_CROSS,error,size));
    REQUIRE(!g->event_item.phase && g->feed_shop.phase==HMBTN_SHOP_OFF);return true;
}
static void aim(HmbtnScenePreview *s)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const HmbtnGatherRecord *r=hmbtn_farm_gather_record(115);
    memcpy(g->intro.player.position,r->position,sizeof(r->position));g->intro.player.position[0]-=400;
    g->intro.player.position[1]=0;g->intro.player.facing=3;g->control.mode=0;
    memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));
    memcpy(g->intro.camera,g->intro.player.position,sizeof(g->intro.camera));
}
static bool counter(HmbtnScenePreview *s,HmbtnHost *host,const char *shots,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);REQUIRE(g->area==180 && g->village.people[1].actor.visible);
    aim(s);REQUIRE(capture(s,host,shots,"rucksack-counter",error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->feed_shop.rucksack && g->event_item.phase==HMBTN_EVENT_ITEM_SHOW);
    REQUIRE(!hmbtn_save_available(g,error,size));error[0]=0;
    REQUIRE(capture(s,host,shots,"rucksack-show",error,size));
    REQUIRE(shop_wait(s,HMBTN_SHOP_OFFER,error,size) && g->game.inventory.held_item.id==g->feed_shop.item);
    REQUIRE(capture(s,host,shots,"rucksack-description",error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->feed_shop.phase==HMBTN_SHOP_MAKER_CONFIRM);
    REQUIRE(capture(s,host,shots,"rucksack-confirm",error,size));return true;
}
int main(int argc,char **argv)
{
    if(argc<3 || argc>4) return 2;const char *shots=argc==4 ? argv[3] : NULL;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock clock={0,3,2,3,12,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&clock,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->game.economy.gold=12000;
    g->village.flags[251+288]=g->village.flags[251+140]=true;
    CHECK(g->game.inventory.rucksack_level==0);
    /* Full starting capacity: upgrades do not require a spare bag cell. */
    for(unsigned i=0;i<2;++i) { g->game.inventory.bag[i]=(HmbtnStoredTool){34,0,(uint16_t)(i+2)};g->game.inventory.items[i]=(HmbtnStoredTool){0x61,(uint16_t)i,1}; }
    CHECK(bag_menu(s,&host,shots,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_load_area(s,180,error,sizeof(error)));
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    for(unsigned level=0;level<2;++level) {
        const unsigned item=0x6d+level,price=level ? 5000 : 3000;
        const unsigned facing=g->village.people[1].actor.facing,gold=g->game.economy.gold;
        CHECK(counter(s,&host,shots,error,sizeof(error)) && g->feed_shop.item==item);
        CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && close_shop(s,error,sizeof(error)));
        CHECK(!g->game.inventory.held_item.id && g->game.inventory.rucksack_level==level && g->game.economy.gold==gold);
        g->game.economy.gold=price-1;
        CHECK(counter(s,&host,shots,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
        CHECK(shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && g->feed_shop.request==0x4f);
        CHECK(close_shop(s,error,sizeof(error)) && g->game.inventory.rucksack_level==level);
        g->game.economy.gold=gold;
        const HmbtnInventory before=g->game.inventory;
        CHECK(counter(s,&host,shots,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
        CHECK(shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && g->feed_shop.request==0xbb && g->game.economy.gold==gold);
        for(unsigned tick=0;tick<2000 && g->event_item.phase!=HMBTN_EVENT_ITEM_RUCKSACK;++tick) CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
        CHECK(g->event_item.phase==HMBTN_EVENT_ITEM_RUCKSACK && g->intro.player.sprite.animation==32);
        CHECK(g->game.economy.gold==gold-price && g->game.inventory.rucksack_level==level+1 && g->game.inventory.rucksack_mornings_left==10);
        CHECK(capture(s,&host,shots,"rucksack-equip",error,sizeof(error)) && close_shop(s,error,sizeof(error)));
        CHECK(!memcmp(before.bag,g->game.inventory.bag,sizeof(before.bag)) && !memcmp(before.items,g->game.inventory.items,sizeof(before.items)));
        CHECK(!g->game.inventory.held_item.id && g->village.people[1].actor.facing==facing);
        const unsigned cap=hmbtn_inventory_capacity(&g->game.inventory);CHECK(cap==(4u<<level));
        for(unsigned i=2u<<level;i<cap;++i) { g->game.inventory.bag[i]=(HmbtnStoredTool){34,0,(uint16_t)(i+2)};g->game.inventory.items[i]=(HmbtnStoredTool){0x61,(uint16_t)i,1}; }
        CHECK(bag_menu(s,&host,shots,error,sizeof(error)));
        /* Both stores use the newly unlocked last cell through the shared UI. */
        CHECK(hmbtn_inventory_storage_swap(&g->game.inventory,0,cap,0,9,0));
        CHECK(hmbtn_inventory_storage_swap(&g->game.inventory,0,9,0,cap,0));
        CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)));
        char snapshot[1024];CHECK(snprintf(snapshot,sizeof(snapshot),"%s.level%u",argv[2],level+1)<(int)sizeof(snapshot));
        CHECK(field_reload(&s,&host,argv[1],snapshot,error,sizeof(error)));g=hmbtn_scene_preview_session(s);
        if(level) break;
        for(unsigned day=0;day<10;++day) {
            CHECK(!hmbtn_inventory_rucksack_offer(&g->game.inventory));
            g->game.weather.tomorrow=HMBTN_WEATHER_SUNNY;
            CHECK(sleep_day(s,error,sizeof(error)) && g->game.inventory.rucksack_mornings_left==9-day);
            if(day==4) { CHECK(snprintf(snapshot,sizeof(snapshot),"%s.waiting",argv[2])<(int)sizeof(snapshot));
                CHECK(field_reload(&s,&host,argv[1],snapshot,error,sizeof(error)));g=hmbtn_scene_preview_session(s); }
        }
        CHECK(hmbtn_inventory_rucksack_offer(&g->game.inventory)==0x6e);
        const unsigned minute=g->game.clock.hour*60+g->game.clock.minute;
        hmbtn_session_advance_minutes(g,(1440+720-minute)%1440);
        if(g->game.clock.weekday==0 || g->game.clock.weekday==2) hmbtn_session_advance_minutes(g,1440);
        CHECK(hmbtn_scene_preview_load_area(s,180,error,sizeof(error)));
        for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    }
    CHECK(!hmbtn_inventory_rucksack_offer(&g->game.inventory));
    CHECK(field_reload(&s,&host,argv[1],argv[2],error,sizeof(error)));
    puts("Original supermarket -> both rucksacks, ten nights, carry/equip ANI, 2/4/8 menus/stores and resume passed");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

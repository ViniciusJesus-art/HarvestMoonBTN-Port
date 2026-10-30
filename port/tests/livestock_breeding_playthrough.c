/* Barley's real stock/text -> original potion ANI -> twenty fed nights ->
 * original event 299, both maternity pens, names and original calf/lamb art.
 * Fixtures provide adult animals and completed barn, money and fodder. */
#include "poultry_helpers.h"
#include "field_helpers.h"
#include "shop_ui_helpers.h"
#include "livestock_art.h"
static bool original_art(const char *root,char *error,size_t size)
{
    HmbtnLivestockArt art={0};
    if(!hmbtn_livestock_art_load(&art,root,error,size)) return false;
    uint8_t *pixels=calloc(1,FRAME_BYTES);bool ok=pixels!=NULL;
    const unsigned packs[]={0,3,4,7};
    for(unsigned i=0;i<4 && ok;++i) for(unsigned face=0;face<4 && ok;++face) {
        memset(pixels,0,FRAME_BYTES);const HmbtnAnimationPlayer pose={0,(uint16_t)face,0};
        ok=hmbtn_animation_draw(&art.animation[packs[i]],art.atlas,&pose,160,120,pixels,320,240,FRAME_BYTES,error,size);
        unsigned visible=0;for(unsigned p=3;p<FRAME_BYTES;p+=4) visible+=pixels[p]!=0;
        ok=ok && visible>100;
    }
    free(pixels);hmbtn_livestock_art_close(&art);return ok;
}
static bool choose(HmbtnScenePreview *s,unsigned selection,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<hmbtn_barn_shop_rows(&g->feed_shop) && g->feed_shop.selection!=selection;++i) REQUIRE(step(s,HMBTN_PAD_DOWN,error,size));
    REQUIRE(g->feed_shop.selection==selection);return true;
}
static bool potion(HmbtnScenePreview *s,unsigned slot,unsigned id,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);HmbtnFarmAnimal *a=&g->livestock.animals[slot];
    /* Equip the purchased tool through the shared inventory exchange. */
    unsigned bag=0;while(bag<8 && g->game.inventory.bag[bag].id!=id) ++bag;
    REQUIRE(g->game.inventory.equipped.id==id || (bag<8 && hmbtn_inventory_swap(&g->game.inventory,0,bag+1)));
    a->timer=300;a->state=1;
    memcpy(g->intro.player.position,a->actor.position,sizeof(a->actor.position));g->intro.player.position[0]-=128;
    g->intro.player.facing=3;g->control.mode=0;
    REQUIRE(step(s,HMBTN_PAD_SQUARE,error,size) && g->farm_action.phase==HMBTN_FARM_MEDICINE && g->farm_action.bank==26);
    REQUIRE(!a->pregnancy && g->game.inventory.equipped.id==id);
    for(unsigned t=0;t<300 && g->farm_action.phase!=HMBTN_FARM_ACTION_FREE;++t) REQUIRE(step(s,0,error,size));
    REQUIRE(a->pregnancy==1 && !g->game.inventory.equipped.id && g->livestock.births[slot].active
        && g->livestock.animals[g->livestock.births[slot].child].reserved);
    const unsigned count=hmbtn_livestock_count(&g->livestock,slot==1);
    for(unsigned t=0;t<20;++t) REQUIRE(step(s,0,error,size));
    REQUIRE(hmbtn_livestock_count(&g->livestock,slot==1)==count);return true;
}
static bool births(HmbtnScenePreview *s,HmbtnHost *host,const char *shots,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);bool started=false,barn=false,no_cancel=false,seen[2]={false,false};
    unsigned names=0;bool naming=false;
    for(unsigned t=0;t<16000;++t) {
        uint32_t pad=HMBTN_PAD_CROSS;
        if(g->animal_name_active) {
            HmbtnNewGame *e=&g->animal_name;pad=0;
            if(!naming) { ++names;naming=true; }
            if(e->message.phase==HMBTN_MESSAGE_WAIT_END && !e->queued) {
                if(!e->length && !no_cancel) { pad=HMBTN_PAD_CIRCLE;no_cancel=true; }
                else if(!e->length) pad=HMBTN_PAD_CROSS;
                else pad=e->confirming ? HMBTN_PAD_CROSS : HMBTN_PAD_START;
            }
        } else naming=false;
        REQUIRE(step(s,pad,error,size) && !g->intro.script.boundary);
        started|=g->animal_event==299;barn|=g->area==96;
        if(pad==HMBTN_PAD_CIRCLE) REQUIRE(g->animal_name_active && !g->animal_name.finished);
        if(g->area==96) for(unsigned mode=0;mode<2;++mode) {
            HmbtnFarmAnimal *a=hmbtn_livestock_newborn(&g->livestock,mode);
            const HmbtnFarmAnimal *mother=&g->livestock.animals[g->livestock.births[mode].mother];
            if(a && !seen[mode] && !g->intro.fade_level
                    && abs(g->intro.player.position[0]-mother->actor.position[0])<1000) {
                REQUIRE(hmbtn_livestock_kind(a)==(mode ? 0x164u : 0x160u));
                REQUIRE(capture(s,host,shots,mode ? "newborn-lamb" : "newborn-calf",error,size));seen[mode]=true;
            }
        }
        if(g->animal_name_active || g->intro.message) REQUIRE(capture(s,host,NULL,"birth-event",error,size));
        if(started && !g->animal_event && g->control.enabled) {
            for(unsigned i=0;i<60 && g->intro.fade_level;++i) REQUIRE(step(s,0,error,size));
            REQUIRE(g->area==80 && barn && names==2 && seen[0] && seen[1] && no_cancel);
            REQUIRE(!g->livestock.births[0].active && !g->livestock.births[1].active);
            REQUIRE(hmbtn_livestock_count(&g->livestock,false)==2 && hmbtn_livestock_count(&g->livestock,true)==2);return true;
        }
    }
    snprintf(error,size,"Livestock birth stuck at original event step %u",g->intro.script.pc);return false;
}
int main(int argc,char **argv)
{
    if(argc<3 || argc>4) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};const char *shots=argc==4 ? argv[3] : NULL;
    CHECK(original_art(argv[1],error,sizeof(error)));
    CHECK(hmbtn_host_open(&host));const HmbtnFarmClock clock={.day=3,.weekday=3,.hour=12};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],136,&clock,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->game.economy.gold=6000;g->game.inventory.supplies[0]=40;
    g->poultry.upgraded=true;g->construction.house_level=1;g->construction.barn_level=1;
    g->village.flags[251+288]=g->village.flags[251+140]=true;
    const uint16_t name[9]={1,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff};
    CHECK(hmbtn_livestock_buy(&g->livestock,0x161,name,&g->game.clock) && hmbtn_livestock_buy(&g->livestock,0x165,name,&g->game.clock));
    g->livestock.animals[0].growth=28;
    const HmbtnIntroActor *barley=&g->village.people[15].actor;CHECK(barley->visible);
    memcpy(g->intro.player.position,barley->position,sizeof(barley->position));g->intro.player.position[2]+=224;
    g->intro.player.facing=0;g->control.mode=0;
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(g->feed_shop.request==0x46 && choose(s,8,error,sizeof(error)) && shop_cursor_pixels(s,23,34,error,sizeof(error)));
    CHECK(capture(s,&host,shots,"barley-potions",error,sizeof(error)));
    for(unsigned i=0;i<2;++i) {
        CHECK(choose(s,8+i,error,sizeof(error)) && g->feed_shop.item==0x1f+i);
        CHECK(step(s,HMBTN_PAD_TRIANGLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)) && g->feed_shop.request==0x65+i);
        CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
        CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)) && hmbtn_inventory_count(&g->game.inventory,0x1f+(int)i,0,2)==1);
    }
    CHECK(!g->game.economy.gold && step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_OFF,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)) && travel(s,96,error,sizeof(error)));
    CHECK(potion(s,0,0x1f,error,sizeof(error)) && potion(s,1,0x20,error,sizeof(error)));
    CHECK(hmbtn_livestock_pack(&g->livestock.animals[0])==3 && hmbtn_livestock_pack(&g->livestock.animals[1])==7);
    char pregnant[2048];CHECK(snprintf(pregnant,sizeof(pregnant),"%s.pregnant",argv[2])<(int)sizeof(pregnant));
    CHECK(capture(s,&host,shots,"pregnant-animals",error,sizeof(error)) && field_reload(&s,&host,argv[1],pregnant,error,sizeof(error)));
    g=hmbtn_scene_preview_session(s);CHECK(g->livestock.animals[0].pregnancy==1 && g->livestock.births[1].active);
    /* Same clock observer cannot advance a gestation twice on the same date. */
    hmbtn_session_clock_events(g,&g->game.clock,HMBTN_CLOCK_DAY);CHECK(g->livestock.animals[0].pregnancy==1);
    for(unsigned day=1;day<=20;++day) {
        CHECK(feed(s,9,0,error,sizeof(error)) && feed(s,41,18,error,sizeof(error)));
        CHECK(feed(s,9,0,error,sizeof(error)) && feed(s,41,19,error,sizeof(error)));
        CHECK(travel(s,0,error,sizeof(error)));
        if(day<20) CHECK(sleep_day(s,error,sizeof(error)));
        else {
            CHECK(travel(s,80,error,sizeof(error)));const unsigned before=g->game.clock.day;
            g->house.mode=HMBTN_HOUSE_SLEEP;
            for(unsigned t=0;t<1200 && (g->game.clock.day==before || g->morning_fade);++t) CHECK(step(s,0,error,sizeof(error)));
            CHECK(g->game.clock.day!=before && !g->morning_fade && g->area==80);
        }
        if(day<20) {
            CHECK(g->livestock.animals[0].pregnancy==day+1 && !g->livestock.births[0].ready);
            CHECK(travel(s,96,error,sizeof(error)));
        }
    }
    CHECK(g->livestock.births[0].ready && g->livestock.births[1].ready && !g->game.inventory.supplies[0]);
    CHECK(births(s,&host,shots,error,sizeof(error)));
    CHECK(travel(s,0,error,sizeof(error)) && travel(s,96,error,sizeof(error)));
    memcpy(g->intro.player.position,g->livestock.animals[2].actor.position,sizeof(g->intro.player.position));
    g->intro.player.position[0]-=400;g->intro.player.position[2]+=400;
    CHECK(step(s,0,error,sizeof(error)) && capture(s,&host,shots,"named-offspring",error,sizeof(error)));
    CHECK(g->livestock.animals[2].name[0]==0 && g->livestock.animals[3].name[0]==0);
    CHECK(field_reload(&s,&host,argv[1],argv[2],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(!g->livestock.births[0].active && hmbtn_livestock_kind(&g->livestock.animals[2])==0x160
        && hmbtn_livestock_kind(&g->livestock.animals[3])==0x164);
    CHECK(travel(s,0,error,sizeof(error)) && travel(s,80,error,sizeof(error)) && field_reload(&s,&host,argv[1],argv[2],error,sizeof(error)));
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);
    puts("Breeding: real Barley potion stock/ANI, reserved records, twenty fed nights, original event 299 with both births/names/art, reload/app resume passed.");return 0;
fail:hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

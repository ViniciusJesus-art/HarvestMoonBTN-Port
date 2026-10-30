/* Loads a checkpoint produced by the real TV Shopping playthrough. Utensils,
 * recipes and flags are never granted here; the cucumber stock is a fixture. */
#include "poultry_helpers.h"
#include "field_helpers.h"
#include "cooking.h"
static bool ready(HmbtnScenePreview *s,HmbtnKitchenPhase phase,char *error,size_t size)
{
    HmbtnHouseInteraction *h=&hmbtn_scene_preview_session(s)->house;
    for(unsigned i=0;i<1600;++i) {
        if(h->mode==HMBTN_HOUSE_COOKING && h->kitchen.phase==phase && !h->closing
                && h->message.phase==HMBTN_MESSAGE_WAIT_END) return true;
        REQUIRE(step(s,h->message.phase==HMBTN_MESSAGE_WAIT_PAGE ? HMBTN_PAD_CROSS : 0,error,size));
    }
    fprintf(stderr,"Kitchen wait expected %u, got mode %u phase %u message %u/%x\n",phase,h->mode,h->kitchen.phase,h->message.phase,h->request);return false;
}
static bool object(HmbtnScenePreview *s,unsigned id,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<50;++i) {
        const uint8_t *z=hmbtn_scene_preview_resources(s).section+16+i*44;
        if(hmbtn_read_le16(z)!=0x4000 || hmbtn_read_le16(z+2)!=id) continue;
        at(s,z);const unsigned face=hmbtn_read_le16(z+8);REQUIRE(face<4);
        g->intro.player.position[face&1 ? 0 : 2]+=face<2 ? 224 : -224;
        memcpy(g->intro.camera,g->intro.player.position,sizeof(g->intro.camera));
        return step(s,HMBTN_PAD_CROSS,error,size);
    }
    return false;
}
static bool new_recipe(HmbtnScenePreview *s,unsigned options,char *error,size_t size)
{
    REQUIRE(object(s,5,error,size) && ready(s,HMBTN_KITCHEN_OPEN,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && ready(s,HMBTN_KITCHEN_FIRST,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && ready(s,HMBTN_KITCHEN_SECOND,error,size));
    REQUIRE(step(s,HMBTN_PAD_DOWN,error,size) && step(s,HMBTN_PAD_CROSS,error,size) && ready(s,HMBTN_KITCHEN_TOOLS,error,size));
    if(options&1u) REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
    if(options&0x100u) {
        REQUIRE(step(s,HMBTN_PAD_UP,error,size) && step(s,HMBTN_PAD_LEFT,error,size) && step(s,HMBTN_PAD_CROSS,error,size));
        REQUIRE(ready(s,HMBTN_KITCHEN_SEASONING,error,size));
        REQUIRE(step(s,HMBTN_PAD_DOWN,error,size) && step(s,HMBTN_PAD_CROSS,error,size) && step(s,HMBTN_PAD_CIRCLE,error,size));
        REQUIRE(ready(s,HMBTN_KITCHEN_TOOLS,error,size));
    }
    REQUIRE(hmbtn_scene_preview_session(s)->house.kitchen.options==options);
    REQUIRE(step(s,HMBTN_PAD_START,error,size) && ready(s,HMBTN_KITCHEN_ITEMS,error,size));
    REQUIRE(step(s,HMBTN_PAD_R1,error,size) && step(s,HMBTN_PAD_CROSS,error,size));
    REQUIRE(step(s,HMBTN_PAD_START,error,size) && ready(s,HMBTN_KITCHEN_CONFIRM,error,size));return true;
}
static bool records(HmbtnScenePreview *s,char *error,size_t size)
{
    REQUIRE(object(s,5,error,size) && ready(s,HMBTN_KITCHEN_OPEN,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && ready(s,HMBTN_KITCHEN_FIRST,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && ready(s,HMBTN_KITCHEN_SECOND,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && ready(s,HMBTN_KITCHEN_RECORDS,error,size));return true;
}
static unsigned cucumbers(const HmbtnInventory *h)
{ return (h->items[0].id==0x43 ? h->items[0].count : 0)+(h->icebox[0].id==0x43 ? h->icebox[0].count : 0); }
static bool preparation(HmbtnScenePreview *s,HmbtnHost *host,const char *shots,unsigned result,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);HmbtnHouseInteraction *h=&g->house;
    const unsigned stock=cucumbers(&g->game.inventory),gold=g->game.economy.gold;
    const HmbtnFarmClock clock=g->game.clock;unsigned stages=0,track_ticks=0;bool black=false;
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
    REQUIRE(h->kitchen.phase==HMBTN_KITCHEN_CLOSE_CONFIRM && cucumbers(&g->game.inventory)==stock && !g->game.inventory.held_item.id);
    uint64_t wait_ticket=0;
    for(unsigned i=0;i<1500;++i) {
        const HmbtnKitchenPhase phase=h->kitchen.phase;
        if(phase==HMBTN_KITCHEN_RESULT && h->message.phase==HMBTN_MESSAGE_WAIT_END) break;
        if(phase>=HMBTN_KITCHEN_CLOSE_CONFIRM) stages|=1u<<(phase-HMBTN_KITCHEN_CLOSE_CONFIRM);
        if(phase==HMBTN_KITCHEN_CLOSE_CONFIRM || phase==HMBTN_KITCHEN_FADE_OUT)
            REQUIRE(cucumbers(&g->game.inventory)==stock && !g->game.inventory.held_item.id);
        if(phase==HMBTN_KITCHEN_TRACK) {
            ++track_ticks;REQUIRE(cucumbers(&g->game.inventory)==stock-1 && !h->request && hmbtn_house_kitchen_shade(h)==255);
            REQUIRE(g->game.inventory.held_item.id && g->menu.mode==HMBTN_MENU_NONE);
            char reason[128];REQUIRE(!hmbtn_save_available(g,reason,sizeof(reason)));
            const HmbtnServiceResult *service=hmbtn_service_poll(&g->services,HMBTN_SERVICE_AUDIO);
            if(service->request.kind==HMBTN_SERVICE_WAIT_XA && service->status==HMBTN_SERVICE_PENDING) {
                if(!wait_ticket) wait_ticket=service->ticket;else REQUIRE(wait_ticket==service->ticket);
            }
            if(h->kitchen.audio_started) {
                const HmbtnXAStream *xa=hmbtn_scene_preview_resources(s).xa;
                REQUIRE(xa->track==0x16 && xa->channel==3);
                if(!black) {
                    const HmbtnKitchenMenu menu=h->kitchen;const unsigned volume=xa->volume;
                    for(unsigned draw=0;draw<3;++draw) REQUIRE(capture(s,host,shots,"cooking-black-xa",error,size));
                    REQUIRE(!memcmp(&menu,&h->kitchen,sizeof(menu)) && xa->volume==volume);
                    SDL_Surface *pixels=SDL_RenderReadPixels(host->renderer,NULL);REQUIRE(pixels);
                    bool zero=true;for(int y=0;y<pixels->h;++y) for(int x=0;x<pixels->w;++x) {
                        const uint8_t *p=(const uint8_t *)pixels->pixels+y*pixels->pitch+x*4;zero &= !p[0] && !p[1] && !p[2];
                    }
                    SDL_DestroySurface(pixels);REQUIRE(zero);black=true;
                }
            }
        }
        if(phase==HMBTN_KITCHEN_FADE_IN) REQUIRE(cucumbers(&g->game.inventory)==stock-1 && !h->request);
        REQUIRE(step(s,HMBTN_PAD_CROSS|HMBTN_PAD_CIRCLE|HMBTN_PAD_START,error,size));
        REQUIRE(!memcmp(&clock,&g->game.clock,sizeof(clock)) && g->game.economy.gold==gold);
    }
    REQUIRE(stages==15 && track_ticks>=122 && wait_ticket && black);
    REQUIRE(h->kitchen.phase==HMBTN_KITCHEN_RESULT && h->message.phase==HMBTN_MESSAGE_WAIT_END && h->request==result);
    REQUIRE(cucumbers(&g->game.inventory)==stock-1 && !hmbtn_scene_preview_resources(s).xa->active);
    return capture(s,host,shots,"cooking-result",error,size);
}
static bool leave_result(HmbtnScenePreview *s,bool yes,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    if(!yes && g->house.request>=0x3c3) REQUIRE(step(s,HMBTN_PAD_DOWN,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
    for(unsigned i=0;i<100 && g->house.mode!=HMBTN_HOUSE_FREE;++i) REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
    REQUIRE(g->house.mode==HMBTN_HOUSE_FREE);
    REQUIRE(step(s,HMBTN_PAD_R2,error,size) && idle(s,error,size) && !g->game.inventory.held_item.id);return true;
}
static bool cooking_pcm(HmbtnScenePreview *s,const char *root,char *error,size_t size)
{
    HmbtnXAStream xa={0};
    REQUIRE(hmbtn_xa_stream_start(&xa,root,0x16,error,size));
    for(unsigned i=0;i<4;++i) hmbtn_xa_stream_tick(&xa);
    int16_t pcm[512*2];uint64_t energy=0;
    bool ok=true;
    for(unsigned i=0;i<512 && ok;++i) {
        memset(pcm,0,sizeof(pcm));ok=hmbtn_xa_stream_mix(&xa,pcm,512,error,size);
        for(unsigned j=0;j<1024;++j) energy+=(uint64_t)((int64_t)pcm[j]*pcm[j]);
    }
    ok=ok && energy>0 && xa.ended && xa.channel==3 && xa.decoder.rate==37800;
    hmbtn_xa_stream_close(&xa);REQUIRE(ok);return true;
}
int main(int argc,char **argv)
{
    if(argc<4 || argc>5) return 2;const char *shots=argc==5 ? argv[4] : NULL;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};CHECK(hmbtn_host_open(&host));
    s=hmbtn_scene_saved_create(host.renderer,argv[1],argv[2],error,sizeof(error));CHECK(s);
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    CHECK(cooking_pcm(s,argv[1],error,sizeof(error)));
    CHECK(g->game.cooking.utensils==255 && hmbtn_cooking_record_count(&g->game.cooking)==1);
    for(unsigned i=0;i<9;++i) CHECK(g->game.shopping.offers[i]==255);
    g->game.inventory.items[0]=(HmbtnStoredTool){0x43,0,2};g->game.inventory.icebox[0]=(HmbtnStoredTool){0x43,0,5};
    CHECK(new_recipe(s,0x100,error,sizeof(error)) && capture(s,&host,shots,"cooking-confirm",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && ready(s,HMBTN_KITCHEN_ITEMS,error,sizeof(error)));
    CHECK(cucumbers(&g->game.inventory)==7 && !g->game.inventory.held_item.id);
    CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)) && ready(s,HMBTN_KITCHEN_CONFIRM,error,sizeof(error)));
    CHECK(preparation(s,&host,shots,0x3c1,error,sizeof(error)) && g->game.inventory.held_item.id==0xba);
    CHECK(g->game.inventory.items[0].count==1 && g->game.inventory.icebox[0].count==5);
    CHECK(leave_result(s,true,error,sizeof(error)));
    unsigned pickles=hmbtn_cooking_record_slot(&g->game.cooking,0xba);CHECK(pickles<HMBTN_RECIPE_RECORDS && g->game.cooking.records[pickles].quality==20);
    CHECK(new_recipe(s,0x101,error,sizeof(error)) && preparation(s,&host,shots,0x3c3,error,sizeof(error)));
    CHECK(leave_result(s,false,error,sizeof(error)) && g->game.cooking.records[pickles].quality==20);
    CHECK(new_recipe(s,0x101,error,sizeof(error)) && preparation(s,&host,shots,0x3c3,error,sizeof(error)));
    CHECK(leave_result(s,true,error,sizeof(error)) && g->game.cooking.records[pickles].quality==23);
    CHECK(new_recipe(s,0x100,error,sizeof(error)) && preparation(s,&host,shots,0x3c4,error,sizeof(error)));
    CHECK(leave_result(s,false,error,sizeof(error)) && g->game.cooking.records[pickles].quality==23);
    CHECK(records(s,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && step(s,HMBTN_PAD_TRIANGLE,error,sizeof(error)) && ready(s,HMBTN_KITCHEN_RECORD_VIEW,error,sizeof(error)));
    CHECK(capture(s,&host,shots,"cooking-saved-recipe-tools",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && ready(s,HMBTN_KITCHEN_RECORDS,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && ready(s,HMBTN_KITCHEN_RECIPE_CONFIRM,error,sizeof(error)));
    CHECK(preparation(s,&host,shots,0x3c0,error,sizeof(error)) && g->game.inventory.held_item.modifier==23);
    CHECK(leave_result(s,true,error,sizeof(error)));
    CHECK(new_recipe(s,0x100,error,sizeof(error)) && preparation(s,&host,shots,0x3c4,error,sizeof(error)));
    CHECK(leave_result(s,true,error,sizeof(error)) && g->game.cooking.records[pickles].quality==20);
    CHECK(new_recipe(s,0,error,sizeof(error)) && preparation(s,&host,shots,0x3c2,error,sizeof(error)));
    CHECK(g->game.inventory.held_item.id==0xe2 && leave_result(s,true,error,sizeof(error)));
    CHECK(!cucumbers(&g->game.inventory) && hmbtn_cooking_record_count(&g->game.cooking)==2);
    CHECK(records(s,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && ready(s,HMBTN_KITCHEN_DESCRIPTION,error,sizeof(error)));
    CHECK(g->house.request==0x3ba && capture(s,&host,shots,"cooking-missing-ingredients",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && ready(s,HMBTN_KITCHEN_RECORDS,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)));
    for(unsigned i=0;i<100 && g->house.mode!=HMBTN_HOUSE_FREE;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)));
    for(unsigned i=0;i<40 && g->menu.phase!=3;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(g->menu.mode==HMBTN_MENU_STATUS && g->menu.cooking==&g->game.cooking && capture(s,&host,shots,"cooking-status-count",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)));
    for(unsigned i=0;i<40 && g->menu.mode!=HMBTN_MENU_NONE;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(field_reload(&s,&host,argv[1],argv[3],error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(g->game.cooking.utensils==255 && hmbtn_cooking_record_count(&g->game.cooking)==2 && g->game.cooking.records[pickles].quality==20);
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);
    puts("Cooking: purchased utensils, seven XA/fade preparations, no duplicate consumption, result/record choices, missing marks, status and resume passed.");return 0;
fail:
    if(s) hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

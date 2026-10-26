/* Installed original scripts/messages/ANI. Fixtures position the player and
 * move starting tools between stores; no rod is granted here. */
#include "scene_fixture.h"
#include "sdl_host.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Greg line %d scenario %u: %s; %s\n",__LINE__,scenario,#x,error);goto fail; } } while(0)
static bool step(HmbtnScenePreview *s,uint32_t pressed,uint32_t held,char *error,size_t size)
{
    return hmbtn_scene_preview_tick(s,(HmbtnLogicalInput){pressed,held},error,size);
}
static unsigned rods(const HmbtnHouseInteraction *h)
{
    unsigned n=h->inventory->equipped.id==56;
    for(unsigned i=0;i<8;++i) n+=h->inventory->bag[i].id==56;
    for(unsigned i=0;i<64;++i) n+=h->inventory->toolbox[i].id==56;
    return n;
}
static bool start(HmbtnScenePreview *s,char *error,size_t size)
{
    const HmbtnIntroActor *g=&hmbtn_scene_preview_session(s)->village.people[32].actor;
    if(!g->visible) { snprintf(error,size,"Greg absent from original beach schedule");return false; }
    memcpy(hmbtn_scene_preview_session(s)->intro.player.position,g->position,sizeof(g->position));
    hmbtn_scene_preview_session(s)->intro.player.position[2]+=300;hmbtn_scene_preview_session(s)->intro.player.facing=0;hmbtn_scene_preview_session(s)->control.mode=0;
    memcpy(hmbtn_scene_preview_session(s)->intro.camera,hmbtn_scene_preview_session(s)->intro.player.position,sizeof(hmbtn_scene_preview_session(s)->intro.camera));
    return hmbtn_village_target(&hmbtn_scene_preview_session(s)->village,&hmbtn_scene_preview_session(s)->intro.player)==32
        && step(s,HMBTN_PAD_CROSS,0,error,size) && hmbtn_scene_preview_session(s)->talk.active;
}
static bool finish(HmbtnScenePreview *s,HmbtnHost *host,bool accept,unsigned *offers,
    unsigned *presentations,const char *capture,char *error,size_t size)
{
    unsigned seen=UINT32_MAX,shown=0;*offers=*presentations=0;
    for(unsigned frame=0;frame<4000 && (hmbtn_scene_preview_session(s)->talk.active || hmbtn_scene_preview_session(s)->quick_ticks);++frame) {
        if(hmbtn_scene_preview_session(s)->talk.pending && hmbtn_scene_preview_session(s)->talk.loaded && hmbtn_scene_preview_session(s)->talk.message_pc!=seen) {
            seen=hmbtn_scene_preview_session(s)->talk.message_pc;if(hmbtn_scene_preview_session(s)->talk.request==1718) ++*offers;
            const HmbtnTextResourceId id={HMBTN_TEXT_RECORD,hmbtn_scene_preview_session(s)->talk.bank,hmbtn_scene_preview_session(s)->talk.request};
            uint8_t original[HMBTN_TEXT_RECORD_BYTES];HmbtnMessage expected;
            if(!hmbtn_text_resource_read(hmbtn_scene_preview_resources(s).root,id,original,sizeof(original),error,size)
                    || !hmbtn_feed_shop_expand(original,sizeof(original),hmbtn_game_registration_const(&hmbtn_scene_preview_session(s)->game))
                    || !hmbtn_message_open(&expected,original,sizeof(original),error,size)) return false;
            if(expected.count!=hmbtn_scene_preview_session(s)->dialogue.message.count || memcmp(expected.words,hmbtn_scene_preview_session(s)->dialogue.message.words,
                    expected.count*sizeof(*expected.words))) { snprintf(error,size,"Greg dialogue differs from installed BIN");return false; }
        }
        uint32_t pressed=HMBTN_PAD_CROSS;
        if(hmbtn_scene_preview_session(s)->talk.active && hmbtn_scene_preview_session(s)->talk.request==1718 && hmbtn_scene_preview_session(s)->dialogue.message.phase==HMBTN_MESSAGE_WAIT_END
                && hmbtn_scene_preview_session(s)->dialogue.message.choices && !accept && !hmbtn_scene_preview_session(s)->dialogue.choice) pressed=HMBTN_PAD_DOWN;
        if(hmbtn_scene_preview_session(s)->quick_ticks && hmbtn_scene_preview_session(s)->quick_id==56) {
            if(hmbtn_scene_preview_session(s)->intro.player.sprite.animation!=71 || !hmbtn_scene_preview_session(s)->quick_tool) return false;
            if(!shown++) {
                ++*presentations;
                if(!hmbtn_scene_preview_draw(s,host->renderer,error,size)) return false;
                if(capture) {
                    SDL_Surface *p=SDL_RenderReadPixels(host->renderer,NULL);if(!p) return false;
                    const bool saved=SDL_SaveBMP(p,capture);SDL_DestroySurface(p);if(!saved) return false;
                }
            }
            int32_t position[3];memcpy(position,hmbtn_scene_preview_session(s)->intro.player.position,sizeof(position));
            if(!step(s,HMBTN_PAD_START,HMBTN_PAD_UP,error,size) || hmbtn_scene_preview_session(s)->menu.mode!=HMBTN_MENU_NONE
                    || memcmp(position,hmbtn_scene_preview_session(s)->intro.player.position,sizeof(position))) return false;
        } else if(!step(s,pressed,0,error,size)) return false;
    }
    return !hmbtn_scene_preview_session(s)->talk.active && !hmbtn_scene_preview_session(s)->talk.script.boundary && !hmbtn_scene_preview_session(s)->quick_ticks
        && hmbtn_scene_preview_session(s)->intro.player.sprite.animation==0 && (!shown || shown==11);
}
int main(int argc,char **argv)
{
    if(argc<2 || argc>3) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};unsigned scenario=0;
    CHECK(hmbtn_host_open(&host));
    for(scenario=0;scenario<4;++scenario) {
        const HmbtnFarmClock clock={0,0,5,5,7,0,0};
        s=hmbtn_scene_area_create_at(host.renderer,argv[1],56,&clock,error,sizeof(error));CHECK(s);
        for(unsigned i=0;i<110;++i) CHECK(step(s,0,0,error,sizeof(error)));
        CHECK(!rods(&hmbtn_scene_preview_session(s)->house) && !hmbtn_scene_preview_session(s)->village.flags[0xE7]);
        if(scenario==1 || scenario==2) {
            hmbtn_scene_preview_session(s)->house.inventory->equipped=hmbtn_scene_preview_session(s)->house.inventory->toolbox[0];hmbtn_scene_preview_session(s)->house.inventory->toolbox[0]=(HmbtnStoredTool){0};
            hmbtn_scene_preview_session(s)->house.inventory->bag[0]=hmbtn_scene_preview_session(s)->house.inventory->toolbox[1];hmbtn_scene_preview_session(s)->house.inventory->toolbox[1]=(HmbtnStoredTool){0};
        }
        if(scenario==2) { hmbtn_scene_preview_session(s)->house.inventory->bag[1]=hmbtn_scene_preview_session(s)->house.inventory->toolbox[2];hmbtn_scene_preview_session(s)->house.inventory->toolbox[2]=(HmbtnStoredTool){0}; }
        const HmbtnInventory before=*hmbtn_scene_preview_session(s)->house.inventory;
        unsigned offers,shown;CHECK(start(s,error,sizeof(error)));
        CHECK(finish(s,&host,scenario!=3,&offers,&shown,argc==3 && !scenario ? argv[2] : NULL,error,sizeof(error)));
        CHECK(offers==(scenario==2 ? 0u : 1u));
        CHECK(rods(&hmbtn_scene_preview_session(s)->house)==(scenario<2 ? 1u : 0u) && shown==(scenario<2 ? 1u : 0u));
        CHECK(hmbtn_scene_preview_session(s)->village.flags[0xE7]==(scenario!=2));
        CHECK(!memcmp(hmbtn_scene_preview_session(s)->house.inventory->toolbox,before.toolbox,sizeof(before.toolbox)));
        if(!scenario) CHECK(hmbtn_scene_preview_session(s)->house.inventory->equipped.id==56 && hmbtn_scene_preview_session(s)->house.inventory->equipped.count==1 && !hmbtn_scene_preview_session(s)->house.inventory->equipped.modifier);
        if(scenario==1) CHECK(hmbtn_scene_preview_session(s)->house.inventory->equipped.id==before.equipped.id && hmbtn_scene_preview_session(s)->house.inventory->bag[0].id==before.bag[0].id
            && hmbtn_scene_preview_session(s)->house.inventory->bag[1].id==56 && hmbtn_scene_preview_session(s)->house.inventory->bag[1].count==1 && !hmbtn_scene_preview_session(s)->house.inventory->bag[1].modifier);
        if(scenario==2 || scenario==3) CHECK(!memcmp(&hmbtn_scene_preview_session(s)->house.inventory->equipped,&before.equipped,sizeof(before.equipped))
            && !memcmp(hmbtn_scene_preview_session(s)->house.inventory->bag,before.bag,sizeof(before.bag)));
        if(scenario==1) {
            CHECK(hmbtn_inventory_cycle(hmbtn_scene_preview_session(s)->house.inventory,true,false) && hmbtn_inventory_cycle(hmbtn_scene_preview_session(s)->house.inventory,true,false));
            CHECK(hmbtn_scene_preview_session(s)->house.inventory->equipped.id==56 && rods(&hmbtn_scene_preview_session(s)->house)==1);
        }
        if(scenario==2) { /* Free a bag slot by returning its starting tool. */
            hmbtn_scene_preview_session(s)->house.inventory->toolbox[0]=hmbtn_scene_preview_session(s)->house.inventory->bag[0];hmbtn_scene_preview_session(s)->house.inventory->bag[0]=(HmbtnStoredTool){0};
        }
        CHECK(start(s,error,sizeof(error)));
        CHECK(finish(s,&host,true,&offers,&shown,NULL,error,sizeof(error)));
        CHECK(offers==(scenario==2 ? 1u : 0u) && shown==(scenario==2 ? 1u : 0u));
        CHECK(rods(&hmbtn_scene_preview_session(s)->house)==(scenario==3 ? 0u : 1u));
        if(scenario==2) CHECK(hmbtn_scene_preview_session(s)->house.inventory->bag[0].id==56);
        /* Scene reloads retain both the item and the script's offer flag. */
        CHECK(hmbtn_scene_preview_load_area(s,52,error,sizeof(error)));hmbtn_scene_preview_session(s)->intro.area=52;
        CHECK(hmbtn_scene_preview_load_area(s,56,error,sizeof(error)));hmbtn_scene_preview_session(s)->intro.area=56;
        CHECK(hmbtn_scene_preview_session(s)->village.flags[0xE7] && rods(&hmbtn_scene_preview_session(s)->house)==(scenario==3 ? 0u : 1u));
        hmbtn_scene_preview_destroy(s);s=NULL;
    }
    puts("Original Greg beach schedule, BIN dialogue, equipped/bag grant, full inventory, refusal, no duplicates and item presentation passed.");
    hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

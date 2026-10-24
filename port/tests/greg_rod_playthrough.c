/* Installed original scripts/messages/ANI. Fixtures position the player and
 * move starting tools between stores; no rod is granted here. */
#include "scene_preview.c"
#include "sdl_host.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Greg line %d scenario %u: %s; %s\n",__LINE__,scenario,#x,error);goto fail; } } while(0)
static bool step(HmbtnScenePreview *s,uint32_t pressed,uint32_t held,char *error,size_t size)
{
    hmbtn_scene_data_tick(&s->scene);s->pending_pressed=pressed;
    const bool ok=opening_tick(s,held,error,size);s->pending_pressed=0;return ok;
}
static unsigned rods(const HmbtnHouseInteraction *h)
{
    unsigned n=h->equipped.id==56;
    for(unsigned i=0;i<8;++i) n+=h->bag[i].id==56;
    for(unsigned i=0;i<64;++i) n+=h->toolbox[i].id==56;
    return n;
}
static bool start(HmbtnScenePreview *s,char *error,size_t size)
{
    const HmbtnIntroActor *g=&s->village.people[32].actor;
    if(!g->visible) { snprintf(error,size,"Greg absent from original beach schedule");return false; }
    memcpy(s->intro.player.position,g->position,sizeof(g->position));
    s->intro.player.position[2]+=300;s->intro.player.facing=0;s->control.mode=0;
    memcpy(s->intro.camera,s->intro.player.position,sizeof(s->intro.camera));
    return hmbtn_village_target(&s->village,&s->intro.player)==32
        && step(s,HMBTN_PAD_CROSS,0,error,size) && s->talk.active;
}
static bool finish(HmbtnScenePreview *s,HmbtnHost *host,bool accept,unsigned *offers,
    unsigned *presentations,const char *capture,char *error,size_t size)
{
    unsigned seen=UINT32_MAX,shown=0;*offers=*presentations=0;
    for(unsigned frame=0;frame<4000 && (s->talk.active || s->quick_ticks);++frame) {
        if(s->talk.pending && s->talk.loaded && s->talk.message_pc!=seen) {
            seen=s->talk.message_pc;if(s->talk.request==1718) ++*offers;
            const HmbtnTextResourceId id={HMBTN_TEXT_RECORD,s->talk.bank,s->talk.request};
            uint8_t original[HMBTN_TEXT_RECORD_BYTES];HmbtnMessage expected;
            if(!hmbtn_text_resource_read(s->data_root,id,original,sizeof(original),error,size)
                    || !hmbtn_feed_shop_expand(original,sizeof(original),&s->dialogue)
                    || !hmbtn_message_open(&expected,original,sizeof(original),error,size)) return false;
            if(expected.count!=s->dialogue.message.count || memcmp(expected.words,s->dialogue.message.words,
                    expected.count*sizeof(*expected.words))) { snprintf(error,size,"Greg dialogue differs from installed BIN");return false; }
        }
        uint32_t pressed=HMBTN_PAD_CROSS;
        if(s->talk.active && s->talk.request==1718 && s->dialogue.message.phase==HMBTN_MESSAGE_WAIT_END
                && s->dialogue.message.choices && !accept && !s->dialogue.choice) pressed=HMBTN_PAD_DOWN;
        if(s->quick_ticks && s->quick_id==56) {
            if(s->intro.player.sprite.animation!=71 || !s->quick_tool) return false;
            if(!shown++) {
                ++*presentations;
                if(!hmbtn_scene_preview_draw(s,host->renderer,0,0,0,error,size)) return false;
                if(capture) {
                    SDL_Surface *p=SDL_RenderReadPixels(host->renderer,NULL);if(!p) return false;
                    const bool saved=SDL_SaveBMP(p,capture);SDL_DestroySurface(p);if(!saved) return false;
                }
            }
            int32_t position[3];memcpy(position,s->intro.player.position,sizeof(position));
            if(!step(s,HMBTN_PAD_START,HMBTN_PAD_UP,error,size) || s->menu.mode!=HMBTN_MENU_NONE
                    || memcmp(position,s->intro.player.position,sizeof(position))) return false;
        } else if(!step(s,pressed,0,error,size)) return false;
    }
    return !s->talk.active && !s->talk.script.boundary && !s->quick_ticks
        && s->intro.player.sprite.animation==0 && (!shown || shown==11);
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
        CHECK(!rods(&s->house) && !s->village.flags[0xE7]);
        if(scenario==1 || scenario==2) {
            s->house.equipped=s->house.toolbox[0];s->house.toolbox[0]=(HmbtnStoredTool){0};
            s->house.bag[0]=s->house.toolbox[1];s->house.toolbox[1]=(HmbtnStoredTool){0};
        }
        if(scenario==2) { s->house.bag[1]=s->house.toolbox[2];s->house.toolbox[2]=(HmbtnStoredTool){0}; }
        const HmbtnHouseInteraction before=s->house;
        unsigned offers,shown;CHECK(start(s,error,sizeof(error)));
        CHECK(finish(s,&host,scenario!=3,&offers,&shown,argc==3 && !scenario ? argv[2] : NULL,error,sizeof(error)));
        CHECK(offers==(scenario==2 ? 0u : 1u));
        CHECK(rods(&s->house)==(scenario<2 ? 1u : 0u) && shown==(scenario<2 ? 1u : 0u));
        CHECK(s->village.flags[0xE7]==(scenario!=2));
        CHECK(!memcmp(s->house.toolbox,before.toolbox,sizeof(before.toolbox)));
        if(!scenario) CHECK(s->house.equipped.id==56 && s->house.equipped.count==1 && !s->house.equipped.modifier);
        if(scenario==1) CHECK(s->house.equipped.id==before.equipped.id && s->house.bag[0].id==before.bag[0].id
            && s->house.bag[1].id==56 && s->house.bag[1].count==1 && !s->house.bag[1].modifier);
        if(scenario==2 || scenario==3) CHECK(!memcmp(&s->house.equipped,&before.equipped,sizeof(before.equipped))
            && !memcmp(s->house.bag,before.bag,sizeof(before.bag)));
        if(scenario==1) {
            CHECK(hmbtn_inventory_cycle(&s->house,true,false) && hmbtn_inventory_cycle(&s->house,true,false));
            CHECK(s->house.equipped.id==56 && rods(&s->house)==1);
        }
        if(scenario==2) { /* Free a bag slot by returning its starting tool. */
            s->house.toolbox[0]=s->house.bag[0];s->house.bag[0]=(HmbtnStoredTool){0};
        }
        CHECK(start(s,error,sizeof(error)));
        CHECK(finish(s,&host,true,&offers,&shown,NULL,error,sizeof(error)));
        CHECK(offers==(scenario==2 ? 1u : 0u) && shown==(scenario==2 ? 1u : 0u));
        CHECK(rods(&s->house)==(scenario==3 ? 0u : 1u));
        if(scenario==2) CHECK(s->house.bag[0].id==56);
        /* Scene reloads retain both the item and the script's offer flag. */
        CHECK(load_area(s,s->data_root,52,error,sizeof(error)));s->intro.area=52;
        CHECK(load_area(s,s->data_root,56,error,sizeof(error)));s->intro.area=56;
        CHECK(s->village.flags[0xE7] && rods(&s->house)==(scenario==3 ? 0u : 1u));
        hmbtn_scene_preview_destroy(s);s=NULL;
    }
    puts("Original Greg beach schedule, BIN dialogue, equipped/bag grant, full inventory, refusal, no duplicates and item presentation passed.");
    hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

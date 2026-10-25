/* Local game assets only. Fixtures stay in this driver, outside the runtime. */
#include "scene_preview.c"
#include "sdl_host.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Shipping playthrough line %d: %s; %s\n",__LINE__,#x,error);goto fail; } } while(0)
static bool step(HmbtnScenePreview *s,uint32_t pressed,char *error,size_t size)
{
    s->pending_pressed=pressed;hmbtn_scene_data_tick(&s->scene);
    const bool ok=opening_tick(s,0,error,size);s->pending_pressed=0;return ok;
}
static bool box_position(const HmbtnModel *model,int32_t point[3])
{
    for(uint32_t i=0;i<model->object_count;++i) {
        HmbtnModelObject object;if(!hmbtn_model_object(model,i,&object)) return false;
        size_t at=12+(size_t)object.primitives;
        for(uint32_t j=0;j<object.primitive_count;++j) {
            HmbtnModelPrimitive p;if(!hmbtn_model_primitive(model,at,&p)) return false;
            if(p.flags&0x80u) {
                memset(point,0,3*sizeof(*point));
                for(unsigned k=0;k<p.count;++k) {
                    int32_t vertex[3];hmbtn_model_vertex(model,&object,p.vertex[k],vertex);
                    for(unsigned axis=0;axis<3;++axis) point[axis]+=vertex[axis];
                }
                for(unsigned axis=0;axis<3;++axis) point[axis]/=(int32_t)p.count;
                HmbtnGroundHit hit;
                if(hmbtn_ground_find(model,point,0,4,&hit) && (hit.flags&0x80u)) return true;
            }
            at+=p.bytes;
        }
    }
    return false;
}
int main(int argc,char **argv)
{
    if(argc<2 || argc>3) { fputs("Usage: shipping_playthrough data-directory [capture-directory]\n",stderr);return 2; }
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};
    CHECK(hmbtn_host_open(&host));s=hmbtn_scene_house_create(host.renderer,argv[1],error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<130;++i) CHECK(step(s,0,error,sizeof(error)));
    s->first_visit_started=true;
    for(unsigned season=0;season<4;++season) {
        s->session->game.clock=(HmbtnFarmClock){0,season,1,1,6,0,0};
        CHECK(load_area(s,s->data_root,0,error,sizeof(error)));s->intro.area=0;
        int32_t box[3];CHECK(box_position(&s->scene.terrain,box));
        printf("Shipping surface season %u: (%d,%d,%d)\n",season,box[0],box[1],box[2]);
        /* Start far enough from the lip for the actual throw to land in it. */
        int y=-300,v=0;unsigned flight=0;
        do { y+=v;v+=9;if(v>45) v=45;++flight; } while(y<box[1] && flight<100);
        s->intro.player.position[0]=box[0]-(int32_t)flight*44;
        s->intro.player.position[1]=0;s->intro.player.position[2]=box[2];
        s->intro.player.facing=3;s->intro.player.sprite=(HmbtnAnimationPlayer){0,3,0};
        memcpy(s->intro.camera,s->intro.player.position,sizeof(s->intro.camera));
        s->house.inventory->held_item=(HmbtnStoredTool){0x41,0,1}; /* Test fixture, turnip 60G. */
        const unsigned gold=s->session->game.economy.gold,count=s->session->game.economy.shipments[0];
        const uint32_t pending=s->session->game.economy.shipping.pending_value;
        CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && s->carry.phase>=2 && s->house.inventory->held_item.id==0x41);
        for(unsigned i=0;i<100 && s->carry.phase;++i) CHECK(step(s,0,error,sizeof(error)));
        CHECK(!s->carry.phase && !s->house.inventory->held_item.id && s->carry.thrown[0].item.id==0x41);
        for(unsigned i=0;i<100 && s->carry.thrown[0].item.id;++i) CHECK(step(s,0,error,sizeof(error)));
        CHECK(!s->carry.thrown[0].item.id && s->session->game.economy.shipping.pending_value==pending+60);
        CHECK(s->session->game.economy.shipments[0]==count+1 && s->session->game.economy.gold==gold && !s->carry.discarded);
        s->session->game.clock.hour=17;s->session->game.clock.minute=57;s->session->game.clock.frames=14;
        CHECK(step(s,0,error,sizeof(error)));
        CHECK(s->session->game.economy.shipping.settled && !s->session->game.economy.shipping.pending_value && s->session->game.economy.gold==gold+pending+60);
        CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)) && s->menu.mode==HMBTN_MENU_STATUS);
        for(unsigned i=0;i<30;++i) CHECK(step(s,0,error,sizeof(error)));
        CHECK(hmbtn_scene_preview_draw(s,host.renderer,0,0,0,error,sizeof(error)));
        if(argc==3 && season==0) {
            char path[1024];snprintf(path,sizeof(path),"%s/shipping-accounts.bmp",argv[2]);
            SDL_Surface *p=SDL_RenderReadPixels(host.renderer,NULL);CHECK(p);
            const bool saved=SDL_SaveBMP(p,path);SDL_DestroySurface(p);CHECK(saved);
        }
        CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)));
        for(unsigned i=0;i<30;++i) CHECK(step(s,0,error,sizeof(error)));
        CHECK(s->menu.mode==HMBTN_MENU_NONE);
        s->session->game.economy.shipping.settled=false;
    }
    /* A sleep before 17:58 must collect in chronological order. */
    s->session->game.clock=(HmbtnFarmClock){0,0,1,1,16,0,0};
    CHECK(hmbtn_farm_shipping_add(hmbtn_game_economy(&s->session->game),0x41));
    const unsigned gold=s->session->game.economy.gold;
    CHECK(load_area(s,s->data_root,80,error,sizeof(error)));s->intro.area=80;
    s->house.mode=HMBTN_HOUSE_SLEEP;s->farm_action.fatigue=0;
    for(unsigned i=0;i<1000 && (s->session->game.clock.day==1 || s->morning_fade);++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(s->session->game.clock.day==2 && s->session->game.clock.hour==6 && !s->session->game.economy.shipping.settled && !s->session->game.economy.shipping.pending_value);
    CHECK(s->session->game.economy.gold==gold+60 && s->control.enabled);
    puts("Original shipping surfaces in all seasons, timed throw, accounts and sleep collection passed.");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

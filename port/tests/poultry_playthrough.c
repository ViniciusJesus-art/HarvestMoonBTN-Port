/* Local-resource economy: forage -> shipping -> shop/name -> coop -> eggs.
 * Fixtures select positions and clock only; no money, stock, hens or eggs granted. */
#include "scene_preview.c"
#include "sdl_host.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Feed shop integration line %d area %u: %s; %s\n",__LINE__,s ? s->area : 999,#x,error);goto fail; } } while(0)
#define REQUIRE(x) do { if(!(x)) { fprintf(stderr,"Feed shop helper line %d area %u: %s; %s\n",__LINE__,s->area,#x,error);return false; } } while(0)
static bool step(HmbtnScenePreview *s,uint32_t pressed,char *error,size_t size)
{
    hmbtn_scene_data_tick(&s->scene);s->pending_pressed=pressed;
    const bool ok=opening_tick(s,pressed,error,size);s->pending_pressed=0;return ok;
}
static int half(const uint8_t *p)
{
    unsigned n=hmbtn_read_le16(p);return n<32768 ? (int)n : (int)n-65536;
}
static void at(HmbtnScenePreview *s,const uint8_t *z)
{
    for(unsigned a=0;a<3;++a) s->intro.player.position[a]=(half(z+14+a*2)+half(z+20+a*2))/2;
    s->intro.player.facing=hmbtn_read_le16(z+8)&3u;s->control.mode=0;
}
static bool travel(HmbtnScenePreview *s,unsigned area,char *error,size_t size)
{
    const uint8_t *zone=NULL;bool door=false;
    for(unsigned i=0;i<50;++i) {
        const uint8_t *z=s->section+16+i*44;
        if(hmbtn_read_le16(z+2)!=area) continue;
        at(s,z);HmbtnHouseExit e;unsigned model;
        if(hmbtn_world_door_at(s->section,0x2000,&s->intro.player,&e,&model)) { zone=z;door=true;break; }
        if(hmbtn_world_gate_at(s->section,0x2000,s->intro.player.position,&e)
                || hmbtn_house_exit_at(s->section,0x2000,s->intro.player.position,&e)) { zone=z;break; }
    }
    REQUIRE(zone);REQUIRE(step(s,door ? HMBTN_PAD_CROSS : 0,error,size));
    REQUIRE(s->door_tick || s->travel_tick);
    for(unsigned i=0;i<120 && (s->area!=area || s->travel_tick || s->door_tick);++i) REQUIRE(step(s,0,error,size));
    REQUIRE(s->area==area && !s->control.exit_pending);return true;
}
static bool shop_wait(HmbtnScenePreview *s,HmbtnFeedShopPhase phase,char *error,size_t size)
{
    for(unsigned i=0;i<3000;++i) {
        if(s->feed_shop.phase==phase && !s->feed_shop.closing
                && (phase==HMBTN_SHOP_OFF || phase==HMBTN_SHOP_NAME || s->feed_shop.message.phase==HMBTN_MESSAGE_WAIT_END)) return true;
        REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
    }
    return false;
}
static bool feed(HmbtnScenePreview *s,unsigned kind,unsigned bin,char *error,size_t size)
{
    const uint8_t *zone=NULL;
    for(unsigned i=0;i<50;++i) {
        const uint8_t *z=s->section+16+44*i;
        if(hmbtn_read_le16(z)==0x4000 && hmbtn_read_le16(z+2)==kind && hmbtn_read_le16(z+4)==bin) { zone=z;break; }
    }
    REQUIRE(zone);at(s,zone);
    unsigned facing=hmbtn_read_le16(zone+8);if(facing==999 || facing==6) facing=3;if(facing==7) facing=2;
    s->intro.player.facing=facing;s->intro.player.position[facing&1u ? 0 : 2]+=facing<2 ? 224 : -224;
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && s->feed.phase);
    for(unsigned i=0;i<100 && s->feed.phase;++i) REQUIRE(step(s,0,error,size));
    REQUIRE(!s->feed.phase);return true;
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
static bool idle(HmbtnScenePreview *s,char *error,size_t size)
{
    for(unsigned i=0;i<150 && (s->carry.phase || s->quick_ticks);++i) REQUIRE(step(s,0,error,size));
    REQUIRE(!s->carry.phase && !s->quick_ticks);return true;
}
static bool ship(HmbtnScenePreview *s,char *error,size_t size)
{
    REQUIRE(s->area==0 && s->house.held_item.id);int32_t box[3];REQUIRE(box_position(&s->scene.terrain,box));
    const unsigned price=(unsigned)hmbtn_farm_item(s->house.held_item.id)->sell_price;
    const unsigned before=s->shipping.pending_value;
    int y=-300,v=0;unsigned flight=0;
    do { y+=v;v+=9;if(v>45) v=45;++flight; } while(y<box[1] && flight<100);
    s->intro.player.position[0]=box[0]-(int32_t)flight*44;s->intro.player.position[1]=0;s->intro.player.position[2]=box[2];
    s->intro.player.facing=3;s->control.mode=0;
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));REQUIRE(idle(s,error,size));
    for(unsigned i=0;i<100 && s->carry.thrown[0].item.id;++i) REQUIRE(step(s,0,error,size));
    REQUIRE(!s->carry.thrown[0].item.id && s->shipping.pending_value==before+price);return true;
}
static bool sleep_day(HmbtnScenePreview *s,char *error,size_t size)
{
    REQUIRE(s->area==0 && travel(s,80,error,size));const unsigned day=s->clock.day;
    s->house.mode=HMBTN_HOUSE_SLEEP;
    for(unsigned i=0;i<1200 && (s->clock.day==day || s->morning_fade);++i) REQUIRE(step(s,0,error,size));
    REQUIRE(s->clock.day!=day && s->clock.hour==6 && !s->morning_fade && s->area==80);
    REQUIRE(travel(s,0,error,size));return true;
}
static bool merchant(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnIntroActor *a=&s->village.people[18].actor;REQUIRE(a->visible);
    memcpy(s->intro.player.position,a->position,sizeof(a->position));s->intro.player.position[2]+=350;
    s->intro.player.facing=0;s->control.mode=0;
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));REQUIRE(shop_wait(s,HMBTN_SHOP_SELECT,error,size));return true;
}
static bool editor_wait(HmbtnScenePreview *s,char *error,size_t size)
{
    for(unsigned i=0;i<1000;++i) {
        HmbtnNewGame *e=&s->feed_shop.editor;
        if(e->message.phase==HMBTN_MESSAGE_WAIT_END && !e->queued) return true;
        REQUIRE(step(s,0,error,size));
    }
    return false;
}
static bool capture(HmbtnScenePreview *s,HmbtnHost *host,const char *dir,const char *name,char *error,size_t size)
{
    s->centre_x=s->intro.camera[0];s->centre_z=s->intro.camera[2];
    s->dirty=true;REQUIRE(hmbtn_scene_preview_draw(s,host->renderer,0,0,0,error,size));
    if(dir) {
        char path[1024];REQUIRE(snprintf(path,sizeof(path),"%s/%s.bmp",dir,name)<(int)sizeof(path));
        SDL_Surface *p=SDL_RenderReadPixels(host->renderer,NULL);REQUIRE(p);
        bool ok=SDL_SaveBMP(p,path);SDL_DestroySurface(p);REQUIRE(ok);
    }
    return true;
}
int main(int argc,char **argv)
{
    if(argc<2 || argc>3) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};const char *shots=argc==3 ? argv[2] : NULL;
    CHECK(hmbtn_host_open(&host));s=hmbtn_scene_area_create(host.renderer,argv[1],0,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    s->clock=(HmbtnFarmClock){0,0,2,2,6,0,0};sync_clock(s);
    CHECK(s->menu.stats.gold==500 && !hmbtn_poultry_count(&s->poultry));
    /* Seven real daily trips, three original spring bamboo records per trip. */
    for(unsigned trip=0;trip<7;++trip) {
        const unsigned pending=s->shipping.pending_value;
        CHECK(travel(s,72,error,sizeof(error)));
        CHECK(hmbtn_read_le16(s->section+2)==12);
        for(unsigned i=146;i<=148;++i) {
            const HmbtnGatherRecord *r=hmbtn_farm_gather_record(i);
            memcpy(s->intro.player.position,r->position,sizeof(r->position));s->intro.player.position[2]+=224;
            s->intro.player.facing=0;s->control.mode=0;
            CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && s->house.held_item.id==r->item && s->gather.taken[i]);
            CHECK(idle(s,error,sizeof(error)));
            if(i<148) { CHECK(step(s,HMBTN_PAD_R2,error,sizeof(error)));CHECK(idle(s,error,sizeof(error)));CHECK(!s->house.held_item.id); }
        }
        CHECK(capture(s,&host,shots,"forage-collected",error,sizeof(error)));
        CHECK(travel(s,0,error,sizeof(error)));
        for(unsigned i=0;i<3;++i) {
            if(i) { CHECK(step(s,HMBTN_PAD_R2,error,sizeof(error)));CHECK(idle(s,error,sizeof(error))); }
            CHECK(ship(s,error,sizeof(error)));
        }
        CHECK(!s->carry.discarded && s->shipping.pending_value==pending+150);
        s->clock.hour=16;s->clock.minute=0;sync_clock(s);
        CHECK(sleep_day(s,error,sizeof(error)));
        CHECK(s->menu.stats.gold+s->shipping.pending_value==500+150*(trip+1));
    }
    CHECK(s->menu.stats.gold==1550 && s->menu.stats.ledger[0][0]==1050);
    s->clock.hour=12;sync_clock(s);
    CHECK(travel(s,32,error,sizeof(error)) && travel(s,24,error,sizeof(error)) && travel(s,128,error,sizeof(error)));
    CHECK(merchant(s,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && s->feed_shop.item==0x109);
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_NAME,error,sizeof(error)));
    CHECK(editor_wait(s,error,sizeof(error)));
    CHECK(capture(s,&host,shots,"chicken-name",error,sizeof(error)));
    /* Cancel empty name: no chicken, no debit; return to the same menu. */
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(s->menu.stats.gold==1550 && !hmbtn_poultry_count(&s->poultry));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_NAME,error,sizeof(error)));
    CHECK(editor_wait(s,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && s->feed_shop.editor.length==1);
    CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)));CHECK(editor_wait(s,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(s->menu.stats.gold==50 && s->menu.stats.animals[0]==1 && s->poultry.hens[0].name[0]==0);
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)));
    CHECK(s->feed_shop.request==0x4F && hmbtn_poultry_count(&s->poultry)==1);
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_UP,error,sizeof(error)));CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_COUNT,error,sizeof(error)));CHECK(step(s,HMBTN_PAD_RIGHT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(s->house.supplies[2]==2 && s->menu.stats.gold==30 && s->menu.stats.ledger[0][1]==1520);
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_OFF,error,sizeof(error)));
    CHECK(travel(s,24,error,sizeof(error)) && travel(s,32,error,sizeof(error)) && travel(s,0,error,sizeof(error)));
    for(unsigned night=0;night<2;++night) {
        CHECK(travel(s,112,error,sizeof(error)));
        if(!night) {
            HmbtnIntroActor before=s->poultry.hens[0].actor;
            bool moved=false;
            for(unsigned tick=0;tick<600;++tick) {
                CHECK(step(s,0,error,sizeof(error)));
                const HmbtnIntroActor *hen=&s->poultry.hens[0].actor;HmbtnGroundHit hit;
                CHECK(hmbtn_ground_find(&s->scene.terrain,hen->position,0xE080,64,&hit) && hit.height==hen->position[1]);
                if(memcmp(before.position,hen->position,sizeof(before.position))) moved=true;
            }
            CHECK(moved);
            CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)) && s->menu.mode==HMBTN_MENU_STATUS);
            before=s->poultry.hens[0].actor;
            for(unsigned tick=0;tick<40;++tick) CHECK(step(s,0,error,sizeof(error)));
            CHECK(memcmp(&before,&s->poultry.hens[0].actor,sizeof(before))==0);
            CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)));
            for(unsigned tick=0;tick<30;++tick) CHECK(step(s,0,error,sizeof(error)));
            s->clock.hour=21;CHECK(step(s,0,error,sizeof(error)));
            CHECK(s->poultry.hens[0].actor.sprite.animation==18);
            CHECK(capture(s,&host,shots,"chicken-sleeping",error,sizeof(error)));
            s->clock.hour=12;
        }
        CHECK(feed(s,26,0,error,sizeof(error)) && feed(s,40,0,error,sizeof(error)));
        CHECK(s->feed.coop[0] && !s->house.held_item.id);
        memcpy(s->intro.camera,s->poultry.hens[0].actor.position,sizeof(s->intro.camera));
        CHECK(capture(s,&host,shots,"chicken-fed",error,sizeof(error)));
        CHECK(travel(s,0,error,sizeof(error)) && sleep_day(s,error,sizeof(error)));
        CHECK(!s->feed.coop[0] && s->poultry.hens[0].affection==20+10*night);
        CHECK(s->poultry.eggs[0].item==(night ? 0x122u : 0u));
    }
    CHECK(travel(s,112,error,sizeof(error)));
    HmbtnCoopEgg *egg=&s->poultry.eggs[0];memcpy(s->intro.player.position,egg->position,sizeof(egg->position));
    s->intro.player.position[2]+=224;s->intro.player.facing=0;s->control.mode=0;
    memcpy(s->intro.camera,s->intro.player.position,sizeof(s->intro.camera));
    CHECK(capture(s,&host,shots,"chicken-egg",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && s->house.held_item.id==0x122 && !egg->item);
    CHECK(idle(s,error,sizeof(error)));
    CHECK(travel(s,0,error,sizeof(error)) && ship(s,error,sizeof(error)));
    CHECK(s->shipping.pending_value==50 && s->menu.stats.shipments[15]==1);
    s->clock.hour=17;s->clock.minute=57;s->clock.frames=14;CHECK(step(s,0,error,sizeof(error)));
    CHECK(s->menu.stats.gold==80 && s->menu.stats.ledger[0][0]==1100 && !s->carry.discarded);
    /* Every item pack and seasonal forage region imports actual art. */
    for(unsigned season=0;season<4;++season) {
        s->clock.season=season;
        for(unsigned area=64;area<=76;area+=4) {
            CHECK(load_area(s,s->data_root,area,error,sizeof(error)));
            CHECK(capture(s,&host,NULL,"season",error,sizeof(error)));
        }
    }
    puts("Original-resource economy: 500G -> seven forage/shipping days -> named chicken + feed -> two nights -> egg pickup/shipping passed.");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

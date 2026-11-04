/* Money/date/positions are fixtures; chicken, feed and eggs are acquired. */
#include "poultry_helpers.h"
#include "field_helpers.h"
static bool house_wait(HmbtnScenePreview *s,HmbtnHouseMode mode,char *error,size_t size)
{
    HmbtnHouseInteraction *h=&hmbtn_scene_preview_session(s)->house;
    for(unsigned i=0;i<2000;++i) {
        if(h->mode==mode && (mode==HMBTN_HOUSE_FREE || (!h->closing && h->message.phase==HMBTN_MESSAGE_WAIT_END))) return true;
        REQUIRE(step(s,h->message.phase==HMBTN_MESSAGE_WAIT_PAGE ? HMBTN_PAD_CROSS : 0,error,size));
    }
    return false;
}
static bool object(HmbtnScenePreview *s,unsigned kind,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<50;++i) {
        const uint8_t *z=hmbtn_scene_preview_resources(s).section+16+i*44;
        if(!(hmbtn_read_le16(z)&0x4000u) || hmbtn_read_le16(z+2)!=kind) continue;
        at(s,z);unsigned face=hmbtn_read_le16(z+8);REQUIRE(face<4);
        g->intro.player.position[face&1 ? 0 : 2]+=face<2 ? 224 : -224;
        memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));
        memcpy(g->intro.camera,g->intro.player.position,sizeof(g->intro.camera));
        return step(s,HMBTN_PAD_CROSS,error,size);
    }
    return false;
}
static bool sleep_bed(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);REQUIRE(g->area==0 && travel(s,80,error,size));
    REQUIRE(object(s,2,error,size) && house_wait(s,HMBTN_HOUSE_BED,error,size));
    const uint64_t date=g->daily_date;REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
    for(unsigned i=0;i<1600 && (g->daily_date==date || g->morning_fade || g->house.mode!=HMBTN_HOUSE_FREE);++i) REQUIRE(step(s,0,error,size));
    REQUIRE(g->daily_date==date+1 && !g->morning_fade && travel(s,0,error,size));return true;
}
static bool checkpoint(HmbtnScenePreview **s,HmbtnHost *host,const char *root,const char *base,const char *suffix,char *error,size_t size)
{ char path[1024];if(snprintf(path,sizeof(path),"%s%s",base,suffix)>=(int)sizeof(path)) return false;return field_reload(s,host,root,path,error,size); }
static bool collect(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);REQUIRE(travel(s,112,error,size));
    HmbtnCoopEgg *egg=&g->poultry.eggs[0];REQUIRE(egg->item==0x122 && !g->game.inventory.held_item.id);
    memcpy(g->intro.player.position,egg->position,sizeof(egg->position));g->intro.player.position[2]+=224;
    g->intro.player.facing=0;g->control.mode=0;memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->game.inventory.held_item.id==0x122 && !egg->item);
    REQUIRE(idle(s,error,size));return true;
}
static bool launch_point(HmbtnScenePreview *s,bool spring)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const HmbtnModel *m=&hmbtn_scene_preview_resources(s).scene->terrain;
    const uint8_t *section=hmbtn_scene_preview_resources(s).section,*z=NULL;
    for(unsigned i=0;i<50;++i) if(hmbtn_read_le16(section+18+i*44)==38) { z=section+16+i*44;break; }
    if(!z) return false;
    const int cx=(half(z+14)+half(z+20))/2,cz=(half(z+18)+half(z+24))/2,cy=half(z+16);
    for(int x=-1800;x<=1800;x+=120) for(int zz=-1800;zz<=1800;zz+=120) {
        int32_t dry[3]={cx+x,cy,cz+zz};HmbtnGroundHit hit;
        if(!hmbtn_ground_find(m,dry,0xE0C7,1720,&hit) || (hit.flags&0xE0C7u)) continue;
        dry[1]=hit.height;
        for(unsigned face=0;face<4;++face) {
            HmbtnFarmCarry trial={.area=g->area};trial.thrown[0]=(HmbtnThrownItem){.item={0x122,0,1},
                .position={dry[0],dry[1]-300,dry[2]},.ground=dry[1],.facing=face,.area=g->area};
            for(unsigned tick=0;tick<100 && !trial.thrown[0].phase;++tick)
                hmbtn_farm_carry_flight(&trial,NULL,m,NULL,section,0x2000);
            if(trial.thrown[0].phase!=(spring ? HMBTN_THROWN_EGG_WATER : 1)) continue;
            memcpy(g->intro.player.position,dry,sizeof(dry));memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));
            g->intro.player.facing=face;g->control.mode=0;memcpy(g->intro.camera,dry,sizeof(dry));return true;
        }
    }
    return false;
}
static bool toss(HmbtnScenePreview *s,HmbtnHost *host,const char *shots,bool spring,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);REQUIRE(g->game.inventory.held_item.id==0x122 && launch_point(s,spring));
    const unsigned discarded=g->carry.discarded;bool splash=false,show=false;
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->carry.phase);
    REQUIRE(!hmbtn_save_available(g,error,size));
    for(unsigned tick=0;tick<500 && (g->carry.phase || g->carry.thrown[0].item.id || g->event_item.phase);++tick) {
        REQUIRE(step(s,0,error,size));
        if(g->carry.thrown[0].item.id && g->carry.thrown[0].phase==HMBTN_THROWN_EGG_WATER) {
            REQUIRE(spring && !g->game.inventory.held_item.id && !g->event_item.phase && !hmbtn_save_available(g,error,size));
            if(!splash) REQUIRE(capture(s,host,shots,"spa-splash",error,size));
            splash=true;
        }
        if(g->event_item.phase) {
            REQUIRE(spring && splash && g->event_item.phase==HMBTN_EVENT_ITEM_SHOW && g->event_item.shown==0x87
                && !g->game.inventory.held_item.id && !g->carry.thrown[0].item.id && !hmbtn_save_available(g,error,size));
            if(!show) REQUIRE(capture(s,host,shots,"spa-receive",error,size));
            show=true;
        }
    }
    REQUIRE(!g->carry.phase && !g->carry.thrown[0].item.id && !g->event_item.phase);
    REQUIRE(spring ? splash && show && g->game.inventory.held_item.id==0x87 && g->carry.discarded==discarded
        : !splash && !show && !g->game.inventory.held_item.id && g->carry.discarded==discarded+1);
    for(unsigned tick=0;tick<30;++tick) REQUIRE(step(s,0,error,size));
    REQUIRE(spring ? g->game.inventory.held_item.id==0x87 : !g->game.inventory.held_item.id);return true;
}
static bool consume(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->farm_action.stamina=40;g->farm_action.fatigue=10;
    REQUIRE(step(s,HMBTN_PAD_SQUARE,error,size) && hmbtn_event_item_consuming(&g->event_item));
    for(unsigned i=0;i<600 && g->event_item.phase;++i) REQUIRE(step(s,0,error,size));
    REQUIRE(!g->event_item.phase && !g->game.inventory.held_item.id && g->farm_action.stamina==44 && g->farm_action.fatigue==9);return true;
}
int main(int argc,char **argv)
{
    if(argc<3 || argc>4) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};const char *shots=argc==4 ? argv[3] : NULL;
    CHECK(hmbtn_host_open(&host));const HmbtnFarmClock date={0,0,4,4,12,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&date,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->game.economy.gold=3000;
    CHECK(!hmbtn_poultry_count(&g->poultry));
    CHECK(travel(s,32,error,sizeof(error)) && travel(s,24,error,sizeof(error)) && travel(s,128,error,sizeof(error)));
    CHECK(merchant(s,error,sizeof(error)) && step(s,HMBTN_PAD_DOWN,error,sizeof(error)) && g->feed_shop.item==0x109);
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_NAME,error,sizeof(error)) && editor_wait(s,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && step(s,HMBTN_PAD_START,error,sizeof(error)) && editor_wait(s,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(hmbtn_poultry_count(&g->poultry)==1 && g->game.economy.gold==1500);
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_OFF,error,sizeof(error)));
    CHECK(merchant(s,error,sizeof(error)) && step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_COUNT,error,sizeof(error)));
    for(unsigned i=0;i<5;++i) CHECK(step(s,HMBTN_PAD_RIGHT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(g->game.inventory.supplies[2]==6 && g->game.economy.gold==1440);
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_OFF,error,sizeof(error)));
    CHECK(travel(s,24,error,sizeof(error)) && travel(s,32,error,sizeof(error)) && travel(s,0,error,sizeof(error)));
    for(unsigned night=0;night<4;++night) {
        CHECK(travel(s,112,error,sizeof(error)) && feed(s,26,0,error,sizeof(error)) && feed(s,40,0,error,sizeof(error)));
        CHECK(travel(s,0,error,sizeof(error)) && sleep_bed(s,error,sizeof(error)));
        if(night) {
            CHECK(collect(s,error,sizeof(error)));
            if(night<3) CHECK(step(s,HMBTN_PAD_R2,error,sizeof(error)) && idle(s,error,sizeof(error)) && !g->game.inventory.held_item.id);
            CHECK(travel(s,0,error,sizeof(error)));
        }
    }
    CHECK(g->game.inventory.held_item.id==0x122 && g->game.inventory.items[0].id==0x122 && g->game.inventory.items[1].id==0x122);
    CHECK(checkpoint(&s,&host,argv[1],argv[2],".raw",error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(travel(s,72,error,sizeof(error)) && toss(s,&host,shots,true,error,sizeof(error)));
    CHECK(g->game.inventory.items[0].id==0x122 && g->game.inventory.items[1].id==0x122);
    CHECK(checkpoint(&s,&host,argv[1],argv[2],".cooked",error,sizeof(error)));g=hmbtn_scene_preview_session(s);
    CHECK(consume(s,error,sizeof(error)) && step(s,HMBTN_PAD_R2,error,sizeof(error)) && idle(s,error,sizeof(error)));
    CHECK(g->game.inventory.held_item.id==0x122 && toss(s,&host,shots,true,error,sizeof(error)));
    CHECK(travel(s,0,error,sizeof(error)) && ship(s,error,sizeof(error)) && g->game.economy.shipping.pending_value==80);
    CHECK(step(s,HMBTN_PAD_R2,error,sizeof(error)) && idle(s,error,sizeof(error)) && g->game.inventory.held_item.id==0x122);
    CHECK(travel(s,72,error,sizeof(error)) && toss(s,&host,shots,false,error,sizeof(error)) && travel(s,0,error,sizeof(error)));
    g->game.clock.hour=16;g->game.clock.minute=0;CHECK(sleep_bed(s,error,sizeof(error)));
    fprintf(stderr,"Spa payment: gold=%u pending=%u ledger=%u\n",g->game.economy.gold,g->game.economy.shipping.pending_value,g->game.economy.ledger[0][0]);
    CHECK(g->game.economy.gold==1520 && !g->game.economy.shipping.pending_value && g->game.economy.ledger[0][0]==80);
    CHECK(!g->game.inventory.items[0].id && !g->game.inventory.items[1].id && g->carry.discarded==1);
    CHECK(checkpoint(&s,&host,argv[1],argv[2],"",error,sizeof(error)));
    puts("Hot spring route: bought/fed hen, three produced eggs, full bag, two conversions, consumption, 80 G payment, dry miss and three checkpoints passed");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

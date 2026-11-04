/* Shared installed pond route helpers; no gameplay item grants. */
#ifndef HMBTN_TEST_POND_HELPERS_H
#define HMBTN_TEST_POND_HELPERS_H
#include "poultry_helpers.h"
#include "field_helpers.h"
static bool cast_point_near(HmbtnScenePreview *s,const int32_t g[3],int32_t position[3],unsigned *facing)
{
    static const int reach[4][2]={{0,-783},{-783,0},{0,783},{783,0}};
    for(int x=-2400;x<=2400;x+=200) for(int z=-2400;z<=2400;z+=200) {
        int32_t p[3]={g[0]+x,g[1],g[2]+z};HmbtnGroundHit dry,wet;
        if(!hmbtn_ground_find(&(*hmbtn_scene_preview_resources(s).scene).terrain,p,0xE087,1720,&dry)) continue;
        p[1]=dry.height;
        for(unsigned face=0;face<4;++face) {
            int32_t ahead[3]={p[0]+reach[face][0],p[1],p[2]+reach[face][1]};
            if(!hmbtn_ground_find(&(*hmbtn_scene_preview_resources(s).scene).terrain,ahead,0,1720,&wet) || !(wet.flags&2u) || (wet.flags&0x44u)) continue;
            HmbtnFishing f={0};HmbtnIntroActor a={0};memcpy(a.position,p,sizeof(p));a.facing=face;
            HmbtnHouseInteraction h;HmbtnGameState h_state;hmbtn_game_state_init(&h_state);hmbtn_house_interaction_init(&h,&h_state);h.inventory->equipped.id=56;
            HmbtnFarmAction status;hmbtn_farm_action_init(&status);bool flags[512]={0};
            hmbtn_fishing_tick(&f,&a,&h,&status,&(*hmbtn_scene_preview_resources(s).scene).terrain,&hmbtn_scene_preview_session(s)->game.clock,44,flags,HMBTN_PAD_SQUARE,HMBTN_PAD_SQUARE);
            for(unsigned i=0;i<200 && f.phase==HMBTN_FISHING_CAST;++i) {
                a.animation_done=true;hmbtn_fishing_tick(&f,&a,&h,&status,&(*hmbtn_scene_preview_resources(s).scene).terrain,&hmbtn_scene_preview_session(s)->game.clock,44,flags,0,HMBTN_PAD_SQUARE);
            }
            if(f.phase==HMBTN_FISHING_WAIT) { memcpy(position,p,sizeof(p));*facing=face;return true; }
        }
    }
    return false;
}
static bool cast_point(HmbtnScenePreview *s,int32_t position[3],unsigned *facing)
{ return cast_point_near(s,hmbtn_scene_preview_session(s)->village.people[32].actor.position,position,facing); }
static bool held(HmbtnScenePreview *s,uint32_t pressed,uint32_t buttons,char *error,size_t size)
{ return hmbtn_scene_preview_tick(s,(HmbtnLogicalInput){pressed,buttons},error,size); }
static bool wait_items(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned t=0;t<400 && (g->carry.phase || g->feed.phase || g->carry.thrown[0].item.id || g->quick_ticks);++t) REQUIRE(step(s,0,error,size));
    REQUIRE(!g->carry.phase && !g->feed.phase && !g->carry.thrown[0].item.id && !g->quick_ticks);return true;
}
static bool close_shop(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned t=0;t<3000 && (g->feed_shop.phase || g->event_item.phase);++t) REQUIRE(step(s,g->event_item.phase ? 0 : HMBTN_PAD_CROSS,error,size));
    REQUIRE(!g->feed_shop.phase && !g->event_item.phase && !g->game.inventory.held_item.id);return true;
}
static bool counter(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const HmbtnGatherRecord *record=NULL;
    for(unsigned i=0;i<HMBTN_GATHER_RECORDS;++i) { const HmbtnGatherRecord *r=hmbtn_farm_gather_record(i);if(r->location==30 && r->item==132) { record=r;break; } }
    REQUIRE(record && g->village.people[1].actor.visible);
    memcpy(g->intro.player.position,record->position,sizeof(record->position));g->intro.player.position[0]-=400;g->intro.player.position[1]=0;g->intro.player.facing=3;g->control.mode=0;
    memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));memcpy(g->intro.camera,g->intro.player.position,sizeof(g->intro.camera));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->feed_shop.fish_food && g->event_item.phase==HMBTN_EVENT_ITEM_SHOW);
    REQUIRE(shop_wait(s,HMBTN_SHOP_GREETING,error,size) && g->feed_shop.request==0xd0 && g->game.inventory.held_item.id==108);
    REQUIRE(shop_wait(s,HMBTN_SHOP_COUNT,error,size));return true;
}
static bool object(HmbtnScenePreview *s,unsigned kind,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    for(unsigned i=0;i<50;++i) {
        const uint8_t *z=hmbtn_scene_preview_resources(s).section+16+i*44;
        if(!(hmbtn_read_le16(z)&0x4000u) || hmbtn_read_le16(z+2)!=kind) continue;
        at(s,z);const unsigned face=hmbtn_read_le16(z+8);REQUIRE(face<4);
        g->intro.player.position[face&1 ? 0 : 2]+=face<2 ? 224 : -224;
        memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));memcpy(g->intro.camera,g->intro.player.position,sizeof(g->intro.camera));
        return step(s,HMBTN_PAD_CROSS,error,size) && wait_items(s,error,size);
    }
    return false;
}
static bool pond_throw(HmbtnScenePreview *s,HmbtnHost *host,const char *shots,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const HmbtnModel *m=&hmbtn_scene_preview_resources(s).scene->terrain;
    const unsigned item=g->game.inventory.held_item.id,count=hmbtn_pond_count(&g->pond);REQUIRE(item);
    bool placed=false;
    for(unsigned ob=0;ob<m->object_count && !placed;++ob) {
        HmbtnModelObject o;REQUIRE(hmbtn_model_object(m,ob,&o));size_t at=12+o.primitives;
        for(unsigned n=0;n<o.primitive_count && !placed;++n) {
            HmbtnModelPrimitive p;REQUIRE(hmbtn_model_primitive(m,at,&p));at+=p.bytes;if(!(p.flags&4u)) continue;
            int32_t wet[3]={0};for(unsigned v=0;v<p.count;++v) { int32_t q[3];hmbtn_model_vertex(m,&o,p.vertex[v],q);for(unsigned a=0;a<3;++a) wet[a]+=q[a]; }
            for(unsigned a=0;a<3;++a) wet[a]/=p.count;
            int y=-300,velocity=0;unsigned ticks=0;do { y+=velocity;velocity+=9;if(velocity>45) velocity=45;++ticks; } while(y<wet[1] && ticks<100);
            for(unsigned face=0;face<4 && !placed;++face) {
                int32_t dry[3]={wet[0],0,wet[2]};dry[face&1 ? 0 : 2]+=face<2 ? (int)ticks*44 : -(int)ticks*44;
                HmbtnGroundHit hit;if(!hmbtn_ground_find(m,dry,0xE087,4,&hit) || (hit.flags&0x87u) || hit.height) continue;
                memcpy(g->intro.player.position,dry,sizeof(dry));g->intro.player.facing=face;g->control.mode=0;
                memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));memcpy(g->intro.camera,dry,sizeof(dry));placed=true;
            }
        }
    }
    REQUIRE(placed && step(s,HMBTN_PAD_CROSS,error,size) && g->carry.phase);
    bool splash=false;
    for(unsigned t=0;t<400 && (g->carry.phase || g->carry.thrown[0].item.id);++t) {
        REQUIRE(step(s,0,error,size));
        if(g->carry.thrown[0].phase==3 && g->carry.thrown[0].item.id && !splash) {
            REQUIRE(g->carry.thrown[0].splash.animation==23 && capture(s,host,shots,"pond-splash",error,size));splash=true;
        }
    }
    REQUIRE(splash && !g->game.inventory.held_item.id && !g->carry.thrown[0].item.id);
    REQUIRE(item==132 ? (g->pond.fed && hmbtn_pond_count(&g->pond)==count) : hmbtn_pond_count(&g->pond)==count+1);return true;
}
static bool checkpoint(HmbtnScenePreview **s,HmbtnHost *host,const char *root,const char *base,const char *suffix,char *error,size_t size)
{ char path[1024];if(snprintf(path,sizeof(path),"%s%s",base,suffix)>=(int)sizeof(path)) return false;return field_reload(s,host,root,path,error,size); }
#endif

/* Shared installed-resource tool/field driver; requires poultry_helpers.h. */
#ifndef HMBTN_TEST_FIELD_HELPERS_H
#define HMBTN_TEST_FIELD_HELPERS_H
#include "save_file.h"
static bool tool(HmbtnScenePreview *s,unsigned id,char *error,size_t size)
{
    /* Equip through the shared inventory, as a menu would. Tools are starting
     * toolbox records; the watering can is refilled through its real action. */
    if(hmbtn_scene_preview_session(s)->game.inventory.equipped.id!=id) {
        unsigned box=64;
        for(unsigned i=0;i<64;++i) if(hmbtn_scene_preview_session(s)->game.inventory.toolbox[i].id==id) { box=i;break; }
        REQUIRE(box<64);
        const HmbtnStoredTool old=hmbtn_scene_preview_session(s)->game.inventory.equipped;
        hmbtn_scene_preview_session(s)->game.inventory.equipped=hmbtn_scene_preview_session(s)->game.inventory.toolbox[box];
        hmbtn_scene_preview_session(s)->game.inventory.toolbox[box]=old;
    }
    REQUIRE(step(s,HMBTN_PAD_SQUARE,error,size));
    for(unsigned i=0;i<250 && hmbtn_scene_preview_session(s)->farm_action.phase!=HMBTN_FARM_ACTION_FREE;++i) REQUIRE(step(s,0,error,size));
    REQUIRE(hmbtn_scene_preview_session(s)->farm_action.phase==HMBTN_FARM_ACTION_FREE);return true;
}
static bool field_refill(HmbtnScenePreview *s,char *error,size_t size)
{
    int32_t pond[3]={0};bool found=false;
    for(unsigned ob=0;ob<(*hmbtn_scene_preview_resources(s).scene).terrain.object_count && !found;++ob) {
        HmbtnModelObject object;REQUIRE(hmbtn_model_object(&(*hmbtn_scene_preview_resources(s).scene).terrain,ob,&object));
        size_t off=12+object.primitives;
        for(unsigned k=0;k<object.primitive_count;++k) {
            HmbtnModelPrimitive primitive;REQUIRE(hmbtn_model_primitive(&(*hmbtn_scene_preview_resources(s).scene).terrain,off,&primitive));
            if(primitive.flags&3u) {
                memset(pond,0,sizeof(pond));
                for(unsigned v=0;v<primitive.count;++v) {
                    int32_t point[3];hmbtn_model_vertex(&(*hmbtn_scene_preview_resources(s).scene).terrain,&object,primitive.vertex[v],point);
                    for(unsigned axis=0;axis<3;++axis) pond[axis]+=point[axis];
                }
                for(unsigned axis=0;axis<3;++axis) pond[axis]/=(int32_t)primitive.count;
                HmbtnGroundHit hit;
                found=hmbtn_ground_find(&(*hmbtn_scene_preview_resources(s).scene).terrain,pond,0,200,&hit) && (hit.flags&3u);
                if(found) break;
            }
            off+=primitive.bytes;
        }
    }
    REQUIRE(found);memcpy(hmbtn_scene_preview_session(s)->intro.player.position,pond,sizeof(pond));
    hmbtn_scene_preview_session(s)->intro.player.position[0]-=224;hmbtn_scene_preview_session(s)->intro.player.facing=3;
    REQUIRE(tool(s,21,error,size) && hmbtn_scene_preview_session(s)->game.inventory.equipped.count==100);
    return true;
}
static void cell(HmbtnScenePreview *s,unsigned col,unsigned row)
{
    /* Stand one cell west and face the target; real ground remains installed. */
    hmbtn_scene_preview_session(s)->intro.player.position[0]=(int32_t)(col-1)*400-4200;
    hmbtn_scene_preview_session(s)->intro.player.position[1]=0;hmbtn_scene_preview_session(s)->intro.player.position[2]=1400-(int32_t)row*400;
    hmbtn_scene_preview_session(s)->intro.player.facing=3;hmbtn_scene_preview_session(s)->control.mode=0;
}
static bool field_reload(HmbtnScenePreview **scene,HmbtnHost *host,const char *root,const char *path,char *error,size_t size)
{
    HmbtnScenePreview *s=*scene;
    uint8_t *before=NULL,*after=NULL;size_t nb=0,na=0;
    REQUIRE(hmbtn_save_encode(hmbtn_scene_preview_session(s),&before,&nb,error,size));
    if(!hmbtn_scene_preview_save(s,path,error,size)) { free(before);return false; }
    hmbtn_scene_preview_destroy(s);*scene=NULL;
    s=hmbtn_scene_saved_create(host->renderer,root,path,error,size);*scene=s;
    if(!s) { free(before);return false; }
    bool ok=hmbtn_save_encode(hmbtn_scene_preview_session(s),&after,&na,error,size)
        && nb==na && !memcmp(before,after,nb);
    free(before);free(after);return ok;
}
#endif

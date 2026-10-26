/* Real installed BIN data: table purchase -> seed ANI -> daily growth -> sickle
 * -> save/load -> barn stock/feeding -> regrowth. No parallel gameplay loop. */
#include "poultry_helpers.h"
#include "field_helpers.h"
#include "save_file.h"
static bool terrain_page(HmbtnScenePreview *s,unsigned index,unsigned page)
{
    const HmbtnFarmField *field=&hmbtn_scene_preview_session(s)->field;
    HmbtnSceneResources resources=hmbtn_scene_preview_resources(s);
    return field->active[index] && field->quad[index]!=SIZE_MAX
        && hmbtn_read_le16(resources.scene->terrain.data+field->quad[index]+10)==page;
}
int main(int argc,char **argv)
{
    if(argc<3 || argc>4) return 2;
    const char *shots=argc==4 ? argv[3] : NULL;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};
    CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock start={0,0,2,3,12,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&start,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    /* This twenty-night crop fixture starts after Thomas's ingredient request;
     * that event has its own traversal and would otherwise open on Spring 15. */
    hmbtn_scene_preview_session(s)->village.flags[251+288]=true;
    CHECK(hmbtn_scene_preview_load_area(s,180,error,sizeof(error)));hmbtn_scene_preview_session(s)->intro.area=180;
    CHECK(step(s,0,error,sizeof(error)));
    const HmbtnGatherRecord *bundle=hmbtn_farm_gather_record(112);CHECK(bundle && bundle->item==0x128);
    HmbtnIntroActor *p=&hmbtn_scene_preview_session(s)->intro.player;
    p->position[0]=bundle->position[0];p->position[1]=0;p->position[2]=bundle->position[2]-400;p->facing=2;
    memset(p->velocity,0,sizeof(p->velocity));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->feed_shop.item==0x36);
    const unsigned gold=hmbtn_scene_preview_session(s)->game.economy.gold;
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_COUNT,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->feed_shop.quantity==1 && hmbtn_feed_shop_cost(&hmbtn_scene_preview_session(s)->feed_shop)==500);
    CHECK(capture(s,&host,shots,"grass-purchase",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->game.economy.gold==gold-500
        && hmbtn_scene_preview_session(s)->game.inventory.equipped.id==0x36
        && hmbtn_scene_preview_session(s)->game.inventory.equipped.count==1);
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)) && shop_wait(s,HMBTN_SHOP_OFF,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)));hmbtn_scene_preview_session(s)->intro.area=0;
    const unsigned index=460;
    /* Clear a fixture plot, then till/plant only via installed action ANIs. */
    for(unsigned row=9;row<=11;++row) for(unsigned col=9;col<=11;++col)
        hmbtn_scene_preview_session(s)->field.cells[row*45+col]=(HmbtnFarmTile){.value=0x167,.ground=0x167};
    for(unsigned row=9;row<=11;++row) for(unsigned col=9;col<=11;++col) {
        cell(s,col,row);CHECK(tool(s,6,error,sizeof(error)));
    }
    cell(s,11,10);CHECK(tool(s,0x36,error,sizeof(error)));
    HmbtnInventory *inv=&hmbtn_scene_preview_session(s)->game.inventory;
    CHECK(!inv->equipped.id && !hmbtn_inventory_count(inv,0x36,0,2));
    for(unsigned row=9;row<=11;++row) for(unsigned col=9;col<=11;++col)
        CHECK(hmbtn_scene_preview_session(s)->field.cells[row*45+col].value==0x36);
    CHECK(capture(s,&host,shots,"grass-sown",error,sizeof(error)));
    for(unsigned day=1;day<=11;++day) {
        CHECK(sleep_day(s,error,sizeof(error)));
        const HmbtnFarmTile *tile=&hmbtn_scene_preview_session(s)->field.cells[index];
        CHECK(tile->kind==(day<3 ? 0u : day<7 ? 1u : day<11 ? 2u : 3u));
        CHECK(!hmbtn_scene_preview_session(s)->game.inventory.supplies[0]);
        if(day>=3) CHECK(terrain_page(s,index,29));
        if(day==4) {
            CHECK(tile->watered_days==1);
            CHECK(field_reload(&s,&host,argv[1],argv[2],error,sizeof(error)));
            CHECK(hmbtn_scene_preview_session(s)->field.cells[index].watered_days==1 && terrain_page(s,index,29));
            cell(s,10,10);CHECK(step(s,0,error,sizeof(error)) && capture(s,&host,shots,"grass-growing-loaded",error,sizeof(error)));
        }
    }
    cell(s,10,10);CHECK(step(s,0,error,sizeof(error)) && capture(s,&host,shots,"grass-ripe",error,sizeof(error)));
    CHECK(tool(s,1,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->field.cells[index].kind==1
        && (hmbtn_scene_preview_session(s)->field.cells[index].growth_flags&0x8000)
        && hmbtn_scene_preview_session(s)->game.inventory.supplies[0]==1
        && !hmbtn_scene_preview_session(s)->game.inventory.held_item.id);
    bool effect=false;
    for(unsigned i=0;i<HMBTN_FIELD_EFFECTS;++i) if(hmbtn_scene_preview_session(s)->field_effects[i].active
            && hmbtn_scene_preview_session(s)->field_effects[i].art==HMBTN_FIELD_EFFECT_GRASS) effect=true;
    CHECK(effect && capture(s,&host,shots,"grass-cut",error,sizeof(error)));
    CHECK(tool(s,1,error,sizeof(error)) && hmbtn_scene_preview_session(s)->game.inventory.supplies[0]==1);
    CHECK(field_reload(&s,&host,argv[1],argv[2],error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->game.inventory.supplies[0]==1
        && (hmbtn_scene_preview_session(s)->field.cells[index].growth_flags&0x8000));
    /* Barn exchange and troughs borrow the very stock produced by the cut. */
    CHECK(travel(s,96,error,sizeof(error)) && feed(s,9,0,error,sizeof(error)));
    CHECK(!hmbtn_scene_preview_session(s)->game.inventory.supplies[0]
        && hmbtn_scene_preview_session(s)->game.inventory.held_item.id==0x68);
    CHECK(feed(s,9,0,error,sizeof(error)) && hmbtn_scene_preview_session(s)->game.inventory.supplies[0]==1
        && !hmbtn_scene_preview_session(s)->game.inventory.held_item.id);
    CHECK(feed(s,9,0,error,sizeof(error)) && feed(s,41,0,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->feed.barn[0] && !hmbtn_scene_preview_session(s)->game.inventory.held_item.id
        && !hmbtn_scene_preview_session(s)->game.inventory.supplies[0]);
    CHECK(capture(s,&host,shots,"grass-fodder-trough",error,sizeof(error)));
    CHECK(travel(s,0,error,sizeof(error)));
    for(unsigned day=0;day<9;++day) {
        CHECK(sleep_day(s,error,sizeof(error)));
        const HmbtnFarmTile *tile=&hmbtn_scene_preview_session(s)->field.cells[index];
        if(day==0) CHECK(tile->kind==1 && !tile->watered_days && !(tile->growth_flags&0x8000));
        if(day<8) CHECK(tile->kind<3);
    }
    CHECK(hmbtn_scene_preview_session(s)->field.cells[index].kind==3);
    cell(s,10,10);CHECK(tool(s,1,error,sizeof(error)) && hmbtn_scene_preview_session(s)->game.inventory.supplies[0]==1);
    CHECK(capture(s,&host,shots,"grass-regrown-cut",error,sizeof(error)));
    CHECK(field_reload(&s,&host,argv[1],argv[2],error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->game.inventory.supplies[0]==1
        && hmbtn_scene_preview_session(s)->field.cells[index].kind==1);
    /* Independent seasonal presentation fixtures retain the same grown field.
     * Exercise both original seasonal cut sprites and winter terrain uploads. */
    for(unsigned season=1;season<=3;++season) {
        HmbtnSession *g=hmbtn_scene_preview_session(s);
        g->game.clock=(HmbtnFarmClock){0,season,2,2,12,0,0};
        g->field.cells[index]=(HmbtnFarmTile){.value=0x36,.ground=0x168,.kind=3};
        hmbtn_farm_field_new_day(&g->field,season);
        CHECK(hmbtn_scene_preview_load_area(s,0,error,sizeof(error)));
        g->intro.area=0;cell(s,10,10);CHECK(step(s,0,error,sizeof(error)));
        CHECK(terrain_page(s,index,season==3 ? 7 : 29));
        if(season<3) {
            const unsigned before=g->game.inventory.supplies[0];
            CHECK(tool(s,1,error,sizeof(error)) && g->game.inventory.supplies[0]==before+1);
            CHECK(capture(s,&host,shots,season==1 ? "grass-summer-cut" : "grass-fall-cut",error,sizeof(error)));
        } else {
            CHECK(g->field.cells[index].kind==1 && g->field.cells[index].watered_days==1);
            CHECK(capture(s,&host,shots,"grass-winter",error,sizeof(error)));
        }
    }
    puts("Grass: original table purchase, nine-cell seed ANI, 11 dry nights, native load, terrain stages/cut art, single fodder, shared barn stock/trough and regrowth passed.");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

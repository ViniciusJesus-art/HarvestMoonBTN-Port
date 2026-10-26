#include "poultry_helpers.h"
#include "save_file.h"
#include "title_preview.h"
static bool same_save(HmbtnScenePreview *a,HmbtnScenePreview *b,char *error,size_t size)
{
    uint8_t *x=NULL,*y=NULL;size_t nx=0,ny=0;
    bool ok=hmbtn_save_encode(hmbtn_scene_preview_session(a),&x,&nx,error,size)
        && hmbtn_save_encode(hmbtn_scene_preview_session(b),&y,&ny,error,size) && nx==ny && !memcmp(x,y,nx);
    if(!ok && x && y && nx==ny) for(size_t i=24;i<nx;++i) if(x[i]!=y[i]) {
        fprintf(stderr,"Save comparison differs at %zu: %u/%u (ticks %u/%u, light %u/%u, idle %u/%u)\n",i,x[i],y[i],
            hmbtn_scene_preview_session(a)->runtime.ticks,hmbtn_scene_preview_session(b)->runtime.ticks,
            hmbtn_scene_preview_session(a)->daylight.taken,hmbtn_scene_preview_session(b)->daylight.taken,
            hmbtn_scene_preview_session(a)->house.options.idle_frames,hmbtn_scene_preview_session(b)->house.options.idle_frames);break;
    }
    free(x);free(y);return ok;
}
static bool bed(HmbtnScenePreview *s,unsigned choice,char *error,size_t size)
{
    const uint8_t *section=hmbtn_scene_preview_resources(s).section,*zone=NULL;
    for(unsigned i=0;i<50;++i) {
        const uint8_t *p=section+16+i*44;
        if((hmbtn_read_le16(p)&0x4000u) && hmbtn_read_le16(p+2)==2) { zone=p;break; }
    }
    REQUIRE(zone);at(s,zone);unsigned facing=hmbtn_read_le16(zone+8);REQUIRE(facing<4);
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    g->intro.player.position[facing&1u ? 0 : 2]+=facing<2 ? 224 : -224;
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->house.mode==HMBTN_HOUSE_BED);
    for(unsigned i=0;i<500 && g->house.message.phase!=HMBTN_MESSAGE_WAIT_END;++i) REQUIRE(step(s,0,error,size));
    REQUIRE(g->house.message.phase==HMBTN_MESSAGE_WAIT_END);
    for(unsigned i=0;i<choice;++i) REQUIRE(step(s,HMBTN_PAD_DOWN,error,size));
    return step(s,HMBTN_PAD_CROSS,error,size);
}
int main(int argc,char **argv)
{
    if(argc!=3) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL,*loaded=NULL;HmbtnTitlePreview *title=NULL;
    char error[512]={0},path[2048],alternate[2048];
    CHECK(snprintf(path,sizeof(path),"%s/farm.hms",argv[2])<(int)sizeof(path));
    CHECK(snprintf(alternate,sizeof(alternate),"%s/other.hms",argv[2])<(int)sizeof(alternate));
    CHECK(hmbtn_host_open(&host));
    HmbtnFarmClock date={0,0,2,2,12,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&date,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<120;++i) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    g->game.inventory.rucksack_level=2;g->game.inventory.equipped=(HmbtnStoredTool){34,3,9};
    g->game.inventory.bag[7]=(HmbtnStoredTool){21,8,1};g->game.inventory.toolbox[63]=(HmbtnStoredTool){36,0,99};
    g->game.inventory.supplies[2]=17;g->game.economy.gold=4321;
    g->game.economy.shipping.pending_value=60;g->game.economy.shipping.settled=false;
    g->game.registration.player_name[0]=0x101;g->game.registration.farm_name[0]=0x102;g->game.registration.dog_name[0]=0x103;
    g->field.cells[460]=(HmbtnFarmTile){.value=34,.ground=0x16b};
    g->feed.coop[0]=true;g->feed.barn[21]=true;g->gather.taken[140]=true;
    g->village.flags[205]=g->village.flags[391]=g->village.flags[0x5E]=true;
    g->village.people[21].friendship=123;g->fishing.streak=2;
    const uint16_t name[9]={0x104,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff};
    CHECK(hmbtn_poultry_buy(&g->poultry,name,&g->game.clock));
    CHECK(hmbtn_poultry_incubate(&g->poultry,0,0x122,&g->game.clock)==0);
    g->poultry.hens[0].fed_today=true;g->poultry.eggs[0]=(HmbtnCoopEgg){0x122,{0,0,0},112};
    CHECK(hmbtn_scene_preview_save(s,path,error,sizeof(error)));
    loaded=hmbtn_scene_saved_create(host.renderer,argv[1],path,error,sizeof(error));CHECK(loaded);
    CHECK(same_save(s,loaded,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(loaded)->village.flags[0x5E] && hmbtn_scene_preview_session(loaded)->fishing.streak==2);
    for(unsigned i=0;i<35;++i) {
        CHECK(step(s,0,error,sizeof(error)) && step(loaded,0,error,sizeof(error)));
        CHECK(same_save(s,loaded,error,sizeof(error)));
    }
    hmbtn_scene_preview_destroy(loaded);loaded=NULL;
    /* Resource failure never replaces or mutates the live session. */
    const unsigned gold=g->game.economy.gold;const uint64_t day=g->daily_date;
    loaded=hmbtn_scene_saved_create(host.renderer,"/hmbtn-missing-resources",path,error,sizeof(error));
    CHECK(!loaded && g->game.economy.gold==gold && g->daily_date==day && step(s,0,error,sizeof(error)));error[0]=0;
    /* Both an interior and its schedules/animal art reconstruct from IDs. */
    CHECK(travel(s,112,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_save(s,alternate,error,sizeof(error)));
    loaded=hmbtn_scene_saved_create(host.renderer,argv[1],alternate,error,sizeof(error));CHECK(loaded && same_save(s,loaded,error,sizeof(error)));
    CHECK(step(s,0,error,sizeof(error)) && step(loaded,0,error,sizeof(error)) && same_save(s,loaded,error,sizeof(error)));
    hmbtn_scene_preview_destroy(loaded);loaded=NULL;
    CHECK(travel(s,0,error,sizeof(error)) && travel(s,80,error,sizeof(error)));
    CHECK(bed(s,1,error,sizeof(error)) && g->house.save_requested);
    CHECK(!hmbtn_scene_preview_save(s,path,error,sizeof(error)));error[0]=0; /* active sleep cannot overwrite */
    const unsigned before_day=g->game.clock.day;
    for(unsigned i=0;i<1600 && !hmbtn_scene_preview_save_pending(s);++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(g->game.clock.day==before_day+1 && g->field.cells[460].watered_days==1);
    CHECK(hmbtn_scene_preview_take_save_request(s) && !hmbtn_scene_preview_take_save_request(s));
    CHECK(hmbtn_scene_preview_save(s,path,error,sizeof(error)) && !g->morning_save_requested);
    CHECK(g->game.economy.gold==gold+60 && !g->game.economy.shipping.pending_value);
    loaded=hmbtn_scene_saved_create(host.renderer,argv[1],path,error,sizeof(error));CHECK(loaded && same_save(s,loaded,error,sizeof(error)));
    HmbtnSession *r=hmbtn_scene_preview_session(loaded);
    CHECK(r->area==80 && r->game.clock.hour==6 && r->daily_date==g->daily_date && r->game.inventory.toolbox[63].count==99);
    for(unsigned i=0;i<80;++i) CHECK(step(s,0,error,sizeof(error)) && step(loaded,0,error,sizeof(error)) && same_save(s,loaded,error,sizeof(error)));
    CHECK(r->field.cells[460].watered_days==1 && r->game.economy.gold==gold+60);
    /* Original diary choice requests the same loader, not an overlay stub. */
    CHECK(bed(s,2,error,sizeof(error)) && hmbtn_scene_preview_load_pending(s));
    hmbtn_scene_preview_ack_load(s);CHECK(!hmbtn_scene_preview_load_pending(s));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)));
    /* Continue is produced by original title inputs, and failure can return to menu. */
    title=hmbtn_title_preview_create(host.renderer,argv[1],error,sizeof(error));CHECK(title);
    for(unsigned i=0;i<500 && !hmbtn_title_preview_continue(title);++i) {
        HmbtnTitleInspection view=hmbtn_title_preview_inspect(title);unsigned pressed=0;
        if(view.flow->step==HMBTN_TITLE_INTRO || view.flow->step==HMBTN_TITLE_START_WAIT) pressed=HMBTN_PAD_START;
        else if(view.flow->step==HMBTN_TITLE_MENU) pressed=HMBTN_PAD_CROSS; /* default Continue */
        CHECK(hmbtn_title_preview_tick(title,(HmbtnLogicalInput){pressed,pressed},error,sizeof(error)));
    }
    CHECK(hmbtn_title_preview_continue(title));hmbtn_title_preview_continue_failed(title);
    CHECK(!hmbtn_title_preview_continue(title) && hmbtn_title_preview_inspect(title).flow->step==HMBTN_TITLE_MENU);
    hmbtn_title_preview_destroy(title);title=NULL;
    hmbtn_scene_preview_destroy(s);s=NULL;hmbtn_scene_preview_destroy(loaded);loaded=NULL;
    /* The native app in the process runner also reads this slot after exit. */
    s=hmbtn_scene_saved_create(host.renderer,argv[1],path,error,sizeof(error));CHECK(s);
    CHECK(hmbtn_scene_preview_session(s)->game.economy.gold==gold+60 && step(s,0,error,sizeof(error)));
    hmbtn_scene_preview_destroy(s);s=NULL;
    /* A fresh seasonal region preview may not have visited its farm yet.
     * Preserve that field state too; a resource rebind is not a new season. */
    date=(HmbtnFarmClock){0,1,4,0,10,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],40,&date,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<120;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_save(s,alternate,error,sizeof(error)));
    loaded=hmbtn_scene_saved_create(host.renderer,argv[1],alternate,error,sizeof(error));CHECK(loaded && same_save(s,loaded,error,sizeof(error)));
    hmbtn_scene_preview_destroy(loaded);loaded=NULL;hmbtn_scene_preview_destroy(s);s=NULL;
    date=(HmbtnFarmClock){0,2,13,0,6,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&date,error,sizeof(error));CHECK(s);
    CHECK(step(s,0,error,sizeof(error)) && step(s,0,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_session(s)->visit.active && hmbtn_scene_preview_session(s)->visit.event==140);
    CHECK(!hmbtn_scene_preview_save(s,path,error,sizeof(error)));error[0]=0;
    for(unsigned i=0;i<6000 && hmbtn_scene_preview_session(s)->visit.active;++i)
        CHECK(step(s,hmbtn_scene_preview_session(s)->intro.message ? HMBTN_PAD_CROSS : 0,error,sizeof(error)));
    for(unsigned i=0;i<60;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(hmbtn_scene_preview_save(s,alternate,error,sizeof(error)));
    loaded=hmbtn_scene_saved_create(host.renderer,argv[1],alternate,error,sizeof(error));CHECK(loaded && same_save(s,loaded,error,sizeof(error)));
    r=hmbtn_scene_preview_session(loaded);const unsigned friendship=r->village.people[21].friendship;
    for(unsigned i=0;i<120;++i) CHECK(step(loaded,0,error,sizeof(error)) && !r->visit.active);
    CHECK(r->village.flags[205] && r->village.flags[391] && r->village.people[21].friendship==friendship);
    hmbtn_scene_preview_destroy(loaded);hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);
    puts("Native save integration: resource rebind, exact roundtrip/ticks, coop, bed save, daily/economy once, diary and title Continue passed.");return 0;
fail:
    hmbtn_title_preview_destroy(title);hmbtn_scene_preview_destroy(loaded);hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

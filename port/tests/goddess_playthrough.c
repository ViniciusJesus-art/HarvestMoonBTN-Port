/* Date, position and relationship fixtures; seeds are bought, grown and harvested. */
#include "poultry_helpers.h"
#include "field_helpers.h"
#include "item_reaction.h"
static bool grow_offerings(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->game.economy.gold=1000;
    REQUIRE(hmbtn_scene_preview_load_area(s,180,error,size) && step(s,0,error,size));
    const HmbtnGatherRecord *bundle=hmbtn_farm_gather_record(112);
    memcpy(g->intro.player.position,bundle->position,sizeof(bundle->position));g->intro.player.position[2]-=400;
    g->intro.player.facing=2;g->control.mode=0;
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && shop_wait(s,HMBTN_SHOP_SELECT,error,size));
    REQUIRE(step(s,HMBTN_PAD_DOWN,error,size) && g->feed_shop.item==0x22);
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && shop_wait(s,HMBTN_SHOP_COUNT,error,size));
    REQUIRE(step(s,HMBTN_PAD_RIGHT,error,size) && step(s,HMBTN_PAD_RIGHT,error,size) && step(s,HMBTN_PAD_RIGHT,error,size));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && shop_wait(s,HMBTN_SHOP_SELECT,error,size) && g->game.economy.gold==520);
    REQUIRE(step(s,HMBTN_PAD_CIRCLE,error,size) && shop_wait(s,HMBTN_SHOP_OFF,error,size));
    REQUIRE(hmbtn_scene_preview_load_area(s,0,error,size));g->intro.area=0;
    for(unsigned plot=0;plot<4;++plot) for(unsigned row=9;row<=11;++row) for(unsigned col=9+4*plot;col<=11+4*plot;++col)
        g->field.cells[row*45+col]=(HmbtnFarmTile){.value=0x167,.ground=0x167};
    for(unsigned plot=0;plot<4;++plot) {
        for(unsigned row=9;row<=11;++row) for(unsigned col=9+4*plot;col<=11+4*plot;++col) {
            cell(s,col,row);REQUIRE(tool(s,6,error,size));
        }
        cell(s,11+4*plot,10);REQUIRE(tool(s,0x22,error,size));
    }
    for(unsigned day=0;day<4;++day) {
        REQUIRE(field_refill(s,error,size));
        for(unsigned plot=0;plot<4;++plot) for(unsigned row=9;row<=11;++row) for(unsigned col=9+4*plot;col<=11+4*plot;++col) {
            cell(s,col,row);REQUIRE(tool(s,21,error,size));
        }
        g->game.weather.tomorrow=0;REQUIRE(sleep_day(s,error,size));
    }
    REQUIRE(g->field.cells[460].kind==2 && hmbtn_scene_preview_load_area(s,72,error,size));g->intro.area=72;
    return true;
}
static bool collect(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    REQUIRE(hmbtn_scene_preview_load_area(s,0,error,size));g->intro.area=0;
    for(unsigned plot=0;plot<4;++plot) for(unsigned row=9;row<=11;++row) for(unsigned col=9+4*plot;col<=11+4*plot;++col) {
        if(g->field.cells[row*45+col].value!=0x22 || g->field.cells[row*45+col].kind!=2) continue;
        cell(s,col,row);REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && idle(s,error,size) && g->game.inventory.held_item.id==0x41);
        REQUIRE(hmbtn_scene_preview_load_area(s,72,error,size));g->intro.area=72;return true;
    }
    REQUIRE(false);return false;
}
static bool launch(HmbtnScenePreview *s,bool target)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const HmbtnModel *m=&hmbtn_scene_preview_resources(s).scene->terrain;
    const uint8_t *section=hmbtn_scene_preview_resources(s).section,*z=NULL;
    for(unsigned i=0;i<50;++i) if(hmbtn_read_le16(section+18+i*44)==44) { z=section+16+i*44;break; }
    if(!z) return false;
    const int cx=(half(z+14)+half(z+20))/2,cz=(half(z+18)+half(z+24))/2,cy=half(z+16);
    for(int x=-2400;x<=2400;x+=100) for(int zz=-2400;zz<=2400;zz+=100) {
        int32_t dry[3]={cx+x,cy,cz+zz};HmbtnGroundHit hit;
        if(!hmbtn_ground_find(m,dry,0xE0C7,1720,&hit) || (hit.flags&0xE0C7u)) continue;
        dry[1]=hit.height;
        for(unsigned face=0;face<4;++face) {
            HmbtnFarmCarry trial={.area=g->area};trial.thrown[0]=(HmbtnThrownItem){.item=g->game.inventory.held_item,
                .position={dry[0],dry[1]-300,dry[2]},.ground=dry[1],.facing=face,.area=g->area};
            for(unsigned tick=0;tick<100 && !trial.thrown[0].phase;++tick) hmbtn_farm_carry_flight(&trial,NULL,m,NULL,section,0x2000);
            if(target ? !trial.offering : trial.thrown[0].phase!=1) continue;
            memcpy(g->intro.player.position,dry,sizeof(dry));memset(g->intro.player.velocity,0,sizeof(g->intro.player.velocity));
            g->intro.player.facing=face;g->control.mode=0;memcpy(g->intro.camera,dry,sizeof(dry));return true;
        }
    }
    return false;
}
static bool offer(HmbtnScenePreview *s,HmbtnHost *host,const char *shots,bool target,bool accepted,bool yes,bool bathe,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);const unsigned count=g->village.days.values[1];
    REQUIRE(collect(s,error,size) && launch(s,target));
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && g->carry.phase);
    bool seen=false,drawn=false,soaked=false,reward_drawn=false,romance_drawn=false;unsigned messages=0,bride=0,bond=0;
    for(unsigned tick=0;tick<5000;++tick) {
        if(g->visit.active) {
            seen=true;REQUIRE(hmbtn_goddess_event(g->visit.event) && !g->intro.script.boundary);
            if(g->intro.message) {
                ++messages;
                REQUIRE(g->intro.message>=523 && g->intro.message<=540);
                if((!drawn || (g->visit.event!=182 && !romance_drawn)) && !g->intro.fade_level
                        && g->runtime.loaded_message==g->intro.message && g->dialogue.message.phase==HMBTN_MESSAGE_WAIT_END) { REQUIRE(capture(s,host,shots,g->intro.message==539 ? "goddess-gotz" : g->intro.message==540 ? "goddess-berry" : g->visit.event==182 ? "goddess" : "goddess-romance",error,size));drawn=true;if(g->visit.event!=182) romance_drawn=true; }
            }
            if(g->visit.event!=182 && !bride) {
                bride=g->visit.event==185 ? 20 : g->visit.event==187 ? 7 : g->visit.event==189 ? 9 : g->visit.event==191 ? 3 : 14;
                bond=g->village.people[bride].affection;
            }
            if(g->visit.event!=182 && g->visit.free) {
                if(!bathe) {
                    /* Let the original one-hour wait expire. */
                    hmbtn_session_advance_minutes(g,65);
                } else if(!soaked && !g->hot_spring.phase) {
                    /* Normal interaction initiates the existing bath. */
                    const int32_t front[3]={-8838,-1892,4315};memcpy(g->intro.player.position,front,sizeof(front));
                    g->intro.player.facing=2;g->control.mode=0;
                    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));
                    fprintf(stderr,"Goddess bath start phase=%u pc=%u time=%u:%u deadline=%d:%d enabled=%d\n",g->hot_spring.phase,g->intro.script.pc,g->game.clock.hour,g->game.clock.minute,g->intro.slots[10],g->intro.slots[11],g->control.enabled);soaked=g->hot_spring.phase!=0;
                }
            }
        }
        if(!reward_drawn && g->event_item.phase!=HMBTN_EVENT_ITEM_IDLE) {
            REQUIRE(capture(s,host,shots,g->event_item.shown==103 ? "goddess-berry-receipt" : "goddess-wood-receipt",error,size));reward_drawn=true;
        }
        uint32_t input=g->intro.message ? HMBTN_PAD_CROSS : 0;
        if(g->intro.message==525 && g->dialogue.message.phase==HMBTN_MESSAGE_WAIT_END && g->dialogue.choice!=(yes ? 0u : 1u)) input=HMBTN_PAD_DOWN;
        if(bathe && g->hot_spring.phase==HMBTN_SPRING_SOAK) input=HMBTN_PAD_CROSS;
        REQUIRE(step(s,input,error,size));
        if(seen && !g->visit.active && !g->carry.phase && !g->event_item.phase && !g->carry.thrown[0].item.id) break;
        if(!accepted && tick>100 && !g->carry.phase && !g->carry.thrown[0].item.id) break;
    }
    if(g->visit.active) fprintf(stderr,"Goddess stuck event=%u pc=%u message=%u spring=%u\n",g->visit.event,g->intro.script.pc,g->intro.message,g->hot_spring.phase);
    REQUIRE(seen==accepted && !g->visit.active && !g->event_item.phase && !g->carry.phase && !g->game.inventory.held_item.id && !g->carry.thrown[0].item.id);
    if(bride) REQUIRE(g->village.people[bride].affection==bond+(bathe ? 2000u : 0u));
    REQUIRE(accepted ? messages && drawn && g->village.days.values[1]>=count : g->village.days.values[1]==count);
    return true;
}
int main(int argc,char **argv)
{
    if(argc<3 || argc>4) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};const char *shots=argc==4 ? argv[3] : NULL;
    CHECK(hmbtn_host_open(&host));HmbtnFarmClock date={0,0,2,3,12,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&date,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    CHECK(grow_offerings(s,error,sizeof(error)));
    CHECK(offer(s,&host,shots,false,false,false,false,error,sizeof(error)));
    g->game.weather.today=HMBTN_WEATHER_RAIN;CHECK(offer(s,&host,NULL,true,false,false,false,error,sizeof(error)));g->game.weather.today=0;
    const unsigned berries=g->menu.stats.power_berries,stamina=g->farm_action.max_stamina,gold=g->game.economy.gold;
    for(unsigned i=0;i<22;++i) {
        fprintf(stderr,"Offering %u count=%u\n",i+1,g->village.days.values[1]);
        CHECK(offer(s,&host,(i==0 || i==4 || i==9 || i==19) ? shots : NULL,true,true,true,true,error,sizeof(error)));
        if(i==4) CHECK(g->menu.stats.power_berries==berries+1 && g->farm_action.max_stamina==stamina+10 && g->game.progress.goddess_berry);
        if(i==5) { char berry[1024];snprintf(berry,sizeof(berry),"%s.berry",argv[2]);CHECK(field_reload(&s,&host,argv[1],berry,error,sizeof(error))); }
        g=hmbtn_scene_preview_session(s);
    }
    CHECK(g->village.days.values[1]==21 && g->menu.stats.power_berries==berries+1 && g->game.economy.gold==gold+1000);
    CHECK(field_reload(&s,&host,argv[1],argv[2],error,sizeof(error)));
    const unsigned brides[4]={3,7,9,14};
    for(unsigned n=0;n<4;++n) {
        g=hmbtn_scene_preview_session(s);g->village.days.values[1]=10;
        const unsigned all[5]={3,7,9,14,20};for(unsigned i=0;i<5;++i) g->village.people[all[i]].affection=0;
        g->village.people[brides[n]].affection=10000;
        CHECK(offer(s,&host,NULL,true,true,true,n!=0,error,sizeof(error)));
    }
    g=hmbtn_scene_preview_session(s);g->village.days.values[1]=10;
    CHECK(offer(s,&host,NULL,true,true,false,false,error,sizeof(error)));
    puts("Goddess: purchased seeds/cultivated turnips, dry miss/rain refusal, consumed gifts, original dialogue/counter, berry, all five romance scripts/bath/timeout/decline, Gotz payment and reload passed");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

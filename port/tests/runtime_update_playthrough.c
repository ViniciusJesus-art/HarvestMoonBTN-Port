#include "scene_fixture.h"
#include "inventory_checks.h"
#include "economy_checks.h"
#include "calendar_checks.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Runtime update scenario %u tick %u line %d: %s; %s\n",scenario,tick,__LINE__,#x,error);goto fail; } } while(0)
static bool equal(const HmbtnSession *a,const HmbtnSession *b)
{
    if(!inventory_equal(&a->game.inventory,&b->game.inventory) || !economy_equal(&a->game.economy,&b->game.economy)
        || !clock_equal(&a->game.clock,&b->game.clock) || !registration_equal(&a->game.registration,&b->game.registration)
        || memcmp(a->village.flags,b->village.flags,sizeof(a->village.flags)) || memcmp(&a->village.days,&b->village.days,sizeof(a->village.days))) return false;
    for(unsigned i=0;i<HMBTN_FIELD_CELLS;++i) {
        const HmbtnFarmTile *x=&a->field.cells[i],*y=&b->field.cells[i];
        if(x->value!=y->value || x->ground!=y->ground || x->kind!=y->kind || x->watered_days!=y->watered_days || x->growth_flags!=y->growth_flags) return false;
    }
    for(unsigned i=1;i<HMBTN_VILLAGERS;++i) {
        const HmbtnVillager *x=&a->village.people[i],*y=&b->village.people[i];
        if(x->friendship!=y->friendship || x->affection!=y->affection || x->location!=y->location || x->step!=y->step) return false;
        if(memcmp(hmbtn_village_actor(&a->village,i),hmbtn_village_actor(&b->village,i),sizeof(HmbtnIntroActor))) return false;
    }
    HmbtnHouseControl ac=a->control,bc=b->control;ac.collision_context=bc.collision_context=NULL;
    return !memcmp(&a->intro.player,&b->intro.player,sizeof(a->intro.player))
        && !memcmp(a->intro.slots,b->intro.slots,sizeof(a->intro.slots))
        && !memcmp(&a->dialogue.message,&b->dialogue.message,sizeof(a->dialogue.message))
        && !memcmp(&ac,&bc,sizeof(ac)) && !memcmp(&a->sleep,&b->sleep,sizeof(a->sleep))
        && a->intro.script.pc==b->intro.script.pc && a->intro.script.finished==b->intro.script.finished
        && a->intro.script.boundary==b->intro.script.boundary && a->intro.message==b->intro.message
        && a->runtime.ticks==b->runtime.ticks && a->runtime.loaded_message==b->runtime.loaded_message
        && a->visit.active==b->visit.active && a->area==b->area && a->door_tick==b->door_tick && a->travel_tick==b->travel_tick
        && a->morning_fade==b->morning_fade && a->daily_date==b->daily_date && a->village.random==b->village.random;
}
static bool failed_area(HmbtnSessionFrame *f,unsigned area,char *error,size_t size)
{
    (void)f;(void)area;snprintf(error,size,"Injected area read failure");return false;
}
static bool draw_unchanged(HmbtnScenePreview *s,HmbtnHost *host,char *error,size_t size)
{
    const HmbtnSession *session=hmbtn_scene_preview_session(s);
    const HmbtnSceneResources resources=hmbtn_scene_preview_resources(s);
    unsigned char *before=malloc(sizeof(*session));if(!before) return false;
    memcpy(before,session,sizeof(*session));const HmbtnSceneData pose=*resources.scene;
    hmbtn_scene_preview_invalidate(s);
    const bool ok=hmbtn_scene_preview_draw(s,host->renderer,error,size)
        && !memcmp(before,session,sizeof(*session)) && !memcmp(&pose,resources.scene,sizeof(pose));
    free(before);return ok;
}
static bool door(HmbtnScenePreview *scene)
{
    HmbtnSession *s=hmbtn_scene_preview_session(scene);const uint8_t *section=hmbtn_scene_preview_resources(scene).section;
    for(unsigned i=0;i<50;++i) {
        const uint8_t *p=section+16+i*44;if(hmbtn_read_le16(p+2)!=80) continue;
        for(unsigned k=0;k<3;++k) {
            const int lo=(int16_t)hmbtn_read_le16(p+14+k*2),hi=(int16_t)hmbtn_read_le16(p+20+k*2);
            s->intro.player.position[k]=(lo+hi)/2;s->intro.player.velocity[k]=0;
        }
        s->intro.player.facing=hmbtn_read_le16(p+8)&3u;s->control.mode=0;
        HmbtnHouseExit e;unsigned model;
        if(hmbtn_world_door_at(section,0x2000,&s->intro.player,&e,&model)) return true;
    }
    return false;
}
int main(int argc,char **argv)
{
    if(argc!=2) return 2;
    HmbtnHost host={0};HmbtnScenePreview *scenes[3]={0};char error[512]={0};unsigned scenario=0,tick=0;
    CHECK(hmbtn_host_open(&host));
    for(;scenario<3;++scenario) {
        const HmbtnFarmClock date={0,scenario<2 ? 2u : 0u,scenario<2 ? 13u : 2u,3,scenario<2 ? 6u : 12u,0,0};
        for(unsigned i=0;i<3;++i) {
            scenes[i]=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&date,error,sizeof(error));CHECK(scenes[i]);
        }
        bool selected=false,opened=false,slept=false,done=false;
        for(tick=0;tick<2500 && !done;++tick) {
            HmbtnSession *lead=hmbtn_scene_preview_session(scenes[0]);uint32_t pressed=0;
            if(scenario<2) {
                pressed=lead->intro.message ? HMBTN_PAD_CROSS : 0;
                if(lead->dialogue.message.choices && lead->dialogue.message.phase==HMBTN_MESSAGE_WAIT_END && !selected) {
                    selected=true;if(scenario==1) pressed=HMBTN_PAD_DOWN;
                }
            } else if(tick==110) {
                for(unsigned i=0;i<3;++i) {
                    HmbtnSession *s=hmbtn_scene_preview_session(scenes[i]);CHECK(door(scenes[i]));
                    s->game.inventory.equipped=(HmbtnStoredTool){34,0,3};
                    s->game.economy.shipping.pending_value=60;s->game.economy.shipping.settled=false;
                    s->field.cells[460]=(HmbtnFarmTile){.value=34,.ground=0x16b};
                }
                pressed=HMBTN_PAD_CROSS;opened=true;
            } else if(opened && lead->area==80 && !lead->door_tick && !lead->travel_tick && !slept) {
                for(unsigned i=0;i<3;++i) hmbtn_scene_preview_session(scenes[i])->house.mode=HMBTN_HOUSE_SLEEP;
                slept=true;
            }
            for(unsigned i=0;i<3;++i) {
                CHECK(hmbtn_scene_preview_tick(scenes[i],(HmbtnLogicalInput){pressed,scenario<2 ? HMBTN_PAD_CROSS : 0},error,sizeof(error)));
                for(unsigned n=0;n<i;++n) CHECK(draw_unchanged(scenes[i],&host,error,sizeof(error)));
            }
            CHECK(equal(lead,hmbtn_scene_preview_session(scenes[1])) && equal(lead,hmbtn_scene_preview_session(scenes[2])));
            if(scenario<2) done=selected && !lead->visit.active && lead->control.enabled;
            else done=slept && lead->game.clock.day==date.day+1 && !lead->morning_fade;
        }
        CHECK(done && tick<2500);
        HmbtnSession *s=hmbtn_scene_preview_session(scenes[0]);
        if(scenario<2) CHECK(s->village.flags[391] && s->village.flags[205]==(scenario==0));
        else CHECK(opened && slept && s->area==80 && s->game.clock.hour==6
            && s->game.inventory.equipped.id==34 && s->game.inventory.equipped.count==3
            && s->game.economy.gold==560 && s->game.economy.ledger[0][0]==60 && !s->game.economy.shipping.pending_value
            && s->field.cells[460].watered_days==1 && s->daily_date==hmbtn_session_date(&s->game.clock));
        if(scenario==2) {
            HmbtnSessionFrame f=hmbtn_scene_preview_frame(scenes[0]);f.area=failed_area;
            const HmbtnEconomy economy=s->game.economy;const HmbtnInventory inventory=s->game.inventory;
            s->travel_tick=10;s->travel_exit.target=0;
            CHECK(!hmbtn_session_tick(s,&f,(HmbtnLogicalInput){0},error,sizeof(error))
                && strstr(error,"Injected area") && !s->active_frame && s->area==80
                && economy_equal(&economy,&s->game.economy) && inventory_equal(&inventory,&s->game.inventory));
            error[0]=0;
        }
        for(unsigned i=0;i<3;++i) { hmbtn_scene_preview_destroy(scenes[i]);scenes[i]=NULL; }
    }
    hmbtn_host_close(&host);puts("Public full-session tick: original event yes/no, doors/area, sleep/daily/shipping match at 0/30/60 presentations; forced draw leaves session and decoded poses unchanged.");return 0;
fail:
    for(unsigned i=0;i<3;++i) hmbtn_scene_preview_destroy(scenes[i]);
    hmbtn_host_close(&host);return 1;
}

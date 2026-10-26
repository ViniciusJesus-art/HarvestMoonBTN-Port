/* Three identical sessions tick directly through the public free-world API.
 * Real geometry/ANI and UI are borrowed from the current adapter; render calls
 * never advance time and are exercised at 0/30/60 presentations per second. */
#include "scene_fixture.h"
#include "sdl_host.h"
#include "inventory_checks.h"
#include "economy_checks.h"
#include "calendar_checks.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Exploration integration line %d tick %u: %s; %s\n",__LINE__,tick,#x,error);goto fail; } } while(0)
static bool same(const HmbtnSession *a,const HmbtnSession *b)
{
    if(!inventory_equal(&a->game.inventory,&b->game.inventory) || !economy_equal(&a->game.economy,&b->game.economy)
            || !clock_equal(&a->game.clock,&b->game.clock) || !registration_equal(&a->game.registration,&b->game.registration)) return false;
    for(unsigned axis=0;axis<3;++axis) if(a->intro.player.position[axis]!=b->intro.player.position[axis]) return false;
    for(unsigned i=0;i<HMBTN_FIELD_CELLS;++i) {
        const HmbtnFarmTile *x=&a->field.cells[i],*y=&b->field.cells[i];
        if(x->value!=y->value || x->ground!=y->ground || x->kind!=y->kind || x->growth_flags!=y->growth_flags || x->watered_days!=y->watered_days) return false;
    }
    for(unsigned id=1;id<HMBTN_VILLAGERS;++id) {
        const HmbtnVillager *x=&a->village.people[id],*y=&b->village.people[id];
        if(x->friendship!=y->friendship || x->affection!=y->affection || x->location!=y->location || x->step!=y->step || x->delay!=y->delay) return false;
        for(unsigned k=0;k<3;++k) if(x->actor.position[k]!=y->actor.position[k]) return false;
    }
    return a->control.mode==b->control.mode && a->control.jumping==b->control.jumping
        && a->intro.player.facing==b->intro.player.facing && a->intro.player.sprite.animation==b->intro.player.sprite.animation
        && a->intro.player.sprite.frame==b->intro.player.sprite.frame && a->intro.player.sprite.ticks==b->intro.player.sprite.ticks
        && a->farm_action.phase==b->farm_action.phase && a->farm_action.stamina==b->farm_action.stamina
        && a->farm_action.fatigue==b->farm_action.fatigue && a->farm_action.experience[1]==b->farm_action.experience[1]
        && a->menu.mode==b->menu.mode && a->menu.phase==b->menu.phase && a->village.random==b->village.random;
}
int main(int argc,char **argv)
{
    if(argc!=2) return 2;
    HmbtnHost host={0};HmbtnScenePreview *scenes[3]={0};char error[512]={0};unsigned tick=0;
    CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock clock={0,0,2,2,12,0,0};
    for(unsigned i=0;i<3;++i) {
        scenes[i]=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&clock,error,sizeof(error));CHECK(scenes[i]);
        CHECK(hmbtn_scene_preview_tick(scenes[i],(HmbtnLogicalInput){0},error,sizeof(error)));
        HmbtnSession *s=hmbtn_scene_preview_session(scenes[i]);
        s->intro.fade_level=0;s->control.mode=0;s->intro.player.position[0]=-600;s->intro.player.position[2]=-2600;
        s->intro.player.facing=3;s->intro.player.sprite=(HmbtnAnimationPlayer){0,3,0};
        HmbtnGroundHit hit;CHECK(hmbtn_ground_find(&(*hmbtn_scene_preview_resources(scenes[i]).scene).terrain,s->intro.player.position,0xC080,200,&hit));
        s->intro.player.position[1]=hit.height;
        s->game.inventory.equipped=(HmbtnStoredTool){6,0,0};s->field.cells[460]=(HmbtnFarmTile){0x167,0x167,0,0};
    }
    HmbtnFarmClock paused={0};bool observed_pause=false;
    for(tick=0;tick<180;++tick) {
        const uint32_t pressed=tick==0 ? HMBTN_PAD_SQUARE : tick==100 || tick==130 ? HMBTN_PAD_START : 0;
        const uint32_t held=tick==0 ? HMBTN_PAD_SQUARE : tick>=155 ? HMBTN_PAD_RIGHT : 0;
        for(unsigned i=0;i<3;++i) {
            CHECK(hmbtn_scene_preview_tick(scenes[i],(HmbtnLogicalInput){pressed,held},error,sizeof(error)));
            CHECK(!hmbtn_scene_preview_session(scenes[i])->travel_tick && !hmbtn_scene_preview_session(scenes[i])->door_tick);
            /* Compare every tick after each session's presentation cadence. */
            for(unsigned n=0;n<i;++n) {
                hmbtn_scene_preview_invalidate(scenes[i]);
                CHECK(hmbtn_scene_preview_draw(scenes[i],host.renderer,error,sizeof(error)));
            }
        }
        CHECK(same(hmbtn_scene_preview_session(scenes[0]),hmbtn_scene_preview_session(scenes[1])) && same(hmbtn_scene_preview_session(scenes[0]),hmbtn_scene_preview_session(scenes[2])));
        if(tick==99) CHECK(hmbtn_scene_preview_session(scenes[0])->field.cells[460].value==0x168 && hmbtn_scene_preview_session(scenes[0])->farm_action.stamina==99);
        if(tick==100) { paused=hmbtn_scene_preview_session(scenes[0])->game.clock;observed_pause=true; }
        if(tick>100 && tick<149) CHECK(clock_equal(&paused,&hmbtn_scene_preview_session(scenes[0])->game.clock));
    }
    CHECK(observed_pause && hmbtn_scene_preview_session(scenes[0])->menu.mode==HMBTN_MENU_NONE);
    CHECK(hmbtn_scene_preview_session(scenes[0])->intro.player.position[0]!=-600);
    for(unsigned i=0;i<3;++i) hmbtn_scene_preview_destroy(scenes[i]);
    hmbtn_host_close(&host);
    puts("Public exploration tick with real assets: identical tool, movement, pause, clock, residents and inventory at 0/30/60 presentations.");return 0;
fail:
    for(unsigned i=0;i<3;++i) hmbtn_scene_preview_destroy(scenes[i]);
    hmbtn_host_close(&host);return 1;
}

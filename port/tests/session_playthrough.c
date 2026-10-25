#include "poultry_helpers.h"
#include "inventory_checks.h"
#include "economy_checks.h"
#include "calendar_checks.h"
static bool state_kept(const HmbtnSession *s,const HmbtnGameState *expected)
{
    return inventory_equal(&s->game.inventory,&expected->inventory)
        && economy_equal(&s->game.economy,&expected->economy)
        && clock_equal(&s->game.clock,&expected->clock)
        && registration_equal(&s->game.registration,&expected->registration);
}
int main(int argc,char **argv)
{
    if(argc!=2) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL,*other=NULL;char error[512]={0};
    CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock clock={0,0,2,2,12,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],112,&clock,error,sizeof(error));CHECK(s);
    other=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&clock,error,sizeof(error));CHECK(other);
    for(unsigned i=0;i<110;++i) {
        CHECK(step(s,0,error,sizeof(error)));
        CHECK(step(other,0,error,sizeof(error)));
    }
    HmbtnSession *session=s->session;
    CHECK(session && session!=other->session && &session->field!=&other->session->field);
    CHECK(session->village_ready && session->village_initialized && session->village.schedule_data==s->schedule_block);
    CHECK(s->house.inventory==&session->game.inventory && s->menu.economy==&session->game.economy);
    CHECK(s->menu.poultry==&session->poultry && s->feed_shop.poultry==&session->poultry);
    session->game.economy.gold=4321;session->game.inventory.bag[7]=(HmbtnStoredTool){21,0,73};
    session->field.cells[400]=(HmbtnFarmTile){0x169,0x168,0,0x1234};
    session->village.people[18].friendship=211;session->village.people[3].affection=3210;
    session->village.days.values[127]=55;session->village.flags[381]=true;
    const uint16_t name[9]={0,1,2,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff};
    CHECK(hmbtn_poultry_buy(&session->poultry,name,&session->game.clock));
    HmbtnHen *hen=&session->poultry.hens[0];hen->affection=150;
    memcpy(s->intro.player.position,hen->actor.position,sizeof(hen->actor.position));
    s->intro.player.position[2]+=300;s->intro.player.facing=0;s->control.mode=0;
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && session->poultry.carry_phase==1 && hen->held);
    for(unsigned i=0;i<200 && session->poultry.carry_phase;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(!session->poultry.carry_phase && s->house.inventory->held_item.id==0x109);
    CHECK(hmbtn_poultry_count(&session->poultry)==1 && hmbtn_poultry_held_slot(&session->poultry)==0);
    const HmbtnGameState expected=session->game;const uint32_t random=session->poultry.random;
    /* Recreate UI controllers while the animal record remains reserved. */
    hmbtn_house_interaction_init(&s->house,&session->game);
    hmbtn_farm_menu_init(&s->menu,&session->game.economy);s->menu.poultry=&session->poultry;
    s->menu.mode=HMBTN_MENU_STATUS;s->menu.phase=3;
    for(unsigned i=0;i<30;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(state_kept(session,&expected) && hen->held && session->poultry.random==random);
    CHECK(hmbtn_scene_preview_draw(s,host.renderer,0,0,0,error,sizeof(error)));
    const unsigned areas[]={80,0,112,128,112};
    for(unsigned i=0;i<sizeof(areas)/sizeof(*areas);++i) {
        CHECK(load_area(s,argv[1],areas[i],error,sizeof(error)));s->intro.area=s->area;
        CHECK(s->session==session && state_kept(session,&expected));
        CHECK(&session->poultry.hens[0]==hen && hen->held && hen->affection==150 && hen->name[2]==2);
        CHECK(hmbtn_poultry_count(&session->poultry)==1 && s->menu.poultry==&session->poultry);
        CHECK(session->village.people[18].friendship==211 && session->village.people[3].affection==3210
            && session->village.days.values[127]==55 && session->village.flags[381]);
        CHECK(session->field.cells[400].value==0x169 && session->field.cells[400].ground==0x168
            && session->field.cells[400].growth_flags==0x1234);
    }
    /* Host storage may move; parsed villagers and progression must not reset. */
    uint8_t *replacement=malloc(0xA800);CHECK(replacement);
    memcpy(replacement,s->schedule_block,0xA800);
    hmbtn_session_unbind_resources(session);
    CHECK(!session->village_ready && session->village_initialized);
    CHECK(hmbtn_session_bind_schedules(session,replacement,0xA800));
    free(s->schedule_block);s->schedule_block=replacement;
    CHECK(session->village.schedule_data==s->schedule_block && session->village.people[18].friendship==211
        && session->village.days.values[127]==55 && state_kept(session,&expected));
    CHECK(other->session->game.economy.gold==500 && !hmbtn_poultry_count(&other->session->poultry));
    hmbtn_scene_preview_destroy(other);other=NULL;
    CHECK(&session->poultry.hens[0]==hen && hen->held && state_kept(session,&expected));
    hmbtn_poultry_release(&session->poultry,&s->house,&s->intro.player,s->area,&s->scene.terrain);
    CHECK(!hen->held && hen->area==112 && !s->house.inventory->held_item.id && hmbtn_poultry_count(&session->poultry)==1);
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);
    puts("Session ownership across live UI, installed areas, held animal transport, schedule rebinding and independent scenes passed.");return 0;
fail:
    hmbtn_scene_preview_destroy(other);hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

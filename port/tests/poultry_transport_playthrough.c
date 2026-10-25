#include "poultry_helpers.h"
static bool animal_idle(HmbtnScenePreview *s,char *error,size_t size)
{
    for(unsigned i=0;i<200 && s->session->poultry.carry_phase;++i) REQUIRE(step(s,0,error,size));
    REQUIRE(!s->session->poultry.carry_phase);return true;
}
static bool take_animal(HmbtnScenePreview *s,unsigned slot,char *error,size_t size)
{
    HmbtnHen *h=&s->session->poultry.hens[slot];REQUIRE(h->area==s->area && !h->held);
    memcpy(s->intro.player.position,h->actor.position,sizeof(h->actor.position));s->intro.player.position[2]+=300;
    s->intro.player.facing=0;s->control.mode=0;
    REQUIRE(step(s,HMBTN_PAD_CROSS,error,size));REQUIRE(s->session->poultry.carry_phase==1 && h->held);
    REQUIRE(animal_idle(s,error,size));REQUIRE(hmbtn_poultry_held_slot(&s->session->poultry)==slot);return true;
}
static bool place_animal(HmbtnScenePreview *s,unsigned slot,char *error,size_t size)
{
    const bool farm=s->area==0;bool found=false;
    for(int z=farm ? -7600 : -1600;z<=(farm ? -4000 : 1200) && !found;z+=400)
        for(int x=farm ? -5200 : -800;x<=(farm ? -4800 : 800);x+=400) {
            int32_t target[3]={x,0,z},player[3]={x,0,z+300};
            if(hmbtn_poultry_occupies(&s->session->poultry,s->area,target)
                    || hmbtn_farm_field_probe(farm ? &s->session->field : NULL,target)
                    || !hmbtn_farm_settle(farm ? &s->session->field : NULL,&s->scene.terrain,target,0xC087,200)
                    || !hmbtn_farm_settle(farm ? &s->session->field : NULL,&s->scene.terrain,player,0xC087,200)) continue;
            memcpy(s->intro.player.position,player,sizeof(player));s->intro.player.facing=0;s->control.mode=0;
            HmbtnHouseExit door;unsigned model;
            if(hmbtn_world_door_at(s->section,0x2000,&s->intro.player,&door,&model)) continue;
            found=true;break;
        }
    REQUIRE(found);REQUIRE(step(s,HMBTN_PAD_CROSS,error,size) && s->session->poultry.carry_phase==2);
    REQUIRE(animal_idle(s,error,size));REQUIRE(!s->house.inventory->held_item.id && !s->session->poultry.hens[slot].held
        && s->session->poultry.hens[slot].area==s->area);return true;
}
int main(int argc,char **argv)
{
    if(argc<2 || argc>3) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};const char *shots=argc==3 ? argv[2] : NULL;
    CHECK(hmbtn_host_open(&host));s=hmbtn_scene_area_create(host.renderer,argv[1],112,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    s->session->game.clock=(HmbtnFarmClock){0,0,2,2,12,0,0};
    /* Declared adult/chick fixtures; preserve the session's 500G and zero supplies. */
    const uint16_t name[9]={0,1,2,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF};
    CHECK(hmbtn_poultry_buy(&s->session->poultry,name,&s->session->game.clock) && hmbtn_poultry_buy(&s->session->poultry,name,&s->session->game.clock));
    s->session->poultry.hens[0].affection=150;s->session->poultry.hens[1].stage=HMBTN_HEN_CHICK;s->session->poultry.hens[1].growth=3;
    CHECK(take_animal(s,0,error,sizeof(error)) && s->house.inventory->held_item.id==0x109);
    const HmbtnHen *adult=&s->session->poultry.hens[0];const uint32_t random=s->session->poultry.random;
    CHECK(capture(s,&host,shots,"transport-adult-held",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_R1|HMBTN_PAD_R2,error,sizeof(error)) && !s->quick_ticks && s->house.inventory->held_item.id==0x109);
    CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)) && s->menu.mode==HMBTN_MENU_STATUS);
    for(unsigned i=0;i<30;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(s->session->poultry.random==random && adult->held);
    CHECK(step(s,HMBTN_PAD_START,error,sizeof(error)));
    for(unsigned i=0;i<30;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(travel(s,0,error,sizeof(error)) && adult->held && s->house.inventory->held_item.id==0x109);
    CHECK(capture(s,&host,shots,"transport-adult-outside-held",error,sizeof(error)));
    CHECK(place_animal(s,0,error,sizeof(error)) && adult->area==0 && !adult->held);
    CHECK(capture(s,&host,shots,"transport-adult-outside",error,sizeof(error)));
    CHECK(!s->house.inventory->supplies[2] && !s->feed.coop[0]);
    for(unsigned night=0;night<2;++night) {
        CHECK(sleep_day(s,error,sizeof(error)) && adult->area==0 && adult->fed_yesterday);
        CHECK(adult->affection==160+night*10 && !s->house.inventory->supplies[2]);
    }
    CHECK(s->session->poultry.eggs[0].item==0x123 && s->session->poultry.eggs[0].area==0);
    /* Import and render each actual seasonal outdoor egg, including golden art. */
    for(unsigned season=0;season<4;++season) {
        s->session->game.clock.season=season;CHECK(load_area(s,s->data_root,0,error,sizeof(error)));
        memcpy(s->intro.camera,s->session->poultry.eggs[0].position,sizeof(s->intro.camera));
        CHECK(capture(s,&host,NULL,"season-egg",error,sizeof(error)));
        const unsigned saved=s->session->poultry.eggs[0].item;s->session->poultry.eggs[0].item=0xE8;
        CHECK(capture(s,&host,NULL,"season-golden-egg",error,sizeof(error)));s->session->poultry.eggs[0].item=saved;
    }
    s->session->game.clock.season=0;CHECK(load_area(s,s->data_root,0,error,sizeof(error)));
    HmbtnCoopEgg *egg=&s->session->poultry.eggs[0];memcpy(s->intro.player.position,egg->position,sizeof(egg->position));
    s->intro.player.position[2]+=600;s->control.mode=0;
    /* Let the adult walk clear of its newly laid egg before the visual check. */
    for(unsigned i=0;i<1200 && abs(adult->actor.position[0]-egg->position[0])+abs(adult->actor.position[2]-egg->position[2])<300;++i)
        CHECK(step(s,0,error,sizeof(error)));
    CHECK(abs(adult->actor.position[0]-egg->position[0])+abs(adult->actor.position[2]-egg->position[2])>=300);
    memcpy(s->intro.player.position,egg->position,sizeof(egg->position));
    s->intro.player.position[2]+=224;s->intro.player.facing=0;s->control.mode=0;
    CHECK(capture(s,&host,shots,"transport-outdoor-egg",error,sizeof(error)));
    const unsigned price=(unsigned)hmbtn_farm_item(egg->item)->sell_price;
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)) && s->house.inventory->held_item.id==0x123 && !egg->item);
    CHECK(idle(s,error,sizeof(error)) && ship(s,error,sizeof(error)) && s->session->game.economy.shipping.pending_value==price);
    CHECK(take_animal(s,0,error,sizeof(error)));
    CHECK(travel(s,112,error,sizeof(error)) && adult->held && adult->area==0);
    CHECK(place_animal(s,0,error,sizeof(error)) && adult->area==112 && adult->affection==170);
    CHECK(capture(s,&host,shots,"transport-adult-returned",error,sizeof(error)));
    CHECK(take_animal(s,1,error,sizeof(error)) && s->house.inventory->held_item.id==0x10A);
    CHECK(capture(s,&host,shots,"transport-chick-held",error,sizeof(error)));
    CHECK(travel(s,0,error,sizeof(error)) && travel(s,80,error,sizeof(error)));
    CHECK(s->house.inventory->held_item.id==0x10A && s->session->poultry.hens[1].held);
    /* Forced sleep releases the record in the house; chicken_place returns it to the coop. */
    s->house.mode=HMBTN_HOUSE_SLEEP;const unsigned day=s->session->game.clock.day;
    for(unsigned i=0;i<1200 && (s->session->game.clock.day==day || s->morning_fade);++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(s->session->game.clock.day!=day && !s->house.inventory->held_item.id && !s->session->poultry.hens[1].held && s->session->poultry.hens[1].area==112);
    CHECK(travel(s,0,error,sizeof(error)));
    for(unsigned i=0;i<4;++i) CHECK(sleep_day(s,error,sizeof(error)));
    CHECK(s->session->poultry.hens[1].stage==HMBTN_HEN_ADULT && hmbtn_poultry_count(&s->session->poultry)==2);
    CHECK(travel(s,112,error,sizeof(error)));
    CHECK(capture(s,&host,shots,"transport-chick-grown",error,sizeof(error)));
    CHECK(!s->carry.discarded && !s->session->game.economy.ledger[0][1] && s->session->game.economy.gold==500+price);
    puts("Transport: reserved adult/chick records, menus, real doors, outdoor hourly feed/eggs/shipping, seasonal art, return, sleep release and growth passed.");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

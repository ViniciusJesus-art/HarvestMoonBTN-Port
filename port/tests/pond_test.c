#include "save_fixture.h"
#include "psx_random.h"
#include "psx_data.h"
#include <stdlib.h>
#include <stdio.h>
static void zone(uint8_t *section,unsigned kind,unsigned facing)
{
    memset(section,0,0x2000);uint8_t *z=section+16;
    hmbtn_write_le16(z,0x4000);hmbtn_write_le16(z+2,(uint16_t)kind);hmbtn_write_le16(z+8,(uint16_t)facing);
    hmbtn_write_le16(z+14,(uint16_t)-200);hmbtn_write_le16(z+20,200);
    hmbtn_write_le16(z+18,(uint16_t)-400);hmbtn_write_le16(z+24,0);
}
int main(void)
{
    HmbtnPond p={.random=7};assert(!hmbtn_pond_add(&p,132) && !hmbtn_pond_take(&p));
    assert(hmbtn_pond_add(&p,163) && hmbtn_pond_add(&p,162) && hmbtn_pond_add(&p,161));
    assert(p.fish[0].days==0 && p.fish[1].days==10 && p.fish[2].days==20 && hmbtn_pond_count(&p)==3);
    assert(hmbtn_pond_take(&p)==161 && hmbtn_pond_take(&p)==162 && hmbtn_pond_take(&p)==163 && !hmbtn_pond_take(&p));
    memset(&p,0,sizeof(p));p.random=7;assert(hmbtn_pond_add(&p,163));
    for(unsigned day=0;day<9;++day) { p.fed=true;hmbtn_pond_new_day(&p); }
    assert(p.fish[0].item==163 && p.fish[0].days==9 && !p.fed && p.random==7);
    for(unsigned day=0;day<10;++day) hmbtn_pond_new_day(&p);assert(p.fish[0].days==9);
    p.fed=true;hmbtn_pond_new_day(&p);assert(p.fish[0].item==162 && p.fish[0].days==10);
    for(unsigned day=0;day<10;++day) { p.fed=true;hmbtn_pond_new_day(&p); }
    assert(p.fish[0].item==161 && p.fish[0].days==20);
    for(unsigned day=0;day<9;++day) { p.fed=true;hmbtn_pond_new_day(&p); }
    /* Seed 23 gives three offspring at density 1. Later free slots are visited
     * in the same original forward pass, so newborns finish at age one. */
    p.random=23;uint32_t expected_random=p.random;
    const unsigned babies=(hmbtn_psx_random_range(&expected_random,100)-1)/25;
    assert(babies>0);p.fed=true;hmbtn_pond_new_day(&p);
    assert(p.fish[0].days==20 && hmbtn_pond_count(&p)==1+babies && p.random==expected_random && !p.fed);
    for(unsigned i=1;i<=babies;++i) assert(p.fish[i].item==163 && p.fish[i].days==1);
    /* A vacancy before the parent is not revisited. */
    p=(HmbtnPond){.random=23};p.fish[1]=(HmbtnPondFish){161,29};p.fed=true;hmbtn_pond_new_day(&p);
    assert(p.fish[0].item==163 && p.fish[0].days==0);
    while(hmbtn_pond_add(&p,161)) {}assert(hmbtn_pond_count(&p)==99 && !hmbtn_pond_add(&p,163));
    p.fish[1].days=29;p.fed=true;hmbtn_pond_new_day(&p);assert(hmbtn_pond_count(&p)==99 && p.fish[1].days==20 && hmbtn_pond_valid(&p));
    p.fish[0].item=108;assert(!hmbtn_pond_valid(&p));
    HmbtnSession *s=save_fixture(1);s->pond=(HmbtnPond){.random=23};assert(hmbtn_pond_add(&s->pond,163));
    memset(&s->game.inventory.held_item,0,sizeof(s->game.inventory.held_item));
    uint8_t section[0x2000];zone(section,45,0);s->intro.player.position[0]=s->intro.player.position[1]=s->intro.player.position[2]=0;s->intro.player.facing=0;
    assert(!hmbtn_farm_feed_pond_tick(&s->feed,&s->pond,&s->house,&s->intro.player,3,section,sizeof(section),HMBTN_PAD_CROSS));
    s->intro.player.facing=1;assert(!hmbtn_farm_feed_pond_tick(&s->feed,&s->pond,&s->house,&s->intro.player,0,section,sizeof(section),HMBTN_PAD_CROSS));s->intro.player.facing=0;
    assert(hmbtn_farm_feed_pond_tick(&s->feed,&s->pond,&s->house,&s->intro.player,0,section,sizeof(section),HMBTN_PAD_CROSS));
    assert(s->game.inventory.held_item.id==163 && !hmbtn_pond_count(&s->pond) && s->feed.phase==1);
    s->intro.player.animation_done=true;hmbtn_farm_feed_tick(&s->feed,&s->house,&s->intro.player,section,sizeof(section),0);
    zone(section,10,0);assert(hmbtn_farm_feed_pond_tick(&s->feed,&s->pond,&s->house,&s->intro.player,0,section,sizeof(section),HMBTN_PAD_CROSS));
    assert(s->game.inventory.held_item.id==163 && !s->feed.phase);
    s->game.inventory.held_item=(HmbtnStoredTool){0};s->game.inventory.supplies[3]=1;
    assert(hmbtn_farm_feed_pond_tick(&s->feed,&s->pond,&s->house,&s->intro.player,3,section,sizeof(section),HMBTN_PAD_CROSS));
    assert(s->game.inventory.held_item.id==132 && !s->game.inventory.supplies[3] && s->feed.phase==1);
    s->intro.player.animation_done=true;hmbtn_farm_feed_tick(&s->feed,&s->house,&s->intro.player,section,sizeof(section),0);
    /* The existing date guard protects pond growth and feeding from reentry. */
    assert(hmbtn_pond_add(&s->pond,161));s->pond.fish[0].days=29;s->pond.fed=true;
    hmbtn_session_advance_minutes(s,1440);assert(!s->pond.fed && s->pond.fish[0].days==20 && hmbtn_pond_count(&s->pond)>1);
    const unsigned count=hmbtn_pond_count(&s->pond);const uint32_t random=s->pond.random;
    hmbtn_session_clock_events(s,&s->game.clock,HMBTN_CLOCK_DAY);assert(hmbtn_pond_count(&s->pond)==count && s->pond.random==random && s->pond.fish[0].days==20);
    s->pond.fed=true;uint8_t *data=NULL;size_t bytes=0;char error[256];assert(hmbtn_save_encode(s,&data,&bytes,error,sizeof(error)));
    HmbtnSession *loaded=hmbtn_save_decode(data,bytes,error,sizeof(error));assert(loaded && loaded->pond.fed && loaded->pond.random==random && hmbtn_pond_count(&loaded->pond)==count);
    for(unsigned i=0;i<HMBTN_POND_FISH;++i) assert(loaded->pond.fish[i].item==s->pond.fish[i].item && loaded->pond.fish[i].days==s->pond.fish[i].days);
    assert(loaded->carry.pond==&loaded->pond && loaded->menu.pond==&loaded->pond);
    free(data);hmbtn_session_destroy(loaded);hmbtn_session_destroy(s);
    puts("Pond: capacity/withdrawal priority, fed-only 10/20/30-day growth/spawn, newborn traversal, winter/stock pickup, date guard and persisted state passed");return 0;
}

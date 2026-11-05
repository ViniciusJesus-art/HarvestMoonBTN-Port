#include "save_fixture.h"
#include "horse.h"
#include <stdio.h>
#include <stdlib.h>
int main(void)
{
    HmbtnSession *s=save_fixture(13);HmbtnHorse *h=&s->horse;char error[256]={0};
    assert(!h->owned && s->village.horse==h && s->menu.horse==h && s->livestock.horse==h);
    assert(hmbtn_horse_spawn(h,&s->game.clock) && !hmbtn_horse_spawn(h,&s->game.clock));
    assert(h->area==92 && h->affection==10 && h->actor.position[0]==460 && h->actor.position[2]==600);
    h->name[0]=0x100;h->name[1]=0xffff;
    assert(hmbtn_horse_talk(h)==600 && !h->called_today && h->track==13);
    assert(hmbtn_horse_brush(h) && !hmbtn_horse_brush(h) && h->affection==10);
    h->called_today=true;hmbtn_horse_new_day(h,4);assert(h->affection==14 && !h->called_today && !h->brushed_today);
    hmbtn_horse_new_day(h,0);assert(h->affection==13);
    h->area=0;hmbtn_horse_new_day(h,1);assert(h->affection==7);
    h->affection=0;hmbtn_horse_new_day(h,4);assert(!h->affection);
    h->area=92;h->affection=255;h->called_today=h->brushed_today=true;
    hmbtn_horse_new_day(h,4);assert(h->affection==255 && hmbtn_horse_talk(h)==601);
    const unsigned values[]={99,100,199,200},increments[]={1,2,2,3};
    for(unsigned i=0;i<4;++i) { h->affection=values[i];h->training=0;h->trained_today=true;hmbtn_horse_new_day(h,0);assert(h->training==increments[i]); }
    h->training=254;h->trained_today=true;hmbtn_horse_new_day(h,0);assert(h->training==255);
    HmbtnIntroActor player={.facing=2};h->affection=255;
    assert(hmbtn_horse_call_tick(h,&player,92,HMBTN_PAD_L2) && player.sprite.animation==69 && h->whistling);
    assert(!hmbtn_save_available(s,error,sizeof(error)));
    player.animation_done=true;assert(hmbtn_horse_call_tick(h,&player,92,0) && h->called_today && h->approaching && h->track==14);
    memcpy(player.position,h->actor.position,sizeof(player.position));player.position[2]+=400;player.facing=0;
    assert(hmbtn_horse_target(h,92,&player,224) && !hmbtn_horse_target(h,0,&player,224));
    const int32_t from[3]={460,0,1100},into[3]={460,0,900},away[3]={460,0,1200};
    assert(hmbtn_horse_blocked(h,92,from,into) && !hmbtn_horse_blocked(h,92,from,away));
    uint8_t *bytes=NULL;size_t count=0;assert(hmbtn_save_encode(s,&bytes,&count,error,sizeof(error)));
    HmbtnSession *loaded=hmbtn_save_decode(bytes,count,error,sizeof(error));assert(loaded);
    assert(loaded->horse.owned && loaded->horse.called_today && loaded->horse.name[0]==0x100
        && loaded->horse.training==255 && loaded->village.horse==&loaded->horse && loaded->livestock.horse==&loaded->horse);
    loaded->horse.affection=10;assert(hmbtn_session_morning(loaded,true) && loaded->horse.affection==12);
    assert(!hmbtn_session_morning(loaded,true) && loaded->horse.affection==12);
    free(bytes);hmbtn_session_destroy(loaded);
    /* Growth follows stored calendar fields, not elapsed 90/120-day counters. */
    hmbtn_horse_clear(h);HmbtnFarmClock c={0,1,10,0,6,0,0};assert(hmbtn_horse_spawn(h,&c));
    c.day=29;hmbtn_horse_morning(h,0,&c);assert(!h->adult);
    c.year=1;c.season=0;hmbtn_horse_morning(h,0,&c);assert(!h->adult);
    c.season=1;c.day=9;hmbtn_horse_morning(h,0,&c);assert(!h->adult);
    c.day=10;h->in_event=true;hmbtn_horse_morning(h,0,&c);assert(!h->adult);
    h->in_event=false;hmbtn_horse_morning(h,0,&c);assert(h->adult);
    s->area=s->intro.area=s->carry.area=92;s->game.inventory.held_item=(HmbtnStoredTool){0};
    memcpy(s->intro.player.position,h->actor.position,sizeof(h->actor.position));s->intro.player.position[2]+=400;
    s->intro.player.facing=0;h->actor.facing=3;int sound;HmbtnDogWorld world={.area=92,.player=&s->intro.player};
    assert(hmbtn_horse_ride_action(h,&s->intro.player,&s->control,&world,HMBTN_PAD_CROSS,&sound));
    assert(h->riding && h->trained_today && h->ride_phase==1 && sound==6 && s->intro.player.sprite.animation==53);
    assert(!hmbtn_save_available(s,error,sizeof(error)) && !hmbtn_horse_blocked(h,92,from,into));
    s->intro.player.animation_done=true;
    assert(hmbtn_horse_ride_action(h,&s->intro.player,&s->control,&world,0,&sound) && !h->ride_phase);
    assert(hmbtn_save_encode(s,&bytes,&count,error,sizeof(error)));
    loaded=hmbtn_save_decode(bytes,count,error,sizeof(error));assert(loaded && loaded->horse.riding && loaded->horse.adult);
    free(bytes);hmbtn_session_destroy(loaded);
    h->affection=199;h->called_today=true;h->training=0;hmbtn_horse_morning(h,0,&c);assert(h->training==3 && h->affection==201);
    int32_t resting[3];memcpy(resting,h->actor.position,sizeof(resting));
    assert(hmbtn_session_morning(s,true) && !h->riding && !h->ride_phase && h->area==92);
    assert(!memcmp(resting,h->actor.position,sizeof(resting)) && h->actor.sprite.animation==0);
    assert(!hmbtn_session_morning(s,true));
    hmbtn_horse_clear(h);assert(!h->owned && !h->riding && h->name[0]==0xffff);
    hmbtn_session_destroy(s);
    puts("Horse: calendar growth, mounted save/guards, unique acquisition, source care/training thresholds, weather, L2, collision and save/morning deduplication passed");return 0;
}

#include "save_fixture.h"
#include "psx_data.h"
#include <stdlib.h>
#include <stdio.h>
int main(void)
{
    HmbtnSession *s=save_fixture(1);HmbtnDog *d=&s->dog;char error[256]={0};
    HmbtnFarmClock c={0,1,29,0,12,0,0};HmbtnInventory hand={0};
    hmbtn_dog_morning(d,0,&c,&hand);assert(!d->adult);
    c.season=2;c.day=0;d->held=true;hand.held_item=(HmbtnStoredTool){HMBTN_YOUNG_DOG,0,1};
    hmbtn_dog_morning(d,0,&c,&hand);assert(d->adult && d->held && hand.held_item.id==HMBTN_ADULT_DOG);
    const int32_t point[3]={-1800,0,3100};d->held=false;d->area=0;hmbtn_dog_ball_place(d,0,point);d->ball.armed=true;
    assert(!hmbtn_dog_fetch(d,0,5) && !hmbtn_dog_fetch(d,0,21) && !hmbtn_dog_fetch(d,80,12));
    d->ball.position[0]=d->actor.position[0]+2000;assert(!hmbtn_dog_fetch(d,0,12));
    d->ball.position[0]--;assert(hmbtn_dog_fetch(d,0,12) && d->trained_today && d->fetch==1 && !hmbtn_dog_fetch(d,0,12));
    /* Training uses affection after care, before the weather penalty; one daily bit. */
    const unsigned affection[]={0,9,10,19,20,255},gain[]={1,1,2,2,3,3};
    for(unsigned i=0;i<6;++i) {
        d->affection=affection[i];d->training=0;d->trained_today=true;d->called_today=d->handled_today=false;
        hmbtn_dog_new_day(d,0);assert(d->training==gain[i] && d->affection==affection[i] && !d->trained_today);
        hmbtn_dog_new_day(d,0);assert(d->training==gain[i]);
    }
    d->affection=8;d->training=254;d->handled_today=d->trained_today=true;hmbtn_dog_new_day(d,1);
    assert(d->affection==5 && d->training==255);
    /* Original trigger 286: farmhouse arrival, counter 16 >=3, adult/gold/free hand/sun. */
    uint8_t table[38];memset(table,255,sizeof(table));hmbtn_write_le16(table,0x91e);table[2]=40;table[3]=16;
    hmbtn_write_le16(table+4,3);hmbtn_write_le16(table+6,127);hmbtn_write_le32(table+8,0x3fffffff);
    table[12]=0xf7;hmbtn_write_le32(table+24,0xfffff);hmbtn_write_le32(table+28,0x7ff98000);table[33]=81;table[34]=2;table[35]=0;
    HmbtnFarmTriggers t;assert(hmbtn_farm_triggers_open(&t,table,sizeof(table),error,sizeof(error)));t.economy=&s->game.economy;
    s->game.inventory.held_item=(HmbtnStoredTool){0};s->village.days.count0[16]=3;s->game.economy.gold=100;d->ball.armed=false;
    assert(hmbtn_farm_event_trigger(&t,&s->village,&s->game.clock,0,2,2,0,&s->game.inventory)==286);
    d->adult=false;assert(!hmbtn_farm_event_trigger(&t,&s->village,&s->game.clock,0,2,2,0,&s->game.inventory));d->adult=true;
    s->game.economy.gold=99;assert(!hmbtn_farm_event_trigger(&t,&s->village,&s->game.clock,0,2,2,0,&s->game.inventory));s->game.economy.gold=100;
    for(unsigned weather=1;weather<5;++weather) assert(!hmbtn_farm_event_trigger(&t,&s->village,&s->game.clock,weather,2,2,0,&s->game.inventory));
    d->ball.armed=true;assert(!hmbtn_farm_event_trigger(&t,&s->village,&s->game.clock,0,2,2,0,&s->game.inventory));
    /* Persist the real mouth/return cursor; no copy of the ball in the ground stamp. */
    d->ball.placed=false;d->fetch=3;d->fetch_time=23;d->fetch_step=2;d->phase=7;d->approaching=true;d->trained_today=true;
    uint8_t *bytes=NULL;size_t length=0;assert(hmbtn_save_encode(s,&bytes,&length,error,sizeof(error)));
    HmbtnSession *restored=hmbtn_save_decode(bytes,length,error,sizeof(error));assert(restored);
    assert(restored->dog.adult && restored->dog.training==255 && restored->dog.fetch==3 && restored->dog.fetch_time==23
        && !restored->dog.ball.placed && restored->dog.ball.armed && restored->dog.trained_today && restored->carry.dog==&restored->dog);
    hmbtn_dog_new_day(&restored->dog,0);assert(restored->dog.ball.placed && !restored->dog.fetch);
    assert(hmbtn_dog_ball_disarm(&restored->dog) && !restored->dog.ball.armed && !restored->dog.ball.placed);
    free(bytes);hmbtn_session_destroy(restored);hmbtn_session_destroy(s);
    puts("Adult dog/ball: seasonal promotion and held identity, training thresholds/deduplication/clamp, Won prerequisites, persisted mouth/return and morning stamp passed");return 0;
}

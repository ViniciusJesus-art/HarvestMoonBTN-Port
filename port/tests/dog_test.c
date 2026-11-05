#include "save_fixture.h"
#include "dog.h"
#include "psx_data.h"
#include <stdio.h>
#include <stdlib.h>
static void floor_bytes(uint8_t b[88],unsigned flags)
{
    memset(b,0,88);hmbtn_write_le32(b,0x42);hmbtn_write_le32(b+8,1);
    hmbtn_write_le32(b+12,28);hmbtn_write_le32(b+16,4);hmbtn_write_le32(b+28,60);hmbtn_write_le32(b+32,1);
    for(unsigned i=0;i<4;++i) { hmbtn_write_le16(b+40+i*8,(uint16_t)(i&1 ? 2000 : -2000));hmbtn_write_le16(b+44+i*8,(uint16_t)(i<2 ? -2000 : 2000)); }
    hmbtn_write_le16(b+72,(uint16_t)flags);hmbtn_write_le16(b+74,0x2901);
    for(unsigned i=0;i<4;++i) hmbtn_write_le16(b+80+i*2,(uint16_t)i);
}
static bool obstruction(const void *ctx,const int32_t from[3],const int32_t to[3])
{ (void)from;(void)to;return *(const bool *)ctx; }
int main(void)
{
    HmbtnSession *s=save_fixture(9);HmbtnDog *d=&s->dog;char error[256]={0};
    assert(d->affection==10 && d->area==0 && d->actor.position[0]==-1800 && d->actor.position[2]==3100);
    assert(s->village.dog==d && s->menu.dog==d);
    HmbtnIntroActor player={0};HmbtnInventory hand={0};int sound;
    uint8_t bytes[88];floor_bytes(bytes,0);HmbtnModel ground;
    assert(hmbtn_model_open(&ground,bytes,sizeof(bytes),error,sizeof(error)));
    bool blocked=false;HmbtnDogWorld world={0,&ground,NULL,&player,&blocked,obstruction};
    for(unsigned facing=0;facing<4;++facing) {
        hmbtn_dog_init(d);memset(d->actor.position,0,sizeof(d->actor.position));player=(HmbtnIntroActor){.facing=facing};
        static const int delta[4][2]={{0,-1},{-1,0},{0,1},{1,0}};
        player.position[0]=-delta[facing][0]*128;player.position[2]=-delta[facing][1]*128;
        assert(hmbtn_dog_carry_tick(d,&hand,&player,&world,HMBTN_PAD_CROSS,&sound));
        assert(d->held && d->carry_phase==1 && sound==7 && hand.held_item.id==HMBTN_YOUNG_DOG && player.sprite.animation==37+facing);
        assert(!hmbtn_inventory_cycle(&hand,false,false) && !hmbtn_inventory_swap(&hand,9,0) && !hmbtn_inventory_swap(&hand,9,10));
        player.animation_done=true;assert(hmbtn_dog_carry_tick(d,&hand,&player,&world,0,&sound) && !d->carry_phase);
        blocked=true;assert(hmbtn_dog_carry_tick(d,&hand,&player,&world,HMBTN_PAD_CROSS,&sound) && d->held && !d->carry_phase);
        blocked=false;floor_bytes(bytes,1);assert(hmbtn_model_open(&ground,bytes,sizeof(bytes),error,sizeof(error)));
        assert(hmbtn_dog_carry_tick(d,&hand,&player,&world,HMBTN_PAD_CROSS,&sound) && d->held && !d->carry_phase);
        floor_bytes(bytes,0);assert(hmbtn_model_open(&ground,bytes,sizeof(bytes),error,sizeof(error)));
        hmbtn_dog_latch_place(d,80);assert(d->area==80);world.area=80;
        assert(hmbtn_dog_carry_tick(d,&hand,&player,&world,HMBTN_PAD_CROSS,&sound) && d->carry_phase==2);
        player.animation_done=true;assert(hmbtn_dog_carry_tick(d,&hand,&player,&world,0,&sound));
        assert(!d->held && !hand.held_item.id && sound==9 && d->handled_today && d->praise==45 && d->affection==10);
        hmbtn_dog_new_day(d,1);assert(d->affection==12 && !d->handled_today);
        hmbtn_dog_new_day(d,1);assert(d->affection==11);world.area=0;
    }
    /* Repeat care is latched for the day; adverse weather uses the original place list. */
    hmbtn_dog_init(d);d->handled_today=d->called_today=true;hmbtn_dog_new_day(d,1);assert(d->affection==9);
    d->area=112;d->handled_today=true;hmbtn_dog_new_day(d,4);assert(d->affection==11); /* coop is indoors */
    d->area=68;hmbtn_dog_new_day(d,2);assert(d->affection==5);
    d->affection=255;d->handled_today=d->called_today=true;d->area=80;hmbtn_dog_new_day(d,0);assert(d->affection==255);
    d->affection=0;d->area=0;hmbtn_dog_new_day(d,1);assert(!d->affection);
    /* Whistle poses/response probability use the original L1 path and one daily flag. */
    hmbtn_dog_init(d);player=(HmbtnIntroActor){.facing=2};
    for(unsigned attempt=0;attempt<30 && !d->called_today;++attempt) {
        assert(hmbtn_dog_call_tick(d,&player,0,HMBTN_PAD_L1,&sound) && d->whistling && player.sprite.animation==69);
        player.animation_done=true;assert(hmbtn_dog_call_tick(d,&player,0,0,&sound) && !d->whistling);
    }
    assert(d->called_today && d->approaching && d->affection==10);
    /* A held reference blocks saving; the ground record and daily care survive decoding. */
    s->game.inventory.held_item=(HmbtnStoredTool){HMBTN_YOUNG_DOG,0,1};d->held=true;
    assert(!hmbtn_save_available(s,error,sizeof(error)));
    hmbtn_dog_release(d,&s->game.inventory,&s->intro.player,80,&ground);
    assert(!d->held && !s->game.inventory.held_item.id && d->area==80 && d->handled_today);
    uint8_t *encoded=NULL;size_t length=0;assert(hmbtn_save_encode(s,&encoded,&length,error,sizeof(error)));
    HmbtnSession *loaded=hmbtn_save_decode(encoded,length,error,sizeof(error));assert(loaded);
    assert(loaded->dog.handled_today && loaded->dog.called_today && loaded->dog.affection==10
        && loaded->village.dog==&loaded->dog && loaded->menu.dog==&loaded->dog);
    assert(hmbtn_session_morning(loaded,true) && loaded->dog.affection==14);
    assert(!hmbtn_session_morning(loaded,true) && loaded->dog.affection==14);
    free(encoded);hmbtn_session_destroy(loaded);hmbtn_session_destroy(s);
    puts("Dog: initial record, four facings, conserved/rejected drops, hand ownership, indoor/weather care, L1, daily deduplication and save/morning passed");return 0;
}

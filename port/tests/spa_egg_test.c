#include "save_fixture.h"
#include "psx_data.h"
#include <stdio.h>
#include <stdlib.h>
static void zone(uint8_t *s,unsigned slot,unsigned kind)
{
    uint8_t *z=s+16+slot*44;hmbtn_write_le16(z,0x4000);hmbtn_write_le16(z+2,(uint16_t)kind);
    for(unsigned a=0;a<3;++a) { hmbtn_write_le16(z+14+a*2,(uint16_t)-100);hmbtn_write_le16(z+20+a*2,100); }
}
static HmbtnThrownItem egg(unsigned id)
{ return (HmbtnThrownItem){.item={(uint16_t)id,3,1},.position={0,1,44},.area=72}; }
int main(void)
{
    char error[256]={0};uint8_t b[88]={0},section[0x2000]={0};
    hmbtn_write_le32(b,0x42);hmbtn_write_le32(b+8,1);hmbtn_write_le32(b+12,28);hmbtn_write_le32(b+16,4);
    hmbtn_write_le32(b+28,60);hmbtn_write_le32(b+32,1);
    for(unsigned i=0;i<4;++i) {
        hmbtn_write_le16(b+40+i*8,(uint16_t)(i&1 ? 2000 : -2000));
        hmbtn_write_le16(b+44+i*8,(uint16_t)(i<2 ? -2000 : 2000));hmbtn_write_le16(b+80+i*2,(uint16_t)i);
    }
    hmbtn_write_le16(b+72,0x40);hmbtn_write_le16(b+74,0x2901);
    HmbtnModel ground;assert(hmbtn_model_open(&ground,b,sizeof(b),error,sizeof(error)));zone(section,1,38);
    uint8_t ani[146]={0};hmbtn_write_le16(ani+23*4,5);
    for(unsigned i=0;i<5;++i) ani[96+i*10+6]=6;
    ani[143]=0xff;
    HmbtnAnimation splash={.data=ani,.size=sizeof(ani),.count={0,0,0,24,5},.section={0,0,0,0,96}};
    const unsigned ids[]={0x122,0x123,0x124,0xe8,0x125,0x87,87,163};
    for(unsigned i=0;i<8;++i) {
        HmbtnFarmCarry c={.area=72};c.thrown[0]=egg(ids[i]);
        hmbtn_farm_carry_flight(&c,NULL,&ground,NULL,section,sizeof(section));
        assert(c.thrown[0].phase==(i<5 ? HMBTN_THROWN_EGG_WATER : HMBTN_THROWN_WATER) && c.sound==16 && !c.discarded);
        for(unsigned t=0;t<14;++t) {
            assert(hmbtn_farm_carry_splashes(&c,&splash,error,sizeof(error)) && c.thrown[0].item.id==ids[i]);
            assert(c.thrown[0].phase!=HMBTN_THROWN_EGG_READY);
        }
        assert(hmbtn_farm_carry_splashes(&c,&splash,error,sizeof(error)));
        assert(i<5 ? c.thrown[0].item.id==ids[i] && c.thrown[0].phase==HMBTN_THROWN_EGG_READY : !c.thrown[0].item.id);
        assert(hmbtn_farm_carry_splashes(&c,&splash,error,sizeof(error)));
    }
    /* Dry ground, inactive/truncated/missing zone, wrong place, strict box
     * edges, asymmetric Y slack and earlier overlapping active zones. */
    for(unsigned test=0;test<12;++test) {
        memset(section,0,sizeof(section));zone(section,1,38);hmbtn_write_le16(b+72,test==0 ? 0 : 0x40);
        HmbtnFarmCarry c={.area=72};c.thrown[0]=egg(0x122);
        if(test==1) hmbtn_write_le16(section+60,0);
        if(test==2) hmbtn_write_le16(section+62,47);
        if(test==3) c.thrown[0].position[0]=-100;
        if(test==4) c.thrown[0].position[0]=100;
        if(test==5) c.thrown[0].position[2]=-56;
        if(test==6) c.thrown[0].position[2]=144;
        if(test==7) hmbtn_write_le16(section+60+16,128);
        if(test==8) hmbtn_write_le16(section+60+22,(uint16_t)-120);
        if(test==9) zone(section,0,47);
        hmbtn_farm_carry_flight(&c,NULL,&ground,NULL,test==10 ? NULL : section,test==11 ? 2215 : sizeof(section));
        assert(c.thrown[0].phase==(test==0 ? 1 : HMBTN_THROWN_WATER));
    }
    /* All three water bits use the same conversion, never a hardcoded area. */
    memset(section,0,sizeof(section));zone(section,0,38);
    for(unsigned bit=2;bit<=0x40;bit=bit==2 ? 4 : 0x40) {
        hmbtn_write_le16(b+72,(uint16_t)bit);HmbtnFarmCarry c={0};c.thrown[0]=egg(0x125);c.thrown[0].area=0;
        hmbtn_farm_carry_flight(&c,NULL,&ground,NULL,section,sizeof(section));assert(c.thrown[0].phase==HMBTN_THROWN_EGG_WATER);
        if(bit==0x40) break;
    }
    HmbtnSession *s=save_fixture(1);HmbtnInventory *h=&s->game.inventory;
    s->carry.thrown[0]=egg(0x122);s->carry.thrown[0].phase=HMBTN_THROWN_EGG_READY;
    assert(!hmbtn_save_available(s,error,sizeof(error)));
    /* Outside the current area a pending flight cannot advance or award. */
    hmbtn_farm_carry_flight(&s->carry,NULL,&ground,NULL,section,sizeof(section));
    assert(s->carry.thrown[0].phase==HMBTN_THROWN_EGG_READY);
    s->carry.thrown[0]=(HmbtnThrownItem){0};assert(hmbtn_save_available(s,error,sizeof(error)));
    HmbtnStoredTool old=h->held_item;HmbtnStoredTool bag[8];memcpy(bag,h->items,sizeof(bag));
    assert(!hmbtn_event_item_show(&s->event_item,&s->intro.player,h,0x87));
    assert(hmbtn_event_item_spa_egg(&s->event_item,&s->intro.player,h));assert(!hmbtn_save_available(s,error,sizeof(error)));
    assert(h->held_item.id==old.id && s->event_item.shown==0x87 && s->intro.player.sprite.animation==11+s->intro.player.facing);
    assert(!hmbtn_event_item_spa_egg(&s->event_item,&s->intro.player,h));
    hmbtn_event_item_tick(&s->event_item,&s->intro.player,h,&s->farm_action);assert(h->held_item.id==old.id);
    s->intro.player.animation_done=true;hmbtn_event_item_tick(&s->event_item,&s->intro.player,h,&s->farm_action);
    assert(h->held_item.id==0x87 && !h->held_item.modifier && h->held_item.count==1 && !memcmp(bag,h->items,sizeof(bag)));
    assert(hmbtn_save_available(s,error,sizeof(error)));
    uint8_t *data=NULL;size_t size=0;assert(hmbtn_save_encode(s,&data,&size,error,sizeof(error)));
    HmbtnSession *loaded=hmbtn_save_decode(data,size,error,sizeof(error));assert(loaded && loaded->game.inventory.held_item.id==0x87);
    free(data);hmbtn_session_destroy(loaded);hmbtn_session_destroy(s);
    const HmbtnFarmItem *info=hmbtn_farm_item(0x87);assert(info->sell_price==80 && info->stamina==4 && info->fatigue==-1);
    puts("Hot spring eggs: five eggs, water/first-zone/bounds, full splash, forced receipt/full bag, save guards and stable record passed");return 0;
}

#include "save_fixture.h"
#include "psx_data.h"
#include <stdlib.h>
#include <stdio.h>
static void entry(uint8_t *section)
{
    memset(section,0,0x2000);hmbtn_write_le16(section+2,12);uint8_t *z=section+16;
    hmbtn_write_le16(z,0x1000);hmbtn_write_le16(z+2,0xfffe);hmbtn_write_le16(z+8,2);hmbtn_write_le16(z+10,46);
    hmbtn_write_le16(z+14,(uint16_t)-9208);hmbtn_write_le16(z+20,(uint16_t)-8538);
    hmbtn_write_le16(z+18,4300);hmbtn_write_le16(z+24,4780);
}
int main(void)
{
    char error[256]={0};uint8_t section[0x2000];entry(section);
    HmbtnHotSpring bath={0};HmbtnIntroActor p={.position={-8838,0,4500},.facing=2},before=p;
    assert(hmbtn_hot_spring_begin(&bath,&p,true,section,sizeof(section),HMBTN_PAD_CROSS) && !bath.phase && !memcmp(&p,&before,sizeof(p)));
    assert(!hmbtn_hot_spring_begin(&bath,&p,false,section,2215,HMBTN_PAD_CROSS));
    assert(!hmbtn_hot_spring_begin(&bath,&p,false,section,sizeof(section),0));
    p.facing=0;assert(!hmbtn_hot_spring_begin(&bath,&p,false,section,sizeof(section),HMBTN_PAD_CROSS));p=before;
    p.position[0]=-9208;assert(!hmbtn_hot_spring_begin(&bath,&p,false,section,sizeof(section),HMBTN_PAD_CROSS));p=before;
    hmbtn_write_le16(section+2,1);assert(!hmbtn_hot_spring_begin(&bath,&p,false,section,sizeof(section),HMBTN_PAD_CROSS));entry(section);
    /* Movement supplies a 48-unit forward probe; do not add it twice. */
    hmbtn_write_le16(section+0x948+24,4400);
    HmbtnHotSpring barrier={0};HmbtnAreaDoors doors={0};hmbtn_hot_spring_bind(&barrier,section,sizeof(section));
    int32_t from[3]={-8838,0,4274},to[3]={-8838,0,4363};
    assert(!hmbtn_hot_spring_blocked(&barrier,&doors,from,to));
    from[2]=4315;to[2]=4404;assert(hmbtn_hot_spring_blocked(&barrier,&doors,from,to));
    from[2]=4450;to[2]=4390;assert(hmbtn_hot_spring_blocked(&barrier,&doors,from,to));
    from[2]=4396;to[2]=4400;assert(hmbtn_hot_spring_blocked(&barrier,&doors,from,to));
    doors.bound=true;doors.models[0].phase=HMBTN_DOOR_HELD;
    assert(!hmbtn_hot_spring_blocked(&barrier,&doors,from,to));
    HmbtnHouseExit exit;unsigned model;assert(!hmbtn_world_door_at(section,sizeof(section),&p,&exit,&model));
    hmbtn_write_le16(section+26,47);p.facing=0;hmbtn_write_le16(section+24,0);
    assert(!hmbtn_world_door_at(section,sizeof(section),&p,&exit,&model));entry(section);p=before;
    uint8_t b[88]={0};hmbtn_write_le32(b,0x42);hmbtn_write_le32(b+8,1);hmbtn_write_le32(b+12,28);hmbtn_write_le32(b+16,4);
    hmbtn_write_le32(b+28,60);hmbtn_write_le32(b+32,1);
    for(unsigned i=0;i<4;++i) {
        hmbtn_write_le16(b+40+i*8,(uint16_t)(i&1 ? 20000 : -20000));
        hmbtn_write_le16(b+44+i*8,(uint16_t)(i<2 ? -20000 : 20000));hmbtn_write_le16(b+80+i*2,(uint16_t)i);
    }
    hmbtn_write_le16(b+74,0x2901);HmbtnModel ground;assert(hmbtn_model_open(&ground,b,sizeof(b),error,sizeof(error)));
    assert(hmbtn_hot_spring_begin(&bath,&p,false,section,sizeof(section),HMBTN_PAD_CROSS));
    for(unsigned i=0;i<37;++i) hmbtn_hot_spring_tick(&bath,&p,&ground,0);
    assert(bath.phase==HMBTN_SPRING_ENTER && bath.bank==0 && bath.sound==6 && p.position[0]==-8838 && p.sprite.animation==4);
    for(unsigned i=0;i<100 && bath.phase==HMBTN_SPRING_ENTER;++i) hmbtn_hot_spring_tick(&bath,&p,&ground,HMBTN_PAD_CROSS);
    assert(bath.phase==HMBTN_SPRING_SPLASH && bath.sound==16);
    hmbtn_hot_spring_tick(&bath,&p,&ground,0);assert(bath.phase==HMBTN_SPRING_SPLASH);
    p.animation_done=true;hmbtn_hot_spring_tick(&bath,&p,&ground,0);assert(bath.phase==HMBTN_SPRING_SOAK && bath.bank==1);
    HmbtnFarmAction status={.stamina=97,.max_stamina=100,.fatigue=2};
    hmbtn_hot_spring_minute(&bath,&status);assert(status.stamina==99 && status.fatigue==1);
    hmbtn_hot_spring_minute(&bath,&status);assert(status.stamina==100 && !status.fatigue);
    hmbtn_hot_spring_minute(&bath,&status);assert(status.stamina==100 && !status.fatigue);
    for(unsigned i=0;i<300;++i) hmbtn_hot_spring_tick(&bath,&p,&ground,0);
    assert(bath.phase==HMBTN_SPRING_REST && p.sprite.animation==1);
    hmbtn_hot_spring_tick(&bath,&p,&ground,HMBTN_PAD_CROSS);assert(bath.phase==HMBTN_SPRING_SOAK && p.sprite.animation==0);
    hmbtn_hot_spring_tick(&bath,&p,&ground,HMBTN_PAD_CROSS);assert(bath.phase==HMBTN_SPRING_LEAVE && bath.bank==16 && bath.sound==6);
    status.stamina=10;status.fatigue=5;hmbtn_hot_spring_minute(&bath,&status);assert(status.stamina==10 && status.fatigue==5);
    for(unsigned i=0;i<100 && bath.phase==HMBTN_SPRING_LEAVE;++i) hmbtn_hot_spring_tick(&bath,&p,&ground,0);
    assert(bath.phase==HMBTN_SPRING_WALK && bath.bank==-2 && p.facing==2);
    for(unsigned i=0;i<20;++i) hmbtn_hot_spring_tick(&bath,&p,&ground,0);
    assert(!bath.phase && bath.finished && !p.pose && !p.position[1]);
    /* The actual spring has an automatic exit zone. It takes precedence
     * over the launch timer/turn, and walks down through the changing room. */
    bath=(HmbtnHotSpring){.phase=HMBTN_SPRING_LEAVE,.landed=true,.exit_known=true,
        .exit_box={-9208,-8550,4300,4780}};
    p=(HmbtnIntroActor){.position={-8838,0,4600},.facing=0};
    hmbtn_hot_spring_tick(&bath,&p,&ground,0);
    assert(bath.phase==HMBTN_SPRING_WALK && bath.exit_pending && bath.walk_ticks==37 && p.facing==0 && bath.bank==-2);
    for(unsigned i=0;i<37;++i) hmbtn_hot_spring_tick(&bath,&p,&ground,0);
    assert(!bath.phase && p.position[2]<4300 && !p.pose);
    HmbtnSession *s=save_fixture(1);s->hot_spring.phase=HMBTN_SPRING_SOAK;assert(!hmbtn_save_available(s,error,sizeof(error)));
    s->hot_spring=(HmbtnHotSpring){0};assert(hmbtn_save_available(s,error,sizeof(error)));
    uint8_t *data=NULL;size_t size=0;assert(hmbtn_save_encode(s,&data,&size,error,sizeof(error)));
    HmbtnSession *loaded=hmbtn_save_decode(data,size,error,sizeof(error));assert(loaded && !loaded->hot_spring.phase);
    free(data);hmbtn_session_destroy(loaded);hmbtn_session_destroy(s);
    puts("Hot spring: entry/occupied hand/bounds, bank sequence, jumps, minute healing/clamps, rest/cross, exit and save guards passed");return 0;
}

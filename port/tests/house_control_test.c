#include "house_control.h"
#include "psx_data.h"
#include "pad_buttons.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Line %d: %s (%s)\n",__LINE__,#x,error); return 1; } } while(0)

/* Three floor quads around a missing furniture footprint. The fourth quad
 * covers the same hole visually, but has the original non-ground flag. */
static size_t make_floor(uint8_t *b, bool multiple)
{
    const int16_t rect[4][4]={{-500,-500,-100,500},{100,-500,500,500},
        {-100,-500,100,0},{-100,0,100,500}};
    const unsigned objects=multiple ? 2u : 1u;
    const size_t vertices=12+objects*28+(multiple ? 20 : 0), primitives=vertices+128;
    memset(b,0,512); hmbtn_write_le32(b,0x42); hmbtn_write_le32(b+8,objects);
    for(unsigned i=0;i<objects;++i) {
        uint8_t *o=b+12+i*28;
        hmbtn_write_le32(o,(uint32_t)vertices-12); hmbtn_write_le32(o+4,16);
        hmbtn_write_le32(o+16,(uint32_t)primitives-12); hmbtn_write_le32(o+20,4);
    }
    if(multiple) {
        hmbtn_write_le32(b+68,1000);
        hmbtn_write_le16(b+72,30000); /* Far first centre: must skip to object 1. */
    }
    for(unsigned i=0;i<4;++i) {
        for(unsigned k=0;k<4;++k) {
            uint8_t *v=b+vertices+(i*4+k)*8;
            hmbtn_write_le16(v,(uint16_t)rect[i][k&1 ? 2 : 0]);
            hmbtn_write_le16(v+4,(uint16_t)rect[i][k<2 ? 1 : 3]);
        }
        uint8_t *p=b+primitives+i*16;
        hmbtn_write_le16(p,i==3 ? 0x8000 : 0); hmbtn_write_le16(p+2,0x2901);
        for(unsigned k=0;k<4;++k) hmbtn_write_le16(p+8+k*2,(uint16_t)(i*4+k));
    }
    return primitives+64;
}
int main(void)
{
    char error[256]={0}; uint8_t bytes[512]; HmbtnModel model; HmbtnGroundHit hit;
    size_t size=make_floor(bytes,false);
    CHECK(hmbtn_model_open(&model,bytes,size,error,sizeof(error)));
    int32_t pos[3]={0,0,-300};
    CHECK(hmbtn_ground_find(&model,pos,0xE080,200,&hit) && hit.height==0);
    pos[2]=100;
    CHECK(!hmbtn_ground_find(&model,pos,0xE080,200,&hit));
    CHECK(hmbtn_ground_find(&model,pos,0,200,&hit) && hit.flags==0x8000);
    pos[0]=-500; CHECK(hmbtn_ground_find(&model,pos,0xE080,200,&hit));
    pos[0]=-501; CHECK(!hmbtn_ground_find(&model,pos,0xE080,200,&hit));
    pos[0]=-200; pos[1]=201; CHECK(!hmbtn_ground_find(&model,pos,0xE080,200,&hit));
    pos[1]=200; CHECK(hmbtn_ground_find(&model,pos,0xE080,200,&hit) && hit.height==0);
    pos[0]=INT32_MAX; CHECK(!hmbtn_ground_find(&model,pos,0xE080,200,&hit));
    /* Plane interpolation and the original first matching object order. */
    for(unsigned k=0;k<4;++k) {
        const int16_t y=(k&1) ? -10 : -50;
        hmbtn_write_le16(bytes+40+k*8+2,(uint16_t)y);
    }
    CHECK(hmbtn_model_open(&model,bytes,size,error,sizeof(error)));
    pos[0]=-300; pos[1]=0; pos[2]=0;
    CHECK(hmbtn_ground_find(&model,pos,0xE080,200,&hit) && hit.height==-30);
    size=make_floor(bytes,true);
    CHECK(hmbtn_model_open(&model,bytes,size,error,sizeof(error)));
    CHECK(hmbtn_ground_find(&model,pos,0xE080,200,&hit) && hit.object==1);
    size=make_floor(bytes,false);
    CHECK(hmbtn_model_open(&model,bytes,size,error,sizeof(error)));
    HmbtnIntroActor p={0}; p.position[2]=-300;
    HmbtnHouseControl control={0};
    hmbtn_house_control_tick(&control,&p,&model,HMBTN_PAD_UP);
    CHECK(p.position[2]==-300); /* Locked until the opening really finishes. */
    control.enabled=true;
    hmbtn_house_control_tick(&control,&p,&model,HMBTN_PAD_UP);
    CHECK(p.position[2]==-300 && p.sprite.animation==3 && p.facing==2);
    hmbtn_house_control_tick(&control,&p,&model,HMBTN_PAD_UP);
    CHECK(p.position[2]==-259);
    hmbtn_house_control_tick(&control,&p,&model,HMBTN_PAD_UP|HMBTN_PAD_RIGHT);
    CHECK(p.facing==2 && p.position[0]==0 && p.position[2]==-218);
    hmbtn_house_control_tick(&control,&p,&model,0);
    CHECK(p.position[2]==-218 && p.sprite.animation==0 && p.sprite.frame==2);
    hmbtn_house_control_tick(&control,&p,&model,HMBTN_PAD_CIRCLE|HMBTN_PAD_RIGHT);
    CHECK(p.position[0]==0 && p.sprite.animation==9);
    hmbtn_house_control_tick(&control,&p,&model,HMBTN_PAD_CIRCLE); /* Run forward without arrows. */
    CHECK(p.position[0]==73 && p.facing==3);
    p.position[0]=0; p.position[2]=-50; control.mode=2;
    for(unsigned i=0;i<60;++i) hmbtn_house_control_tick(&control,&p,&model,HMBTN_PAD_UP);
    CHECK(p.position[0]==0 && p.position[2]==-50 && control.blocked_frames==5);
    p.position[0]=450; p.position[2]=-300; control.mode=4;
    for(unsigned i=0;i<100;++i) hmbtn_house_control_tick(&control,&p,&model,HMBTN_PAD_RIGHT|HMBTN_PAD_CIRCLE);
    CHECK(p.position[0]<=500 && p.position[2]>=-500 && p.position[1]==0);
    /* Strict zone edges and flag filtering; destination coordinates stay integers. */
    uint8_t section[16+50*44]={0}; HmbtnHouseExit exit;
    for(unsigned i=0;i<50;++i) hmbtn_write_le16(section+16+i*44+2,0xFFFF);
    uint8_t *z=section+16;
    hmbtn_write_le16(z+2,0); hmbtn_write_le16(z+10,99);
    hmbtn_write_le16(z+14,100); hmbtn_write_le16(z+18,(uint16_t)-200);
    hmbtn_write_le16(z+20,200); hmbtn_write_le16(z+24,200);
    hmbtn_write_le16(z+38,(uint16_t)-4464); hmbtn_write_le16(z+42,4032);
    pos[0]=150; pos[1]=0; pos[2]=0;
    CHECK(hmbtn_house_exit_at(section,sizeof(section),pos,&exit));
    CHECK(exit.target==0 && exit.zone==0 && exit.entry[0]==-4464 && exit.entry[2]==4032);
    hmbtn_write_le16(z,0x4000); CHECK(!hmbtn_house_exit_at(section,sizeof(section),pos,&exit));
    hmbtn_write_le16(z,0); pos[0]=100;
    CHECK(!hmbtn_house_exit_at(section,sizeof(section),pos,&exit));
    CHECK(!hmbtn_house_exit_at(section,16,pos,&exit));
    control.exit_pending=true; p.position[0]=0;
    hmbtn_house_control_tick(&control,&p,&model,HMBTN_PAD_RIGHT);
    CHECK(p.position[0]==0);
    puts("Ground flags, furniture footprints, slopes, object clips, house movement and exit detection passed.");
    return 0;
}

#include "farm_action.h"
#include "psx_data.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Line %d: %s (%s)\n",__LINE__,#x,error);return 1; } } while(0)
static void floor_model(uint8_t *b)
{
    memset(b,0,128);hmbtn_write_le32(b,0x42);hmbtn_write_le32(b+8,1);
    hmbtn_write_le32(b+12,28);hmbtn_write_le32(b+16,4);
    hmbtn_write_le32(b+28,60);hmbtn_write_le32(b+32,1);
    for(unsigned i=0;i<4;++i) {
        hmbtn_write_le16(b+40+i*8,(uint16_t)(i&1 ? 15000 : -10000));
        hmbtn_write_le16(b+44+i*8,(uint16_t)(i<2 ? -10000 : 5000));
        hmbtn_write_le16(b+80+i*2,(uint16_t)i);
    }
    hmbtn_write_le16(b+74,0x2901);
}
int main(void)
{
    char error[256]={0};HmbtnFarmField f;hmbtn_farm_field_init(&f,NULL,0);
    CHECK(f.cells[0].value==0x167 && f.cells[1124].ground==0x167);
    int c,r;int32_t pos[3]={-4200,0,1400};
    CHECK(hmbtn_farm_world_cell(pos,&c,&r) && c==0 && r==0);
    pos[0]=-4401;CHECK(!hmbtn_farm_world_cell(pos,&c,&r));
    pos[0]=INT32_MAX;CHECK(!hmbtn_farm_world_cell(pos,&c,&r));
    const unsigned i=10*45+10;pos[0]=-200;pos[2]=-2600;
    CHECK(hmbtn_farm_world_cell(pos,&c,&r) && c==10 && r==10);
    const unsigned ids[]={0x3a,0x3e,0x40,0x3b,0x3c,0x3d,0x16c,0x16d,0x16e,0x16f,0x170,0x171,0x172,0x173,0x174};
    for(unsigned k=0;k<sizeof(ids)/sizeof(*ids);++k) {
        f.cells[i].value=(uint16_t)ids[k];
        CHECK(hmbtn_farm_field_probe(&f,pos)==(k<3 ? 1u : 2u));
    }
    unsigned supplies[4]={0};f.cells[i].value=0x3b;
    CHECK(!hmbtn_farm_field_tool(&f,10,10,3,0,20,supplies)); /* Basic hammer refuses boulder. */
    f.cells[i].value=0x3c;
    CHECK(!hmbtn_farm_field_tool(&f,10,10,3,1,100,supplies)); /* Rock requires silver. */
    hmbtn_farm_field_init(&f,NULL,0);f.cells[i].value=0x40;
    CHECK(!hmbtn_farm_field_tool(&f,10,10,1,0,1,supplies));
    CHECK(hmbtn_farm_field_tool(&f,10,10,0,0,1,supplies));
    CHECK(hmbtn_farm_field_tool(&f,10,10,1,0,1,supplies));
    CHECK(f.cells[i].value==0x168 && !hmbtn_farm_field_probe(&f,pos));
    CHECK(hmbtn_farm_field_tool(&f,10,10,4,0,1,supplies));
    CHECK(f.cells[i].value==0x169 && f.cells[i].ground==0x169);
    CHECK(!hmbtn_farm_field_tool(&f,10,10,4,0,1,supplies));
    CHECK(hmbtn_farm_field_tool(&f,10,10,3,0,1,supplies));
    CHECK(f.cells[i].value==0x167 && f.cells[i].ground==0x167);
    f.cells[i].value=0x3e;CHECK(hmbtn_farm_field_tool(&f,10,10,2,0,1,supplies));
    CHECK(supplies[1]==1 && f.cells[i].value==0x167);
    f.cells[i].value=0x3d;f.cells[i+1].value=0x172;f.cells[i+45].value=0x174;f.cells[i+46].value=0x173;
    CHECK(!hmbtn_farm_field_tool(&f,11,11,2,0,100,supplies));
    CHECK(!hmbtn_farm_field_tool(&f,11,11,2,1,11,supplies));
    CHECK(hmbtn_farm_field_tool(&f,11,11,2,1,12,supplies));
    CHECK(supplies[1]==7 && f.cells[i].value==0x167 && f.cells[i+46].value==0x167);
    uint8_t bytes[128];floor_model(bytes);HmbtnModel ground;
    CHECK(hmbtn_model_open(&ground,bytes,sizeof(bytes),error,sizeof(error)));
    f.cells[i].value=0x3a;HmbtnIntroActor p={0};p.position[0]=-600;p.position[2]=-2600;p.facing=3;
    HmbtnHouseControl control={.enabled=true};
    for(unsigned k=0;k<30 && !control.jumping;++k) hmbtn_farm_control_tick(&control,&p,&ground,&f,HMBTN_PAD_CIRCLE|HMBTN_PAD_RIGHT);
    CHECK(control.jumping && p.position[1]<0 && p.sprite.animation==10 && p.sprite.frame==3);
    for(unsigned k=0;k<20 && control.jumping;++k) hmbtn_farm_control_tick(&control,&p,&ground,&f,0);
    CHECK(!control.jumping && control.landed && p.position[1]==0 && p.position[0]>0);
    f.cells[i].value=0x3b;p.position[0]=-600;control.mode=0;
    for(unsigned k=0;k<100;++k) hmbtn_farm_control_tick(&control,&p,&ground,&f,HMBTN_PAD_CIRCLE|HMBTN_PAD_RIGHT);
    CHECK(!control.jumping && p.position[0]<-400);
    HmbtnHouseInteraction h;HmbtnGameState h_state;hmbtn_game_state_init(&h_state);hmbtn_house_interaction_init(&h,&h_state);h.inventory->equipped.id=6;
    HmbtnFarmAction a;hmbtn_farm_action_init(&a);hmbtn_farm_field_init(&f,NULL,0);
    p.position[0]=-600;p.position[2]=-2600;p.facing=3;
    CHECK(hmbtn_farm_action_tick(&a,&p,&h,&f,&ground,HMBTN_PAD_SQUARE,HMBTN_PAD_SQUARE));
    CHECK(a.phase==HMBTN_FARM_TOOL_CHARGE && p.sprite.animation==107);
    CHECK(hmbtn_farm_action_tick(&a,&p,&h,&f,&ground,0,0));
    CHECK(a.phase==HMBTN_FARM_TOOL_SWING && p.sprite.frame==1 && a.sound==34);
    CHECK(f.cells[i].value==0x167);p.animation_done=true;
    CHECK(hmbtn_farm_action_tick(&a,&p,&h,&f,&ground,0,0));
    CHECK(a.phase==HMBTN_FARM_ACTION_FREE && a.changed && a.stamina==99 && f.cells[i].value==0x168);
    h.inventory->equipped.id=21;h.inventory->equipped.count=0;
    CHECK(!hmbtn_farm_action_tick(&a,&p,&h,&f,&ground,HMBTN_PAD_SQUARE,0));
    f.cells[0]=(HmbtnFarmTile){0x169,0x169,0,0};
    f.cells[1]=(HmbtnFarmTile){0x16d,0x169,0,0};
    f.cells[2]=(HmbtnFarmTile){0x3a,0x167,0,0};
    hmbtn_farm_field_new_day(&f,0);
    CHECK(f.cells[0].value==0x168 && f.cells[0].ground==0x168);
    CHECK(f.cells[1].value==0x16d && f.cells[1].ground==0x168 && f.cells[2].value==0x3a);
    hmbtn_farm_field_new_day(&f,3);CHECK(f.cells[2].value==0x167 && f.cells[2].ground==0x167);
    puts("Original field rules, coordinates, tool requirements, 2x2 obstacles, supplies, hop/block and delayed tool commits passed.");
    return 0;
}

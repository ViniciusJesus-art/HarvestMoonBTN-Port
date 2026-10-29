#include "fishing.h"
#include "farm_carry.h"
#include "psx_data.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Fishing line %d: %s; %s\n",__LINE__,#x,error);return 1; } } while(0)
static void plane(uint8_t b[88],unsigned flags)
{
    memset(b,0,88);hmbtn_write_le32(b,0x42);hmbtn_write_le32(b+8,1);
    hmbtn_write_le32(b+12,28);hmbtn_write_le32(b+16,4);
    hmbtn_write_le32(b+28,60);hmbtn_write_le32(b+32,1);
    for(unsigned i=0;i<4;++i) {
        hmbtn_write_le16(b+40+i*8,(uint16_t)(i&1 ? 3000 : -3000));
        hmbtn_write_le16(b+44+i*8,(uint16_t)(i<2 ? -3000 : 3000));
        hmbtn_write_le16(b+80+i*2,(uint16_t)i);
    }
    hmbtn_write_le16(b+72,(uint16_t)flags);hmbtn_write_le16(b+74,0x2901);
}
int main(void)
{
    char error[256]={0};HmbtnFishing f={0};HmbtnFarmClock c={0,0,1,1,6,0,0};bool flags[512]={0};
    const unsigned waits[3][5]={{15,60,60,150,30},{60,180,180,210,90},{30,120,120,210,60}};
    const unsigned minutes[]={0,479,480,1079,1080,1319,1320,1439};
    for(unsigned i=0;i<8;++i) for(unsigned kind=0;kind<5;++kind) {
        f.kind=kind;f.random=0;
        const unsigned row=minutes[i]<480 || minutes[i]>=1320 ? 0 : minutes[i]<1080 ? 1 : 2;
        CHECK(hmbtn_fishing_wait(&f,minutes[i])==waits[row][kind] && f.pull==5 && f.random==12345);
    }
    const uint32_t before=f.random;CHECK(!hmbtn_fishing_wait(&f,1440) && f.random==before);
    /* Golden LCG seeds select specific percentile bands in the original tables. */
    const unsigned seeds[]={0,51,1,37,96,69,77,54,23};
    const unsigned catches[]={0,235,236,233,62,163,162,161,234};
    for(unsigned i=0;i<9;++i) {
        memset(flags,0,sizeof(flags));f=(HmbtnFishing){0};f.random=seeds[i];
        CHECK(hmbtn_fishing_roll(&f,&c,flags)==catches[i]);
    }
    CHECK(flags[0x5A]);f.random=23;CHECK(hmbtn_fishing_roll(&f,&c,flags)==0); /* Recipe taken only once. */
    memset(flags,0,sizeof(flags));f.random=70;CHECK(hmbtn_fishing_roll(&f,&c,flags)==0);
    flags[0x5F]=true;f.random=70;CHECK(hmbtn_fishing_roll(&f,&c,flags)==242 && flags[0x59] && (f.catch_flags&0x80));
    memset(flags,0,sizeof(flags));c.season=1;f.random=23;CHECK(hmbtn_fishing_roll(&f,&c,flags)==0);
    flags[0x5E]=true;f.random=23;CHECK(hmbtn_fishing_roll(&f,&c,flags)==240 && flags[0x57]);
    memset(flags,0,sizeof(flags));c.season=3;c.hour=22;c.minute=0;f.random=23;
    CHECK(hmbtn_fishing_roll(&f,&c,flags)==0);
    c.minute=1;f.random=23;CHECK(hmbtn_fishing_roll(&f,&c,flags)==241 && flags[0x58]);
    memset(flags,0,sizeof(flags));f.kind=1;f.random=70;CHECK(hmbtn_fishing_roll(&f,&c,flags)==0);
    for(unsigned i=0x55;i<=0x59;++i) flags[i]=true;
    f.random=70;CHECK(hmbtn_fishing_roll(&f,&c,flags)==237 && flags[0x54]);
    memset(flags,0,sizeof(flags));f.kind=2;f.random=70;CHECK(hmbtn_fishing_roll(&f,&c,flags)==0);
    flags[0x5B]=flags[0x5C]=flags[0x5D]=true;f.random=70;
    CHECK(hmbtn_fishing_roll(&f,&c,flags)==238 && flags[0x55]);
    f.kind=0;c.season=0;f.random=0;f.streak=11;
    CHECK(hmbtn_fishing_roll(&f,&c,flags)==161 && !f.streak);
    uint8_t b[88];plane(b,2);HmbtnModel ground;
    CHECK(hmbtn_model_open(&ground,b,sizeof(b),error,sizeof(error)));
    HmbtnHouseInteraction h;HmbtnGameState h_state;hmbtn_game_state_init(&h_state);hmbtn_house_interaction_init(&h,&h_state);h.inventory->equipped.id=56;
    HmbtnFarmAction status;hmbtn_farm_action_init(&status);HmbtnIntroActor p={0};
    c=(HmbtnFarmClock){0,0,1,1,6,0,0};f=(HmbtnFishing){0};
    CHECK(hmbtn_fishing_tick(&f,&p,&h,&status,&ground,&c,44,flags,HMBTN_PAD_SQUARE,HMBTN_PAD_SQUARE));
    CHECK(f.phase==HMBTN_FISHING_CAST && f.bank_pending && f.using_action && f.sound==15);
    for(unsigned i=0;i<200 && f.phase==HMBTN_FISHING_CAST;++i) {
        p.animation_done=true;CHECK(hmbtn_fishing_tick(&f,&p,&h,&status,&ground,&c,44,flags,0,HMBTN_PAD_SQUARE));
    }
    CHECK(f.phase==HMBTN_FISHING_WAIT && f.water && f.kind==0 && status.stamina==99);
    CHECK(hmbtn_fishing_tick(&f,&p,&h,&status,&ground,&c,44,flags,0,0) && f.phase==HMBTN_FISHING_REEL);
    p.animation_done=true;CHECK(hmbtn_fishing_tick(&f,&p,&h,&status,&ground,&c,44,flags,0,0));
    CHECK(f.phase==HMBTN_FISHING_FREE && !f.marker_visible && !h.inventory->held_item.id);
    /* A missed bite returns to waiting; releasing Square while hooked reels a catch. */
    f.phase=HMBTN_FISHING_HOOKED;f.pull=0;
    CHECK(hmbtn_fishing_tick(&f,&p,&h,&status,&ground,&c,44,flags,0,HMBTN_PAD_SQUARE) && f.phase==HMBTN_FISHING_WAIT);
    f.phase=HMBTN_FISHING_HOOKED;f.random=54;
    CHECK(hmbtn_fishing_tick(&f,&p,&h,&status,&ground,&c,44,flags,0,0) && f.phase==HMBTN_FISHING_CATCH);
    p.animation_done=true;CHECK(hmbtn_fishing_tick(&f,&p,&h,&status,&ground,&c,44,flags,0,0));
    CHECK(f.phase==HMBTN_FISHING_RECEIVE && f.caught==161 && !h.inventory->held_item.id && !f.using_action);
    p.animation_done=true;CHECK(hmbtn_fishing_tick(&f,&p,&h,&status,&ground,&c,44,flags,0,0));
    CHECK(f.phase==HMBTN_FISHING_FREE && h.inventory->held_item.id==161 && h.inventory->held_item.count==1);
    /* The unique berry really raises maximum stamina, refills and is consumed. */
    h.inventory->held_item=(HmbtnStoredTool){0};memset(flags,0,sizeof(flags));c.season=3;
    f.phase=HMBTN_FISHING_CATCH;f.random=54;status.fatigue=10;p.animation_done=true;
    CHECK(hmbtn_fishing_tick(&f,&p,&h,&status,&ground,&c,44,flags,0,0) && f.caught==103 && flags[0x53]);
    p.animation_done=true;CHECK(hmbtn_fishing_tick(&f,&p,&h,&status,&ground,&c,44,flags,0,0) && f.phase==HMBTN_FISHING_EAT);
    p.animation_done=true;CHECK(hmbtn_fishing_tick(&f,&p,&h,&status,&ground,&c,44,flags,0,0));
    CHECK(f.berry_eaten && status.max_stamina==110 && status.stamina==110 && status.fatigue==9 && !h.inventory->held_item.id);
    c.hour=12;f.random=54;CHECK(hmbtn_fishing_roll(&f,&c,flags)==0 && flags[0x53]);
    /* A king is released and its print is shown/stored without filling the bag. */
    f.phase=HMBTN_FISHING_CATCH;f.kind=3;f.random=23;p.animation_done=true;
    CHECK(hmbtn_fishing_tick(&f,&p,&h,&status,&ground,&c,66,flags,0,0) && f.caught==239 && flags[0x56]);
    p.animation_done=true;CHECK(hmbtn_fishing_tick(&f,&p,&h,&status,&ground,&c,66,flags,0,0) && f.phase==HMBTN_FISHING_RELEASE);
    p.animation_done=true;CHECK(hmbtn_fishing_tick(&f,&p,&h,&status,&ground,&c,66,flags,0,0));
    for(unsigned i=0;i<200 && f.phase==HMBTN_FISHING_RELEASE_FLIGHT;++i)
        CHECK(hmbtn_fishing_tick(&f,&p,&h,&status,&ground,&c,66,flags,0,0));
    CHECK(f.phase==HMBTN_FISHING_PRINT && f.shown==127 && !h.inventory->held_item.id);
    p.animation_done=true;CHECK(hmbtn_fishing_tick(&f,&p,&h,&status,&ground,&c,66,flags,0,0));
    for(unsigned i=0;i<31;++i) CHECK(hmbtn_fishing_tick(&f,&p,&h,&status,&ground,&c,66,flags,0,0));
    CHECK(f.phase==HMBTN_FISHING_PRINT_STORE);p.animation_done=true;
    CHECK(hmbtn_fishing_tick(&f,&p,&h,&status,&ground,&c,66,flags,0,0) && f.phase==HMBTN_FISHING_FREE);
    CHECK(flags[0x56] && (f.catch_flags&0x10) && !h.inventory->items[0].id);
    HmbtnFarmCarry carry={0};HmbtnEconomy economy={0};carry.area=56;
    carry.thrown[0]=(HmbtnThrownItem){{163,0,1},{0,1,0},0,0,0,56};
    hmbtn_farm_carry_flight(&carry,NULL,&ground,&economy);
    CHECK(carry.small_fish_placed && carry.thrown[0].phase==1);
    hmbtn_farm_carry_flight(&carry,NULL,&ground,&economy);CHECK(!carry.small_fish_placed);
    for(unsigned i=0;i<4;++i) hmbtn_farm_carry_flight(&carry,NULL,&ground,&economy);
    CHECK(!carry.thrown[0].item.id);
    /* Dry ground, pond and invalid water all wind the cast up without a catch. */
    const unsigned invalid[]={0,4,0x40};
    for(unsigned j=0;j<3;++j) {
        plane(b,invalid[j]);CHECK(hmbtn_model_open(&ground,b,sizeof(b),error,sizeof(error)));
        f.phase=HMBTN_FISHING_FREE;p=(HmbtnIntroActor){0};
        CHECK(hmbtn_fishing_tick(&f,&p,&h,&status,&ground,&c,44,flags,HMBTN_PAD_SQUARE,HMBTN_PAD_SQUARE));
        for(unsigned i=0;i<200 && f.phase==HMBTN_FISHING_CAST;++i) {
            p.animation_done=true;CHECK(hmbtn_fishing_tick(&f,&p,&h,&status,&ground,&c,44,flags,0,HMBTN_PAD_SQUARE));
        }
        CHECK(f.phase==HMBTN_FISHING_REEL && !f.water);
    }
    puts("Original fishing waits, catch bands, prerequisites, unique catches, fallback, cast/cancel, berry and king print lifecycle passed.");return 0;
}

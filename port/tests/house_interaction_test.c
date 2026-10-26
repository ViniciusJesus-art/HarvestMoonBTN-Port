#include "house_interaction.h"
#include "psx_data.h"
#include "house_sleep.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static void ready(HmbtnHouseInteraction *h) { h->message.phase=HMBTN_MESSAGE_WAIT_END; }
int main(void)
{
    HmbtnHouseInteraction h; HmbtnGameState h_state;hmbtn_game_state_init(&h_state);hmbtn_house_interaction_init(&h,&h_state);
    CHECK(h_state.clock.year==0 && h_state.clock.day==1 && h_state.clock.weekday==1 && h_state.clock.hour==6);
    unsigned message,group;
    h_state.weather.tomorrow=0; /* Sunny forecast vector; new games now roll a real forecast. */
    hmbtn_house_tv_program(&h,0,&message,&group); CHECK(message==0x26c && group==4);
    hmbtn_house_tv_program(&h,1,&message,&group); CHECK(message==0x278 && group==0);
    hmbtn_house_tv_program(&h,2,&message,&group); CHECK(message==0x263 && group==10);
    hmbtn_house_tv_program(&h,3,&message,&group); CHECK(message==0x2f0 && group==1);
    h_state.weather.tomorrow=1; hmbtn_house_tv_program(&h,0,&message,&group); CHECK(message==0x26d && group==5);
    h_state.weather.today=3; hmbtn_house_tv_program(&h,0,&message,&group); CHECK(message==0x26a && group==17);
    h_state.weather.today=0; h_state.clock.hour=5; hmbtn_house_tv_program(&h,3,&message,&group); CHECK(message==0x26b && group==16);
    CHECK(hmbtn_house_calendar_message(0,7)==0x3cb && hmbtn_house_calendar_message(0,1)==0x3dc);
    CHECK(hmbtn_house_calendar_message(3,29)==0x3db);
    hmbtn_game_state_init(&h_state);hmbtn_house_interaction_init(&h,&h_state);
    uint8_t section[16+50*44]={0}; uint8_t *z=section+16;
    hmbtn_write_le16(z,0x4000); hmbtn_write_le16(z+2,3); hmbtn_write_le16(z+8,2);
    hmbtn_write_le16(z+14,(uint16_t)-200); hmbtn_write_le16(z+18,1000);
    hmbtn_write_le16(z+20,200); hmbtn_write_le16(z+24,1600);
    HmbtnIntroActor p={0}; p.position[2]=992; p.facing=1;
    CHECK(!hmbtn_house_interact(&h,section,sizeof(section),&p));
    p.facing=2; CHECK(hmbtn_house_interact(&h,section,sizeof(section),&p));
    CHECK(h.mode==HMBTN_HOUSE_TV_SELECT && h.request==0x3a1 && h.message_bank==3);
    ready(&h); hmbtn_house_interaction_tick(&h,HMBTN_PAD_LEFT,0);
    CHECK(h.mode==HMBTN_HOUSE_TV && h.request==0x263 && h.tv_group==10 && h.tv_dirty);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_CIRCLE,0);
    CHECK(h.closing && h.tv_group==18);
    for(unsigned i=0;i<12;++i) hmbtn_house_interaction_tick(&h,0,0);
    CHECK(h.mode==HMBTN_HOUSE_FREE);
    hmbtn_write_le16(z+2,8); CHECK(hmbtn_house_interact(&h,section,sizeof(section),&p));
    ready(&h); hmbtn_house_interaction_tick(&h,HMBTN_PAD_LEFT,0); CHECK(h.calendar_day==0);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_LEFT,0); CHECK(h.calendar_day==29);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_RIGHT,0); CHECK(h.calendar_day==0);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_CROSS,0); CHECK(h.detail && h.request==0x3ca);
    h.message.phase=HMBTN_MESSAGE_CLOSED; hmbtn_house_interaction_tick(&h,0,0);
    CHECK(!h.detail && h.request==0x3b4);
    hmbtn_game_state_init(&h_state);hmbtn_house_interaction_init(&h,&h_state); hmbtn_write_le16(z+2,22);
    CHECK(hmbtn_house_interact(&h,section,sizeof(section),&p));
    CHECK(h.mode==HMBTN_HOUSE_TOOLBOX_OPEN && h.pan==0);
    for(unsigned i=0;i<20;++i) hmbtn_house_interaction_tick(&h,0,0);
    CHECK(h.pan==640 && h.mode==HMBTN_HOUSE_TOOLBOX_OPEN);
    hmbtn_house_interaction_tick(&h,0,0); CHECK(h.request==0x378);
    ready(&h); hmbtn_house_interaction_tick(&h,0,0); CHECK(h.request==0x3ad);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_DOWN,0); CHECK(h.cursor==9);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_CROSS,0); CHECK(h.picked==9);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_UP,0); CHECK(h.cursor==1);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_LEFT,0); CHECK(h.cursor==0);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_CROSS,0);
    CHECK(h.inventory->equipped.id==1 && h.inventory->toolbox[0].id==0 && h.picked==255);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_R1,0); CHECK(h.page==1);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_CROSS,0);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_DOWN,0);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_CROSS,0);
    CHECK(h.inventory->toolbox[8].id==1 && !h.inventory->equipped.id);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_SELECT,0);
    CHECK(h.inventory->toolbox[0].id==11 && h.inventory->toolbox[4].id==1 && h.inventory->toolbox[8].id==0);
    unsigned count=0; for(unsigned i=0;i<64;++i) count+=h.inventory->toolbox[i].id!=0;
    CHECK(count==5);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_CROSS,0); CHECK(h.picked!=255);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_CIRCLE,0); CHECK(h.picked==255 && !h.closing);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_START,0);
    for(unsigned i=0;i<21;++i) hmbtn_house_interaction_tick(&h,0,0);
    CHECK(h.mode==HMBTN_HOUSE_FREE && h.pan==0 && h.inventory->toolbox[4].id==1);
    hmbtn_write_le16(z+2,1); CHECK(hmbtn_house_interact(&h,section,sizeof(section),&p));
    CHECK(h.mode==HMBTN_HOUSE_OPTIONS && h.options.stereo && h.options.clock);
    ready(&h); hmbtn_house_interaction_tick(&h,HMBTN_PAD_CROSS,0);
    CHECK(h.options.page==1 && h.request==0x3b2);
    ready(&h); hmbtn_house_interaction_tick(&h,HMBTN_PAD_RIGHT,0);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_UP,0);
    CHECK(h.options.manual_column==1 && h.options.manual_row==5);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_CROSS,0);
    CHECK(h.detail && h.request==0x371);
    h.message.phase=HMBTN_MESSAGE_CLOSED; hmbtn_house_interaction_tick(&h,0,0);
    CHECK(!h.detail && h.request==0x3b2);
    ready(&h); hmbtn_house_interaction_tick(&h,HMBTN_PAD_CIRCLE,0);
    ready(&h); hmbtn_house_interaction_tick(&h,HMBTN_PAD_DOWN,0);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_CROSS,0);
    CHECK(h.options.page==2 && h.options.row==0 && h.options.column==0);
    ready(&h); hmbtn_house_interaction_tick(&h,HMBTN_PAD_RIGHT,0);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_CROSS,0);
    CHECK(h.options.layout==1);
    CHECK(hmbtn_house_world_buttons(&h.options,HMBTN_PAD_UP|HMBTN_PAD_CIRCLE)==(HMBTN_PAD_LEFT|HMBTN_PAD_CIRCLE));
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_RIGHT,0);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_CROSS,0); CHECK(h.options.layout==2);
    CHECK(hmbtn_house_world_buttons(&h.options,HMBTN_PAD_UP)==HMBTN_PAD_UP);
    for(unsigned i=0;i<17;++i) hmbtn_house_options_idle(&h.options,false,1);
    CHECK(hmbtn_house_world_buttons(&h.options,HMBTN_PAD_LEFT)==HMBTN_PAD_DOWN);
    hmbtn_house_options_idle(&h.options,true,2); CHECK(h.options.rotated && h.options.idle_frames==0);
    for(unsigned i=0;i<17;++i) hmbtn_house_options_idle(&h.options,false,2);
    CHECK(!h.options.rotated);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_DOWN,0); CHECK(h.options.column==1);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_CROSS,0); CHECK(!h.options.vibration);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_LEFT,0);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_CROSS,0); CHECK(h.options.vibration && h.options.rumble);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_DOWN,0);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_RIGHT,0);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_CROSS,0); CHECK(!h.options.stereo);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_DOWN,0);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_CROSS,0); CHECK(!h.options.clock);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_TRIANGLE,0); CHECK(h.detail && h.request==0x381);
    ready(&h); hmbtn_house_interaction_tick(&h,HMBTN_PAD_START,0);
    CHECK(h.closing && !h.detail); /* Start also closes from a ready description. */
    for(unsigned i=0;i<12;++i) hmbtn_house_interaction_tick(&h,0,0);
    CHECK(h.mode==HMBTN_HOUSE_FREE && !h.options.stereo && !h.options.clock);
    hmbtn_write_le16(z+2,2); CHECK(hmbtn_house_interact(&h,section,sizeof(section),&p));
    CHECK(h.mode==HMBTN_HOUSE_BED && h.request==0x1b7);
    ready(&h); hmbtn_house_interaction_tick(&h,HMBTN_PAD_UP,0); CHECK(h.bed_choice==3);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_CROSS,0); CHECK(h.boundary==3 && h.mode==HMBTN_HOUSE_BED);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_UP,0);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_CROSS,0); CHECK(h.boundary==2 && h.mode==HMBTN_HOUSE_BED);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_UP,0);
    hmbtn_house_interaction_tick(&h,HMBTN_PAD_CROSS,0);
    CHECK(h.mode==HMBTN_HOUSE_SLEEP && h.save_requested && h.message.phase==HMBTN_MESSAGE_CLOSED);
    HmbtnHouseSleep sleep;
    p.position[0]=-1000; p.position[1]=0; p.position[2]=-500;
    hmbtn_house_sleep_start(&sleep,&p);
    CHECK(p.facing==0 && p.sprite.animation==0 && p.pose);
    for(unsigned i=0;i<9;++i) hmbtn_house_sleep_tick(&sleep,&p);
    CHECK(sleep.link_step==8 && !sleep.second_bed);
    p.animation_done=true; hmbtn_house_sleep_tick(&sleep,&p);
    CHECK(sleep.phase==1 && sleep.second_bed && p.sprite.animation==1);
    p.animation_done=true;
    for(unsigned i=0;i<18;++i) hmbtn_house_sleep_tick(&sleep,&p);
    CHECK(sleep.phase==2 && sleep.sleeping && p.position[0]==-1350 && p.position[1]==-40 && p.position[2]==-960);
    CHECK(sleep.camera[0]==-1350 && sleep.camera[1]==0 && sleep.camera[2]==-960);
    for(unsigned i=0;i<16;++i) hmbtn_house_sleep_tick(&sleep,&p);
    CHECK(sleep.phase==3 && sleep.link_step==12 && p.sprite.animation==3);
    p.animation_done=true; hmbtn_house_sleep_tick(&sleep,&p);
    for(unsigned i=0;i<30;++i) hmbtn_house_sleep_tick(&sleep,&p);
    CHECK(sleep.boundary && sleep.fade==30 && h_state.clock.day==1);
    hmbtn_game_state_init(&h_state);hmbtn_house_interaction_init(&h,&h_state); hmbtn_write_le16(z,0);
    CHECK(!hmbtn_house_interact(&h,section,sizeof(section),&p));
    CHECK(!hmbtn_house_interact(&h,section,10,&p));
    /* The book and farm signs use bank 2, including FACING_ANY signs. */
    hmbtn_write_le16(z,0x4000); hmbtn_write_le16(z+2,16);
    hmbtn_write_le16(z+4,0x19e); hmbtn_write_le16(z+8,5);
    p.position[0]=0; p.position[1]=0; p.position[2]=1224; p.facing=0;
    CHECK(!hmbtn_house_interact(&h,section,sizeof(section),&p)); /* Strict front edge. */
    p.position[2]=1300;
    CHECK(hmbtn_house_interact(&h,section,sizeof(section),&p));
    CHECK(h.mode==HMBTN_HOUSE_BOOK && h.message_bank==2 && h.request==0x19e);
    ready(&h); hmbtn_house_interaction_tick(&h,HMBTN_PAD_CROSS,0);
    CHECK(h.message.phase==HMBTN_MESSAGE_CLOSING);
    for(unsigned i=0;i<12;++i) {
        hmbtn_house_interaction_tick(&h,0,0);
        CHECK(h.message.phase!=HMBTN_MESSAGE_OPENING);
        if(i<11) CHECK(h.message.box_frame==i+1);
    }
    CHECK(h.message.phase==HMBTN_MESSAGE_CLOSED && h.mode==HMBTN_HOUSE_FREE);
    for(unsigned i=0;i<15;++i) {
        hmbtn_house_interaction_tick(&h,0,0);
        int x,y,w,height; CHECK(!hmbtn_message_box(&h.message,&x,&y,&w,&height));
    }
    hmbtn_game_state_init(&h_state);hmbtn_house_interaction_init(&h,&h_state); hmbtn_write_le16(z+2,3);
    CHECK(!hmbtn_house_interact(&h,section,sizeof(section),&p)); /* TV has no wildcard. */
    uint8_t record[8192]={0}; record[0]=0xa5;
    uint16_t dog[HMBTN_NAME_CHARS+1]={1,14,1,0xffff};
    memcpy(h_state.registration.dog_name,dog,sizeof(dog));
    memcpy(h_state.registration.farm_name,dog,sizeof(dog));
    const uint16_t source[]={0x400c,0xffd,0x4012,0xffd,0x4013,0xffd,0x4014,0xffd,0x4015,0xffff};
    const uint16_t expected[]={1,14,1,0xffd,0x40,0xffd,0x44,0x42,0xffd,
        0x49,0x49,0x49,0x49,0xffd,0x41,0x40,0x40,0xffff};
    h.inventory->supplies[1]=42; h.inventory->supplies[2]=9999; h.inventory->supplies[3]=100;
    for(unsigned i=0;i<sizeof(source)/sizeof(*source);++i) hmbtn_write_le16(record+0x1800+i*2,source[i]);
    CHECK(hmbtn_house_expand_reading(&h,record,sizeof(record)));
    CHECK(record[0]==0xa5); /* Preserve the portrait/image section. */
    for(unsigned i=0;i<sizeof(expected)/sizeof(*expected);++i)
        CHECK(hmbtn_read_le16(record+0x1800+i*2)==expected[i]);
    HmbtnMessage expanded; char error[256]={0};
    CHECK(hmbtn_message_open(&expanded,record,sizeof(record),error,sizeof(error)));
    CHECK(!hmbtn_house_expand_reading(&h,record,8191));
    for(unsigned i=0;i<HMBTN_MESSAGE_WORDS;++i) hmbtn_write_le16(record+0x1800+i*2,0x400c);
    CHECK(!hmbtn_house_expand_reading(&h,record,sizeof(record))); /* No terminator/capacity. */
    hmbtn_write_le16(record+0x1800,0x4012); hmbtn_write_le16(record+0x1802,0xffff);
    h.inventory->supplies[0]=10000;
    CHECK(!hmbtn_house_expand_reading(&h,record,sizeof(record)));
    hmbtn_write_le16(record+0x1800,0x4010);hmbtn_write_le16(record+0x1802,0xffff);
    h.cursor=0;h.inventory->equipped=(HmbtnStoredTool){21,0,100};
    CHECK(hmbtn_house_expand_reading(&h,record,sizeof(record)));
    CHECK(hmbtn_read_le16(record+0x1800)==0x41 && hmbtn_read_le16(record+0x1802)==0x40
        && hmbtn_read_le16(record+0x1804)==0x40 && hmbtn_read_le16(record+0x1806)==0xffff);
    uint16_t farm[HMBTN_NAME_CHARS+1]={2,3,4,5,6,7,8,9,0xffff};
    memcpy(h_state.registration.farm_name,farm,sizeof(farm));
    hmbtn_write_le16(record+0x1800,0x400d);hmbtn_write_le16(record+0x1802,0xffff);
    CHECK(hmbtn_house_expand_reading(&h,record,sizeof(record)));
    for(unsigned i=0;i<8;++i) CHECK(hmbtn_read_le16(record+0x1800+i*2)==farm[i]);
    CHECK(hmbtn_read_le16(record+0x1810)==0xffff);
    puts("House objects, options, controller layouts, bed/diary boundaries and original sleep sequence passed.");
    return 0;
}

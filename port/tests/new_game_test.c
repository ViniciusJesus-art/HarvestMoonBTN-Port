#include "new_game.h"
#include "psx_data.h"
#include <stdio.h>
#include <string.h>
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"New game line %d: %s\n",__LINE__,#c);return 1; } } while(0)
static void tick(HmbtnNewGame *g,uint32_t pressed) { hmbtn_new_game_tick(g,pressed,pressed); }
static void many(HmbtnNewGame *g,unsigned n) { while(n--) tick(g,0); }
static void record(uint8_t *data,const uint16_t *text,size_t count)
{
    memset(data,0,8192);
    for(size_t i=0;i<count;++i) hmbtn_write_le16(data+0x1800+i*2,text[i]);
}
int main(void)
{
    uint8_t data[8192]; char error[256]={0};
    HmbtnNewGame g;hmbtn_new_game_init(&g);
    CHECK(g.step==11 && g.name[0]==0xffff);
    tick(&g,0);CHECK(g.step==12 && g.request==0x3a2);
    const uint16_t pages[]={0xf87,0xff7,0xff4,0,0xffc,0xffb,0xfffe,1,0xffff};
    record(data,pages,sizeof(pages)/2);
    hmbtn_write_le32(data,104);
    CHECK(hmbtn_message_open(&g.message,data,sizeof(data),error,sizeof(error)));
    int x,y,w,h;
    CHECK(!hmbtn_message_box(&g.message,&x,&y,&w,&h));
    tick(&g,0);CHECK(hmbtn_message_box(&g.message,&x,&y,&w,&h) && w==44 && h==4);
    many(&g,40);CHECK(g.message.phase==HMBTN_MESSAGE_WAIT_PAGE && g.message.face);
    tick(&g,HMBTN_PAD_START);CHECK(g.message.phase==HMBTN_MESSAGE_WAIT_PAGE);
    tick(&g,HMBTN_PAD_CROSS);CHECK(g.message.page==7);
    many(&g,8);CHECK(g.message.phase==HMBTN_MESSAGE_WAIT_END);
    tick(&g,HMBTN_PAD_CIRCLE);CHECK(g.message.phase==HMBTN_MESSAGE_CLOSING);
    many(&g,12);CHECK(g.step==13);
    tick(&g,0);CHECK(g.step==14 && g.request==0x397);
    const uint16_t simple[]={0,0xffff};record(data,simple,2);
    CHECK(hmbtn_message_open(&g.message,data,sizeof(data),error,sizeof(error)));
    many(&g,30);tick(&g,HMBTN_PAD_CROSS);many(&g,13);
    CHECK(g.step==15 && g.request==0x3a8);
    CHECK(hmbtn_message_open(&g.message,data,sizeof(data),error,sizeof(error)));
    many(&g,30);
    tick(&g,HMBTN_PAD_CIRCLE);CHECK(g.length==0 && g.sound==2 && g.step==15);
    tick(&g,HMBTN_PAD_START);CHECK(!g.confirming && g.sound==2);
    g.cell=27;tick(&g,HMBTN_PAD_CROSS);CHECK(g.name[0]==0xffd && g.length==1 && !g.dirty);
    tick(&g,HMBTN_PAD_START);CHECK(!g.confirming && g.sound==2);
    tick(&g,HMBTN_PAD_CIRCLE);CHECK(g.length==0);
    g.cell=0;tick(&g,HMBTN_PAD_CROSS);CHECK(g.name[0]==0 && g.name[1]==0xffff && g.dirty);
    tick(&g,HMBTN_PAD_RIGHT);CHECK(g.cell==1);
    for(unsigned i=0;i<27;++i) hmbtn_new_game_tick(&g,0,HMBTN_PAD_RIGHT);
    CHECK(g.cell==1);hmbtn_new_game_tick(&g,0,HMBTN_PAD_RIGHT);CHECK(g.cell==2);
    g.cell=70;tick(&g,HMBTN_PAD_RIGHT);CHECK(g.cell==75);tick(&g,HMBTN_PAD_RIGHT);CHECK(g.cell==83);
    tick(&g,HMBTN_PAD_DOWN);CHECK(g.cell==13);
    tick(&g,HMBTN_PAD_CROSS);CHECK(g.grid==1);
    g.cell=9;tick(&g,HMBTN_PAD_RIGHT);CHECK(g.cell==13);
    tick(&g,HMBTN_PAD_L1|HMBTN_PAD_L2);CHECK(g.grid==0);
    g.cell=1;
    for(unsigned i=0;i<12;++i) tick(&g,HMBTN_PAD_CROSS);
    CHECK(g.length==8 && g.name[8]==0xffff);
    tick(&g,HMBTN_PAD_START);CHECK(g.confirming && g.queued==0x3a6);
    many(&g,12);CHECK(g.request==0x3a6 && g.queued==0);
    CHECK(hmbtn_message_open(&g.message,data,sizeof(data),error,sizeof(error)));
    many(&g,30);tick(&g,HMBTN_PAD_DOWN);CHECK(g.choice==1);
    tick(&g,HMBTN_PAD_CROSS);CHECK(!g.confirming && g.queued==0x3a8 && g.step==15);
    many(&g,12);CHECK(g.request==0x3a8);
    CHECK(hmbtn_message_open(&g.message,data,sizeof(data),error,sizeof(error)));
    many(&g,30);tick(&g,HMBTN_PAD_START);many(&g,12);
    CHECK(hmbtn_message_open(&g.message,data,sizeof(data),error,sizeof(error)));
    many(&g,30);tick(&g,HMBTN_PAD_CROSS);CHECK(g.step==16 && g.player_name[0]==0 && g.length==8);
    /* Birthday prompt -> fixed picker help, with original wrap and back paths. */
    many(&g,12);CHECK(g.step==17 && g.request==0x3a5);
    CHECK(hmbtn_message_open(&g.message,data,sizeof(data),error,sizeof(error)));
    many(&g,30);tick(&g,HMBTN_PAD_CROSS);many(&g,12);
    CHECK(g.step==18 && g.request==0x3a9 && g.birthday_phase==1);
    /* FFC/FFB/newline separates pages; four actual lines on page 2 remain valid. */
    const uint16_t paged[]={0,0xffc,0xffb,0xfffe,1,0xfffe,2,0xfffe,3,0xfffe,4,0xffff};
    record(data,paged,sizeof(paged)/2);
    HmbtnMessage paging;
    CHECK(hmbtn_message_open(&paging,data,sizeof(data),error,sizeof(error)));
    for(unsigned i=0;i<24;++i) hmbtn_message_tick(&paging,0,HMBTN_PAD_CROSS);
    CHECK(paging.phase==HMBTN_MESSAGE_WAIT_PAGE);
    hmbtn_message_tick(&paging,HMBTN_PAD_CROSS,HMBTN_PAD_CROSS);
    CHECK(paging.page==4 && paging.words[paging.page]==1);
    const uint16_t help[]={0xff3,0,1,2,0xfffe,3,4,5,0xffff};record(data,help,sizeof(help)/2);
    CHECK(hmbtn_message_open(&g.message,data,sizeof(data),error,sizeof(error)));
    many(&g,17);CHECK(g.message.phase==HMBTN_MESSAGE_OPENING && g.message.cursor==0);
    tick(&g,0);CHECK(g.message.phase==HMBTN_MESSAGE_WAIT_END && g.message.cursor==g.message.count && g.sound==-1);
    tick(&g,HMBTN_PAD_LEFT);CHECK(g.birthday_value==3);
    tick(&g,HMBTN_PAD_RIGHT);CHECK(g.birthday_value==0);
    tick(&g,HMBTN_PAD_RIGHT);tick(&g,HMBTN_PAD_CROSS);
    CHECK(g.birthday_phase==2 && g.season==1 && g.birthday_value==0);
    tick(&g,HMBTN_PAD_UP);CHECK(g.birthday_value==28);
    tick(&g,HMBTN_PAD_DOWN);CHECK(g.birthday_value==0);
    g.birthday_value=0;tick(&g,HMBTN_PAD_LEFT);CHECK(g.birthday_value==29);
    tick(&g,HMBTN_PAD_RIGHT);CHECK(g.birthday_value==0);
    for(unsigned day=0;day<30;++day) {
        const uint32_t arrows[]={HMBTN_PAD_UP,HMBTN_PAD_DOWN,HMBTN_PAD_LEFT,HMBTN_PAD_RIGHT};
        for(unsigned i=0;i<4;++i) {g.birthday_value=day;tick(&g,arrows[i]);CHECK(g.birthday_value<30);}
    }
    g.birthday_value=29;tick(&g,HMBTN_PAD_CIRCLE);CHECK(g.birthday_phase==1 && g.birthday_value==1);
    tick(&g,HMBTN_PAD_CROSS);g.birthday_value=29;tick(&g,HMBTN_PAD_CROSS);
    CHECK(g.day==29 && g.birthday_phase==3 && g.queued==0x3a7);
    many(&g,12);CHECK(g.request==0x3a7);
    CHECK(hmbtn_message_open(&g.message,data,sizeof(data),error,sizeof(error)));
    many(&g,18);tick(&g,HMBTN_PAD_DOWN);tick(&g,HMBTN_PAD_CROSS);
    CHECK(g.birthday_phase==2 && g.birthday_value==29 && g.queued==0x3a9);
    many(&g,12);CHECK(hmbtn_message_open(&g.message,data,sizeof(data),error,sizeof(error)));
    many(&g,18);tick(&g,HMBTN_PAD_CROSS);many(&g,12);
    CHECK(hmbtn_message_open(&g.message,data,sizeof(data),error,sizeof(error)));
    many(&g,18);tick(&g,HMBTN_PAD_CROSS);CHECK(g.step==19);
    many(&g,12);CHECK(g.step==20 && g.request==0x3a4);
    CHECK(hmbtn_message_open(&g.message,data,sizeof(data),error,sizeof(error)));
    many(&g,18);tick(&g,HMBTN_PAD_CROSS);many(&g,12);CHECK(g.step==21 && g.request==0x3a8);
    CHECK(hmbtn_message_open(&g.message,data,sizeof(data),error,sizeof(error)));
    many(&g,18);tick(&g,HMBTN_PAD_CROSS);tick(&g,HMBTN_PAD_START);many(&g,12);
    CHECK(hmbtn_message_open(&g.message,data,sizeof(data),error,sizeof(error)));
    many(&g,18);tick(&g,HMBTN_PAD_CROSS);CHECK(g.step==22 && g.farm_name[0]==0);
    many(&g,12);CHECK(g.request==0x3a3 && g.step==23);
    CHECK(hmbtn_message_open(&g.message,data,sizeof(data),error,sizeof(error)));
    many(&g,18);tick(&g,HMBTN_PAD_CROSS);many(&g,12);CHECK(g.step==24);
    CHECK(hmbtn_message_open(&g.message,data,sizeof(data),error,sizeof(error)));
    many(&g,18);tick(&g,HMBTN_PAD_CIRCLE);CHECK(g.step==19); /* Dog back -> farm question. */
    many(&g,12);CHECK(g.step==20);
    /* Resume dog editor to verify its stored name, summary choice and exit fade. */
    g.step=24;g.message.phase=HMBTN_MESSAGE_WAIT_END;
    tick(&g,HMBTN_PAD_CROSS);tick(&g,HMBTN_PAD_START);many(&g,12);
    CHECK(hmbtn_message_open(&g.message,data,sizeof(data),error,sizeof(error)));
    many(&g,18);tick(&g,HMBTN_PAD_CROSS);many(&g,12);
    CHECK(g.step==26 && g.request==0x3aa && g.dog_name[0]==0);
    const uint16_t choices[]={0xffa,0xfffe,0,0xfffe,1,0xffff};record(data,choices,sizeof(choices)/2);
    CHECK(hmbtn_message_open(&g.message,data,sizeof(data),error,sizeof(error)));
    many(&g,18);CHECK(g.message.choices==2 && g.message.page==2 && g.message.phase==HMBTN_MESSAGE_WAIT_END);
    tick(&g,HMBTN_PAD_DOWN);tick(&g,HMBTN_PAD_CROSS);CHECK(g.step==13 && !g.finished);
    many(&g,12);CHECK(g.step==14 && g.request==0x397);
    g.step=26;g.choice=0;g.message.phase=HMBTN_MESSAGE_WAIT_END;
    tick(&g,HMBTN_PAD_CROSS);CHECK(g.step==27 && g.fade==0);
    many(&g,10);CHECK(g.step==28 && g.fade==10 && !g.finished);
    tick(&g,0);CHECK(g.finished && g.step==100 && g.season==1 && g.day==29);
    CHECK(!hmbtn_message_open(&g.message,data,64,error,sizeof(error)));
    const uint16_t event_text[]={0xff5,0xe2d,0x20,0xffff};record(data,event_text,4);
    CHECK(hmbtn_message_open(&g.message,data,sizeof(data),error,sizeof(error)));
    for (unsigned i=0;i<18;++i) hmbtn_message_tick(&g.message,0,0);
    CHECK(g.message.phase==HMBTN_MESSAGE_PRINTING);
    for (unsigned i=0;i<4;++i) hmbtn_message_tick(&g.message,0,0);
    CHECK(g.message.cursor==2 && g.message.wait_frames==1);
    hmbtn_message_tick(&g.message,0,0);
    CHECK(g.message.cursor==2 && g.message.wait_frames==0);
    const uint16_t invalid[]={0x400a,0xffff};record(data,invalid,2);
    CHECK(!hmbtn_message_open(&g.message,data,sizeof(data),error,sizeof(error)));
    memset(data+0x1800,0,0x800);
    CHECK(!hmbtn_message_open(&g.message,data,sizeof(data),error,sizeof(error)));
    return 0;
}

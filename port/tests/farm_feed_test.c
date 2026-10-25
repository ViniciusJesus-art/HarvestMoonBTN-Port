#include "farm_feed.h"
#include "psx_data.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Feed line %d: %s\n",__LINE__,#x);return 1; } } while(0)
static void zone(uint8_t *s,unsigned kind,unsigned facing,unsigned bin)
{
    memset(s,0,0x2000);uint8_t *p=s+16;
    hmbtn_write_le16(p,0x4000);hmbtn_write_le16(p+2,(uint16_t)kind);
    hmbtn_write_le16(p+4,(uint16_t)bin);hmbtn_write_le16(p+8,(uint16_t)facing);
    hmbtn_write_le16(p+14,(uint16_t)-300);hmbtn_write_le16(p+18,(uint16_t)-300);
    hmbtn_write_le16(p+20,300);hmbtn_write_le16(p+24,300);
}
static void finish(HmbtnFarmFeed *f,HmbtnHouseInteraction *h,HmbtnIntroActor *p,uint8_t *s)
{
    p->animation_done=true;hmbtn_farm_feed_tick(f,h,p,s,0x2000,0);
}
int main(void)
{
    HmbtnFarmFeed f={0};HmbtnHouseInteraction h;HmbtnIntroActor p={0};uint8_t s[0x2000];
    HmbtnGameState h_state;hmbtn_game_state_init(&h_state);hmbtn_house_interaction_init(&h,&h_state);zone(s,9,999,0);
    CHECK(hmbtn_farm_feed_tick(&f,&h,&p,s,sizeof(s),HMBTN_PAD_CROSS));
    CHECK(f.request==0x19C && !h.inventory->held_item.id && !f.phase);
    h.inventory->supplies[0]=2;
    CHECK(hmbtn_farm_feed_tick(&f,&h,&p,s,sizeof(s),HMBTN_PAD_CROSS));
    CHECK(f.phase==1 && h.inventory->held_item.id==0x68 && h.inventory->supplies[0]==1);
    CHECK(p.sprite.animation==11 && f.sound==7);
    CHECK(hmbtn_farm_feed_tick(&f,&h,&p,s,sizeof(s),HMBTN_PAD_CROSS) && h.inventory->supplies[0]==1);
    finish(&f,&h,&p,s);CHECK(!f.phase && h.inventory->held_item.id==0x68);
    zone(s,41,6,20);p.facing=2;
    CHECK(hmbtn_farm_feed_tick(&f,&h,&p,s,sizeof(s),HMBTN_PAD_CROSS) && !f.phase && !f.barn[20]);
    p.facing=3;
    CHECK(hmbtn_farm_feed_tick(&f,&h,&p,s,sizeof(s),HMBTN_PAD_CROSS) && f.barn[20]);
    CHECK(f.phase==2 && h.inventory->held_item.id==0x68 && p.sprite.animation==26);
    finish(&f,&h,&p,s);CHECK(!h.inventory->held_item.id && f.barn[20]);
    zone(s,26,1,0);p.facing=1;
    CHECK(hmbtn_farm_feed_tick(&f,&h,&p,s,sizeof(s),HMBTN_PAD_CROSS) && f.request==0x19F);
    h.inventory->supplies[2]=999;
    CHECK(hmbtn_farm_feed_tick(&f,&h,&p,s,sizeof(s),HMBTN_PAD_CROSS));
    CHECK(h.inventory->held_item.id==0x69 && h.inventory->supplies[2]==998);finish(&f,&h,&p,s);
    CHECK(hmbtn_farm_feed_tick(&f,&h,&p,s,sizeof(s),HMBTN_PAD_CROSS));
    CHECK(h.inventory->supplies[2]==999 && h.inventory->held_item.id==0x69);finish(&f,&h,&p,s);CHECK(!h.inventory->held_item.id);
    zone(s,40,2,4);p.facing=2;
    CHECK(hmbtn_farm_feed_tick(&f,&h,&p,s,sizeof(s),HMBTN_PAD_CROSS) && f.request==0x1E5);
    h.inventory->held_item=(HmbtnStoredTool){0x69,0,1};
    CHECK(hmbtn_farm_feed_tick(&f,&h,&p,s,sizeof(s),HMBTN_PAD_CROSS) && f.coop[4]);finish(&f,&h,&p,s);
    h.inventory->held_item=(HmbtnStoredTool){0x69,0,1};
    CHECK(hmbtn_farm_feed_tick(&f,&h,&p,s,sizeof(s),HMBTN_PAD_CROSS) && f.phase==2);
    finish(&f,&h,&p,s);CHECK(!h.inventory->held_item.id && f.coop[4]);
    zone(s,40,2,10);h.inventory->held_item=(HmbtnStoredTool){0x69,0,1};
    CHECK(hmbtn_farm_feed_tick(&f,&h,&p,s,sizeof(s),HMBTN_PAD_CROSS) && !f.phase && h.inventory->held_item.id==0x69);
    hmbtn_farm_feed_new_day(&f);CHECK(!f.barn[20] && !f.coop[4] && h.inventory->supplies[2]==999);
    int32_t pos[3];CHECK(hmbtn_farm_feed_position(true,20,pos) && pos[0]==2900 && pos[2]==-2450);
    CHECK(hmbtn_farm_feed_position(false,4,pos) && pos[0]==700 && pos[2]==-100);
    CHECK(!hmbtn_farm_feed_position(false,10,pos));
    CHECK(!hmbtn_farm_feed_tick(&f,&h,&p,s,16,HMBTN_PAD_CROSS));
    puts("Original feed stock exchange, direction groups, bin bounds, action consumption and daily clearing passed.");return 0;
}

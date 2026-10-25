#include "farm_carry.h"
#include "psx_data.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Shipping line %d: %s\n",__LINE__,#x);return 1; } } while(0)
typedef HmbtnEconomy Accounts;
static void minute_event(void *context,const HmbtnFarmClock *clock,unsigned changes)
{
    Accounts *a=context;hmbtn_farm_shipping_clock(a,clock,changes);
}
static void floor_model(uint8_t data[92],uint16_t flags)
{
    const int16_t vertices[4][3]={{-2000,-120,-2000},{2000,-120,-2000},{-2000,-120,2000},{2000,-120,2000}};
    memset(data,0,92);hmbtn_write_le32(data,0x42);hmbtn_write_le32(data+8,1);
    hmbtn_write_le32(data+12,28);hmbtn_write_le32(data+16,4);
    hmbtn_write_le32(data+28,60);hmbtn_write_le32(data+32,1);
    for(unsigned i=0;i<4;++i) for(unsigned j=0;j<3;++j) hmbtn_write_le16(data+40+i*8+j*2,(uint16_t)vertices[i][j]);
    hmbtn_write_le16(data+72,flags);hmbtn_write_le16(data+74,0x2901);
    for(unsigned i=0;i<4;++i) hmbtn_write_le16(data+80+i*2,(uint16_t)i);
}
int main(void)
{
    Accounts a={0};HmbtnFarmClock c={0,0,1,1,17,57,14};
    a.gold=500;
    CHECK(!hmbtn_farm_shipping_add(&a,0x40)); /* Weed, no price. */
    CHECK(!hmbtn_farm_shipping_add(&a,0x15)); /* Tool. */
    CHECK(!hmbtn_farm_shipping_add(&a,380));
    CHECK(hmbtn_farm_shipping_add(&a,0x41)); /* Turnip 60G. */
    CHECK(a.shipping.pending_value==60 && a.gold==500 && a.shipments[0]==1);
    CHECK(hmbtn_farm_clock_tick(&c,true)==0);
    minute_event(&a,&c,0);CHECK(a.gold==500);
    minute_event(&a,&c,hmbtn_farm_clock_tick(&c,false));
    CHECK(a.gold==560 && a.ledger[0][0]==60 && !a.shipping.pending_value && a.shipping.settled);
    CHECK(hmbtn_farm_shipping_add(&a,0x41));
    minute_event(&a,&c,HMBTN_CLOCK_MINUTE); /* Idempotent cutoff. */
    CHECK(a.gold==560 && a.shipping.pending_value==60);
    hmbtn_farm_clock_advance_notify(&c,12*60+2,minute_event,&a);
    CHECK(c.day==2 && c.hour==6 && !a.shipping.settled && a.shipping.pending_value==60);
    hmbtn_farm_clock_advance_notify(&c,11*60+58,minute_event,&a);
    CHECK(a.gold==620 && a.ledger[0][0]==120 && !a.shipping.pending_value);
    /* Sleeping across collection pays before the next morning and season. */
    c=(HmbtnFarmClock){0,3,29,6,16,0,0};a.shipping.settled=false;
    CHECK(hmbtn_farm_shipping_add(&a,0x41));
    hmbtn_farm_clock_advance_notify(&c,14*60,minute_event,&a);
    CHECK(c.year==1 && c.season==0 && c.day==0 && c.hour==6);
    CHECK(a.gold==680 && a.ledger[1][0]==180 && !a.ledger[0][0]);
    /* All special dates used by clock_date_index defer collection. */
    const unsigned dates[14][2]={{0,0},{0,7},{0,17},{0,21},{1,6},{1,11},{2,8},
        {3,9},{1,0},{1,19},{2,20},{1,23},{2,2},{3,23}};
    for(unsigned i=0;i<14;++i) {
        memset(&a,0,sizeof(a));c=(HmbtnFarmClock){0,dates[i][0],dates[i][1],1,17,57,0};
        CHECK(hmbtn_farm_shipping_add(&a,0x41));
        hmbtn_farm_clock_advance_notify(&c,1,minute_event,&a);
        CHECK(a.shipping.pending_value==60 && a.shipping.settled && !a.gold);
        hmbtn_farm_clock_advance_notify(&c,24*60,minute_event,&a);
        CHECK(a.gold==60 && !a.shipping.pending_value);
    }
    /* Crops, quality groups, fish and herbs use the original 25 bins. */
    memset(&a,0,sizeof(a));
    for(unsigned id=0x41;id<0x50;++id) CHECK(hmbtn_farm_shipping_add(&a,id));
    for(unsigned i=0;i<15;++i) CHECK(a.shipments[i]==1);
    const unsigned goods[]={0x122,0xe8,0x8b,0xe3,0xe7,0xe5,0xe4,0xe6,0xa1,0xa2,0xa3,0x5d,0x5e,0x5f};
    for(unsigned i=0;i<sizeof(goods)/sizeof(goods[0]);++i) CHECK(hmbtn_farm_shipping_add(&a,goods[i]));
    CHECK(a.shipments[15]==2 && a.shipments[16]==2);
    for(unsigned i=17;i<=20;++i) CHECK(a.shipments[i]==1);
    CHECK(a.shipments[21]==3 && a.shipping.fish_shipped==3);
    for(unsigned i=22;i<25;++i) CHECK(a.shipments[i]==1);
    a.shipments[0]=999;CHECK(hmbtn_farm_shipping_add(&a,0x41));
    CHECK(a.shipments[0]==999);
    uint32_t value=a.shipping.pending_value;
    CHECK(hmbtn_farm_shipping_add(&a,0x60)); /* Ore: value, no bin. */
    CHECK(a.shipping.pending_value==value+40);
    a.shipping.honey_bonus=true;value=a.shipping.pending_value;
    CHECK(hmbtn_farm_shipping_add(&a,0x97));CHECK(a.shipping.pending_value==value+60);
    a.gold=9999990;c=(HmbtnFarmClock){0,0,1,1,17,58,0};value=a.shipping.pending_value;
    minute_event(&a,&c,HMBTN_CLOCK_MINUTE);
    CHECK(a.gold==9999999 && a.ledger[0][0]==value && !a.shipping.pending_value);
    /* A real primitive hit (0x80), positive price and completed fall are required. */
    uint8_t bytes[92];char error[256]={0};HmbtnModel model;
    floor_model(bytes,0x80);CHECK(hmbtn_model_open(&model,bytes,sizeof(bytes),error,sizeof(error)));
    memset(&a,0,sizeof(a));HmbtnFarmCarry carry={0};
    carry.thrown[0]=(HmbtnThrownItem){{0x41,0,1},{0,-300,0},0,0,3};
    for(unsigned i=0;i<6;++i) hmbtn_farm_carry_flight(&carry,NULL,&model,&a);
    CHECK(carry.thrown[0].item.id==0x41 && !a.shipping.pending_value);
    hmbtn_farm_carry_flight(&carry,NULL,&model,&a);
    CHECK(!carry.thrown[0].item.id && carry.thrown[0].ground==-120 && a.shipping.pending_value==60);
    CHECK(carry.sound==19 && !carry.discarded && a.shipments[0]==1);
    for(unsigned i=0;i<30;++i) hmbtn_farm_carry_flight(&carry,NULL,&model,&a);
    CHECK(a.shipping.pending_value==60 && carry.sound==-1);
    const unsigned flags[]={0,0x4000,0x80};
    for(unsigned i=0;i<3;++i) {
        floor_model(bytes,(uint16_t)flags[i]);
        carry.thrown[0]=(HmbtnThrownItem){{i==2 ? 0x40 : 0x41,0,1},{0,-300,0},0,0,3};
        for(unsigned j=0;j<30;++j) hmbtn_farm_carry_flight(&carry,NULL,&model,&a);
        CHECK(!carry.thrown[0].item.id && a.shipping.pending_value==60 && carry.discarded==i+1);
    }
    puts("Shipping surfaces, prices/bins, delayed settlement, special dates, sleep, ledger and limits passed.");return 0;
}

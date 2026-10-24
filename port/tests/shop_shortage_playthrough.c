#include "poultry_helpers.h"
int main(int argc,char **argv)
{
    if(argc<2 || argc>3) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};const char *shots=argc==3 ? argv[2] : NULL;
    CHECK(hmbtn_host_open(&host));s=hmbtn_scene_area_create(host.renderer,argv[1],128,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    s->clock=(HmbtnFarmClock){0,0,2,2,12,0,0};sync_clock(s);CHECK(step(s,0,error,sizeof(error)));
    CHECK(s->menu.stats.gold==500 && !hmbtn_poultry_count(&s->poultry));
    CHECK(merchant(s,error,sizeof(error)));CHECK(step(s,HMBTN_PAD_DOWN,error,sizeof(error)));
    CHECK(s->feed_shop.selection==1 && hmbtn_feed_shop_cost(&s->feed_shop)==1500);
    CHECK(capture(s,&host,shots,"chicken-price-500G",error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CROSS,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_RESULT,error,sizeof(error)));
    CHECK(s->feed_shop.request==0x4F && s->menu.stats.gold==500 && !hmbtn_poultry_count(&s->poultry));
    uint8_t record[8192];HmbtnMessage original;
    CHECK(hmbtn_resource_read(argv[1],"A_FILE/04_messege2.bin",0x4F*8192u,record,sizeof(record),error,sizeof(error)));
    CHECK(hmbtn_message_open(&original,record,sizeof(record),error,sizeof(error)));
    CHECK(s->feed_shop.message.count==original.count
        && !memcmp(s->feed_shop.message.words,original.words,original.count*sizeof(*original.words)));
    CHECK(capture(s,&host,shots,"chicken-shortage-500G",error,sizeof(error)));
    CHECK(shop_wait(s,HMBTN_SHOP_SELECT,error,sizeof(error)));
    CHECK(step(s,HMBTN_PAD_CIRCLE,error,sizeof(error)));CHECK(shop_wait(s,HMBTN_SHOP_OFF,error,sizeof(error)));
    CHECK(s->menu.stats.gold==500 && !s->menu.stats.ledger[0][1]);
    puts("Chicken 1500G with wallet 500G: visible price/wallet, unchanged original refusal, no debit/animal and clean return passed.");
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 0;
fail:
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}

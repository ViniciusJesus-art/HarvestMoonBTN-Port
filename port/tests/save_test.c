#include "save_fixture.h"
#include "psx_data.h"
#include <stdio.h>
#include <stdlib.h>
static void crc(uint8_t *data,size_t bytes)
{
    uint32_t v=UINT32_MAX;for(size_t i=24;i<bytes;++i) {
        v^=data[i];for(unsigned k=0;k<8;++k) v=(v>>1)^(0xedb88320u&(0u-(v&1u)));
    }
    hmbtn_write_le32(data+20,~v);
}
static void reject(const uint8_t *data,size_t n)
{ char e[256]={0};HmbtnSession *s=hmbtn_save_decode(data,n,e,sizeof(e));assert(!s && e[0]); }
int main(void)
{
    char error[256]={0};HmbtnSession *s=save_fixture(1);uint8_t *data=NULL;size_t n=0;
    s->field.plant_random=0x98765432;s->game.progress.moondrop_seeds=true;s->construction.mornings_left=3;s->construction.pending=HMBTN_BUILD_COOP;
    s->game.weather=(HmbtnWeather){.today=1,.tomorrow=2,.random=0xabcdef12};
    hmbtn_village_refresh_weather(&s->village);
    unsigned char *before=malloc(sizeof(*s));assert(before);memcpy(before,s,sizeof(*s));
    if(!hmbtn_save_encode(s,&data,&n,error,sizeof(error))) { fprintf(stderr,"%s\n",error);return 1; }
    assert(!memcmp(before,s,sizeof(*s)));free(before);
    assert(!memcmp(data,"HMBTNSV",8) && hmbtn_read_le32(data+8)==HMBTN_SAVE_VERSION && hmbtn_read_le32(data+12)==n-24);
    assert(hmbtn_read_le32(data+24)==0x454d4147);
    HmbtnSession *loaded=hmbtn_save_decode(data,n,error,sizeof(error));assert(loaded);
    assert(loaded!=s && loaded->house.inventory==&loaded->game.inventory && loaded->house.clock==&loaded->game.clock);
    assert(loaded->menu.economy==&loaded->game.economy && loaded->menu.poultry==&loaded->poultry);
    assert(loaded->village.player==&loaded->intro.player && !loaded->intro.ground && !loaded->village_ready && !loaded->active_frame);
    assert(loaded->game.economy.gold==0x123457 && loaded->field.cells[2].watered_days==2);
    assert(loaded->village.flags[205] && loaded->village.people[21].friendship==123);
    assert(loaded->poultry.hens[0].days_sick==3 && loaded->poultry.eggs[0].item==0x122);
    static uint8_t schedule[1];assert(hmbtn_session_bind_schedules(loaded,schedule,1));
    uint8_t *again=NULL;size_t m=0;
    assert(hmbtn_save_encode(loaded,&again,&m,error,sizeof(error)) && n==m && !memcmp(data,again,n));free(again);
    assert(loaded->house.weather==&loaded->game.weather && loaded->village.weather==&loaded->game.weather);
    assert(loaded->game.weather.today==1 && loaded->game.weather.tomorrow==2 && loaded->game.weather.random==0xabcdef12);
    assert(loaded->field.plant_random==s->field.plant_random && loaded->game.progress.moondrop_seeds
        && loaded->house.progress==&loaded->game.progress && loaded->feed_shop.progress==&loaded->game.progress);
    assert(loaded->construction.mornings_left==3 && loaded->feed_shop.construction==&loaded->construction);
    /* Development saves intentionally accept only this version. */
    uint8_t *old=malloc(n);assert(old);
    for(unsigned version=0;version<HMBTN_SAVE_VERSION;++version) {
        memcpy(old,data,n);hmbtn_write_le32(old+8,version);reject(old,n);
    }
    free(old);
    /* Ledger and growth must not be replayed on read or repeated notifications. */
    const unsigned gold=loaded->game.economy.gold;
    hmbtn_session_clock_events(loaded,&loaded->game.clock,HMBTN_CLOCK_DAY);
    assert(loaded->field.cells[2].watered_days==2 && loaded->game.economy.gold==gold);
    loaded->game.clock.hour=17;loaded->game.clock.minute=58;
    hmbtn_session_clock_events(loaded,&loaded->game.clock,HMBTN_CLOCK_MINUTE);
    assert(loaded->game.economy.gold==gold+120);
    hmbtn_session_clock_events(loaded,&loaded->game.clock,HMBTN_CLOCK_MINUTE);assert(loaded->game.economy.gold==gold+120);
    HmbtnSession *two=save_fixture(2);assert(two->game.economy.gold==s->game.economy.gold+1);
    two->game.inventory.toolbox[63].count=5;assert(s->game.inventory.toolbox[63].count==99);
    /* Policy blocks every class of unsafe state without touching the source. */
    s->visit.active=true;assert(!hmbtn_save_available(s,error,sizeof(error)));s->visit.active=false;
    s->talk.active=true;assert(!hmbtn_save_available(s,error,sizeof(error)));s->talk.active=false;
    s->door_tick=1;assert(!hmbtn_save_available(s,error,sizeof(error)));s->door_tick=0;
    s->morning_fade=1;assert(!hmbtn_save_available(s,error,sizeof(error)));s->morning_fade=0;
    s->house.mode=HMBTN_HOUSE_BED;assert(!hmbtn_save_available(s,error,sizeof(error)));s->house.mode=HMBTN_HOUSE_FREE;
    s->farm_action.phase=HMBTN_FARM_PLANT_SEED;assert(!hmbtn_save_available(s,error,sizeof(error)));s->farm_action.phase=HMBTN_FARM_ACTION_FREE;
    s->carry.thrown[0].item.id=0x125;assert(!hmbtn_save_available(s,error,sizeof(error)));s->carry.thrown[0].item.id=0;
    s->poultry.hens[0].held=true;assert(!hmbtn_save_available(s,error,sizeof(error)));s->poultry.hens[0].held=false;
    assert(hmbtn_service_begin(&s->services,HMBTN_SERVICE_RESOURCE,(HmbtnServiceRequest){HMBTN_SERVICE_TEXT_DATA,1,1,0,0}));
    assert(!hmbtn_save_available(s,error,sizeof(error)));hmbtn_services_clear(&s->services);
    assert(hmbtn_save_available(s,error,sizeof(error)));
    /* Structural corruption is tested with a valid checksum as well. */
    for(size_t cut=0;cut<32;++cut) reject(data,cut);
    reject(data,n-1);uint8_t *bad=malloc(n+1);assert(bad);memcpy(bad,data,n);bad[n]=0;reject(bad,n+1);
    bad[0]^=1;reject(bad,n);memcpy(bad,data,n);
    hmbtn_write_le32(bad+8,HMBTN_SAVE_VERSION+1);reject(bad,n);memcpy(bad,data,n);
    bad[n-1]^=1;reject(bad,n);memcpy(bad,data,n);
    hmbtn_write_le32(bad+28,UINT32_MAX);crc(bad,n);reject(bad,n);memcpy(bad,data,n);
    hmbtn_write_le32(bad+24,0xffffffff);crc(bad,n);reject(bad,n);memcpy(bad,data,n);
    size_t second=32+hmbtn_read_le32(data+28);hmbtn_write_le32(bad+second,hmbtn_read_le32(data+24));crc(bad,n);reject(bad,n);memcpy(bad,data,n);
    hmbtn_write_le32(bad+32,99);crc(bad,n);reject(bad,n);memcpy(bad,data,n);
    /* Locate PLAY and make area invalid, then a noncanonical boolean. */
    size_t play=24;
    while(hmbtn_read_le32(data+play)!=0x59414c50) play+=8+hmbtn_read_le32(data+play+4);
    hmbtn_write_le32(bad+play+8,230);crc(bad,n);reject(bad,n);memcpy(bad,data,n);
    hmbtn_write_le32(bad+play+16,2);crc(bad,n);reject(bad,n);
    free(bad);free(data);hmbtn_session_destroy(two);hmbtn_session_destroy(loaded);hmbtn_session_destroy(s);
    puts("Native save codec: roundtrip, ownership, policy, version/corruption and effect-once checks passed.");return 0;
}

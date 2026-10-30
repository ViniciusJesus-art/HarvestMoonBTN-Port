#include "world_map.h"
#include "psx_data.h"
#include <stdio.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"World map line %d: %s\n",__LINE__,#x);return 1; } } while(0)
int main(void)
{
    unsigned logical,resource;
    for(unsigned season=0;season<4;++season) {
        CHECK(hmbtn_world_area(0,season,&logical,&resource) && logical==0 && resource==season);
        CHECK(hmbtn_world_area(80,season,&logical,&resource) && logical==80 && resource==80+season);
        for(unsigned selection=24;selection<80;++selection) {
            CHECK(hmbtn_world_area(selection,season,&logical,&resource));
            CHECK(logical==selection/4*4 && resource==logical+season);
        }
    }
    for(unsigned season=0;season<4;++season) {
        CHECK(hmbtn_world_area(92,season,&logical,&resource) && logical==92 && resource==92+season);
        CHECK(hmbtn_world_area(96,season,&logical,&resource) && logical==96 && resource==96+season);
        CHECK(hmbtn_world_area(112,season,&logical,&resource) && logical==112 && resource==112+season);
    }
    for(unsigned season=0;season<4;++season) for(unsigned area=128;area<216;++area) {
        CHECK(hmbtn_world_area(area,season,&logical,&resource));
        CHECK(logical==(area&~3u) && resource==logical+season);
    }
    const unsigned pending[]={4,20,84,100,120,216,233,255,999,UINT32_MAX};
    for(unsigned i=0;i<sizeof(pending)/sizeof(pending[0]);++i)
        CHECK(!hmbtn_world_area(pending[i],0,&logical,&resource));
    CHECK(!hmbtn_world_area(32,4,&logical,&resource));
    CHECK(!hmbtn_world_area(32,0,NULL,&resource));
    uint8_t section[16]={0};hmbtn_write_le16(section+14,999);
    for(unsigned season=0;season<4;++season) {
        CHECK(hmbtn_world_song(section,sizeof(section),season,12)==season+1);
        CHECK(hmbtn_world_song(section,sizeof(section),season,17)==28);
        CHECK(hmbtn_world_music_volume(section,sizeof(section),season,12)==100);
        CHECK(hmbtn_world_music_volume(section,sizeof(section),season,17)==(season==3 ? 0u : 50u));
    }
    hmbtn_write_le16(section+14,27);CHECK(hmbtn_world_song(section,16,0,12)==27);
    hmbtn_write_le16(section+14,24);
    CHECK(hmbtn_world_song(section,16,0,12)==25 && hmbtn_world_song(section,16,0,17)==24);
    hmbtn_write_le16(section,0xB);CHECK(hmbtn_world_music_volume(section,16,0,17)==100);
    hmbtn_write_le16(section,0x33);CHECK(hmbtn_world_music_volume(section,16,0,17)==100);
    hmbtn_write_le16(section,0);hmbtn_write_le16(section+2,0xF);
    CHECK(hmbtn_world_music_volume(section,16,0,17)==0);
    hmbtn_write_le16(section+2,0x28);CHECK(hmbtn_world_music_volume(section,16,3,17)==100);
    hmbtn_write_le16(section+2,0);hmbtn_write_le16(section+8,0x1000);
    CHECK(hmbtn_world_music_volume(section,16,0,12)==80);
    CHECK(!hmbtn_world_song(NULL,0,0,12) && !hmbtn_world_music_volume(NULL,0,0,12));
    uint8_t bounds[0x948]={0};int32_t centre[3]={-2000,-800,5000};
    hmbtn_write_le16(bounds+0x938,(uint16_t)-1000);hmbtn_write_le16(bounds+0x940,2000);
    hmbtn_write_le16(bounds+0x944,(uint16_t)-3000);hmbtn_write_le16(bounds+0x93c,4000);
    hmbtn_world_camera(bounds,sizeof(bounds),centre);
    CHECK(centre[0]==-1000 && centre[1]==-800 && centre[2]==4000);
    centre[0]=3000;centre[2]=-4000;hmbtn_world_camera(bounds,sizeof(bounds),centre);
    CHECK(centre[0]==2000 && centre[1]==-800 && centre[2]==-3000);
    centre[0]=100;centre[2]=200;hmbtn_world_camera(bounds,sizeof(bounds),centre);
    CHECK(centre[0]==100 && centre[2]==200);
    hmbtn_world_camera(bounds,16,centre);CHECK(centre[0]==100 && centre[2]==200);
    puts("Seasonal region selection, unsupported destinations, regional tracks and volume rules passed.");return 0;
}

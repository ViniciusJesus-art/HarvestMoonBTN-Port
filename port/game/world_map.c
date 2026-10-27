#include "world_map.h"
#include "farm_clock.h"
#include "house_control.h"
#include "psx_data.h"

bool hmbtn_world_area(unsigned selection,unsigned season,unsigned *logical,unsigned *resource)
{
    if(season>3 || !logical || !resource) return false;
    if(selection>3 && (selection<24 || selection>=84)
            && !(selection>=92 && selection<100) && !(selection>=112 && selection<116)
            && !(selection>=128 && selection<216)) return false;
    *logical=selection&~3u;*resource=*logical+season;
    return true;
}
bool hmbtn_world_area_buildings(unsigned selection,unsigned season,bool upgraded,unsigned house_level,unsigned barn_level,unsigned *logical,unsigned *resource)
{
    if(house_level>1 || barn_level>1 || !hmbtn_world_area(selection,season,logical,resource)) return false;
    if(*logical==0) *resource=(house_level ? 8u+barn_level*4u : upgraded ? 4u : 0u)+season;
    else if(*logical==80) *resource=80+house_level*4+season;
    else if(*logical==112 && upgraded) *resource+=4;
    else if(*logical==96) *resource+=barn_level*4;
    return true;
}
bool hmbtn_world_area_coop(unsigned selection,unsigned season,bool upgraded,unsigned *logical,unsigned *resource)
{ return hmbtn_world_area_buildings(selection,season,upgraded,0,0,logical,resource); }
unsigned hmbtn_world_song_weather(const uint8_t *section,size_t size,unsigned season,unsigned hour,unsigned weather)
{
    if(!section || size<16 || season>3) return 0;
    const unsigned kind=hmbtn_read_le16(section+14);
    const bool night=hmbtn_farm_clock_period(hour)==3;
    if(kind==29 || kind==14) return kind;
    if(weather==3 || weather==4) return 23;
    if(weather==1) return 22;
    if(weather==2) return season+1;
    if(kind==24) return night ? 24u : 25u;
    if(night) return 28;
    if(kind==999) return season+1;
    if(kind>=31 && kind<=36) return kind;
    return 27;
}
unsigned hmbtn_world_music_volume_weather(const uint8_t *section,size_t size,unsigned season,unsigned hour,unsigned weather)
{
    if(!section || size<16) return 0;
    unsigned volume=hmbtn_farm_music_volume(section,size);
    const unsigned place=hmbtn_read_le16(section),location=hmbtn_read_le16(section+2);
    if(hmbtn_farm_clock_period(hour)==3) {
        if(place!=0xB && place!=0x33) volume/=2;
        if(weather==1 && !(hmbtn_read_le16(section+8)&0x1000u)) volume=100;
        if(location==0xF || season==3) volume=0;
    }
    return location==0x28 ? 100u : volume;
}
unsigned hmbtn_world_song(const uint8_t *section,size_t size,unsigned season,unsigned hour)
{ return hmbtn_world_song_weather(section,size,season,hour,0); }
unsigned hmbtn_world_music_volume(const uint8_t *section,size_t size,unsigned season,unsigned hour)
{ return hmbtn_world_music_volume_weather(section,size,season,hour,0); }
static int32_t half(const uint8_t *p)
{
    const unsigned value=hmbtn_read_le16(p);return value<32768 ? (int32_t)value : (int32_t)value-65536;
}
void hmbtn_world_camera(const uint8_t *section,size_t size,int32_t centre[3])
{
    if(!section || size<0x948 || !centre) return;
    const int32_t min_x=half(section+0x938),max_x=half(section+0x940);
    const int32_t min_z=half(section+0x944),max_z=half(section+0x93c);
    if(min_x>max_x || min_z>max_z) return;
    if(centre[0]<min_x) centre[0]=min_x;
    if(centre[0]>max_x) centre[0]=max_x;
    if(centre[2]<min_z) centre[2]=min_z;
    if(centre[2]>max_z) centre[2]=max_z;
}

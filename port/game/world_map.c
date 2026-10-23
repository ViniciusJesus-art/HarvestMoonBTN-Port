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
unsigned hmbtn_world_song(const uint8_t *section,size_t size,unsigned season,unsigned hour)
{
    if(!section || size<16 || season>3) return 0;
    const unsigned kind=hmbtn_read_le16(section+14);
    const bool night=hmbtn_farm_clock_period(hour)==3;
    if(kind==29 || kind==14) return kind;
    if(kind==24) return night ? 24u : 25u;
    if(night) return 28;
    if(kind==999) return season+1;
    if(kind>=31 && kind<=36) return kind;
    return 27;
}
unsigned hmbtn_world_music_volume(const uint8_t *section,size_t size,unsigned season,unsigned hour)
{
    if(!section || size<16) return 0;
    unsigned volume=hmbtn_farm_music_volume(section,size);
    const unsigned place=hmbtn_read_le16(section),location=hmbtn_read_le16(section+2);
    if(hmbtn_farm_clock_period(hour)==3) {
        if(place!=0xB && place!=0x33) volume/=2;
        if(location==0xF || season==3) volume=0;
    }
    return location==0x28 ? 100u : volume;
}
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

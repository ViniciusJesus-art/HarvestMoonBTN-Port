#include "text_resources.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Text resource line %d: %s; %s\n",__LINE__,#x,error);return 1; } } while(0)
int main(int argc,char **argv)
{
    char error[256]={0};uint8_t data[HMBTN_TEXT_RECORD_BYTES+2];
    HmbtnTextResourceRange range;
    CHECK(argc==2);
    for(unsigned bank=0;bank<5;++bank) {
        HmbtnTextResourceId id={HMBTN_TEXT_RECORD,bank,1};
        CHECK(hmbtn_text_resource_locate(id,&range));
        CHECK(range.offset==8192 && range.size==8192);
        memset(data,0xA5,sizeof(data));
        CHECK(hmbtn_text_resource_read(argv[1],id,data+1,8192,error,sizeof(error)));
        CHECK(data[0]==0xA5 && data[8193]==0xA5);
        for(unsigned i=1;i<=8192;++i) CHECK(data[i]=='A'+bank);
        id.record=2;CHECK(!hmbtn_text_resource_read(argv[1],id,data+1,8192,error,sizeof(error)));
    }
    HmbtnTextResourceId portrait={HMBTN_TEXT_PORTRAIT,1,0};
    CHECK(hmbtn_text_resource_locate(portrait,&range));
    CHECK(!strcmp(range.archive,"A_FILE/01_face.bin") && range.offset==6144 && range.size==6144);
    CHECK(hmbtn_text_resource_read(argv[1],portrait,data,6144,error,sizeof(error)));
    for(unsigned i=0;i<6144;++i) CHECK(data[i]=='P');
    const HmbtnTextResourceId bad[]={{HMBTN_TEXT_RECORD,5,0},{HMBTN_TEXT_RECORD,0,UINT32_MAX},
        {HMBTN_TEXT_PORTRAIT,UINT32_MAX,0},{HMBTN_TEXT_PORTRAIT,0,1},{(HmbtnTextResourceKind)99,0,0}};
    for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);++i) {
        CHECK(!hmbtn_text_resource_locate(bad[i],&range));
        CHECK(!range.archive && !range.size);
        memset(data,0xA5,sizeof(data));
        CHECK(!hmbtn_text_resource_read(argv[1],bad[i],data,8192,error,sizeof(error)) && data[0]==0xA5);
    }
    CHECK(!hmbtn_text_resource_read(argv[1],portrait,data,6143,error,sizeof(error)));
    CHECK(!hmbtn_text_resource_read(argv[1],portrait,NULL,6144,error,sizeof(error)));
    puts("Logical message/portrait ids: five banks, exact reads, guard bytes, missing records and overflow rejection passed.");return 0;
}

#include "action_resources.h"
#include <stdio.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Action resource line %d: %s\n",__LINE__,#x);return 1; } } while(0)
int main(void)
{
    HmbtnActionResourceRange r;
    CHECK(hmbtn_action_resource_locate(10,&r) && r.offset==0x3CDB800 && r.animation_bytes==776 && r.bytes==14336);
    CHECK(hmbtn_action_resource_locate(12,&r) && r.offset==0x3CE2800 && r.animation_bytes==352);
    CHECK(hmbtn_action_resource_locate(15,&r) && r.offset==0x3CED000 && r.animation_bytes==312);
    CHECK(hmbtn_action_resource_locate(4,&r) && r.offset==0x3CC6800 && r.animation_bytes==508);
    CHECK(hmbtn_action_resource_locate(22,&r) && r.animation_bytes==3012);
    CHECK(hmbtn_action_resource_locate(23,&r) && r.animation_bytes==1960);
    CHECK(!hmbtn_action_resource_locate(27,&r) && !hmbtn_action_resource_locate(28,&r)
        && !hmbtn_action_resource_locate(UINT32_MAX,&r) && !hmbtn_action_resource_locate(10,NULL));
    uint8_t byte;char error[128]={0};
    CHECK(!hmbtn_action_resource_read("missing",10,&byte,1,&r,error,sizeof(error)));
    puts("Original logical action rows, ANI boundaries and invalid resource requests passed.");return 0;
}

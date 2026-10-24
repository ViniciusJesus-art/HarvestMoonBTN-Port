#include "item_reaction.h"
#include <stdio.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Item reaction line %d: %s\n",__LINE__,#x);return 1; } } while(0)
int main(void)
{
    /* Golden original picture codes for real items: ten codes per 32-bit word. */
    const unsigned people[]={3,7,9,14,18,20,32};
    const unsigned items[]={58,62,161,162,163,165,233,235,236};
    const int expected[9][7]={
        {4,4,4,4,4,4,4},{4,3,4,4,4,4,4},{2,1,1,2,0,2,0},
        {2,3,1,2,2,2,1},{2,3,1,2,2,2,3},{2,2,2,2,2,2,4},
        {4,4,4,4,4,4,4},{4,4,4,4,4,4,4},{4,4,4,4,4,4,4}
    };
    for(unsigned i=0;i<9;++i) for(unsigned j=0;j<7;++j)
        CHECK(hmbtn_item_villager_reaction(items[i],people[j])==expected[i][j]);
    for(unsigned id=0;id<37;++id) {
        CHECK(hmbtn_item_villager_reaction(265,id)==(id==27 ? 6 : 5) && hmbtn_item_villager_reaction(266,id)==5);
        CHECK(hmbtn_item_villager_reaction(1,id)==-2);
        CHECK(hmbtn_item_villager_reaction(169,id)==-1);
    }
    CHECK(hmbtn_item_villager_reaction(380,3)==-1 && hmbtn_item_villager_reaction(161,40)==-1);
    puts("Original picture codes, word groups, absent pictures, tools and carried-animal reaction remapping passed.");return 0;
}

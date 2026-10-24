#include "psx_random.h"
uint32_t hmbtn_psx_random_range(uint32_t *state,uint32_t range)
{
    const uint32_t value=*state*UINT32_C(0x41C64E6D)+UINT32_C(12345);
    *state=value;
    return ((value>>16)*(range&UINT32_C(0xFFFF)))>>16;
}

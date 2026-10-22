#include <stdio.h>
#include <string.h>
#include "psx_data.h"

/* Explicit failures also run in Release builds where assert may be disabled. */
#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition); \
    return 1; } } while (0)

int main(void)
{
    const uint8_t bytes[] = {0xA5, 0x78, 0x56, 0x34, 0xF2, 0x5A};
    uint8_t out[6];
    CHECK(hmbtn_read_le16(bytes + 1) == UINT16_C(0x5678));
    CHECK(hmbtn_read_le32(bytes + 1) == UINT32_C(0xF2345678));
    memset(out, 0xA5, sizeof(out));
    hmbtn_write_le32(out + 1, UINT32_C(0xF2345678));
    CHECK(memcmp(out + 1, bytes + 1, 4) == 0);
    CHECK(out[0] == 0xA5 && out[5] == 0xA5);
    memset(out, 0xA5, sizeof(out));
    hmbtn_write_le16(out + 1, UINT16_C(0xFEDC));
    CHECK(out[1] == 0xDC && out[2] == 0xFE);
    CHECK(out[0] == 0xA5 && out[3] == 0xA5);
    hmbtn_write_le32(out, UINT32_MAX);
    CHECK(hmbtn_read_le32(out) == UINT32_MAX);
    CHECK(out[0] == 0xFF && out[3] == 0xFF);
    hmbtn_write_le32(out, 0);
    CHECK(hmbtn_read_le32(out) == 0);
    hmbtn_write_le16(out, UINT16_MAX);
    CHECK(out[0] == 0xFF && out[1] == 0xFF);
    CHECK(hmbtn_read_le16(out) == UINT16_MAX);
    hmbtn_write_le16(out, 0);
    CHECK(hmbtn_read_le16(out) == 0);
    return 0;
}

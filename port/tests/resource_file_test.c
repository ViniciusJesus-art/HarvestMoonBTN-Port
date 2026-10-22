#include "resource_file.h"
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "Failed at line %d: %s (%s)\n", __LINE__, #condition, error); \
    return 1; } } while (0)

int main(int argc, char **argv)
{
    char error[256] = {0};
    uint8_t out[5] = {0xA5, 0, 0, 0, 0xA5};
    CHECK(argc == 2);
    const char *root = argv[1];
    CHECK(hmbtn_resource_read(root, "resource_fixture.bin", 2, out + 1, 3, error, sizeof(error)));
    CHECK(memcmp(out + 1, "cde", 3) == 0 && out[0] == 0xA5 && out[4] == 0xA5);
    CHECK(hmbtn_resource_read(root, "resource_fixture.bin", 8, out, 0, error, sizeof(error)));
    CHECK(!hmbtn_resource_read(root, "resource_fixture.bin", 7, out, 2, error, sizeof(error)));
    CHECK(!hmbtn_resource_read(root, "resource_fixture.bin", UINT32_MAX, out, 1, error, sizeof(error)));
    CHECK(!hmbtn_resource_read(root, "../resource_fixture.bin", 0, out, 1, error, sizeof(error)));
    CHECK(!hmbtn_resource_read(root, "/resource_fixture.bin", 0, out, 1, error, sizeof(error)));
    CHECK(!hmbtn_resource_read(root, "C:\\file.bin", 0, out, 1, error, sizeof(error)));
    CHECK(!hmbtn_resource_read(root, "missing.bin", 0, out, 1, error, sizeof(error)));
    return 0;
}

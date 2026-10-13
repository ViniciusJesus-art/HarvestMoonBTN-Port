#include "types.h"
#include "compiled_out.h"
#include "debug_stubs.h"

/* Debug print of a 16-byte buffer. Compiled out of the release build. */
void debug_print_buf(void) {
    u8 buffer[16];
    DEBUG_PRINT_BUF(buffer);
}

/* Empty hooks of the debug module. Nothing calls them. */
static void debug_stub_1(void) {
}

static int debug_stub_2(void) {
    return 0;
}

static void debug_stub_3(void) {
}

static signed int debug_stub_4(void) {
    return 0;
}

static void debug_stub_5(void) {
}

static void debug_stub_6(void) {
}

static void debug_stub_7(void) {
}

#include "zero/assert.h"

static fw_assert_handler_t s_assert_handler = FW_NULL;

void fw_set_assert_handler(fw_assert_handler_t handler) {
    s_assert_handler = handler;
}

FW_NOINLINE void fw_assert_failed(const char *file, int line, const char *expr) {
    if (s_assert_handler != FW_NULL) {
        s_assert_handler(file, line, expr);
    }
    
    /* Default bare-metal trap / infinite spin loop */
    volatile int trap = 1;
    while (trap) {
        /* Hang in debug loop for hardware debugger probe / JTAG */
    }
}

#include "swap_lib_calls.h"
#include "os_io_seproxyhal.h"
#include "apdu.h"

void swap_check() {
    // io_seproxyhal_touch_tx_ok(NULL);
    finalize_successful_send(0);
    os_sched_exit(0);
}
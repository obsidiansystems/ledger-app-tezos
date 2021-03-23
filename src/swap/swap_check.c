#include "swap_lib_calls.h"
#include "os_io_seproxyhal.h"
#include "apdu.h"

size_t swap_check() {
    // scott check
    return finalize_successful_send(2);
}
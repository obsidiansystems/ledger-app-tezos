#include "ui.h"

// Order matters
#include "os.h"
#include "cx.h"

#include "globals.h"

__attribute__((noreturn))
void app_main(void);

__attribute__((section(".boot"))) int main(void) {
    // exit critical section
    __asm volatile("cpsie i");

    // ensure exception will work as planned
    os_boot();

    uint8_t tag;
    init_globals();
    global.stack_root = &tag;

    for (;;) {
        BEGIN_TRY {
            TRY {
                ui_init();

                io_seproxyhal_init();

                USB_power(0);
                USB_power(1);

                ui_initial_screen();

                app_main();
            }
            CATCH(EXCEPTION_IO_RESET) {
                // reset IO and UX
                continue;
            }
            CATCH_OTHER(e) {
                break;
            }
            FINALLY {
            }
        }
        END_TRY;
    }

    // Only reached in case of uncaught exception
#ifdef BAKING_APP
    io_seproxyhal_power_off(); // Should not be allowed dashboard access
#else
    exit_app();
#endif
}

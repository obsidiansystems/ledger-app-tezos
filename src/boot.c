#include "ui.h"

// Order matters
#include "os.h"
#include "cx.h"

#include "swap/swap_lib_calls.h"

#include "globals.h"

__attribute__((noreturn)) void app_main(void);

__attribute__((section(".boot"))) int main(arg0)
{
    // exit critical section
    __asm volatile("cpsie i");

    // ensure exception will work as planned
    os_boot();

    uint8_t tag;
    init_globals();
    global.stack_root = &tag;

    if (arg0 != 0)
    {
        // Called as library from another app
        struct libargs_s *args = (struct libargs_s *)arg0;
        if (args->id == 0x100)
        {
            library_main(args);
        }
        else
        {
            exit_app();
        }
    }
    else 
    {
        for (;;)
        {
            BEGIN_TRY
            {
                TRY
                {
                    ui_init();

                    io_seproxyhal_init();

#ifdef TARGET_NANOX
                    // grab the current plane mode setting
                    // requires "--appFlag 0x240" to be set in makefile
                    G_io_app.plane_mode = os_setting_get(OS_SETTING_PLANEMODE, NULL, 0);
#endif // TARGET_NANOX

                    USB_power(0);
                    USB_power(1);

#ifdef HAVE_BLE
                    BLE_power(0, NULL);
                    BLE_power(1, "Nano X");
#endif // HAVE_BLE

                    ui_initial_screen();

                    app_main();
                }
                CATCH(EXCEPTION_IO_RESET)
                {
                    // reset IO and UX
                    continue;
                }
                CATCH_OTHER(e)
                {
                    break;
                }
                FINALLY
                {
                }
            }
            END_TRY;
        }
    }

    // Only reached in case of uncaught exception
#ifdef BAKING_APP
    io_seproxyhal_power_off(); // Should not be allowed dashboard access
#else
    exit_app();
#endif
}
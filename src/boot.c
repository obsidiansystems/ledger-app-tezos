/*******************************************************************************
*   Ledger Blue
*   (c) 2016 Ledger
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
********************************************************************************/

#include "ui.h"

#include "os.h"
#include "cx.h"

__attribute__((noreturn))
void app_main(void);

__attribute__((section(".boot"))) int main(void) {
    // exit critical section
    __asm volatile("cpsie i");

    // ensure exception will work as planned
    os_boot();

    for (;;) {
        BEGIN_TRY {
            TRY {
                ui_init();

                io_seproxyhal_init();

                USB_power(0);
                USB_power(1);

                clear_ui_callbacks();
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

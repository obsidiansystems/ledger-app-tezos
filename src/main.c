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
*  See the License for * Linode?
** Figure out how to stop hosting Bud Wolfe's stuff
** Migrate e-mail/shareyourgifts.net webmail to thecodedmessage.com server
** Be down to one server
the specific language governing permissions and
*  limitations under the License.
********************************************************************************/

#include "apdu.h"
#include "apdu_pubkey.h"
#include "apdu_sign.h"
#include "baking_auth.h"
#include "paths.h"
#include "ui.h"

#include "os.h"
#include "cx.h"
#include <stdbool.h>
#include <string.h>

void app_main(void) {
    static apdu_handler handlers[INS_MASK + 1];

    // TODO: Consider using static initialization of a const, instead of this
    for (int i = 0; i < INS_MASK + 1; i++) {
        handlers[i] = handle_apdu_error;
    }
    handlers[INS_GET_PUBLIC_KEY] = handle_apdu_get_public_key;
    handlers[INS_RESET] = handle_apdu_reset;
    handlers[INS_SIGN] = handle_apdu_sign;
    handlers[INS_SIGN_UNSAFE] = handle_apdu_sign;
    handlers[INS_EXIT] = handle_apdu_exit;
    main_loop(handlers);
}

void app_exit(void) {
    BEGIN_TRY_L(exit) {
        TRY_L(exit) {
            os_sched_exit(-1);
        }
        FINALLY_L(exit) {
        }
    }
    END_TRY_L(exit);
}

// TODO: I have no idea what this function does, but it is called by the OS
unsigned short io_exchange_al(unsigned char channel, unsigned short tx_len) {
    switch (channel & ~(IO_FLAGS)) {
    case CHANNEL_KEYBOARD:
        break;

    // multiplexed io exchange over a SPI channel and TLV encapsulated protocol
    case CHANNEL_SPI:
        if (tx_len) {
            io_seproxyhal_spi_send(G_io_apdu_buffer, tx_len);

            if (channel & IO_RESET_AFTER_REPLIED) {
                reset();
            }
            return 0; // nothing received from the master so far (it's a tx
                      // transaction)
        } else {
            return io_seproxyhal_spi_recv(G_io_apdu_buffer,
                                          sizeof(G_io_apdu_buffer), 0);
        }

    default:
        THROW(INVALID_PARAMETER);
    }
    return 0;
}

__attribute__((section(".boot"))) int main(void) {
    // exit critical section
    __asm volatile("cpsie i");

    ui_init();

    // ensure exception will work as planned
    os_boot();

    BEGIN_TRY {
        TRY {
            io_seproxyhal_init();

            USB_power(1);

            ui_initial_screen();

            app_main();
        }
        CATCH_OTHER(e) {
        }
        FINALLY {
        }
    }
    END_TRY;

    app_exit();
}

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
#include "apdu_reset.h"

void app_main(void) {
    static apdu_handler handlers[INS_MASK + 1];

    // TODO: Consider using static initialization of a const, instead of this
    for (int i = 0; i < INS_MASK + 1; i++) {
        handlers[i] = handle_apdu_error;
    }
    handlers[INS_GET_PUBLIC_KEY] = handle_apdu_get_public_key;
    handlers[INS_PROMPT_PUBLIC_KEY] = handle_apdu_get_public_key;
#ifdef BAKING_APP
    handlers[INS_AUTHORIZE_BAKING] = handle_apdu_get_public_key;
    handlers[INS_RESET] = handle_apdu_reset;
    handlers[INS_QUERY_HWM] = handle_apdu_hwm;
#endif
    handlers[INS_SIGN] = handle_apdu_sign;
#ifndef BAKING_APP
    handlers[INS_SIGN_UNSAFE] = handle_apdu_sign;
#endif
    handlers[INS_EXIT] = handle_apdu_exit;
    main_loop(handlers);
}

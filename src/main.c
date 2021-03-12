#include "apdu_baking.h"
#include "apdu_hmac.h"
#include "apdu_pubkey.h"
#include "apdu_setup.h"
#include "apdu_sign.h"
#include "apdu.h"
#include "globals.h"
#include "memory.h"

bool called_from_swap;
swap_values_t swap_values;

__attribute__((noreturn))
void app_main(void) {
    // TODO: Consider using static initialization of a const, instead of this
    for (size_t i = 0; i < NUM_ELEMENTS(global.handlers); i++) {
        global.handlers[i] = handle_apdu_error;
    }
    global.handlers[APDU_INS(INS_VERSION)] = handle_apdu_version;
    global.handlers[APDU_INS(INS_GET_PUBLIC_KEY)] = handle_apdu_get_public_key;
    global.handlers[APDU_INS(INS_PROMPT_PUBLIC_KEY)] = handle_apdu_get_public_key;
    global.handlers[APDU_INS(INS_SIGN)] = handle_apdu_sign;
    global.handlers[APDU_INS(INS_GIT)] = handle_apdu_git;
    global.handlers[APDU_INS(INS_SIGN_WITH_HASH)] = handle_apdu_sign_with_hash;
#ifdef BAKING_APP
    global.handlers[APDU_INS(INS_AUTHORIZE_BAKING)] = handle_apdu_get_public_key;
    global.handlers[APDU_INS(INS_RESET)] = handle_apdu_reset;
    global.handlers[APDU_INS(INS_QUERY_AUTH_KEY)] = handle_apdu_query_auth_key;
    global.handlers[APDU_INS(INS_QUERY_MAIN_HWM)] = handle_apdu_main_hwm;
    global.handlers[APDU_INS(INS_SETUP)] = handle_apdu_setup;
    global.handlers[APDU_INS(INS_QUERY_ALL_HWM)] = handle_apdu_all_hwm;
    global.handlers[APDU_INS(INS_DEAUTHORIZE)] = handle_apdu_deauthorize;
    global.handlers[APDU_INS(INS_QUERY_AUTH_KEY_WITH_CURVE)] = handle_apdu_query_auth_key_with_curve;
    global.handlers[APDU_INS(INS_HMAC)] = handle_apdu_hmac;
#else
    global.handlers[APDU_INS(INS_SIGN_UNSAFE)] = handle_apdu_sign;
#endif
    main_loop(global.handlers, sizeof(global.handlers));
}

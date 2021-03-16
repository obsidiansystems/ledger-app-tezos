#include "apdu_pubkey.h"

#include "apdu.h"
#include "cx.h"
#include "globals.h"
#include "keys.h"
#include "protocol.h"
#include "to_string.h"
#include "ui.h"
#ifdef BAKING_APP
#include "baking_auth.h"
#endif // BAKING_APP

#include <string.h>

static bool pubkey_ok(void) {
    cx_ecfp_public_key_t public_key = {0};
    generate_public_key(&public_key, global.path_with_curve.derivation_type, &global.path_with_curve.bip32_path);
    delayed_send(provide_pubkey(G_io_apdu_buffer, &public_key));
    return true;
}

#ifdef BAKING_APP
static bool baking_ok(void) {
    authorize_baking(global.path_with_curve.derivation_type, &global.path_with_curve.bip32_path);
    pubkey_ok();
    return true;
}
#endif

#ifdef BAKING_APP
char const *const *get_baking_prompts() {
    static const char *const baking_prompts[] = {
        PROMPT("Authorize Baking"),
        PROMPT("Public Key Hash"),
        NULL,
    };
    return baking_prompts;
}
#endif

__attribute__((noreturn))
static void prompt_address(
#ifndef BAKING_APP
    __attribute__((unused))
#endif
    bool baking,
    ui_callback_t ok_cb,
    ui_callback_t cxl_cb
) {
    init_screen_stack();

#   ifdef BAKING_APP
    if (baking) {
        push_ui_callback("Authorize Baking", copy_string, "With Public Key?");
        push_ui_callback("Public Key Hash", bip32_path_with_curve_to_pkh_string, &global.path_with_curve);
    } else {
#   endif
        push_ui_callback("Provide", copy_string, "Public Key");
        push_ui_callback("Publick Key Hash", bip32_path_with_curve_to_pkh_string, &global.path_with_curve);
#   ifdef BAKING_APP
    }
#   endif

    ux_confirm_screen(ok_cb, cxl_cb);
}

size_t handle_apdu_get_public_key(uint8_t instruction) {
    uint8_t *dataBuffer = G_io_apdu_buffer + OFFSET_CDATA;

    if (READ_UNALIGNED_BIG_ENDIAN(uint8_t, &G_io_apdu_buffer[OFFSET_P1]) != 0) THROW(EXC_WRONG_PARAM);

    // do not expose pks without prompt through U2F (permissionless legacy comm in browser)
    if (instruction == INS_GET_PUBLIC_KEY) require_permissioned_comm();

    global.path_with_curve.derivation_type = parse_derivation_type(READ_UNALIGNED_BIG_ENDIAN(uint8_t, &G_io_apdu_buffer[OFFSET_CURVE]));

    size_t const cdata_size = READ_UNALIGNED_BIG_ENDIAN(uint8_t, &G_io_apdu_buffer[OFFSET_LC]);

#ifdef BAKING_APP
    if (cdata_size == 0 && instruction == INS_AUTHORIZE_BAKING) {
        copy_bip32_path_with_curve(&global.path_with_curve, &N_data.baking_key);
    } else {
#endif
        read_bip32_path(&global.path_with_curve.bip32_path, dataBuffer, cdata_size);
#ifdef BAKING_APP
        if (global.path_with_curve.bip32_path.length == 0) THROW(EXC_WRONG_LENGTH_FOR_INS);
    }
#endif

    cx_ecfp_public_key_t public_key = {0};
    generate_public_key(&public_key, global.path_with_curve.derivation_type, &global.path_with_curve.bip32_path);

    if (instruction == INS_GET_PUBLIC_KEY) {
        return provide_pubkey(G_io_apdu_buffer, &public_key);
    } else {
        // instruction == INS_PROMPT_PUBLIC_KEY || instruction == INS_AUTHORIZE_BAKING
        ui_callback_t cb;
        bool bake;
#ifdef BAKING_APP
        if (instruction == INS_AUTHORIZE_BAKING) {
            cb = baking_ok;
            bake = true;
        } else {
#endif
            // INS_PROMPT_PUBLIC_KEY
            cb = pubkey_ok;
            bake = false;
#ifdef BAKING_APP
        }
#endif
        prompt_address(bake, cb, delay_reject);
    }
}

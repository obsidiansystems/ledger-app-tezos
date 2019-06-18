#include "apdu_pubkey.h"

#include "apdu.h"
#include "baking_auth.h"
#include "cx.h"
#include "globals.h"
#include "keys.h"
#include "key_macros.h"
#include "protocol.h"
#include "to_string.h"
#include "ui.h"

#include <string.h>

#define G global.u.pubkey

static bool pubkey_ok(void) {
    delayed_send(provide_pubkey(G_io_apdu_buffer, &G.public_key));
    return true;
}

#ifdef BAKING_APP
static bool baking_ok(void) {
    authorize_baking(G.key.curve, &G.key.bip32_path);
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
    static size_t const TYPE_INDEX = 0;
    static size_t const ADDRESS_INDEX = 1;

#   ifdef BAKING_APP
    if (baking) {
        REGISTER_STATIC_UI_VALUE(TYPE_INDEX, "With Public Key?");
        register_ui_callback(ADDRESS_INDEX, bip32_path_with_curve_to_pkh_string, &G.key);
        ui_prompt(get_baking_prompts(), ok_cb, cxl_cb);
    } else {
#   endif
        static const char *const pubkey_labels[] = {
            PROMPT("Provide"),
            PROMPT("Public Key Hash"),
            NULL,
        };
        REGISTER_STATIC_UI_VALUE(TYPE_INDEX, "Public Key");
        register_ui_callback(ADDRESS_INDEX, bip32_path_with_curve_to_pkh_string, &G.key);
        ui_prompt(pubkey_labels, ok_cb, cxl_cb);
#   ifdef BAKING_APP
    }
#   endif
}

#ifdef BAKING_APP
size_t handle_apdu_authorize_baking(__attribute__((unused)) uint8_t instruction) {
    if (READ_UNALIGNED_BIG_ENDIAN(uint8_t, &G_io_apdu_buffer[OFFSET_P1]) != 0) THROW(EXC_WRONG_PARAM);

    uint8_t const curve_code = READ_UNALIGNED_BIG_ENDIAN(uint8_t, &G_io_apdu_buffer[OFFSET_CURVE]);
    G.key.curve = curve_code_to_curve(curve_code);

    size_t const cdata_size = READ_UNALIGNED_BIG_ENDIAN(uint8_t, &G_io_apdu_buffer[OFFSET_LC]);
    uint8_t const *const cdata = &G_io_apdu_buffer[OFFSET_CDATA];
    read_bip32_path(&G.key.bip32_path, cdata, cdata_size);
    if (G.key.bip32_path.length == 0) THROW(EXC_WRONG_LENGTH_FOR_INS);

    prompt_address(true, baking_ok, delay_reject);
}
#endif

size_t handle_apdu_get_public_key(uint8_t instruction) {
    if (READ_UNALIGNED_BIG_ENDIAN(uint8_t, &G_io_apdu_buffer[OFFSET_P1]) != 0) THROW(EXC_WRONG_PARAM);

    // do not expose pks without prompt over U2F (browser support)
    if (instruction == INS_GET_PUBLIC_KEY) require_hid();

    uint8_t const curve_code = READ_UNALIGNED_BIG_ENDIAN(uint8_t, &G_io_apdu_buffer[OFFSET_CURVE]);
    G.key.curve = curve_code_to_curve(curve_code);

    size_t const cdata_size = READ_UNALIGNED_BIG_ENDIAN(uint8_t, &G_io_apdu_buffer[OFFSET_LC]);
    uint8_t const *const cdata = &G_io_apdu_buffer[OFFSET_CDATA];
    read_bip32_path(&G.key.bip32_path, cdata, cdata_size);

    generate_public_key_cached(&G.public_key, &G.key);

    if (instruction == INS_GET_PUBLIC_KEY) {
        return provide_pubkey(G_io_apdu_buffer, &G.public_key);
    } else {
        // instruction == INS_PROMPT_PUBLIC_KEY
        prompt_address(false, pubkey_ok, delay_reject);
    }
}

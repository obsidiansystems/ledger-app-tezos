#include "apdu_pubkey.h"

#include "apdu.h"
#include "baking_auth.h"
#include "cx.h"
#include "globals.h"
#include "keys.h"
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

unsigned int handle_apdu_get_public_key(uint8_t instruction) {
    uint8_t *dataBuffer = G_io_apdu_buffer + OFFSET_CDATA;

    if (READ_UNALIGNED_BIG_ENDIAN(uint8_t, &G_io_apdu_buffer[OFFSET_P1]) != 0) THROW(EXC_WRONG_PARAM);

    // do not expose pks without prompt over U2F (browser support)
    if (instruction == INS_GET_PUBLIC_KEY) require_hid();

    uint8_t const curve_code = READ_UNALIGNED_BIG_ENDIAN(uint8_t, &G_io_apdu_buffer[OFFSET_CURVE]);
    G.key.curve = curve_code_to_curve(curve_code);

    size_t const cdata_size = READ_UNALIGNED_BIG_ENDIAN(uint8_t, &G_io_apdu_buffer[OFFSET_LC]);

#ifdef BAKING_APP
    if (cdata_size == 0 && instruction == INS_AUTHORIZE_BAKING) {
        copy_bip32_path_with_curve(&G.key, &N_data.baking_key);
    } else {
#endif
        read_bip32_path(&G.key.bip32_path, dataBuffer, cdata_size);
#ifdef BAKING_APP
        if (G.key.bip32_path.length == 0) THROW(EXC_WRONG_LENGTH_FOR_INS);
    }
#endif
    cx_ecfp_public_key_t const *const pubkey = generate_public_key(G.key.curve, &G.key.bip32_path);
    memcpy(&G.public_key, pubkey, sizeof(G.public_key));

    if (instruction == INS_GET_PUBLIC_KEY) {
        return provide_pubkey(G_io_apdu_buffer, &G.public_key);
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
        prompt_address(bake, G.key.curve, &G.public_key, cb, delay_reject);
    }
}

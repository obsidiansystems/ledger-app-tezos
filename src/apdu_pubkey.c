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
    authorize_baking(G.curve, &G.bip32_path);
    pubkey_ok();
    return true;
}
#endif

unsigned int handle_apdu_get_public_key(uint8_t instruction) {
    uint8_t *dataBuffer = G_io_apdu_buffer + OFFSET_CDATA;

    if (G_io_apdu_buffer[OFFSET_P1] != 0) {
        THROW(EXC_WRONG_PARAM);
    }

    // do not expose pks without prompt over U2F (browser support)
    if (instruction == INS_GET_PUBLIC_KEY) {
        require_hid();
    }

    G.curve = curve_code_to_curve(G_io_apdu_buffer[OFFSET_CURVE]);

#ifdef BAKING_APP
    if (G_io_apdu_buffer[OFFSET_LC] == 0 && instruction == INS_AUTHORIZE_BAKING) {
        G.curve = N_data.curve;
        copy_bip32_path(&G.bip32_path, &N_data.bip32_path);
    } else {
#endif
        read_bip32_path(&G.bip32_path, dataBuffer, G_io_apdu_buffer[OFFSET_LC]);
#ifdef BAKING_APP
        if (G.bip32_path.length == 0) {
            THROW(EXC_WRONG_LENGTH_FOR_INS);
        }
    }
#endif
    struct key_pair *pair = generate_key_pair(G.curve, &G.bip32_path);
    memset(&pair->private_key, 0, sizeof(pair->private_key));
    memcpy(&G.public_key, &pair->public_key, sizeof(G.public_key));

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
        prompt_address(bake, G.curve, &G.public_key, cb, delay_reject);
    }
}

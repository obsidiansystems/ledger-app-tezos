#include "apdu_pubkey.h"

#include "apdu.h"
#include "baking_auth.h"
#include "keys.h"

#include "cx.h"
#include "ui.h"

#include <string.h>

static cx_ecfp_public_key_t public_key;
static cx_curve_t curve;

// The following need to be persisted for baking app
static uint8_t path_length;
static uint32_t bip32_path[MAX_BIP32_PATH];

static int provide_pubkey(void) {
    int tx = 0;
    G_io_apdu_buffer[tx++] = public_key.W_len;
    os_memmove(G_io_apdu_buffer + tx,
               public_key.W,
               public_key.W_len);
    tx += public_key.W_len;
    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;
    return tx;
}

static bool pubkey_ok(void) {
    int tx = provide_pubkey();
    delayed_send(tx);
    return true;
}

#ifdef BAKING_APP
static bool baking_ok(void) {
    authorize_baking(curve, bip32_path, path_length);
    pubkey_ok();
    return true;
}
#endif

unsigned int handle_apdu_get_public_key(uint8_t instruction) {
    uint8_t *dataBuffer = G_io_apdu_buffer + OFFSET_CDATA;

    if (G_io_apdu_buffer[OFFSET_P1] != 0)
        THROW(EXC_WRONG_PARAM);

    // do not expose pks without prompt over U2F
    if (instruction == INS_GET_PUBLIC_KEY) {
        require_hid();
    }

    curve = curve_code_to_curve(G_io_apdu_buffer[OFFSET_CURVE]);

#ifdef BAKING_APP
    if (G_io_apdu_buffer[OFFSET_LC] == 0 && instruction == INS_AUTHORIZE_BAKING) {
        curve = N_data.curve;
        path_length = N_data.path_length;
        memcpy(bip32_path, N_data.bip32_path, sizeof(*bip32_path) * path_length);
    } else {
#endif
        path_length = read_bip32_path(G_io_apdu_buffer[OFFSET_LC], bip32_path, dataBuffer);
#ifdef BAKING_APP
        if (path_length == 0) {
            THROW(EXC_WRONG_LENGTH_FOR_INS);
        }
    }
#endif
    struct key_pair *pair = generate_key_pair(curve, path_length, bip32_path);
    os_memset(&pair->private_key, 0, sizeof(pair->private_key));
    memcpy(&public_key, &pair->public_key, sizeof(public_key));

    if (instruction == INS_GET_PUBLIC_KEY) {
        return provide_pubkey();
    } else {
        // instruction == INS_PROMPT_PUBLIC_KEY || instruction == INS_AUTHORIZE_BAKING
        callback_t cb;
        bool bake;
        bool prompt_your_address = false;
#ifdef BAKING_APP
        if (instruction == INS_AUTHORIZE_BAKING) {
            cb = baking_ok;
            bake = true;
        } else {
#endif
            // INS_PROMPT_PUBLIC_KEY
            prompt_your_address = instruction == INS_PROMPT_YOUR_ADDRESS;
            cb = pubkey_ok;
            bake = false;
#ifdef BAKING_APP
        }
#endif
        prompt_address(bake, prompt_your_address, curve, &public_key, cb, delay_reject);
    }
}

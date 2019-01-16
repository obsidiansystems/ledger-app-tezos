#include "apdu_pubkey.h"

#include "apdu.h"
#include "baking_auth.h"
#include "globals.h"
#include "keys.h"
#include "ui.h"

// Order matters
#include "cx.h"

#include <string.h>

static int provide_pubkey(void) {
    int tx = 0;
    G_io_apdu_buffer[tx++] = global.pubkey.public_key.W_len;
    os_memmove(G_io_apdu_buffer + tx,
               global.pubkey.public_key.W,
               global.pubkey.public_key.W_len);
    tx += global.pubkey.public_key.W_len;
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
    authorize_baking(global.pubkey.curve, global.pubkey.bip32_path, global.pubkey.bip32_path_length);
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

    global.pubkey.curve = curve_code_to_curve(G_io_apdu_buffer[OFFSET_CURVE]);

#ifdef BAKING_APP
    if (G_io_apdu_buffer[OFFSET_LC] == 0 && instruction == INS_AUTHORIZE_BAKING) {
        global.pubkey.curve = N_data.curve;
        global.pubkey.bip32_path_length = N_data.path_length;
        memcpy(global.pubkey.bip32_path, N_data.bip32_path, sizeof(*global.pubkey.bip32_path) * global.pubkey.bip32_path_length);
    } else {
#endif
        global.pubkey.bip32_path_length = read_bip32_path(G_io_apdu_buffer[OFFSET_LC], global.pubkey.bip32_path, dataBuffer);
#ifdef BAKING_APP
        if (global.pubkey.bip32_path_length == 0) {
            THROW(EXC_WRONG_LENGTH_FOR_INS);
        }
    }
#endif
    struct key_pair *pair = generate_key_pair(global.pubkey.curve, global.pubkey.bip32_path_length, global.pubkey.bip32_path);
    os_memset(&pair->private_key, 0, sizeof(pair->private_key));
    memcpy(&global.pubkey.public_key, &pair->public_key, sizeof(global.pubkey.public_key));

    if (instruction == INS_GET_PUBLIC_KEY) {
        return provide_pubkey();
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
        prompt_address(bake, global.pubkey.curve, &global.pubkey.public_key, cb, delay_reject);
    }
}

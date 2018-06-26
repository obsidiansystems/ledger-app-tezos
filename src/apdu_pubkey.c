#include "apdu_pubkey.h"

#include "apdu.h"
#include "baking_auth.h"
#include "prompt_pubkey.h"
#include "paths.h"

#include "cx.h"
#include "ui.h"

static cx_ecfp_public_key_t public_key;
static cx_curve_t curve;

// The following need to be persisted for baking app
static uint8_t path_length;
static uint32_t bip32_path[MAX_BIP32_PATH];

static int provide_pubkey() {
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

static void pubkey_ok() {
    int tx = provide_pubkey();
    delay_send(tx);
}

#ifdef BAKING_APP
static void baking_ok() {
    authorize_baking(curve, bip32_path, path_length);
    pubkey_ok();
}
#endif

unsigned int handle_apdu_get_public_key(uint8_t instruction) {
    uint8_t *dataBuffer = G_io_apdu_buffer + OFFSET_CDATA;

    cx_ecfp_private_key_t privateKey;

    if (G_io_apdu_buffer[OFFSET_P1] != 0)
        THROW(0x6B00);

    if(G_io_apdu_buffer[OFFSET_P2] > 2)
        THROW(0x6B00);

    switch(G_io_apdu_buffer[OFFSET_P2]) {
        case 0:
            curve = CX_CURVE_Ed25519;
            break;
        case 1:
            curve = CX_CURVE_SECP256K1;
            break;
        case 2:
            curve = CX_CURVE_SECP256R1;
            break;
        default:
            THROW(0x6B00);
    }

    path_length = read_bip32_path(G_io_apdu_buffer[OFFSET_LC], bip32_path, dataBuffer);
    generate_key_pair(curve, path_length, bip32_path, &public_key, &privateKey);

    os_memset(&privateKey, 0, sizeof(privateKey));

    if (instruction == INS_GET_PUBLIC_KEY) {
        return provide_pubkey();
    } else {
        // instruction == INS_PROMPT_PUBLIC_KEY || instruction == INS_AUTHORIZE_BAKING
        callback_t cb;
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
        prompt_address(bake, curve, &public_key, cb, delay_reject);
        THROW(ASYNC_EXCEPTION);
    }
}

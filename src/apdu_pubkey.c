#include "apdu_pubkey.h"

#include "apdu.h"
#include "cx.h"
#include "ui.h"

static bool pubkey_enabled;
static cx_curve_t curve;
static cx_ecfp_public_key_t public_key;

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

static void pubkey_ok(void *ignore) {
    pubkey_enabled = true;

    int tx = provide_pubkey();
    delay_send(tx);
}

unsigned int handle_apdu_get_public_key(uint8_t instruction) {
    uint8_t privateKeyData[32];
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
    }

    uint8_t path_length;
    uint32_t bip32_path[MAX_BIP32_PATH];

    path_length = read_bip32_path(bip32_path, dataBuffer);
    os_perso_derive_node_bip32(curve, bip32_path, path_length, privateKeyData, NULL);
    cx_ecfp_init_private_key(curve, privateKeyData, 32, &privateKey);
    cx_ecfp_generate_pair(curve, &public_key, &privateKey, 1);

    os_memset(&privateKey, 0, sizeof(privateKey));
    os_memset(privateKeyData, 0, sizeof(privateKeyData));

    if (curve == CX_CURVE_Ed25519) {
        cx_edward_compress_point(curve, public_key.W, public_key.W_len);
        public_key.W_len = 33;
    }

    if (pubkey_enabled) {
        return provide_pubkey();
    } else {
        prompt_address(public_key.W, public_key.W_len, pubkey_ok, delay_reject);
        THROW(ASYNC_EXCEPTION);
    }
}

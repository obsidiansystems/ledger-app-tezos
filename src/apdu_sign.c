#include "apdu_sign.h"

#include "apdu.h"
#include "baking_auth.h"
#include "blake2.h"
#include "protocol.h"

#include "cx.h"
#include "ui.h"

#include "sign_screens.h"

#define TEZOS_BUFSIZE 1024

static uint8_t message_data[TEZOS_BUFSIZE];
static uint32_t message_data_length;
static cx_curve_t curve;
static uint8_t bip32_path_length;
static uint32_t bip32_path[MAX_BIP32_PATH];

static int perform_signature(bool hash_first);

void sign_unsafe_ok() {
    int tx = perform_signature(false);
    delay_send(tx);
}

void sign_ok() {
    int tx = perform_signature(true);
    delay_send(tx);
}

#define P1_FIRST 0x00
#define P1_NEXT 0x01
#define P1_LAST_MARKER 0x80

unsigned int handle_apdu_sign(uint8_t instruction) {
    uint8_t p1 = G_io_apdu_buffer[OFFSET_P1];
    uint8_t *dataBuffer = G_io_apdu_buffer + OFFSET_CDATA;
    uint32_t dataLength = G_io_apdu_buffer[OFFSET_LC];

    bool last = (p1 & P1_LAST_MARKER) != 0;
    switch (p1 & ~P1_LAST_MARKER) {
    case P1_FIRST:
        os_memset(message_data, 0, TEZOS_BUFSIZE);
        message_data_length = 0;
        bip32_path_length = read_bip32_path(dataLength, bip32_path, dataBuffer);
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
        return_ok();
    case P1_NEXT:
        break;
    default:
        THROW(0x6B00);
    }

    if(G_io_apdu_buffer[OFFSET_P2] > 2) {
        THROW(0x6B00);
    }

    if (message_data_length + dataLength > TEZOS_BUFSIZE) {
        THROW(0x6C00);
    }

    os_memmove(message_data + message_data_length, dataBuffer, dataLength);
    message_data_length += dataLength;

    if (!last) {
        return_ok();
    }

    if (instruction == INS_SIGN_UNSAFE) {
        ASYNC_PROMPT(ui_sign_unsafe_screen, sign_unsafe_ok, delay_reject);
    }

    // Have raw data, can get insight into it
    switch (get_magic_byte(message_data, message_data_length)) {
        case MAGIC_BYTE_BLOCK:
        case MAGIC_BYTE_BAKING_OP:
#ifdef BAKING_APP
            check_baking_authorized(curve, message_data, message_data_length,
                                    bip32_path, bip32_path_length);
            return perform_signature(true);
#else
            THROW(0x9685);
#endif
        case MAGIC_BYTE_UNSAFE_OP:
        case MAGIC_BYTE_UNSAFE_OP2:
        case MAGIC_BYTE_UNSAFE_OP3:
#ifdef BAKING_APP
            THROW(0x9685);
#else
            ASYNC_PROMPT(ui_sign_screen, sign_ok, delay_reject);
#endif
        default:
            THROW(0x6C00);
    }
}

#define HASH_SIZE 32

static int perform_signature(bool hash_first) {
    uint8_t privateKeyData[32];
    cx_ecfp_private_key_t privateKey;
    static uint8_t hash[HASH_SIZE];
    uint8_t *data = message_data;
    uint32_t datalen = message_data_length;

#ifdef BAKING_APP
    update_high_water_mark(message_data, message_data_length);
#endif

    if (hash_first) {
        blake2b(hash, HASH_SIZE, message_data, message_data_length, NULL, 0);
        data = hash;
        datalen = HASH_SIZE;
    }

    os_perso_derive_node_bip32(curve,
                               bip32_path,
                               bip32_path_length,
                               privateKeyData,
                               NULL);

    cx_ecfp_init_private_key(curve,
                             privateKeyData,
                             32,
                             &privateKey);

    os_memset(privateKeyData, 0, sizeof(privateKeyData));

    int tx;
    switch (curve) {
    case CX_CURVE_Ed25519: {
        tx = cx_eddsa_sign(&privateKey,
                           0,
                           CX_SHA512,
                           data,
                           datalen,
                           NULL,
                           0,
                           &G_io_apdu_buffer[0],
                           64,
                           NULL);
    }
        break;
    case CX_CURVE_SECP256K1:
    case CX_CURVE_SECP256R1:
    {
        unsigned int info;
        tx = cx_ecdsa_sign(&privateKey,
                           CX_LAST | CX_RND_TRNG,
                           CX_NONE,
                           data,
                           datalen,
                           &G_io_apdu_buffer[0],
                           100,
                           &info);
        if (info & CX_ECCINFO_PARITY_ODD) {
            G_io_apdu_buffer[0] |= 0x01;
        }
    }
        break;
    default:
        THROW(0x6B00); // This should not be able to happen.
    }

    os_memset(&privateKey, 0, sizeof(privateKey));

    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;

    return tx;
}

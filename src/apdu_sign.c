#include "apdu_sign.h"

#include "apdu.h"
#include "baking_auth.h"
#include "blake2.h"
#include "protocol.h"
#include "prompt_pubkey.h"

#include "cx.h"
#include "ui.h"

#include "sign_screens.h"

#define TEZOS_BUFSIZE 512
#define SIGN_HASH_SIZE 32
#define BLAKE2B_BLOCKBYTES 128

static uint8_t message_data[TEZOS_BUFSIZE];
static uint32_t message_data_length;
static cx_curve_t curve;
static uint8_t bip32_path_length;
static uint32_t bip32_path[MAX_BIP32_PATH];

static blake2s_state hash_state;
static bool is_hash_state_inited;
static uint8_t magic_number;
static bool hash_only;

static void conditional_init_hash_state(void) {
    if (!is_hash_state_inited) {
        blake2b_init(&hash_state, SIGN_HASH_SIZE);
        is_hash_state_inited = true;
    }
}

static void hash_buffer(void) {
    const uint8_t *current = message_data;
    conditional_init_hash_state();
    while (message_data_length > BLAKE2B_BLOCKBYTES) {
        blake2b_update(&hash_state, current, BLAKE2B_BLOCKBYTES);
        message_data_length -= BLAKE2B_BLOCKBYTES;
        current += BLAKE2B_BLOCKBYTES;
    }
    // XXX use circular buffer at some point
    memmove(message_data, current, message_data_length);
}

static void finish_hashing(uint8_t *hash, size_t hash_size) {
    hash_buffer();
    blake2b_update(&hash_state, message_data, message_data_length);
    blake2b_final(&hash_state, hash, hash_size);
    message_data_length = 0;
    is_hash_state_inited = false;
}

static int perform_signature(bool hash_first);

static void sign_unsafe_ok(void) {
    int tx = perform_signature(false);
    delayed_send(tx);
}

static void bake_auth_ok(void) {
    authorize_baking(curve, bip32_path, bip32_path_length);
    int tx = perform_signature(true);
    delayed_send(tx);
}

static void sign_ok(void) {
    int tx = perform_signature(true);
    delayed_send(tx);
}

static void clear_data(void) {
    bip32_path_length = 0;
    message_data_length = 0;
    is_hash_state_inited = false;
    magic_number = 0;
    hash_only = false;
}

static void sign_reject(void) {
    clear_data();
    delay_reject();
}

#ifdef BAKING_APP
uint32_t baking_sign_complete(void) {
    // Have raw data, can get insight into it
    switch (magic_number) {
        case MAGIC_BYTE_BLOCK:
        case MAGIC_BYTE_BAKING_OP:
            guard_baking_authorized(curve, message_data, message_data_length,
                                    bip32_path, bip32_path_length);
            return perform_signature(true);

        case MAGIC_BYTE_UNSAFE_OP:
            guard_valid_self_delegation(message_data, message_data_length, curve,
                                        bip32_path_length, bip32_path);
            cx_ecfp_private_key_t priv_key;
            cx_ecfp_public_key_t pub_key;
            generate_key_pair(curve, bip32_path_length, bip32_path, &pub_key, &priv_key);
            memset(&priv_key, 0, sizeof(priv_key));
            prompt_address(true, curve, &pub_key, bake_auth_ok, sign_reject);
            THROW(ASYNC_EXCEPTION);

        case MAGIC_BYTE_UNSAFE_OP2:
        case MAGIC_BYTE_UNSAFE_OP3:
        default:
            THROW(EXC_PARSE_ERROR);
    }
}

#endif

#define P1_FIRST 0x00
#define P1_NEXT 0x01
#define P1_HASH_ONLY_NEXT 0x03 // You only need it once
#define P1_LAST_MARKER 0x80

unsigned int handle_apdu_sign(uint8_t instruction) {
    uint8_t p1 = G_io_apdu_buffer[OFFSET_P1];
    uint8_t *dataBuffer = G_io_apdu_buffer + OFFSET_CDATA;
    uint32_t dataLength = G_io_apdu_buffer[OFFSET_LC];

    bool last = (p1 & P1_LAST_MARKER) != 0;
    switch (p1 & ~P1_LAST_MARKER) {
    case P1_FIRST:
        clear_data();
        os_memset(message_data, 0, TEZOS_BUFSIZE);
        message_data_length = 0;
        bip32_path_length = read_bip32_path(dataLength, bip32_path, dataBuffer);
        switch(G_io_apdu_buffer[OFFSET_CURVE]) {
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
                THROW(EXC_WRONG_PARAM);
        }
        return_ok();
#ifndef BAKING_APP
    case P1_HASH_ONLY_NEXT:
        // This is a debugging Easter egg
        hash_only = true;
        // FALL THROUGH
#endif
    case P1_NEXT:
        if (bip32_path_length == 0) {
            THROW(EXC_WRONG_LENGTH_FOR_INS);
        }
        break;
    default:
        THROW(EXC_WRONG_PARAM);
    }

    if (instruction != INS_SIGN_UNSAFE && magic_number == 0) {
        magic_number = get_magic_byte(dataBuffer, dataLength);
    }

#ifndef BAKING_APP
    if (instruction == INS_SIGN) {
        hash_buffer();
    }
#endif

    if (message_data_length + dataLength > TEZOS_BUFSIZE) {
        THROW(EXC_PARSE_ERROR);
    }

    os_memmove(message_data + message_data_length, dataBuffer, dataLength);
    message_data_length += dataLength;

    if (!last) {
        return_ok();
    }

#ifdef BAKING_APP
    return baking_sign_complete();
#else
    if (instruction == INS_SIGN_UNSAFE) {
        ASYNC_PROMPT(ui_sign_unsafe_screen, sign_unsafe_ok, delay_reject);
    } else {
        switch (magic_number) {
            case MAGIC_BYTE_BLOCK:
            case MAGIC_BYTE_BAKING_OP:
            default:
                THROW(EXC_PARSE_ERROR);
            case MAGIC_BYTE_UNSAFE_OP:
            case MAGIC_BYTE_UNSAFE_OP2:
            case MAGIC_BYTE_UNSAFE_OP3:
                ASYNC_PROMPT(ui_sign_screen, sign_ok, delay_reject);
        }
    }
#endif
}

static int perform_signature(bool hash_first) {
    cx_ecfp_private_key_t privateKey;
    cx_ecfp_public_key_t publicKey;
    static uint8_t hash[SIGN_HASH_SIZE];
    uint8_t *data = message_data;
    uint32_t datalen = message_data_length;

#ifdef BAKING_APP
    update_high_water_mark(message_data, message_data_length);
#endif

    if (hash_first) {
        hash_buffer();
        finish_hashing(hash, sizeof(hash));
        data = hash;
        datalen = SIGN_HASH_SIZE;

#ifndef BAKING_APP
        if (hash_only) {
            memcpy(G_io_apdu_buffer, data, datalen);
            uint32_t tx = datalen;

            G_io_apdu_buffer[tx++] = 0x90;
            G_io_apdu_buffer[tx++] = 0x00;
            return tx;
        }
#endif
    }

    generate_key_pair(curve, bip32_path_length, bip32_path, &publicKey, &privateKey);

    uint32_t tx;
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
        THROW(EXC_WRONG_PARAM); // This should not be able to happen.
    }

    os_memset(&privateKey, 0, sizeof(privateKey));

    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;

    clear_data();

    return tx;
}

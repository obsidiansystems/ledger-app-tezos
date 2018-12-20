#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "os.h"
#include "cx.h"

#include "exception.h"

#define MAX_BIP32_PATH 10

// The following need to be persisted for baking app
extern uint8_t bip32_path_length;
extern uint32_t bip32_path[MAX_BIP32_PATH];

// Throws upon error
uint32_t read_bip32_path(uint32_t bytes, uint32_t *bip32_path, const uint8_t *buf);

struct key_pair {
    cx_ecfp_public_key_t public_key;
    cx_ecfp_private_key_t private_key;
};

struct key_pair *generate_key_pair(cx_curve_t curve, uint32_t path_size, uint32_t *bip32_path);

// TODO: Rename to KEY_HASH_SIZE
#define HASH_SIZE 20
#define PKH_STRING_SIZE 40

cx_ecfp_public_key_t *public_key_hash(uint8_t output[HASH_SIZE], cx_curve_t curve,
                                      const cx_ecfp_public_key_t *public_key);

enum curve_code {
    TEZOS_ED,
    TEZOS_SECP256K1,
    TEZOS_SECP256R1,
    TEZOS_NO_CURVE = 255,
};

static inline uint8_t curve_to_curve_code(cx_curve_t curve) {
    switch(curve) {
        case CX_CURVE_Ed25519:
            return TEZOS_ED;
        case CX_CURVE_SECP256K1:
            return TEZOS_SECP256K1;
        case CX_CURVE_SECP256R1:
            return TEZOS_SECP256R1;
        default:
            THROW(EXC_MEMORY_ERROR);
    }
}

static inline cx_curve_t curve_code_to_curve(uint8_t curve_code) {
    static const cx_curve_t curves[] = { CX_CURVE_Ed25519, CX_CURVE_SECP256K1, CX_CURVE_SECP256R1 };
    if (curve_code > sizeof(curves) / sizeof(*curves)) {
        THROW(EXC_WRONG_PARAM);
    }
    return curves[curve_code];
}

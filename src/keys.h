#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "os.h"
#include "cx.h"

#include "exception.h"

#define MAX_BIP32_PATH 10

// Throws upon error
uint32_t read_bip32_path(uint32_t bytes, uint32_t *bip32_path, const uint8_t *buf);

void generate_key_pair(cx_curve_t curve, uint32_t path_size, uint32_t *bip32_path,
                       cx_ecfp_public_key_t *public_key, cx_ecfp_private_key_t *private_key);

#define HASH_SIZE 20
void public_key_hash(uint8_t output[HASH_SIZE], cx_curve_t curve,
                     const cx_ecfp_public_key_t *restrict public_key,
                     cx_ecfp_public_key_t *restrict pubkey_out);

static inline uint8_t curve_to_curve_code(cx_curve_t curve) {
    switch(curve) {
        case CX_CURVE_Ed25519:
            return 0;
        case CX_CURVE_SECP256K1:
            return 1;
        case CX_CURVE_SECP256R1:
            return 2;
        default:
            THROW(EXC_MEMORY_ERROR);
    }
}

static inline cx_curve_t curve_code_to_curve(uint8_t curve_code) {
    static const cx_curve_t curves[] = { CX_CURVE_Ed25519, CX_CURVE_SECP256K1, CX_CURVE_SECP256R1 };
    if (curve_code > sizeof(curves) / sizeof(*curves)) {
        THROW(EXC_MEMORY_ERROR);
    }
    return curves[curve_code];
}

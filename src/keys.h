#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "exception.h"
#include "os_cx.h"
#include "types.h"

struct bip32_path_wire {
    uint8_t length;
    uint32_t components[0];
} __attribute__((packed));

// throws
size_t read_bip32_path(bip32_path_t *const out, uint8_t const *const in, size_t const in_size);

struct key_pair *generate_key_pair(cx_curve_t const curve, bip32_path_t const *const bip32_path);
cx_ecfp_public_key_t const *generate_public_key(cx_curve_t const curve, bip32_path_t const *const bip32_path);

cx_ecfp_public_key_t const *public_key_hash(
    uint8_t output[HASH_SIZE], cx_curve_t curve,
    cx_ecfp_public_key_t const *const restrict public_key);

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
            return TEZOS_NO_CURVE;
    }
}

static inline cx_curve_t curve_code_to_curve(uint8_t curve_code) {
    static const cx_curve_t curves[] = { CX_CURVE_Ed25519, CX_CURVE_SECP256K1, CX_CURVE_SECP256R1 };
    if (curve_code > sizeof(curves) / sizeof(*curves)) {
        THROW(EXC_WRONG_PARAM);
    }
    return curves[curve_code];
}

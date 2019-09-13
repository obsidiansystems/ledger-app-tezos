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

// Non-reentrant
struct key_pair *generate_key_pair_return_global(
    cx_curve_t const curve,
    bip32_path_t const *const bip32_path);

// Non-reentrant
static inline void generate_key_pair(
    struct key_pair *const out,
    cx_curve_t const curve,
    bip32_path_t const *const bip32_path
) {
    check_null(out);
    struct key_pair *const result = generate_key_pair_return_global(curve, bip32_path);
    memcpy(out, result, sizeof(*out));
}

// Non-reentrant
cx_ecfp_public_key_t const *generate_public_key_return_global(
    cx_curve_t const curve,
    bip32_path_t const *const bip32_path);

// Non-reentrant
static inline void generate_public_key(
    cx_ecfp_public_key_t *const out,
    cx_curve_t const curve,
    bip32_path_t const *const bip32_path
) {
    check_null(out);
    cx_ecfp_public_key_t const *const result = generate_public_key_return_global(curve, bip32_path);
    memcpy(out, result, sizeof(*out));
}

// Non-reentrant
cx_ecfp_public_key_t const *public_key_hash_return_global(
    uint8_t *const out, size_t const out_size,
    cx_curve_t const curve,
    cx_ecfp_public_key_t const *const restrict public_key);

// Non-reentrant
static inline void public_key_hash(
    uint8_t *const hash_out, size_t const hash_out_size,
    cx_ecfp_public_key_t *const pubkey_out, // pass NULL if this value is not desired
    cx_curve_t const curve,
    cx_ecfp_public_key_t const *const restrict public_key
) {
    cx_ecfp_public_key_t const *const pubkey = public_key_hash_return_global(
        hash_out, hash_out_size, curve, public_key);
    if (pubkey_out != NULL) {
        memcpy(pubkey_out, pubkey, sizeof(*pubkey_out));
    }
}

size_t sign(
    uint8_t *const out, size_t const out_size,
    bip32_path_with_curve_t const *const key,
    uint8_t const *const in, size_t const in_size);

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

#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "exception.h"
#include "memory.h"
#include "os_cx.h"
#include "types.h"

#if CX_APILEVEL <= 8
#error "CX_APILEVEL 8 and below is not supported"
#endif

struct bip32_path_wire {
    uint8_t length;
    uint32_t components[0];
} __attribute__((packed));

// throws
size_t read_bip32_path(bip32_path_t *const out, uint8_t const *const in, size_t const in_size);

int generate_key_pair(key_pair_t *key_pair,
                      derivation_type_t const derivation_type,
                      bip32_path_t const *const bip32_path);

// Non-reentrant
void public_key_hash(
    uint8_t *const hash_out,
    size_t const hash_out_size,
    cx_ecfp_public_key_t *const compressed_out,  // pass NULL if this value is not desired
    derivation_type_t const derivation_type,
    cx_ecfp_public_key_t const *const restrict public_key);

size_t sign(uint8_t *const out,
            size_t const out_size,
            derivation_type_t const derivation_type,
            key_pair_t const *const key,
            uint8_t const *const in,
            size_t const in_size);

// Read a curve code from wire-format and parse into `deviration_type`.
static inline derivation_type_t parse_derivation_type(uint8_t const curve_code) {
    switch (curve_code) {
        case 0:
            return DERIVATION_TYPE_ED25519;
        case 1:
            return DERIVATION_TYPE_SECP256K1;
        case 2:
            return DERIVATION_TYPE_SECP256R1;
        case 3:
            return DERIVATION_TYPE_BIP32_ED25519;
        default:
            THROW(EXC_WRONG_PARAM);
    }
}

// Convert `derivation_type` to wire-format.
static inline uint8_t unparse_derivation_type(derivation_type_t const derivation_type) {
    switch (derivation_type) {
        case DERIVATION_TYPE_ED25519:
            return 0;
        case DERIVATION_TYPE_SECP256K1:
            return 1;
        case DERIVATION_TYPE_SECP256R1:
            return 2;
        case DERIVATION_TYPE_BIP32_ED25519:
            return 3;
        default:
            THROW(EXC_REFERENCED_DATA_NOT_FOUND);
    }
}

static inline signature_type_t derivation_type_to_signature_type(
    derivation_type_t const derivation_type) {
    switch (derivation_type) {
        case DERIVATION_TYPE_SECP256K1:
            return SIGNATURE_TYPE_SECP256K1;
        case DERIVATION_TYPE_SECP256R1:
            return SIGNATURE_TYPE_SECP256R1;
        case DERIVATION_TYPE_ED25519:
            return SIGNATURE_TYPE_ED25519;
        case DERIVATION_TYPE_BIP32_ED25519:
            return SIGNATURE_TYPE_ED25519;
        default:
            return SIGNATURE_TYPE_UNSET;
    }
}

static inline cx_curve_t signature_type_to_cx_curve(signature_type_t const signature_type) {
    switch (signature_type) {
        case SIGNATURE_TYPE_SECP256K1:
            return CX_CURVE_SECP256K1;
        case SIGNATURE_TYPE_SECP256R1:
            return CX_CURVE_SECP256R1;
        case SIGNATURE_TYPE_ED25519:
            return CX_CURVE_Ed25519;
        default:
            return CX_CURVE_NONE;
    }
}

int generate_public_key(cx_ecfp_public_key_t *public_key,
                        derivation_type_t const derivation_type,
                        bip32_path_t const *const bip32_path);
/*******************************************************************************
*   Ledger Blue
*   (c) 2016 Ledger
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
********************************************************************************/

#include "keys.h"

#include "apdu.h"
#include "globals.h"
#include "memory.h"
#include "protocol.h"
#include "types.h"

#include <stdbool.h>
#include <string.h>

size_t read_bip32_path(bip32_path_t *const out, uint8_t const *const in, size_t const in_size) {
    struct bip32_path_wire const *const buf_as_bip32 = (struct bip32_path_wire const *)in;

    if (in_size < sizeof(buf_as_bip32->length)) THROW(EXC_WRONG_LENGTH_FOR_INS);

    size_t ix = 0;
    out->length = CONSUME_UNALIGNED_BIG_ENDIAN(ix, uint8_t, &buf_as_bip32->length);

    if (in_size - ix < out->length * sizeof(*buf_as_bip32->components)) THROW(EXC_WRONG_LENGTH_FOR_INS);
    if (out->length == 0 || out->length > NUM_ELEMENTS(out->components)) THROW(EXC_WRONG_VALUES);

    for (size_t i = 0; i < out->length; i++) {
        out->components[i] = CONSUME_UNALIGNED_BIG_ENDIAN(ix, uint32_t, &buf_as_bip32->components[i]);
    }

    return ix;
}

int crypto_derive_private_key(
    cx_ecfp_private_key_t *private_key,
    derivation_type_t const derivation_type,
    bip32_path_t const *const bip32_path
) {
    check_null(bip32_path);
    uint8_t raw_private_key[PRIVATE_KEY_DATA_SIZE] = {0};
    int error;

    cx_curve_t const cx_curve = signature_type_to_cx_curve(derivation_type_to_signature_type(derivation_type));

    BEGIN_TRY {
        TRY {
            if (derivation_type == DERIVATION_TYPE_ED25519) {
                // Old, non BIP32_Ed25519 way...
                os_perso_derive_node_bip32_seed_key(
                    HDW_ED25519_SLIP10, CX_CURVE_Ed25519, bip32_path->components, bip32_path->length,
                    raw_private_key, NULL, NULL, 0);
            } else {
                // derive the seed with bip32_path
                os_perso_derive_node_bip32(
                    cx_curve, bip32_path->components, bip32_path->length,
                    raw_private_key, NULL);
            }

            // new private_key from raw
            error = cx_ecfp_init_private_key(cx_curve, raw_private_key, 32, private_key);
        } FINALLY {
            explicit_bzero(raw_private_key, sizeof(raw_private_key));
        }
    }
    END_TRY;

    return error;
}

int crypto_init_public_key(
    derivation_type_t const derivation_type,
    cx_ecfp_private_key_t *private_key,
    cx_ecfp_public_key_t *public_key
) {

    cx_curve_t const cx_curve = signature_type_to_cx_curve(derivation_type_to_signature_type(derivation_type));
    int error;

    // generate corresponding public key
    error = cx_ecfp_generate_pair(cx_curve, public_key, private_key, 1);

    // If we're using the old curve, make sure to adjust accordingly.
    if (cx_curve == CX_CURVE_Ed25519) {
        cx_edward_compress_point(CX_CURVE_Ed25519,
                                 public_key->W,
                                 public_key->W_len);
        public_key->W_len = 33;
    }

    return error;
}

// The caller should not forget to bzero out the `key_pair` as it contains sensitive information.
int generate_key_pair(key_pair_t *key_pair,
                        derivation_type_t const derivation_type,
                        bip32_path_t const *const bip32_path
                      ) {
    // derive private key according to BIP32 path
    crypto_derive_private_key(&key_pair->private_key, derivation_type, bip32_path);
    // generate corresponding public key
    crypto_init_public_key(derivation_type, &key_pair->private_key, &key_pair->public_key);
    return (0);
}

int generate_public_key(cx_ecfp_public_key_t *public_key,
                    derivation_type_t const derivation_type,
                    bip32_path_t const *const bip32_path
                    ) {
    cx_ecfp_private_key_t private_key = {0};
    int error;

    error = crypto_derive_private_key(&private_key, derivation_type, bip32_path);
    if (error) {
        return (error);
    }
    error = crypto_init_public_key(derivation_type, &private_key, public_key);
    return (error);   
}

void public_key_hash(
    uint8_t *const hash_out, size_t const hash_out_size,
    cx_ecfp_public_key_t *compressed_out,
    derivation_type_t const derivation_type,
    cx_ecfp_public_key_t const *const restrict public_key)
{
    check_null(hash_out);
    check_null(public_key);
    if (hash_out_size < HASH_SIZE) THROW(EXC_WRONG_LENGTH);

    cx_ecfp_public_key_t compressed = {0};
    switch (derivation_type_to_signature_type(derivation_type)) {
        case SIGNATURE_TYPE_ED25519:
            {
                compressed.W_len = public_key->W_len - 1;
                memcpy(compressed.W, public_key->W + 1, compressed.W_len);
                break;
            }
        case SIGNATURE_TYPE_SECP256K1:
        case SIGNATURE_TYPE_SECP256R1:
            {
                memcpy(compressed.W, public_key->W, public_key->W_len);
                compressed.W[0] = 0x02 + (public_key->W[64] & 0x01);
                compressed.W_len = 33;
                break;
            }
        default:
            THROW(EXC_WRONG_PARAM);
    }

    cx_blake2b_t hash_state;
    cx_blake2b_init(&hash_state, HASH_SIZE*8); // cx_blake2b_init takes size in bits.
    cx_hash((cx_hash_t *) &hash_state, CX_LAST, compressed.W, compressed.W_len, hash_out, HASH_SIZE);
    if (compressed_out != NULL) {
        memmove(compressed_out, &compressed, sizeof (*compressed_out));
    }
}

size_t sign(
    uint8_t *const out, size_t const out_size,
    derivation_type_t const derivation_type,
    key_pair_t const *const pair,
    uint8_t const *const in, size_t const in_size
) {
    check_null(out);
    check_null(pair);
    check_null(in);

    size_t tx = 0;
    switch (derivation_type_to_signature_type(derivation_type)) {
    case SIGNATURE_TYPE_ED25519: {
        static size_t const SIG_SIZE = 64;
        if (out_size < SIG_SIZE) THROW(EXC_WRONG_LENGTH);
        tx += cx_eddsa_sign(
            &pair->private_key,
            0,
            CX_SHA512,
            (uint8_t const *)PIC(in),
            in_size,
            NULL,
            0,
            out,
            SIG_SIZE,
            NULL);
    }
        break;
    case SIGNATURE_TYPE_SECP256K1:
    case SIGNATURE_TYPE_SECP256R1:
    {
        static size_t const SIG_SIZE = 100;
        if (out_size < SIG_SIZE) THROW(EXC_WRONG_LENGTH);
        unsigned int info;
        tx += cx_ecdsa_sign(
            &pair->private_key,
            CX_LAST | CX_RND_RFC6979,
            CX_SHA256,  // historical reasons...semantically CX_NONE
            (uint8_t const *)PIC(in),
            in_size,
            out,
            SIG_SIZE,
            &info);
        if (info & CX_ECCINFO_PARITY_ODD) {
            out[0] |= 0x01;
        }
    }
        break;
    default:
        THROW(EXC_WRONG_PARAM); // This should not be able to happen.
    }

    return tx;
}

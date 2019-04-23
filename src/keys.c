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
#include "blake2.h"
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

struct key_pair *generate_key_pair_return_global(
    cx_curve_t const curve,
    bip32_path_t const *const bip32_path
) {
    check_null(bip32_path);
    struct priv_generate_key_pair *const priv = &global.priv.generate_key_pair;

#if CX_APILEVEL > 8
    if (curve == CX_CURVE_Ed25519) {
        os_perso_derive_node_bip32_seed_key(
            HDW_ED25519_SLIP10, curve, bip32_path->components, bip32_path->length,
            priv->privateKeyData, NULL, NULL, 0);
    } else {
#endif
        os_perso_derive_node_bip32(curve, bip32_path->components, bip32_path->length, priv->privateKeyData, NULL);
#if CX_APILEVEL > 8
    }
#endif
    cx_ecfp_init_private_key(curve, priv->privateKeyData, sizeof(priv->privateKeyData), &priv->res.private_key);
    cx_ecfp_generate_pair(curve, &priv->res.public_key, &priv->res.private_key, 1);

    if (curve == CX_CURVE_Ed25519) {
        cx_edward_compress_point(
            curve,
            priv->res.public_key.W,
            priv->res.public_key.W_len);
        priv->res.public_key.W_len = 33;
    }
    memset(priv->privateKeyData, 0, sizeof(priv->privateKeyData));
    return &priv->res;
}

cx_ecfp_public_key_t const *generate_public_key_return_global(
    cx_curve_t const curve,
    bip32_path_t const *const bip32_path
) {
    check_null(bip32_path);
    struct key_pair *const pair = generate_key_pair_return_global(curve, bip32_path);
    memset(&pair->private_key, 0, sizeof(pair->private_key));
    return &pair->public_key;
}

cx_ecfp_public_key_t const *public_key_hash_return_global(
    uint8_t *const out, size_t const out_size,
    cx_curve_t const curve,
    cx_ecfp_public_key_t const *const restrict public_key)
{
    check_null(out);
    check_null(public_key);
    if (out_size < HASH_SIZE) THROW(EXC_WRONG_LENGTH);

    cx_ecfp_public_key_t *const compressed = &global.priv.public_key_hash.compressed;
    switch (curve) {
        case CX_CURVE_Ed25519:
            {
                compressed->W_len = public_key->W_len - 1;
                memcpy(compressed->W, public_key->W + 1, compressed->W_len);
                break;
            }
        case CX_CURVE_SECP256K1:
        case CX_CURVE_SECP256R1:
            {
                memcpy(compressed->W, public_key->W, public_key->W_len);
                compressed->W[0] = 0x02 + (public_key->W[64] & 0x01);
                compressed->W_len = 33;
                break;
            }
        default:
            THROW(EXC_WRONG_PARAM);
    }

    b2b_state hash_state;
    b2b_init(&hash_state, HASH_SIZE);
    b2b_update(&hash_state, compressed->W, compressed->W_len);
    b2b_final(&hash_state, out, HASH_SIZE);
    return compressed;
}

size_t sign(
    uint8_t *const out, size_t const out_size,
    bip32_path_with_curve_t const *const key,
    uint8_t const *const in, size_t const in_size
) {
    check_null(out);
    check_null(key);
    check_null(in);

    struct key_pair *const pair = generate_key_pair_return_global(key->curve, &key->bip32_path);

    size_t tx = 0;
    switch (key->curve) {
    case CX_CURVE_Ed25519: {
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
    case CX_CURVE_SECP256K1:
    case CX_CURVE_SECP256R1:
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

    memset(&pair->private_key, 0, sizeof(pair->private_key));

    return tx;
}

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
#include "os.h"
#include "blake2.h"
#include "protocol.h"

#include <stdbool.h>
#include <string.h>

uint32_t read_bip32_path(uint32_t bytes, uint32_t *bip32_path, const uint8_t *buf) {
    uint32_t path_length = *buf;
    if (bytes < path_length * sizeof(uint32_t) + 1) THROW(EXC_WRONG_LENGTH_FOR_INS);

    buf++;

    if (path_length == 0 || path_length > MAX_BIP32_PATH) {
        screen_printf("Invalid path\n");
        THROW(EXC_WRONG_VALUES);
    }

    for (size_t i = 0; i < path_length; i++) {
        bip32_path[i] = READ_UNALIGNED_BIG_ENDIAN(uint32_t, (uint32_t*)buf);
        buf += 4;
    }

    return path_length;
}

struct key_pair *generate_key_pair(cx_curve_t curve, uint32_t path_length, uint32_t *bip32_path) {
    static uint8_t privateKeyData[32];
    static struct key_pair res;
#if CX_APILEVEL > 8
    if (curve == CX_CURVE_Ed25519) {
        os_perso_derive_node_bip32_seed_key(HDW_ED25519_SLIP10, curve, bip32_path, path_length,
                                            privateKeyData, NULL, NULL, 0);
    } else {
#endif
        os_perso_derive_node_bip32(curve, bip32_path, path_length, privateKeyData, NULL);
#if CX_APILEVEL > 8
    }
#endif
    cx_ecfp_init_private_key(curve, privateKeyData, 32, &res.private_key);
    cx_ecfp_generate_pair(curve, &res.public_key, &res.private_key, 1);

    if (curve == CX_CURVE_Ed25519) {
        cx_edward_compress_point(curve, res.public_key.W, res.public_key.W_len);
        res.public_key.W_len = 33;
    }
    os_memset(privateKeyData, 0, sizeof(privateKeyData));
    return &res;
}

cx_ecfp_public_key_t *public_key_hash(uint8_t output[HASH_SIZE], cx_curve_t curve,
                                      const cx_ecfp_public_key_t *restrict public_key) {
    static cx_ecfp_public_key_t compressed;
    switch (curve) {
        case CX_CURVE_Ed25519:
            {
                compressed.W_len = public_key->W_len - 1;
                memcpy(compressed.W, public_key->W + 1, compressed.W_len);
                break;
            }
        case CX_CURVE_SECP256K1:
        case CX_CURVE_SECP256R1:
            {
                memcpy(compressed.W, public_key->W, public_key->W_len);
                compressed.W[0] = 0x02 + (public_key->W[64] & 0x01);
                compressed.W_len = 33;
                break;
            }
        default:
            THROW(EXC_WRONG_PARAM);
    }
    b2b_init(&hash_state, HASH_SIZE);
    b2b_update(&hash_state, compressed.W, compressed.W_len);
    b2b_final(&hash_state, output, HASH_SIZE);
    return &compressed;
}

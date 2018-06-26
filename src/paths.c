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

#include "paths.h"

#include "os.h"
#include "blake2.h"

#include <stdbool.h>
#include <string.h>

#define MAX_INT_DIGITS 10

// This function does not output terminating null bytes.
uint32_t number_to_string(char *dest, uint32_t number) {
    char tmp[MAX_INT_DIGITS];
    char *const tmp_end = tmp + MAX_INT_DIGITS;
    char *ptr;
    for (ptr = tmp_end - 1; ptr >= tmp; ptr--) {
        *ptr = '0' + number % 10;
        number /= 10;
        if (number == 0) {
            break;
        }
    }
    int length = tmp_end - ptr;
    memcpy(dest, ptr, length);
    return length;
}

#define HARDENING_BIT (1u << 31)

// This function does not output terminating null bytes
static uint32_t path_item_to_string(char *dest, uint32_t input) {
    int length = number_to_string(dest, input & ~HARDENING_BIT);
    if (input & HARDENING_BIT) {
        dest[length] = '\'';
        length++;
    }
    return length;
}

// This function does output terminating null bytes
uint32_t path_to_string(char *buf, uint32_t path_length, uint32_t *bip32_path) {
    uint32_t offset = 0;
    for (uint32_t i = 0; i < path_length; i++) {
        offset += path_item_to_string(buf + offset, bip32_path[i]);
        if (i != path_length - 1) {
            buf[offset++] = '/';
        }
    }
    buf[offset++] = '\0';
    return offset;
}

uint32_t read_bip32_path(uint32_t bytes, uint32_t *bip32_path, const uint8_t *buf) {
    uint32_t path_length = *buf;
    if (bytes < path_length * sizeof(uint32_t) + 1) THROW(0x6B00);

    buf++;

    if (path_length == 0 || path_length > MAX_BIP32_PATH) {
        screen_printf("Invalid path\n");
        THROW(0x6a80);
    }

    for (uint32_t i = 0; i < path_length; i++) {
        bip32_path[i] = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | (buf[3]);
        buf += 4;
    }

    return path_length;
}

void generate_key_pair(cx_curve_t curve, uint32_t path_length, uint32_t *bip32_path,
                       cx_ecfp_public_key_t *public_key, cx_ecfp_private_key_t *private_key) {
    uint8_t privateKeyData[32];
    os_perso_derive_node_bip32(curve, bip32_path, path_length, privateKeyData, NULL);
    cx_ecfp_init_private_key(curve, privateKeyData, 32, private_key);
    cx_ecfp_generate_pair(curve, public_key, private_key, 1);

    if (curve == CX_CURVE_Ed25519) {
        cx_edward_compress_point(curve, public_key->W, public_key->W_len);
        public_key->W_len = 33;
    }
    os_memset(privateKeyData, 0, sizeof(privateKeyData));
}

void public_key_hash(uint8_t output[HASH_SIZE], cx_curve_t curve,
                     const cx_ecfp_public_key_t *public_key) {
    switch (curve) {
        case CX_CURVE_Ed25519:
            blake2b(output, HASH_SIZE, public_key->W + 1, public_key->W_len - 1, NULL, 0);
            return;
        case CX_CURVE_SECP256K1:
            {
                char bytes[33];
                memcpy(bytes, public_key->W, 33);
                bytes[0] = 0x02 + (public_key->W[64] & 0x01);
                blake2b(output, HASH_SIZE, bytes, sizeof(bytes), NULL, 0);
                return;
            }
        default:
            blake2b(output, HASH_SIZE, public_key->W, public_key->W_len, NULL, 0);
    }
}

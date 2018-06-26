#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "os.h"
#include "cx.h"

#define MAX_BIP32_PATH 10

// This function does not output terminating null bytes.
uint32_t number_to_string(char *dest, uint32_t number);

// Throws upon error
uint32_t read_bip32_path(uint32_t bytes, uint32_t *bip32_path, const uint8_t *buf);

void generate_key_pair(cx_curve_t curve, uint32_t path_size, uint32_t *bip32_path,
                       cx_ecfp_public_key_t *public_key, cx_ecfp_private_key_t *private_key);

#define HASH_SIZE 20
void public_key_hash(uint8_t output[HASH_SIZE], cx_curve_t curve,
                     const cx_ecfp_public_key_t *restrict public_key,
                     cx_ecfp_public_key_t *restrict pubkey_out);

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "keys.h"
#include "os.h"
#include "cx.h"
#include "ui.h"

int pubkey_to_pkh_string(char *buff, uint32_t buff_size, cx_curve_t curve,
                         const cx_ecfp_public_key_t *public_key);
int pkh_to_string(char *buff, uint32_t buff_size, cx_curve_t curve, uint8_t hash[HASH_SIZE]);

// These functions output terminating null bytes, and return the ending offset.
size_t number_to_string(char *dest, uint64_t number);
size_t microtez_to_string(char *dest, uint64_t number);

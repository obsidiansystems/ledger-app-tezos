#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "keys.h"
#include "operations.h"

#include "os.h"
#include "cx.h"
#include "ui.h"

#define EXPR_HASH_SIZE 32
#define EXPR_STRING_SIZE 56

int pubkey_to_pkh_string(char *buff, uint32_t buff_size, cx_curve_t curve,
                         const cx_ecfp_public_key_t *public_key);
int pkh_to_string(char *buff, uint32_t buff_size, cx_curve_t curve, const uint8_t hash[HASH_SIZE]);
int parsed_contract_to_string(char *buff, uint32_t buff_size, const struct parsed_contract *contract);
int expr_hash_to_string(char *buff, uint32_t buff_size, const uint8_t hash[EXPR_HASH_SIZE]);

// These functions output terminating null bytes, and return the ending offset.
#define MAX_INT_DIGITS 20
size_t number_to_string(char *dest, uint64_t number);
size_t microtez_to_string(char *dest, uint64_t number);

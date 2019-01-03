#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "keys.h"
#include "operations.h"

#include "os.h"
#include "cx.h"
#include "ui.h"

int pubkey_to_pkh_string(char *buff, uint32_t buff_size, cx_curve_t curve,
                         const cx_ecfp_public_key_t *public_key);
size_t protocol_hash_to_string(char *buff, const size_t buff_size, const uint8_t hash[PROTOCOL_HASH_SIZE]);
int parsed_contract_to_string(char *buff, uint32_t buff_size, const struct parsed_contract *contract);


// These functions output terminating null bytes, and return the ending offset.
#define MAX_INT_DIGITS 20
size_t number_to_string(char *dest, uint64_t number);
size_t microtez_to_string(char *dest, uint64_t number);

// These take their number parameter through a pointer and take a length
size_t number_to_string_indirect(char *dest, uint32_t buff_size, const uint64_t *number);
size_t microtez_to_string_indirect(char *dest, uint32_t buff_size, const uint64_t *number);

bool copy_string(char *dest, uint32_t buff_size, const char *src);

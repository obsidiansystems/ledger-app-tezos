#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "keys.h"
#include "operations.h"
#include "os_cx.h"
#include "types.h"
#include "ui.h"

void pubkey_to_pkh_string(char *buff, uint32_t buff_size, cx_curve_t curve,
                          const cx_ecfp_public_key_t *public_key);
void protocol_hash_to_string(char *buff, const size_t buff_size, const uint8_t hash[PROTOCOL_HASH_SIZE]);
void parsed_contract_to_string(char *buff, uint32_t buff_size, const struct parsed_contract *contract);
void chain_id_to_string(char *buff, size_t const buff_size, chain_id_t const chain_id);
void chain_id_to_string_with_aliases(char *const out, size_t const out_size, chain_id_t const *const chain_id);

// dest must be at least MAX_INT_DIGITS
size_t number_to_string(char *dest, uint64_t number);

// These take their number parameter through a pointer and take a length
void number_to_string_indirect64(char *dest, uint32_t buff_size, const uint64_t *number);
void number_to_string_indirect32(char *dest, uint32_t buff_size, const uint32_t *number);
void microtez_to_string_indirect(char *dest, uint32_t buff_size, const uint64_t *number);

// This is designed to be called with potentially unrelocated pointers from rodata tables
// for the src argument, so performs PIC on src.
void copy_string(char *const dest, size_t const buff_size, char const *const src);

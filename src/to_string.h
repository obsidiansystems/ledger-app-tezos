#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "keys.h"
#include "operations.h"
#include "os_cx.h"
#include "types.h"
#include "ui.h"

void pubkey_to_pkh_string(
    char *const out, size_t const out_size,
    derivation_type_t const derivation_type,
    cx_ecfp_public_key_t const *const public_key
);
void bip32_path_with_curve_to_pkh_string(
    char *const out, size_t const out_size,
    bip32_path_with_curve_t const *const key
);
void protocol_hash_to_string(char *const buff, size_t const buff_size, uint8_t const hash[PROTOCOL_HASH_SIZE]);
void parsed_contract_to_string(char *const buff, size_t const buff_size, parsed_contract_t const *const contract);
void lookup_parsed_contract_name(char *const buff, size_t const buff_size, parsed_contract_t const *const contract);
void chain_id_to_string(char *const buff, size_t const buff_size, chain_id_t const chain_id);
void chain_id_to_string_with_aliases(char *const out, size_t const out_size, chain_id_t const *const chain_id);

// dest must be at least MAX_INT_DIGITS
size_t number_to_string(char *const dest, uint64_t number);

// These take their number parameter through a pointer and take a length
void number_to_string_indirect64(char *const dest, size_t const buff_size, uint64_t const *const number);
void number_to_string_indirect32(char *const dest, size_t const buff_size, uint32_t const *const number);
void microtez_to_string_indirect(char *const dest, size_t const buff_size, uint64_t const *const number);

int microtez_to_string_indirect_no_throw(char *const dest, size_t const buff_size, uint64_t const *const number);

// `src` may be unrelocated pointer to rodata.
void copy_string(char *const dest, size_t const buff_size, char const *const src);

// Encodes binary blob to hex string.
// `in` may be unrelocated pointer to rodata.
void bin_to_hex(char *const out, size_t const out_size, uint8_t const *const in, size_t const in_size);
// Wrapper around `bin_to_hex` that works on `buffer_t`.
// `in` may be unrelocated pointer to rodata.
void buffer_to_hex(char *const out, size_t const out_size, buffer_t const *const in);

// Encodes binary blob to base58 string.
// `in` may be unrelocated pointer to rodata.
void bin_to_base58(char *const out, size_t const out_size, uint8_t const *const in, size_t const in_size);
// Wrapper around `bin_to_base58` that works on `buffer_t`.
// `in` may be unrelocated pointer to rodata.
void buffer_to_base58(char *const out, size_t const out_size, buffer_t const *const in);

#pragma once

#include <stdint.h>

#define MAX_BIP32_PATH 10

// This function does not output terminating null bytes.
uint32_t number_to_string(char *dest, uint32_t number);

// This function does output terminating null bytes.
uint32_t path_to_string(char *buf, uint32_t path_length, uint32_t *bip32_path);

// Throws upon error
uint32_t read_bip32_path(uint32_t bytes, uint32_t *bip32_path, const uint8_t *buf);

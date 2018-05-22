#pragma once

#include <stdint.h>

#define MAX_BIP32_PATH 10

uint32_t path_to_string(char *buf, uint32_t path_length, uint32_t *bip32_path);
uint32_t read_bip32_path(uint32_t *bip32_path, const uint8_t *buf);

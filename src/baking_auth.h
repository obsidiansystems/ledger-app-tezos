#pragma once

#include "os.h"
#include <stdbool.h>
#include <stdint.h>

#define MAX_BIP32_PATH 10

typedef struct {
    int highest_level;
    uint8_t path_length;
    uint32_t bip32_path[MAX_BIP32_PATH];
} nvram_data;
extern WIDE nvram_data N_data_real;
#define N_data (*(WIDE nvram_data*)PIC(&N_data_real))

bool authorize_baking(int level, uint32_t *bip32_path, uint8_t pathLength);
void write_highest_level(int level);
bool is_baking_authorized(uint32_t *bip32_path, uint8_t path_length);
bool is_level_authorized(int level);

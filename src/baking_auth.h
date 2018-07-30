#pragma once

#include "os.h"
#include <stdbool.h>
#include <stdint.h>

#include "apdu.h"

typedef uint32_t level_t;

#define MAX_BIP32_PATH 10

typedef struct {
    cx_curve_t curve;
    level_t highest_level;
    bool had_endorsement;
    uint8_t path_length;
    uint32_t bip32_path[MAX_BIP32_PATH];
} nvram_data;
extern WIDE nvram_data N_data_real;
#define N_data (*(WIDE nvram_data*)PIC(&N_data_real))

void authorize_baking(cx_curve_t curve, uint32_t *bip32_path, uint8_t pathLength);
void guard_baking_authorized(cx_curve_t curve, void *data, int datalen, uint32_t *bip32_path,
                             uint8_t path_length);
bool is_path_authorized(cx_curve_t curve, uint32_t *bip32_path, uint8_t path_length);
void update_high_water_mark(void *data, int datalen);
void write_highest_level(level_t level, bool is_endorsement);
bool is_level_authorized(level_t level, bool is_endorsement);
bool is_valid_level(level_t level);
void update_auth_text(void);

#pragma once

#include "apdu.h"
#include "operations.h"
#include "protocol.h"
#include "types.h"

#include <stdbool.h>
#include <stdint.h>

void authorize_baking(cx_curve_t const curve, bip32_path_t const *const bip32_path);
void guard_baking_authorized(parsed_baking_data_t const *const baking_data, bip32_path_with_curve_t const *const key);
bool is_path_authorized(cx_curve_t curve, bip32_path_t const *const bip32_path);
bool is_valid_level(level_t level);
void write_high_water_mark(parsed_baking_data_t const *const in);

void prompt_contract_for_baking(struct parsed_contract *contract, ui_callback_t ok_cb, ui_callback_t cxl_cb);
void prompt_address(bool bake, cx_curve_t curve,
                    const cx_ecfp_public_key_t *key,
                    ui_callback_t ok_cb, ui_callback_t cxl_cb) __attribute__((noreturn));

// Return false if it is invalid
bool parse_baking_data(parsed_baking_data_t *const out, void const *const data, size_t const length);

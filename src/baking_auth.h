#pragma once

#ifdef BAKING_APP

#include "apdu.h"
#include "operations.h"
#include "protocol.h"
#include "types.h"

#include <stdbool.h>
#include <stdint.h>

void authorize_baking(derivation_type_t const derivation_type,
                      bip32_path_t const *const bip32_path);
void guard_baking_authorized(parsed_baking_data_t const *const baking_data,
                             bip32_path_with_curve_t const *const key);
bool is_path_authorized(derivation_type_t const derivation_type,
                        bip32_path_t const *const bip32_path);
bool is_valid_level(level_t level);
void write_high_water_mark(parsed_baking_data_t const *const in);

// Return false if it is invalid
bool parse_baking_data(parsed_baking_data_t *const out,
                       void const *const data,
                       size_t const length);

bool parse_tenderbake_baking_data(parsed_baking_data_t *const out,
                                  void const *const data,
                                  size_t const length);

#endif  // #ifdef BAKING_APP

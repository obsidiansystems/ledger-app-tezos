#pragma once

#include <stddef.h>
#include <stdint.h>

size_t handle_apdu_reset(uint8_t instruction);
size_t handle_apdu_query_auth_key(uint8_t instruction);
size_t handle_apdu_query_auth_key_with_curve(uint8_t instruction);
size_t handle_apdu_main_hwm(uint8_t instruction);
size_t handle_apdu_all_hwm(uint8_t instruction);
size_t handle_apdu_deauthorize(uint8_t instruction);

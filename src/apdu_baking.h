#pragma once

#include <stdint.h>

unsigned int handle_apdu_reset(uint8_t instruction);
unsigned int handle_apdu_query_auth_key(uint8_t instruction);
unsigned int handle_apdu_main_hwm(uint8_t instruction);
unsigned int handle_apdu_all_hwm(uint8_t instruction);

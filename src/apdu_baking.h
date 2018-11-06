#pragma once

#include <stdint.h>

#define INS_RESET 0x06
#define INS_QUERY_AUTH_KEY 0x07
#define INS_QUERY_HWM 0x08

unsigned int handle_apdu_reset(uint8_t instruction);
unsigned int handle_apdu_query_auth_key(uint8_t instruction);
unsigned int handle_apdu_hwm(uint8_t instruction);

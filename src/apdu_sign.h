#pragma once

#include "apdu.h"

#define INS_SIGN 0x04
#define INS_SIGN_UNSAFE 0x05 // Data that is already hashed.

unsigned int handle_apdu_sign(uint8_t instruction);

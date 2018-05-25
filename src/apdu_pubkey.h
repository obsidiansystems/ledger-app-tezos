#pragma once

#include "apdu.h"

#define INS_GET_PUBLIC_KEY 0x02

unsigned int handle_apdu_get_public_key(uint8_t instruction);

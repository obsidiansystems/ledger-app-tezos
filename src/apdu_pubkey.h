#pragma once

#include "apdu.h"

#define INS_AUTHORIZE_BAKING 0x01
#define INS_GET_PUBLIC_KEY 0x02
#define INS_PROMPT_PUBLIC_KEY 0x03

unsigned int handle_apdu_get_public_key(uint8_t instruction);

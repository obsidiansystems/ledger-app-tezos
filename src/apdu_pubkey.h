#pragma once

#include "apdu.h"

size_t handle_apdu_get_public_key(uint8_t instruction);

#ifdef BAKING_APP
size_t handle_apdu_authorize_baking(uint8_t instruction);
#endif

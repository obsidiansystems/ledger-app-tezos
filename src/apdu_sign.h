#pragma once

#include "apdu.h"

size_t handle_apdu_sign(uint8_t instruction);
size_t handle_apdu_sign_with_hash(uint8_t instruction);

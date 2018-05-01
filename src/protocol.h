#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

bool is_block(const uint8_t *data, size_t length);
bool is_valid_block(const uint8_t *data, size_t length);

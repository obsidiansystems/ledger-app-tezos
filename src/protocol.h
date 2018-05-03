#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#define MAGIC_BYTE_BLOCK 0x01
#define MAGIC_BYTE_BAKING_OP 0x02
#define MAGIC_BYTE_UNSAFE_OP 0x03

static inline uint8_t get_magic_byte(const uint8_t *data, size_t length) {
    if (length == 0) return 0x00;
    else return *data;
}

static inline bool is_baking(const uint8_t *data, size_t length) {
    uint8_t byte = get_magic_byte(data, length);
    return byte == MAGIC_BYTE_BLOCK || byte == MAGIC_BYTE_BAKING_OP;
}

int32_t get_block_level(const uint8_t *data, size_t length);

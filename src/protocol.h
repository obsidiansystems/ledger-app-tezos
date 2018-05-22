#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#define MAGIC_BYTE_BLOCK 0x01
#define MAGIC_BYTE_BAKING_OP 0x02
#define MAGIC_BYTE_UNSAFE_OP 0x03
#define MAGIC_BYTE_UNSAFE_OP2 0x04
#define MAGIC_BYTE_UNSAFE_OP3 0x05

static inline uint8_t get_magic_byte(const uint8_t *data, size_t length) {
    if (length == 0) return 0x00;
    else return *data;
}

bool is_block_valid(const void *data, size_t length);

int32_t get_block_level(const void *data, size_t length); // Precondition: is_block returns true
int32_t read_unaligned_big_endian(const void *in);

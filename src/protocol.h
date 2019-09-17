#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "os.h"
#include "cx.h"
#include "types.h"

#define MAGIC_BYTE_INVALID 0x00
#define MAGIC_BYTE_BLOCK 0x01
#define MAGIC_BYTE_BAKING_OP 0x02
#define MAGIC_BYTE_UNSAFE_OP 0x03
#define MAGIC_BYTE_UNSAFE_OP2 0x04
#define MAGIC_BYTE_UNSAFE_OP3 0x05

static inline uint8_t get_magic_byte(uint8_t const *const data, size_t const length) {
    return (data == NULL || length == 0) ? MAGIC_BYTE_INVALID : *data;
}

#define READ_UNALIGNED_BIG_ENDIAN(type, in) \
    ({ \
        uint8_t const *bytes = (uint8_t const *)in; \
        uint8_t out_bytes[sizeof(type)]; \
        type res; \
\
        for (size_t i = 0; i < sizeof(type); i++) { \
            out_bytes[i] = bytes[sizeof(type) - i - 1]; \
        } \
        memcpy(&res, out_bytes, sizeof(type)); \
\
        res; \
    })

// Same as READ_UNALIGNED_BIG_ENDIAN but helps keep track of how many bytes
// have been read by adding sizeof(type) to the given counter.
#define CONSUME_UNALIGNED_BIG_ENDIAN(counter, type, addr) ({ counter += sizeof(type); READ_UNALIGNED_BIG_ENDIAN(type, addr); })

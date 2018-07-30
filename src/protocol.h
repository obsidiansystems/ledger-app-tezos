#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "baking_auth.h"

#include "cx.h"

#define MAGIC_BYTE_INVALID 0x00
#define MAGIC_BYTE_BLOCK 0x01
#define MAGIC_BYTE_BAKING_OP 0x02
#define MAGIC_BYTE_UNSAFE_OP 0x03
#define MAGIC_BYTE_UNSAFE_OP2 0x04
#define MAGIC_BYTE_UNSAFE_OP3 0x05

static inline uint8_t get_magic_byte(const uint8_t *data, size_t length) {
    if (data == NULL || length == 0) return MAGIC_BYTE_INVALID;
    else return *data;
}

struct parsed_baking_data {
    bool is_endorsement;
    level_t level;
};
// Return false if it is invalid
bool parse_baking_data(const void *data, size_t length, struct parsed_baking_data *out);

#define READ_UNALIGNED_BIG_ENDIAN(type, in) \
    ({ \
        const uint8_t *bytes = (uint8_t*)in; \
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

struct parsed_contract {
    uint8_t originated;
    uint8_t curve_code; // TEZOS_NO_CURVE in originated case
    uint8_t hash[HASH_SIZE];
};

struct parsed_operation_data {
    cx_ecfp_public_key_t public_key; // compressed
    struct parsed_contract signing;
    struct parsed_contract source;
    struct parsed_contract destination;
    uint32_t transaction_count;
    uint32_t delegation_count;
    uint64_t total_amount;
    uint64_t total_fee;
};

// Zero means correct parse, non-zero means problem
// Specific return code can be used for debugging purposes
uint32_t parse_operations(const void *data, size_t length, cx_curve_t curve, size_t path_length,
                          uint32_t *bip32_path, struct parsed_operation_data *out);

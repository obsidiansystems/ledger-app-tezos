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

enum operation_tag {
    OPERATION_TAG_NONE = -1,
    OPERATION_TAG_REVEAL = 7,
    OPERATION_TAG_TRANSACTION = 8,
    OPERATION_TAG_DELEGATION = 10,
};

#define MAX_OPERATIONS_PER_GROUP 4

struct parsed_operation {
    enum operation_tag tag;
    struct parsed_contract source;
    struct parsed_contract destination;
    uint64_t amount; // 0 where inappropriate
};

static inline bool is_safe_operation(const struct parsed_operation *op) {
    return op->tag == OPERATION_TAG_NONE || op->tag == OPERATION_TAG_REVEAL;
}

struct parsed_operation_group {
    cx_ecfp_public_key_t public_key; // compressed
    uint64_t total_fee;
    struct parsed_contract signing;
    struct parsed_operation operations[MAX_OPERATIONS_PER_GROUP];
};

static inline const struct parsed_operation *
find_sole_unsafe_operation(const struct parsed_operation_group *ops, enum operation_tag key) {
    const struct parsed_operation *res = NULL;
    for (size_t i = 0; i < MAX_OPERATIONS_PER_GROUP; i++) {
        if (is_safe_operation(&ops->operations[i])) continue;
        if (ops->operations[i].tag == key) {
            res = &ops->operations[i];
        } else {
            return NULL;
        }
    }
    return res;
}

// Zero means correct parse, non-zero means problem
// Specific return code can be used for debugging purposes
uint32_t parse_operations(const void *data, size_t length, cx_curve_t curve, size_t path_length,
                          uint32_t *bip32_path, struct parsed_operation_group *out);

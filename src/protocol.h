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
    OPERATION_TAG_NONE = -1, // Sentinal value, as 0 is possibly used for something
    OPERATION_TAG_REVEAL = 7,
    OPERATION_TAG_TRANSACTION = 8,
    OPERATION_TAG_DELEGATION = 10,
};

typedef uint32_t allowed_operation_set;

static inline void allow_operation(allowed_operation_set *ops, enum operation_tag tag) {
    *ops |= (1 << (uint32_t)tag);
}

static inline bool is_operation_allowed(allowed_operation_set ops, enum operation_tag tag) {
    return (ops & (1 << (uint32_t)tag)) != 0;
}

static inline void clear_operation_set(allowed_operation_set *ops) {
    *ops = 0;
}

struct parsed_operation {
    enum operation_tag tag;
    struct parsed_contract source;
    struct parsed_contract destination;
    uint64_t amount; // 0 where inappropriate
};

struct parsed_operation_group {
    cx_ecfp_public_key_t public_key; // compressed
    uint64_t total_fee;
    struct parsed_contract signing;
    struct parsed_operation operation;
};

// Throws upon invalid data.
// Allows arbitrarily many "REVEAL" operations but only one operation of any other type,
// which is the one it puts into the group.

// Returns pointer to static data -- non-reentrant as hell.
struct parsed_operation_group *
parse_operations(const void *data, size_t length, cx_curve_t curve, size_t path_length,
                 uint32_t *bip32_path, allowed_operation_set ops);

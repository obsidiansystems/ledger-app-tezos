#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "keys.h"
#include "protocol.h"

#include "cx.h"
#include "types.h"

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

// Allows arbitrarily many "REVEAL" operations but only one operation of any other type,
// which is the one it puts into the group.
bool parse_operations(
    struct parsed_operation_group *const out,
    uint8_t const *const data,
    size_t length,
    bip32_path_with_curve_t const *const key,
    allowed_operation_set ops
);

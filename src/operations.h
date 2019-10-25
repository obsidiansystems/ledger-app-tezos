#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "keys.h"
#include "protocol.h"

#include "cx.h"
#include "types.h"

typedef bool (*is_operation_allowed_t)(enum operation_tag);

// Allows arbitrarily many "REVEAL" operations but only one operation of any other type,
// which is the one it puts into the group.
bool parse_operations(
    struct parsed_operation_group *const out,
    uint8_t const *const data,
    size_t length,
    derivation_type_t curve,
    bip32_path_t const *const bip32_path,
    is_operation_allowed_t is_operation_allowed
);

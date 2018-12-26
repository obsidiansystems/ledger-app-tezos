#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "keys.h"
#include "protocol.h"

#include "cx.h"

struct parsed_contract {
    uint8_t originated;
    uint8_t curve_code; // TEZOS_NO_CURVE in originated case
                        // An implicit contract with TEZOS_NO_CURVE means not present
    uint8_t hash[HASH_SIZE];
};

#define PROTOCOL_HASH_SIZE 32

struct parsed_proposal {
    int32_t voting_period;
    uint8_t protocol_hash[PROTOCOL_HASH_SIZE];
};

enum ballot_vote {
    BALLOT_VOTE_YEA,
    BALLOT_VOTE_NAY,
    BALLOT_VOTE_PASS,
};

struct parsed_ballot {
    int32_t voting_period;
    uint8_t protocol_hash[PROTOCOL_HASH_SIZE];
    enum ballot_vote vote;
};

enum operation_tag {
    OPERATION_TAG_NONE = -1, // Sentinal value, as 0 is possibly used for something
    OPERATION_TAG_PROPOSAL = 5,
    OPERATION_TAG_BALLOT = 6,
    OPERATION_TAG_REVEAL = 7,
    OPERATION_TAG_TRANSACTION = 8,
    OPERATION_TAG_ORIGINATION = 9,
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

#define ORIGINATION_FLAG_SPENDABLE 1
#define ORIGINATION_FLAG_DELEGATABLE 2

struct parsed_operation {
    enum operation_tag tag;
    struct parsed_contract source;
    struct parsed_contract destination;
    struct parsed_contract delegate; // For originations only
    struct parsed_proposal proposal; // For proposals only
    struct parsed_ballot ballot; // For ballots only
    uint64_t amount; // 0 where inappropriate
    uint32_t flags;  // Interpretation depends on operation type
};

struct parsed_operation_group {
    cx_ecfp_public_key_t public_key; // compressed
    uint64_t total_fee;
    uint64_t total_storage_limit;
    bool has_reveal;
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

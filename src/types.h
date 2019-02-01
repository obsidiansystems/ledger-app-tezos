#pragma once

#include "os.h"
#include "os_io_seproxyhal.h"

#include <stdbool.h>
#include <string.h>

// Return number of bytes to transmit (tx)
typedef uint32_t (*apdu_handler)(uint8_t instruction);

typedef uint32_t level_t;

#define CHAIN_ID_BASE58_STRING_SIZE (15 + 1) // with null termination

#define MAX_INT_DIGITS 20

typedef struct {
    uint32_t v;
} chain_id_t;

// Mainnet Chain ID: NetXdQprcVkpaWU
static chain_id_t const mainnet_chain_id = { .v = 0x7A06A770 };

// UI
typedef bool (*ui_callback_t)(void); // return true to go back to idle screen

// Uses K&R style declaration to avoid being stuck on const void *, to avoid having to cast the
// function pointers.
typedef void (*string_generation_callback)(/* char *buffer, size_t buffer_size, const void *data */);

// Keys
struct key_pair {
    cx_ecfp_public_key_t public_key;
    cx_ecfp_private_key_t private_key;
};

// Baking Auth
#define MAX_BIP32_PATH 10

typedef struct {
    uint8_t length;
    uint32_t components[MAX_BIP32_PATH];
} bip32_path_t;

static inline void copy_bip32_path(bip32_path_t *const out, bip32_path_t const *const in) {
    memcpy(out->components, in->components, in->length * sizeof(*in->components));
    out->length = in->length;
}

static inline bool bip32_paths_eq(bip32_path_t const *const a, bip32_path_t const *const b) {
    return a == b || (
            a != NULL &&
            b != NULL &&
            a->length == b->length &&
            memcmp(a->components, b->components, a->length * sizeof(*a->components)) == 0
        );
}

typedef struct {
    level_t highest_level;
    bool had_endorsement;
} high_watermark_t;

typedef struct {
    chain_id_t main_chain_id;
    struct {
        high_watermark_t main;
        high_watermark_t test;
    } hwm;
    cx_curve_t curve;
    bip32_path_t bip32_path;
} nvram_data;


#define PKH_STRING_SIZE 40
#define PROTOCOL_HASH_BASE58_STRING_SIZE 52 // e.g. "ProtoBetaBetaBetaBetaBetaBetaBetaBetaBet11111a5ug96" plus null byte

#define MAX_SCREEN_COUNT 7 // Current maximum usage
#define PROMPT_WIDTH 16
#define VALUE_WIDTH PROTOCOL_HASH_BASE58_STRING_SIZE

// Macros to wrap a static prompt and value strings and ensure they aren't too long.
#define PROMPT(str) \
  ({ \
    _Static_assert(sizeof(str) <= PROMPT_WIDTH + 1/*null byte*/ , str " won't fit in the UI prompt."); \
    str; \
  })

#define STATIC_UI_VALUE(str) \
  ({ \
    _Static_assert(sizeof(str) <= VALUE_WIDTH + 1/*null byte*/, str " won't fit in the UI.".); \
    str; \
  })


// Operations
#define PROTOCOL_HASH_SIZE 32

// TODO: Rename to KEY_HASH_SIZE
#define HASH_SIZE 20

struct parsed_contract {
    uint8_t originated;
    uint8_t curve_code; // TEZOS_NO_CURVE in originated case
                        // An implicit contract with TEZOS_NO_CURVE means not present
    uint8_t hash[HASH_SIZE];
};

struct parsed_proposal {
    uint32_t voting_period;
    // TODO: Make 32 bit version of number_to_string_indirect
    uint8_t protocol_hash[PROTOCOL_HASH_SIZE];
};

enum ballot_vote {
    BALLOT_VOTE_YEA,
    BALLOT_VOTE_NAY,
    BALLOT_VOTE_PASS,
};

struct parsed_ballot {
    uint32_t voting_period;
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

// TODO: Make this an enum.
// Flags for parsed_operation.flag
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

// Maximum number of APDU instructions
#define INS_MAX 0x0C

#define APDU_INS(x) ({ \
    _Static_assert(x <= INS_MAX, "APDU instruction is out of bounds"); \
    x; \
})

#define STRCPY(buff, x) ({ \
    _Static_assert(sizeof(buff) >= sizeof(x) && sizeof(*x) == sizeof(char), "String won't fit in buffer"); \
    strcpy(buff, x); \
})

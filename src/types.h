#pragma once

#include "exception.h"
#include "os.h"
#include "os_io_seproxyhal.h"

#include <stdbool.h>
#include <string.h>

// Type-safe versions of true/false
#undef true
#define true ((bool) 1)
#undef false
#define false ((bool) 0)

// NOTE: There are *two* ways that "key type" or "curve code" are represented in
// this code base:
//   1. `derivation_type` represents how a key will be derived from the seed. It
//      is almost the same as `signature_type` but allows for multiple derivation
//      strategies for ed25519. This type is often parsed from the APDU
//      instruction. See `parse_derivation_type` for the mapping.
//   2. `signature_type` represents how a key will be used for signing.
//      The mapping from `derivation_type` to `signature_type` is injective.
//      See `derivation_type_to_signature_type`.
//      This type is parsed from Tezos data headers. See the relevant parsing
//      code for the mapping.
typedef enum {
    DERIVATION_TYPE_SECP256K1 = 1,
    DERIVATION_TYPE_SECP256R1 = 2,
    DERIVATION_TYPE_ED25519 = 3,
    DERIVATION_TYPE_BIP32_ED25519 = 4
} derivation_type_t;

typedef enum {
    SIGNATURE_TYPE_UNSET = 0,
    SIGNATURE_TYPE_SECP256K1 = 1,
    SIGNATURE_TYPE_SECP256R1 = 2,
    SIGNATURE_TYPE_ED25519 = 3
} signature_type_t;

typedef enum {
    BAKING_TYPE_BLOCK = 0,
    BAKING_TYPE_ENDORSEMENT = 1,
    BAKING_TYPE_TENDERBAKE_BLOCK = 2,
    BAKING_TYPE_TENDERBAKE_ENDORSEMENT = 3,
    BAKING_TYPE_TENDERBAKE_PREENDORSEMENT = 4
} baking_type_t;

// Return number of bytes to transmit (tx)
typedef size_t (*apdu_handler)(uint8_t instruction);

typedef uint32_t level_t;
typedef uint32_t round_t;

#define CHAIN_ID_BASE58_STRING_SIZE sizeof("NetXdQprcVkpaWU")

#define MAX_INT_DIGITS 20

typedef struct {
    size_t length;
    size_t size;
    uint8_t *bytes;
} buffer_t;

typedef struct {
    uint32_t v;
} chain_id_t;

// Mainnet Chain ID: NetXdQprcVkpaWU
static chain_id_t const mainnet_chain_id = {.v = 0x7A06A770};

// UI
typedef bool (*ui_callback_t)(void);  // return true to go back to idle screen

// Uses K&R style declaration to avoid being stuck on const void *, to avoid having to cast the
// function pointers.
typedef void (*string_generation_callback)(
    /* char *buffer, size_t buffer_size, const void *data */);

// Keys
typedef struct {
    cx_ecfp_public_key_t public_key;
    cx_ecfp_private_key_t private_key;
} key_pair_t;

// Baking Auth
#define MAX_BIP32_LEN 10

typedef struct {
    uint8_t length;
    uint32_t components[MAX_BIP32_LEN];
} bip32_path_t;

static inline void copy_bip32_path(bip32_path_t *const out, bip32_path_t volatile const *const in) {
    check_null(out);
    check_null(in);
    memcpy(out->components, (void *) in->components, in->length * sizeof(*in->components));
    out->length = in->length;
}

static inline bool bip32_paths_eq(bip32_path_t volatile const *const a,
                                  bip32_path_t volatile const *const b) {
    return a == b || (a != NULL && b != NULL && a->length == b->length &&
                      memcmp((void const *) a->components,
                             (void const *) b->components,
                             a->length * sizeof(*a->components)) == 0);
}

typedef struct {
    bip32_path_t bip32_path;
    derivation_type_t derivation_type;
} bip32_path_with_curve_t;

static inline void copy_bip32_path_with_curve(bip32_path_with_curve_t *const out,
                                              bip32_path_with_curve_t volatile const *const in) {
    check_null(out);
    check_null(in);
    copy_bip32_path(&out->bip32_path, &in->bip32_path);
    out->derivation_type = in->derivation_type;
}

static inline bool bip32_path_with_curve_eq(bip32_path_with_curve_t volatile const *const a,
                                            bip32_path_with_curve_t volatile const *const b) {
    return a == b || (a != NULL && b != NULL && bip32_paths_eq(&a->bip32_path, &b->bip32_path) &&
                      a->derivation_type == b->derivation_type);
}

typedef struct {
    level_t highest_level;
    round_t highest_round;
    bool had_endorsement;
    bool had_preendorsement;
    bool migrated_to_tenderbake;
} high_watermark_t;

typedef struct {
    chain_id_t main_chain_id;
    struct {
        high_watermark_t main;
        high_watermark_t test;
    } hwm;
    bip32_path_with_curve_t baking_key;
} nvram_data;

#define SIGN_HASH_SIZE 32  // TODO: Rename or use a different constant.

#define PKH_STRING_SIZE 40  // includes null byte // TODO: use sizeof for this.
#define PROTOCOL_HASH_BASE58_STRING_SIZE \
    sizeof("ProtoBetaBetaBetaBetaBetaBetaBetaBetaBet11111a5ug96")

#define MAX_SCREEN_STACK_SIZE 7  // Maximum number of screens in a flow.
#define PROMPT_WIDTH          16
#define VALUE_WIDTH           PROTOCOL_HASH_BASE58_STRING_SIZE

// Macros to wrap a static prompt and value strings and ensure they aren't too long.
#define PROMPT(str)                                                   \
    ({                                                                \
        _Static_assert(sizeof(str) <= PROMPT_WIDTH + 1 /*null byte*/, \
                       str " won't fit in the UI prompt.");           \
        str;                                                          \
    })

#define STATIC_UI_VALUE(str)                                                                       \
    ({                                                                                             \
        _Static_assert(sizeof(str) <= VALUE_WIDTH + 1 /*null byte*/, str " won't fit in the UI."); \
        str;                                                                                       \
    })

// Operations
#define PROTOCOL_HASH_SIZE 32

// TODO: Rename to KEY_HASH_SIZE
#define HASH_SIZE 20

// HASH_SIZE encoded in base-58 ASCII
#define HASH_SIZE_B58 36

typedef struct {
    chain_id_t chain_id;
    baking_type_t type;
    level_t level;
    round_t round;
    bool is_tenderbake;
} parsed_baking_data_t;

typedef struct parsed_contract {
    uint8_t originated;  // a lightweight bool
    signature_type_t
        signature_type;  // 0 in originated case
                         // An implicit contract with signature_type of 0 means not present

    uint8_t hash[HASH_SIZE];

    char *hash_ptr;
} parsed_contract_t;

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
    OPERATION_TAG_NONE = -1,  // Sentinal value, as 0 is possibly used for something
    OPERATION_TAG_PROPOSAL = 5,
    OPERATION_TAG_BALLOT = 6,
    OPERATION_TAG_ATHENS_REVEAL = 7,
    OPERATION_TAG_ATHENS_TRANSACTION = 8,
    OPERATION_TAG_ATHENS_ORIGINATION = 9,
    OPERATION_TAG_ATHENS_DELEGATION = 10,
    OPERATION_TAG_BABYLON_REVEAL = 107,
    OPERATION_TAG_BABYLON_TRANSACTION = 108,
    OPERATION_TAG_BABYLON_ORIGINATION = 109,
    OPERATION_TAG_BABYLON_DELEGATION = 110,
};

// TODO: Make this an enum.
// Flags for parsed_operation.flag
#define ORIGINATION_FLAG_SPENDABLE   1
#define ORIGINATION_FLAG_DELEGATABLE 2

struct parsed_operation {
    enum operation_tag tag;
    struct parsed_contract source;
    struct parsed_contract destination;
    union {
        struct parsed_contract delegate;  // For originations only
        struct parsed_proposal proposal;  // For proposals only
        struct parsed_ballot ballot;      // For ballots only
    };

    bool is_manager_tz_operation;
    struct parsed_contract implicit_account;  // For manager.tz transactions

    uint64_t amount;  // 0 where inappropriate
    uint32_t flags;   // Interpretation depends on operation type
};

struct parsed_operation_group {
    cx_ecfp_public_key_t public_key;  // compressed
    uint64_t total_fee;
    uint64_t total_storage_limit;
    bool has_reveal;
    struct parsed_contract signing;
    struct parsed_operation operation;
};

// Maximum number of APDU instructions
#define INS_MAX 0x0F

#define APDU_INS(x)                                                        \
    ({                                                                     \
        _Static_assert(x <= INS_MAX, "APDU instruction is out of bounds"); \
        x;                                                                 \
    })

#define STRCPY(buff, x)                                                         \
    ({                                                                          \
        _Static_assert(sizeof(buff) >= sizeof(x) && sizeof(*x) == sizeof(char), \
                       "String won't fit in buffer");                           \
        strcpy(buff, x);                                                        \
    })

#define CUSTOM_MAX(a, b)                   \
    ({                                     \
        __typeof__(a) ____a_ = (a);        \
        __typeof__(b) ____b_ = (b);        \
        ____a_ > ____b_ ? ____a_ : ____b_; \
    })

#define CUSTOM_MIN(a, b)                   \
    ({                                     \
        __typeof__(a) ____a_ = (a);        \
        __typeof__(b) ____b_ = (b);        \
        ____a_ < ____b_ ? ____a_ : ____b_; \
    })

typedef struct {
    uint64_t amount;
    uint64_t fees;
    char destination[57];
} swap_values_t;

extern bool called_from_swap;
extern swap_values_t swap_values;

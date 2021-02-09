#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "keys.h"
#include "protocol.h"

#include "cx.h"
#include "types.h"

typedef bool (*is_operation_allowed_t)(enum operation_tag);

// Wire format that gets parsed into `signature_type`.
typedef struct {
    uint8_t v;
} __attribute__((packed)) raw_tezos_header_signature_type_t;

struct operation_group_header {
    uint8_t magic_byte;
    uint8_t hash[32];
} __attribute__((packed));

struct implicit_contract {
    raw_tezos_header_signature_type_t signature_type;
    uint8_t pkh[HASH_SIZE];
} __attribute__((packed));

struct contract {
    uint8_t originated;
    union {
        struct implicit_contract implicit;
        struct {
            uint8_t pkh[HASH_SIZE];
            uint8_t padding;
        } originated;
    } u;
} __attribute__((packed));

struct delegation_contents {
    raw_tezos_header_signature_type_t signature_type;
    uint8_t hash[HASH_SIZE];
} __attribute__((packed));

struct proposal_contents {
    int32_t period;
    size_t num_bytes;
    uint8_t hash[PROTOCOL_HASH_SIZE];
} __attribute__((packed));

struct ballot_contents {
    int32_t period;
    uint8_t proposal[PROTOCOL_HASH_SIZE];
    int8_t ballot;
} __attribute__((packed));

typedef struct {
    uint8_t v[HASH_SIZE];
} __attribute__((packed)) hash_t;

struct int_subparser_state {
	uint32_t lineno; // Has to be in _all_ members of the subparser union.
	uint64_t value; // Still need to fix this.
	uint8_t shift;
};

struct nexttype_subparser_state {
  uint32_t lineno;
  union {
    raw_tezos_header_signature_type_t sigtype;

    struct operation_group_header ogh;

    struct implicit_contract ic;
    struct contract c;

    struct delegation_contents dc;
    struct proposal_contents pc;
    struct ballot_contents bc;

    hash_t ht;

    uint16_t i16;
    uint32_t i32;
    uint64_t i64;

    uint8_t raw[1];
    uint8_t key[64]; // FIXME: check key length for non-tz1.
    uint8_t text_pkh[HASH_SIZE_B58];
  } body;
  uint32_t fill_idx;
};

struct michelson_address_subparser_state {
  uint32_t lineno;
  uint8_t address_step;
  uint8_t micheline_type;
  uint32_t addr_length;
  hash_t key_hash;
  parsed_contract_t result; // Not neccessarily optimal, but easy.
  struct nexttype_subparser_state subsub_state;
  raw_tezos_header_signature_type_t signature_type;

};

union subparser_state {
	struct int_subparser_state integer;
	struct nexttype_subparser_state nexttype;
        struct michelson_address_subparser_state michelson_address;
};

struct parse_state {
	int16_t op_step;
	union subparser_state subparser_state;
	enum operation_tag tag;
        uint32_t argument_length;
        uint16_t michelson_op;
        uint16_t contract_code;

        // Places to stash textual base58-encoded PKHes.
        char base58_pkh1[HASH_SIZE_B58];
        char base58_pkh2[HASH_SIZE_B58];
};

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

void parse_operations_init(
    struct parsed_operation_group *const out,
    derivation_type_t derivation_type,
    bip32_path_t const *const bip32_path,
    struct parse_state *const state
    );

bool parse_operations_final(struct parse_state *const state, struct parsed_operation_group *const out);

bool parse_operations_packet(
    struct parsed_operation_group *const out,
    uint8_t const *const data,
    size_t length,
    is_operation_allowed_t is_operation_allowed
);

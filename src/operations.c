#include "operations.h"

#include "apdu.h"
#include "globals.h"
#include "memory.h"
#include "to_string.h"
#include "ui.h"
#include "michelson.h"

#include <stdint.h>
#include <string.h>

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

// Argument is to distinguish between different parse errors for debugging purposes only
__attribute__((noreturn))
static void parse_error(
#ifndef TEZOS_DEBUG
    __attribute__((unused))
#endif
    uint32_t lineno) {
#ifdef TEZOS_DEBUG
    THROW(0x9000 + lineno);
#else
    THROW(EXC_PARSE_ERROR);
#endif
}

#define PARSE_ERROR() parse_error(__LINE__)

static inline void advance_ix(size_t *ix, size_t length, size_t amount) {
    if (*ix + amount > length) PARSE_ERROR();

    *ix += amount;
}

static inline uint8_t next_byte(void const *data, size_t *ix, size_t length, uint32_t lineno) {
    if (*ix >= length) parse_error(lineno);
    uint8_t res = ((const char *)data)[*ix];
    (*ix)++;
    return res;
}

#define NEXT_BYTE(data, ix, length) next_byte(data, ix, length, __LINE__)

static inline uint64_t parse_z(void const *data, size_t *ix, size_t length, uint32_t lineno) {
    uint64_t acc = 0;
    for (uint8_t shift = 0; ; ) {
        const uint8_t byte = next_byte(data, ix, length, lineno);
        acc |= ((uint64_t)byte & 0x7F) << shift;
        if (!(byte & 0x80)) {
            break;
        }
        shift += 7;
    }
    return acc;
}

#define PARSE_Z(data, ix, length) parse_z(data, ix, length, __LINE__)

static inline uint64_t parse_z_michelson(void const *data, size_t *ix, size_t length, uint32_t lineno) {
    uint64_t acc = 0;
    for (uint8_t shift = 0; ; ) {
        const uint8_t byte = next_byte(data, ix, length, lineno);
        acc |= ((uint64_t)byte & 0x7F) << shift;
        if (!(byte & 0x80)) {
            break;
        }
        // For some reason we are getting numbers shifted 1 bit to the
        // left. TODO: figure out why this happens
        if (shift == 0) {
            shift += 6;
        } else {
            shift += 7;
        }
    }
    return acc;
}

#define PARSE_Z_MICHELSON(data, ix, length) parse_z_michelson(data, ix, length, __LINE__)

// This macro assumes:
// * Beginning of data: const void *data
// * Total length of data: size_t length
// * Current index of data: size_t ix
// Any function that uses these macros should have these as local variables
#define NEXT_TYPE(type) ({ \
    const type *val = data + ix; \
    advance_ix(&ix, length, sizeof(type)); \
    val; \
})

static inline signature_type_t parse_raw_tezos_header_signature_type(
    raw_tezos_header_signature_type_t const *const raw_signature_type
) {
    check_null(raw_signature_type);
    switch (READ_UNALIGNED_BIG_ENDIAN(uint8_t, &raw_signature_type->v)) {
        case 0: return SIGNATURE_TYPE_ED25519;
        case 1: return SIGNATURE_TYPE_SECP256K1;
        case 2: return SIGNATURE_TYPE_SECP256R1;
        default: PARSE_ERROR();
    }
}

static inline void compute_pkh(
    cx_ecfp_public_key_t *const compressed_pubkey_out,
    parsed_contract_t *const contract_out,
    derivation_type_t const derivation_type,
    bip32_path_t const *const bip32_path
) {
    check_null(bip32_path);
    check_null(compressed_pubkey_out);
    check_null(contract_out);
    cx_ecfp_public_key_t const *const pubkey = generate_public_key_return_global(derivation_type, bip32_path);
    public_key_hash(
        contract_out->hash, sizeof(contract_out->hash),
        compressed_pubkey_out,
        derivation_type, pubkey);
    contract_out->signature_type = derivation_type_to_signature_type(derivation_type);
    if (contract_out->signature_type == SIGNATURE_TYPE_UNSET) THROW(EXC_MEMORY_ERROR);
    contract_out->originated = 0;
}

static inline void parse_implicit(
    parsed_contract_t *const out,
    raw_tezos_header_signature_type_t const *const raw_signature_type,
    uint8_t const hash[HASH_SIZE]
) {
    check_null(raw_signature_type);
    out->originated = 0;
    out->signature_type = parse_raw_tezos_header_signature_type(raw_signature_type);
    memcpy(out->hash, hash, sizeof(out->hash));
}

static inline void parse_contract(parsed_contract_t *const out, struct contract const *const in) {
    out->originated = in->originated;
    if (out->originated == 0) { // implicit
        out->signature_type = parse_raw_tezos_header_signature_type(&in->u.implicit.signature_type);
        memcpy(out->hash, in->u.implicit.pkh, sizeof(out->hash));
    } else { // originated
        out->signature_type = SIGNATURE_TYPE_UNSET;
        memcpy(out->hash, in->u.originated.pkh, sizeof(out->hash));
    }
}

static inline uint32_t michelson_read_length(void const *data, size_t *ix, size_t length, uint32_t lineno) {
    if (*ix >= length) parse_error(lineno);
    const uint32_t res = READ_UNALIGNED_BIG_ENDIAN(uint32_t, &data[*ix]);
    (*ix) += sizeof(uint32_t);
    return res;
}

#define MICHELSON_READ_LENGTH(data, ix, length) michelson_read_length(data, ix, length, __LINE__)

static inline uint16_t michelson_read_short(void const *data, size_t *ix, size_t length, uint32_t lineno) {
    if (*ix >= length) parse_error(lineno);
    const uint16_t res = READ_UNALIGNED_BIG_ENDIAN(uint16_t, &data[*ix]);
    (*ix) += sizeof(uint16_t);
    return res;
}

#define MICHELSON_READ_SHORT(data, ix, length) michelson_read_short(data, ix, length, __LINE__)

static inline void michelson_read_address(parsed_contract_t *const out, const void *data, size_t *ix, size_t length) {
    switch (NEXT_BYTE(data, ix, length)) {
        case MICHELSON_TYPE_BYTE_SEQUENCE: {
            // Need 1 byte for signature, plus the rest of the hash.
            if (MICHELSON_READ_LENGTH(data, ix, length) != HASH_SIZE + 1) {
                PARSE_ERROR();
            }
            const raw_tezos_header_signature_type_t* signature_type = data + *ix;
            advance_ix(ix, length, sizeof(raw_tezos_header_signature_type_t));
            const hash_t *key_hash = data + *ix;
            advance_ix(ix, length, sizeof(hash_t));
            parse_implicit(out, signature_type, (const uint8_t *)key_hash);
            break;
        }
        case MICHELSON_TYPE_STRING: {
            if (MICHELSON_READ_LENGTH(data, ix, length) != HASH_SIZE_B58) {
                PARSE_ERROR();
            }
            out->hash_ptr = (char*)data + *ix;
            (*ix) += 36;
            out->originated = false;
            out->signature_type = SIGNATURE_TYPE_UNSET;
            break;
        }
        default: PARSE_ERROR();
    }
}

static void parse_operations_throws_parse_error(
    struct parsed_operation_group *const out,
    void const *const data,
    size_t length,
    derivation_type_t derivation_type,
    bip32_path_t const *const bip32_path,
    is_operation_allowed_t is_operation_allowed
) {
    check_null(out);
    check_null(data);
    check_null(bip32_path);
    memset(out, 0, sizeof(*out));

    out->operation.tag = OPERATION_TAG_NONE;

    compute_pkh(&out->public_key, &out->signing, derivation_type, bip32_path);

    size_t ix = 0;

    // Verify magic byte, ignore block hash
    const struct operation_group_header *ogh = NEXT_TYPE(struct operation_group_header);
    if (ogh->magic_byte != MAGIC_BYTE_UNSAFE_OP) PARSE_ERROR();

    // Start out with source = signing, for reveals
    // TODO: This is slightly hackish
    memcpy(&out->operation.source, &out->signing, sizeof(out->signing));

    while (ix < length) {
        const enum operation_tag tag = NEXT_BYTE(data, &ix, length);  // 1 byte is always aligned

        if (!is_operation_allowed(tag)) PARSE_ERROR();

        // Parse 'source'
        switch (tag) {
            // Tags that don't have "originated" byte only support tz accounts, not KT or tz.
            case OPERATION_TAG_PROPOSAL:
            case OPERATION_TAG_BALLOT:
            case OPERATION_TAG_BABYLON_DELEGATION:
            case OPERATION_TAG_BABYLON_ORIGINATION:
            case OPERATION_TAG_BABYLON_REVEAL:
            case OPERATION_TAG_BABYLON_TRANSACTION: {
                struct implicit_contract const *const implicit_source = NEXT_TYPE(struct implicit_contract);
                out->operation.source.originated = 0;
                out->operation.source.signature_type = parse_raw_tezos_header_signature_type(&implicit_source->signature_type);
                memcpy(out->operation.source.hash, implicit_source->pkh, sizeof(out->operation.source.hash));
                break;
            }

            case OPERATION_TAG_ATHENS_DELEGATION:
            case OPERATION_TAG_ATHENS_ORIGINATION:
            case OPERATION_TAG_ATHENS_REVEAL:
            case OPERATION_TAG_ATHENS_TRANSACTION: {
                struct contract const *const source = NEXT_TYPE(struct contract);
                parse_contract(&out->operation.source, source);
                break;
            }

            default: PARSE_ERROR();
        }

        // out->operation.source IS NORMALIZED AT THIS POINT

        // Parse common fields for non-governance related operations.
        if (tag != OPERATION_TAG_PROPOSAL && tag != OPERATION_TAG_BALLOT) {
            out->total_fee += PARSE_Z(data, &ix, length); // fee
            PARSE_Z(data, &ix, length); // counter
            PARSE_Z(data, &ix, length); // gas limit
            out->total_storage_limit += PARSE_Z(data, &ix, length); // storage limit
        }

        if (tag == OPERATION_TAG_ATHENS_REVEAL || tag == OPERATION_TAG_BABYLON_REVEAL) {
            // Public key up next! Ensure it matches signing key.
            // Ignore source :-) and do not parse it from hdr.
            // We don't much care about reveals, they have very little in the way of bad security
            // implications and any fees have already been accounted for
            raw_tezos_header_signature_type_t const *const sig_type = NEXT_TYPE(raw_tezos_header_signature_type_t);
            if (parse_raw_tezos_header_signature_type(sig_type) != out->signing.signature_type) PARSE_ERROR();

            size_t klen = out->public_key.W_len;
            advance_ix(&ix, length, klen);
            if (memcmp(out->public_key.W, data + ix - klen, klen) != 0) PARSE_ERROR();

            out->has_reveal = true;
            continue;
        }

        if (out->operation.tag != OPERATION_TAG_NONE) {
            // We are only currently allowing one non-reveal operation
            PARSE_ERROR();
        }

        // This is the one allowable non-reveal operation per set

        out->operation.tag = (uint8_t)tag;

        // If the source is an implicit contract,...
        if (out->operation.source.originated == 0) {
            // ... it had better match our key, otherwise why are we signing it?
            if (COMPARE(&out->operation.source, &out->signing) != 0) PARSE_ERROR();
        }
        // OK, it passes muster.

        // This should by default be blanked out
        out->operation.delegate.signature_type = SIGNATURE_TYPE_UNSET;
        out->operation.delegate.originated = 0;

        switch (tag) {
            case OPERATION_TAG_PROPOSAL:
                {
                    const struct proposal_contents *proposal_data = NEXT_TYPE(struct proposal_contents);
                    if (ix != length) PARSE_ERROR();

                    const size_t payload_size = READ_UNALIGNED_BIG_ENDIAN(int32_t, &proposal_data->num_bytes);
                    if (payload_size != PROTOCOL_HASH_SIZE) PARSE_ERROR(); // We only accept exactly 1 proposal hash.

                    out->operation.proposal.voting_period = READ_UNALIGNED_BIG_ENDIAN(int32_t, &proposal_data->period);
                    memcpy(out->operation.proposal.protocol_hash, proposal_data->hash, sizeof(out->operation.proposal.protocol_hash));
                }
                break;
            case OPERATION_TAG_BALLOT:
                {
                    const struct ballot_contents *ballot_data = NEXT_TYPE(struct ballot_contents);
                    if (ix != length) PARSE_ERROR();

                    out->operation.ballot.voting_period = READ_UNALIGNED_BIG_ENDIAN(int32_t, &ballot_data->period);
                    memcpy(out->operation.ballot.protocol_hash, ballot_data->proposal, sizeof(out->operation.ballot.protocol_hash));

                    const int8_t ballot_vote = READ_UNALIGNED_BIG_ENDIAN(int8_t, &ballot_data->ballot);
                    switch (ballot_vote) {
                        case 0:
                            out->operation.ballot.vote = BALLOT_VOTE_YEA;
                            break;
                        case 1:
                            out->operation.ballot.vote = BALLOT_VOTE_NAY;
                            break;
                        case 2:
                            out->operation.ballot.vote = BALLOT_VOTE_PASS;
                            break;
                        default:
                            PARSE_ERROR();
                    }
                }
                break;
            case OPERATION_TAG_ATHENS_DELEGATION:
            case OPERATION_TAG_BABYLON_DELEGATION:
                {
                    uint8_t delegate_present = NEXT_BYTE(data, &ix, length);
                    if (delegate_present) {
                        const struct delegation_contents *dlg = NEXT_TYPE(struct delegation_contents);
                        parse_implicit(&out->operation.destination, &dlg->signature_type, dlg->hash);
                    } else {
                        // Encode "not present"
                        out->operation.destination.originated = 0;
                        out->operation.destination.signature_type = SIGNATURE_TYPE_UNSET;
                    }
                }
                break;
            case OPERATION_TAG_ATHENS_ORIGINATION:
            case OPERATION_TAG_BABYLON_ORIGINATION:
                {
                    struct origination_header {
                        raw_tezos_header_signature_type_t signature_type;
                        uint8_t hash[HASH_SIZE];
                    } __attribute__((packed));
                    struct origination_header const *const hdr = NEXT_TYPE(struct origination_header);

                    parse_implicit(&out->operation.destination, &hdr->signature_type, hdr->hash);
                    out->operation.amount = PARSE_Z(data, &ix, length);
                    if (NEXT_BYTE(data, &ix, length) != 0) {
                        out->operation.flags |= ORIGINATION_FLAG_SPENDABLE;
                    }
                    if (NEXT_BYTE(data, &ix, length) != 0) {
                        out->operation.flags |= ORIGINATION_FLAG_DELEGATABLE;
                    }
                    if (NEXT_BYTE(data, &ix, length) != 0) {
                        // Has delegate
                        const struct delegation_contents *dlg = NEXT_TYPE(struct delegation_contents);
                        parse_implicit(&out->operation.delegate, &dlg->signature_type, dlg->hash);
                    }
                    if (NEXT_BYTE(data, &ix, length) != 0) PARSE_ERROR(); // Has script
                }
                break;
            case OPERATION_TAG_ATHENS_TRANSACTION:
            case OPERATION_TAG_BABYLON_TRANSACTION:
                {
                    out->operation.amount = PARSE_Z(data, &ix, length);

                    const struct contract *destination = NEXT_TYPE(struct contract);
                    parse_contract(&out->operation.destination, destination);

                    switch (NEXT_BYTE(data, &ix, length)) {
                        // We cannot parse all parameters, but a subset of
                        // manager.tz operations is accepted.
                        case MICHELSON_PARAMS_SOME: {
                            // Destination is now source
                            out->operation.is_manager_tz_operation = true;
                            memcpy(&out->operation.implicit_account, &out->operation.source, sizeof(parsed_contract_t));
                            memcpy(&out->operation.source, &out->operation.destination, sizeof(parsed_contract_t));

                            // Operations cannot actually transfer any amount.
                            if (out->operation.amount > 0) {
                                PARSE_ERROR();
                            }

                            const enum entrypoint_tag entrypoint = NEXT_BYTE(data, &ix, length);

                            // Named entrypoints have up to 31 bytes.
                            if (entrypoint == ENTRYPOINT_NAMED) {
                                const uint8_t entrypoint_length = NEXT_BYTE(data, &ix, length);
                                if (entrypoint_length > MAX_ENTRYPOINT_LENGTH) {
                                    PARSE_ERROR();
                                }
                                for (int n = 0; n < entrypoint_length; n++) {
                                    NEXT_BYTE(data, &ix, length);
                                }
                            }

                            // Anything that’s not “do” is not a
                            // manager.tz contract.
                            if (entrypoint != ENTRYPOINT_DO) {
                                PARSE_ERROR();
                            }

                            const uint32_t argument_length = MICHELSON_READ_LENGTH(data, &ix, length);

                            // Error on anything but a michelson
                            // sequence.
                            if (NEXT_BYTE(data, &ix, length) != MICHELSON_TYPE_SEQUENCE) {
                                PARSE_ERROR();
                            }

                            const uint32_t sequence_length = MICHELSON_READ_LENGTH(data, &ix, length);

                            // Only allow single sequence (5 is needed
                            // in argument length for above two
                            // bytes). Also bail out on really big
                            // Michellson that we don’t support.
                            if (   sequence_length + 5 != argument_length
                                || argument_length > MAX_MICHELSON_SEQUENCE_LENGTH) {
                                PARSE_ERROR();
                            }

                            // All manager.tz operations should begin
                            // with this. Otherwise, bail out.
                            if (   MICHELSON_READ_SHORT(data, &ix, length) != MICHELSON_DROP
                                || MICHELSON_READ_SHORT(data, &ix, length) != MICHELSON_NIL
                                || MICHELSON_READ_SHORT(data, &ix, length) != MICHELSON_OPERATION) {
                                PARSE_ERROR();
                            }

                            // First real michelson op.
                            switch (MICHELSON_READ_SHORT(data, &ix, length)) {
                                case MICHELSON_PUSH: {
                                    switch (MICHELSON_READ_SHORT(data, &ix, length)) {
                                        case MICHELSON_KEY_HASH: {
                                            michelson_read_address(&out->operation.destination, data, &ix, length);

                                            switch (MICHELSON_READ_SHORT(data, &ix, length)) {
                                                case MICHELSON_SOME: { // Set delegate
                                                    // Matching: PUSH key_hash <dlgt> ; SOME ; SET_DELEGATE
                                                    if (MICHELSON_READ_SHORT(data, &ix, length) != MICHELSON_SET_DELEGATE) {
                                                        PARSE_ERROR();
                                                    }
                                                    out->operation.tag = OPERATION_TAG_BABYLON_DELEGATION;
                                                    out->operation.destination.originated = true;
                                                    break;
                                                }
                                                case MICHELSON_IMPLICIT_ACCOUNT: { // transfer contract to implicit
                                                    // Matching: PUSH key_hash <adr> ; IMPLICIT_ACCOUNT ; PUSH mutez <val> ; UNIT ; TRANSFER_TOKENS
                                                    if (   MICHELSON_READ_SHORT(data, &ix, length) != MICHELSON_PUSH
                                                        || MICHELSON_READ_SHORT(data, &ix, length) != MICHELSON_MUTEZ) {
                                                        PARSE_ERROR();
                                                    }

                                                    if (NEXT_BYTE(data, &ix, length) != 0) {
                                                        PARSE_ERROR();
                                                    };

                                                    out->operation.amount = PARSE_Z_MICHELSON(data, &ix, length);

                                                    if (   MICHELSON_READ_SHORT(data, &ix, length) != MICHELSON_UNIT
                                                        || MICHELSON_READ_SHORT(data, &ix, length) != MICHELSON_TRANSFER_TOKENS) {
                                                        PARSE_ERROR();
                                                    }
                                                    out->operation.tag = OPERATION_TAG_BABYLON_TRANSACTION;
                                                    break;
                                                }
                                                default: PARSE_ERROR();
                                            }
                                            break;
                                        }
                                        case MICHELSON_ADDRESS: { // transfer contract to contract
                                            // Matching: PUSH address <adr> ; CONTRACT <par> ; ASSERT_SOME ; PUSH mutez <val> ; UNIT ; TRANSFER_TOKENS
                                            michelson_read_address(&out->operation.destination, data, &ix, length);
                                            const enum michelson_code contract_code = MICHELSON_READ_SHORT(data, &ix, length);
                                            const enum michelson_code type = MICHELSON_READ_SHORT(data, &ix, length);
                                            switch (contract_code) {
                                                case MICHELSON_CONTRACT_WITH_ENTRYPOINT: {
                                                    // No way to display
                                                    // entrypoint now, so need to
                                                    // bail out on anything but
                                                    // default.
                                                    // TODO: display entrypoints
                                                    if (NEXT_BYTE(data, &ix, length) != ENTRYPOINT_DEFAULT) {
                                                        PARSE_ERROR();
                                                    }

                                                    break;
                                                }
                                                case MICHELSON_CONTRACT: {
                                                    break;
                                                }
                                                default: PARSE_ERROR();
                                            }

                                            // Can’t display any parameters, need
                                            // to throw anything but unit out for now.
                                            // TODO: display michelson arguments
                                            if (type != MICHELSON_CONTRACT_UNIT) {
                                                PARSE_ERROR();
                                            }

                                            // Matching: ASSERT_SOME (unfolded)
                                            if (NEXT_BYTE(data, &ix, length) != MICHELSON_TYPE_SEQUENCE) {
                                                PARSE_ERROR();
                                            }
                                            if (MICHELSON_READ_LENGTH(data, &ix, length) != 0x15) {
                                                PARSE_ERROR();
                                            }
                                            if (MICHELSON_READ_SHORT(data, &ix, length) != MICHELSON_IF_NONE) {
                                                PARSE_ERROR();
                                            }
                                            if (NEXT_BYTE(data, &ix, length) != MICHELSON_TYPE_SEQUENCE) {
                                                PARSE_ERROR();
                                            }
                                            if (MICHELSON_READ_LENGTH(data, &ix, length) != 9) {
                                                PARSE_ERROR();
                                            }
                                            if (NEXT_BYTE(data, &ix, length) != MICHELSON_TYPE_SEQUENCE) {
                                                PARSE_ERROR();
                                            }
                                            if (MICHELSON_READ_LENGTH(data, &ix, length) != 4) {
                                                PARSE_ERROR();
                                            }
                                            if (MICHELSON_READ_SHORT(data, &ix, length) != MICHELSON_UNIT) {
                                                PARSE_ERROR();
                                            }
                                            if (MICHELSON_READ_SHORT(data, &ix, length) != MICHELSON_FAILWITH) {
                                                PARSE_ERROR();
                                            }
                                            if (NEXT_BYTE(data, &ix, length) != MICHELSON_TYPE_SEQUENCE) {
                                                PARSE_ERROR();
                                            }
                                            if (MICHELSON_READ_LENGTH(data, &ix, length) != 0) {
                                                PARSE_ERROR();
                                            }

                                            if (   MICHELSON_READ_SHORT(data, &ix, length) != MICHELSON_PUSH
                                                || MICHELSON_READ_SHORT(data, &ix, length) != MICHELSON_MUTEZ) {
                                                PARSE_ERROR();
                                            }

                                            if (NEXT_BYTE(data, &ix, length) != 0) {
                                                PARSE_ERROR();
                                            };

                                            out->operation.amount = PARSE_Z_MICHELSON(data, &ix, length);

                                            if (   MICHELSON_READ_SHORT(data, &ix, length) != MICHELSON_UNIT
                                                || MICHELSON_READ_SHORT(data, &ix, length) != MICHELSON_TRANSFER_TOKENS) {
                                                PARSE_ERROR();
                                            }
                                            out->operation.tag = OPERATION_TAG_BABYLON_TRANSACTION;
                                            break;
                                        }
                                        default: PARSE_ERROR();
                                    }
                                    break;
                                }
                                case MICHELSON_NONE: { // withdraw delegate
                                    // Matching: NONE key_hash ; SET_DELEGATE
                                    if (   MICHELSON_READ_SHORT(data, &ix, length) != MICHELSON_KEY_HASH
                                        || MICHELSON_READ_SHORT(data, &ix, length) != MICHELSON_SET_DELEGATE) {
                                        PARSE_ERROR();
                                    }
                                    out->operation.tag = OPERATION_TAG_BABYLON_DELEGATION;
                                    out->operation.destination.originated = 0;
                                    out->operation.destination.signature_type = SIGNATURE_TYPE_UNSET;
                                    break;
                                }
                                default: PARSE_ERROR();
                            }

                            // All michelson contracts end with a cons.
                            if (MICHELSON_READ_SHORT(data, &ix, length) != MICHELSON_CONS) {
                                PARSE_ERROR();
                            }

                            // This should be the exact end of the
                            // APDU. Any more data can’t be parsed.
                            if (ix != length) {
                                PARSE_ERROR();
                            }

                            break;
                        }
                        case MICHELSON_PARAMS_NONE: {
                            break;
                        }
                        default: PARSE_ERROR();
                    }
                }
                break;
            default:
                PARSE_ERROR();
        }
    }

    if (out->operation.tag == OPERATION_TAG_NONE && !out->has_reveal) {
        PARSE_ERROR(); // Must have at least one op
    }
}

bool parse_operations(
    struct parsed_operation_group *const out,
    uint8_t const *const data,
    size_t length,
    derivation_type_t derivation_type,
    bip32_path_t const *const bip32_path,
    is_operation_allowed_t is_operation_allowed
) {
    BEGIN_TRY {
        TRY {
            parse_operations_throws_parse_error(out, data, length, derivation_type, bip32_path, is_operation_allowed);
        }
        CATCH(EXC_PARSE_ERROR) {
            return false;
        }
        CATCH_OTHER(e) {
            THROW(e);
        }
        FINALLY { }
    }
    END_TRY;
    return true;
}

#include "operations.h"

#include "apdu.h"
#include "globals.h"
#include "memory.h"
#include "to_string.h"
#include "ui.h"

#include <stdint.h>
#include <string.h>

struct operation_group_header {
    uint8_t magic_byte;
    uint8_t hash[32];
} __attribute__((packed));

struct implicit_contract {
    uint8_t curve_code;
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
    uint8_t curve_code;
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

static void advance_ix(size_t *ix, size_t length, size_t amount) {
    if (*ix + amount > length) PARSE_ERROR();

    *ix += amount;
}

static uint8_t next_byte(const void *data, size_t *ix, size_t length, uint32_t lineno) {
    if (*ix == length) parse_error(lineno);
    uint8_t res = ((const char *)data)[*ix];
    (*ix)++;
    return res;
}

#define NEXT_BYTE(data, ix, length) next_byte(data, ix, length, __LINE__)

static inline uint64_t parse_z(const void *data, size_t *ix, size_t length, uint32_t lineno) {
    uint64_t acc = 0;
    uint64_t shift = 0;
    while (true) {
        uint64_t byte = next_byte(data, ix, length, lineno);
        acc |= (byte & 0x7F) << shift;
        shift += 7;
        if (!(byte & 0x80)) {
            break;
        }
    }
    return acc;
}

#define PARSE_Z(data, ix, length) parse_z(data, ix, length, __LINE__)

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

static inline void compute_pkh(cx_curve_t curve, bip32_path_t const *const bip32_path,
                        struct parsed_operation_group *const out) {
    check_null(bip32_path);
    check_null(out);
    cx_ecfp_public_key_t const *const pubkey = generate_public_key(curve, bip32_path);
    cx_ecfp_public_key_t const *const key = public_key_hash(out->signing.hash, curve, pubkey);
    memcpy(&out->public_key, key, sizeof(out->public_key));
    out->signing.curve_code = curve_to_curve_code(curve);
    out->signing.originated = 0;
}

static inline void parse_implicit(struct parsed_contract *out, uint8_t curve_code,
                           const uint8_t hash[HASH_SIZE]) {
    out->originated = 0;
    out->curve_code = curve_code;
    memcpy(out->hash, hash, sizeof(out->hash));
}

static inline void parse_contract(struct parsed_contract *out, const struct contract *in) {
    out->originated = in->originated;
    if (out->originated == 0) { // implicit
        out->curve_code = in->u.implicit.curve_code;
        memcpy(out->hash, in->u.implicit.pkh, sizeof(out->hash));
    } else { // originated
        out->curve_code = TEZOS_NO_CURVE;
        memcpy(out->hash, in->u.originated.pkh, sizeof(out->hash));
    }
}

struct parsed_operation_group *parse_operations(const void *data, size_t length, cx_curve_t curve,
                                                bip32_path_t const *const bip32_path,
                                                allowed_operation_set ops) {
    check_null(data);
    check_null(bip32_path);

    struct parsed_operation_group *const out = &global.priv.parse_operations.out;
    memset(out, 0, sizeof(*out));

    out->operation.tag = OPERATION_TAG_NONE;

    compute_pkh(curve, bip32_path, out); // sets up "signing" and "public_key" members

    size_t ix = 0;

    // Verify magic byte, ignore block hash
    const struct operation_group_header *ogh = NEXT_TYPE(struct operation_group_header);
    if (ogh->magic_byte != MAGIC_BYTE_UNSAFE_OP) PARSE_ERROR();

    // Start out with source = signing, for reveals
    // TODO: This is slightly hackish
    memcpy(&out->operation.source, &out->signing, sizeof(out->signing));

    while (ix < length) {
        const enum operation_tag tag = NEXT_BYTE(data, &ix, length);  // 1 byte is always aligned

        if (!is_operation_allowed(ops, tag)) PARSE_ERROR();

        if (tag == OPERATION_TAG_PROPOSAL || tag == OPERATION_TAG_BALLOT) {
            // These tags don't have the "originated" byte so we have to parse PKH differently.
            const struct implicit_contract *implicit_source = NEXT_TYPE(struct implicit_contract);
            out->operation.source.originated = 0;
            out->operation.source.curve_code = READ_UNALIGNED_BIG_ENDIAN(uint8_t, &implicit_source->curve_code);
            memcpy(out->operation.source.hash, implicit_source->pkh, sizeof(out->operation.source.hash));
        } else {
            const struct contract *source = NEXT_TYPE(struct contract);
            parse_contract(&out->operation.source, source);

            out->total_fee += PARSE_Z(data, &ix, length); // fee
            PARSE_Z(data, &ix, length); // counter
            PARSE_Z(data, &ix, length); // gas limit
            out->total_storage_limit += PARSE_Z(data, &ix, length); // storage limit
        }

        // out->operation.source IS NORMALIZED AT THIS POINT

        if (tag == OPERATION_TAG_REVEAL) {
            // Public key up next! Ensure it matches signing key.
            // Ignore source :-) and do not parse it from hdr.
            // We don't much care about reveals, they have very little in the way of bad security
            // implications and any fees have already been accounted for
            if (NEXT_BYTE(data, &ix, length) != out->signing.curve_code) PARSE_ERROR();

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
            if (COMPARE(&out->operation.source, &out->signing)) return false;
        }
        // OK, it passes muster.

        // This should by default be blanked out
        out->operation.delegate.curve_code = TEZOS_NO_CURVE;
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
            case OPERATION_TAG_DELEGATION:
                {
                    uint8_t delegate_present = NEXT_BYTE(data, &ix, length);
                    if (delegate_present) {
                        const struct delegation_contents *dlg = NEXT_TYPE(struct delegation_contents);
                        parse_implicit(&out->operation.destination, dlg->curve_code, dlg->hash);
                    } else {
                        // Encode "not present"
                        out->operation.destination.originated = 0;
                        out->operation.destination.curve_code = TEZOS_NO_CURVE;
                    }
                }
                break;
            case OPERATION_TAG_ORIGINATION:
                {
                    struct origination_header {
                        uint8_t curve_code;
                        uint8_t hash[HASH_SIZE];
                    } __attribute__((packed));
                    const struct origination_header *hdr = NEXT_TYPE(struct origination_header);

                    parse_implicit(&out->operation.destination, hdr->curve_code, hdr->hash);
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
                        parse_implicit(&out->operation.delegate, dlg->curve_code, dlg->hash);
                    }
                    if (NEXT_BYTE(data, &ix, length) != 0) PARSE_ERROR(); // Has script
                }
                break;
            case OPERATION_TAG_TRANSACTION:
                {
                    out->operation.amount = PARSE_Z(data, &ix, length);

                    const struct contract *destination = NEXT_TYPE(struct contract);
                    parse_contract(&out->operation.destination, destination);

                    uint8_t params = NEXT_BYTE(data, &ix, length);
                    if (params) PARSE_ERROR(); // TODO: Support params
                }
                break;
            default:
                PARSE_ERROR();
        }
    }

    if (out->operation.tag == OPERATION_TAG_NONE && !out->has_reveal) {
        PARSE_ERROR(); // Must have at least one op
    }

    return out; // Success!
}

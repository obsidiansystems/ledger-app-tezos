#include "protocol.h"

#include "apdu.h"
#include "ui.h"
#include "to_string.h"

#include <stdint.h>
#include <string.h>

#include "os.h"

struct __attribute__((packed)) block {
    char magic_byte;
    uint32_t chain_id;
    level_t level;
    uint8_t proto;
    // ... beyond this we don't care
};

struct __attribute__((packed)) endorsement {
    uint8_t magic_byte;
    uint32_t chain_id;
    uint8_t branch[32];
    uint8_t tag;
    uint32_t level;
};

bool parse_baking_data(const void *data, size_t length, struct parsed_baking_data *out) {
    switch (get_magic_byte(data, length)) {
        case MAGIC_BYTE_BAKING_OP:
            if (length != sizeof(struct endorsement)) return false;
            const struct endorsement *endorsement = data;
            // TODO: Check chain ID
            out->is_endorsement = true;
            out->level = READ_UNALIGNED_BIG_ENDIAN(level_t, &endorsement->level);
            return true;
        case MAGIC_BYTE_BLOCK:
            if (length < sizeof(struct block)) return false;
            // TODO: Check chain ID
            out->is_endorsement = false;
            const struct block *block = data;
            out->level = READ_UNALIGNED_BIG_ENDIAN(level_t, &block->level);
            return true;
        case MAGIC_BYTE_INVALID:
        default:
            return false;
    }
}

struct operation_group_header {
    uint8_t magic_byte;
    uint8_t hash[32];
} __attribute__((packed));

struct contract {
    uint8_t originated;
    union {
        struct {
            uint8_t curve_code;
            uint8_t pkh[HASH_SIZE];
        } implicit;
        struct {
            uint8_t pkh[HASH_SIZE];
            uint8_t padding;
        } originated;
    } u;
} __attribute__((packed));

struct operation_header {
    uint8_t tag;
    struct contract contract;
} __attribute__((packed));

struct delegation_contents {
    uint8_t delegate_present;
    uint8_t curve_code;
    uint8_t hash[HASH_SIZE];
} __attribute__((packed));

__attribute__((noreturn))

// Argument is to distinguish between different parse errors for debugging purposes only
static void parse_error(
#ifndef DEBUG
    __attribute__((unused))
#endif
    uint32_t code) {
#ifdef DEBUG
    THROW(0x9000 + code);
#else
    THROW(EXC_PARSE_ERROR);
#endif
}

static void advance_ix(size_t *ix, size_t length, size_t amount) {
    if (*ix + amount > length) parse_error(length);

    *ix += amount;
}

static uint8_t next_byte(const void *data, size_t *ix, size_t length) {
    if (*ix == length) parse_error(1);
    uint8_t res = ((const char *)data)[*ix];
    (*ix)++;
    return res;
}

static uint64_t parse_z(const void *data, size_t *ix, size_t length) {
    uint64_t acc = 0;
    uint64_t shift = 0;
    while (true) {
        uint8_t byte = next_byte(data, ix, length);
        acc |= (byte & 0x7F) << shift;
        shift += 7;
        if (!(byte & 0x80)) {
            break;
        }
    }
    return acc;
}

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

static void compute_pkh(cx_curve_t curve, size_t path_length, uint32_t *bip32_path,
                        struct parsed_operation_group *out) {
    cx_ecfp_public_key_t public_key_init;
    cx_ecfp_private_key_t private_key;
    generate_key_pair(curve, path_length, bip32_path, &public_key_init, &private_key);
    os_memset(&private_key, 0, sizeof(private_key));

    public_key_hash(out->signing.hash, curve, &public_key_init, &out->public_key);
    out->signing.curve_code = curve_to_curve_code(curve);
    out->signing.originated = 0;
}

static void parse_implicit(struct parsed_contract *out, uint8_t curve_code,
                           const uint8_t hash[HASH_SIZE]) {
    out->originated = 0;
    out->curve_code = curve_code;
    memcpy(out->hash, hash, sizeof(out->hash));
}

static void parse_contract(struct parsed_contract *out, const struct contract *in) {
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
                                                size_t path_length, uint32_t *bip32_path,
                                                allowed_operation_set ops) {
    static struct parsed_operation_group out;
    check_null(data);
    check_null(bip32_path);
    memset(&out, 0, sizeof(out));

    out.operation.tag = OPERATION_TAG_NONE;

    compute_pkh(curve, path_length, bip32_path, &out); // sets up "signing" and "public_key" members

    size_t ix = 0;

    // Verify magic byte, ignore block hash
    const struct operation_group_header *ogh = NEXT_TYPE(struct operation_group_header);
    if (ogh->magic_byte != MAGIC_BYTE_UNSAFE_OP) parse_error(15);

    while (ix < length) {
        const struct operation_header *hdr = NEXT_TYPE(struct operation_header);

        out.total_fee += parse_z(data, &ix, length); // fee
        parse_z(data, &ix, length); // counter
        parse_z(data, &ix, length); // gas limit
        parse_z(data, &ix, length); // storage limit

        enum operation_tag tag = hdr->tag;
        if (!is_operation_allowed(ops, tag)) parse_error(44);

        if (tag == OPERATION_TAG_REVEAL) {
            // Public key up next! Ensure it matches signing key.
            // Ignore source :-) and do not parse it from hdr
            // We don't much care about reveals, they have very little in the way of bad security
            // implementations and any fees have already been accounted for
            if (next_byte(data, &ix, length) != out.signing.curve_code) parse_error(64);

            size_t klen = out.public_key.W_len;
            advance_ix(&ix, length, klen);
            if (memcmp(out.public_key.W, data + ix - klen, klen) != 0) parse_error(4);

            continue;
        }

        if (out.operation.tag != OPERATION_TAG_NONE) {
            // We are only currently allowing one non-reveal operation
            parse_error(90);
        }

        // This is the one allowable non-reveal operation.

        out.operation.tag = (uint8_t)tag;
        parse_contract(&out.operation.source, &hdr->contract);

        switch (tag) {
            case OPERATION_TAG_DELEGATION:
                {
                    const struct delegation_contents *dlg = NEXT_TYPE(struct delegation_contents);
                    parse_implicit(&out.operation.destination, dlg->curve_code, dlg->hash);
                }
                break;
            case OPERATION_TAG_TRANSACTION:
                {
                    out.operation.amount = parse_z(data, &ix, length);

                    const struct contract *destination = NEXT_TYPE(struct contract);
                    parse_contract(&out.operation.destination, destination);

                    uint8_t params = next_byte(data, &ix, length);
                    if (params) parse_error(101); // TODO: Support params
                }
                break;
            default:
                parse_error(8);
        }
    }

    if (out.operation.tag == OPERATION_TAG_NONE) parse_error(13); // Must have one non-reveal op

    return &out; // Success!
}

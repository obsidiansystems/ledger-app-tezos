#include "protocol.h"

#include "apdu.h"
#include "ui.h"
#include "to_string.h"

#include <stdint.h>
#include <string.h>

#include "os.h"

struct __attribute__((__packed__)) block {
    char magic_byte;
    uint32_t chain_id;
    level_t level;
    uint8_t proto;
    // ... beyond this we don't care
};

bool is_block_valid(const void *data, size_t length) {
    if (length < sizeof(struct block)) return false;
    if (get_magic_byte(data, length) != MAGIC_BYTE_BLOCK) return false;
    // TODO: Check chain id
    return true;
}

level_t get_block_level(const void *data, __attribute__((unused)) size_t length) {
    check_null(data);
    const struct block *blk = data;
    return READ_UNALIGNED_BIG_ENDIAN(level_t, &blk->level);
}

struct operation_group_header {
    uint8_t magic_byte;
    uint8_t hash[32];
} __attribute__((packed));

struct contract {
    uint8_t outright;
    uint8_t curve_code;
    uint8_t pkh[HASH_SIZE];
} __attribute__((packed));

enum operation_tag {
    OPERATION_TAG_REVEAL = 7,
    OPERATION_TAG_TRANSACTION = 8,
    OPERATION_TAG_DELEGATION = 10,
};

struct operation_header {
    uint8_t tag;
    struct contract contract;
} __attribute__((packed));

struct delegation_contents {
    uint8_t delegate_present;
    uint8_t curve_code;
    uint8_t hash[HASH_SIZE];
} __attribute__((packed));

// These macros assume:
// * Beginning of data: const void *data
// * Total length of data: size_t length
// * Current index of data: size_t ix
// Any function that uses these macros should have these as local variables
#define NEXT_TYPE(type) ({ \
    if (ix + sizeof(type) > length) return sizeof(type); \
    const type *val = data + ix; \
    ix += sizeof(type); \
    val; \
})

#define NEXT_BYTE() (*NEXT_TYPE(uint8_t))

#define PARSE_Z() ({ \
    uint64_t acc = 0; \
    uint64_t shift = 0; \
    while (true) { \
        if (ix >= length) return 23; \
        uint8_t next_byte = NEXT_BYTE(); \
        acc |= (next_byte & 0x7F) << shift; \
        shift += 7; \
        if (!(next_byte & 0x80)) { \
            break; \
        } \
    } \
    acc; \
})

static void compute_pkh(cx_curve_t curve, size_t path_length, uint32_t *bip32_path,
                        struct parsed_operation_data *out) {
    cx_ecfp_public_key_t public_key_init;
    cx_ecfp_private_key_t private_key;
    generate_key_pair(curve, path_length, bip32_path, &public_key_init, &private_key);
    os_memset(&private_key, 0, sizeof(private_key));

    public_key_hash(out->hash, curve, &public_key_init, &out->public_key);

    out->curve_code = curve_to_curve_code(curve);
}

uint32_t parse_operations(const void *data, size_t length, cx_curve_t curve,
                          size_t path_length, uint32_t *bip32_path, struct parsed_operation_data *out) {
    check_null(data);
    check_null(bip32_path);
    check_null(out);
    memset(out, 0, sizeof(*out));

    compute_pkh(curve, path_length, bip32_path, out);

    size_t ix = 0;

    // Verify magic byte, ignore block hash
    const struct operation_group_header *ogh = NEXT_TYPE(struct operation_group_header);
    if (ogh->magic_byte != MAGIC_BYTE_UNSAFE_OP) return 15;

    while (ix < length) {
        const struct operation_header *hdr = NEXT_TYPE(struct operation_header);
        if (hdr->contract.curve_code != out->curve_code) return 13;
        if (hdr->contract.outright != 0) return 12;
        if (memcmp(hdr->contract.pkh, out->hash, HASH_SIZE) != 0) return 14;

        out->total_fee = PARSE_Z(); // fee
        PARSE_Z(); // counter
        PARSE_Z(); // gas limit
        PARSE_Z(); // storage limit
        switch (hdr->tag) {
            case OPERATION_TAG_REVEAL:
                // Public key up next!
                if (NEXT_BYTE() != out->curve_code) return 64;

                size_t klen = out->public_key.W_len;
                if (ix + klen > length) return klen;
                if (memcmp(out->public_key.W, data + ix, klen) != 0) return 4;
                ix += klen;
                break;
            case OPERATION_TAG_DELEGATION:
                {
                    // Delegation up next!
                    const struct delegation_contents *dlg = NEXT_TYPE(struct delegation_contents);
                    if (dlg->curve_code != out->curve_code) return 5;
                    if (dlg->delegate_present != 0xFF) return 6;
                    if (memcmp(dlg->hash, out->hash, HASH_SIZE) != 0) return 7;
                    out->contains_self_delegation = true;
                }
                break;
            case OPERATION_TAG_TRANSACTION:
                out->transaction_count++;
                out->amount += PARSE_Z();
                const struct contract *destination = NEXT_TYPE(struct contract);
                if (destination->outright != 0) return 102; // TODO: Support outright dests
                out->destination_curve_code = destination->curve_code;
                memcpy(out->destination_hash, destination->pkh, sizeof(out->destination_hash));
                uint8_t params = NEXT_BYTE();
                if (params) return 101; // TODO: Support params
                break;
            default:
                return 8;
        }
    }

    return 0; // Success
}

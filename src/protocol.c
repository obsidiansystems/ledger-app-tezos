#include "protocol.h"

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

#define SUPPORTED_PROTO_VERSION 1

bool is_block_valid(const void *data, size_t length) {
    if (length < sizeof(struct block)) return false;
    if (get_magic_byte(data, length) != MAGIC_BYTE_BLOCK) return false;
    const struct block *blk = data;
    if (blk->proto != SUPPORTED_PROTO_VERSION) return false;
    return true;
}

int32_t read_unaligned_big_endian(const void *in) {
    const uint8_t *bytes = in;
    uint8_t out_bytes[4];
    int32_t res;

    out_bytes[0] = bytes[3];
    out_bytes[1] = bytes[2];
    out_bytes[2] = bytes[1];
    out_bytes[3] = bytes[0];
    memcpy(&res, out_bytes, 4);

    return res;
}

level_t get_block_level(const void *data, size_t length) {
    const struct block *blk = data;
    return READ_UNALIGNED_BIG_ENDIAN(level_t, &blk->level);
}

// Error code can be made to depend on N for debugging
#define PARSE_ERROR(N) THROW(0x9600 + N)

static const void *next_bytes(const void *data, size_t length, size_t *ix, size_t data_len) {
    const uint8_t *bytes = data;
    if (*ix + data_len > length) PARSE_ERROR(16 + data_len);
    const void *res = bytes + *ix;
    *ix += data_len;
    return res;
}

static uint64_t parse_z(const uint8_t *bytes, size_t length, size_t *pos) {
    uint64_t acc = 0;
    uint32_t shift = 0;
    while (true) {
        uint8_t next_byte = *(uint8_t*)next_bytes(bytes, length, pos, 1);
        acc |= (next_byte & 0x7F) << shift;
        shift += 7;
        if (!(next_byte & 0x80)) { // last byte has no high bit set
            return acc;
        }
    }
}

void guard_valid_self_delegation(const void *data, size_t length, cx_curve_t curve,
                                 size_t path_length, uint32_t *bip32_path) {
    if (length < 33) PARSE_ERROR(0);

    cx_ecfp_public_key_t public_key_init;
    cx_ecfp_public_key_t public_key;
    cx_ecfp_private_key_t private_key;
    uint8_t hash[HASH_SIZE];

    generate_key_pair(curve, path_length, bip32_path, &public_key_init, &private_key);
    os_memset(&private_key, 0, sizeof(private_key));

    public_key_hash(hash, curve, &public_key_init, &public_key);

    uint8_t curve_code;
    switch (curve) {
        case CX_CURVE_Ed25519:
            curve_code = 0;
            break;
        case CX_CURVE_SECP256K1:
            curve_code = 1;
            break;
        case CX_CURVE_SECP256R1:
            curve_code = 2;
            break;
        default:
            PARSE_ERROR(9); // Should not be reached
    }

    size_t ix = 0;

#define NEXT_TYPE(type) (*(const type*)next_bytes(data, length, &ix, sizeof(type)))
#define NEXT_BYTE() NEXT_TYPE(uint8_t)
#define PARSE_Z() parse_z(data, length, &ix)

    // Magic number
    if (NEXT_BYTE() != MAGIC_BYTE_UNSAFE_OP) PARSE_ERROR(15);

    struct block_hash {
        uint8_t hash[32];
    };
    NEXT_TYPE(struct block_hash);

    while (ix < length) {
        const struct operation_header *hdr = &NEXT_TYPE(struct operation_header);
        if (hdr->contract.curve_code != curve_code) PARSE_ERROR(13);
        if (hdr->contract.outright != 0) PARSE_ERROR(12);
        if (memcmp(hdr->contract.pkh, hash, HASH_SIZE) != 0) PARSE_ERROR(14);

        uint64_t fee = PARSE_Z(); // fee
        PARSE_Z(); // counter
        if (PARSE_Z() != 0) PARSE_ERROR(1); // gas limit
        if (PARSE_Z() != 0) PARSE_ERROR(2); // storage limit

        switch (hdr->tag) {
            case OPERATION_TAG_REVEAL:
                if (fee != 0) PARSE_ERROR(3); // Who sets a fee for reveal?

                // Public key up next!
                if (NEXT_BYTE() != curve_code) PARSE_ERROR(-2);
                const uint8_t *pubkey = next_bytes(data, length, &ix, public_key.W_len);
                if (memcmp(public_key.W, pubkey, public_key.W_len) != 0) PARSE_ERROR(4);
                break;
            case OPERATION_TAG_DELEGATION:
                if (fee != 50000) PARSE_ERROR(-1); // You want another fee, use wallet app!

                // Delegation up next!
                const struct delegation_contents *dlg = &NEXT_TYPE(struct delegation_contents);
                if (dlg->curve_code != curve_code) PARSE_ERROR(5);
                if (dlg->delegate_present != 0xFF) PARSE_ERROR(6);
                if (memcmp(dlg->hash, hash, HASH_SIZE) != 0) PARSE_ERROR(7);
                break;
            default:
                PARSE_ERROR(8);
        }
    }
    // Success!
}

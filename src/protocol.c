#include "protocol.h"

#include <stdint.h>
#include <string.h>

#include "os.h"

struct __attribute__((__packed__)) block {
    char magic_byte;
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

static bool read_z(const uint8_t *bytes, size_t length, size_t *pos) {
    for (; *pos < length; (*pos)++) {
        if (!(bytes[*pos] & 0x80)) { // Found last byte
            (*pos)++;                // Eat it
            return true;             // Success!
        }
    }
    return false; // Bounds check failure
}

bool is_valid_self_delegation(const void *data, size_t length, cx_curve_t curve,
                              size_t path_length, uint32_t *bip32_path) {
    if (length  < 33) return false;

    cx_ecfp_public_key_t public_key;
    cx_ecfp_private_key_t private_key;
    uint8_t hash[HASH_SIZE];

    generate_key_pair(curve, path_length, bip32_path, &public_key, &private_key);
    os_memset(&private_key, 0, sizeof(private_key));

    public_key_hash(hash, curve, &public_key);

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
            return false;
    }

    const uint8_t *bytes = data;
    size_t ix = 0;
    // Magic number
    if (bytes[ix++] != MAGIC_BYTE_UNSAFE_OP) return false;

    // Block hash
    ix += 32;

    while (ix < length) {
        if (length - ix < sizeof(struct operation_header)) {
            return false;
        }
        struct operation_header *hdr = (void*)&bytes[ix];
        if (hdr->contract.curve_code != curve_code) return false;
        if (hdr->contract.outright != 0) return false;
        if (memcmp(hdr->contract.pkh, hash, HASH_SIZE) != 0) return false;

        uint64_t fee = READ_UNALIGNED_BIG_ENDIAN(uint64_t, &hdr->fee);
        ix += sizeof(struct operation_header);

        if (!read_z(bytes, length, &ix)) return false; // counter -- blargh
        if (ix + 2 > length) return false;
        if (bytes[ix++] != 0) return false; // gas limit
        if (bytes[ix++] != 0) return false; // storage limit

        switch (hdr->tag) {
            case OPERATION_TAG_REVEAL:
                if (fee != 0) return false; // Who sets a fee for reveal?

                // Public key up next!
                if (ix + 34 > length) return false;
                if (bytes[ix++] != curve_code) return false;
                if (memcmp(public_key.W, &bytes[ix], 33) != 0) return false;
                ix += 33;
                break;
            case OPERATION_TAG_DELEGATION:
                if (fee != 50000) return false; // You want another fee, use wallet app!

                // Delegation up next!
                if (ix + 22 > length) return false;
                struct delegation_contents *dlg = (void*)&bytes[ix];
                if (dlg->curve_code != curve_code) return false;
                if (dlg->delegate_present != 0xFF) return false;
                if (memcmp(dlg->hash, hash, HASH_SIZE) != 0) return false;
                break;
            default:
                return false;
        }
    }
    return true;
}

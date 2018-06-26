#include "display.h"

#include "base58.h"
#include "blake2.h"
#include "paths.h"

#include "os.h"
#include "cx.h"

#include <string.h>

int convert_address(char *buff, uint32_t buff_size, cx_curve_t curve,
                    const cx_ecfp_public_key_t *public_key) {
    // Data to encode
    struct __attribute__((packed)) {
        char prefix[3];
        uint8_t hash[HASH_SIZE];
        char checksum[4];
    } data;

    // prefix
    data.prefix[0] = 6;
    data.prefix[1] = 161;
    switch (curve) {
        case CX_CURVE_Ed25519: // Ed25519
            data.prefix[2] = 159;
            break;
        case CX_CURVE_SECP256K1: // Secp256k1
            data.prefix[2] = 161;
            break;
        case CX_CURVE_SECP256R1: // Secp256k1
            data.prefix[2] = 163;
            break;
        default:
            THROW(0x6F00); // Should not reach
    }

    // hash
    public_key_hash(data.hash, curve, public_key);

    // checksum -- twice because them's the rules
    uint8_t checksum[32];
    cx_hash_sha256(&data, sizeof(data) - sizeof(data.checksum), checksum, sizeof(checksum));
    cx_hash_sha256(checksum, sizeof(checksum), checksum, sizeof(checksum));
    memcpy(data.checksum, checksum, sizeof(data.checksum));

    b58enc(buff, &buff_size, &data, sizeof(data));
    return buff_size;
}

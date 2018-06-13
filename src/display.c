#include "display.h"

#include "base58.h"
#include "blake2.h"

#include "os.h"
#include "cx.h"

int convert_address(char *buff, uint32_t buff_size, void *raw_bytes, uint32_t size) {
    const char type = *(char*)raw_bytes;

    // Data to encode
    const int HASH_SIZE = 20;
    struct __attribute__((packed)) {
        char prefix[3];
        char hash[HASH_SIZE];
        char checksum[4];
    } data;

    // prefix
    data.prefix[0] = 6;
    data.prefix[1] = 161;
    switch (type) {
        case 0x02: // Ed25519
            data.prefix[2] = 159;
            break;
        case 0x04: // Secp256k1
            data.prefix[2] = 161;
            break;
    }

    // hash
    switch (type) {
        case 0x02: // Ed25519
            {
                // Already compressed
                blake2b(data.hash, sizeof(data.hash), raw_bytes + 1, size - 1, NULL, 0);
                break;
            }
        case 0x04: // Secp256k1
            {
                // Compress before hashing
                char *bytes = raw_bytes;
                char parity = bytes[64] & 0x01;
                bytes[0] = 0x02 + parity;
                size = 33;
                blake2b(data.hash, sizeof(data.hash), bytes, size, NULL, 0);

                // Restore original data
                bytes[0] = 0x04;
                break;
            }
    }

    // checksum -- twice because them's the rules
    uint8_t checksum[32];
    cx_hash_sha256(&data, sizeof(data) - sizeof(data.checksum), checksum, sizeof(checksum));
    cx_hash_sha256(checksum, sizeof(checksum), checksum, sizeof(checksum));
    memcpy(data.checksum, checksum, sizeof(data.checksum));

    b58enc(buff, &buff_size, &data, sizeof(data));
    return buff_size;
}

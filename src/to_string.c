#include "to_string.h"

#include "apdu.h"
#include "base58.h"
#include "keys.h"

#include <string.h>



int pubkey_to_pkh_string(char *buff, uint32_t buff_size, cx_curve_t curve,
                         const cx_ecfp_public_key_t *public_key) {
    uint8_t hash[HASH_SIZE];
    public_key_hash(hash, curve, public_key, NULL);
    return pkh_to_string(buff, buff_size, curve, hash);
}

int pkh_to_string(char *buff, uint32_t buff_size, cx_curve_t curve, uint8_t hash[HASH_SIZE]) {
    // Data to encode
    struct __attribute__((packed)) {
        char prefix[3];
        uint8_t hash[HASH_SIZE];
        char checksum[4];
    } data;

    // prefix
    switch (curve) {
        case CX_CURVE_NONE:
            data.prefix[0] = 2;
            data.prefix[1] = 90;
            data.prefix[2] = 121;
        case CX_CURVE_Ed25519: // Ed25519
            data.prefix[0] = 6;
            data.prefix[1] = 161;
            data.prefix[2] = 159;
            break;
        case CX_CURVE_SECP256K1: // Secp256k1
            data.prefix[0] = 6;
            data.prefix[1] = 161;
            data.prefix[2] = 161;
            break;
        case CX_CURVE_SECP256R1: // Secp256r1
            data.prefix[0] = 6;
            data.prefix[1] = 161;
            data.prefix[2] = 164;
            break;
        default:
            THROW(EXC_WRONG_PARAM); // Should not reach
    }

    // hash
    memcpy(data.hash, hash, sizeof(data.hash));

    // checksum -- twice because them's the rules
    uint8_t checksum[32];
    cx_hash_sha256((void*)&data, sizeof(data) - sizeof(data.checksum), checksum, sizeof(checksum));
    cx_hash_sha256(checksum, sizeof(checksum), checksum, sizeof(checksum));
    memcpy(data.checksum, checksum, sizeof(data.checksum));

    b58enc(buff, &buff_size, &data, sizeof(data));
    return buff_size;
}

// These functions do not output terminating null bytes.

#define MAX_INT_DIGITS 20

// This function fills digits, potentially with all leading zeroes, from the end of the buffer backwards
// This is intended to be used with a temporary buffer of length MAX_INT_DIGITS
// Returns offset of where it stopped filling in
static inline size_t convert_number(char dest[MAX_INT_DIGITS], uint64_t number, bool leading_zeroes) {
    char *const end = dest + MAX_INT_DIGITS;
    for (char *ptr = end - 1; ptr >= dest; ptr--) {
        *ptr = '0' + number % 10;
        number /= 10;
        if (!leading_zeroes && number == 0) { // TODO: This is ugly
            return ptr - dest;
        }
    }
    return 0;
}

size_t number_to_string(char *dest, uint64_t number) {
    char tmp[MAX_INT_DIGITS];
    size_t off = convert_number(tmp, number, false);

    // Copy without leading 0s
    size_t length = sizeof(tmp) - off;
    memcpy(dest, tmp + off, length);
    dest[length] = '\0';
    return length;
}

// Microtez are in millionths
#define TEZ_SCALE 1000000
#define DECIMAL_DIGITS 6

size_t microtez_to_string(char *dest, uint64_t number) {
    uint64_t whole_tez = number / TEZ_SCALE;
    uint64_t fractional = number % TEZ_SCALE;
    size_t off = number_to_string(dest, whole_tez);
    if (fractional == 0) {
        return off;
    }
    dest[off++] = '.';

    char tmp[MAX_INT_DIGITS];
    convert_number(tmp, number, true);

    // Eliminate trailing 0s
    char *start = tmp + MAX_INT_DIGITS - DECIMAL_DIGITS;
    char *end;
    for (end = tmp + MAX_INT_DIGITS - 1; end >= start; end--) {
        if (*end != '0') {
            end++;
            break;
        }
    }

    size_t length = end - start;
    memcpy(dest + off, start, length);
    off += length;
    dest[off] = '\0';
    return off;
}

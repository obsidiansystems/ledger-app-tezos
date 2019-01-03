#include "to_string.h"

#include "apdu.h"
#include "base58.h"
#include "keys.h"

#include <string.h>

#define NO_CONTRACT_STRING "None"

static int pkh_to_string(char *buff, const size_t buff_size, const cx_curve_t curve, const uint8_t hash[HASH_SIZE]);

int parsed_contract_to_string(char *buff, uint32_t buff_size, const struct parsed_contract *contract) {
    if (contract->originated == 0 && contract->curve_code == TEZOS_NO_CURVE) {
        if (buff_size < sizeof(NO_CONTRACT_STRING)) return 0;
        strcpy(buff, NO_CONTRACT_STRING);
        return buff_size;
    }

    cx_curve_t curve;
    if (contract->originated != 0) {
        curve = CX_CURVE_NONE;
    } else {
        curve = curve_code_to_curve(contract->curve_code);
    }
    return pkh_to_string(buff, buff_size, curve, contract->hash);
}

int pubkey_to_pkh_string(char *buff, uint32_t buff_size, cx_curve_t curve,
                         const cx_ecfp_public_key_t *public_key) {
    uint8_t hash[HASH_SIZE];
    public_key_hash(hash, curve, public_key);
    return pkh_to_string(buff, buff_size, curve, hash);
}

// TODO: this should return size_t
int pkh_to_string(char *buff, const size_t buff_size, const cx_curve_t curve, const uint8_t hash[HASH_SIZE]) {
    if (buff_size < PKH_STRING_SIZE) THROW(EXC_WRONG_LENGTH);

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
            break;
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

    size_t out_size = buff_size;
    if (!b58enc(buff, &out_size, &data, sizeof(data))) THROW(EXC_WRONG_LENGTH);
    return out_size;
}

size_t protocol_hash_to_string(char *buff, const size_t buff_size, const uint8_t hash[PROTOCOL_HASH_SIZE]) {
    if (buff_size < PROTOCOL_HASH_BASE58_STRING_SIZE) THROW(EXC_WRONG_LENGTH);

    // Data to encode
    struct __attribute__((packed)) {
        char prefix[2];
        uint8_t hash[PROTOCOL_HASH_SIZE];
        char checksum[4];
    } data = {
        .prefix = {2, 170}
    };

    memcpy(data.hash, hash, sizeof(data.hash));

    // checksum -- twice because them's the rules
    // Hash the input (prefix + hash)
    // Hash that hash.
    // Take the first 4 bytes of that
    // Voila: a checksum.
    uint8_t checksum[32];
    cx_hash_sha256((void*)&data, sizeof(data) - sizeof(data.checksum), checksum, sizeof(checksum));
    cx_hash_sha256(checksum, sizeof(checksum), checksum, sizeof(checksum));
    memcpy(data.checksum, checksum, sizeof(data.checksum));

    size_t out_size = buff_size;
    if (!b58enc(buff, &out_size, &data, sizeof(data))) THROW(EXC_WRONG_LENGTH);
    return out_size;
}

// These functions do not output terminating null bytes.

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

size_t number_to_string_indirect(char *dest, size_t buff_size, const uint64_t *number) {
    if (buff_size < MAX_INT_DIGITS + 1) return 0; // terminating null
    return number_to_string(dest, *number);
}

size_t microtez_to_string_indirect(char *dest, size_t buff_size, const uint64_t *number) {
    if (buff_size < MAX_INT_DIGITS + 1) return 0; // + terminating null + decimal point
    return microtez_to_string(dest, *number);
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

bool copy_string(char *dest, uint32_t buff_size, const char *src) {
    // I don't care that we will loop through the string twice, latency is not an issue
    if (strlen(src) >= buff_size) return false;
    strcpy(dest, src);
    return true;
}

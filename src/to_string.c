#include "to_string.h"

#include "apdu.h"
#include "base58.h"
#include "keys.h"
#include "delegates.h"

#include <string.h>

#define NO_CONTRACT_STRING "None"
#define NO_CONTRACT_NAME_STRING "Custom Delegate: please verify the address"

#define TEZOS_HASH_CHECKSUM_SIZE 4

void pkh_to_string(
    char *const buff, size_t const buff_size,
    signature_type_t const signature_type,
    uint8_t const hash[HASH_SIZE]
);

// These functions output terminating null bytes, and return the ending offset.
static size_t microtez_to_string(char *dest, uint64_t number);

void parsed_contract_to_string(
    char *const buff,
    size_t const buff_size,
    parsed_contract_t const *const contract
) {
    // If hash_ptr exists, show it to us now. Otherwise, we unpack the
    // packed hash.
    if (contract->hash_ptr != NULL) {
        if (buff_size < HASH_SIZE_B58) THROW(EXC_WRONG_LENGTH);
        memcpy(buff, contract->hash_ptr, HASH_SIZE_B58);
    } else if (contract->originated == 0 && contract->signature_type == SIGNATURE_TYPE_UNSET) {
        if (buff_size < sizeof(NO_CONTRACT_STRING)) THROW(EXC_WRONG_LENGTH);
        strcpy(buff, NO_CONTRACT_STRING);
    } else {
        signature_type_t const signature_type =
            contract->originated != 0
                ? SIGNATURE_TYPE_UNSET
                : contract->signature_type;
        pkh_to_string(buff, buff_size, signature_type, contract->hash);
    }
}

void lookup_parsed_contract_name(
    char *const buff,
    size_t const buff_size,
    parsed_contract_t const *const contract
) {
    parsed_contract_to_string(buff, buff_size, contract);

    for (uint16_t i = 0; i < sizeof(named_delegates) / sizeof(named_delegate_t); i++) {
        if (memcmp(named_delegates[i].bakerAccount, buff, HASH_SIZE_B58) == 0) {
            // Found a matching baker, display it.
            const char* name = (const char*)pic((unsigned int)named_delegates[i].bakerName);
            if (buff_size <= strlen(name)) THROW(EXC_WRONG_LENGTH);
            strcpy(buff, name);
            return;
        }
    }

    if (buff_size <= strlen(NO_CONTRACT_NAME_STRING)) THROW(EXC_WRONG_LENGTH);
    strcpy(buff, NO_CONTRACT_NAME_STRING);
}

void pubkey_to_pkh_string(
    char *const out,
    size_t const out_size,
    derivation_type_t const derivation_type,
    cx_ecfp_public_key_t const *const public_key
) {
    check_null(out);
    check_null(public_key);

    uint8_t hash[HASH_SIZE];
    public_key_hash(hash, sizeof(hash), NULL, derivation_type, public_key);
    pkh_to_string(out, out_size, derivation_type_to_signature_type(derivation_type), hash);
}

void bip32_path_with_curve_to_pkh_string(
    char *const out, size_t const out_size,
    bip32_path_with_curve_t const *const key
) {
    check_null(out);
    check_null(key);

    cx_ecfp_public_key_t pubkey = {0};
    generate_public_key(
        &pubkey, key->derivation_type, &key->bip32_path);
    pubkey_to_pkh_string(out, out_size, key->derivation_type, &pubkey);
}


void compute_hash_checksum(uint8_t out[TEZOS_HASH_CHECKSUM_SIZE], void const *const data, size_t size) {
    uint8_t checksum[CX_SHA256_SIZE];
    cx_hash_sha256(data, size, checksum, sizeof(checksum));
    cx_hash_sha256(checksum, sizeof(checksum), checksum, sizeof(checksum));
    memcpy(out, checksum, TEZOS_HASH_CHECKSUM_SIZE);
}

void bin_to_base58(
    char *const out, size_t const out_size,
    uint8_t const *const in, size_t const in_size
) {
    check_null(out);
    check_null(in);
    size_t buff_size = out_size;
    if (!b58enc(out, &buff_size, (uint8_t const *)PIC(in), in_size)) THROW(EXC_WRONG_LENGTH);
}

void buffer_to_base58(char *const out, size_t const out_size, buffer_t const *const in) {
    check_null(out);
    check_null(in);
    buffer_t const *const src = (buffer_t const *)PIC(in);
    bin_to_base58(out, out_size, src->bytes, src->length);
}

void pkh_to_string(
    char *const buff, size_t const buff_size,
    signature_type_t const signature_type,
    uint8_t const hash[HASH_SIZE]
) {
    check_null(buff);
    check_null(hash);
    if (buff_size < PKH_STRING_SIZE) THROW(EXC_WRONG_LENGTH);

    // Data to encode
    struct __attribute__((packed)) {
        uint8_t prefix[3];
        uint8_t hash[HASH_SIZE];
        uint8_t checksum[TEZOS_HASH_CHECKSUM_SIZE];
    } data;

    // prefix
    switch (signature_type) {
        case SIGNATURE_TYPE_UNSET:
            data.prefix[0] = 2;
            data.prefix[1] = 90;
            data.prefix[2] = 121;
            break;
        case SIGNATURE_TYPE_ED25519:
            data.prefix[0] = 6;
            data.prefix[1] = 161;
            data.prefix[2] = 159;
            break;
        case SIGNATURE_TYPE_SECP256K1:
            data.prefix[0] = 6;
            data.prefix[1] = 161;
            data.prefix[2] = 161;
            break;
        case SIGNATURE_TYPE_SECP256R1:
            data.prefix[0] = 6;
            data.prefix[1] = 161;
            data.prefix[2] = 164;
            break;
        default:
            THROW(EXC_WRONG_PARAM); // Should not reach
    }

    // hash
    memcpy(data.hash, hash, sizeof(data.hash));
    compute_hash_checksum(data.checksum, &data, sizeof(data) - sizeof(data.checksum));

    size_t out_size = buff_size;
    if (!b58enc(buff, &out_size, &data, sizeof(data))) THROW(EXC_WRONG_LENGTH);
}

void protocol_hash_to_string(char *buff, const size_t buff_size, const uint8_t hash[PROTOCOL_HASH_SIZE]) {
    check_null(buff);
    check_null(hash);
    if (buff_size < PROTOCOL_HASH_BASE58_STRING_SIZE) THROW(EXC_WRONG_LENGTH);

    // Data to encode
    struct __attribute__((packed)) {
        uint8_t prefix[2];
        uint8_t hash[PROTOCOL_HASH_SIZE];
        uint8_t checksum[TEZOS_HASH_CHECKSUM_SIZE];
    } data = {
        .prefix = {2, 170}
    };

    memcpy(data.hash, hash, sizeof(data.hash));
    compute_hash_checksum(data.checksum, &data, sizeof(data) - sizeof(data.checksum));

    size_t out_size = buff_size;
    if (!b58enc(buff, &out_size, &data, sizeof(data))) THROW(EXC_WRONG_LENGTH);
}

void chain_id_to_string(char *const buff, size_t const buff_size, chain_id_t const chain_id) {
    check_null(buff);
    if (buff_size < CHAIN_ID_BASE58_STRING_SIZE) THROW(EXC_WRONG_LENGTH);

    // Data to encode
    struct __attribute__((packed)) {
        uint8_t prefix[3];
        int32_t chain_id;
        uint8_t checksum[TEZOS_HASH_CHECKSUM_SIZE];
    } data = {
        .prefix = {87, 82, 0},

        // Must hash big-endian data so treating little endian as big endian just flips
        .chain_id = READ_UNALIGNED_BIG_ENDIAN(uint32_t, &chain_id.v)
    };

    compute_hash_checksum(data.checksum, &data, sizeof(data) - sizeof(data.checksum));

    size_t out_size = buff_size;
    if (!b58enc(buff, &out_size, &data, sizeof(data))) THROW(EXC_WRONG_LENGTH);
}

#define STRCPY_OR_THROW(buff, size, x, exc) ({ \
    if (size < sizeof(x)) THROW(exc); \
    strcpy(buff, x); \
})

void chain_id_to_string_with_aliases(char *const out, size_t const out_size, chain_id_t const *const chain_id) {
    check_null(out);
    check_null(chain_id);
    if (chain_id->v == 0) {
        STRCPY_OR_THROW(out, out_size, "any", EXC_WRONG_LENGTH);
    } else if (chain_id->v == mainnet_chain_id.v) {
        STRCPY_OR_THROW(out, out_size, "mainnet", EXC_WRONG_LENGTH);
    } else {
        chain_id_to_string(out, out_size, *chain_id);
    }
}

// These functions do not output terminating null bytes.

// This function fills digits, potentially with all leading zeroes, from the end of the buffer backwards
// This is intended to be used with a temporary buffer of length MAX_INT_DIGITS
// Returns offset of where it stopped filling in
static inline size_t convert_number(char dest[MAX_INT_DIGITS], uint64_t number, bool leading_zeroes) {
    check_null(dest);
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

void number_to_string_indirect64(char *const dest, size_t const buff_size, uint64_t const *const number) {
    check_null(dest);
    check_null(number);
    if (buff_size < MAX_INT_DIGITS + 1) THROW(EXC_WRONG_LENGTH); // terminating null
    number_to_string(dest, *number);
}

void number_to_string_indirect32(char *const dest, size_t const buff_size, uint32_t const *const number) {
    check_null(dest);
    check_null(number);
    if (buff_size < MAX_INT_DIGITS + 1) THROW(EXC_WRONG_LENGTH); // terminating null
    number_to_string(dest, *number);
}

void microtez_to_string_indirect(char *const dest, size_t const buff_size, uint64_t const *const number) {
    check_null(dest);
    check_null(number);
    if (buff_size < MAX_INT_DIGITS + 1) THROW(EXC_WRONG_LENGTH); // + terminating null + decimal point
    microtez_to_string(dest, *number);
}

// Like `microtez_to_strind_indirect` but returns an error code
int microtez_to_string_indirect_no_throw(char *const dest, size_t const buff_size, uint64_t const *const number) {
    if (!dest || !number) {
        return (0);
    }
    if (buff_size < MAX_INT_DIGITS + 1) {// + terminating null + decimal point
        return (0);
    }
    // Can safely call `microtez_to_string` because we know dest is not NULL.
    microtez_to_string(dest, *number);
    return (1);
}


size_t number_to_string(char *const dest, uint64_t number) {
    check_null(dest);
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

size_t microtez_to_string(char *const dest, uint64_t number) {
    check_null(dest);
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

void copy_string(char *const dest, size_t const buff_size, char const *const src) {
    check_null(dest);
    check_null(src);
    char const *const src_in = (char const *)PIC(src);
    // I don't care that we will loop through the string twice, latency is not an issue
    if (strlen(src_in) >= buff_size) THROW(EXC_WRONG_LENGTH);
    strcpy(dest, src_in);
}

void bin_to_hex(char *const out, size_t const out_size, uint8_t const *const in, size_t const in_size) {
    check_null(out);
    check_null(in);

    size_t const out_len = in_size * 2;
    if (out_size < out_len + 1) THROW(EXC_MEMORY_ERROR);

    char const *const src = (char const *)PIC(in);
    for (size_t i = 0; i < in_size; i++) {
        out[i*2]   = "0123456789ABCDEF"[src[i] >> 4];
        out[i*2+1] = "0123456789ABCDEF"[src[i] & 0x0F];
    }
    out[out_len] = '\0';
}

void buffer_to_hex(char *const out, size_t const out_size, buffer_t const *const in) {
    check_null(out);
    check_null(in);
    buffer_t const *const src = (buffer_t const *)PIC(in);
    bin_to_hex(out, out_size, src->bytes, src->length);
}

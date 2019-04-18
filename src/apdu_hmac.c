#ifdef BAKING_APP

#include "apdu_hmac.h"

#include "globals.h"
#include "keys.h"
#include "protocol.h"


#define G global.u.hmac

static inline size_t hmac(
    uint8_t *const out, size_t const out_size,
    apdu_hmac_state_t *const state,
    uint8_t const *const in, size_t const in_size
) {
    check_null(out);
    check_null(state);
    check_null(in);
    if (out_size < CX_SHA256_SIZE) THROW(EXC_WRONG_LENGTH);

    // Pick a static, arbitrary SHA256 value based on a quote of Jesus.
    static uint8_t const key_sha256[] = {
        0x6c, 0x4e, 0x7e, 0x70, 0x6c, 0x54, 0xd3, 0x67,
        0xc8, 0x7a, 0x8d, 0x89, 0xc1, 0x6a, 0xdf, 0xe0,
        0x6c, 0xb5, 0x68, 0x0c, 0xb7, 0xd1, 0x8e, 0x62,
        0x5a, 0x90, 0x47, 0x5e, 0xc0, 0xdb, 0xdb, 0x9f };

    // Deterministically sign the SHA256 value to get something directly tied to the secret key.
    size_t const signed_hmac_key_size = sign(
        state->signed_hmac_key, sizeof(state->signed_hmac_key),
        &state->key,
        key_sha256, sizeof(key_sha256));

    // Hash the signed value with SHA512 to get a 64-byte key for HMAC.
    cx_hash_sha512(
        state->signed_hmac_key, signed_hmac_key_size,
        state->hashed_signed_hmac_key, sizeof(state->hashed_signed_hmac_key));

    return cx_hmac_sha256(
         state->hashed_signed_hmac_key, sizeof(state->hashed_signed_hmac_key),
         in, in_size,
         out, out_size);
}

size_t handle_apdu_hmac(__attribute__((unused)) uint8_t instruction) {
    if (READ_UNALIGNED_BIG_ENDIAN(uint8_t, &G_io_apdu_buffer[OFFSET_P1]) != 0) THROW(EXC_WRONG_PARAM);

    uint8_t const *const buff = &G_io_apdu_buffer[OFFSET_CDATA];
    uint8_t const buff_size = READ_UNALIGNED_BIG_ENDIAN(uint8_t, &G_io_apdu_buffer[OFFSET_LC]);
    if (buff_size > MAX_APDU_SIZE) THROW(EXC_WRONG_LENGTH_FOR_INS);

    memset(&G, 0, sizeof(G));

    G.key.curve = curve_code_to_curve(
        READ_UNALIGNED_BIG_ENDIAN(uint8_t, &G_io_apdu_buffer[OFFSET_CURVE]));

    size_t consumed = 0;
    consumed += read_bip32_path(&G.key.bip32_path, buff, buff_size);

    uint8_t const *const data_to_hmac = &buff[consumed];
    size_t const data_to_hmac_size = buff_size - consumed;

    size_t const hmac_size = hmac(
        G.hmac, sizeof(G.hmac),
        &G,
        data_to_hmac, data_to_hmac_size);

    size_t tx = 0;
    memcpy(G_io_apdu_buffer, G.hmac, hmac_size);
    tx += hmac_size;
    return finalize_successful_send(tx);
}

#endif // #ifdef BAKING_APP

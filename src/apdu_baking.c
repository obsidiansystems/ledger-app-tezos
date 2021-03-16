#ifdef BAKING_APP

#include "apdu_baking.h"

#include "apdu.h"
#include "baking_auth.h"
#include "globals.h"
#include "os_cx.h"
#include "protocol.h"
#include "to_string.h"
#include "ui.h"

#include <string.h>

#define G global.apdu.u.baking

static bool reset_ok(void);

size_t handle_apdu_reset(__attribute__((unused)) uint8_t instruction) {
    uint8_t *dataBuffer = G_io_apdu_buffer + OFFSET_CDATA;
    uint32_t dataLength = G_io_apdu_buffer[OFFSET_LC];
    if (dataLength != sizeof(level_t)) {
        THROW(EXC_WRONG_LENGTH_FOR_INS);
    }
    level_t const lvl = READ_UNALIGNED_BIG_ENDIAN(level_t, dataBuffer);
    if (!is_valid_level(lvl)) THROW(EXC_PARSE_ERROR);

    G.reset_level = lvl;

    init_screen_stack();
    push_ui_callback("Reset HWM", number_to_string_indirect32, &G.reset_level);

    ux_confirm_screen(reset_ok, delay_reject);
}

bool reset_ok(void) {
    UPDATE_NVRAM(ram, {
        ram->hwm.main.highest_level = G.reset_level;
        ram->hwm.main.had_endorsement = false;
        ram->hwm.test.highest_level = G.reset_level;
        ram->hwm.test.had_endorsement = false;
    });

    // Send back the response, do not restart the event loop
    delayed_send(finalize_successful_send(0));
    return true;
}

size_t send_word_big_endian(size_t tx, uint32_t word) {
    char word_bytes[sizeof(word)];

    memcpy(word_bytes, &word, sizeof(word));

    // endian.h functions do not compile
    uint32_t i = 0;
    for (; i < sizeof(word); i++) {
        G_io_apdu_buffer[i + tx] = word_bytes[sizeof(word) - i - 1];
    }

    return tx + i;
}

size_t handle_apdu_all_hwm(__attribute__((unused)) uint8_t instruction) {
    size_t tx = 0;
    tx = send_word_big_endian(tx, N_data.hwm.main.highest_level);
    tx = send_word_big_endian(tx, N_data.hwm.test.highest_level);
    tx = send_word_big_endian(tx, N_data.main_chain_id.v);
    return finalize_successful_send(tx);
}

size_t handle_apdu_main_hwm(__attribute__((unused)) uint8_t instruction) {
    size_t tx = 0;
    tx = send_word_big_endian(tx, N_data.hwm.main.highest_level);
    return finalize_successful_send(tx);
}


size_t handle_apdu_query_auth_key(__attribute__((unused)) uint8_t instruction) {
    uint8_t const length = N_data.baking_key.bip32_path.length;

    size_t tx = 0;
    G_io_apdu_buffer[tx++] = length;

    for (uint8_t i = 0; i < length; ++i) {
        tx = send_word_big_endian(tx, N_data.baking_key.bip32_path.components[i]);
    }

    return finalize_successful_send(tx);
}

size_t handle_apdu_query_auth_key_with_curve(__attribute__((unused)) uint8_t instruction) {
    uint8_t const length = N_data.baking_key.bip32_path.length;

    size_t tx = 0;
    G_io_apdu_buffer[tx++] = unparse_derivation_type(N_data.baking_key.derivation_type);
    G_io_apdu_buffer[tx++] = length;
    for (uint8_t i = 0; i < length; ++i) {
        tx = send_word_big_endian(tx, N_data.baking_key.bip32_path.components[i]);
    }

    return finalize_successful_send(tx);
}

size_t handle_apdu_deauthorize(__attribute__((unused)) uint8_t instruction) {
    if (READ_UNALIGNED_BIG_ENDIAN(uint8_t, &G_io_apdu_buffer[OFFSET_P1]) != 0) THROW(EXC_WRONG_PARAM);
    if (READ_UNALIGNED_BIG_ENDIAN(uint8_t, &G_io_apdu_buffer[OFFSET_LC]) != 0) THROW(EXC_PARSE_ERROR);
    UPDATE_NVRAM(ram, {
        memset(&ram->baking_key, 0, sizeof(ram->baking_key));
    });

    return finalize_successful_send(0);
}

#endif // #ifdef BAKING_APP

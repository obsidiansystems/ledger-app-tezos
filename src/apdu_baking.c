#include "apdu_baking.h"

#include "apdu.h"
#include "baking_auth.h"
#include "globals.h"
#include "os_cx.h"
#include "protocol.h"
#include "to_string.h"
#include "ui_prompt.h"

#include <string.h>

#define G global.u.baking

static bool reset_ok(void);

unsigned int handle_apdu_reset(__attribute__((unused)) uint8_t instruction) {
    uint8_t *dataBuffer = G_io_apdu_buffer + OFFSET_CDATA;
    uint32_t dataLength = G_io_apdu_buffer[OFFSET_LC];
    if (dataLength != sizeof(int)) {
        THROW(EXC_WRONG_LENGTH_FOR_INS);
    }
    level_t const lvl = READ_UNALIGNED_BIG_ENDIAN(level_t, dataBuffer);
    if (!is_valid_level(lvl)) THROW(EXC_PARSE_ERROR);

    G.reset_level = lvl;

    register_ui_callback(0, number_to_string_indirect32, &G.reset_level);

    static const char *const reset_prompts[] = {
        PROMPT("Reset HWM"),
        NULL,
    };
    ui_prompt(reset_prompts, NULL, reset_ok, delay_reject);
}

bool reset_ok(void) {
    UPDATE_NVRAM(ram, {
        ram->hwm.main.highest_level = G.reset_level;
        ram->hwm.main.had_endorsement = false;
        ram->hwm.test.highest_level = G.reset_level;
        ram->hwm.test.had_endorsement = false;
    });

    uint32_t tx = 0;
    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;

    // Send back the response, do not restart the event loop
    delayed_send(tx);
    return true;
}

uint32_t send_word_big_endian(uint32_t tx, uint32_t word) {
    char word_bytes[sizeof(word)];

    memcpy(word_bytes, &word, sizeof(word));

    // endian.h functions do not compile
    uint32_t i = 0;
    for (; i < sizeof(word); i++) {
        G_io_apdu_buffer[i + tx] = word_bytes[sizeof(word) - i - 1];
    }

    return tx + i;
}

unsigned int handle_apdu_all_hwm(__attribute__((unused)) uint8_t instruction) {
    uint32_t tx = 0;
    tx = send_word_big_endian(tx, N_data.hwm.main.highest_level);
    tx = send_word_big_endian(tx, N_data.hwm.test.highest_level);
    tx = send_word_big_endian(tx, N_data.main_chain_id.v);
    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;
    return tx;
}

unsigned int handle_apdu_main_hwm(__attribute__((unused)) uint8_t instruction) {
    uint32_t tx = 0;
    tx = send_word_big_endian(tx, N_data.hwm.main.highest_level);
    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;
    return tx;
}


unsigned int handle_apdu_query_auth_key(__attribute__((unused)) uint8_t instruction) {
    uint8_t const length = N_data.bip32_path.length;

    uint32_t tx = 0;
    G_io_apdu_buffer[tx++] = length;

    for (uint8_t i = 0; i < length; ++i) {
        tx = send_word_big_endian(tx, N_data.bip32_path.components[i]);
    }

    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;
    return tx;
}

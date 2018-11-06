#include "apdu.h"
#include "baking_auth.h"
#include "apdu_baking.h"
#include "cx.h"
#include "os.h"
#include "protocol.h"
#include "to_string.h"
#include "ui_prompt.h"

#include <string.h>

static level_t reset_level;

const char *const reset_prompts[] = {
    "Reset HWM",
    NULL,
};

static bool reset_ok(void);

unsigned int handle_apdu_reset(__attribute__((unused)) uint8_t instruction) {
    uint8_t *dataBuffer = G_io_apdu_buffer + OFFSET_CDATA;
    uint32_t dataLength = G_io_apdu_buffer[OFFSET_LC];
    if (dataLength != sizeof(int)) {
        THROW(EXC_WRONG_LENGTH_FOR_INS);
    }
    level_t lvl = READ_UNALIGNED_BIG_ENDIAN(level_t, dataBuffer);

    if (!is_valid_level(lvl)) {
        THROW(EXC_PARSE_ERROR);
    }

    reset_level = lvl;

    number_to_string(get_value_buffer(0), reset_level);
    ui_prompt(reset_prompts, NULL, reset_ok, delay_reject);
}

bool reset_ok(void) {
    write_highest_level(reset_level, false); // We have not yet had an endorsement at this level

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

unsigned int handle_apdu_hwm(__attribute__((unused)) uint8_t instruction) {
    uint32_t tx = 0;

    level_t level = N_data.highest_level;
    tx = send_word_big_endian(tx, level);

    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;
    return tx;
}

unsigned int handle_apdu_query_auth_key(__attribute__((unused)) uint8_t instruction) {
    uint32_t tx = 0;

    uint8_t length = N_data.path_length;
    G_io_apdu_buffer[tx++] = length;

    for (uint8_t i = 0; i < length; ++i) {
        tx = send_word_big_endian(tx, N_data.bip32_path[i]);
    }

    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;
    return tx;
}

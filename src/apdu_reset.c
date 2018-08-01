#include "apdu.h"
#include "baking_auth.h"
#include "apdu_reset.h"
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

char reset_string[21];

const char *const reset_values[] = {
    reset_string,
    NULL,
};

static bool reset_ok(void);

unsigned int handle_apdu_reset(__attribute__((unused)) uint8_t instruction) {
    uint8_t *dataBuffer = G_io_apdu_buffer + OFFSET_CDATA;
    uint32_t dataLength = G_io_apdu_buffer[OFFSET_LC];
    if (dataLength != sizeof(int)) {
        THROW(EXC_WRONG_LENGTH_FOR_INS);
    }
    level_t lvl = READ_UNALIGNED_BIG_ENDIAN(int32_t, dataBuffer);

    if (!is_valid_level(lvl)) {
        THROW(EXC_PARSE_ERROR);
    }
    reset_level = lvl;

    number_to_string(reset_string, reset_level);
    ui_prompt_multiple(reset_prompts, reset_values, reset_ok, delay_reject);
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

unsigned int handle_apdu_hwm(__attribute__((unused)) uint8_t instruction) {
    level_t level = N_data.highest_level;
    return send_word_big_endian(level);
}

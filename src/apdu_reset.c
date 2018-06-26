#include "apdu.h"
#include "baking_auth.h"
#include "apdu_reset.h"
#include "cx.h"
#include "os.h"
#include "protocol.h"
#include <string.h>

#define RESET_STRING "Reset HWM: "
char reset_string[sizeof(RESET_STRING) + 10]; // 10 is max number of digits in 32-bit number

const bagl_element_t ui_bake_reset_screen[] = {
    // type                               userid    x    y   w    h  str rad
    // fill      fg        bg      fid iid  txt   touchparams...       ]
    {{BAGL_RECTANGLE, 0x00, 0, 0, 128, 32, 0, 0, BAGL_FILL, 0x000000, 0xFFFFFF,
      0, 0},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_LABELINE, 0x01, 0, 12, 128, 32, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     "Tezos",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_LABELINE, 0x01, 0, 26, 128, 32, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     reset_string,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_ICON, 0x00, 3, 12, 7, 7, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
      BAGL_GLYPH_ICON_CROSS},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_ICON, 0x00, 117, 13, 8, 6, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
      BAGL_GLYPH_ICON_CHECK},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
};

static level_t reset_level;

static void reset_ok();

unsigned int handle_apdu_reset(uint8_t instruction) {
    uint8_t *dataBuffer = G_io_apdu_buffer + OFFSET_CDATA;
    uint32_t dataLength = G_io_apdu_buffer[OFFSET_LC];
    if (dataLength != sizeof(int)) {
        THROW(0x6C00);
    }
    level_t lvl = READ_UNALIGNED_BIG_ENDIAN(int32_t, dataBuffer);

    if (!is_valid_level(lvl)) {
        THROW(0x6B00);
    }
    reset_level = lvl;

    strcpy(reset_string, RESET_STRING);
    char *number_field = reset_string + sizeof(RESET_STRING) - 1;
    uint32_t res = number_to_string(number_field, reset_level);
    number_field[res] = '\0';

    ASYNC_PROMPT(ui_bake_reset_screen, reset_ok, delay_reject);
}

void reset_ok() {
    write_highest_level(reset_level);

    uint32_t tx = 0;
    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;


    // Send back the response, do not restart the event loop
    delay_send(tx);
}

unsigned int handle_apdu_hwm(uint8_t instruction) {
    uint32_t level = N_data.highest_level;
    char level_bytes[sizeof(level)];
    memcpy(level_bytes, &level, sizeof(level));
    uint32_t tx = 0;
    for (; tx < sizeof(level); tx++) {
        G_io_apdu_buffer[tx] = level_bytes[sizeof(level) - tx - 1];
    }
    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;
    return tx;
}

#pragma once

#include "ui.h"
#include "os.h"
#include "paths.h"

#include <stdbool.h>
#include <stdint.h>

#define OFFSET_CLA 0
#define OFFSET_INS 1
#define OFFSET_P1 2
#define OFFSET_CURVE 3
#define OFFSET_LC 4
#define OFFSET_CDATA 5

#define INS_VERSION 0x00
#define INS_MASK 0x0F
#define INS_EXIT 0x0F // Equivalent to 0xFF, it's only significant in the bits in INS_MASK

// Throw this to indicate prompting
#define ASYNC_EXCEPTION 0x2000

#define EXC_WRONG_PARAM 0x6B00
#define EXC_WRONG_LENGTH 0x6C00
#define EXC_INVALID_INS 0x6D00
#define EXC_WRONG_LENGTH_FOR_INS 0x917E
#define EXC_REJECT 0x6985
#define EXC_PARSE_ERROR 0x9405
#define EXC_WRONG_VALUES 0x6A80
#define EXC_SECURITY 0x6982
#define EXC_CLASS 0x6E00
#define EXC_NO_ERROR 0x9000
#define EXC_MEMORY_ERROR 0x9200

static inline void check_null(void *ptr) {
    if (ptr == NULL) {
        THROW(EXC_MEMORY_ERROR);
    }
}

#define ASYNC_PROMPT(screen, ok, cxl) \
    UI_PROMPT(screen, ok, cxl); \
    THROW(ASYNC_EXCEPTION)

// Return number of bytes to transmit (tx)
typedef uint32_t (*apdu_handler)(uint8_t instruction);

void main_loop(apdu_handler handlers[INS_MASK + 1]);

static inline void return_ok(void) {
    THROW(EXC_NO_ERROR);
}

// Send back response; do not restart the event loop
static inline void delayed_send(uint32_t tx) {
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);
}

static inline void delay_reject(void) {
    G_io_apdu_buffer[0] = EXC_REJECT >> 8;
    G_io_apdu_buffer[1] = EXC_REJECT & 0xFF;
    delayed_send(2);
}

uint32_t handle_apdu_error(uint8_t instruction);
uint32_t handle_apdu_version(uint8_t instruction);
uint32_t handle_apdu_exit(uint8_t instruction);

uint32_t send_word_big_endian(uint32_t word);

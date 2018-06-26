#pragma once

#include "ui.h"
#include "os.h"
#include "paths.h"

#include <stdbool.h>
#include <stdint.h>

#define OFFSET_CLA 0
#define OFFSET_INS 1
#define OFFSET_P1 2
#define OFFSET_P2 3
#define OFFSET_LC 4
#define OFFSET_CDATA 5

#define TEZOS_BUFSIZE 1024

#define INS_VERSION 0x00
#define INS_MASK 0x0F
#define INS_EXIT 0x0F // Equivalent to 0xFF, it's only significant in the bits in INS_MASK

// Throw this to indicate prompting
#define ASYNC_EXCEPTION 0x2000

#define ASYNC_PROMPT(screen, ok, cxl) \
    UI_PROMPT(screen, ok, cxl); \
    THROW(ASYNC_EXCEPTION)

// Return number of bytes to transmit (tx)
typedef uint32_t (*apdu_handler)(uint8_t instruction);

void main_loop(apdu_handler handlers[INS_MASK + 1]);

static inline void return_ok() {
    THROW(0x9000);
}

// Send back response; do not restart the event loop
static inline void delay_send(int tx) {
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);
}

static inline void delay_reject() {
    G_io_apdu_buffer[0] = 0x69;
    G_io_apdu_buffer[1] = 0x85;
    delay_send(2);
}

uint32_t handle_apdu_error(uint8_t instruction);
uint32_t handle_apdu_version(uint8_t instruction);
uint32_t handle_apdu_exit(uint8_t instruction);

uint32_t send_word_big_endian(uint32_t word);

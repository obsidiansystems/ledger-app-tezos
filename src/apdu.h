#pragma once

#include "exception.h"
#include "keys.h"

#include "ui.h"
#include "os.h"

#include <stdbool.h>
#include <stdint.h>

#if CX_APILEVEL < 8
#error "May only compile with API level 8 or higher; requires newer firmware"
#endif

#define OFFSET_CLA 0
#define OFFSET_INS 1
#define OFFSET_P1 2
#define OFFSET_CURVE 3
#define OFFSET_LC 4
#define OFFSET_CDATA 5

#define INS_VERSION 0x00
#define INS_MAX 0x09

// Return number of bytes to transmit (tx)
typedef uint32_t (*apdu_handler)(uint8_t instruction);

__attribute__((noreturn))
void main_loop(apdu_handler handlers[INS_MAX]);

static inline void return_ok(void) {
    THROW(EXC_NO_ERROR);
}

// Send back response; do not restart the event loop
static inline void delayed_send(uint32_t tx) {
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);
}

static inline bool delay_reject(void) {
    G_io_apdu_buffer[0] = EXC_REJECT >> 8;
    G_io_apdu_buffer[1] = EXC_REJECT & 0xFF;
    delayed_send(2);
    return true;
}

static inline void require_hid(void) {
    if (G_io_apdu_media != IO_APDU_MEDIA_USB_HID) {
       THROW(EXC_SECURITY);
    }
}

uint32_t handle_apdu_error(uint8_t instruction);
uint32_t handle_apdu_version(uint8_t instruction);

extern uint32_t app_stack_canary;

extern void *stack_root;
static inline void throw_stack_size() {
    uint8_t st;
    // uint32_t tmp1 = (uint32_t)&st - (uint32_t)&app_stack_canary;
    uint32_t tmp2 = (uint32_t)stack_root - (uint32_t)&st;
    THROW(0x9000 + tmp2);
}

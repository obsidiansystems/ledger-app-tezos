#pragma once

#include "exception.h"
#include "keys.h"
#include "types.h"
#include "ui.h"

#include "os.h"

#include <stdbool.h>
#include <stdint.h>

#if CX_APILEVEL < 8
#error "May only compile with API level 8 or higher; requires newer firmware"
#endif

#define OFFSET_CLA 0
#define OFFSET_INS 1    // instruction code
#define OFFSET_P1 2     // user-defined 1-byte parameter
#define OFFSET_CURVE 3
#define OFFSET_LC 4     // length of CDATA
#define OFFSET_CDATA 5  // payload

// Instruction codes
#define INS_VERSION 0x00
#define INS_AUTHORIZE_BAKING 0x01
#define INS_GET_PUBLIC_KEY 0x02
#define INS_PROMPT_PUBLIC_KEY 0x03
#define INS_SIGN 0x04
#define INS_SIGN_UNSAFE 0x05 // Data that is already hashed.
#define INS_RESET 0x06
#define INS_QUERY_AUTH_KEY 0x07
#define INS_QUERY_HWM 0x08
#define INS_GIT 0x09

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
       THROW(EXC_HID_REQUIRED);
    }
}

uint32_t handle_apdu_error(uint8_t instruction);
uint32_t handle_apdu_version(uint8_t instruction);
uint32_t handle_apdu_git(uint8_t instruction);

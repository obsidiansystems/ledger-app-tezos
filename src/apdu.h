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
#define INS_QUERY_MAIN_HWM 0x08
#define INS_GIT 0x09
#define INS_SETUP 0x0A
#define INS_QUERY_ALL_HWM 0x0B
#define INS_DEAUTHORIZE 0x0C
#define INS_QUERY_AUTH_KEY_WITH_CURVE 0x0D
#define INS_HMAC 0x0E
#define INS_SIGN_WITH_HASH 0x0F

__attribute__((noreturn))
void main_loop(apdu_handler const *const handlers, size_t const handlers_size);

static inline size_t finalize_successful_send(size_t tx) {
    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;
    return tx;
}

// Send back response; do not restart the event loop
static inline void delayed_send(size_t tx) {
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);
}

static inline bool delay_reject(void) {
    size_t tx = 0;
    G_io_apdu_buffer[tx++] = EXC_REJECT >> 8;
    G_io_apdu_buffer[tx++] = EXC_REJECT & 0xFF;
    delayed_send(tx);
    return true;
}

static inline void require_hid(void) {
    if (G_io_apdu_media != IO_APDU_MEDIA_USB_HID) {
       THROW(EXC_HID_REQUIRED);
    }
}

size_t provide_pubkey(uint8_t *const io_buffer, cx_ecfp_public_key_t const *const pubkey);

size_t handle_apdu_error(uint8_t instruction);
size_t handle_apdu_version(uint8_t instruction);
size_t handle_apdu_git(uint8_t instruction);

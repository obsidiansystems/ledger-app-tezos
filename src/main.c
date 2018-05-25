/*******************************************************************************
*   Ledger Blue
*   (c) 2016 Ledger
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
********************************************************************************/

#include "baking_auth.h"
#include "paths.h"
#include "blake2.h"
#include "protocol.h"
#include "ui.h"

#include "os.h"
#include "cx.h"
#include <stdbool.h>
#include <string.h>

#include "prompt_screens.h"

#define CLA 0x80

#define P1_FIRST 0x00
#define P1_NEXT 0x01
#define P1_LAST_MARKER 0x80

#define OFFSET_CLA 0
#define OFFSET_INS 1
#define OFFSET_P1 2
#define OFFSET_P2 3
#define OFFSET_LC 4
#define OFFSET_CDATA 5

static bool address_enabled;

static void bake_ok(void *);
static void sign_ok(void *);
static void sign_unsafe_ok(void *);
static void sign_cancel(void *);

static void address_ok(void *);
static void address_cancel(void *);

static void delay_reject();

static int perform_signature(bool hash_first);
static int provide_address();

#define TEZOS_BUFSIZE 1024

typedef struct operationContext_t {
    uint8_t path_length;
    uint32_t bip32_path[MAX_BIP32_PATH];
    cx_ecfp_public_key_t publicKey;
    cx_curve_t curve;
    uint8_t data[TEZOS_BUFSIZE];
    uint32_t datalen;
} operationContext_t;

operationContext_t operationContext;

#define HARDENING_BIT (1u << 31)

void sign_unsafe_ok(void *context) {
    int tx = perform_signature(false);
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);
}

void sign_ok(void *context) {
    int tx = perform_signature(true);
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);
}

void bake_ok(void *ignore) {
    bool success = authorize_baking(operationContext.data, operationContext.datalen,
                                    operationContext.bip32_path, operationContext.path_length);
    if (!success) {
        return delay_reject(); // Bad BIP32 path
    }

    int tx = perform_signature(true);
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);
}

void reset_ok(void *ignore) {
    int level = read_unaligned_big_endian(operationContext.data);
    write_highest_level(level);

    G_io_apdu_buffer[0] = 0x90;
    G_io_apdu_buffer[1] = 0x00;

    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
}

void reset_cancel(void *ignore) {
    delay_reject();
}

#define HASH_SIZE 32

int perform_signature(bool hash_first) {
    uint8_t privateKeyData[32];
    cx_ecfp_private_key_t privateKey;
    static uint8_t hash[HASH_SIZE];
    uint8_t *data = operationContext.data;
    uint32_t datalen = operationContext.datalen;

    update_high_water_mark(operationContext.data, operationContext.datalen);

    if (hash_first) {
        blake2b(hash, HASH_SIZE, operationContext.data, operationContext.datalen, NULL, 0);
        data = hash;
        datalen = HASH_SIZE;
    }

    os_perso_derive_node_bip32(operationContext.curve,
                               operationContext.bip32_path,
                               operationContext.path_length,
                               privateKeyData,
                               NULL);

    cx_ecfp_init_private_key(operationContext.curve,
                             privateKeyData,
                             32,
                             &privateKey);

    os_memset(privateKeyData, 0, sizeof(privateKeyData));

    int tx;
    switch(operationContext.curve) {
    case CX_CURVE_Ed25519: {
        tx = cx_eddsa_sign(&privateKey,
                           0,
                           CX_SHA512,
                           data,
                           datalen,
                           NULL,
                           0,
                           &G_io_apdu_buffer[0],
                           64,
                           NULL);
    }
        break;
    case CX_CURVE_SECP256K1: {
        unsigned int info;
        tx = cx_ecdsa_sign(&privateKey,
                           CX_LAST | CX_RND_TRNG,
                           CX_NONE,
                           data,
                           datalen,
                           &G_io_apdu_buffer[0],
                           100,
                           &info);
        if (info & CX_ECCINFO_PARITY_ODD) {
            G_io_apdu_buffer[0] |= 0x01;
        }
    }
        break;
    default:
        THROW(0x6B00);
    }

    os_memset(&privateKey, 0, sizeof(privateKey));

    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;

    return tx;
}

void sign_cancel(void *ignore) {
    delay_reject();
}

void address_ok(void *ignore) {
    address_enabled = true;

    int tx = provide_address();

    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);
}

int provide_address() {
    int tx = 0;
    G_io_apdu_buffer[tx++] = operationContext.publicKey.W_len;
    os_memmove(G_io_apdu_buffer + tx,
               operationContext.publicKey.W,
               operationContext.publicKey.W_len);
    tx += operationContext.publicKey.W_len;
    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;
    return tx;
}

void address_cancel(void *ignore) {
    delay_reject();
}

void delay_reject() {
    G_io_apdu_buffer[0] = 0x69;
    G_io_apdu_buffer[1] = 0x85;
    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
}

#define INS_MASK 0x07

#define INS_GET_PUBLIC_KEY 0x02
#define INS_SIGN 0x04
#define INS_SIGN_UNSAFE 0x05 // Data that is already hashed.
#define INS_RESET 0x06
#define INS_EXIT 0x07 // So you can send 0xFF, only significant to (x & INS_MASK)

// Throw this to indicate prompting
#define ASYNC_EXCEPTION 0x2000

#define ASYNC_PROMPT(screen, ok, cxl) \
    UI_PROMPT(screen, ok, cxl); \
    THROW(ASYNC_EXCEPTION)

// Return number of bytes to transmit (tx)
typedef unsigned int (*apdu_handler)(uint8_t instruction);

unsigned int handle_apdu_get_public_key(uint8_t instruction) {
    uint8_t privateKeyData[32];
    uint8_t *dataBuffer = G_io_apdu_buffer + OFFSET_CDATA;
    cx_ecfp_private_key_t privateKey;

    if (G_io_apdu_buffer[OFFSET_P1] != 0)
        THROW(0x6B00);

    if(G_io_apdu_buffer[OFFSET_P2] > 2)
        THROW(0x6B00);

    switch(G_io_apdu_buffer[OFFSET_P2]) {
        case 0:
            operationContext.curve = CX_CURVE_Ed25519;
            break;
        case 1:
            operationContext.curve = CX_CURVE_SECP256K1;
            break;
        case 2:
            operationContext.curve = CX_CURVE_SECP256R1;
            break;
    }

    operationContext.path_length = read_bip32_path(operationContext.bip32_path, dataBuffer);

    os_perso_derive_node_bip32(operationContext.curve,
                               operationContext.bip32_path,
                               operationContext.path_length,
                               privateKeyData, NULL);

    cx_ecfp_init_private_key(operationContext.curve,
                             privateKeyData,
                             32,
                             &privateKey);

    cx_ecfp_generate_pair(operationContext.curve,
                          &operationContext.publicKey,
                          &privateKey, 1);

    os_memset(&privateKey, 0, sizeof(privateKey));
    os_memset(privateKeyData, 0, sizeof(privateKeyData));

    if (operationContext.curve == CX_CURVE_Ed25519) {
        cx_edward_compress_point(operationContext.curve,
                                 operationContext.publicKey.W,
                                 operationContext.publicKey.W_len);
        operationContext.publicKey.W_len = 33;
    }

    if (address_enabled) {
        return provide_address();
    } else {
        prompt_address(operationContext.publicKey.W, operationContext.publicKey.W_len,
                       address_ok, address_cancel);
        THROW(ASYNC_EXCEPTION);
    }
}

unsigned int handle_apdu_reset(uint8_t instruction) {
    uint8_t *dataBuffer = G_io_apdu_buffer + OFFSET_CDATA;
    uint32_t dataLength = G_io_apdu_buffer[OFFSET_LC];
    if (dataLength != sizeof(int)) {
        THROW(0x6C00);
    }
    operationContext.datalen = sizeof(int);
    memcpy(operationContext.data, dataBuffer, sizeof(int));
    ASYNC_PROMPT(ui_bake_reset_screen, reset_ok, reset_cancel);
}

static void return_ok() {
    THROW(0x9000);
}

unsigned int handle_apdu_sign(uint8_t instruction) {
    uint8_t p1 = G_io_apdu_buffer[OFFSET_P1];
    uint8_t *dataBuffer = G_io_apdu_buffer + OFFSET_CDATA;
    uint32_t dataLength = G_io_apdu_buffer[OFFSET_LC];

    bool last = (p1 & P1_LAST_MARKER) != 0;
    switch (p1 & ~P1_LAST_MARKER) {
    case P1_FIRST:
        os_memset(operationContext.data, 0, TEZOS_BUFSIZE);
        operationContext.datalen = 0;
        operationContext.path_length = read_bip32_path(operationContext.bip32_path,
                                                       dataBuffer);
        switch(G_io_apdu_buffer[OFFSET_P2]) {
            case 0:
                operationContext.curve = CX_CURVE_Ed25519;
                break;
            case 1:
                operationContext.curve = CX_CURVE_SECP256K1;
                break;
            case 2:
                operationContext.curve = CX_CURVE_SECP256R1;
                break;
        }
        return_ok();
    case P1_NEXT:
        break;
    default:
        THROW(0x6B00);
    }

    if(G_io_apdu_buffer[OFFSET_P2] > 2) {
        THROW(0x6B00);
    }

    if (operationContext.datalen + dataLength > TEZOS_BUFSIZE) {
        THROW(0x6C00);
    }

    os_memmove(operationContext.data + operationContext.datalen, dataBuffer, dataLength);
    operationContext.datalen += dataLength;

    if (!last) {
        return_ok();
    }

    if (instruction == INS_SIGN_UNSAFE) {
        ASYNC_PROMPT(ui_sign_unsafe_screen, sign_unsafe_ok, sign_cancel);
    }

    // Have raw data, can get insight into it
    switch (get_magic_byte(operationContext.data, operationContext.datalen)) {
        case MAGIC_BYTE_BLOCK:
        case MAGIC_BYTE_BAKING_OP:
            if (is_baking_authorized(operationContext.data,
                                     operationContext.datalen,
                                     operationContext.bip32_path,
                                     operationContext.path_length)) {
                return perform_signature(true);
            } else {
                ASYNC_PROMPT(ui_bake_screen, bake_ok, sign_cancel);
            }
        case MAGIC_BYTE_UNSAFE_OP:
        case MAGIC_BYTE_UNSAFE_OP2:
        case MAGIC_BYTE_UNSAFE_OP3:
            ASYNC_PROMPT(ui_sign_screen, sign_ok, sign_cancel);
        default:
            THROW(0x6C00);
    }
}

unsigned int handle_apdu_error(uint8_t instruction) {
    THROW(0x6D00);
}

unsigned int handle_apdu_exit(uint8_t instruction) {
    os_sched_exit(-1);
    THROW(0x6D00); // avoid warning
}

void main_loop(apdu_handler handlers[INS_MASK + 1]) {
    volatile uint32_t rx = io_exchange(CHANNEL_APDU, 0);
    while (true) {
        BEGIN_TRY {
            TRY {
                if (rx == 0) {
                    // no apdu received, well, reset the session, and reset the
                    // bootloader configuration
                    THROW(0x6982);
                }

                if (G_io_apdu_buffer[0] != CLA) {
                    THROW(0x6E00);
                }

                uint8_t instruction = G_io_apdu_buffer[1];
                uint32_t tx = handlers[instruction & INS_MASK](instruction);
                rx = io_exchange(CHANNEL_APDU, tx);
            }
            CATCH_OTHER(e) {
                unsigned short sw = e;
                switch (sw) {
                case ASYNC_EXCEPTION:
                    rx = io_exchange(CHANNEL_APDU | IO_ASYNCH_REPLY, 0);
                    break;
                default:
                    sw = 0x6800 | (e & 0x7FF);
                case 0x6000 ... 0x6FFF:
                case 0x9000 ... 0x9FFF:
                    G_io_apdu_buffer[0] = sw >> 8;
                    G_io_apdu_buffer[1] = sw;
                    rx = io_exchange(CHANNEL_APDU, 2);
                    break;
                }
            }
            FINALLY {
            }
        }
        END_TRY;
    }
}

void app_main(void) {
    static apdu_handler handlers[INS_MASK + 1];

    // TODO: Consider using static initialization of a const, instead of this
    for (int i = 0; i < INS_MASK + 1; i++) {
        handlers[i] = handle_apdu_error;
    }
    handlers[INS_GET_PUBLIC_KEY] = handle_apdu_get_public_key;
    handlers[INS_RESET] = handle_apdu_reset;
    handlers[INS_SIGN] = handle_apdu_sign;
    handlers[INS_SIGN_UNSAFE] = handle_apdu_sign;
    handlers[INS_EXIT] = handle_apdu_exit;
    main_loop(handlers);
}
void app_exit(void) {
    BEGIN_TRY_L(exit) {
        TRY_L(exit) {
            os_sched_exit(-1);
        }
        FINALLY_L(exit) {
        }
    }
    END_TRY_L(exit);
}

// TODO: I have no idea what this function does, but it is called by the OS
unsigned short io_exchange_al(unsigned char channel, unsigned short tx_len) {
    switch (channel & ~(IO_FLAGS)) {
    case CHANNEL_KEYBOARD:
        break;

    // multiplexed io exchange over a SPI channel and TLV encapsulated protocol
    case CHANNEL_SPI:
        if (tx_len) {
            io_seproxyhal_spi_send(G_io_apdu_buffer, tx_len);

            if (channel & IO_RESET_AFTER_REPLIED) {
                reset();
            }
            return 0; // nothing received from the master so far (it's a tx
                      // transaction)
        } else {
            return io_seproxyhal_spi_recv(G_io_apdu_buffer,
                                          sizeof(G_io_apdu_buffer), 0);
        }

    default:
        THROW(INVALID_PARAMETER);
    }
    return 0;
}

__attribute__((section(".boot"))) int main(void) {
    // exit critical section
    __asm volatile("cpsie i");

    ui_init();

    // ensure exception will work as planned
    os_boot();

    address_enabled = false;

    BEGIN_TRY {
        TRY {
            io_seproxyhal_init();

            USB_power(1);

            ui_initial_screen();

            app_main();
        }
        CATCH_OTHER(e) {
        }
        FINALLY {
        }
    }
    END_TRY;

    app_exit();
}

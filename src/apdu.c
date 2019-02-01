#include "apdu.h"
#include "version.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

unsigned int handle_apdu_error(uint8_t __attribute__((unused)) instruction) {
    THROW(EXC_INVALID_INS);
}

unsigned int handle_apdu_version(uint8_t __attribute__((unused)) instruction) {
    int tx = 0;
    memcpy(G_io_apdu_buffer, &version, sizeof(version_t));
    tx += sizeof(version_t);
    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;
    return tx;
}

unsigned int handle_apdu_git(uint8_t __attribute__((unused)) instruction) {
    static const char commit[] = COMMIT;
    memcpy(G_io_apdu_buffer, commit, sizeof(commit));

    uint32_t tx = sizeof(commit);
    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;
    return tx;
}

#define CLA 0x80

__attribute__((noreturn))
void main_loop(apdu_handler handlers[INS_MAX]) {
    volatile uint32_t rx = io_exchange(CHANNEL_APDU, 0);
    while (true) {
        BEGIN_TRY {
            TRY {
                // Process APDU of size rx

                if (rx == 0) {
                    // no apdu received, well, reset the session, and reset the
                    // bootloader configuration
                    THROW(EXC_SECURITY);
                }

                if (G_io_apdu_buffer[OFFSET_CLA] != CLA) {
                    THROW(EXC_CLASS);
                }

                // The amount of bytes we get in our APDU must match what the APDU declares
                // its own content length is. All these values are unsigned, so this implies
                // that if rx < OFFSET_CDATA it also throws.
                if (rx != G_io_apdu_buffer[OFFSET_LC] + OFFSET_CDATA) {
                    THROW(EXC_WRONG_LENGTH);
                }

                const uint8_t instruction = G_io_apdu_buffer[OFFSET_INS];
                const apdu_handler cb = (instruction >= INS_MAX)
                    ? handle_apdu_error
                    : handlers[instruction];

                const uint32_t tx = cb(instruction);
                rx = io_exchange(CHANNEL_APDU, tx);
            }
            CATCH(ASYNC_EXCEPTION) {
                rx = io_exchange(CHANNEL_APDU | IO_ASYNCH_REPLY, 0);
            }
            CATCH_OTHER(e) {
                uint16_t sw = e;
                switch (sw) {
                default:
                    sw = 0x6800 | (e & 0x7FF);
                    // FALL THROUGH
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

// I have no idea what this function does, but it is called by the OS
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

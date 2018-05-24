#include "ui.h"

#include "idle_screen.h"
#include "error_screen.h"

#include <stdbool.h>

ux_state_t ux;
unsigned char G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B];

static void *cb_context;
static callback_t ok_callback;
static callback_t cxl_callback;

static unsigned button_handler(unsigned button_mask, unsigned button_mask_counter);
static void do_nothing(void *context);
static void exit_app(void *context);

static uint32_t ux_step, ux_step_count;

void ui_init(void) {
    UX_INIT();
    ok_callback = NULL;
    cxl_callback = NULL;
}

void ui_display_error(void) {
    UI_PROMPT(ui_error_screen, exit_app, exit_app);
}

static void do_nothing(void *context) {
}

static void exit_app(void *context) {
    os_sched_exit(0);
}

static void ui_idle(void) {
    ui_prompt(ui_idle_screen, sizeof(ui_idle_screen)/sizeof(*ui_idle_screen),
              do_nothing, exit_app, NULL, NULL);
}

void ui_initial_screen(void) {
    ui_idle();
}

unsigned button_handler(unsigned button_mask, unsigned button_mask_counter) {
    switch (button_mask) {
        case BUTTON_EVT_RELEASED | BUTTON_LEFT:
            cxl_callback(cb_context);
            break;
        case BUTTON_EVT_RELEASED | BUTTON_RIGHT:
            ok_callback(cb_context);
            break;
        default:
            return 0;
    }
    ui_idle(); // display original screen
    return 0; // do not redraw the widget
}

#ifndef TIMEOUT_SECONDS
#define TIMEOUT_SECONDS 30
#endif

const bagl_element_t *timer_setup(const bagl_element_t *elem) {
    ux_step_count = 0;
    io_seproxyhal_setup_ticker(TIMEOUT_SECONDS * 1000);
    return elem;
}

void ui_prompt(const bagl_element_t *elems, size_t sz, callback_t ok_c, callback_t cxl_c, void *cxt,
               bagl_element_callback_t prepro) {
    // Copied from definition of UX_DISPLAY in header file
    ok_callback = ok_c;
    cxl_callback = cxl_c;
    cb_context = cxt;
    ux.elements = elems;
    ux.elements_count = sz;
    ux.button_push_handler = button_handler;
    ux.elements_preprocessor = prepro;
    UX_WAKE_UP();
    UX_REDISPLAY();
}

unsigned char io_event(unsigned char channel) {
    // nothing done with the event, throw an error on the transport layer if
    // needed

    // can't have more than one tag in the reply, not supported yet.
    switch (G_io_seproxyhal_spi_buffer[0]) {
    case SEPROXYHAL_TAG_FINGER_EVENT:
        UX_FINGER_EVENT(G_io_seproxyhal_spi_buffer);
        break;

    case SEPROXYHAL_TAG_BUTTON_PUSH_EVENT:
        UX_BUTTON_PUSH_EVENT(G_io_seproxyhal_spi_buffer);
        break;

    case SEPROXYHAL_TAG_DISPLAY_PROCESSED_EVENT:
        UX_DISPLAYED_EVENT({});
        break;
    case SEPROXYHAL_TAG_TICKER_EVENT:
        if (ux_step_count != 0) {
            UX_TICKER_EVENT(G_io_seproxyhal_spi_buffer, {
                // don't redisplay if UX not allowed (pin locked in the common bolos
                // ux ?)
                if (ux_step_count && UX_ALLOWED) {
                    // prepare next screen
                    ux_step = (ux_step + 1) % ux_step_count;
                    // redisplay screen
                    UX_REDISPLAY();
                }
            });
        } else if (cxl_callback != exit_app) {
            cxl_callback(cb_context);
            ui_idle();
        }
        break;

    // unknown events are acknowledged
    default:
        break;
    }

    // close the event if not done previously (by a display or whatever)
    if (!io_seproxyhal_spi_is_status_sent()) {
        io_seproxyhal_general_status();
    }
    // command has been processed, DO NOT reset the current APDU transport
    return 1;
}

void io_seproxyhal_display(const bagl_element_t *element) {
    return io_seproxyhal_display_default((bagl_element_t *)element);
}

#define ADDRESS_BYTES 40
char address_display_data[ADDRESS_BYTES * 2 + 1];

const bagl_element_t ui_display_address[] = {
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

    //{{BAGL_ICON                           , 0x01,  21,   9,  14,  14, 0, 0, 0
    //, 0xFFFFFF, 0x000000, 0, BAGL_GLYPH_ICON_TRANSACTION_BADGE  }, NULL, 0, 0,
    //0, NULL, NULL, NULL },
    {{BAGL_LABELINE, 0x01, 0, 12, 128, 12, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     "Provide",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_LABELINE, 0x01, 0, 26, 128, 12, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     "public key?",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_LABELINE, 0x02, 0, 12, 128, 12, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     "Public Key",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_LABELINE, 0x02, 23, 26, 82, 12, 0x80 | 10, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 26},
     address_display_data,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
};

static inline char to_hexit(char in) {
    if (in < 0xA) {
        return in + '0';
    } else {
        return in - 0xA + 'A';
    }
}

static const struct bagl_element_e *prompt_address_prepro(const struct bagl_element_e *element);
void prompt_address(void *raw_bytes, uint32_t size, callback_t ok_cb, callback_t cxl_cb) {
    if (size > ADDRESS_BYTES) {
        uint16_t sz = 0x6B | (size & 0xFF);
        THROW(sz);
    }
    uint8_t *bytes = raw_bytes;
    uint32_t i;
    for (i = 0; i < size; i++) {
        address_display_data[i * 2] = to_hexit(bytes[i] >> 4);
        address_display_data[i * 2 + 1] = to_hexit(bytes[i] & 0xF);
    }
    address_display_data[i * 2] = '\0';
    ui_prompt(ui_display_address, sizeof(ui_display_address)/sizeof(*ui_display_address),
              ok_cb, cxl_cb, NULL, prompt_address_prepro);
    ux_step = 0;
    ux_step_count = 2;
}

const struct bagl_element_e *prompt_address_prepro(const struct bagl_element_e *element) {
    if (element->component.userid > 0) {
        unsigned int display = ux_step == element->component.userid - 1;
        if (display) {
            switch (element->component.userid) {
            case 1:
                UX_CALLBACK_SET_INTERVAL(2000);
                break;
            case 2:
                UX_CALLBACK_SET_INTERVAL(MAX(
                    3000, 1000 + bagl_label_roundtrip_duration_ms(element, 7)));
                break;
            }
        }
        return (void*)display;
    }
    return (void*)1;
}

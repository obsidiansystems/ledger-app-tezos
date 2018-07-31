#include "ui.h"

#include "baking_auth.h"
#include "keys.h"
#include "to_string.h"

#include <stdbool.h>
#include <string.h>

ux_state_t ux;
unsigned char G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B];

static callback_t ok_callback;
static callback_t cxl_callback;
static callback_t both_callback;

static unsigned button_handler(unsigned button_mask, unsigned button_mask_counter);
static bool do_nothing(void);

uint32_t ux_step, ux_step_count, ux_planned_step_count;

#define PROMPT_TIMEOUT 300 // 100 ms per tick
#define PROMPT_CYCLES 3
static uint32_t timeout_count;

static bool do_nothing(void) {
    return false;
}

static char idle_text[16];
char baking_auth_text[40];

const bagl_element_t ui_idle_screen[] = {
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

    //{{BAGL_ICON                           , 0x01,  21,   9,  14,  14, 0, 0, 0
    //, 0xFFFFFF, 0x000000, 0, BAGL_GLYPH_ICON_TRANSACTION_BADGE  }, NULL, 0, 0,
    //0, NULL, NULL, NULL },
    {{BAGL_LABELINE, 0x01, 0, 12, 128, 12, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
#ifdef BAKING_APP
     "Last Baked Level",
#else
     "Tezos",
#endif
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

#ifdef BAKING_APP
    {{BAGL_LABELINE, 0x01, 0, 26, 128, 12, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     idle_text,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
#endif

    {{BAGL_LABELINE, 0x02, 0, 12, 128, 12, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
#ifdef BAKING_APP
     "Baking Key",
#else
     "Wallet App",
#endif
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

#ifdef BAKING_APP
    {{BAGL_LABELINE, 0x02, 23, 26, 82, 12, 0x80 | 10, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 26},
     baking_auth_text,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
#endif
};

static void ui_idle(void) {
#ifdef BAKING_APP
    update_auth_text();
#endif
    ui_prompt(ui_idle_screen, sizeof(ui_idle_screen)/sizeof(*ui_idle_screen),
              do_nothing, exit_app, 2);
}

void change_idle_display(uint32_t new) {
    number_to_string(idle_text, new);
    update_auth_text();
}

void ui_initial_screen(void) {
#ifdef BAKING_APP
    change_idle_display(N_data.highest_level);
#endif
    ui_idle();
}

void cancel_pressed(void) {
    (void) button_handler(BUTTON_EVT_RELEASED | BUTTON_RIGHT, 0);
}

unsigned button_handler(unsigned button_mask, __attribute__((unused)) unsigned button_mask_counter) {
    callback_t callback;
    switch (button_mask) {
        case BUTTON_EVT_RELEASED | BUTTON_LEFT:
            callback = cxl_callback;
            break;
        case BUTTON_EVT_RELEASED | BUTTON_RIGHT:
            callback = ok_callback;
            break;
        case BUTTON_EVT_RELEASED | BUTTON_LEFT | BUTTON_RIGHT:
            callback = both_callback;
            break;
        default:
            return 0;
    }
    if (callback()) {
        ui_idle();
    }
    return 0;
}

const bagl_element_t *prepro(const bagl_element_t *element) {
    ux_step_count = ux_planned_step_count;

    // Always display elements with userid 0
    if (element->component.userid == 0) return element;

    if (ux_step == element->component.userid - 1) {
        UX_CALLBACK_SET_INTERVAL(MAX(2000, 1500 + bagl_label_roundtrip_duration_ms(element, 7)));
        return element;
    } else {
        return NULL;
    }
}

void ui_prompt(const bagl_element_t *elems, size_t sz, callback_t ok_c, callback_t cxl_c,
               uint32_t step_count) {
    // Adapted from definition of UX_DISPLAY in header file
    timeout_count = 0;
    ux_step = 0;
    ok_callback = ok_c;
    cxl_callback = cxl_c;
    both_callback = do_nothing;
    ux.elements = elems;
    ux.elements_count = sz;
    ux.button_push_handler = button_handler;
    ux.elements_preprocessor = prepro;
    ux_planned_step_count = step_count;
    UX_WAKE_UP();
    UX_REDISPLAY();
}

void set_both_callback(callback_t cb) {
    both_callback = cb;
}

unsigned char io_event(__attribute__((unused)) unsigned char channel) {
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
            if (ux.callback_interval_ms != 0) {
                ux.callback_interval_ms -= MIN(ux.callback_interval_ms, 100);
                if (ux.callback_interval_ms == 0) {
                    // prepare next screen
                    ux_step = (ux_step + 1) % ux_step_count;
                    if (cxl_callback != exit_app && ux_step == 0) {
                        timeout_count++;
                        if (timeout_count == PROMPT_CYCLES) {
                            cancel_pressed();
                            break;
                        }
                    }
                    // redisplay screen
                    UX_REDISPLAY();
                }
            }
        } else if (cxl_callback != exit_app) {
            if (timeout_count == PROMPT_TIMEOUT) {
                cancel_pressed();
            }
            timeout_count++;
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

__attribute__((noreturn))
bool exit_app(void) {
    BEGIN_TRY_L(exit) {
        TRY_L(exit) {
            os_sched_exit(-1);
        }
        FINALLY_L(exit) {
        }
    }
    END_TRY_L(exit);

    THROW(0); // Suppress warning
}

void ui_init(void) {
    UX_INIT();
    ok_callback = NULL;
    cxl_callback = NULL;
    idle_text[0] = '\0';
}

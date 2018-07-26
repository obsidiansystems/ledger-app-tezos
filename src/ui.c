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

static unsigned button_handler(unsigned button_mask, unsigned button_mask_counter);
static void do_nothing(void);

uint32_t ux_step, ux_step_count;

#define PROMPT_TIMEOUT 300 // 100 ms per tick
#define PROMPT_CYCLES 3
static uint32_t timeout_count;

static void do_nothing(void) {
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
              do_nothing, exit_app, two_screens_scroll_second_prepro);
}

void change_idle_display(uint32_t new) {
    uint32_t it = number_to_string(idle_text, new);
    idle_text[it] = '\0';
    update_auth_text();
}

void ui_initial_screen(void) {
#ifdef BAKING_APP
    change_idle_display(N_data.highest_level);
#endif
    ui_idle();
}

static void cancel_pressed(void) {
    cxl_callback();
    ui_idle();
}

static void ok_pressed(void) {
    ok_callback();
    ui_idle();
}

unsigned button_handler(unsigned button_mask, unsigned button_mask_counter) {
    switch (button_mask) {
        case BUTTON_EVT_RELEASED | BUTTON_LEFT:
            cancel_pressed();
            break;
        case BUTTON_EVT_RELEASED | BUTTON_RIGHT:
            ok_pressed();
            break;
        default:
            return 0;
    }
    return 0; // do not redraw the widget
}

// TODO: Rename this
const bagl_element_t *default_prepro(const bagl_element_t *elem) {
    ux_step_count = 0;
    return elem;
}

const bagl_element_t *two_screens_scroll_second_prepro(const bagl_element_t *element) {
    ux_step_count = 2;
    if (element->component.userid > 0) {
        unsigned int display = ux_step == element->component.userid - 1;
        if (display) {
            switch (element->component.userid) {
            case 1:
                UX_CALLBACK_SET_INTERVAL(2000);
                break;
            case 2:
                UX_CALLBACK_SET_INTERVAL(MAX(2000, 1500 + bagl_label_roundtrip_duration_ms(element, 7)));
                break;
            }
        }
        return (void*)display;
    }
    return (void*)1;
}

void ui_prompt(const bagl_element_t *elems, size_t sz, callback_t ok_c, callback_t cxl_c,
               bagl_element_callback_t prepro) {
    // Adapted from definition of UX_DISPLAY in header file
    timeout_count = 0;
    ux_step = 0;
    ok_callback = ok_c;
    cxl_callback = cxl_c;
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

void exit_app(void) {
    BEGIN_TRY_L(exit) {
        TRY_L(exit) {
            os_sched_exit(-1);
        }
        FINALLY_L(exit) {
        }
    }
    END_TRY_L(exit);
}

void ui_init(void) {
    UX_INIT();
    ok_callback = NULL;
    cxl_callback = NULL;
    idle_text[0] = '\0';
}

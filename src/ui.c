#include "ui.h"

#include "ui_menu.h"
#include "ui_prompt.h"

#include "baking_auth.h"
#include "globals.h"
#include "keys.h"
#include "memory.h"
#include "to_string.h"

#include <stdbool.h>
#include <string.h>

#define G global.ui

static unsigned button_handler(unsigned button_mask, unsigned button_mask_counter);

#define PROMPT_CYCLES 3

void require_pin(void) {
    bolos_ux_params_t params;
    memset(&params, 0, sizeof(params));
    params.ux_id = BOLOS_UX_VALIDATE_PIN;
    os_ux_blocking(&params);
}

#ifdef BAKING_APP
static const bagl_element_t ui_idle_screen[] = {
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
     "Last Block Level",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_LABELINE, 0x01, 0, 26, 128, 12, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     G.idle_text,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_LABELINE, 0x02, 0, 12, 128, 12, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     "Baking Key",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_LABELINE, 0x02, 23, 26, 82, 12, 0x80 | 10, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 26},
     G.baking_auth_text,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
};

static bool do_nothing(void) {
    return false;
}
#endif

static void update_baking_idle_screens(void) {
    number_to_string(G.idle_text, N_data.hwm.main.highest_level);

    if (N_data.bip32_path.length == 0) {
        strcpy(G.baking_auth_text, "No Key Authorized");
    } else {
        cx_ecfp_public_key_t const *const pubkey = generate_public_key(N_data.curve, &N_data.bip32_path);
        pubkey_to_pkh_string(
            G.baking_auth_text, sizeof(G.baking_auth_text),
            N_data.curve, pubkey);
    }
}

static void ui_idle(void) {
#ifdef BAKING_APP
    update_baking_idle_screens();
    ui_display(ui_idle_screen, NUM_ELEMENTS(ui_idle_screen),
               do_nothing, exit_app, 2);
#else
    G.cxl_callback = exit_app;
    main_menu();
#endif
}

void ui_initial_screen(void) {
#ifdef BAKING_APP
    update_baking_idle_screens();
#endif
    clear_ui_callbacks();
    ui_idle();
}

static bool is_idling(void) {
    return G.cxl_callback == exit_app;
}

static void timeout(void) {
    if (is_idling()) {
        // Idle app timeout
        update_baking_idle_screens();
        G.timeout_cycle_count = 0;
        UX_REDISPLAY();
    } else {
        // Prompt timeout -- simulate cancel button
        (void) button_handler(BUTTON_EVT_RELEASED | BUTTON_LEFT, 0);
    }
}

static unsigned button_handler(unsigned button_mask, __attribute__((unused)) unsigned button_mask_counter) {
    ui_callback_t callback;
    switch (button_mask) {
        case BUTTON_EVT_RELEASED | BUTTON_LEFT:
            callback = G.cxl_callback;
            break;
        case BUTTON_EVT_RELEASED | BUTTON_RIGHT:
            callback = G.ok_callback;
            break;
        default:
            return 0;
    }
    if (callback()) {
        clear_ui_callbacks();
        ui_idle();
    }
    return 0;
}

const bagl_element_t *prepro(const bagl_element_t *element) {
    if (element->component.userid == BAGL_STATIC_ELEMENT) return element;

    static const uint32_t pause_millis = 1500;
    uint32_t min = 2000;
    static const uint32_t div = 2;

    if (is_idling()) {
        min = 4000;
    }

    if (G.ux_step == element->component.userid - 1 || element->component.userid == BAGL_SCROLLING_ELEMENT) {
        // timeouts are in millis
        UX_CALLBACK_SET_INTERVAL(MAX(min,
                                     (pause_millis + bagl_label_roundtrip_duration_ms(element, 7)) / div));
        return element;
    } else {
        return NULL;
    }
}

void ui_display(const bagl_element_t *elems, size_t sz, ui_callback_t ok_c, ui_callback_t cxl_c,
                uint32_t step_count) {
    // Adapted from definition of UX_DISPLAY in header file
    G.timeout_cycle_count = 0;
    G.ux_step = 0;
    G.ux_step_count = step_count;
    G.ok_callback = ok_c;
    G.cxl_callback = cxl_c;
    if (!is_idling()) {
        switch_screen(0);
    }
    ux.elements = elems;
    ux.elements_count = sz;
    ux.button_push_handler = button_handler;
    ux.elements_preprocessor = prepro;
    UX_WAKE_UP();
    UX_REDISPLAY();
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
        if (ux.callback_interval_ms != 0) {
            ux.callback_interval_ms -= MIN(ux.callback_interval_ms, 100);
            if (ux.callback_interval_ms == 0) {
                // prepare next screen
                G.ux_step = (G.ux_step + 1) % G.ux_step_count;
                if (!is_idling()) {
                    switch_screen(G.ux_step);
                }

                // check if we've timed out
                if (G.ux_step == 0) {
                    G.timeout_cycle_count++;
                    if (G.timeout_cycle_count == PROMPT_CYCLES) {
                        timeout();
                        break; // timeout() will often display a new screen
                    }
                }

                // redisplay screen
                UX_REDISPLAY();
            }
        }
        break;
    default:
        // unknown events are acknowledged
        break;
    }

    // close the event if not done previously (by a display or whatever)
    if (!io_seproxyhal_spi_is_status_sent()) {
        io_seproxyhal_general_status();
    }
    // command has been processed, DO NOT reset the current APDU transport
    // TODO: I don't understand that comment or what this return value means
    return 1;
}

void io_seproxyhal_display(const bagl_element_t *element) {
    return io_seproxyhal_display_default((bagl_element_t *)element);
}

__attribute__((noreturn))
bool exit_app(void) {
#ifdef BAKING_APP
    require_pin();
#endif
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
}

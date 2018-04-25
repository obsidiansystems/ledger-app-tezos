#include "ui.h"
#include "main.h"

#include "idle_screen.h"
#include "error_screen.h"

#include <stdbool.h>

ux_state_t ux;

static void *cb_context;
static callback_t ok_callback;
static callback_t cxl_callback;
static bool idle;
static bool s_error;

static unsigned ui_idle_nanos_button(unsigned, unsigned);
static unsigned button_handler(unsigned button_mask, unsigned button_mask_counter);
static void do_nothing(void *context);

static void error(void) {
    s_error = true;
}

void check_idle() {
    if (idle && ok_callback != do_nothing) error();
    if (!idle && ok_callback == do_nothing) error();
}

void ui_init(void) {
    UX_INIT();
    idle = false;
    ok_callback = NULL;
    cxl_callback = NULL;
    s_error = false;
}

static void do_nothing(void *context) {
}

static void exit_app(void *context) {
    ui_exit();
}

static void ui_idle(void) {
    check_idle();
    ok_callback = do_nothing;
    cxl_callback = exit_app;
    if (!s_error) {
        UX_DISPLAY(ui_idle_nanos, NULL);
    }
    idle = true;
    check_idle();
}

void ui_initial_screen(void) {
    ui_idle();
}

unsigned ui_idle_nanos_button(unsigned button_mask, unsigned button_mask_counter) {
    return button_handler(button_mask, button_mask_counter);
}

unsigned button_handler(unsigned button_mask, unsigned button_mask_counter) {
    check_idle();
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
    if (s_error) {
        UX_DISPLAY(ui_error, NULL);
    }
    return 0; // do not redraw the widget
}

void prompt(const bagl_element_t *elems, size_t sz, callback_t ok_c, callback_t cxl_c, void *cxt) {
    check_idle();
    ok_callback = ok_c;
    cxl_callback = cxl_c;
    cb_context = cxt;
    idle = false;
    check_idle();
    ux.elements = elems;
    ux.elements_count = sz;
    ux.button_push_handler = button_handler;
    ux.elements_preprocessor = NULL;
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

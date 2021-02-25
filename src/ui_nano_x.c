#include "bolos_target.h"

#include "ui.h"

#include "baking_auth.h"
#include "exception.h"
#include "globals.h"
#include "glyphs.h" // ui-menu
#include "keys.h"
#include "memory.h"
#include "os_cx.h" // ui-menu
#include "to_string.h"

#include <stdbool.h>
#include <string.h>


#define G global.ui

void display_next_state(bool is_upper_border);

void ui_refresh(void) {
    ux_stack_display(0);
}

// CALLED BY THE SDK
unsigned char io_event(unsigned char channel);

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

    case SEPROXYHAL_TAG_STATUS_EVENT:
        if (G_io_apdu_media == IO_APDU_MEDIA_USB_HID && !(U4BE(G_io_seproxyhal_spi_buffer, 3) & SEPROXYHAL_TAG_STATUS_EVENT_FLAG_USB_POWERED)) {
            THROW(EXCEPTION_IO_RESET);
        }
        // no break is intentional
    default:
        UX_DEFAULT_EVENT();
        break;

    case SEPROXYHAL_TAG_DISPLAY_PROCESSED_EVENT:
        UX_DISPLAYED_EVENT({});
        break;

    case SEPROXYHAL_TAG_TICKER_EVENT:
#       ifdef BAKING_APP
            // Disable ticker event handling to prevent screen saver from starting.
#       else
            UX_TICKER_EVENT(G_io_seproxyhal_spi_buffer, {});
#       endif

        break;
    }

    // close the event if not done previously (by a display or whatever)
    if (!io_seproxyhal_spi_is_status_sent()) {
        io_seproxyhal_general_status();
    }

    // command has been processed, DO NOT reset the current APDU transport
    return 1;
}

UX_STEP_INIT(
    ux_init_upper_border,
    NULL,
    NULL,
    {
        display_next_state(true);
    });
UX_STEP_NOCB(
    ux_variable_display, 
    bnnn_paging,
    {
      .title = global.dynamic_display.screen_title,
      .text = global.dynamic_display.screen_value,
    });
UX_STEP_INIT(
    ux_init_lower_border,
    NULL,
    NULL,
    {
        display_next_state(false);
    });

UX_STEP_NOCB(
    ux_app_is_ready_step,
    nn,
    {
      "Application",
      "is ready",
    });

UX_STEP_CB(
    ux_idle_quit_step,
    pb,
    exit_app(),
    {
      &C_icon_dashboard_x,
      "Quit",
    });

UX_FLOW(ux_idle_flow,
    &ux_app_is_ready_step,

    &ux_init_upper_border,
    &ux_variable_display,
    &ux_init_lower_border,

    &ux_idle_quit_step,
    FLOW_LOOP
);

static void prompt_response(bool const accepted) {
    ui_initial_screen();
    if (accepted) {
        global.dynamic_display.ok_callback();
    } else {
        global.dynamic_display.cxl_callback();
    }
}

UX_STEP_CB(
    ux_prompt_flow_accept_step,
    pb,
    prompt_response(true),
    {
        &C_icon_validate_14,
        "Accept"
    });

UX_STEP_CB(
    ux_prompt_flow_reject_step,
    pb,
    prompt_response(false),
    {
        &C_icon_crossmark,
        "Reject"
    });

UX_STEP_NOCB(
    ux_eye_step,
    nn,
    {
      "Hello",
      "World",
    });

UX_FLOW(ux_confirm_flow,
  &ux_eye_step,

  &ux_init_upper_border,
  &ux_variable_display,
  &ux_init_lower_border,

  &ux_prompt_flow_reject_step,
  &ux_prompt_flow_accept_step
);

void clear_data() {
    explicit_bzero(&global.dynamic_display.screen_title, sizeof(global.dynamic_display.screen_title));
    explicit_bzero(&global.dynamic_display.screen_value, sizeof(global.dynamic_display.screen_value));
}

void set_state_data() {
    struct screen_data *fmt = &global.dynamic_display.screen_stack[global.dynamic_display.formatter_index];
    clear_data();
    copy_string((char *)global.dynamic_display.screen_title, sizeof(global.dynamic_display.screen_title), fmt->title);
    fmt->callback_fn(global.dynamic_display.screen_value, sizeof(global.dynamic_display.screen_value), fmt->data);
}

/*
* Enables coherent behavior on bnnn_paging when there are multiple
* screens.
*/
void update_layout() {
    G_ux.flow_stack[G_ux.stack_count - 1].prev_index =
    G_ux.flow_stack[G_ux.stack_count - 1].index - 2;
    G_ux.flow_stack[G_ux.stack_count - 1].index--;
    ux_flow_relayout();
}

void display_next_state(bool is_upper_border) {
    if (is_upper_border) {
        if (global.dynamic_display.current_state == OUT_OF_BORDERS) {
            global.dynamic_display.current_state = IN_BORDERS;
            global.dynamic_display.formatter_index = 0;
            set_state_data();
            ux_flow_next();
        } else {
            // Go back to first screen
            if (global.dynamic_display.formatter_index == 0) {
                global.dynamic_display.current_state = OUT_OF_BORDERS;
                ux_flow_prev();
            } else {
                global.dynamic_display.formatter_index--;
                set_state_data();
                ux_flow_next();
            }
        }
    } else {
        if (global.dynamic_display.current_state == OUT_OF_BORDERS) {
            global.dynamic_display.current_state = IN_BORDERS;
            global.dynamic_display.formatter_index = global.dynamic_display.screen_stack_size - 1;
            set_state_data();
            ux_flow_prev();
        } else {
            global.dynamic_display.formatter_index++;
            if (global.dynamic_display.formatter_index == global.dynamic_display.screen_stack_size) {
                global.dynamic_display.current_state = OUT_OF_BORDERS;
                ux_flow_next();
            } else {
                set_state_data();
                update_layout(); // Similar to `ux_flow_prev()` but updates layout to account for `bnnn_paging`'s weird behaviour.
            }
        }
    }
}

void ui_initial_screen(void) {
    // reserve a display stack slot if none yet
    if(G_ux.stack_count == 0) {
        ux_stack_push();
    }

    init_screen_stack();
#   ifdef BAKING_APP
        calculate_baking_idle_screens_data();
#   else
        push_ui_callback("Tezos Wallet", copy_string, VERSION);
#   endif

    ux_idle_screen(NULL, NULL);
}

void ux_prepare_display(ui_callback_t ok_c, ui_callback_t cxl_c) {
    global.dynamic_display.screen_stack_size = global.dynamic_display.formatter_index;
    global.dynamic_display.formatter_index = 0;
    global.dynamic_display.current_state = OUT_OF_BORDERS;

    if (ok_c)
        global.dynamic_display.ok_callback = ok_c;
    if (cxl_c)
        global.dynamic_display.cxl_callback = cxl_c;
}

void ux_confirm_screen(ui_callback_t ok_c, ui_callback_t cxl_c) {
    ux_prepare_display(ok_c, cxl_c);
    ux_flow_init(0, ux_confirm_flow, NULL);
    THROW(ASYNC_EXCEPTION);
}

void ux_idle_screen(ui_callback_t ok_c, ui_callback_t cxl_c) {
    ux_prepare_display(ok_c, cxl_c);
    ux_flow_init(0, ux_idle_flow, NULL);
}

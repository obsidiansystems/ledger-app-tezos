#include "bolos_target.h"

#include "ui.h"

#include "baking_auth.h"
#include "exception.h"
#include "globals.h"
#include "glyphs.h"  // ui-menu
#include "keys.h"
#include "memory.h"
#include "os_cx.h"  // ui-menu
#include "to_string.h"

#include <stdbool.h>
#include <string.h>

#define G global.ui

void display_next_state(bool is_left_ux_step);

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
            if (G_io_apdu_media == IO_APDU_MEDIA_USB_HID &&
                !(U4BE(G_io_seproxyhal_spi_buffer, 3) &
                  SEPROXYHAL_TAG_STATUS_EVENT_FLAG_USB_POWERED)) {
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
#ifdef BAKING_APP
            // Disable ticker event handling to prevent screen saver from starting.
#else
            UX_TICKER_EVENT(G_io_seproxyhal_spi_buffer, {});
#endif

            break;
    }

    // close the event if not done previously (by a display or whatever)
    if (!io_seproxyhal_spi_is_status_sent()) {
        io_seproxyhal_general_status();
    }

    // command has been processed, DO NOT reset the current APDU transport
    return 1;
}

UX_STEP_INIT(ux_init_upper_border, NULL, NULL, { display_next_state(true); });
UX_STEP_NOCB(ux_variable_display,
             bnnn_paging,
             {
                 .title = global.dynamic_display.screen_title,
                 .text = global.dynamic_display.screen_value,
             });
UX_STEP_INIT(ux_init_lower_border, NULL, NULL, { display_next_state(false); });

UX_STEP_NOCB(ux_app_is_ready_step,
             nn,
             {
                 "Application",
                 "is ready",
             });

UX_STEP_CB(ux_idle_quit_step,
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
        FLOW_LOOP);

static void prompt_response(bool const accepted) {
    ui_initial_screen();
    if (accepted) {
        global.dynamic_display.ok_callback();
    } else {
        global.dynamic_display.cxl_callback();
    }
}

UX_STEP_CB(ux_prompt_flow_accept_step, pb, prompt_response(true), {&C_icon_validate_14, "Accept"});

UX_STEP_CB(ux_prompt_flow_reject_step, pb, prompt_response(false), {&C_icon_crossmark, "Reject"});

UX_STEP_NOCB(ux_eye_step,
             nn,
             {
                 "Review",
                 "Request",
             });

UX_FLOW(ux_confirm_flow,
        &ux_eye_step,

        &ux_init_upper_border,
        &ux_variable_display,
        &ux_init_lower_border,

        &ux_prompt_flow_reject_step,
        &ux_prompt_flow_accept_step);

#define G_display global.dynamic_display

void clear_data() {
    explicit_bzero(&G_display.screen_title, sizeof(G_display.screen_title));
    explicit_bzero(&G_display.screen_value, sizeof(G_display.screen_value));
}

// Fills the screen with the data in the `screen_stack` pointed by the index
// `G_display.formatter_index`. Fills the `screen_title` by copying the `.title` field and fills the
// `screen_value` by computing `callback_fn` with the `.data` field as a parameter
void set_screen_data() {
    struct screen_data *fmt = &G_display.screen_stack[G_display.formatter_index];
    if (fmt->title == NULL) {
      // Avoid seg faulting for bad reasons...
      G_display.formatter_index = 0;
      fmt = &G_display.screen_stack[0];
    }
    clear_data();
    copy_string((char *) G_display.screen_title, sizeof(G_display.screen_title), fmt->title);
    fmt->callback_fn(G_display.screen_value, sizeof(G_display.screen_value), fmt->data);
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

void display_next_state(bool is_left_ux_step) {
    if (is_left_ux_step) {  // We're called from the LEFT ux step
        if (G_display.current_state == STATIC_SCREEN) {
            // The previous screen was a static screen, so we're now entering the stack (from the
            // start of the stack).

            // Since we're in the stack, set the state to DYNAMIC_SCREEN.
            G_display.current_state = DYNAMIC_SCREEN;
            // We're just entering the stack so the `formatter_index` is set to 0.
            G_display.formatter_index = 0;
            // Update the screen data.
            set_screen_data();
            // Move to the next step, which will display the screen.
            ux_flow_next();
        } else {
            // The previous screen was NOT a static screen, so we were already in the stack.
            if (G_display.formatter_index == 0) {
                // If `formatter_index` is 0 then we need to exit the dynamic display.

                // Update the current_state accordingly.
                G_display.current_state = STATIC_SCREEN;
                // Don't need to update the screen data, simply display the ux_flow_prev() which
                // will be a static screen.
                ux_flow_prev();
            } else {
                // The user is still browsing the stack: simply decrement `formatter_index` and
                // update `screen_data`.
                G_display.formatter_index--;
                set_screen_data();
                ux_flow_next();
            }
        }
    } else {
        // We're called from the RIGHT ux step.
        if (G_display.current_state == STATIC_SCREEN) {
            // We're called from a static screen, so we're now entering the stack (from the end of
            // the stack).

            // Update the current_state.
            G_display.current_state = DYNAMIC_SCREEN;
            // We're starting the stack from the end, so the index is the size - 1.
            G_display.formatter_index = G_display.screen_stack_size - 1;
            // Update the screen data.
            set_screen_data();
            // Since we're called from the RIGHT ux step, if we wish to display
            // the data we need to call ux_flow_PREV and not `ux_flow_NEXT`.
            ux_flow_prev();
        } else {
            // We're being called from a dynamic screen, so the user was already browsing the stack.
            // The user wants to go to the next screen so increment the index.
            G_display.formatter_index++;
            if (G_display.formatter_index == G_display.screen_stack_size) {
                // Index is equal to stack size, so time to exit the stack and display the static
                // screens.

                // Make sure we update the state accordingly.
                G_display.current_state = STATIC_SCREEN;
                ux_flow_next();
            } else {
                // We're just browsing the stack so update the screen and display it.
                set_screen_data();
                // Similar to `ux_flow_prev()` but updates layout to account for `bnnn_paging`'s
                // weird behaviour.
                update_layout();
            }
        }
    }
}

void ui_initial_screen(void) {
    // reserve a display stack slot if none yet
    if (G_ux.stack_count == 0) {
        ux_stack_push();
    }

    init_screen_stack();
#ifdef BAKING_APP
    calculate_baking_idle_screens_data();
#else
    push_ui_callback("Tezos Wallet", copy_string, VERSION);
#endif

    ux_idle_screen(NULL, NULL);
}

void ux_prepare_display(ui_callback_t ok_c, ui_callback_t cxl_c) {
    global.dynamic_display.screen_stack_size = global.dynamic_display.formatter_index;
    global.dynamic_display.formatter_index = 0;
    global.dynamic_display.current_state = STATIC_SCREEN;

    if (ok_c) global.dynamic_display.ok_callback = ok_c;
    if (cxl_c) global.dynamic_display.cxl_callback = cxl_c;
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

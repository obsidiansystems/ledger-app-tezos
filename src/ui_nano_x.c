#include "bolos_target.h"

#ifdef TARGET_NANOX

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
        UX_TICKER_EVENT(G_io_seproxyhal_spi_buffer, {});
        break;
    }

    // close the event if not done previously (by a display or whatever)
    if (!io_seproxyhal_spi_is_status_sent()) {
        io_seproxyhal_general_status();
    }

    // command has been processed, DO NOT reset the current APDU transport
    return 1;
}

UX_STEP_NOCB(
    ux_idle_flow_1_step,
    bnn,
    {
      "Tezos Wallet",
      VERSION,
      COMMIT
    });
UX_STEP_CB(
    ux_idle_flow_2_step,
    pb,
    exit_app(),
    {
      &C_icon_dashboard,
      "Quit",
    });

UX_FLOW(ux_idle_flow,
    &ux_idle_flow_1_step,
    &ux_idle_flow_2_step
);



// prompt
#define PROMPT_SCREEN_TPL(idx) \
    UX_STEP_NOCB( \
        ux_prompt_flow_ ## idx ## _step, \
        bnnn_paging, \
        { \
            .title = G.prompt.screen[idx].prompt, \
            .text = G.prompt.screen[idx].value, \
        })

PROMPT_SCREEN_TPL(0);
PROMPT_SCREEN_TPL(1);
PROMPT_SCREEN_TPL(2);
PROMPT_SCREEN_TPL(3);
PROMPT_SCREEN_TPL(4);
PROMPT_SCREEN_TPL(5);
PROMPT_SCREEN_TPL(6);

static void prompt_response(bool const accepted) {
    ux_flow_init(0, ux_idle_flow, NULL);
    if (accepted) {
        G.ok_callback();
    } else {
        G.cxl_callback();
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


// yes ridiculous
UX_FLOW(ux_prompt_1_flow,
    &ux_prompt_flow_0_step,
    &ux_prompt_flow_reject_step,
    &ux_prompt_flow_accept_step
);
UX_FLOW(ux_prompt_2_flow,
    &ux_prompt_flow_0_step,
    &ux_prompt_flow_1_step,
    &ux_prompt_flow_reject_step,
    &ux_prompt_flow_accept_step
);
UX_FLOW(ux_prompt_3_flow,
    &ux_prompt_flow_0_step,
    &ux_prompt_flow_1_step,
    &ux_prompt_flow_2_step,
    &ux_prompt_flow_reject_step,
    &ux_prompt_flow_accept_step
);
UX_FLOW(ux_prompt_4_flow,
    &ux_prompt_flow_0_step,
    &ux_prompt_flow_1_step,
    &ux_prompt_flow_2_step,
    &ux_prompt_flow_3_step,
    &ux_prompt_flow_reject_step,
    &ux_prompt_flow_accept_step
);
UX_FLOW(ux_prompt_5_flow,
    &ux_prompt_flow_0_step,
    &ux_prompt_flow_1_step,
    &ux_prompt_flow_2_step,
    &ux_prompt_flow_3_step,
    &ux_prompt_flow_4_step,
    &ux_prompt_flow_reject_step,
    &ux_prompt_flow_accept_step
);
UX_FLOW(ux_prompt_6_flow,
    &ux_prompt_flow_0_step,
    &ux_prompt_flow_1_step,
    &ux_prompt_flow_2_step,
    &ux_prompt_flow_3_step,
    &ux_prompt_flow_4_step,
    &ux_prompt_flow_5_step,
    &ux_prompt_flow_reject_step,
    &ux_prompt_flow_accept_step
);
UX_FLOW(ux_prompt_7_flow,
    &ux_prompt_flow_0_step,
    &ux_prompt_flow_1_step,
    &ux_prompt_flow_2_step,
    &ux_prompt_flow_3_step,
    &ux_prompt_flow_4_step,
    &ux_prompt_flow_5_step,
    &ux_prompt_flow_6_step,
    &ux_prompt_flow_reject_step,
    &ux_prompt_flow_accept_step
);


void ui_initial_screen(void) {
    // reserve a display stack slot if none yet
    if(G_ux.stack_count == 0) {
        ux_stack_push();
    }
    ux_flow_init(0, ux_idle_flow, NULL);
// #ifdef BAKING_APP
//     update_baking_idle_screens();
// #endif
//     clear_ui_callbacks();
//     ui_idle();
}

__attribute__((noreturn))
void ui_prompt(const char *const *labels, const char *const *data, ui_callback_t ok_c, ui_callback_t cxl_c) {
    check_null(labels);

    size_t i;
    for (i = 0; labels[i] != NULL; i++) {
        const char *const label = (const char *)PIC(labels[i]);
        if (i >= MAX_SCREEN_COUNT || strlen(label) > PROMPT_WIDTH) THROW(EXC_MEMORY_ERROR);

        strcpy(G.prompt.screen[i].prompt, label);

        if (data == NULL) {
            G.prompt.callbacks[i](G.prompt.screen[i].value, sizeof(G.prompt.screen[i].value), global.ui.prompt.callback_data[i]);
        } else {
            copy_string(G.prompt.screen[i].value, sizeof(G.prompt.screen[i].value), data[i]);
        }
    }
    size_t const screen_count = i;


    G.ok_callback = ok_c;
    G.cxl_callback = cxl_c;
    switch (screen_count) {
        case 1: ux_flow_init(0, ux_prompt_1_flow, NULL); break;
        case 2: ux_flow_init(0, ux_prompt_2_flow, NULL); break;
        case 3: ux_flow_init(0, ux_prompt_3_flow, NULL); break;
        case 4: ux_flow_init(0, ux_prompt_4_flow, NULL); break;
        case 5: ux_flow_init(0, ux_prompt_5_flow, NULL); break;
        case 6: ux_flow_init(0, ux_prompt_6_flow, NULL); break;
        case 7: ux_flow_init(0, ux_prompt_7_flow, NULL); break;
        default: THROW(EXC_WRONG_LENGTH);
    }

    THROW(ASYNC_EXCEPTION);
}

#endif // #ifdef TARGET_NANOX

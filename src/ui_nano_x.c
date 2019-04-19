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
#define PROMPT_SCREEN_NAME(idx) ux_prompt_flow_ ## idx ## _step
#define PROMPT_SCREEN_TPL(idx) \
    UX_STEP_NOCB( \
        PROMPT_SCREEN_NAME(idx), \
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

UX_FLOW(ux_prompts_flow,
    &PROMPT_SCREEN_NAME(0),
    &PROMPT_SCREEN_NAME(1),
    &PROMPT_SCREEN_NAME(2),
    &PROMPT_SCREEN_NAME(3),
    &PROMPT_SCREEN_NAME(4),
    &PROMPT_SCREEN_NAME(5),
    &PROMPT_SCREEN_NAME(6),
    &ux_prompt_flow_reject_step,
    &ux_prompt_flow_accept_step
);
_Static_assert(NUM_ELEMENTS(ux_prompts_flow) - 3 /*reject + accept + end*/ == MAX_SCREEN_COUNT, "ux_prompts_flow doesn't have the same number of screens as MAX_SCREEN_COUNT");


void ui_initial_screen(void) {
#   ifdef BAKING_APP
        update_baking_idle_screens();
#   endif

    // reserve a display stack slot if none yet
    if(G_ux.stack_count == 0) {
        ux_stack_push();
    }
    ux_flow_init(0, ux_idle_flow, NULL);
}

__attribute__((noreturn))
void ui_prompt(const char *const *labels, ui_callback_t ok_c, ui_callback_t cxl_c) {
    check_null(labels);

    size_t const screen_count = ({
        size_t i = 0;
        while (i < MAX_SCREEN_COUNT && labels[i] != NULL) { i++; }
        i;
    });

    // We fill the destination buffers at the end instead of the beginning so we can
    // use the same array for any number of screens.
    size_t const offset = MAX_SCREEN_COUNT - screen_count;
    for (size_t i = 0; labels[i] != NULL; i++) {
        const char *const label = (const char *)PIC(labels[i]);
        if (strlen(label) > sizeof(G.prompt.screen[i].prompt)) THROW(EXC_MEMORY_ERROR);
        strcpy(G.prompt.screen[offset + i].prompt, label);

        G.prompt.callbacks[i](G.prompt.screen[offset + i].value, sizeof(G.prompt.screen[offset + i].value), G.prompt.callback_data[i]);
    }

    G.ok_callback = ok_c;
    G.cxl_callback = cxl_c;
    ux_flow_init(0, &ux_prompts_flow[offset], NULL);
    THROW(ASYNC_EXCEPTION);
}

#endif // #ifdef TARGET_NANOX

#include "bolos_target.h"

#ifndef TARGET_NANOX

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

void ui_refresh(void) {
    // DO NOTHING
}

// CALLED BY THE SDK
unsigned char io_event(unsigned char channel);
void io_seproxyhal_display(const bagl_element_t *element);


static void ui_display(const bagl_element_t *elems, size_t sz, ui_callback_t ok_c, ui_callback_t cxl_c,
                uint32_t step_count);


// ----------------------------- ui_prompt
// This is called by internal UI code to implement buffering
static void switch_screen(uint32_t which);
// This is called by internal UI code to prevent callbacks from sticking around
static void clear_ui_callbacks(void);

// ------------------------------- ui_meno
static void main_menu(void);


static unsigned button_handler(unsigned button_mask, unsigned button_mask_counter);

#define PROMPT_CYCLES 3

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
     G.baking_idle_screens.hwm,
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
     G.baking_idle_screens.pkh,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_LABELINE, 0x03, 0, 12, 128, 12, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     "Chain",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_LABELINE, 0x03, 23, 26, 82, 12, 0x80 | 10, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 26},
     G.baking_idle_screens.chain,
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

static void ui_idle(void) {
#   ifdef BAKING_APP
        update_baking_idle_screens();
        ui_display(
            ui_idle_screen, NUM_ELEMENTS(ui_idle_screen),
            do_nothing, exit_app, 3);
#   else
        G.cxl_callback = exit_app;
        main_menu();
#   endif
}

void ui_initial_screen(void) {
#   ifdef BAKING_APP
        update_baking_idle_screens();
#   endif
    clear_ui_callbacks();
    ui_idle();
}

static bool is_idling(void) {
    return G.cxl_callback == exit_app;
}

static void timeout(void) {
    if (is_idling()) {
        // Idle app timeout
#       ifdef BAKING_APP
            update_baking_idle_screens();
#       endif
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
#if CX_APILEVEL < 10 
    ux.elements = elems;
    ux.elements_count = sz;
    ux.button_push_handler = button_handler;
    ux.elements_preprocessor = prepro;
#else
    ux.stack[0].element_arrays[0].element_array = elems;
    ux.stack[0].element_arrays[0].element_array_count = sz;
    ux.stack[0].element_arrays_count=1;
    ux.stack[0].button_push_callback = button_handler;
    G_ux.stack[0].screen_before_element_display_callback = prepro;
#endif
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
            ux.callback_interval_ms -= MIN(ux.callback_interval_ms, 100u);
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


#pragma mark uiprompt

static const bagl_element_t ui_multi_screen[] = {
    {{BAGL_RECTANGLE, BAGL_STATIC_ELEMENT, 0, 0, 128, 32, 0, 0, BAGL_FILL, 0x000000, 0xFFFFFF,
      0, 0},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_ICON, BAGL_STATIC_ELEMENT, 3, 12, 7, 7, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
      BAGL_GLYPH_ICON_CROSS},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_ICON, BAGL_STATIC_ELEMENT, 117, 13, 8, 6, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
      BAGL_GLYPH_ICON_CHECK},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_LABELINE, BAGL_STATIC_ELEMENT, 0, 12, 128, 12, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     global.ui.prompt.active_prompt,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_LABELINE, BAGL_SCROLLING_ELEMENT, 23, 26, 82, 12, 0x80 | 10, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 26},
     global.ui.prompt.active_value,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
};

void switch_screen(uint32_t which) {
    if (which >= MAX_SCREEN_COUNT) THROW(EXC_MEMORY_ERROR);
    const char *label = (const char*)PIC(global.ui.prompt.prompts[which]);

    strncpy(global.ui.prompt.active_prompt, label, sizeof(global.ui.prompt.active_prompt));
    if (global.ui.prompt.callbacks[which] == NULL) THROW(EXC_MEMORY_ERROR);
    global.ui.prompt.callbacks[which](
        global.ui.prompt.active_value, sizeof(global.ui.prompt.active_value),
        global.ui.prompt.callback_data[which]);
}

void clear_ui_callbacks(void) {
    for (int i = 0; i < MAX_SCREEN_COUNT; ++i) {
        global.ui.prompt.callbacks[i] = NULL;
    }
}

__attribute__((noreturn))
void ui_prompt(const char *const *labels, ui_callback_t ok_c, ui_callback_t cxl_c) {
    check_null(labels);
    global.ui.prompt.prompts = labels;

    size_t i;
    for (i = 0; labels[i] != NULL; i++) {
        const char *label = (const char *)PIC(labels[i]);
        if (i >= MAX_SCREEN_COUNT || strlen(label) > PROMPT_WIDTH) THROW(EXC_MEMORY_ERROR);
    }
    size_t screen_count = i;

    ui_display(ui_multi_screen, NUM_ELEMENTS(ui_multi_screen),
               ok_c, cxl_c, screen_count);
    THROW(ASYNC_EXCEPTION);
}


#pragma mark ui_menu


#ifndef BAKING_APP

void exit_app_cb(__attribute__((unused)) unsigned int cb) {
    exit_app();
}

// Mutually recursive static variables require forward declarations
static const ux_menu_entry_t main_menu_data[];
static const ux_menu_entry_t about_menu_data[];

static const ux_menu_entry_t about_menu_data[] = {
    {NULL, NULL, 0, NULL, "Tezos Wallet", "Version " VERSION, 0, 0},
    {main_menu_data, NULL, 1, &C_icon_back, "Back", NULL, 61, 40}, // TODO: Put icon for "back" in
    UX_MENU_END
};

static const ux_menu_entry_t main_menu_data[] = {
    {NULL, NULL, 0, NULL, "Use wallet to", "view accounts", 0, 0},
    {about_menu_data, NULL, 0, NULL, "About", NULL, 0, 0},
    {NULL, exit_app_cb, 0, &C_icon_dashboard, "Quit app", NULL, 50, 29}, // TODO: Put icon for "dashboard" in
    UX_MENU_END
};

void main_menu() {
    UX_MENU_DISPLAY(0, main_menu_data, NULL);
}
#endif // #ifdef BAKING_APP

#endif // #ifndef TARGET_NANOX

#pragma once

#include "os_io_seproxyhal.h"

typedef void (*callback_t)(void *);

extern ux_state_t ux; // TODO: Do we need this?

void ui_initial_screen(void);
void ui_init(void);
void ui_prompt(const bagl_element_t *elems, size_t sz, callback_t ok_c, callback_t cxl_c,
               void *context, bagl_element_callback_t prepro);
unsigned char io_event(unsigned char channel); // TODO: Who calls this? How?
void io_seproxyhal_display(const bagl_element_t *element);
void ui_display_error(void);
void prompt_address(void *raw_bytes, uint32_t size, callback_t ok_cb, callback_t cxl_cb);

const bagl_element_t *timer_setup (const bagl_element_t *elem);

// Helper for simple situations
#define UI_PROMPT(elems, ok_c, cxl_c) ui_prompt(elems, \
                                                sizeof(elems) / sizeof(elems[0]), \
                                                ok_c, cxl_c, NULL, timer_setup)

#pragma once

#include "os_io_seproxyhal.h"

typedef void (*callback_t)();

extern uint32_t ux_step, ux_step_count;

void ui_initial_screen(void);
void ui_init(void);
void exit_app(void);

void ui_prompt(const bagl_element_t *elems, size_t sz, callback_t ok_c, callback_t cxl_c,
               bagl_element_callback_t prepro);
unsigned char io_event(unsigned char channel); // TODO: Who calls this? How?
void io_seproxyhal_display(const bagl_element_t *element);
void change_idle_display(uint32_t new);

const bagl_element_t *timer_setup (const bagl_element_t *elem);

extern char baking_auth_text[40];

// Helper for simple situations
#define UI_PROMPT(elems, ok_c, cxl_c) \
    ux_step = 0; \
    ui_prompt(elems, \
              sizeof(elems) / sizeof(elems[0]), \
              ok_c, cxl_c, timer_setup)

#pragma once

#include "os_io_seproxyhal.h"

#include <stdbool.h>

typedef bool (*callback_t)(void); // return true to go back to idle screen

extern uint32_t ux_step, ux_step_count;

void ui_initial_screen(void);
void ui_init(void);
__attribute__((noreturn))
bool exit_app(void);

void ui_prompt(const bagl_element_t *elems, size_t sz, callback_t ok_c, callback_t cxl_c,
               uint32_t step_count);
unsigned char io_event(unsigned char channel);
void io_seproxyhal_display(const bagl_element_t *element);
void change_idle_display(uint32_t new);

extern char baking_auth_text[40]; // TODO: Is this the right name?

// Helper for simple situations
#define UI_PROMPT(elems, ok_c, cxl_c) \
    ux_step = 0; \
    ui_prompt(elems, \
              sizeof(elems) / sizeof(elems[0]), \
              ok_c, cxl_c, 0)

#pragma once

#include "os_io_seproxyhal.h"

#include <stdbool.h>

#include "keys.h"

typedef bool (*callback_t)(void); // return true to go back to idle screen

void ui_initial_screen(void);
void ui_init(void);
__attribute__((noreturn))
bool exit_app(void); // Might want to send it arguments to use as callback

void ui_display(const bagl_element_t *elems, size_t sz, callback_t ok_c, callback_t cxl_c,
                uint32_t step_count);
unsigned char io_event(unsigned char channel);
void io_seproxyhal_display(const bagl_element_t *element);
void change_idle_display(uint32_t new);

extern char baking_auth_text[PKH_STRING_SIZE]; // TODO: Is this the right name?

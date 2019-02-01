#pragma once

#include "os_io_seproxyhal.h"

#include "types.h"

#include "keys.h"

#define BAGL_STATIC_ELEMENT 0
#define BAGL_SCROLLING_ELEMENT 100 // Arbitrary value chosen to connect data structures with prepro func

void ui_initial_screen(void);
void ui_init(void);
__attribute__((noreturn))
bool exit_app(void); // Might want to send it arguments to use as callback

void ui_display(const bagl_element_t *elems, size_t sz, ui_callback_t ok_c, ui_callback_t cxl_c,
                uint32_t step_count);
unsigned char io_event(unsigned char channel);
void io_seproxyhal_display(const bagl_element_t *element);

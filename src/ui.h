#pragma once

#include "os_io_seproxyhal.h"

typedef void (*callback_t)(void *);

extern ux_state_t ux; // TODO: Do we need this?

void ui_initial_screen(void);
void ui_init(void);
void prompt(const bagl_element_t *elems, size_t sz, callback_t ok_c, callback_t cxl_c, void *context);
unsigned char io_event(unsigned char channel); // TODO: Who calls this? How?
void io_seproxyhal_display(const bagl_element_t *element);

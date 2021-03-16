#pragma once

#include "os_io_seproxyhal.h"

#include "types.h"

#include "keys.h"

void ui_initial_screen(void);
void ui_init(void);
void ui_refresh(void);

__attribute__((noreturn)) bool exit_app(void); // Might want to send it arguments to use as callback

__attribute__((noreturn))
void ux_confirm_screen(ui_callback_t ok_c, ui_callback_t cxl_c);

void ux_idle_screen(ui_callback_t ok_c, ui_callback_t cxl_c);


/* Initializes the formatter stack. Should be called once before calling `push_ui_callback()`. */
void init_screen_stack();
/* User MUST call `init_screen_stack()` before calling this function for the first time. */
void push_ui_callback(char *title, string_generation_callback cb, void *data);
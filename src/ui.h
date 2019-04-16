#pragma once

#include "os_io_seproxyhal.h"

#include "types.h"

#include "keys.h"

#define BAGL_STATIC_ELEMENT 0
#define BAGL_SCROLLING_ELEMENT 100 // Arbitrary value chosen to connect data structures with prepro func

void ui_initial_screen(void);
void ui_init(void);

__attribute__((noreturn)) bool exit_app(void); // Might want to send it arguments to use as callback

/// ui-prompt--------------------------

// Displays labels (terminated with a NULL pointer) associated with data
// labels must be completely static string constants
// data may be dynamic
// Alternatively, if data is NULL, assume we've registered appropriate callbacks to generate the data
// All pointers may be unrelocated
__attribute__((noreturn))
void ui_prompt(const char *const *labels, const char *const *data, ui_callback_t ok_c, ui_callback_t cxl_c);
// STAY THE SAME


// This function registers how a value is to be produced
void register_ui_callback(uint32_t which, string_generation_callback cb, const void *data); // STAY THE SAME
#define REGISTER_STATIC_UI_VALUE(index, str) register_ui_callback(index, copy_string, STATIC_UI_VALUE(str))

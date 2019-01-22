#pragma once

#include "ui.h"
#include "types.h"

// Displays labels (terminated with a NULL pointer) associated with data
// labels must be completely static string constants
// data may be dynamic
// Alternatively, if data is NULL, assume we've registered appropriate callbacks to generate the data
// All pointers may be unrelocated
__attribute__((noreturn))
void ui_prompt(const char *const *labels, const char *const *data, ui_callback_t ok_c, ui_callback_t cxl_c);

// This is called by internal UI code to implement buffering
void switch_screen(uint32_t which);
// This is called by internal UI code to prevent callbacks from sticking around
void clear_ui_callbacks(void);

// This function registers how a value is to be produced
void register_ui_callback(uint32_t which, string_generation_callback cb, const void *data);

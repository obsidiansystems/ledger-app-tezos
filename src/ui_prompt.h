#pragma once

#include "ui.h"

#define MAX_SCREEN_COUNT 6 // Current maximum usage
#define PROMPT_WIDTH 25
#define VALUE_WIDTH PKH_STRING_SIZE

// Displays labels (terminated with a NULL pointer) associated with data
// If data is NULL, assume we've filled it in directly with get_value_buffer
__attribute__((noreturn))
void ui_prompt(const char *const *labels, const char *const *data, callback_t ok_c, callback_t cxl_c);

char *get_value_buffer(uint32_t which);

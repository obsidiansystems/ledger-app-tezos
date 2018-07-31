#pragma once

#include "ui.h"

#define MAX_SCREEN_COUNT 6 // Why not?

// Displays labels (terminated with a NULL pointer) associated with data
__attribute__((noreturn))
void ui_prompt_multiple(const char *const *labels, const char *const *data,
                        callback_t ok_c, callback_t cxl_c);

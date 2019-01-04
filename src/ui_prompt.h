#pragma once

#include "ui.h"

#define MAX_SCREEN_COUNT 7 // Current maximum usage
#define PROMPT_WIDTH 16
#define VALUE_WIDTH PROTOCOL_HASH_BASE58_STRING_SIZE

// Macros to wrap a static prompt and value strings and ensure they aren't too long.
#define PROMPT(str) \
  ({ \
    _Static_assert(sizeof(str) <= PROMPT_WIDTH + 1/*null byte*/ , str " won't fit in the UI prompt."); \
    str; \
  })

#define STATIC_UI_VALUE(str) \
  ({ \
    _Static_assert(sizeof(str) <= VALUE_WIDTH + 1/*null byte*/, str " won't fit in the UI.".); \
    str; \
  })

// Displays labels (terminated with a NULL pointer) associated with data
// If data is NULL, assume we've filled it in directly with get_value_buffer
__attribute__((noreturn))
void ui_prompt(const char *const *labels, const char *const *data, callback_t ok_c, callback_t cxl_c);

char *get_value_buffer(uint32_t which);

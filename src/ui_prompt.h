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
// labels must be completely static string constants
// data may be dynamic
// Alternatively, if data is NULL, assume we've registered appropriate callbacks to generate the data
// All pointers may be unrelocated
__attribute__((noreturn))
void ui_prompt(const char *const *labels, const char *const *data, callback_t ok_c, callback_t cxl_c);

// This is called by internal UI code to implement buffering
void switch_screen(uint32_t which);
// This is called by internal UI code to prevent callbacks from sticking around
void clear_ui_callbacks(void);

// Uses K&R style declaration to avoid being stuck on const void *, to avoid having to cast the
// function pointers.
typedef void (*string_generation_callback)(/* char *buffer, size_t buffer_size, const void *data */);
// This function registers how a value is to be produced
void register_ui_callback(uint32_t which, string_generation_callback cb, const void *data);

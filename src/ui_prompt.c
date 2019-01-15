#include "ui_prompt.h"

#include "exception.h"
#include "memory.h"
#include "to_string.h"

#include <string.h>

// This will and must always be static memory full of constants
static const char *const *prompts;

static string_generation_callback callbacks[MAX_SCREEN_COUNT];
static const void *callback_data[MAX_SCREEN_COUNT];

static char active_prompt[PROMPT_WIDTH + 1];
static char active_value[VALUE_WIDTH + 1];

static const bagl_element_t ui_multi_screen[] = {
    {{BAGL_RECTANGLE, BAGL_STATIC_ELEMENT, 0, 0, 128, 32, 0, 0, BAGL_FILL, 0x000000, 0xFFFFFF,
      0, 0},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_ICON, BAGL_STATIC_ELEMENT, 3, 12, 7, 7, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
      BAGL_GLYPH_ICON_CROSS},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_ICON, BAGL_STATIC_ELEMENT, 117, 13, 8, 6, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
      BAGL_GLYPH_ICON_CHECK},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_LABELINE, BAGL_STATIC_ELEMENT, 0, 12, 128, 12, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     active_prompt,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_LABELINE, BAGL_SCROLLING_ELEMENT, 23, 26, 82, 12, 0x80 | 10, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 26},
     active_value,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
};

void switch_screen(uint32_t which) {
    if (which >= MAX_SCREEN_COUNT) THROW(EXC_MEMORY_ERROR);
    const char *label = (const char*)PIC(prompts[which]);

    strncpy(active_prompt, label, sizeof(active_prompt));
    if (callbacks[which] == NULL) THROW(EXC_MEMORY_ERROR);
    callbacks[which](active_value, sizeof(active_value), callback_data[which]);
}

void register_ui_callback(uint32_t which, string_generation_callback cb, const void *data) {
    if (which >= MAX_SCREEN_COUNT) THROW(EXC_MEMORY_ERROR);
    callbacks[which] = cb;
    callback_data[which] = data;
}

void clear_ui_callbacks(void) {
    for (int i = 0; i < MAX_SCREEN_COUNT; ++i) {
        callbacks[i] = NULL;
    }
}

__attribute__((noreturn))
void ui_prompt(const char *const *labels, const char *const *data, callback_t ok_c, callback_t cxl_c) {
    check_null(labels);
    prompts = labels;

    size_t i;
    for (i = 0; labels[i] != NULL; i++) {
        const char *label = (const char *)PIC(labels[i]);
        if (i >= MAX_SCREEN_COUNT || strlen(label) > PROMPT_WIDTH) THROW(EXC_MEMORY_ERROR);

        if (data != NULL) {
            register_ui_callback(i, copy_string, data[i]);
        }
    }
    size_t screen_count = i;

    ui_display(ui_multi_screen, NUM_ELEMENTS(ui_multi_screen),
               ok_c, cxl_c, screen_count);
    THROW(ASYNC_EXCEPTION);
}

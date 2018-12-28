#include "ui_prompt.h"

#include "exception.h"

#include <string.h>

static char prompts[MAX_SCREEN_COUNT][PROMPT_WIDTH + 1]; // Additional bytes init'ed to null,
static char values[MAX_SCREEN_COUNT][VALUE_WIDTH + 1];   // and null they shall remain.
        // TODO: Get rid of +1

static char active_prompt[PROMPT_WIDTH + 1];
static char active_value[PROMPT_WIDTH + 1];

static const bagl_element_t ui_multi_screen[] = {
    {{BAGL_RECTANGLE, 0x00, 0, 0, 128, 32, 0, 0, BAGL_FILL, 0x000000, 0xFFFFFF,
      0, 0},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_ICON, 0x00, 3, 12, 7, 7, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
      BAGL_GLYPH_ICON_CROSS},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_ICON, 0x00, 117, 13, 8, 6, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
      BAGL_GLYPH_ICON_CHECK},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_LABELINE, 100, 0, 12, 128, 12, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     active_prompt,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_LABELINE, 100, 23, 26, 82, 12, 0x80 | 10, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 26},
     active_value,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL}
};

char *get_value_buffer(uint32_t which) {
    if (which >= MAX_SCREEN_COUNT) THROW(EXC_MEMORY_ERROR);
    return values[which];
}

void switch_screen(uint32_t which) {
    memcpy(active_prompt, prompts[which], sizeof(active_prompt));
    memcpy(active_value, values[which], sizeof(active_value));
}

__attribute__((noreturn))
void ui_prompt(const char *const *labels, const char *const *data, callback_t ok_c, callback_t cxl_c) {
    check_null(labels);

    size_t i;
    for (i = 0; labels[i] != NULL; i++) {
        const char *label = (const char *)PIC(labels[i]);
        if (i >= MAX_SCREEN_COUNT || strlen(label) > PROMPT_WIDTH) THROW(EXC_MEMORY_ERROR);

        // These will not overwrite terminating bytes
        strncpy(prompts[i], label, PROMPT_WIDTH);
        if (data != NULL) {
            const char *value = (const char *)PIC(data[i]);
            if (strlen(value) > VALUE_WIDTH) THROW(EXC_MEMORY_ERROR);
            strncpy(values[i], value, VALUE_WIDTH);
        }
    }
    size_t screen_count = i;

    ui_display(ui_multi_screen, sizeof(ui_multi_screen) / sizeof(*ui_multi_screen),
               ok_c, cxl_c, screen_count);
    THROW(ASYNC_EXCEPTION);
}

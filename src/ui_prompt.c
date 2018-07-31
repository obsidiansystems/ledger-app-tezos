#include "ui_prompt.h"

#include "exception.h"

#include <string.h>

#define PROMPT_WIDTH 25
#define VALUE_WIDTH 40
static char prompts[MAX_SCREEN_COUNT][PROMPT_WIDTH + 1]; // Additional bytes init'ed to null,
static char values[MAX_SCREEN_COUNT][VALUE_WIDTH + 1];   // and null they shall remain.

#define SCREEN_FOR(SCREEN_ID) \
    {{BAGL_LABELINE, SCREEN_ID + 1, 0, 12, 128, 12, 0, 0, 0, 0xFFFFFF, 0x000000, \
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 0}, \
     prompts[SCREEN_ID], \
     0, \
     0, \
     0, \
     NULL, \
     NULL, \
     NULL}, \
    {{BAGL_LABELINE, SCREEN_ID + 1, 23, 26, 82, 12, 0x80 | 10, 0, 0, 0xFFFFFF, 0x000000, \
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 26}, \
     values[SCREEN_ID], \
     0, \
     0, \
     0, \
     NULL, \
     NULL, \
     NULL}

#define INITIAL_ELEMENTS_COUNT 3
#define ELEMENTS_PER_SCREEN 2

const bagl_element_t ui_multi_screen[] = {
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

    SCREEN_FOR(0),
    SCREEN_FOR(1),
    SCREEN_FOR(2),
    SCREEN_FOR(3),
    SCREEN_FOR(4),
    SCREEN_FOR(5),
};

__attribute__((noreturn))
void ui_prompt_multiple(const char *const *labels, const char *const *data,
                        callback_t ok_c, callback_t cxl_c) {
    check_null(labels);
    check_null(data);

    size_t i;
    for (i = 0; labels[i] != NULL; i++) {
        if (i >= MAX_SCREEN_COUNT) THROW(EXC_MEMORY_ERROR);
        // These will not overwrite terminating bytes
        strncpy(prompts[i], (const char *)PIC(labels[i]), PROMPT_WIDTH);
        strncpy(values[i], (const char *)PIC(data[i]), VALUE_WIDTH);
    }
    size_t screen_count = i;

    ui_prompt(ui_multi_screen, INITIAL_ELEMENTS_COUNT + screen_count * ELEMENTS_PER_SCREEN,
              ok_c, cxl_c, screen_count);
    THROW(ASYNC_EXCEPTION);
}

#include "ui.h"
#include "ux.h"

#include "globals.h"
#include "os.h"

void io_seproxyhal_display(const bagl_element_t *element);

void io_seproxyhal_display(const bagl_element_t *element) {
    return io_seproxyhal_display_default((bagl_element_t *)element);
}

void ui_init(void) {
    UX_INIT();
}

// User MUST call `init_formatter_stack()` before the first call to this function.
void push_ui_callback(char *title, string_generation_callback cb, void *data) {
    if (global.formatter_index + 1 >= MAX_SCREEN_COUNT) { // scott update screen
        THROW(0x6124);
    }
    struct fmt_callback *fmt = &global.formatter_stack[global.formatter_index];

    fmt->title = title;
    fmt->callback_fn = cb;
    fmt->data = data;
    global.formatter_index++;
}

void init_formatter_stack() {
    explicit_bzero(&global.formatter_stack, sizeof(global.formatter_stack));
    global.formatter_index = 0;
    global.formatter_stack_size = 0;
    global.current_state = OUT_OF_BORDERS;
}

void require_pin(void) {
    bolos_ux_params_t params;
    memset(&params, 0, sizeof(params));
    params.ux_id = BOLOS_UX_VALIDATE_PIN;
    os_ux_blocking(&params);
}

__attribute__((noreturn))
bool exit_app(void) {
    BEGIN_TRY_L(exit) {
        TRY_L(exit) {
            os_sched_exit(-1);
        }
        FINALLY_L(exit) {
        }
    }
    END_TRY_L(exit);

    THROW(0); // Suppress warning
}

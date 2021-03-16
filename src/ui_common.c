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

// User MUST call `init_screen_stack()` before the first call to this function.
void push_ui_callback(char *title, string_generation_callback cb, void *data) {
    if (global.dynamic_display.formatter_index + 1 >= MAX_SCREEN_STACK_SIZE) {
        THROW(0x6124);
    }
    struct screen_data *fmt = &global.dynamic_display.screen_stack[global.dynamic_display.formatter_index];

    fmt->title = title;
    fmt->callback_fn = cb;
    fmt->data = data;
    global.dynamic_display.formatter_index++;
}

void init_screen_stack() {
    explicit_bzero(&global.dynamic_display.screen_stack, sizeof(global.dynamic_display.screen_stack));
    global.dynamic_display.formatter_index = 0;
    global.dynamic_display.screen_stack_size = 0;
    global.dynamic_display.current_state = STATIC_SCREEN;
}

void require_pin(void) {
    bolos_ux_params_t params;
    memset(&params, 0, sizeof(params));
    params.ux_id = BOLOS_UX_VALIDATE_PIN;
    os_ux_blocking(&params);
}

__attribute__((noreturn))
bool exit_app(void) {
    #ifdef BAKING_APP
      require_pin();
    #endif
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

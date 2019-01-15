#include "globals.h"

#include <string.h>

// The GLOBAL macro helps avoid any temptation to define an initialization value
// since they are not supported on the ledger.
#define GLOBAL(type, name) type name
#define CLEAR_VAR(name) memset(&name, 0, sizeof(name))
#define CLEAR_ARRAY(name) memset(name, 0, sizeof(name))

GLOBAL(level_t, reset_level);

GLOBAL(cx_ecfp_public_key_t, public_key);
GLOBAL(cx_curve_t, curve);

GLOBAL(uint8_t, message_data[TEZOS_BUFSIZE]);
GLOBAL(uint32_t, message_data_length);
GLOBAL(cx_curve_t, curve);

GLOBAL(bool, is_hash_state_inited);
GLOBAL(uint8_t, magic_number);
GLOBAL(bool, hash_only);

// UI
GLOBAL(ux_state_t, ux);
GLOBAL(unsigned, char G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B]);

GLOBAL(ui_callback_t, ok_callback);
GLOBAL(ui_callback_t, cxl_callback);

GLOBAL(uint32_t, ux_step);
GLOBAL(uint32_t, ux_step_count);

GLOBAL(uint32_t, timeout_cycle_count);

GLOBAL(char, idle_text[16]);
GLOBAL(char, baking_auth_text[PKH_STRING_SIZE]);

// UI Prompt
GLOBAL(string_generation_callback, callbacks[MAX_SCREEN_COUNT]);
GLOBAL(char, active_prompt[PROMPT_WIDTH + 1]);
GLOBAL(char, active_value[VALUE_WIDTH + 1]);

// Baking Auth
GLOBAL(WIDE nvram_data, N_data_real); // TODO: What does WIDE actually mean?
GLOBAL(nvram_data, new_data);  // Staging area for setting N_data

void init_globals(void) {
  CLEAR_VAR(reset_level);
  CLEAR_VAR(public_key);
  CLEAR_VAR(curve);
  CLEAR_ARRAY(message_data);
  CLEAR_VAR(message_data_length);
  CLEAR_VAR(curve);
  CLEAR_VAR(is_hash_state_inited);
  CLEAR_VAR(magic_number);
  CLEAR_VAR(hash_only);
  CLEAR_VAR(ux);
  CLEAR_ARRAY(G_io_seproxyhal_spi_buffer);
  CLEAR_VAR(ok_callback);
  CLEAR_VAR(cxl_callback);
  CLEAR_VAR(ux_step);
  CLEAR_VAR(ux_step_count);
  CLEAR_VAR(timeout_cycle_count);
  CLEAR_ARRAY(idle_text);
  CLEAR_ARRAY(baking_auth_text);
  CLEAR_ARRAY(callbacks);
  CLEAR_ARRAY(active_prompt);
  CLEAR_ARRAY(active_value);
  CLEAR_VAR(N_data_real);
  CLEAR_VAR(new_data);
}

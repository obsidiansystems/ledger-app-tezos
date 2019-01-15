#pragma once

#include "types.h"

#include <stdbool.h>


void init_globals(void);

// Where does this number come from?
#ifdef BAKING_APP
#   define TEZOS_BUFSIZE 512
#else
#   define TEZOS_BUFSIZE 256
#endif

extern level_t reset_level;
extern cx_ecfp_public_key_t public_key;
extern cx_curve_t curve;

extern uint8_t message_data[TEZOS_BUFSIZE];
extern uint32_t message_data_length;
extern cx_curve_t curve;

extern bool is_hash_state_inited;
extern uint8_t magic_number;
extern bool hash_only;

// UI
extern ux_state_t ux;
extern unsigned char G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B];

extern ui_callback_t ok_callback;
extern ui_callback_t cxl_callback;

extern uint32_t ux_step;
extern uint32_t ux_step_count;

extern uint32_t timeout_cycle_count;

extern char idle_text[16];
extern char baking_auth_text[PKH_STRING_SIZE];

// UI Prompt
extern string_generation_callback callbacks[MAX_SCREEN_COUNT];
extern char active_prompt[PROMPT_WIDTH + 1];
extern char active_value[VALUE_WIDTH + 1];

// Baking Auth
extern WIDE nvram_data N_data_real; // TODO: What does WIDE actually mean?
extern nvram_data new_data;  // Staging area for setting N_data

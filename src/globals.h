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

#define INS_MAX 0x0B

typedef struct {
  void *stack_root;
  apdu_handler handlers[INS_MAX];

  struct {
    level_t reset_level;
  } baking;

  struct {
    cx_ecfp_public_key_t public_key;
    cx_curve_t curve;

    // The following need to be persisted for baking app
    uint8_t bip32_path_length;
    uint32_t bip32_path[MAX_BIP32_PATH];
  } pubkey;

  struct {
    uint8_t message_data[TEZOS_BUFSIZE];
    uint32_t message_data_length;
    cx_curve_t curve;

    bool is_hash_state_inited;
    uint8_t magic_number;
    bool hash_only;
  } sign;

  struct {
    ux_state_t ux;
    unsigned char G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B];

    ui_callback_t ok_callback;
    ui_callback_t cxl_callback;

    uint32_t ux_step;
    uint32_t ux_step_count;

    uint32_t timeout_cycle_count;

    char idle_text[16];
    char baking_auth_text[PKH_STRING_SIZE];

    struct {
      string_generation_callback callbacks[MAX_SCREEN_COUNT];
      const void *callback_data[MAX_SCREEN_COUNT];
      char active_prompt[PROMPT_WIDTH + 1];
      char active_value[VALUE_WIDTH + 1];
    } prompt;
  } ui;

  struct {
    WIDE nvram_data N_data_real; // TODO: What does WIDE actually mean?
    nvram_data new_data;  // Staging area for setting N_data
  } baking_auth;
} globals_t;

extern globals_t global;

extern unsigned int app_stack_canary; // From SDK

#pragma once

#include "blake2.h"
#include "types.h"

#include <stdbool.h>


void init_globals(void);

// Where does this number come from?
#ifdef BAKING_APP
#   define TEZOS_BUFSIZE 512
#else
#   define TEZOS_BUFSIZE 256
#endif

#define PRIVATE_KEY_DATA_SIZE 32

struct priv_generate_key_pair {
    uint8_t privateKeyData[PRIVATE_KEY_DATA_SIZE];
    struct key_pair res;
};

typedef struct {
  void *stack_root;
  apdu_handler handlers[INS_MAX + 1];

  union {
    struct {
      level_t reset_level;
    } baking;

    struct {
      bip32_path_with_curve_t key;
      cx_ecfp_public_key_t public_key;
    } pubkey;

    struct {
      bip32_path_with_curve_t key;

      uint8_t message_data[TEZOS_BUFSIZE];
      uint32_t message_data_length;

      bool is_hash_state_inited;
      uint8_t magic_number;
      bool hash_only;

      struct {
          uint64_t total_fee;
      } register_delegate;
    } sign;

    struct {
        bip32_path_with_curve_t key;
        chain_id_t main_chain_id;
        struct {
            level_t main;
            level_t test;
        } hwm;
    } setup;
  } u;

  struct {
    ui_callback_t ok_callback;
    ui_callback_t cxl_callback;

    uint32_t ux_step;
    uint32_t ux_step_count;

    uint32_t timeout_cycle_count;

    struct {
        char hwm[MAX_INT_DIGITS + 1]; // with null termination
        char pkh[PKH_STRING_SIZE];
        char chain[CHAIN_ID_BASE58_STRING_SIZE];
    } baking_idle_screens;

    struct {
      string_generation_callback callbacks[MAX_SCREEN_COUNT];
      const void *callback_data[MAX_SCREEN_COUNT];
      char active_prompt[PROMPT_WIDTH + 1];
      char active_value[VALUE_WIDTH + 1];

      // This will and must always be static memory full of constants
      const char *const *prompts;
    } prompt;
  } ui;

  struct {
    nvram_data new_data;  // Staging area for setting N_data

    char address_display_data[VALUE_WIDTH];
  } baking_auth;

  struct {
    b2b_state hash_state;
  } blake2b; // TODO: Use blake2b from SDK

  struct {
    struct priv_generate_key_pair generate_key_pair;

    struct {
      cx_ecfp_public_key_t compressed;
    } public_key_hash;

    struct {
      struct parsed_operation_group out;
    } parse_operations;
  } priv;
} globals_t;

extern globals_t global;

extern unsigned int app_stack_canary; // From SDK

// Used by macros that we don't control.
extern ux_state_t ux;
extern unsigned char G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B];

static inline void throw_stack_size() {
    uint8_t st;
    // uint32_t tmp1 = (uint32_t)&st - (uint32_t)&app_stack_canary;
    uint32_t tmp2 = (uint32_t)global.stack_root - (uint32_t)&st;
    THROW(0x9000 + tmp2);
}

// #ifdef BAKING_APP
extern WIDE nvram_data N_data_real; // TODO: What does WIDE actually mean?

#define N_data (*(WIDE nvram_data*)PIC(&N_data_real))

void update_baking_idle_screens(void);
high_watermark_t *select_hwm_by_chain(chain_id_t const chain_id, nvram_data *const ram);

// Properly updates NVRAM data to prevent any clobbering of data.
// 'out_param' defines the name of a pointer to the nvram_data struct
// that 'body' can change to apply updates.
#define UPDATE_NVRAM(out_name, body) ({ \
    nvram_data *const out_name = &global.baking_auth.new_data; \
    memcpy(&global.baking_auth.new_data, &N_data, sizeof(global.baking_auth.new_data)); \
    body; \
    nvm_write((void*)&N_data, &global.baking_auth.new_data, sizeof(N_data)); \
    update_baking_idle_screens(); \
})
//#endif

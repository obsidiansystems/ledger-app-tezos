#pragma once

#include "types.h"

#include "bolos_target.h"

#include "operations.h"

// Zeros out all globals that can keep track of APDU instruction state.
// Notably this does *not* include UI state.
void clear_apdu_globals(void);

// Zeros out all application-specific globals and SDK-specific UI/exchange buffers.
void init_globals(void);

#define MAX_APDU_SIZE 230  // Maximum number of bytes in a single APDU

// Our buffer must accommodate any remainder from hashing and the next message at once.
#define TEZOS_BUFSIZE (BLAKE2B_BLOCKBYTES + MAX_APDU_SIZE)

#define PRIVATE_KEY_DATA_SIZE 32

#define MAX_SIGNATURE_SIZE 100

struct priv_generate_key_pair {
    uint8_t private_key_data[PRIVATE_KEY_DATA_SIZE];
    key_pair_t res;
};

#ifdef BAKING_APP
typedef struct {
    bip32_path_with_curve_t key;
    uint8_t signed_hmac_key[MAX_SIGNATURE_SIZE];
    uint8_t hashed_signed_hmac_key[CX_SHA512_SIZE];
    uint8_t hmac[CX_SHA256_SIZE];
} apdu_hmac_state_t;
#endif

typedef struct {
    cx_blake2b_t state;
    bool initialized;
} blake2b_hash_state_t;

typedef struct {
    bip32_path_with_curve_t key;

    uint8_t packet_index; // 0-index is the initial setup packet, 1 is first packet to hash, etc.

#   ifdef BAKING_APP
    parsed_baking_data_t parsed_baking_data;
#   endif

    struct {
      bool is_valid;
      struct parsed_operation_group v;
    } maybe_ops;

    uint8_t message_data[TEZOS_BUFSIZE];
    uint32_t message_data_length;
    buffer_t message_data_as_buffer;

    blake2b_hash_state_t hash_state;
    uint8_t final_hash[SIGN_HASH_SIZE];

    uint8_t magic_byte;
    bool hash_only;
    struct parse_state parse_state;
} apdu_sign_state_t;

typedef struct {
  void *stack_root;
  apdu_handler handlers[INS_MAX + 1];

  struct {
    ui_callback_t ok_callback;
    ui_callback_t cxl_callback;

#   ifndef TARGET_NANOX
    uint32_t ux_step;
    uint32_t ux_step_count;

    uint32_t timeout_cycle_count;
#   endif

#   ifdef BAKING_APP
    struct {
        char hwm[MAX_INT_DIGITS + 1]; // with null termination
        char pkh[PKH_STRING_SIZE];
        char chain[CHAIN_ID_BASE58_STRING_SIZE];
    } baking_idle_screens;
#   endif

    struct {
      string_generation_callback callbacks[MAX_SCREEN_COUNT];
      const void *callback_data[MAX_SCREEN_COUNT];

#     ifdef TARGET_NANOX
      struct {
        char prompt[PROMPT_WIDTH + 1];
        char value[VALUE_WIDTH + 1];
      } screen[MAX_SCREEN_COUNT];
#     else
      char active_prompt[PROMPT_WIDTH + 1];
      char active_value[VALUE_WIDTH + 1];

      // This will and must always be static memory full of constants
      const char *const *prompts;
#     endif
    } prompt;
  } ui;

  struct {
      union {
          struct {
              bip32_path_with_curve_t key;
              cx_ecfp_public_key_t public_key;
          } pubkey;

          apdu_sign_state_t sign;

#         ifdef BAKING_APP
          struct {
            level_t reset_level;
          } baking;

          struct {
              bip32_path_with_curve_t key;
              chain_id_t main_chain_id;
              struct {
                  level_t main;
                  level_t test;
              } hwm;
          } setup;

          apdu_hmac_state_t hmac;
#         endif
      } u;

#     ifdef BAKING_APP
      struct {
          nvram_data new_data;  // Staging area for setting N_data

          char address_display_data[VALUE_WIDTH + 1];
      } baking_auth;
#     endif

      struct {
          struct priv_generate_key_pair generate_key_pair;

          struct {
              cx_ecfp_public_key_t compressed;
          } public_key_hash;
      } priv;
    } apdu;
} globals_t;

extern globals_t global;

extern unsigned int app_stack_canary; // From SDK

// Used by macros that we don't control.
#ifdef TARGET_NANOX
    extern ux_state_t G_ux;
    extern bolos_ux_params_t G_ux_params;
#else
    extern ux_state_t ux;
#endif
extern unsigned char G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B];

static inline void throw_stack_size() {
    uint8_t st;
    // uint32_t tmp1 = (uint32_t)&st - (uint32_t)&app_stack_canary;
    uint32_t tmp2 = (uint32_t)global.stack_root - (uint32_t)&st;
    THROW(0x9000 + tmp2);
}

#ifdef BAKING_APP
#   ifdef TARGET_NANOX
        extern nvram_data const N_data_real;
#       define N_data (*(volatile nvram_data *)PIC(&N_data_real))
#    else
        extern nvram_data N_data_real;
#       define N_data (*(nvram_data*)PIC(&N_data_real))
#    endif

void calculate_baking_idle_screens_data(void);
void update_baking_idle_screens(void);
high_watermark_t volatile *select_hwm_by_chain(chain_id_t const chain_id, nvram_data volatile *const ram);

// Properly updates NVRAM data to prevent any clobbering of data.
// 'out_param' defines the name of a pointer to the nvram_data struct
// that 'body' can change to apply updates.
#define UPDATE_NVRAM(out_name, body) ({ \
    nvram_data *const out_name = &global.apdu.baking_auth.new_data; \
    memcpy(&global.apdu.baking_auth.new_data, (nvram_data const *const)&N_data, sizeof(global.apdu.baking_auth.new_data)); \
    body; \
    nvm_write((void*)&N_data, &global.apdu.baking_auth.new_data, sizeof(N_data)); \
    update_baking_idle_screens(); \
})
#endif

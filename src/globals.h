#pragma once

#include "blake2.h"
#include "types.h"

#include "bolos_target.h"

void init_globals(void);

#define MAX_APDU_SIZE 230  // Maximum number of bytes in a single APDU

// Our buffer must accommodate any remainder from hashing and the next message at once.
#define TEZOS_BUFSIZE (BLAKE2B_BLOCKBYTES + MAX_APDU_SIZE)

#define PRIVATE_KEY_DATA_SIZE 32

#define MAX_SIGNATURE_SIZE 100

struct priv_generate_key_pair {
    uint8_t privateKeyData[PRIVATE_KEY_DATA_SIZE];
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
    b2b_state state;
    bool initialized;
} blake2b_hash_state_t;

typedef struct {
    bip32_path_with_curve_t key;

#   ifdef BAKING_APP
    struct {
      bool is_valid;
      parsed_baking_data_t v;
    } maybe_parsed_baking_data;
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

    uint8_t magic_number;
    bool hash_only;
} apdu_sign_state_t;

typedef struct {
# ifdef BAKING_APP
  struct {
    // cached so we can bake while the device is locked.
    key_pair_t keys;

    // tezos-client queries the root key to generate kung-fu animal names.
    cx_ecfp_public_key_t root_public_key;
  } baking_cache;
# endif

  void *stack_root;
  apdu_handler handlers[INS_MAX + 1];

  union {
    struct {
      bip32_path_with_curve_t key;
      cx_ecfp_public_key_t public_key;
    } pubkey;

    apdu_sign_state_t sign;

#   ifdef BAKING_APP
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
#   endif
  } u;

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

#ifdef BAKING_APP
  struct {
    nvram_data new_data;  // Staging area for setting N_data

    char address_display_data[VALUE_WIDTH + 1];
  } baking_auth;
#endif

  struct {
    struct priv_generate_key_pair generate_key_pair;

    struct {
      cx_ecfp_public_key_t compressed;
    } public_key_hash;
  } priv;
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

void update_baking_keys_cache(void);

static inline bool baking_has_authorized_key() {
    return N_data.baking_key.curve != CX_CURVE_NONE && N_data.baking_key.bip32_path.length > 0;
}

// Returns NULL if the cache is not valid.
static inline key_pair_t const *get_cached_baking_key_pair() {
    return baking_has_authorized_key()
      ? &global.baking_cache.keys
      : NULL;
}

// Returns NULL if the cache is not valid.
static inline cx_ecfp_public_key_t const *get_cached_baking_pubkey() {
    key_pair_t const *const key_pair = get_cached_baking_key_pair();
    return key_pair == NULL
      ? NULL
      : &key_pair->public_key;
}

// Properly updates NVRAM data to prevent any clobbering of data.
// 'out_param' defines the name of a pointer to the nvram_data struct
// that 'body' can change to apply updates.
#define UPDATE_NVRAM(out_name, body) ({ \
    nvram_data *const out_name = &global.baking_auth.new_data; \
    memcpy(&global.baking_auth.new_data, (nvram_data const *const)&N_data, sizeof(global.baking_auth.new_data)); \
    body; \
    bool const baking_keys_changed = memcmp((void const *const)&N_data.baking_key, &global.baking_auth.new_data.baking_key, sizeof(N_data.baking_key)) != 0; \
    nvm_write((void *const)&N_data, &global.baking_auth.new_data, sizeof(N_data)); \
    if (baking_keys_changed) { /* requires secure chip so update only when necessary */ update_baking_keys_cache(); } \
    update_baking_idle_screens(); \
})
#endif

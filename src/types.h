#pragma once

#include "os.h"
#include "os_io_seproxyhal.h"

#include <stdbool.h>

// Return number of bytes to transmit (tx)
typedef uint32_t (*apdu_handler)(uint8_t instruction);

typedef uint32_t level_t;

// UI
typedef bool (*ui_callback_t)(void); // return true to go back to idle screen

// Uses K&R style declaration to avoid being stuck on const void *, to avoid having to cast the
// function pointers.
typedef void (*string_generation_callback)(/* char *buffer, size_t buffer_size, const void *data */);

// Baking Auth
#define MAX_BIP32_PATH 10
typedef struct {
    cx_curve_t curve;
    level_t highest_level;
    bool had_endorsement;
    uint8_t path_length;
    uint32_t bip32_path[MAX_BIP32_PATH];
} nvram_data;

#define PKH_STRING_SIZE 40
#define PROTOCOL_HASH_BASE58_STRING_SIZE 52 // e.g. "ProtoBetaBetaBetaBetaBetaBetaBetaBetaBet11111a5ug96" plus null byte

#define MAX_SCREEN_COUNT 7 // Current maximum usage
#define PROMPT_WIDTH 16
#define VALUE_WIDTH PROTOCOL_HASH_BASE58_STRING_SIZE

// Macros to wrap a static prompt and value strings and ensure they aren't too long.
#define PROMPT(str) \
  ({ \
    _Static_assert(sizeof(str) <= PROMPT_WIDTH + 1/*null byte*/ , str " won't fit in the UI prompt."); \
    str; \
  })

#define STATIC_UI_VALUE(str) \
  ({ \
    _Static_assert(sizeof(str) <= VALUE_WIDTH + 1/*null byte*/, str " won't fit in the UI.".); \
    str; \
  })

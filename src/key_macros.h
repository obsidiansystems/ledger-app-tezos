#pragma once

#include "globals.h"
#include "keys.h"

// Yes you need this oddness if you want to use __LINE__
#define CONCAT_(a, b) a##b
#define CONCAT(a, b) CONCAT_(a, b)
#define MACROVAR(a, b) CONCAT(____ ## _ ## a ## _ ## b, __LINE__)

#define WITH_KEY_PAIR(bip32_path_with_curve, vname, type, body) ({ \
    bip32_path_with_curve_t volatile const *const MACROVAR(vname, key) = &(bip32_path_with_curve); \
    key_pair_t *const MACROVAR(vname, generated_pair) = \
        generate_key_pair_return_global( \
            (derivation_type_t const /* cast away volatile! */)MACROVAR(vname, key)->derivation_type, \
            (bip32_path_t const *const /* cast away volatile! */)&MACROVAR(vname, key)->bip32_path); \
    volatile type MACROVAR(vname, retval); \
    BEGIN_TRY { \
        TRY { \
            key_pair_t *const vname = MACROVAR(vname, generated_pair); \
            MACROVAR(vname, retval) = body; \
        } \
        CATCH_OTHER(e) { \
            THROW(e); \
        } \
        FINALLY { \
            if (MACROVAR(vname, generated_pair) != NULL) { \
                memset(MACROVAR(vname, generated_pair), 0, sizeof(*MACROVAR(vname, generated_pair))); \
            } \
        } \
    } \
    END_TRY; \
    MACROVAR(vname, retval); \
})

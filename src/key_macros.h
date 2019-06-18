#pragma once

#include "globals.h"
#include "keys.h"

// Yes you need this if you want to use __LINE__
#define CONCAT_(a, b) a##b
#define CONCAT(a, b) CONCAT_(a, b)
#define MACROVAR(a, b) CONCAT(____ ## _ ## a ## _ ## b, __LINE__)

#define WITH_KEY_PAIR_NO_CACHE(bip32_path_with_curve, vname, type, body) ({ \
    bip32_path_with_curve_t volatile const *const MACROVAR(vname, key) = &(bip32_path_with_curve); \
    key_pair_t *const MACROVAR(vname, generated_pair) = \
        generate_key_pair_return_global( \
            (cx_curve_t const /* cast away volatile! */)MACROVAR(vname, key)->curve, \
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

// Runs `body` in a scope where `vname` is a `key_pair_t` generated from `bip32_path_with_curve`.
// The key data will be zero'ed safely when `body` finishes or if an exception occurs.
// Uses cached key data when available.
#ifdef BAKING_APP
#define WITH_KEY_PAIR(bip32_path_with_curve, vname, type, body) ({ \
    bip32_path_with_curve_t volatile const *const MACROVAR(vname, key) = &(bip32_path_with_curve); \
    key_pair_t const *const MACROVAR(vname, cached_pair) = get_cached_baking_key_pair(); \
    volatile type MACROVAR(vname, retval); \
    if (MACROVAR(vname, cached_pair) != NULL && bip32_path_with_curve_eq(MACROVAR(vname, key), &N_data.baking_key)) { \
        key_pair_t const *const vname = MACROVAR(vname, cached_pair); \
        MACROVAR(vname, retval) = body; \
    } else { \
        MACROVAR(vname, retval) = WITH_KEY_PAIR_NO_CACHE(*MACROVAR(vname, key), vname, type, body); \
    } \
    MACROVAR(vname, retval); \
})
#else
#define WITH_KEY_PAIR(a, b, c, d) WITH_KEY_PAIR_NO_CACHE(a, b, c, d)
#endif


static inline void generate_public_key_cached(
    cx_ecfp_public_key_t *const out,
    bip32_path_with_curve_t volatile const *const key
) {
    check_null(out);
    check_null(key);
#ifdef BAKING_APP
    cx_ecfp_public_key_t const *const baking_pubkey = get_cached_baking_pubkey();
    if (baking_pubkey != NULL && bip32_path_with_curve_eq(key, &kung_fu_animal_bip32_path_with_curve)) {
        memcpy(out, &global.baking_cache.root_public_key, sizeof(*out));
    } else if (baking_pubkey != NULL && bip32_path_with_curve_eq(key, &N_data.baking_key)) {
        memcpy(out, baking_pubkey, sizeof(*out));
    } else {
#endif
        generate_public_key(out, key->curve, (bip32_path_t const *const)&key->bip32_path);
#ifdef BAKING_APP
    }
#endif
}

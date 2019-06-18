#include "globals.h"

#include "exception.h"
#include "keys.h"
#include "key_macros.h"
#include "to_string.h"

#ifdef TARGET_NANOX
#include "ux.h"
#endif

#include <string.h>


// WARNING: ***************************************************
// Non-const globals MUST NOT HAVE AN INITIALIZER.
//
// Providing an initializer will cause the application to crash
// if you write to it.
// ************************************************************


globals_t global;

// These are strange variables that the SDK relies on us to define but uses directly itself.
#ifdef TARGET_NANOX
    ux_state_t G_ux;
    bolos_ux_params_t G_ux_params;
#else
    ux_state_t ux;
#endif

unsigned char G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B];

void init_globals(void) {
    memset(&global, 0, sizeof(global));

#ifdef TARGET_NANOX
    memset(&G_ux, 0, sizeof(G_ux));
    memset(&G_ux_params, 0, sizeof(G_ux_params));
#else
    memset(&ux, 0, sizeof(ux));
#endif

    memset(G_io_seproxyhal_spi_buffer, 0, sizeof(G_io_seproxyhal_spi_buffer));
}

#ifdef BAKING_APP

// DO NOT TRY TO INIT THIS. This can only be written via an system call.
// The "N_" is *significant*. It tells the linker to put this in NVRAM.
#    ifdef TARGET_NANOX
        nvram_data const N_data_real;
#    else
        nvram_data N_data_real;
#    endif

high_watermark_t volatile *select_hwm_by_chain(chain_id_t const chain_id, nvram_data volatile *const ram) {
  check_null(ram);
  return chain_id.v == ram->main_chain_id.v || ram->main_chain_id.v == 0
      ? &ram->hwm.main
      : &ram->hwm.test;
}

void calculate_baking_idle_screens_data(void) {
#   ifdef TARGET_NANOX
        memset(global.ui.baking_idle_screens.hwm, 0, sizeof(global.ui.baking_idle_screens.hwm));
        static char const HWM_PREFIX[] = "HWM: ";
        strcpy(global.ui.baking_idle_screens.hwm, HWM_PREFIX);
        number_to_string(&global.ui.baking_idle_screens.hwm[sizeof(HWM_PREFIX) - 1], (level_t const)N_data.hwm.main.highest_level);
#   else
        number_to_string(global.ui.baking_idle_screens.hwm, N_data.hwm.main.highest_level);
#   endif

    if (baking_has_authorized_key()) {
        bip32_path_with_curve_to_pkh_string(
            global.ui.baking_idle_screens.pkh, sizeof(global.ui.baking_idle_screens.pkh),
            &N_data.baking_key);
    } else {
        STRCPY(global.ui.baking_idle_screens.pkh, "No Key Authorized");
    }

#   ifdef TARGET_NANOX
        if (N_data.main_chain_id.v == 0) {
            strcpy(global.ui.baking_idle_screens.chain, "Chain: any");
        } else {
#   endif

    chain_id_to_string_with_aliases(
        global.ui.baking_idle_screens.chain, sizeof(global.ui.baking_idle_screens.chain),
        (chain_id_t const *const)&N_data.main_chain_id);

#   ifdef TARGET_NANOX
        }
#   endif
}

void update_baking_idle_screens(void) {
    calculate_baking_idle_screens_data();
    ui_refresh();
}

void update_baking_keys_cache(void) {
    if (baking_has_authorized_key()) {
        generate_public_key(
            &global.baking_cache.root_public_key,
            kung_fu_animal_bip32_path_with_curve.curve,
            &kung_fu_animal_bip32_path_with_curve.bip32_path);
        generate_key_pair(
            &global.baking_cache.keys,
            N_data.baking_key.curve,
            (const bip32_path_t *)&N_data.baking_key.bip32_path);
    } else {
        memset(&global.baking_cache, 0, sizeof(global.baking_cache));
    }
}

#endif // #ifdef BAKING_APP

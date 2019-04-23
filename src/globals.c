#include "globals.h"

#include "exception.h"
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
        WIDE nvram_data N_data_real; // TODO: What does WIDE actually mean?
#    endif

high_watermark_t *select_hwm_by_chain(chain_id_t const chain_id, nvram_data *const ram) {
  check_null(ram);
  return chain_id.v == ram->main_chain_id.v || ram->main_chain_id.v == 0
      ? &ram->hwm.main
      : &ram->hwm.test;
}

void update_baking_idle_screens(void) {
    number_to_string(global.ui.baking_idle_screens.hwm, N_data.hwm.main.highest_level);

    if (N_data.baking_key.bip32_path.length == 0) {
        STRCPY(global.ui.baking_idle_screens.pkh, "No Key Authorized");
    } else {
        cx_ecfp_public_key_t const *const pubkey = generate_public_key_return_global(
            N_data.baking_key.curve, &N_data.baking_key.bip32_path);
        pubkey_to_pkh_string(
            global.ui.baking_idle_screens.pkh, sizeof(global.ui.baking_idle_screens.pkh),
            N_data.baking_key.curve, pubkey);
    }

    chain_id_to_string_with_aliases(global.ui.baking_idle_screens.chain, sizeof(global.ui.baking_idle_screens.chain), &N_data.main_chain_id);
}

#endif // #ifdef BAKING_APP

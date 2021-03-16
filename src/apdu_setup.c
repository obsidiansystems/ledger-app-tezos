#ifdef BAKING_APP

#include "apdu_setup.h"

#include "apdu.h"
#include "cx.h"
#include "globals.h"
#include "keys.h"
#include "to_string.h"
#include "ui.h"

#include <string.h>

#define G global.apdu.u.setup

struct setup_wire {
    uint32_t main_chain_id;
    struct {
        uint32_t main;
        uint32_t test;
    } hwm;
    struct bip32_path_wire bip32_path;
} __attribute__((packed));

static bool ok(void) {
    UPDATE_NVRAM(ram, {
        copy_bip32_path_with_curve(&ram->baking_key, &global.path_with_curve);
        ram->main_chain_id = G.main_chain_id;
        ram->hwm.main.highest_level = G.hwm.main;
        ram->hwm.main.had_endorsement = false;
        ram->hwm.test.highest_level = G.hwm.test;
        ram->hwm.test.had_endorsement = false;
    });

    cx_ecfp_public_key_t pubkey = {0};
    generate_public_key(
        &pubkey,
        global.path_with_curve.derivation_type, &global.path_with_curve.bip32_path);
    delayed_send(provide_pubkey(G_io_apdu_buffer, &pubkey));
    return true;
}

__attribute__((noreturn)) static void prompt_setup(
    ui_callback_t const ok_cb,
    ui_callback_t const cxl_cb)
{
    init_screen_stack();
    push_ui_callback("Setup", copy_string, "Baking?");
    push_ui_callback("Address", bip32_path_with_curve_to_pkh_string, &global.path_with_curve);
    push_ui_callback("Chain", chain_id_to_string_with_aliases, &G.main_chain_id);
    push_ui_callback("Main Chain HWM", number_to_string_indirect32, &G.hwm.main);
    push_ui_callback("Test Chain HWM", number_to_string_indirect32, &G.hwm.test);

    ux_confirm_screen(ok_cb, cxl_cb);
}

__attribute__((noreturn)) size_t handle_apdu_setup(__attribute__((unused)) uint8_t instruction) {
    if (READ_UNALIGNED_BIG_ENDIAN(uint8_t, &G_io_apdu_buffer[OFFSET_P1]) != 0) THROW(EXC_WRONG_PARAM);

    uint32_t const buff_size = READ_UNALIGNED_BIG_ENDIAN(uint8_t, &G_io_apdu_buffer[OFFSET_LC]);
    if (buff_size < sizeof(struct setup_wire)) THROW(EXC_WRONG_LENGTH_FOR_INS);

    global.path_with_curve.derivation_type = parse_derivation_type(READ_UNALIGNED_BIG_ENDIAN(uint8_t, &G_io_apdu_buffer[OFFSET_CURVE]));

    {
        struct setup_wire const *const buff_as_setup = (struct setup_wire const *)&G_io_apdu_buffer[OFFSET_CDATA];

        size_t consumed = 0;
        G.main_chain_id.v = CONSUME_UNALIGNED_BIG_ENDIAN(consumed, uint32_t, (uint8_t const *)&buff_as_setup->main_chain_id);
        G.hwm.main = CONSUME_UNALIGNED_BIG_ENDIAN(consumed, uint32_t, (uint8_t const *)&buff_as_setup->hwm.main);
        G.hwm.test = CONSUME_UNALIGNED_BIG_ENDIAN(consumed, uint32_t, (uint8_t const *)&buff_as_setup->hwm.test);
        consumed += read_bip32_path(&global.path_with_curve.bip32_path, (uint8_t const *)&buff_as_setup->bip32_path, buff_size - consumed);

        if (consumed != buff_size) THROW(EXC_WRONG_LENGTH);
    }

    prompt_setup(ok, delay_reject);
}

#endif // #ifdef BAKING_APP

#include "baking_auth.h"

#include "apdu.h"
#include "globals.h"
#include "keys.h"
#include "memory.h"
#include "protocol.h"
#include "to_string.h"
#include "ui_prompt.h"

#include "os_cx.h"

#include <string.h>

bool is_valid_level(level_t lvl) {
    return !(lvl & 0xC0000000);
}

static void write_high_watermark(parsed_baking_data_t const *const in) {
    check_null(in);
    if (!is_valid_level(in->level)) THROW(EXC_WRONG_VALUES);
    UPDATE_NVRAM(ram, {
        // If the chain matches the main chain *or* the main chain is not set, then use 'main' HWM.
        high_watermark_t *const dest = select_hwm_by_chain(in->chain_id, ram);
        dest->highest_level = in->level;
        dest->had_endorsement = in->is_endorsement;
    });
    change_idle_display(N_data.hwm.main.highest_level);
}

void authorize_baking(cx_curve_t curve, bip32_path_t const *const bip32_path) {
    check_null(bip32_path);
    if (bip32_path->length > NUM_ELEMENTS(N_data.bip32_path.components) || bip32_path->length == 0) return;

    UPDATE_NVRAM(ram, {
        ram->curve = curve;
        copy_bip32_path(&ram->bip32_path, bip32_path);
    });
    change_idle_display(N_data.hwm.main.highest_level);
}

static bool is_level_authorized(parsed_baking_data_t const *const baking_info) {
    check_null(baking_info);
    if (!is_valid_level(baking_info->level)) return false;
    high_watermark_t const *const hwm = select_hwm_by_chain(baking_info->chain_id, &N_data);
    return baking_info->level > hwm->highest_level

        // Levels are tied. In order for this to be OK, this must be an endorsement, and we must not
        // have previously seen an endorsement.
        || (baking_info->is_endorsement && !hwm->had_endorsement);
}

bool is_path_authorized(cx_curve_t curve, bip32_path_t const *const bip32_path) {
    check_null(bip32_path);
    return
        curve == N_data.curve &&
        bip32_path->length > 0 &&
        bip32_paths_eq(bip32_path, &N_data.bip32_path);
}

void guard_baking_authorized(cx_curve_t curve, void *data, int datalen, bip32_path_t const *const bip32_path) {
    check_null(data);
    check_null(bip32_path);
    if (!is_path_authorized(curve, bip32_path)) THROW(EXC_SECURITY);

    parsed_baking_data_t baking_info;
    if (!parse_baking_data(&baking_info, data, datalen)) THROW(EXC_PARSE_ERROR);
    if (!is_level_authorized(&baking_info)) THROW(EXC_WRONG_VALUES);
}

void update_high_water_mark(void *data, int datalen) {
    check_null(data);
    parsed_baking_data_t baking_info;
    if (!parse_baking_data(&baking_info, data, datalen)) {
        return; // Must be signing a delegation
    }
    write_high_watermark(&baking_info);
}

void update_auth_text(void) {
    if (N_data.bip32_path.length == 0) {
        strcpy(global.ui.baking_auth_text, "No Key Authorized");
    } else {
        struct key_pair *pair = generate_key_pair(N_data.curve, &N_data.bip32_path);
        memset(&pair->private_key, 0, sizeof(pair->private_key));
        pubkey_to_pkh_string(
            global.ui.baking_auth_text, sizeof(global.ui.baking_auth_text),
            N_data.curve, &pair->public_key);
    }
}

static const char *const pubkey_values[] = {
    "Public Key",
    global.baking_auth.address_display_data,
    NULL,
};

#ifdef BAKING_APP

static char const * const * get_baking_prompts() {
    static const char *const baking_prompts[] = {
        PROMPT("Authorize Baking"),
        PROMPT("Public Key Hash"),
        NULL,
    };
    return baking_prompts;
}

static const char *const baking_values[] = {
    "With Public Key?",
    global.baking_auth.address_display_data,
    NULL,
};

// TODO: Unshare code with next function
void prompt_contract_for_baking(struct parsed_contract *contract, ui_callback_t ok_cb, ui_callback_t cxl_cb) {
    parsed_contract_to_string(
        global.baking_auth.address_display_data, sizeof(global.baking_auth.address_display_data), contract);
    ui_prompt(get_baking_prompts(), baking_values, ok_cb, cxl_cb);
}
#endif

void prompt_address(
#ifndef BAKING_APP
        __attribute__((unused))
#endif
        bool baking,
        cx_curve_t curve, const cx_ecfp_public_key_t *key, ui_callback_t ok_cb,
        ui_callback_t cxl_cb) {
    pubkey_to_pkh_string(
        global.baking_auth.address_display_data, sizeof(global.baking_auth.address_display_data), curve, key);

#ifdef BAKING_APP
    if (baking) {
        ui_prompt(get_baking_prompts(), baking_values, ok_cb, cxl_cb);
    } else {
#endif
        static const char *const pubkey_labels[] = {
            PROMPT("Provide"),
            PROMPT("Public Key Hash"),
            NULL,
        };
        ui_prompt(pubkey_labels, pubkey_values, ok_cb, cxl_cb);
#ifdef BAKING_APP
    }
#endif
}

struct block_wire {
    uint8_t magic_byte;
    uint32_t chain_id;
    uint32_t level;
    uint8_t proto;
    // ... beyond this we don't care
} __attribute__((packed));

struct endorsement_wire {
    uint8_t magic_byte;
    uint32_t chain_id;
    uint8_t branch[32];
    uint8_t tag;
    uint32_t level;
} __attribute__((packed));

bool parse_baking_data(parsed_baking_data_t *const out, void const *const data, size_t const length) {
    switch (get_magic_byte(data, length)) {
        case MAGIC_BYTE_BAKING_OP:
            if (length != sizeof(struct endorsement_wire)) return false;
            struct endorsement_wire const *const endorsement = data;
            out->is_endorsement = true;
            out->chain_id.v = READ_UNALIGNED_BIG_ENDIAN(uint32_t, &endorsement->chain_id);
            out->level = READ_UNALIGNED_BIG_ENDIAN(uint32_t, &endorsement->level);
            return true;
        case MAGIC_BYTE_BLOCK:
            if (length < sizeof(struct block_wire)) return false;
            struct block_wire const *const block = data;
            out->is_endorsement = false;
            out->chain_id.v = READ_UNALIGNED_BIG_ENDIAN(uint32_t, &block->chain_id);
            out->level = READ_UNALIGNED_BIG_ENDIAN(level_t, &block->level);
            return true;
        case MAGIC_BYTE_INVALID:
        default:
            return false;
    }
}

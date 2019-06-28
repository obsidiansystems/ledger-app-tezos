#ifdef BAKING_APP

#include "baking_auth.h"

#include "apdu.h"
#include "globals.h"
#include "keys.h"
#include "memory.h"
#include "protocol.h"
#include "to_string.h"
#include "ui.h"

#include "os_cx.h"

#include <string.h>

bool is_valid_level(level_t lvl) {
    return !(lvl & 0xC0000000);
}

void write_high_water_mark(parsed_baking_data_t const *const in) {
    check_null(in);
    if (!is_valid_level(in->level)) THROW(EXC_WRONG_VALUES);
    UPDATE_NVRAM(ram, {
        // If the chain matches the main chain *or* the main chain is not set, then use 'main' HWM.
        high_watermark_t volatile *const dest = select_hwm_by_chain(in->chain_id, ram);
        dest->highest_level = MAX(in->level, dest->highest_level);
        dest->had_endorsement = in->is_endorsement;
    });
}

void authorize_baking(derivation_type_t const derivation_type, bip32_path_t const *const bip32_path) {
    check_null(bip32_path);
    if (bip32_path->length > NUM_ELEMENTS(N_data.baking_key.bip32_path.components) || bip32_path->length == 0) return;

    UPDATE_NVRAM(ram, {
        ram->baking_key.derivation_type = derivation_type;
        copy_bip32_path(&ram->baking_key.bip32_path, bip32_path);
    });
}

static bool is_level_authorized(parsed_baking_data_t const *const baking_info) {
    check_null(baking_info);
    if (!is_valid_level(baking_info->level)) return false;
    high_watermark_t volatile const *const hwm = select_hwm_by_chain(baking_info->chain_id, &N_data);
    return baking_info->level > hwm->highest_level

        // Levels are tied. In order for this to be OK, this must be an endorsement, and we must not
        // have previously seen an endorsement.
        || (baking_info->level == hwm->highest_level
            && baking_info->is_endorsement && !hwm->had_endorsement);
}

bool is_path_authorized(derivation_type_t const derivation_type, bip32_path_t const *const bip32_path) {
    check_null(bip32_path);
    return
        derivation_type != 0 &&
        derivation_type == N_data.baking_key.derivation_type &&
        bip32_path->length > 0 &&
        bip32_paths_eq(bip32_path, (const bip32_path_t *)&N_data.baking_key.bip32_path);
}

void guard_baking_authorized(parsed_baking_data_t const *const baking_info, bip32_path_with_curve_t const *const key) {
    check_null(baking_info);
    check_null(key);
    if (!is_path_authorized(key->derivation_type, &key->bip32_path)) THROW(EXC_SECURITY);
    if (!is_level_authorized(baking_info)) THROW(EXC_WRONG_VALUES);
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

#endif // #ifdef BAKING_APP

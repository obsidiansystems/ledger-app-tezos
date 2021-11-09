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
        if ((in->level > dest->highest_level) || (in->round > dest->highest_round)) {
            dest->had_endorsement = false;
            dest->had_preendorsement = false;
        };
        dest->highest_level = CUSTOM_MAX(in->level, dest->highest_level);
        dest->highest_round = in->round;
        dest->had_endorsement |= (in->type == BAKING_TYPE_ENDORSEMENT || in->type == BAKING_TYPE_TENDERBAKE_ENDORSEMENT);
        dest->had_preendorsement |= in->type == BAKING_TYPE_TENDERBAKE_PREENDORSEMENT;
        dest->migrated_to_tenderbake |= in->is_tenderbake;
    });
}

void authorize_baking(derivation_type_t const derivation_type,
                      bip32_path_t const *const bip32_path) {
    check_null(bip32_path);
    if (bip32_path->length > NUM_ELEMENTS(N_data.baking_key.bip32_path.components) ||
        bip32_path->length == 0)
        return;

    UPDATE_NVRAM(ram, {
        ram->baking_key.derivation_type = derivation_type;
        copy_bip32_path(&ram->baking_key.bip32_path, bip32_path);
    });
}

static bool is_level_authorized(parsed_baking_data_t const *const baking_info) {
    check_null(baking_info);
    if (!is_valid_level(baking_info->level)) return false;
    high_watermark_t volatile const *const hwm =
        select_hwm_by_chain(baking_info->chain_id, &N_data);

    if (baking_info->is_tenderbake) {
        return baking_info->level > hwm->highest_level ||

               (baking_info->level == hwm->highest_level &&
                baking_info->round > hwm->highest_round) ||

               // It is ok to sign an endorsement if we have not already signed an endorsement for
               // the level/round
               (baking_info->level == hwm->highest_level &&
                baking_info->round == hwm->highest_round &&
                baking_info->type == BAKING_TYPE_TENDERBAKE_ENDORSEMENT && !hwm->had_endorsement) ||

               // It is ok to sign a preendorsement if we have not already signed neither an
               // endorsement nor a preendorsement for the level/round
               (baking_info->level == hwm->highest_level &&
                baking_info->round == hwm->highest_round &&
                baking_info->type == BAKING_TYPE_TENDERBAKE_PREENDORSEMENT &&
                !hwm->had_endorsement && !hwm->had_preendorsement);

    } else {
        if (hwm->migrated_to_tenderbake) return false;
        return baking_info->level > hwm->highest_level

               // Levels are tied. In order for this to be OK, this must be an endorsement, and we
               // must not have previously seen an endorsement.
               || (baking_info->level == hwm->highest_level &&
                   baking_info->type == BAKING_TYPE_ENDORSEMENT && !hwm->had_endorsement);
    }
}

bool is_path_authorized(derivation_type_t const derivation_type,
                        bip32_path_t const *const bip32_path) {
    check_null(bip32_path);
    return derivation_type != 0 && derivation_type == N_data.baking_key.derivation_type &&
           bip32_path->length > 0 &&
           bip32_paths_eq(bip32_path, (const bip32_path_t *) &N_data.baking_key.bip32_path);
}

void guard_baking_authorized(parsed_baking_data_t const *const baking_info,
                             bip32_path_with_curve_t const *const key) {
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
    uint8_t predecessor[32];
    uint64_t timestamp;
    uint8_t validation_pass;
    uint8_t operation_hash[32];
    uint32_t fitness_size;
    // ... beyond this we don't care
} __attribute__((packed));

struct consensus_op_wire {
    uint8_t magic_byte;
    uint32_t chain_id;
    uint8_t branch[32];
    uint8_t tag;
    uint32_t level;
    // ... beyond this we don't care
} __attribute__((packed));

struct tenderbake_consensus_op_wire {
    uint8_t magic_byte;
    uint32_t chain_id;
    uint8_t branch[32];
    uint8_t tag;
    uint16_t slot;
    uint32_t level;
    uint32_t round;
    uint8_t block_payload_hash[32];
} __attribute__((packed));

#define EMMY_FITNESS_SIZE               17
#define MINIMAL_TENDERBAKE_FITNESS_SIZE 33  // Locked round = None, otherwise 37

#define TENDERBAKE_PROTO_FITNESS_VERSION 2

uint8_t get_proto_version(void const *const fitness) {
    return READ_UNALIGNED_BIG_ENDIAN(uint8_t, fitness + sizeof(uint32_t));
}

uint32_t get_round(void const *const fitness, uint32_t fitness_size) {
    return READ_UNALIGNED_BIG_ENDIAN(uint32_t, (fitness + fitness_size - 4));
}

bool parse_block(parsed_baking_data_t *const out, void const *const data, size_t const length) {
    if (length < sizeof(struct block_wire) + EMMY_FITNESS_SIZE) return false;
    struct block_wire const *const block = data;
    out->chain_id.v = READ_UNALIGNED_BIG_ENDIAN(uint32_t, &block->chain_id);
    out->level = READ_UNALIGNED_BIG_ENDIAN(level_t, &block->level);

    void const *const fitness = data + sizeof(struct block_wire);
    uint8_t proto_version = get_proto_version(fitness);
    switch (proto_version) {
        case 0:  // Emmy 0 to 4
        case 1:  // Emmy 5 to 11
            out->type = BAKING_TYPE_BLOCK;
            out->is_tenderbake = false;
            out->round = 0;  // irrelevant
            return true;
        case 2:  // Tenderbake
            out->type = BAKING_TYPE_TENDERBAKE_BLOCK;
            out->is_tenderbake = true;
            uint32_t fitness_size = READ_UNALIGNED_BIG_ENDIAN(uint32_t, &block->fitness_size);
            out->round = get_round(fitness, fitness_size);
            return true;
        default:
            return false;
    }
}

bool parse_consensus_operation(parsed_baking_data_t *const out,
                               void const *const data,
                               size_t const length) {
    if (length < sizeof(struct consensus_op_wire)) return false;
    struct consensus_op_wire const *const op = data;

    out->chain_id.v = READ_UNALIGNED_BIG_ENDIAN(uint32_t, &op->chain_id);
    out->level = READ_UNALIGNED_BIG_ENDIAN(uint32_t, &op->level);

    switch (op->tag) {
        case 0:  // emmy endorsement (without slot)
            out->type = BAKING_TYPE_ENDORSEMENT;
            out->is_tenderbake = false;
            out->round = 0;  // irrelevant
            return true;
        case 20:  // tenderbake preendorsement
        case 21:  // tenderbake endorsement
            if (length < sizeof(struct tenderbake_consensus_op_wire)) return false;
            struct tenderbake_consensus_op_wire const *const op = data;
            out->is_tenderbake = true;
            out->level = READ_UNALIGNED_BIG_ENDIAN(uint32_t, &op->level);
            out->round = READ_UNALIGNED_BIG_ENDIAN(uint32_t, &op->round);
            if (op->tag == 20) {
                out->type = BAKING_TYPE_TENDERBAKE_PREENDORSEMENT;
            } else {
                out->type = BAKING_TYPE_TENDERBAKE_ENDORSEMENT;
            }
            return true;
        default:
            return false;
    }
}

bool parse_baking_data(parsed_baking_data_t *const out,
                       void const *const data,
                       size_t const length) {
    switch (get_magic_byte(data, length)) {
        case MAGIC_BYTE_BAKING_OP:
        case MAGIC_BYTE_TENDERBAKE_PREENDORSEMENT:
        case MAGIC_BYTE_TENDERBAKE_ENDORSEMENT:
            return parse_consensus_operation(out, data, length);
        case MAGIC_BYTE_BLOCK:
        case MAGIC_BYTE_TENDERBAKE_BLOCK:
            return parse_block(out, data, length);
        case MAGIC_BYTE_INVALID:
        default:
            return false;
    }
}

#endif  // #ifdef BAKING_APP

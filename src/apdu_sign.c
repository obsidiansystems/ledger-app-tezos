#include "apdu_sign.h"

#include "apdu.h"
#include "baking_auth.h"
#include "base58.h"
#include "globals.h"
#include "keys.h"
#include "memory.h"
#include "protocol.h"
#include "to_string.h"
#include "ui.h"
#include "swap_lib_calls.h"

#include "cx.h"

#include <string.h>

#define G global.apdu.u.sign

#define PARSE_ERROR() THROW(EXC_PARSE_ERROR)

#define B2B_BLOCKBYTES 128

static inline void conditional_init_hash_state(blake2b_hash_state_t *const state) {
    check_null(state);
    if (!state->initialized) {
        cx_blake2b_init(&state->state, SIGN_HASH_SIZE*8); // cx_blake2b_init takes size in bits.
        state->initialized = true;
    }
}

static void blake2b_incremental_hash(
    /*in/out*/ uint8_t *const out, size_t const out_size,
    /*in/out*/ size_t *const out_length,
    /*in/out*/ blake2b_hash_state_t *const state
) {
    check_null(out);
    check_null(out_length);
    check_null(state);

    uint8_t *current = out;
    while (*out_length > B2B_BLOCKBYTES) {
        if (current - out > (int)out_size) THROW(EXC_MEMORY_ERROR);
        conditional_init_hash_state(state);
        cx_hash((cx_hash_t *) &state->state, 0, current, B2B_BLOCKBYTES, NULL, 0);
        *out_length -= B2B_BLOCKBYTES;
        current += B2B_BLOCKBYTES;
    }
    // TODO use circular buffer at some point
    memmove(out, current, *out_length);
}

static void blake2b_finish_hash(
    /*out*/ uint8_t *const out, size_t const out_size,
    /*in/out*/ uint8_t *const buff, size_t const buff_size,
    /*in/out*/ size_t *const buff_length,
    /*in/out*/ blake2b_hash_state_t *const state
) {
    check_null(out);
    check_null(buff);
    check_null(buff_length);
    check_null(state);

    conditional_init_hash_state(state);
    blake2b_incremental_hash(buff, buff_size, buff_length, state);
    cx_hash((cx_hash_t *) &state->state, CX_LAST, buff, *buff_length, out, out_size);
}

static int perform_signature(bool const on_hash, bool const send_hash);

static inline void clear_data(void) {
    memset(&G, 0, sizeof(G));
}

static bool sign_without_hash_ok(void) {
    delayed_send(perform_signature(true, false));
    return true;
}

static bool sign_with_hash_ok(void) {
    delayed_send(perform_signature(true, true));
    return true;
}

static bool sign_reject(void) {
    clear_data();
    delay_reject();
    return true; // Return to idle
}

static bool is_operation_allowed(enum operation_tag tag) {
    switch (tag) {
        case OPERATION_TAG_ATHENS_DELEGATION: return true;
        case OPERATION_TAG_ATHENS_REVEAL: return true;
        case OPERATION_TAG_BABYLON_DELEGATION: return true;
        case OPERATION_TAG_BABYLON_REVEAL: return true;
#       ifndef BAKING_APP
            case OPERATION_TAG_PROPOSAL: return true;
            case OPERATION_TAG_BALLOT: return true;
            case OPERATION_TAG_ATHENS_ORIGINATION: return true;
            case OPERATION_TAG_ATHENS_TRANSACTION: return true;
            case OPERATION_TAG_BABYLON_ORIGINATION: return true;
            case OPERATION_TAG_BABYLON_TRANSACTION: return true;
#       endif
        default: return false;
    }
}

#ifdef BAKING_APP
static bool parse_allowed_operations(
    struct parsed_operation_group *const out,
    uint8_t const *const in,
    size_t const in_size,
    bip32_path_with_curve_t const *const key
) {
    return parse_operations(out, in, in_size, key->derivation_type, &key->bip32_path, &is_operation_allowed);
}

#else

static bool parse_allowed_operation_packet(
    struct parsed_operation_group *const out,
    uint8_t const *const in,
    size_t const in_size
) {
    return parse_operations_packet(out, in, in_size, &is_operation_allowed);
}

#endif

#ifdef BAKING_APP // ----------------------------------------------------------

__attribute__((noreturn)) static void prompt_register_delegate(
    ui_callback_t const ok_cb,
    ui_callback_t const cxl_cb
) {
    if (!G.maybe_ops.is_valid) THROW(EXC_MEMORY_ERROR);

    init_screen_stack();
    push_ui_callback("Register", copy_string, "as delegate?");
    push_ui_callback("Address", bip32_path_with_curve_to_pkh_string, &global.path_with_curve);
    push_ui_callback("Fee", microtez_to_string_indirect, &G.maybe_ops.v.total_fee);

    ux_confirm_screen(ok_cb, cxl_cb);
}

size_t baking_sign_complete(bool const send_hash) {
    switch (G.magic_byte) {
        case MAGIC_BYTE_BLOCK:
        case MAGIC_BYTE_BAKING_OP:
            guard_baking_authorized(&G.parsed_baking_data, &global.path_with_curve);
            return perform_signature(true, send_hash);
            break;

        case MAGIC_BYTE_UNSAFE_OP:
            {
                if (!G.maybe_ops.is_valid) PARSE_ERROR();

                // Must be self-delegation signed by the *authorized* baking key
                if (bip32_path_with_curve_eq(&global.path_with_curve, &N_data.baking_key) &&

                    // ops->signing is generated from G.bip32_path and G.curve
                    COMPARE(&G.maybe_ops.v.operation.source, &G.maybe_ops.v.signing) == 0 &&
                    COMPARE(&G.maybe_ops.v.operation.destination, &G.maybe_ops.v.signing) == 0
                ) {
                    ui_callback_t const ok_c = send_hash ? sign_with_hash_ok : sign_without_hash_ok;
                    prompt_register_delegate(ok_c, sign_reject);
                }
                THROW(EXC_SECURITY);
                break;
            }
        case MAGIC_BYTE_UNSAFE_OP2:
        case MAGIC_BYTE_UNSAFE_OP3:
        default:
            PARSE_ERROR();
    }
}

#else // ifdef BAKING_APP -----------------------------------------------------

static bool sign_unsafe_ok(void) {
    delayed_send(perform_signature(false, false));
    return true;
}

#define MAX_NUMBER_CHARS (MAX_INT_DIGITS + 2) // include decimal point and terminating null

bool prompt_transaction(
    struct parsed_operation_group const *const ops,
    bip32_path_with_curve_t const *const key,
    ui_callback_t ok, ui_callback_t cxl
) {
    check_null(ops);
    check_null(key);

    switch (ops->operation.tag) {
        default:
            PARSE_ERROR();

        case OPERATION_TAG_PROPOSAL:
            {
                init_screen_stack();
                push_ui_callback("Confirm", copy_string, "Proposal");
                push_ui_callback("Source", parsed_contract_to_string, &ops->operation.source);
                push_ui_callback("Period", number_to_string_indirect32, &ops->operation.proposal.voting_period);
                push_ui_callback("Protocol", protocol_hash_to_string, ops->operation.proposal.protocol_hash);

                ux_confirm_screen(ok, cxl);
            }

        case OPERATION_TAG_BALLOT:
            {
                char *vote;

                switch (ops->operation.ballot.vote) {
                    case BALLOT_VOTE_YEA:
                        vote = "Yea";
                        break;
                    case BALLOT_VOTE_NAY:
                        vote = "Nay";
                        break;
                    case BALLOT_VOTE_PASS:
                        vote = "Pass";
                        break;
                }

                init_screen_stack();
                push_ui_callback("Confirm Vote", copy_string, vote);
                push_ui_callback("Source", parsed_contract_to_string, &ops->operation.source);
                push_ui_callback("Protocol", protocol_hash_to_string,
                                     ops->operation.ballot.protocol_hash);
                push_ui_callback("Period", number_to_string_indirect32, &ops->operation.ballot.voting_period);

                ux_confirm_screen(ok, cxl);
            }

        case OPERATION_TAG_ATHENS_ORIGINATION:
        case OPERATION_TAG_BABYLON_ORIGINATION:
            {

                if (!(ops->operation.flags & ORIGINATION_FLAG_SPENDABLE)) return false;

                init_screen_stack();
                push_ui_callback("Confirm", copy_string, "Origination");
                push_ui_callback("Amount", microtez_to_string_indirect, &ops->operation.amount);
                push_ui_callback("Fee", microtez_to_string_indirect, &ops->total_fee);
                push_ui_callback("Source", parsed_contract_to_string, &ops->operation.source);
                push_ui_callback("Manager", parsed_contract_to_string,
                                     &ops->operation.destination);

                bool const delegatable = ops->operation.flags & ORIGINATION_FLAG_DELEGATABLE;
                bool const has_delegate = ops->operation.delegate.signature_type != SIGNATURE_TYPE_UNSET;
                if (delegatable && has_delegate) {
                    push_ui_callback("Delegate", parsed_contract_to_string,
                                         &ops->operation.delegate);
                } else if (delegatable && !has_delegate) {
                    push_ui_callback("Delegate", copy_string, "Any");
                } else if (!delegatable && has_delegate) {
                    push_ui_callback("Fixed Delegate", parsed_contract_to_string,
                                         &ops->operation.delegate);
                } else if (!delegatable && !has_delegate) {
                    push_ui_callback("Delegation", copy_string, "Disabled");
                }
                push_ui_callback("Storage Limit", number_to_string_indirect64,
                                     &ops->total_storage_limit);

                ux_confirm_screen(ok, cxl);
            }
        case OPERATION_TAG_ATHENS_DELEGATION:
        case OPERATION_TAG_BABYLON_DELEGATION:
            {
                bool const withdrawal = ops->operation.destination.originated == 0 &&
                    ops->operation.destination.signature_type == SIGNATURE_TYPE_UNSET;
                
                
                char *type_msg;
                if (withdrawal) {
                    type_msg = "Withdraw";
                } else {
                    type_msg = "Confirm";
                }
                init_screen_stack();
                push_ui_callback(type_msg, copy_string, "Delegation");

                push_ui_callback("Fee", microtez_to_string_indirect, &ops->total_fee);
                push_ui_callback("Source", parsed_contract_to_string,
                                     &ops->operation.source);
                push_ui_callback("Delegate", parsed_contract_to_string,
                                     &ops->operation.destination);
                push_ui_callback("Delegate Name", lookup_parsed_contract_name,
                                     &ops->operation.destination);
                push_ui_callback("Storage Limit", number_to_string_indirect64,
                                     &ops->total_storage_limit);

                ux_confirm_screen(ok, cxl);
            }

        case OPERATION_TAG_ATHENS_TRANSACTION:
        case OPERATION_TAG_BABYLON_TRANSACTION:
            {
                init_screen_stack();
                push_ui_callback("Confirm", copy_string, "Transaction");
                push_ui_callback("Amount", microtez_to_string_indirect, &ops->operation.amount);
                push_ui_callback("Fee", microtez_to_string_indirect, &ops->total_fee);
                push_ui_callback("Source", parsed_contract_to_string, &ops->operation.source);
                push_ui_callback("Destination", parsed_contract_to_string,
                                     &ops->operation.destination);
                push_ui_callback("Storage Limit", number_to_string_indirect64,
                                     &ops->total_storage_limit);

                ux_confirm_screen(ok, cxl);
            }
        case OPERATION_TAG_NONE:
            {
                init_screen_stack();
                push_ui_callback("Reveal Key", copy_string, "To Blockchain");
                push_ui_callback("Key", parsed_contract_to_string, &ops->operation.source);
                push_ui_callback("Fee", microtez_to_string_indirect, &ops->total_fee);
                push_ui_callback("Storage Limit", number_to_string_indirect64,
                                     &ops->total_storage_limit);

                ux_confirm_screen(ok, cxl);
            }
    }
}

static size_t wallet_sign_complete(uint8_t instruction, uint8_t magic_byte) {
    char *ops;
    if (magic_byte == MAGIC_BYTE_UNSAFE_OP3) {
      ops = "Michelson";
    }
    else {
      ops = "Operation";
    }

    if (instruction == INS_SIGN_UNSAFE) {
        G.message_data_as_buffer.bytes = (uint8_t *)&G.message_data;
        G.message_data_as_buffer.size = sizeof(G.message_data);
        G.message_data_as_buffer.length = G.message_data_length;
        init_screen_stack();
        push_ui_callback("Pre-hashed", copy_string, ops);
        // Base58 encoding of 32-byte hash is 43 bytes long.
        push_ui_callback("Sign Hash", buffer_to_base58, &G.message_data_as_buffer);
        ux_confirm_screen(sign_unsafe_ok, sign_reject);
    } else {
        ui_callback_t const ok_c = instruction == INS_SIGN_WITH_HASH ? sign_with_hash_ok : sign_without_hash_ok;

        switch (G.magic_byte) {
            case MAGIC_BYTE_BLOCK:
            case MAGIC_BYTE_BAKING_OP:
            default:
                PARSE_ERROR();
            case MAGIC_BYTE_UNSAFE_OP:
                if (!G.maybe_ops.is_valid || !prompt_transaction(&G.maybe_ops.v, &global.path_with_curve, ok_c, sign_reject)) {
                    goto unsafe;
                }

            case MAGIC_BYTE_UNSAFE_OP2:
            case MAGIC_BYTE_UNSAFE_OP3:
                goto unsafe;
        }
unsafe:
        G.message_data_as_buffer.bytes = (uint8_t *)&G.final_hash;
        G.message_data_as_buffer.size = sizeof(G.final_hash);
        G.message_data_as_buffer.length = sizeof(G.final_hash);
        init_screen_stack();
        // Base58 encoding of 32-byte hash is 43 bytes long.
        push_ui_callback("Unrecognized", copy_string, ops);
        push_ui_callback("Sign Hash", buffer_to_base58, &G.message_data_as_buffer);
        ux_confirm_screen(ok_c, sign_reject);
    }
}

#endif // ifdef BAKING_APP ----------------------------------------------------

#define P1_FIRST 0x00
#define P1_NEXT 0x01
#define P1_HASH_ONLY_NEXT 0x03 // You only need it once
#define P1_LAST_MARKER 0x80

static uint8_t get_magic_byte_or_throw(uint8_t const *const buff, size_t const buff_size) {
    uint8_t const magic_byte = get_magic_byte(buff, buff_size);
    switch (magic_byte) {
#       ifdef BAKING_APP
        case MAGIC_BYTE_BLOCK:
        case MAGIC_BYTE_BAKING_OP:
        case MAGIC_BYTE_UNSAFE_OP: // Only for self-delegations
#       else
        case MAGIC_BYTE_UNSAFE_OP:
        case MAGIC_BYTE_UNSAFE_OP3:
#       endif
            return magic_byte;

        case MAGIC_BYTE_UNSAFE_OP2:
        default: PARSE_ERROR();
    }
}

static size_t handle_apdu(bool const enable_hashing, bool const enable_parsing, uint8_t const instruction) {
    uint8_t *const buff = &G_io_apdu_buffer[OFFSET_CDATA];
    uint8_t const p1 = READ_UNALIGNED_BIG_ENDIAN(uint8_t, &G_io_apdu_buffer[OFFSET_P1]);
    uint8_t const buff_size = READ_UNALIGNED_BIG_ENDIAN(uint8_t, &G_io_apdu_buffer[OFFSET_LC]);
    if (buff_size > MAX_APDU_SIZE) THROW(EXC_WRONG_LENGTH_FOR_INS);

    bool last = (p1 & P1_LAST_MARKER) != 0;
    switch (p1 & ~P1_LAST_MARKER) {
    case P1_FIRST:
        clear_data();
        read_bip32_path(&global.path_with_curve.bip32_path, buff, buff_size);
        global.path_with_curve.derivation_type = parse_derivation_type(READ_UNALIGNED_BIG_ENDIAN(uint8_t, &G_io_apdu_buffer[OFFSET_CURVE]));
        return finalize_successful_send(0);
#ifndef BAKING_APP
    case P1_HASH_ONLY_NEXT:
        // This is a debugging Easter egg
        G.hash_only = true;
        // FALL THROUGH
#endif
    case P1_NEXT:
        if (global.path_with_curve.bip32_path.length == 0) THROW(EXC_WRONG_LENGTH_FOR_INS);

        // Guard against overflow
        if (G.packet_index >= 0xFF) PARSE_ERROR();
        G.packet_index++;

        break;
    default:
        THROW(EXC_WRONG_PARAM);
    }

    if (enable_parsing) {
#       ifdef BAKING_APP
            if (G.packet_index != 1) PARSE_ERROR(); // Only parse a single packet when baking

            G.magic_byte = get_magic_byte_or_throw(buff, buff_size);
            if (G.magic_byte == MAGIC_BYTE_UNSAFE_OP) {
                // Parse the operation. It will be verified in `baking_sign_complete`.
                G.maybe_ops.is_valid = parse_allowed_operations(&G.maybe_ops.v, buff, buff_size, &global.path_with_curve);
            } else {
                // This should be a baking operation so parse it.
                if (!parse_baking_data(&G.parsed_baking_data, buff, buff_size)) PARSE_ERROR();
            }
#       else
	    if (G.packet_index == 1) {

	        G.maybe_ops.is_valid = false;
          G.magic_byte = get_magic_byte_or_throw(buff, buff_size);

          // If it is an "operation" (starting with the 0x03 magic byte), set up parsing
          // If it is arbitrary Michelson (starting with 0x05), dont bother parsing and show the "Sign Hash" prompt
          if (G.magic_byte == MAGIC_BYTE_UNSAFE_OP) {
            parse_operations_init(&G.maybe_ops.v, global.path_with_curve.derivation_type, &global.path_with_curve.bip32_path, &G.parse_state);
          }
          // If magic byte is not 0x03 or 0x05, fail
          else if (G.magic_byte != MAGIC_BYTE_UNSAFE_OP3) {
            PARSE_ERROR();
          }
	    }

      // Only parse if the message is an "Operation"
      if (G.magic_byte == MAGIC_BYTE_UNSAFE_OP) {
        parse_allowed_operation_packet(&G.maybe_ops.v, buff, buff_size);
      }

#       endif
    }

    if (enable_hashing) {
        // Hash contents of *previous* message (which may be empty).
        blake2b_incremental_hash(
            G.message_data, sizeof(G.message_data),
            &G.message_data_length,
            &G.hash_state);
    }

    if (G.message_data_length + buff_size > sizeof(G.message_data)) PARSE_ERROR();

    memmove(G.message_data + G.message_data_length, buff, buff_size);
    G.message_data_length += buff_size;

    if (called_from_swap) {
        swap_check();
        os_sched_exit(0);
    }

    if (last) {
        if (enable_hashing) {
            // Hash contents of *this* message and then get the final hash value.
            blake2b_incremental_hash(
                G.message_data, sizeof(G.message_data),
                &G.message_data_length,
                &G.hash_state);
            blake2b_finish_hash(
                G.final_hash, sizeof(G.final_hash),
                G.message_data, sizeof(G.message_data),
                &G.message_data_length,
                &G.hash_state);
        }

	G.maybe_ops.is_valid = parse_operations_final(&G.parse_state, &G.maybe_ops.v);

        return
#           ifdef BAKING_APP
                baking_sign_complete(instruction == INS_SIGN_WITH_HASH);
#           else
                wallet_sign_complete(instruction, G.magic_byte);
#           endif
    } else {
        return finalize_successful_send(0);
    }
}

size_t handle_apdu_sign(uint8_t instruction) {
    bool const enable_hashing = instruction != INS_SIGN_UNSAFE;
    bool const enable_parsing = enable_hashing;
    return handle_apdu(enable_hashing, enable_parsing, instruction);
}

size_t handle_apdu_sign_with_hash(uint8_t instruction) {
    bool const enable_hashing = true;
    bool const enable_parsing = true;
    return handle_apdu(enable_hashing, enable_parsing, instruction);
}

static int perform_signature(bool const on_hash, bool const send_hash) {
#   ifdef BAKING_APP
        write_high_water_mark(&G.parsed_baking_data);
#   else
        if (on_hash && G.hash_only) {
            memcpy(G_io_apdu_buffer, G.final_hash, sizeof(G.final_hash));
            clear_data();
            return finalize_successful_send(sizeof(G.final_hash));
        }
#   endif

    size_t tx = 0;
    if (send_hash && on_hash) {
        memcpy(&G_io_apdu_buffer[tx], G.final_hash, sizeof(G.final_hash));
        tx += sizeof(G.final_hash);
    }

    uint8_t const *const data = on_hash ? G.final_hash : G.message_data;
    size_t const data_length = on_hash ? sizeof(G.final_hash) : G.message_data_length;

    key_pair_t key_pair = {0};
    size_t signature_size = 0;

    BEGIN_TRY {
        TRY {
            generate_key_pair(&key_pair, global.path_with_curve.derivation_type, &global.path_with_curve.bip32_path);
            signature_size = sign(&G_io_apdu_buffer[tx], MAX_SIGNATURE_SIZE, global.path_with_curve.derivation_type, &key_pair, data, data_length);
        }
        CATCH_OTHER(e) {
            THROW(e);
        }
        FINALLY {
            memset(&key_pair, 0, sizeof(key_pair));
        }
    }
    END_TRY

    tx += signature_size;

    clear_data();
    return finalize_successful_send(tx);
}
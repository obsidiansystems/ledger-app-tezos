#include "apdu_sign.h"

#include "apdu.h"
#include "baking_auth.h"
#include "base58.h"
#include "blake2.h"
#include "globals.h"
#include "keys.h"
#include "memory.h"
#include "protocol.h"
#include "to_string.h"
#include "ui.h"
#include "ui_prompt.h"

#include "cx.h"

#include <string.h>

#define G global.u.sign

#define B2B_BLOCKBYTES 128

static inline void conditional_init_hash_state(blake2b_hash_state_t *const state) {
    check_null(state);
    if (!state->initialized) {
        b2b_init(&state->state, SIGN_HASH_SIZE);
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
        b2b_update(&state->state, current, B2B_BLOCKBYTES);
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
    b2b_update(&state->state, buff, *buff_length);
    b2b_final(&state->state, out, out_size);
}

static int perform_signature(bool hash_first);

static inline void clear_data(void) {
    memset(&G, 0, sizeof(G));
}

static bool sign_ok(void) {
    delayed_send(perform_signature(true));
    return true;
}

static bool sign_reject(void) {
    clear_data();
    delay_reject();
    return true; // Return to idle
}

static bool parse_allowed_operations(
    struct parsed_operation_group *const out,
    uint8_t const *const in,
    size_t const in_size,
    bip32_path_with_curve_t const *const key
) {
    // TODO: Simplify this to just switch on what we got.
    allowed_operation_set allowed;
    clear_operation_set(&allowed);

    allow_operation(&allowed, OPERATION_TAG_DELEGATION);
    allow_operation(&allowed, OPERATION_TAG_REVEAL);
#   ifndef BAKING_APP
        allow_operation(&allowed, OPERATION_TAG_PROPOSAL);
        allow_operation(&allowed, OPERATION_TAG_BALLOT);
        allow_operation(&allowed, OPERATION_TAG_ORIGINATION);
        allow_operation(&allowed, OPERATION_TAG_TRANSACTION);
        // TODO: Add still other operations
#   endif

    return parse_operations(out, in, in_size, key->curve, &key->bip32_path, allowed);
}

#ifdef BAKING_APP // ----------------------------------------------------------

__attribute__((noreturn)) static void prompt_register_delegate(
    ui_callback_t const ok_cb,
    ui_callback_t const cxl_cb
) {
    static const size_t TYPE_INDEX = 0;
    static const size_t ADDRESS_INDEX = 1;
    static const size_t FEE_INDEX = 2;

    static const char *const prompts[] = {
        PROMPT("Register"),
        PROMPT("Address"),
        PROMPT("Fee"),
        NULL,
    };

    if (!G.maybe_ops.is_valid) THROW(EXC_MEMORY_ERROR);

    REGISTER_STATIC_UI_VALUE(TYPE_INDEX, "as delegate?");
    register_ui_callback(ADDRESS_INDEX, bip32_path_with_curve_to_pkh_string, &G.key);
    register_ui_callback(FEE_INDEX, microtez_to_string_indirect, &G.maybe_ops.v.total_fee);

    ui_prompt(prompts, NULL, ok_cb, cxl_cb);
}

uint32_t baking_sign_complete(void) {
    // Have raw data, can get insight into it
    switch (G.magic_number) {
        case MAGIC_BYTE_BLOCK:
        case MAGIC_BYTE_BAKING_OP:
            if (G.maybe_parsed_baking_data.is_valid) {
                guard_baking_authorized(&G.maybe_parsed_baking_data.v, &G.key);
                return perform_signature(true);
            } else {
                THROW(EXC_PARSE_ERROR);
            }
            break;

        case MAGIC_BYTE_UNSAFE_OP:
            {
                // Must be self-delegation signed by the *authorized* baking key
                if (G.maybe_ops.is_valid &&
                    bip32_path_with_curve_eq(&G.key, &N_data.baking_key) &&

                    // ops->signing is generated from G.bip32_path and G.curve
                    COMPARE(&G.maybe_ops.v.operation.source, &G.maybe_ops.v.signing) == 0 &&
                    COMPARE(&G.maybe_ops.v.operation.destination, &G.maybe_ops.v.signing) == 0
                ) {
                    prompt_register_delegate(sign_ok, sign_reject);
                }
                THROW(EXC_SECURITY);
            }
        case MAGIC_BYTE_UNSAFE_OP2:
        case MAGIC_BYTE_UNSAFE_OP3:
        default:
            THROW(EXC_PARSE_ERROR);
    }
}

#else // ifdef BAKING_APP -----------------------------------------------------

static bool sign_unsafe_ok(void) {
    delayed_send(perform_signature(false));
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
            THROW(EXC_PARSE_ERROR);

        case OPERATION_TAG_PROPOSAL:
            {
                static const uint32_t TYPE_INDEX = 0;
                static const uint32_t SOURCE_INDEX = 1;
                static const uint32_t PERIOD_INDEX = 2;
                static const uint32_t PROTOCOL_HASH_INDEX = 3;

                static const char *const proposal_prompts[] = {
                    PROMPT("Confirm"),
                    PROMPT("Source"),
                    PROMPT("Period"),
                    PROMPT("Protocol"),
                    NULL,
                };

                register_ui_callback(SOURCE_INDEX, parsed_contract_to_string, &ops->operation.source);
                register_ui_callback(PERIOD_INDEX, number_to_string_indirect32, &ops->operation.proposal.voting_period);
                register_ui_callback(PROTOCOL_HASH_INDEX, protocol_hash_to_string, ops->operation.proposal.protocol_hash);

                REGISTER_STATIC_UI_VALUE(TYPE_INDEX, "Proposal");
                ui_prompt(proposal_prompts, NULL, ok, cxl);
            }

        case OPERATION_TAG_BALLOT:
            {
                static const uint32_t TYPE_INDEX = 0;
                static const uint32_t SOURCE_INDEX = 1;
                static const uint32_t PROTOCOL_HASH_INDEX = 2;
                static const uint32_t PERIOD_INDEX = 3;

                static const char *const ballot_prompts[] = {
                    PROMPT("Confirm Vote"),
                    PROMPT("Source"),
                    PROMPT("Protocol"),
                    PROMPT("Period"),
                    NULL,
                };

                register_ui_callback(SOURCE_INDEX, parsed_contract_to_string, &ops->operation.source);
                register_ui_callback(PROTOCOL_HASH_INDEX, protocol_hash_to_string,
                                     ops->operation.ballot.protocol_hash);
                register_ui_callback(PERIOD_INDEX, number_to_string_indirect32, &ops->operation.ballot.voting_period);

                switch (ops->operation.ballot.vote) {
                    case BALLOT_VOTE_YEA:
                        REGISTER_STATIC_UI_VALUE(TYPE_INDEX, "Yea");
                        break;
                    case BALLOT_VOTE_NAY:
                        REGISTER_STATIC_UI_VALUE(TYPE_INDEX, "Nay");
                        break;
                    case BALLOT_VOTE_PASS:
                        REGISTER_STATIC_UI_VALUE(TYPE_INDEX, "Pass");
                        break;
                }

                ui_prompt(ballot_prompts, NULL, ok, cxl);
            }

        case OPERATION_TAG_ORIGINATION:
            {
                static const uint32_t TYPE_INDEX = 0;
                static const uint32_t AMOUNT_INDEX = 1;
                static const uint32_t FEE_INDEX = 2;
                static const uint32_t SOURCE_INDEX = 3;
                static const uint32_t DESTINATION_INDEX = 4;
                static const uint32_t DELEGATE_INDEX = 5;
                static const uint32_t STORAGE_INDEX = 6;

                register_ui_callback(SOURCE_INDEX, parsed_contract_to_string, &ops->operation.source);
                register_ui_callback(DESTINATION_INDEX, parsed_contract_to_string,
                                     &ops->operation.destination);
                register_ui_callback(FEE_INDEX, microtez_to_string_indirect, &ops->total_fee);
                register_ui_callback(STORAGE_INDEX, number_to_string_indirect64,
                                     &ops->total_storage_limit);

                static const char *const origination_prompts_fixed[] = {
                    PROMPT("Confirm"),
                    PROMPT("Amount"),
                    PROMPT("Fee"),
                    PROMPT("Source"),
                    PROMPT("Manager"),
                    PROMPT("Fixed Delegate"),
                    PROMPT("Storage"),
                    NULL,
                };
                static const char *const origination_prompts_delegatable[] = {
                    PROMPT("Confirm"),
                    PROMPT("Amount"),
                    PROMPT("Fee"),
                    PROMPT("Source"),
                    PROMPT("Manager"),
                    PROMPT("Delegate"),
                    PROMPT("Storage"),
                    NULL,
                };
                static const char *const origination_prompts_undelegatable[] = {
                    PROMPT("Confirm"),
                    PROMPT("Amount"),
                    PROMPT("Fee"),
                    PROMPT("Source"),
                    PROMPT("Manager"),
                    PROMPT("Delegation"),
                    PROMPT("Storage"),
                    NULL,
                };

                if (!(ops->operation.flags & ORIGINATION_FLAG_SPENDABLE)) return false;

                REGISTER_STATIC_UI_VALUE(TYPE_INDEX, "Origination");
                register_ui_callback(AMOUNT_INDEX, microtez_to_string_indirect, &ops->operation.amount);

                char const *const *prompts;
                bool const delegatable = ops->operation.flags & ORIGINATION_FLAG_DELEGATABLE;
                bool const has_delegate = ops->operation.delegate.curve_code != TEZOS_NO_CURVE;
                if (delegatable && has_delegate) {
                    prompts = origination_prompts_delegatable;
                    register_ui_callback(DELEGATE_INDEX, parsed_contract_to_string,
                                         &ops->operation.delegate);
                } else if (delegatable && !has_delegate) {
                    prompts = origination_prompts_delegatable;
                    REGISTER_STATIC_UI_VALUE(DELEGATE_INDEX, "Any");
                } else if (!delegatable && has_delegate) {
                    prompts = origination_prompts_fixed;
                    register_ui_callback(DELEGATE_INDEX, parsed_contract_to_string,
                                         &ops->operation.delegate);
                } else if (!delegatable && !has_delegate) {
                    prompts = origination_prompts_undelegatable;
                    REGISTER_STATIC_UI_VALUE(DELEGATE_INDEX, "Disabled");
                }

                ui_prompt(prompts, NULL, ok, cxl);
            }
        case OPERATION_TAG_DELEGATION:
            {
                static const uint32_t TYPE_INDEX = 0;
                static const uint32_t FEE_INDEX = 1;
                static const uint32_t SOURCE_INDEX = 2;
                static const uint32_t DESTINATION_INDEX = 3;
                static const uint32_t STORAGE_INDEX = 4;

                register_ui_callback(SOURCE_INDEX, parsed_contract_to_string,
                                     &ops->operation.source);
                register_ui_callback(DESTINATION_INDEX, parsed_contract_to_string,
                                     &ops->operation.destination);
                register_ui_callback(FEE_INDEX, microtez_to_string_indirect, &ops->total_fee);
                register_ui_callback(STORAGE_INDEX, number_to_string_indirect64,
                                     &ops->total_storage_limit);

                static const char *const withdrawal_prompts[] = {
                    PROMPT("Withdraw"),
                    PROMPT("Fee"),
                    PROMPT("Source"),
                    PROMPT("Delegate"),
                    PROMPT("Storage"),
                    NULL,
                };
                static const char *const delegation_prompts[] = {
                    PROMPT("Confirm"),
                    PROMPT("Fee"),
                    PROMPT("Source"),
                    PROMPT("Delegate"),
                    PROMPT("Storage"),
                    NULL,
                };

                REGISTER_STATIC_UI_VALUE(TYPE_INDEX, "Delegation");

                bool withdrawal = ops->operation.destination.originated == 0 &&
                    ops->operation.destination.curve_code == TEZOS_NO_CURVE;

                ui_prompt(withdrawal ? withdrawal_prompts : delegation_prompts, NULL, ok, cxl);
            }

        case OPERATION_TAG_TRANSACTION:
            {
                static const uint32_t TYPE_INDEX = 0;
                static const uint32_t AMOUNT_INDEX = 1;
                static const uint32_t FEE_INDEX = 2;
                static const uint32_t SOURCE_INDEX = 3;
                static const uint32_t DESTINATION_INDEX = 4;
                static const uint32_t STORAGE_INDEX = 5;

                static const char *const transaction_prompts[] = {
                    PROMPT("Confirm"),
                    PROMPT("Amount"),
                    PROMPT("Fee"),
                    PROMPT("Source"),
                    PROMPT("Destination"),
                    PROMPT("Storage"),
                    NULL,
                };

                register_ui_callback(SOURCE_INDEX, parsed_contract_to_string, &ops->operation.source);
                register_ui_callback(DESTINATION_INDEX, parsed_contract_to_string,
                                     &ops->operation.destination);
                register_ui_callback(FEE_INDEX, microtez_to_string_indirect, &ops->total_fee);
                register_ui_callback(STORAGE_INDEX, number_to_string_indirect64,
                                     &ops->total_storage_limit);
                register_ui_callback(AMOUNT_INDEX, microtez_to_string_indirect, &ops->operation.amount);

                REGISTER_STATIC_UI_VALUE(TYPE_INDEX, "Transaction");

                ui_prompt(transaction_prompts, NULL, ok, cxl);
            }
        case OPERATION_TAG_NONE:
            {
                static const uint32_t TYPE_INDEX = 0;
                static const uint32_t SOURCE_INDEX = 1;
                static const uint32_t FEE_INDEX = 2;
                static const uint32_t STORAGE_INDEX = 3;

                register_ui_callback(SOURCE_INDEX, parsed_contract_to_string, &ops->operation.source);
                register_ui_callback(FEE_INDEX, microtez_to_string_indirect, &ops->total_fee);

                // Parser function guarantees this has a reveal
                static const char *const reveal_prompts[] = {
                    PROMPT("Reveal Key"),
                    PROMPT("Key"),
                    PROMPT("Fee"),
                    PROMPT("Storage"),
                    NULL,
                };

                REGISTER_STATIC_UI_VALUE(TYPE_INDEX, "To Blockchain");
                register_ui_callback(STORAGE_INDEX, number_to_string_indirect64,
                                     &ops->total_storage_limit);
                register_ui_callback(SOURCE_INDEX, parsed_contract_to_string, &ops->operation.source);

                ui_prompt(reveal_prompts, NULL, ok, cxl);
            }
    }
}

static size_t wallet_sign_complete(uint8_t instruction) {
    static size_t const TYPE_INDEX = 0;
    static size_t const HASH_INDEX = 1;

    static const char *const parse_fail_prompts[] = {
        PROMPT("Unrecognized"),
        PROMPT("Sign Hash"),
        NULL,
    };

    REGISTER_STATIC_UI_VALUE(TYPE_INDEX, "Operation");

    if (instruction == INS_SIGN_UNSAFE) {
        static const char *const prehashed_prompts[] = {
            PROMPT("Pre-hashed"),
            PROMPT("Sign Hash"),
            NULL,
        };

        G.message_data_as_buffer.bytes = (uint8_t *)&G.message_data;
        G.message_data_as_buffer.size = sizeof(G.message_data);
        G.message_data_as_buffer.length = G.message_data_length;
        // Base58 encoding of 32-byte hash is 43 bytes long.
        register_ui_callback(HASH_INDEX, buffer_to_base58, &G.message_data_as_buffer);
        ui_prompt(prehashed_prompts, NULL, sign_unsafe_ok, sign_reject);
    } else {
        switch (G.magic_number) {
            case MAGIC_BYTE_BLOCK:
            case MAGIC_BYTE_BAKING_OP:
            default:
                THROW(EXC_PARSE_ERROR);
            case MAGIC_BYTE_UNSAFE_OP:
                if (!G.maybe_ops.is_valid || !prompt_transaction(&G.maybe_ops.v, &G.key, sign_ok, sign_reject)) {
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
        // Base58 encoding of 32-byte hash is 43 bytes long.
        register_ui_callback(HASH_INDEX, buffer_to_base58, &G.message_data_as_buffer);
        ui_prompt(parse_fail_prompts, NULL, sign_ok, sign_reject);
    }
}

#endif // ifdef BAKING_APP ----------------------------------------------------

#define P1_FIRST 0x00
#define P1_NEXT 0x01
#define P1_HASH_ONLY_NEXT 0x03 // You only need it once
#define P1_LAST_MARKER 0x80

size_t handle_apdu_sign(uint8_t instruction) {
    uint8_t *const buff = &G_io_apdu_buffer[OFFSET_CDATA];
    uint8_t const p1 = READ_UNALIGNED_BIG_ENDIAN(uint8_t, &G_io_apdu_buffer[OFFSET_P1]);
    uint8_t const buff_size = READ_UNALIGNED_BIG_ENDIAN(uint8_t, &G_io_apdu_buffer[OFFSET_LC]);
    if (buff_size > MAX_APDU_SIZE) THROW(EXC_WRONG_LENGTH_FOR_INS);

    bool last = (p1 & P1_LAST_MARKER) != 0;
    switch (p1 & ~P1_LAST_MARKER) {
    case P1_FIRST:
        clear_data();
        read_bip32_path(&G.key.bip32_path, buff, buff_size);
        G.key.curve = curve_code_to_curve(READ_UNALIGNED_BIG_ENDIAN(uint8_t, &G_io_apdu_buffer[OFFSET_CURVE]));
        return finalize_successful_send(0);
#ifndef BAKING_APP
    case P1_HASH_ONLY_NEXT:
        // This is a debugging Easter egg
        G.hash_only = true;
        // FALL THROUGH
#endif
    case P1_NEXT:
        if (G.key.bip32_path.length == 0) {
            THROW(EXC_WRONG_LENGTH_FOR_INS);
        }
        break;
    default:
        THROW(EXC_WRONG_PARAM);
    }

    if (instruction != INS_SIGN_UNSAFE && G.magic_number == 0) {
        G.magic_number = get_magic_byte(buff, buff_size);
    }

#   ifdef BAKING_APP
        // Keep trying to parse baking data unless we already did so successfully.
        G.maybe_parsed_baking_data.is_valid =
            G.maybe_parsed_baking_data.is_valid
            || parse_baking_data(&G.maybe_parsed_baking_data.v, buff, buff_size);
#   endif

    // Keep trying to parse operations.
    G.maybe_ops.is_valid =
        G.maybe_ops.is_valid
        || parse_allowed_operations(&G.maybe_ops.v, buff, buff_size, &G.key);

    if (instruction == INS_SIGN) {
        // Hash contents of *previous* message (which may be empty).
        blake2b_incremental_hash(
            G.message_data, sizeof(G.message_data),
            &G.message_data_length,
            &G.hash_state);
    }

    if (G.message_data_length + buff_size > sizeof(G.message_data)) {
        THROW(EXC_PARSE_ERROR);
    }

    memmove(G.message_data + G.message_data_length, buff, buff_size);
    G.message_data_length += buff_size;

    if (last) {
        if (instruction == INS_SIGN) {
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

        return
#           ifdef BAKING_APP
                baking_sign_complete();
#           else
                wallet_sign_complete(instruction);
#           endif
    } else {
        return finalize_successful_send(0);
    }
}

static int perform_signature(bool on_hash) {
#   ifdef BAKING_APP
        if (G.maybe_parsed_baking_data.is_valid) {
            write_high_water_mark(&G.maybe_parsed_baking_data.v);
        }
#   else
        if (on_hash && G.hash_only) {
            memcpy(G_io_apdu_buffer, G.final_hash, sizeof(G.final_hash));
            clear_data();
            return finalize_successful_send(sizeof(G.final_hash));
        }
#   endif

    uint8_t const *const data = on_hash ? G.final_hash : G.message_data;
    size_t const data_length = on_hash ? sizeof(G.final_hash) : G.message_data_length;

    size_t const tx = sign(G_io_apdu_buffer, MAX_SIGNATURE_SIZE, &G.key, data, data_length);

    clear_data();
    return finalize_successful_send(tx);
}

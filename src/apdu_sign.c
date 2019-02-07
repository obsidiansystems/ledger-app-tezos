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

// Order matters
#include "cx.h"

#include <string.h>

#define G global.u.sign

#define SIGN_HASH_SIZE 32
#define B2B_BLOCKBYTES 128

static void conditional_init_hash_state(void) {
    if (!G.is_hash_state_inited) {
        b2b_init(&global.blake2b.hash_state, SIGN_HASH_SIZE);
        G.is_hash_state_inited = true;
    }
}

static void hash_buffer(void) {
    const uint8_t *current = G.message_data;
    while (G.message_data_length > B2B_BLOCKBYTES) {
        conditional_init_hash_state();
        b2b_update(&global.blake2b.hash_state, current, B2B_BLOCKBYTES);
        G.message_data_length -= B2B_BLOCKBYTES;
        current += B2B_BLOCKBYTES;
    }
    // TODO use circular buffer at some point
    memmove(G.message_data, current, G.message_data_length);
}

static void finish_hashing(uint8_t *hash, size_t hash_size) {
    hash_buffer();
    conditional_init_hash_state();
    b2b_update(&global.blake2b.hash_state, G.message_data, G.message_data_length);
    b2b_final(&global.blake2b.hash_state, hash, hash_size);
    G.message_data_length = 0;
    G.is_hash_state_inited = false;
}

static int perform_signature(bool hash_first);

static bool sign_ok(void) {
    int tx = perform_signature(true);
    delayed_send(tx);
    return true;
}

#ifndef BAKING_APP
static bool sign_unsafe_ok(void) {
    int tx = perform_signature(false);
    delayed_send(tx);
    return true;
}
#endif

static void clear_data(void) {
    G.key.bip32_path.length = 0;
    G.message_data_length = 0;
    G.is_hash_state_inited = false;
    G.magic_number = 0;
    G.hash_only = false;
}

static bool sign_reject(void) {
    clear_data();
    delay_reject();
    return true; // Return to idle
}

#ifdef BAKING_APP

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

    REGISTER_STATIC_UI_VALUE(TYPE_INDEX, "as delegate?");
    register_ui_callback(ADDRESS_INDEX, bip32_path_with_curve_to_pkh_string, &G.key);
    register_ui_callback(FEE_INDEX, microtez_to_string_indirect, &G.register_delegate.total_fee);

    ui_prompt(prompts, NULL, ok_cb, cxl_cb);
}

uint32_t baking_sign_complete(void) {
    // Have raw data, can get insight into it
    switch (G.magic_number) {
        case MAGIC_BYTE_BLOCK:
        case MAGIC_BYTE_BAKING_OP:
            guard_baking_authorized(
                G.key.curve, G.message_data, G.message_data_length,
                &G.key.bip32_path);
            return perform_signature(true);

        case MAGIC_BYTE_UNSAFE_OP:
            {

                allowed_operation_set allowed;
                clear_operation_set(&allowed);
                allow_operation(&allowed, OPERATION_TAG_DELEGATION);
                allow_operation(&allowed, OPERATION_TAG_REVEAL);

                struct parsed_operation_group *ops =
                    parse_operations(
                        G.message_data, G.message_data_length,
                        G.key.curve, &G.key.bip32_path, allowed);

                // Must be self-delegation signed by the *authorized* baking key
                if (bip32_path_with_curve_eq(&G.key, &N_data.baking_key) &&

                    // ops->signing is generated from G.bip32_path and G.curve
                    COMPARE(&ops->operation.source, &ops->signing) == 0 &&
                    COMPARE(&ops->operation.destination, &ops->signing) == 0
                ) {
                    G.register_delegate.total_fee = ops->total_fee;
                    prompt_register_delegate(sign_ok, sign_reject);
                }
                THROW(EXC_PARSE_ERROR);
            }
        case MAGIC_BYTE_UNSAFE_OP2:
        case MAGIC_BYTE_UNSAFE_OP3:
        default:
            THROW(EXC_PARSE_ERROR);
    }
}

#else


const char *const insecure_values[] = {
    "Operation",
    "Unverified?",
    NULL,
};

#define MAX_NUMBER_CHARS (MAX_INT_DIGITS + 2) // include decimal point and terminating null

// Return false if the transaction isn't easily parseable, otherwise prompt with given callbacks
// and do not return, but rather throw ASYNC.
static bool prompt_transaction(const void *data, size_t length, cx_curve_t curve,
                               bip32_path_t const *const bip32_path,
                               ui_callback_t ok, ui_callback_t cxl) {
    check_null(data);
    check_null(bip32_path);
    struct parsed_operation_group *ops;

#ifndef TEZOS_DEBUG
    BEGIN_TRY { // TODO: Eventually, "unsafe" operations will be another APDU,
                //       and we will parse enough operations that it will rarely need to be used,
                //       hopefully ultimately never.
        TRY {
#endif
            // TODO: Simplify this to just switch on what we got.
            allowed_operation_set allowed;
            clear_operation_set(&allowed);
            allow_operation(&allowed, OPERATION_TAG_PROPOSAL);
            allow_operation(&allowed, OPERATION_TAG_BALLOT);
            allow_operation(&allowed, OPERATION_TAG_DELEGATION);
            allow_operation(&allowed, OPERATION_TAG_REVEAL);
            allow_operation(&allowed, OPERATION_TAG_ORIGINATION);
            allow_operation(&allowed, OPERATION_TAG_TRANSACTION);
            // TODO: Add still other operations

            ops = parse_operations(data, length, curve, bip32_path, allowed);
#ifndef TEZOS_DEBUG
        }
        CATCH_OTHER(e) {
            return false;
        }
        FINALLY { }
    }
    END_TRY;
#endif

    // Now to display it to make sure it's what the user intended.

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

                const char *const *prompts;
                bool delegatable = ops->operation.flags & ORIGINATION_FLAG_DELEGATABLE;
                bool has_delegate = ops->operation.delegate.curve_code != TEZOS_NO_CURVE;
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

uint32_t wallet_sign_complete(uint8_t instruction) {
    static const char *const parse_fail_prompts[] = {
        PROMPT("Unrecognized"),
        PROMPT("Sign"),
        NULL,
    };

    if (instruction == INS_SIGN_UNSAFE) {
        static const char *const prehashed_prompts[] = {
            PROMPT("Pre-hashed"),
            PROMPT("Sign"),
            NULL,
        };
        ui_prompt(prehashed_prompts, insecure_values, sign_unsafe_ok, sign_reject);
    } else {
        switch (G.magic_number) {
            case MAGIC_BYTE_BLOCK:
            case MAGIC_BYTE_BAKING_OP:
            default:
                THROW(EXC_PARSE_ERROR);
            case MAGIC_BYTE_UNSAFE_OP:
                if (G.is_hash_state_inited) {
                    goto unsafe;
                }
                if (!prompt_transaction(
                        G.message_data, G.message_data_length, G.key.curve,
                        &G.key.bip32_path, sign_ok, sign_reject)) {
                    goto unsafe;
                }
            case MAGIC_BYTE_UNSAFE_OP2:
            case MAGIC_BYTE_UNSAFE_OP3:
                goto unsafe;
        }
unsafe:
        ui_prompt(parse_fail_prompts, insecure_values, sign_ok, sign_reject);
    }
}
#endif

#define P1_FIRST 0x00
#define P1_NEXT 0x01
#define P1_HASH_ONLY_NEXT 0x03 // You only need it once
#define P1_LAST_MARKER 0x80

unsigned int handle_apdu_sign(uint8_t instruction) {
    uint8_t p1 = G_io_apdu_buffer[OFFSET_P1];
    uint8_t *dataBuffer = G_io_apdu_buffer + OFFSET_CDATA;
    uint32_t dataLength = G_io_apdu_buffer[OFFSET_LC];

    bool last = (p1 & P1_LAST_MARKER) != 0;
    switch (p1 & ~P1_LAST_MARKER) {
    case P1_FIRST:
        clear_data();
        memset(G.message_data, 0, sizeof(G.message_data));
        G.message_data_length = 0;
        read_bip32_path(&G.key.bip32_path, dataBuffer, dataLength);
        G.key.curve = curve_code_to_curve(G_io_apdu_buffer[OFFSET_CURVE]);
        return_ok();
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
        G.magic_number = get_magic_byte(dataBuffer, dataLength);
    }

#ifndef BAKING_APP
    if (instruction == INS_SIGN) {
        hash_buffer();
    }
#endif

    if (G.message_data_length + dataLength > sizeof(G.message_data)) {
        THROW(EXC_PARSE_ERROR);
    }

    os_memmove(G.message_data + G.message_data_length, dataBuffer, dataLength);
    G.message_data_length += dataLength;

    if (!last) {
        return_ok();
    }

#ifdef BAKING_APP
    return baking_sign_complete();
#else
    return wallet_sign_complete(instruction);
#endif
}

static int perform_signature(bool hash_first) {
    uint8_t hash[SIGN_HASH_SIZE];
    uint8_t *data = G.message_data;
    uint32_t datalen = G.message_data_length;

#ifdef BAKING_APP
    update_high_water_mark(G.message_data, G.message_data_length);
#endif

    if (hash_first) {
        hash_buffer();
        finish_hashing(hash, sizeof(hash));
        data = hash;
        datalen = SIGN_HASH_SIZE;

#ifndef BAKING_APP
        if (G.hash_only) {
            memcpy(G_io_apdu_buffer, data, datalen);
            uint32_t tx = datalen;

            G_io_apdu_buffer[tx++] = 0x90;
            G_io_apdu_buffer[tx++] = 0x00;
            return tx;
        }
#endif
    }

    struct key_pair *const pair = generate_key_pair(G.key.curve, &G.key.bip32_path);

    uint32_t tx;
    switch (G.key.curve) {
    case CX_CURVE_Ed25519: {
        tx = cx_eddsa_sign(&pair->private_key,
                           0,
                           CX_SHA512,
                           data,
                           datalen,
                           NULL,
                           0,
                           &G_io_apdu_buffer[0],
                           64,
                           NULL);
    }
        break;
    case CX_CURVE_SECP256K1:
    case CX_CURVE_SECP256R1:
    {
        unsigned int info;
        tx = cx_ecdsa_sign(&pair->private_key,
                           CX_LAST | CX_RND_TRNG,
                           CX_NONE,
                           data,
                           datalen,
                           &G_io_apdu_buffer[0],
                           100,
                           &info);
        if (info & CX_ECCINFO_PARITY_ODD) {
            G_io_apdu_buffer[0] |= 0x01;
        }
    }
        break;
    default:
        THROW(EXC_WRONG_PARAM); // This should not be able to happen.
    }

    memset(&pair->private_key, 0, sizeof(pair->private_key));

    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;

    clear_data();

    return tx;
}
